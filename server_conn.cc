#include "server_conn.hh"
#include "common.hh"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

GlobalData g_data;

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

static void response_begin(Buffer &buf) {
  buf.message_begin();
}

static size_t response_size(Buffer &buf) {
  return buf.message_size();
}

static void response_end(Buffer &buf) {
  size_t msg_size = response_size(buf);
  if (msg_size > k_max_msg) {
    // rollback payload to just after header placeholder
    buf.writable_begin = buf.inflight_header_pos + 4;
    out_err(buf, ERR_TOO_BIG, "response too big");
    msg_size = response_size(buf);
  }
  uint32_t len = (uint32_t)msg_size; 
  buf.message_end(len);
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

// static void make_response(Buffer &buf, uint32_t status, uint8_t const *resp_data, size_t data_size) {
//   uint32_t resp_len = 4 + data_size;          // status_len + resp_data_len
//   buf.append((uint8_t const *)&resp_len, 4);  // header
//   buf.append((uint8_t const*)&status, 4);     // payload: status
//   buf.append(resp_data, data_size);           // payload: resp_data
// }

void do_request_and_make_response(std::vector<std::string> &cmd, Buffer &buffer) {
  if (cmd[0] == "get" && cmd.size() == 2) {
    return do_get(cmd, buffer);
  } else if (cmd[0] == "set" && cmd.size() == 3) {
    return do_set(cmd, buffer);
  } else if (cmd[0] == "del" && cmd.size() == 2) {
    return do_del(cmd, buffer);
  } else {
    return out_err(buffer, ERR_UNKNOWN, "unknown command");
  }
}

// equality comparison for `struct Entry`
// static bool entry_eq(HNode *lhs, HNode *rhs) {
//   struct Entry *le = container_of(lhs, struct Entry, node);
//   struct Entry *re = container_of(rhs, struct Entry, node);
//   return le->key == re->key;
// }

// equality comparison for `struct Entry` and `struct LookupKey`
static bool key_eq(HNode *lhs, HNode *rhs) {
  struct Entry *le     = container_of(lhs, struct Entry, node);
  struct LookupKey *rk = container_of(rhs, struct LookupKey, node);
  return le->key == rk->key;
}

// FNV hash
static uint64_t str_hash(uint8_t const *data, size_t len) {
  uint32_t h = 0x811C9DC5;
  for (size_t i = 0; i < len; i++) {
    h = (h + data[i]) * 0x1000193;
  }
  return h;
}

void out_nil(Buffer &buf) {
  buf_append_u8(buf, TAG_NIL);
}

void out_str(Buffer &buf, char const *s, size_t size) {
  buf_append_u8(buf, TAG_STR);                    // tag
  buf_append_u32(buf, (uint32_t)size);            // len
  buf_append_str(buf, (uint8_t const *)s, size);  // val
}

void out_int(Buffer &buf, int64_t val) {
  buf_append_u8(buf, TAG_INT);
  buf_append_i64(buf, val);
}

void out_dbl(Buffer &buf, double val) {
  buf_append_u8(buf, TAG_DBL);
  buf_append_dbl(buf, val);
}

void out_arr(Buffer &buf, uint32_t n) {
  buf_append_u8(buf, TAG_ARR);
  buf_append_u32(buf, n);
}

void out_err(Buffer &buf, uint32_t code, std::string const &msg) {
  buf_append_u8(buf, TAG_ERR);
  buf_append_u32(buf, code);
  buf_append_u32(buf, (uint32_t)msg.size());
  buf_append_str(buf, (uint8_t const *)msg.data(), msg.size());
}

void do_get(std::vector<std::string> &cmd, Buffer &buffer) {
  LookupKey key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t const *)key.key.data(), key.key.size());
  HNode *node = hm_lookup(&g_data.db, &key.node, &key_eq);
  if (!node) {
    return out_nil(buffer);
  }
  std::string const &val = container_of(node, struct Entry, node)->val;
  return out_str(buffer, val.data(), val.size());
}

void do_set(std::vector<std::string> &cmd, Buffer &buffer) {
  LookupKey key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t const *)key.key.data(), key.key.size());
  HNode *node = hm_lookup(&g_data.db, &key.node, &key_eq);
  if (node) {
    // found, update the value
    container_of(node, struct Entry, node)->val.swap(cmd[2]);
  } else {
    // not found, allocate & insert a new pair
    Entry *ent = new Entry();
    ent->key.swap(key.key);
    ent->val.swap(cmd[2]);
    ent->node.hcode = key.node.hcode;
    hm_insert(&g_data.db, &ent->node);
  }
  return out_nil(buffer);
}

void do_del(std::vector<std::string> &cmd, Buffer &buffer) {
  LookupKey key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t const *)key.key.data(), key.key.size());
  HNode *node = hm_delete(&g_data.db, &key.node, &key_eq);
  if (node) {  // deallocate the pair if found
    delete container_of(node, struct Entry, node);
  }
  return out_int(buffer, node ? 1 : 0);  // the number of deleted keys
}
