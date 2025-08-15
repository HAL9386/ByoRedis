#include "common.hh"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>

void msg(char const *msg) { fprintf(stderr, "%s\n", msg); }

void msg_errno(char const *msg) { fprintf(stderr, "[%d] %s\n", errno, msg); }

void die(char const *msg) {
  fprintf(stderr, "[%d] %s\n", errno, msg);
  abort();
}

void fd_set_nb(int fd) {
  errno = 0;
  int flags = fcntl(fd, F_GETFL, 0);
  if (errno) {
    die("fcntl F_GETFL error");
    return;
  }
  flags |= O_NONBLOCK;
  errno = 0;
  (void)fcntl(fd, F_SETFL, flags);
  if (errno) {
    die("fcntl F_SETFL error");
  }
}

bool read_u32(uint8_t const *&cur, uint8_t const *end, uint32_t &out) {
  if (cur + 4 > end) {
    return false;
  }
  memcpy(&out, cur, 4);
  cur += 4;
  return true;
}

bool read_str(uint8_t const *&cur, uint8_t const *end, size_t n, std::string &out) {
  if (cur + n > end) {
    return false;
  }
  out.assign(cur, cur + n);
  cur += n;
  return true;
}

void buf_append_u8(Buffer &buf, uint8_t data) {
  buf.append(&data, 1);
}

void buf_append_u32(Buffer &buf, uint32_t data) {
  buf.append((uint8_t const *)&data, 4);  // assume little-endian
}

void buf_append_i64(Buffer &buf, int64_t data) {
  buf.append((uint8_t const *)&data, 8);
}

void buf_append_dbl(Buffer &buf, double data) {
  buf.append((uint8_t const *)&data, 8);
}

void buf_append_str(Buffer &buf, uint8_t const *data, size_t len) {
  buf.append(data, len);
}
