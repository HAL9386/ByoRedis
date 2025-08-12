#include "common.hh"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

int32_t read_full(int fd, uint8_t *buf, size_t n) {
  while (n > 0) {
    ssize_t rv = read(fd, buf, n);
    if (rv <= 0) {
      return -1;  // error, or unexpected EOF
    }
    assert((size_t)rv <= n);
    n -= (size_t)rv;
    buf += rv;
  }
  return 0;
}

int32_t write_all(int fd, uint8_t const *buf, size_t n) {
  while (n > 0) {
    ssize_t rv = write(fd, buf, n);
    if (rv <= 0) {
      return -1;  // error
    }
    assert((size_t)rv <= n);
    n -= (size_t)rv;
    buf += rv;
  }
  return 0;
}
