#include "server_conn.hh"
#include "common.hh"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
  fprintf(stderr, "new client from %u.%u.%u.%u:%u\n",
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
bool try_process_one_request(Conn *conn) {
  // try to parse the protocol: message header
  if (conn->incoming.readable_size() < 4) {
    return false;  // want read
  }
  uint32_t len = 0;
  memcpy(&len, conn->incoming.readable_data(), 4);
  if (len > k_max_msg) {
    die("request too large");
    conn->want_close = true;
    return false;  // want close
  }
  // message body
  if (4 + len > conn->incoming.readable_size()) {
    return false;  // want read
  }
  uint8_t const *request = conn->incoming.readable_data() + 4;
  // got some request, do some application logic
  std::vector<std::string> cmd;
  if (parse_req(request, len, cmd) < 0) {
    msg("bad request");
    conn->want_close = true;
    return false;  // want close
  }
  do_request_and_make_response(cmd, conn->outgoing);
  // application logic done, remove the request from the incoming buffer
  conn->incoming.consume(4 + len);
  return true;
}

// application callback when the socket is writable
void handle_write(Conn *conn) {
  assert(conn->outgoing.readable_size() > 0);
  ssize_t rv = write(conn->fd, conn->outgoing.readable_data(), conn->outgoing.readable_size());
  if (rv < 0 && errno == EAGAIN) {
    return;  // actually not ready
  }
  if (rv < 0) {
    msg_errno("write() error");
    conn->want_close = true;  // error handling
    return;
  }
  // remove written data from outgoing
  conn->outgoing.consume((size_t)rv);
  // update the readiness intention
  if (conn->outgoing.readable_size() == 0) {  // all data written
    conn->want_write = false;
    conn->want_read = true;
    conn->outgoing.shrink_if_wasteful(1u << 20);
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
    if (conn->incoming.readable_size() == 0) {
      msg("client closed");
    } else {
      msg("unexpected EOF");
    }
    conn->want_close = true;
    return;  // want close
  }
  // got some new data
  conn->incoming.append(buf, (size_t)rv);
  // parse requests and generate responses
  while (try_process_one_request(conn)) {}
  size_t unread = conn->incoming.readable_size();
  if (unread == 0) {
    conn->incoming.shrink_if_wasteful();
  } else if (conn->incoming.capacity() > (1u << 20)        // > 1MB
             && unread < (conn->incoming.capacity() >> 3)  // < 1/8
             && unread < 4096) {
    conn->incoming.shrink_if_wasteful();
  }
  // update the readiness intention
  if (conn->outgoing.readable_size() > 0) {  // has a response
    conn->want_read = false;
    conn->want_write = true;
    // The socket is likely ready to write in a request-response protocol,
    // try to write it without waiting for the next iteration.
    return handle_write(conn);
  } // else: want read
}

int32_t parse_req(uint8_t const *data, size_t size, std::vector<std::string> &out) {
  uint8_t const *end = data + size;
  uint32_t nstr = 0;
  if (!read_u32(data, end, nstr)) {
    return -1;
  }
  if (nstr > k_max_args) {
    return -1;
  }
  while (out.size() < nstr) {
    uint32_t len = 0;
    if (!read_u32(data, end, len)) {
      return -1;
    }
    out.push_back(std::string());
    if (!read_str(data, end, len, out.back())) {
      return -1;
    }
  }
  if (data != end) {
    return -1;  // trailing garbage
  }
  return 0;
}

static void make_response(Buffer &buf, uint32_t status, uint8_t const *resp_data, size_t data_size) {
  uint32_t resp_len = 4 + data_size;          // status_len + resp_data_len
  buf.append((uint8_t const *)&resp_len, 4);  // header
  buf.append((uint8_t const*)&status, 4);     // payload: status
  buf.append(resp_data, data_size);           // payload: resp_data
}

void do_request_and_make_response(std::vector<std::string> &cmd, Buffer &buffer) {
  if (cmd[0] == "get" && cmd.size() == 2) {
    auto it = g_data.find(cmd[1]);
    if (it == g_data.end()) {
      make_response(buffer, RES_NX, {}, 0);  // not found
      return;
    }
    make_response(buffer, RES_OK, (uint8_t const *)it->second.data(), it->second.size());
  } else if (cmd[0] == "set" && cmd.size() == 3) {
    g_data[cmd[1]].swap(cmd[2]);
    make_response(buffer, RES_OK, {}, 0);
  } else if (cmd[0] == "del" && cmd.size() == 2) {
    g_data.erase(cmd[1]);
    make_response(buffer, RES_OK, {}, 0);
  } else {
    make_response(buffer, RES_ERR, {}, 0);  // unrecognized command
  }
}
