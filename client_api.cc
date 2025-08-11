#include "client_api.hh"
#include "common.hh"

#include <vector>
#include <string.h>
#include <stdio.h>
#include <errno.h>

int32_t send_req(int fd, uint8_t const *text, size_t len) {
  if (len > k_max_msg) {
    die("request text too large");
    return -1;
  }
  std::vector<uint8_t> wbuf;
  buf_append(wbuf, (uint8_t const *)&len, 4);
  buf_append(wbuf, text, len);
  return write_all(fd, wbuf.data(), wbuf.size());
}

int32_t read_res(int fd) {
  // 4 bytes header
  std::vector<uint8_t> rbuf;
  rbuf.resize(4);
  errno = 0;
  int32_t err = read_full(fd, &rbuf[0], 4);
  if (err) {
    if (errno == 0) {
      msg("EOF");
    } else {
      msg_errno("read_res error");
    }
    return err;
  }
  uint32_t len = 0;
  memcpy(&len, rbuf.data(), 4);
  if (len > k_max_msg) {
    die("response text too large");
    return -1;
  }
  // message body
  rbuf.resize(4 + len);
  err = read_full(fd, &rbuf[4], len);
  if (err) {
    msg_errno("read_res error");
    return err;
  }
  // do something
  printf("len:%u data:%.*s\n", len, len < 100 ? len : 100, &rbuf[4]);
  return 0;
}
