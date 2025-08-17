#ifndef HASH_TABLE_HH
#define HASH_TABLE_HH

#include <stddef.h>
#include <stdint.h>

// hashtablde intrusive linked list node with hash value of the key,
// should be embedded into the payload
struct HNode {
  HNode    *next = NULL;
  uint64_t hcode = 0;     // hash value
};

// a simple fixed-sized hashtable
struct HTab {
  HNode **tab = NULL;  // array of slots
  // Modulo or division are slow CPU operations,
  // so it’s common to use powers of 2 sizes.
  // Modulo by a power of 2 is just taking the lower bits,
  // so it can be done by a fast bitwise: hash(key) & (N - 1)
  size_t mask = 0;     // power of 2 array size, 2^n - 1
  size_t size = 0;     // number of keys
};

// the resizable HMap is based on the fixed-size HTab,
// contains 2 of them for the progressive rehashing.
struct HMap {
  HTab newer;
  HTab older;
  size_t migrate_pos = 0;
};

HNode * hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void    hm_insert(HMap *hmap, HNode *node);
HNode * hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void    hm_clear(HMap *hmap);
size_t  hm_size(HMap *hmap);
// invoke the callback on each node until it returns false
void    hm_foreach(HMap *hmap, bool (*cb)(HNode *, void *), void *arg);

#endif  // HASH_TABLE_HH
