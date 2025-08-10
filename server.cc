#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#include "common.hh"
#include <assert.h>

int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    die("socket()");
  }
  // set reuse
  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  // bind
  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = ntohs(1234),
    .sin_addr = {
      .s_addr = ntohl(0),
    },
  };
  int rv = bind(fd, (struct sockaddr const *)&addr, sizeof(addr));
  if (rv) {
    die("bind()");
  }

  // set the listen fd to nonblocking mode
  fd_set_nb(fd);

  // listen
  rv = listen(fd, SOMAXCONN);
  if (rv) {
    die("listen()");
  }

  // a map of all client connections, keyed by fd
  std::vector<Conn *> fd2conn;
  std::vector<struct pollfd> poll_args;

  // the event loop
  while (true) {
    // prepare the arguments of the poll()
    poll_args.clear();
    // put the listening sockets in the first position
    struct pollfd pfd = {
      .fd      = fd,
      .events  = POLLIN,  // want read
      .revents = 0,
    };
    poll_args.push_back(pfd);
    // the rest are connection sockets
    for (Conn *conn : fd2conn) {
      if (!conn) {
        continue;
      }
      // always poll() for error
      struct pollfd pfd = {
        .fd      = conn->fd,
        .events  = POLLERR,
        .revents = 0,
      };
      // poll() flags from the application's intent
      if (conn->want_read) {
        pfd.events |= POLLIN;
      }
      if (conn->want_write) {
        pfd.events |= POLLOUT;
      }
      poll_args.push_back(pfd);
    }
    // call poll(), wait for readiness
    int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
    if (rv < 0 && errno == EINTR) {
      continue;  // not an error
    }
    if (rv < 0) {
      die("poll()");
    }
    // handle the listening socket
    if (poll_args[0].revents) {
      if (Conn *conn = handle_accept(fd)) {
        // put it into the map
        if (fd2conn.size() <= (size_t)conn->fd) {
          fd2conn.resize(conn->fd + 1);
        }
        assert(!fd2conn[conn->fd]);
        fd2conn[conn->fd] = conn;
      }
    }
    // handle connection sockets
    for (size_t i = 1; i < poll_args.size(); i++) {  // skip the 1st
      uint32_t ready_mask = poll_args[i].revents;
      if (ready_mask == 0) {
        continue;
      }
      Conn *conn = fd2conn[poll_args[i].fd];
      if (ready_mask & POLLIN) {
        assert(conn->want_read);
        handle_read(conn);  // application logic
      }
      if (ready_mask & POLLOUT) {
        assert(conn->want_write);
        handle_write(conn);  // application logic
      }
      // close the socket from socket error or application logic
      if ((ready_mask & POLLERR) || conn->want_close) {
        (void)close(conn->fd);
        fd2conn[conn->fd] = NULL;
        delete conn;
      }
    } // for each connection socket
  } // the event loop
  return 0;
}
