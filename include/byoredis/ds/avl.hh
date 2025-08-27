#pragma once

#include <stddef.h>
#include <stdint.h>

struct AVLNode {
  AVLNode *parent = NULL;
  AVLNode *left   = NULL;
  AVLNode *right  = NULL;
  uint32_t height = 0;  // Height of the node in the tree
  size_t   size   = 0;  // Size of the subtree rooted at this node
};

inline void avl_init(AVLNode *node) {
  node->left = node->right = node->parent = NULL;
  node->height = 1;
  node->size = 1;
}

// helpers

inline uint32_t avl_height(AVLNode *node) { return node ? node->height : 0; }
inline uint32_t avl_size(AVLNode *node) { return node ? node->size : 0; }

// API

AVLNode * avl_fix(AVLNode *node);
AVLNode * avl_del(AVLNode *node);
AVLNode * avl_offset(AVLNode *node, int64_t offset);
