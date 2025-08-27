#include "byoredis/server/db.hh"
#include "byoredis/ds/intrusive.hh"  // for container_of
#include "byoredis/ds/zset.hh"
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
  delete ent;
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
