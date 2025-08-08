#include "common.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void msg(const char *msg) {
  fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg) {
  int err = errno;
  fprintf(stderr, "[%d] %s\n", err, msg);
  abort();
}

static int32_t read_full(int fd, char *buf, size_t n) {
  while (n > 0) {
    ssize_t rv = read(fd, buf, n);
    if (rv <= 0) {
      return -1;  // error, or EOF
    }
    n -= rv;
    buf += rv;
  }
  return 0;
}

static int32_t write_all(int fd, const char *buf, size_t n) {
  while (n > 0) {
    ssize_t rv = write(fd, buf, n);
    if (rv <= 0) {
      return -1;
    }
    n -= rv;
    buf += rv;
  }
  return 0;
}

static int32_t one_request(int connfd, uint32_t MAX_MSG_LEN) {
  char rbuf[4 + MAX_MSG_LEN];  // 4 bytes header
  errno = 0;
  int32_t err = 0;
  err = read_full(connfd, rbuf, 4);
  if (err) {
    msg(errno == 0 ? "EOF" : "read() error");
    return err;
  }
  uint32_t msg_len = 0;
  memcpy(&msg_len, rbuf, 4);
  if (msg_len > MAX_MSG_LEN) {
    msg("too long");
    return -1;
  }
  err = read_full(connfd, rbuf + 4, msg_len);
  if (err) {
    msg("read() error");
    return err;
  }
  fprintf(stdout, "client says: %.*s\n", msg_len, &rbuf[4]);
  const char reply[] = "world";
  char wbuf[4 + sizeof(reply)];
  msg_len = (uint32_t) strlen(reply);
  memcpy(wbuf, &msg_len, 4);
  memcpy(wbuf + 4, reply, msg_len);
  return write_all(connfd, wbuf, 4 + msg_len);
}

static int32_t query(int fd, const char *text, uint32_t MAX_MSG_LEN) {
  uint32_t msg_len = (uint32_t) strlen(text);
  int32_t err = 0;
  if (msg_len > MAX_MSG_LEN) {
    msg("too long");
    return -1;
  }
  char wbuf[4 + msg_len];
  memcpy(wbuf, &msg_len, 4);
  memcpy(wbuf + 4, text, msg_len);
  if (err = write_all(fd, wbuf, 4 + msg_len)) {
    return err;
  }
  char rbuf[4 + MAX_MSG_LEN + 1];
  errno = 0;
  err = read_full(fd, rbuf, 4);
  if (err) {
    msg(errno == 0 ? "EOF" : "read() error");
    return err;
  }
  memcpy(&msg_len, rbuf, 4);
  if (msg_len > MAX_MSG_LEN) {
    msg("too long");
    return -1;
  }
  err = read_full(fd, rbuf + 4, msg_len);
  if (err) {
    msg("read() error");
    return err;
  }
  printf("server says: %.*s\n", msg_len, &rbuf[4]);
  return 0;
}