#include "byoredis/ds/heap.hh"

inline static size_t heap_parent(size_t i) {
  return (i - 1) / 2;
}

inline static size_t heap_left(size_t i) {
  return 2 * i + 1;
}

inline static size_t heap_right(size_t i) {
  return 2 * i + 2;
}

static void heap_up(HeapItem *a, size_t pos) {
  HeapItem t = a[pos];
  while (pos > 0 && t.val < a[heap_parent(pos)].val) {
    // swap with the parent
    a[pos] = a[heap_parent(pos)];
    *a[pos].ref = pos;
    pos = heap_parent(pos);
  }
  a[pos] = t;
  *a[pos].ref = pos;
}

static void heap_down(HeapItem *a, size_t pos, size_t len) {
  HeapItem t = a[pos];
  while (true) {
    // find the smallest one among the parent and their kids
    size_t l = heap_left(pos);
    size_t r = heap_right(pos);
    size_t   min_pos = pos;
    uint64_t min_val = t.val;
    if (l < len && a[l].val < min_val) {
      min_pos = l;
      min_val = a[l].val;
    }
    if (r < len && a[r].val < min_val) {
      min_pos = r;
    }
    if (min_pos == pos) {
      break;
    }
    // swap with the smallest kid
    a[pos] = a[min_pos];
    *a[pos].ref = pos;
    pos = min_pos;
  }
  a[pos] = t;
  *a[pos].ref = pos;
}

void heap_update(HeapItem *a, size_t pos, size_t len) {
  if (pos > 0 && a[pos].val < a[heap_parent(pos)].val) {
    heap_up(a, pos);
  } else {
    heap_down(a, pos, len);
  }
}

void heap_delete(std::vector<HeapItem> &a, size_t pos) {
  if (pos >= a.size()) {
    return;
  }
  // swap the erased item with the last item
  a[pos] = a.back();
  a.pop_back();
  // update the swapped item
  if (pos < a.size()) {
    heap_update(a.data(), pos, a.size());
  }
}

void heap_insert(std::vector<HeapItem> &a, HeapItem t) {
  a.push_back(t);
  heap_update(a.data(), a.size() - 1, a.size());
}

void heap_upsert(std::vector<HeapItem> &a, size_t pos, HeapItem t) {
  if (pos < a.size()) {
    a[pos] = t;       // update an existing item
  } else {
    pos = a.size();
    a.push_back(t);   // or add a new item
  }
  heap_update(a.data(), pos, a.size());
}
