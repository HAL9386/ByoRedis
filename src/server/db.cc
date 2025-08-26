#include "byoredis/server/db.hh"
#include "byoredis/ds/intrusive.hh"  // for container_of

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