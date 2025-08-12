#ifndef COMMON_HH
#define COMMON_HH

#include <vector>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

size_t const k_max_msg = 32 << 20;

struct Buffer {
  std::vector<uint8_t> buf;
  size_t readable_begin = 0;
  size_t writable_begin = 0;

  explicit Buffer(size_t init_cap = 4096) { buf.resize(init_cap); }

  // actual size of the buffer, also upper bound of writable data
  size_t          capacity()      const { return buf.size(); }
  size_t          readable_size() const { return writable_begin - readable_begin; }
  size_t          writable_size() const { return capacity() - writable_begin; }
  uint8_t const * readable_data() const { return &buf[readable_begin]; }
  uint8_t       * writable_data()       { return &buf[writable_begin]; }

  void ensure_writable(size_t ensure_size) {
    if (writable_size() >= ensure_size) {
      return;
    }
    size_t const unread_len = readable_size();
    // solution 1: move the readable data(not consumed yet) to the front
    if (readable_begin + writable_size() >= ensure_size) {
      memmove(&buf[0], &buf[readable_begin], unread_len);
      readable_begin = 0;
      writable_begin = unread_len;
      return;
    }
    // solution 2: resize the buffer
    size_t new_cap = std::max(capacity() * 2, unread_len + ensure_size);
    std::vector<uint8_t> new_buf;
    new_buf.resize(new_cap);
    memcpy(&new_buf[0], &buf[readable_begin], unread_len);
    buf.swap(new_buf);
    readable_begin = 0;
    writable_begin = unread_len;
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
      readable_begin = 0;
      writable_begin = unread_len;
    }
  }
};

// Shared utilities
void msg(const char *msg);
void msg_errno(char const *msg);
void die(const char *msg);
void fd_set_nb(int fd);
void buf_append(std::vector<uint8_t> &buf, uint8_t const *data, size_t len);
void buf_consume(std::vector<uint8_t> &buf, size_t len);
int32_t read_full(int fd, uint8_t *buf, size_t n);
int32_t write_all(int fd, uint8_t const *buf, size_t n);

#endif
