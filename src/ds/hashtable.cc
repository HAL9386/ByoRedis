#include <stdlib.h>  // calloc(), free()
#include "byoredis/ds/hashtable.hh"
#include <assert.h>

size_t const k_max_load_factor = 8;
size_t const k_rehashing_work = 128;  // how many keys to migrate in one rehashing step

static void round_up_power_of_2(size_t &n) {
  n -= 1;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n |= n >> 32;
  n += 1;
}

static void h_init(HTab *htab, size_t n) {
  round_up_power_of_2(n);
  assert(n > 0 && ((n - 1) & n) == 0);
  htab->tab  = (HNode **)calloc(n, sizeof(HNode *));
  htab->mask = n - 1;
  htab->size = 0;
}

// insert at the front of the chaining linked list
static void h_insert(HTab *htab, HNode *node) {
  size_t pos  = node->hcode & htab->mask;  // node->hcode & (n - 1)
  HNode *next = htab->tab[pos];
  node->next     = next;
  htab->tab[pos] = node;
  htab->size    += 1;
}

// actually return the address of its parent pointer(who hold target node's address)
// either the slot or the prev node's next field
static HNode ** h_lookup(HTab *htab, HNode *key, bool (*eq)(HNode *, HNode *)) {
  if (!htab->tab) {
    return NULL;
  }
  size_t pos = key->hcode & htab->mask;
  HNode **from = &htab->tab[pos];        // incoming pointer to the target
  for (HNode *cur; (cur = *from) != NULL; from = &cur->next) {
    if (cur->hcode == key->hcode && eq(cur, key)) {  // compare the hash value before calling the callback,
      return from;                                   // an optimization to rule out candidates early.
    }
  }
  return NULL;
}

static HNode * h_detach(HTab *htab, HNode **from) {
  HNode *node = *from;
  *from = node->next;
  htab->size--;
  return node;
}

static void hm_trigger_rehashing(HMap *hmap) {
  hmap->older = hmap->newer;  // (newer, older) <- (new_table, newer)
  h_init(&hmap->newer, (hmap->newer.mask + 1) * 2);
  hmap->migrate_pos = 0;
}

static void hm_help_rehashing(HMap *hmap) {
  size_t nwork = 0;
  while (nwork < k_rehashing_work && hmap->older.size > 0) {
    // find a non-empty slot
    HNode **from = &hmap->older.tab[hmap->migrate_pos];
    if (!*from) {
      hmap->migrate_pos++;
      continue;  // empty slot
    }
    // move the first list item to the newer table
    h_insert(&hmap->newer, h_detach(&hmap->older, from));
    nwork++;
  }
  // discard the old table if done
  if (hmap->older.size == 0 && hmap->older.tab) {
    free(hmap->older.tab);
    hmap->older = HTab{};
  }
}

HNode * hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
  hm_help_rehashing(hmap);
  HNode **from = h_lookup(&hmap->newer, key, eq);
  if (!from) {
    from = h_lookup(&hmap->older, key, eq);
  }
  return from ? *from : NULL;
}

HNode * hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
  hm_help_rehashing(hmap);
  if (HNode **from = h_lookup(&hmap->newer, key, eq)) {
    return h_detach(&hmap->newer, from);
  }
  if (HNode **from = h_lookup(&hmap->older, key, eq)) {
    return h_detach(&hmap->older, from);
  }
  return NULL;
}

// Insertion always update the newer table
// It triggers rehashing when load factor is high
void hm_insert(HMap *hmap, HNode *node) {
  if (!hmap->newer.tab) {
    h_init(&hmap->newer, 4);     // initialized it if empty
  }
  h_insert(&hmap->newer, node);
  if (!hmap->older.tab) {  // check whether we need to trigger rehash. When older table is not empty, means we are in the rehashing process.
    size_t shreshold = (hmap->newer.mask + 1) * k_max_load_factor;
    if (hmap->newer.size >= shreshold) {
      hm_trigger_rehashing(hmap);
    }
  }
  hm_help_rehashing(hmap);       // migrate some keys
}

void hm_clear(HMap *hmap) {
  free(hmap->newer.tab);
  free(hmap->older.tab);
  *hmap = HMap{};
}

size_t hm_size(HMap *hmap) {
  return hmap->newer.size + hmap->older.size;
}

static bool h_foreach(HTab *htab, bool (*cb)(HNode *, void *), void *arg) {
  for (size_t i = 0; htab->mask != 0 && i <= htab->mask; i++) {
    for (HNode *node = htab->tab[i]; node != NULL; node = node->next) {
      if (!cb(node, arg)) {
        return false;
      }
    }
  }
  return true;
}

void hm_foreach(HMap *hmap, bool (*cb)(HNode *, void *), void *arg) {
  h_foreach(&hmap->newer, cb, arg) && h_foreach(&hmap->older, cb, arg);
}
