#include "client_api.hh"
#include "common.hh"

#include <vector>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

// payload
// +------|-----|------|-----|------|-----|-----|------+
// | nstr | len | str1 | len | str2 | ... | len | strn |
// +------|-----|------|-----|------|-----|-----|------+

int32_t send_req(int fd, std::vector<std::string> const &cmd) {
  // calculate the total payload size
  size_t payload_size = 4;  // for nstr
  for (std::string const &s : cmd) {
    payload_size += 4 + s.size();  // for len + str
  }
  if (payload_size > k_max_msg) {
    msg("request too large");
    return -1;
  }
  
  // prepare the buffer
  std::vector<uint8_t> wbuf;
  wbuf.resize(4 + payload_size);
  memcpy(&wbuf[0], &payload_size, 4);            // header

  // write nstr
  uint32_t nstr = (uint32_t)cmd.size();
  memcpy(&wbuf[4], &nstr, 4);                    // nstr
  // write each string
  size_t cur = 8;                                // current offset
  for (std::string const &s : cmd) {
    uint32_t len = (uint32_t)s.size();
    memcpy(&wbuf[cur], &len, 4);                 // len
    memcpy(&wbuf[cur + 4], s.data(), s.size());  // str
    cur += 4 + s.size();
  }
  return write_all(fd, (char const *)&wbuf[0], 4 + payload_size);
}

int32_t read_res(int fd) {
  std::vector<uint8_t> rbuf;
  rbuf.resize(4);  // 4 bytes header
  errno = 0;
  int32_t err = read_full(fd, (char *)&rbuf[0], 4);
  if (err) {
    if (errno == 0) {
      msg("EOF");
    } else {
      msg_errno("read_res error");
    }
    return err;
  }
  uint32_t len = 0;  // payload size
  memcpy(&len, &rbuf[0], 4);
  if (len > k_max_msg) {
    msg("response text too large");
    return -1;
  }
  // reply body
  rbuf.resize(4 + len);
  err = read_full(fd, (char *)&rbuf[4], len);
  if (err) {
    msg("read_res error");
    return err;
  }
  // Response from server(aka payload)
  // +--------|---------+
  // | status | data... |
  // +--------|---------+
  // print the result
  uint32_t rescode = 0;
  if (len < 4) {
    msg("bad response");  // not enough for a status
    return -1;
  }
  memcpy(&rescode, &rbuf[4], 4);
  printf("server says: [%u] %.*s\n", rescode, len - 4, &rbuf[8]);
  return 0;
}

int32_t read_full(int fd, char *buf, size_t n) {
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

int32_t write_all(int fd, char const *buf, size_t n) {
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
