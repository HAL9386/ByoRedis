#include "byoredis/ds/zset.hh"
#include "byoredis/server/db.hh"
#include "byoredis/ds/intrusive.hh"
#include <string.h>
#include <assert.h>

// lookup by name in the hashtable
ZNode * zset_lookup(ZSet *zset, char const *name, size_t len) {
  if (!zset->root) {
    return NULL;
  }
  HKey key;
  key.node.hcode = str_hash((uint8_t const *)name, len);
  key.name = name;
  key.len = len;
  HNode *found = hm_lookup(&zset->hmap, &key.node, &hcmp);
  return found ? container_of(found, ZNode, hmap) : NULL;
}

// (lhs.score, lhs.name) < (rhs.score, rhs.name)
static bool zless(AVLNode *lhs, AVLNode *rhs) {
  ZNode *zl = container_of(lhs, ZNode, tree);
  ZNode *zr = container_of(rhs, ZNode, tree);
  if (zl->score != zr->score) {
    return zl->score < zr->score;
  }
  int rv = memcmp(zl->name, zr->name, std::min(zl->len, zr->len));
  return (rv != 0) ? (rv < 0) : (zl->len < zr->len);
}

// compare by the (score, name) tuple
static bool zless(AVLNode *lhs, double score, char const *name, size_t len) {
  ZNode *zl = container_of(lhs, ZNode, tree);
  if (zl->score != score) {
    return zl->score < score;
  }
  int rv = memcmp(zl->name, name, std::min(zl->len, len));
  return (rv != 0) ? (rv < 0) : (zl->len < len);
}

// insert into the AVL tree
static void tree_insert(ZSet *zset, ZNode *node) {
  AVLNode *parent = NULL;
  AVLNode **from  = &zset->root;
  while (*from) {
    parent = *from;
    from = zless(&node->tree, parent) ? &parent->left : &parent->right;
  }
  *from = &node->tree;
  node->tree.parent = parent;
  zset->root = avl_fix(&node->tree);
}

// update the score of an existing node
static void zset_update(ZSet *zset, ZNode *node, double score) {
  // detach the tree node
  zset->root = avl_del(&node->tree);
  avl_init(&node->tree);
  // reinsert the tree node
  node->score = score;
  tree_insert(zset, node);
}

static ZNode * znode_new(char const *name, size_t len, double score) {
  ZNode *node = (ZNode *)malloc(sizeof(ZNode) + len);  // struct + array
  assert(node);  // not a good idea in read projects
  avl_init(&node->tree);   // init AVLNode
  node->hmap.next  = NULL; // init HNode
  node->hmap.hcode = str_hash((uint8_t const *)name, len);
  node->score = score;     // init data
  node->len   = len;
  memcpy(&node->name[0], name, len);
  return node;
}

static void znode_free(ZNode *node) {
  free(node);
}

// add a new (score, name) tuple, or update the score of the existing tuple
bool zset_insert(ZSet *zset, char const *name, size_t len, double score) {
  if (ZNode *node = zset_lookup(zset, name, len)) {
    zset_update(zset, node, score);
    return false;
  }
  ZNode *node = znode_new(name, len, score);
  hm_insert(&zset->hmap, &node->hmap);
  tree_insert(zset, node);
  return true;
}

// delete a node from both the AVL tree and the hashtable
void zset_delete(ZSet *zset, ZNode *node) {
  // remove from the hashtable
  HKey key;
  key.node.hcode = node->hmap.hcode;
  key.name = node->name;
  key.len  = node->len;
  HNode *found = hm_delete(&zset->hmap, &key.node, &hcmp);
  assert(found);
  // remove from the AVL tree
  zset->root = avl_del(&node->tree);
  // deallocate the node
  znode_free(node);
}

// find the first (score, name) tuple that is >= key
// range query command: ZQUERY key score name offset limit
// 1. Seek to the first pair where pair >= (score, name).
// 2. Walk to the n-th successor/predecessor (offset).
// 3. Iterate and output (limit).
ZNode * zset_seekge(ZSet *zset, double score, char const *name, size_t len) {
  AVLNode *found = NULL;
  for (AVLNode *node = zset->root; node; ) {
    if (zless(node, score, name, len)) {
      node = node->right; // node < key
    } else {
      found = node;       // candidate
      node = node->left;
    }
  }
  return found ? container_of(found, ZNode, tree) : NULL;
}

// offset into the succeeding or preceding node in the AVL tree
ZNode * znode_offset(ZNode *node, int64_t offset) {
  AVLNode *tnode = node ? avl_offset(&node->tree, offset) : NULL;
  return tnode ? container_of(tnode, ZNode, tree) : NULL;
}

static void tree_dispose(AVLNode *node) {
  if (!node) {
    return;
  }
  tree_dispose(node->left);
  tree_dispose(node->right);
  znode_free(container_of(node, ZNode, tree));
}

// destroy the zset
void zset_clear(ZSet *zset) {
  hm_clear(&zset->hmap);
  tree_dispose(zset->root);
  zset->root = NULL;
}
