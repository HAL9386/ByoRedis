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

// append to the back
void buf_append(std::vector<uint8_t> &buf, uint8_t const *data, size_t len) {
  buf.insert(buf.end(), data, data + len);
}

// remove from the front
void buf_consume(std::vector<uint8_t> &buf, size_t len) {
  if (len > buf.size()) {
    die("buf_consume: length exceeds buffer size");
  }
  buf.erase(buf.begin(), buf.begin() + len);
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
