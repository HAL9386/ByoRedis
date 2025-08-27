#include "byoredis/proto/buffer.hh"
#include <string.h>

void Buffer::ensure_writable(size_t ensure_size) {
  if (writable_size() >= ensure_size) {
    return;
  }
  size_t const unread_len = readable_size();
  size_t old_readable_begin = readable_begin;
  // solution 1: move the readable data(not consumed yet) to the front
  if (readable_begin + writable_size() >= ensure_size) {
    memmove(&buf[0], &buf[readable_begin], unread_len);
    readable_begin = 0;
    writable_begin = unread_len;

    // adjust all deferred placeholders
    if (!placeholder_stack.empty()) {
      size_t shift = old_readable_begin;
      for (size_t &pos : placeholder_stack) {
        if (pos >= old_readable_begin) {
          pos -= shift;
        }
      }
    }
    return;
  }
  // solution 2: resize the buffer
  size_t new_cap = std::max(capacity() * 2, unread_len + ensure_size);
  std::vector<uint8_t> new_buf;
  new_buf.resize(new_cap);
  memcpy(&new_buf[0], &buf[readable_begin], unread_len);
  size_t shift = old_readable_begin;
  buf.swap(new_buf);
  readable_begin = 0;
  writable_begin = unread_len;
  if (!placeholder_stack.empty()) {
    for (size_t &pos : placeholder_stack) {
      if (pos >= shift) {
        pos -= shift;
      }
    }
  }
}

void Buffer::append(uint8_t const *data, size_t n) {
  ensure_writable(n);
  memcpy(&buf[writable_begin], data, n);
  writable_begin += n;
}

void Buffer::consume(size_t n) {
  n = std::min(n, readable_size());
  readable_begin += n;
  if (readable_begin >= writable_begin) {
    // all consumed, reset
    readable_begin = 0;
    writable_begin = 0;
  }
}

void Buffer::shrink_if_wasteful(size_t hard_min) {
  size_t const unread_len = readable_size();
  if (capacity() > std::max(hard_min, unread_len * 4)) {
    size_t new_cap = std::max(hard_min, unread_len * 2);
    std::vector<uint8_t> new_buf;
    new_buf.resize(new_cap);
    memcpy(&new_buf[0], &buf[readable_begin], unread_len);
    buf.swap(new_buf);
    size_t shift = readable_begin;
    readable_begin = 0;
    writable_begin = unread_len;
    if (!placeholder_stack.empty()) {
      for (size_t &pos : placeholder_stack) {
        if (pos >= shift) {
          pos -= shift;
        }
      }
    }
  }
}
