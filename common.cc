#include "common.hh"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>

void msg(char const *msg) {
  fprintf(stderr, "%s\n", msg);
}

void msg_errno(char const *msg) {
  fprintf(stderr, "[%d] %s\n", errno, msg);
}

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

// application callback when the listening socket is ready
Conn *handle_accept(int fd) {
  // accept
  struct sockaddr_in client_addr = {};
  socklen_t addr_len = sizeof(client_addr);
  int connfd = accept(fd, (struct sockaddr *)&client_addr, &addr_len);
  if (connfd < 0) {
    msg_errno("accept() error");
    return NULL;
  }
  uint32_t ip = client_addr.sin_addr.s_addr;
  fprintf(stderr, "new client from %u.%u.%u.%u\n",
    ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24,
    ntohs(client_addr.sin_port)
  );
  // set the new connection non-blocking
  fd_set_nb(connfd);
  // create a new connection object
  Conn *conn = new Conn();
  conn->fd = connfd;
  conn->want_read = true;
  return conn;
}

// process 1 request if there is enough data in the incoming buffer
bool try_one_request(Conn *conn) {
  // try to parse the protocol: message header
  if (conn->incoming.size() < 4) {
    return false;  // want read
  }
  uint32_t len = 0;
  memcpy(&len, conn->incoming.data(), 4);
  if (len > k_max_msg) {
    die("request too large");
    conn->want_close = true;
    return false;
  }
  // message body
  if (4 + len > conn->incoming.size()) {
    return false;  // want read
  }
  uint8_t const *request = &conn->incoming[4];
  // got some request, do some application logic
  printf("client says: len:%d data:%.*s\n",
    len, len < 100 ? len : 100, request);
  // generate the response
  buf_append(conn->outgoing, (uint8_t const *)&len, 4);
  buf_append(conn->outgoing, request, len);
  // application logic done, remove the request from the incoming buffer
  buf_consume(conn->incoming, 4 + len);
  return true;
}

// application callback when the socket is writable
void handle_write(Conn *conn) {
  assert(conn->outgoing.size() > 0);
  ssize_t rv = write(conn->fd, &conn->outgoing[0], conn->outgoing.size());
  if (rv < 0 && errno == EAGAIN) {
    return;  // actually not ready
  }  
  if (rv < 0) {
    msg_errno("write() error");
    conn->want_close = true;  // error handling
    return;
  }
  // remove written data from outgoing
  buf_consume(conn->outgoing, (size_t)rv);
  // update the readiness intention
  if (conn->outgoing.empty()) {  // all data written
    conn->want_write = false;
    conn->want_read = true;
  } // else: want write
}

// application callback when the socket is readable
void handle_read(Conn *conn) {
  // read some data
  uint8_t buf[64 * 1024];
  ssize_t rv = read(conn->fd, buf, sizeof(buf));
  if (rv < 0 && errno == EAGAIN) {
    return;  // actually not ready
  }
  // handle IO error
  if (rv < 0) {
    msg_errno("read() error");
    conn->want_close = true;
    return;
  }
  // handle EOF
  if (rv == 0) {
    if (conn->incoming.size() == 0) {
      msg("client closed");
    } else {
      msg("unexpected EOF");
    }
    conn->want_close = true;
    return;  // want close
  }
  // got some new data
  buf_append(conn->incoming, buf, (size_t)rv);
  // parse requests and generate responses
  while (try_one_request(conn)) {}
  // update the readiness intention
  if (conn->outgoing.size() > 0) {  // has a response
    conn->want_read = false;
    conn->want_write = true;
    // The socket is likely ready to write in a request-response protocol,
    // try to write it without waiting for the next iteration.
    return handle_write(conn);
  } // else: want read
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
