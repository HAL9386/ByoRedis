#ifndef COMMON_HH
#define COMMON_HH

#include <vector>
#include <string>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

size_t const k_max_msg  = 32 << 20;
size_t const k_max_args = 200 * 1000;

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

  size_t message_begin() {
    assert(!inflight);
    inflight = true;
    inflight_header_pos = writable_begin;
    uint32_t zero = 0;
    append((uint8_t const *)&zero, 4);
    return inflight_header_pos;
  }

  size_t message_size() {
    assert(inflight);
    return writable_begin - inflight_header_pos - 4;
  }

  void message_end(uint32_t msg_size) {
    assert(inflight);
    // size_t payload_size = writable_begin - inflight_header_pos - 4;
    // uint32_t len = (uint32_t)payload_size;
    // memcpy(&buf[inflight_header_pos], &len, 4);
    memcpy(&buf[inflight_header_pos], &msg_size, 4);
    inflight = false;
  }

  void ensure_writable(size_t ensure_size) {
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

  void append(uint8_t const *data, size_t n) {
    ensure_writable(n);
    memcpy(&buf[writable_begin], data, n);
    writable_begin += n;
  }

  void consume(size_t n) {
    n = std::min(n, readable_size());
    readable_begin += n;
    if (readable_begin >= writable_begin) {
      // all consumed, reset
      readable_begin = 0;
      writable_begin = 0;
    }
  }

  void shrink_if_wasteful(size_t hard_min = 4096) {
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
};

void buf_append_u8(Buffer &buf, uint8_t data);
void buf_append_u32(Buffer &buf, uint32_t data);
void buf_append_i64(Buffer &buf, int64_t data);
void buf_append_dbl(Buffer &buf, double data);
void buf_append_str(Buffer &buf, uint8_t const *data, size_t len);

void out_nil(Buffer &buf);
void out_str(Buffer &buf, char const *s, size_t size);
void out_int(Buffer &buf, int64_t val);
void out_dbl(Buffer &buf, double val);
void out_arr(Buffer &buf, uint32_t n);
void out_err(Buffer &buf, uint32_t code, std::string const &msg);

// Shared utilities
void msg(const char *msg);
void msg_errno(char const *msg);
void die(const char *msg);
void fd_set_nb(int fd);

// read 4 bytes as uint32_t, and move cur forward by 4
bool read_u32(uint8_t const *&cur, uint8_t const *end, uint32_t &out);
// read n bytes as string, and move cur forward by n
bool read_str(uint8_t const *&cur, uint8_t const *end, size_t n, std::string &out);

#endif
