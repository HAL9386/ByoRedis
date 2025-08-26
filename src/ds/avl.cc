#include <assert.h>
#include "byoredis/ds/avl.hh"

static uint32_t max(uint32_t lhs, uint32_t rhs) {
  return lhs > rhs ? lhs : rhs;
}

// maintain the height and size of the node
static void avl_update(AVLNode *node) {
  node->height = 1 + max(avl_height(node->left), avl_height(node->right));
  node->size   = 1 + avl_size(node->left) + avl_size(node->right);
}

// The rotated node links to the parent, but the parent-to-child link is not updated here.
// cuz the rotated node may be a root node without a parent,
// and only the caller knows how to link a root node, so this link is left to the caller.
static AVLNode *rot_left(AVLNode *node) {
  AVLNode *parent = node->parent;
  AVLNode *new_node = node->right;
  AVLNode *inner = new_node->left;
  // node <-> inner
  node->right = inner;
  if (inner) {
    inner->parent = node;
  }
  // parent <- new_node
  new_node->parent = parent;
  // new_node <-> node
  new_node->left = node;
  node->parent = new_node;
  // auxiliary data
  avl_update(node);
  avl_update(new_node);
  return new_node;
}

static AVLNode *rot_right(AVLNode *node) {
  AVLNode *parent = node->parent;
  AVLNode *new_node = node->left;
  AVLNode *inner = new_node->right;
  // node <-> inner
  node->left = inner;
  if (inner) {
    inner->parent = node;
  }
  // parent <- new_node
  new_node->parent = parent;
  // new_node <-> node
  new_node->right = node;
  node->parent = new_node;
  // auxiliary data
  avl_update(node);
  avl_update(new_node);
  return new_node;
}

static AVLNode *avl_fix_left(AVLNode *node) {
  if (avl_height(node->left->left) < avl_height(node->left->right)) {
    // LR
    node->left = rot_left(node->left);
  } // LL
  return rot_right(node);
}

static AVLNode *avl_fix_right(AVLNode *node) {
  if (avl_height(node->right->right) < avl_height(node->right->left)) {
    // RL
    node->right = rot_right(node->right);
  } // RR
  return rot_left(node);
}

// fix imbalance nodes and maintain invariants until the root is reached
AVLNode *avl_fix(AVLNode *node) {
  while (true) {
    AVLNode **from = &node;  // save the fixed subtree here
    AVLNode *parent = node->parent;
    if (parent) {
      // attach the fixed subtree to its parent
      from = parent->left == node ? &parent->left : &parent->right;
    } // else: save to the local variable `node`
    // auxiliary data
    avl_update(node);
    // fix the height difference of 2
    uint32_t lh = avl_height(node->left);
    uint32_t rh = avl_height(node->right);
    if (lh == rh + 2) {
      *from = avl_fix_left(node);
    } else if (rh == lh + 2) {
      *from = avl_fix_right(node);
    }
    // root node, stop
    if (!parent) {
      return *from;
    }
    // continue to the parent node cuz its height may be changed
    node = parent;
  }
}

// detach a node where 1 of its children is NULL
static AVLNode *avl_del_easy(AVLNode *node) {
  assert(!node->left || !node->right);
  AVLNode *child = node->left ? node->left : node->right;  // can be NULL
  AVLNode *parent = node->parent;
  if (child) {
    child->parent = parent;
  }
  if (!parent) {
    return child;  // new root
  }  
  AVLNode **from = parent->left == node ? &parent->left : &parent->right;
  *from = child;
  return avl_fix(parent);  // fix from the parent
}

// detach a node and return the new root of the tree
AVLNode *avl_del(AVLNode *node) {
  // the easy case of 0 or 1 child
  if (!node->left || !node->right) {
    return avl_del_easy(node);
  }
  // find the in-order successor (the smallest node in the right subtree)
  AVLNode *victim = node->right;
  while (victim->left) {
    victim = victim->left;
  }
  // detach the successor
  AVLNode *root = avl_del_easy(victim);
  // swap with the successor
  *victim = *node;  // copy all fields, left, right, parent
  if (victim->left) {
    victim->left->parent = victim;
  }
  if (victim->right) {
    victim->right->parent = victim;
  }
  // attach the successor to the parent, or update the root pointer
  AVLNode **from = &root;
  AVLNode *parent = node->parent;
  if (parent) {
    from = parent->left == node ? &parent->left : &parent->right;
  }
  *from = victim;
  return root;
}
