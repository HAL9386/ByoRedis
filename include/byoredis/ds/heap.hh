#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>

struct HeapItem {
  uint64_t val = 0;     // heap value, the expiration time in ms
  size_t  *ref = NULL;  // points to `Entry::heap_idx`
};

void heap_update(HeapItem *a, size_t pos, size_t len);
void heap_delete(std::vector<HeapItem> &a, size_t pos);
void heap_upsert(std::vector<HeapItem> &a, size_t pos, HeapItem t);
