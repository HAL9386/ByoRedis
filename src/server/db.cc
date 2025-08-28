#include "byoredis/server/db.hh"
#include "byoredis/ds/intrusive.hh"  // for container_of
#include "byoredis/ds/zset.hh"
#include "byoredis/server/time.hh"
#include <string.h>

GlobalData g_data{};

Entry * entry_new(uint32_t type) {
  Entry *ent = new Entry();
  ent->type = type;
  return ent;
}

void entry_free(Entry *ent) {
  if (ent->type == T_ZSET) {
    zset_clear(&ent->zset);
  }
  entry_set_ttl(ent, -1); // remove from the TTL heap
  delete ent;
}

// set or remove the TTL
void entry_set_ttl(Entry *ent, int64_t ttl_ms) {
  if (ttl_ms < 0 && ent->heap_idx != (size_t)-1) {
    // setting a negative TTL means removing the TTL
    heap_delete(g_data.heap, ent->heap_idx);
    ent->heap_idx = -1;
  } else if (ttl_ms >= 0) {
    // add or update the TTL
    uint64_t expire_at = get_monotonic_msec() + (uint64_t)ttl_ms;
    HeapItem item = {expire_at, &ent->heap_idx};
    heap_upsert(g_data.heap, ent->heap_idx, item);
  }
}

// equality comparison for `struct Entry` and `struct LookupKey`
bool key_eq(HNode *lhs, HNode *rhs) {
  struct Entry *le     = container_of(lhs, struct Entry, node);
  struct LookupKey *rk = container_of(rhs, struct LookupKey, node);
  return le->key == rk->key;
}

// FNV hash
uint64_t str_hash(uint8_t const *data, size_t len) {
  uint32_t h = 0x811C9DC5;
  for (size_t i = 0; i < len; i++) {
    h = (h + data[i]) * 0x1000193;
  }
  return h;
}

bool hcmp(HNode *node, HNode *key) {
  ZNode *znode = container_of(node, ZNode, hmap);
  HKey  *hkey  = container_of(key, HKey, node);
  if (znode->len != hkey->len) {
    return false;
  }
  return 0 == memcmp(znode->name, hkey->name, znode->len);
}

// used for deleting a expired node
bool hnode_same(HNode *node, HNode *key) {
  return node == key;
}
