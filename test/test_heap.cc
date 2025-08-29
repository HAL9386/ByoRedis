#include "byoredis/ds/heap.hh"
#include <vector>
#include <map>
#include <assert.h>

// local helpers for verifying heap shape
inline static size_t heap_left(size_t i)  { return 2 * i + 1; }
inline static size_t heap_right(size_t i) { return 2 * i + 2; }

struct Data {
  size_t heap_idx = -1;
};

struct Container {
  std::vector<HeapItem> heap;
  std::multimap<uint64_t, Data *> map;
};

static void dispose(Container &c) {
  for (auto const &p : c.map) {
    delete p.second;
  }
}

static void add(Container &c, uint64_t val) {
  Data *d = new Data();
  c.map.insert(std::make_pair(val, d));
  HeapItem item;
  item.ref = &d->heap_idx;
  item.val = val;
  heap_insert(c.heap, item);
}

static void del(Container &c, uint64_t val) {
  auto it = c.map.find(val);
  assert(it != c.map.end());
  Data *d = it->second;
  assert(c.heap.at(d->heap_idx).val == val);
  assert(c.heap.at(d->heap_idx).ref == &d->heap_idx);
  heap_delete(c.heap, d->heap_idx);
  c.map.erase(it);
  delete d;
}

static void verify(Container &c) {
  assert(c.heap.size() == c.map.size());
  for (size_t i = 0; i < c.heap.size(); i++) {
    size_t l = heap_left(i);
    size_t r = heap_right(i);
    assert(l >= c.heap.size() || c.heap[i].val <= c.heap[l].val);
    assert(r >= c.heap.size() || c.heap[i].val <= c.heap[r].val);
    assert(*c.heap[i].ref == i);
  }
}

static void test_case(size_t sz) {
  for (uint32_t j = 0; j < 2 + sz * 2; j++) {
    Container c;
    for (uint32_t i = 0; i < sz; i++) {
      add(c, 1 + i * 2);
    }
    verify(c);
    add(c, j);
    verify(c);
    dispose(c);
  }
  for (uint32_t j = 0; j < sz; j++) {
    Container c;
    for (uint32_t i = 0; i < sz; i++) {
      add(c, i);
    }
    verify(c);
    del(c, j);
    verify(c);
    dispose(c);
  }
}

int main() {
  for (uint32_t i = 0; i < 200; i++) {
    test_case(i);
  }
  return 0;
}
