#include <stdlib.h>  // calloc(), free()
#include "hashtable.hh"
#include <assert.h>

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

// the address of the pointer whose value is the address of target node
// actually return the address of its parent pointer,
// we need the addresss of the pointer to delete it,
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
