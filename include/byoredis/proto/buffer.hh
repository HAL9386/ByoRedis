#pragma once

#include <vector>
#include <stdint.h>
#include <assert.h>

struct Buffer {
  std::vector<uint8_t> buf;
  size_t readable_begin = 0;
  size_t writable_begin = 0;

  // In-flight message(4 bytes header placeholder for message length)
  bool   inflight = false;
  size_t inflight_header_pos = 0;

  explicit Buffer(size_t init_cap = 4096) { buf.resize(init_cap); }

  // actual size of the buffer, also upper bound of writable data
  size_t          capacity()      const { return buf.size(); }
  size_t          readable_size() const { return writable_begin - readable_begin; }
  size_t          writable_size() const { return capacity() - writable_begin; }
  uint8_t const * readable_data() const { return &buf[readable_begin]; }
  uint8_t       * writable_data()       { return &buf[writable_begin]; }

  size_t message_begin();

  size_t message_size() {
    assert(inflight);
    return writable_begin - inflight_header_pos - 4;
  }

  void message_end(uint32_t msg_size);

  void ensure_writable(size_t ensure_size); 

  void append(uint8_t const *data, size_t n); 

  void consume(size_t n); 

  void shrink_if_wasteful(size_t hard_min = 4096); 
};
