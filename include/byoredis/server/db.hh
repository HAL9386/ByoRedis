#pragma once

#include <string>

#include "byoredis/ds/hashtable.hh"

struct GlobalData {
  HMap db;  // top-level hashtable
};
extern GlobalData g_data;

// KV pair for the top-level hashtable
struct Entry {
  struct HNode node;  // hashtable node
  std::string key;
  std::string val; 
};

struct LookupKey {  // for lookup only
  HNode node;
  std::string key;
};

bool key_eq(HNode *lhs, HNode *rhs);
uint64_t str_hash(uint8_t const *data, size_t len);
