#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <assert.h>
#include <sys/epoll.h>

#include "byoredis/common/log.hh"
#include "byoredis/common/net.hh"
#include "byoredis/server/conn.hh"
#include "byoredis/server/db.hh"
#include "byoredis/server/time.hh"

int main() {
  // initialization
  dlist_init(&g_data.idle_list);
  thread_pool_init(&g_data.thread_pool, 4);

  // the listening socket
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    die("socket()");
  }
  // set reuse
  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  // bind
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(1234);
  addr.sin_addr.s_addr = ntohl(0);
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

  // create epoll instance
  g_data.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  if (g_data.epoll_fd < 0) {
    die("epoll_create1()");
  }
  struct epoll_event ev = {};
  ev.data.fd = fd;
  ev.events = EPOLLIN | EPOLLERR; // monitor listen socket for new connections
  if (epoll_ctl(g_data.epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
    die("epoll_ctl(ADD listen)");
  }

  std::vector<struct epoll_event> events(1024);

  // the event loop
  while (true) {
    int32_t timeout_ms = next_timer_ms();
    int n = epoll_wait(g_data.epoll_fd, events.data(), (int)events.size(), timeout_ms);
    if (n < 0 && errno == EINTR) {
      continue;  // not an error
    }
    if (n < 0) {
      die("epoll_wait()");
    }
    for (int i = 0; i < n; i++) {
      int evfd = events[i].data.fd;
      uint32_t ready_mask = events[i].events;
      if (evfd == fd) {
        if (ready_mask & EPOLLIN) {
          handle_accept(fd);
        }
        continue;
      }
      Conn *conn = (evfd >= 0 && (size_t)evfd < g_data.fd2conn.size()) ? g_data.fd2conn[evfd] : NULL;
      if (!conn) {
        continue;
      }

      // update the idle timer by moving conn to the end of the list
      conn->last_active_ms = get_monotonic_msec();
      dlist_detach(&conn->idle_node);
      dlist_insert_before(&g_data.idle_list, &conn->idle_node);

      // handle IO
      if (ready_mask & EPOLLIN) {
        assert(conn->want_read);
        handle_read(conn);  // application logic
      }
      if (ready_mask & EPOLLOUT) {
        assert(conn->want_write);
        handle_write(conn);  // application logic
      }
      // close the socket from socket error or application logic
      if ((ready_mask & (EPOLLERR | EPOLLHUP)) || conn->want_close) {
        conn_destroy(conn);
      }
    }
    // handle timers
    process_timers();
  } // the event loop
  return 0;
}
