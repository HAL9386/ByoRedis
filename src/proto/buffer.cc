#include "byoredis/proto/buffer.hh"
#include <string.h>

size_t Buffer::message_begin() {
  assert(!inflight);
  inflight = true;
  inflight_header_pos = writable_begin;
  uint32_t zero = 0;
  append((uint8_t const *)&zero, 4);
  return inflight_header_pos;
}

void Buffer::message_end(uint32_t msg_size) {
  assert(inflight);
  memcpy(&buf[inflight_header_pos], &msg_size, 4);
  inflight = false;
}

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
    if (inflight && inflight_header_pos >= old_readable_begin) {
      size_t shift = old_readable_begin;
      inflight_header_pos -= shift;
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
  if (inflight && inflight_header_pos >= shift) {
    inflight_header_pos -= shift;
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
    if (inflight && inflight_header_pos >= shift) {
      inflight_header_pos -= shift;
    }
  }
}
