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

#endif  // HASH_TABLE_HH
