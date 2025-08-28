#include "byoredis/client/api.hh"
#include "byoredis/common/log.hh"
#include "byoredis/proto/tlv.hh"
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sstream>

// payload
// +------|-----|------|-----|------|-----|-----|------+
// | nstr | len | str1 | len | str2 | ... | len | strn |
// +------|-----|------|-----|------|-----|-----|------+

int32_t send_req(int fd, std::vector<std::string> const &cmd) {
  // calculate the total payload size
  uint32_t payload_size = 4;  // for nstr
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
  // print the result
  int32_t rv = print_response((uint8_t *)&rbuf[4], len);
  if (rv > 0 && (uint32_t)rv != len) {
    msg("print_response: incomplete data");
    return -1;
  }
  return rv;
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

int32_t print_response(uint8_t const *data, size_t size) {
  if (size < 1) {
    msg("empty response");
    return -1;
  }
  switch (data[0]) {
  case TAG_NIL:
    printf("(nil)\n");
    return 1;
  case TAG_ERR:
    if (size < 1 + 8) {
      msg("bad error response");
      return -1;
    }
    {
      int32_t code = 0;
      uint32_t len = 0;
      memcpy(&code, &data[1], 4);     // error code, skip 1 byte(tag)
      memcpy(&len, &data[1 + 4], 4);  // error message len
      if (size < 1 + 8 + len) {
        msg("bad error response");
        return -1;
      }
      printf("(err) %d %.*s\n", code, len, &data[1 + 8]);
      return 1 + 8 + len;
    }
  case TAG_STR:
    if (size < 1 + 4) {
      msg("bad string response");
      return -1;
    }
    {
      uint32_t len = 0;
      memcpy(&len, &data[1], 4);  // string len
      if (size < 1 + 4 + len) {
        msg("bad string response");
        return -1;
      }
      printf("(str) %.*s\n", len, &data[1 + 4]);
      return 1 + 4 + len;
    }
  case TAG_INT:
    if (size < 1 + 8) {
      msg("bad int response");
      return -1;
    }
    {
      int64_t val = 0;
      memcpy(&val, &data[1], 8);
      printf("(int) %ld\n", val);
      return 1 + 8;
    }
  case TAG_DBL:
    if (size < 1 + 8) {
      msg("bad dbl response");
      return -1;
    }
    {
      double val = 0.0;
      memcpy(&val, &data[1], 8);
      printf("(dbl) %g\n", val);
      return 1 + 8;
    }
  case TAG_ARR:
    if (size < 1 + 4) {
      msg("bad array response");
      return -1;
    }
    {
      uint32_t len = 0;
      memcpy(&len, &data[1], 4);  // array len
      printf("(arr) len=%u\n", len);
      size_t arr_bytes = 1 + 4;  // 1 byte(tag) + 4 byte(len)
      for (uint32_t i = 0; i < len; i++) {
        int32_t rv = print_response(&data[arr_bytes], size - arr_bytes);
        if (rv < 0) {
          msg("bad array response");
          return rv;
        }
        arr_bytes += (size_t)rv;
      }
      printf("(arr) end\n");
      return (int32_t)arr_bytes;
    }
  default:
    msg("unknown response tag");
    return -1;
  }
}

// helper to split a command line by whitespace into argv-like tokens
static std::vector<std::string> split_cmd(std::string const &line) {
  std::vector<std::string> out;
  std::istringstream iss(line);
  std::string tok;
  while (iss >> tok) {
    out.push_back(tok);
  }
  return out;
}

int32_t multi_req(int fd) {
  std::vector<std::string> commands = {
    "set k 1",
    "get k",
    "zadd z 10 a",
    "zadd z 20 b",
    "zquery z 0 0 0 10",
  };
  for (std::string const &cmd : commands) {
    std::vector<std::string> tokens = split_cmd(cmd);
    if (tokens.empty()) {
      continue;
    }
    int32_t err = send_req(fd, tokens);
    if (err < 0) return err;
    err = read_res(fd);
    if (err < 0) return err;
  }
  return 0;
}
