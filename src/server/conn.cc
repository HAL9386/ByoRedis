#include "byoredis/server/conn.hh"
#include "byoredis/common/log.hh"
#include "byoredis/common/net.hh"
#include "byoredis/proto/tlv.hh"
#include "byoredis/server/commands.hh"
#include "byoredis/server/time.hh"
#include "byoredis/server/db.hh"
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

void conn_destroy(Conn *conn) {
  (void)close(conn->fd);
  g_data.fd2conn[conn->fd] = NULL;
  dlist_detach(&conn->idle_node);
  delete conn;
}

// application callback when the listening socket is ready
int32_t handle_accept(int fd) {
  // accept
  struct sockaddr_in client_addr = {};
  socklen_t addr_len = sizeof(client_addr);
  int connfd = accept(fd, (struct sockaddr *)&client_addr, &addr_len);
  if (connfd < 0) {
    msg_errno("accept() error");
    return -1;
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
  conn->last_active_ms = get_monotonic_msec();
  dlist_insert_before(&g_data.idle_list, &conn->idle_node);
  // put it into the map
  if (g_data.fd2conn.size() <= (size_t)conn->fd) {
    g_data.fd2conn.resize(conn->fd + 1);
  }
  assert(!g_data.fd2conn[conn->fd]);
  g_data.fd2conn[conn->fd] = conn;
  return 0;
}

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

static void response_begin(Buffer &buf) {
  // buf.message_begin();
  buf.push_placeholder(); // reserve space for message length
  buf_append_u32(buf, 0); // placeholder, filled by response_end()
}

static size_t response_size(Buffer &buf) {
  // return buf.message_size();
  return buf.writable_begin - buf.peek_placeholder() - 4;
}

static void response_end(Buffer &buf) {
  size_t msg_size = response_size(buf);
  if (msg_size > k_max_msg) {
    // rollback payload to just after header placeholder
    // buf.writable_begin = buf.inflight_header_pos + 4;
    buf.writable_begin = buf.peek_placeholder() + 4;
    out_err(buf, ERR_TOO_BIG, "response too big");
    msg_size = response_size(buf);
  }
  uint32_t len = (uint32_t)msg_size; 
  memcpy(&buf.buf[buf.pop_placeholder()], &len, 4);
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
  response_begin(conn->outgoing);
  do_request_and_make_response(cmd, conn->outgoing);
  response_end(conn->outgoing);
  // application logic done, remove the request from the incoming buffer
  conn->incoming.consume(4 + len);
  return true;
}

// payload
// +------|-----|------|-----|------|-----|-----|------+
// | nstr | len | str1 | len | str2 | ... | len | strn |
// +------|-----|------|-----|------|-----|-----|------+

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

void do_request_and_make_response(std::vector<std::string> &cmd, Buffer &buffer) {
  if (cmd.size() == 2 && cmd[0] == "get") {
    return do_get(cmd, buffer);
  } else if (cmd.size() == 3 && cmd[0] == "set") {
    return do_set(cmd, buffer);
  } else if (cmd.size() == 2 && cmd[0] == "del") {
    return do_del(cmd, buffer);
  } else if (cmd.size() == 1 && cmd[0] == "keys") {
    return do_keys(cmd, buffer);
  } else if (cmd.size() == 4 && cmd[0] == "zadd") {
    return do_zadd(cmd, buffer);
  } else if (cmd.size() == 3 && cmd[0] == "zrem") {
    return do_zrem(cmd, buffer);
  } else if (cmd.size() == 3 && cmd[0] == "zscore") {
    return do_zscore(cmd, buffer);
  } else if (cmd.size() == 6 && cmd[0] == "zquery") {
    return do_zquery(cmd, buffer);
  } else if (cmd.size() == 3 && cmd[0] == "zrank") {
    return do_zrank(cmd, buffer);
  } else if (cmd.size() == 6 && cmd[0] == "zcount") {
    return do_zcount(cmd, buffer);
  } else {
    return out_err(buffer, ERR_UNKNOWN, "unknown command");
  }
}
