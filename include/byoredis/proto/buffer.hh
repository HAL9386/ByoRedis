#pragma once

#include <vector>
#include <stdint.h>
#include <assert.h>

struct Buffer {
  std::vector<uint8_t> buf;
  size_t readable_begin = 0;
  size_t writable_begin = 0;

  // Stack of placeholder positions (offsets into buf) that need to be backfilled later.
  // Used for response message size or TLV array size and potentially other deferred fields (LIFO order).
  std::vector<size_t> placeholder_stack;

  explicit Buffer(size_t init_cap = 4096) { buf.resize(init_cap); }

  // actual size of the buffer, also upper bound of writable data
  size_t          capacity()      const { return buf.size(); }
  size_t          readable_size() const { return writable_begin - readable_begin; }
  size_t          writable_size() const { return capacity() - writable_begin; }
  uint8_t const * readable_data() const { return &buf[readable_begin]; }
  uint8_t       * writable_data()       { return &buf[writable_begin]; }

  void ensure_writable(size_t ensure_size); 

  void append(uint8_t const *data, size_t n); 

  void consume(size_t n); 

  void shrink_if_wasteful(size_t hard_min = 4096); 

  // Placeholder helpers for deferred backfilling
  inline void push_placeholder() { 
    placeholder_stack.push_back(writable_begin); 
  }
  inline size_t peek_placeholder() {
    assert(!placeholder_stack.empty());
    return placeholder_stack.back();
  }
  inline size_t pop_placeholder() {
    assert(!placeholder_stack.empty());
    size_t pos = placeholder_stack.back();
    placeholder_stack.pop_back();
    return pos;
  }
};
