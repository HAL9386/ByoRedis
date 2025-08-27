#pragma once

#include <string>

#include "byoredis/ds/hashtable.hh"
#include "byoredis/ds/zset.hh"

struct GlobalData {
  HMap db;  // top-level hashtable
};
extern GlobalData g_data;

enum ENTRY_TYPE {
  T_INIT = 0,
  T_STR  = 1,  // string
  T_ZSET = 2,  // sorted set
};

// KV pair for the top-level hashtable
struct Entry {
  struct HNode node;  // hashtable node
  std::string key;
  // value
  uint32_t type = T_INIT;
  // one of the following
  std::string str; 
  ZSet zset;
};

struct LookupKey {  // for lookup only
  HNode node;
  std::string key;
};

struct HKey {  // for the hashtable key(zset name) compare function
  HNode node;
  char const *name = NULL;
  size_t len = 0;
};

Entry * entry_new(uint32_t type);
void    entry_free(Entry *ent);

bool key_eq(HNode *lhs, HNode *rhs);
uint64_t str_hash(uint8_t const *data, size_t len);
bool hcmp(HNode *node, HNode *key);
