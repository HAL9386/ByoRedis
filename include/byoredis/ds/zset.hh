#pragma once

#include "byoredis/ds/avl.hh"
#include "byoredis/ds/hashtable.hh"

struct ZSet {
  AVLNode *root = NULL;  // index by (score, name)
  HMap hmap;             // index by name
};

struct ZNode {
  // data structure nodes
  AVLNode tree;
  HNode   hmap;
  // data
  double  score = 0;
  size_t  len = 0;
  char    name[0];       // flexible array
};

bool    zset_insert(ZSet *zset, char const *name, size_t len, double score);
ZNode * zset_lookup(ZSet *zset, char const *name, size_t len);
void    zset_delete(ZSet *zset, ZNode *node);
ZNode * zset_seekge(ZSet *zset, double score, char const *name, size_t len);
ZNode * znode_offset(ZNode *node, int64_t offset);
void    zset_clear(ZSet *zset);
