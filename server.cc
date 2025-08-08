#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include "common.hh"

const size_t k_max_msg = 4096;

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
  int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (rv) {
    die("bind()");
  }

  // listen
  rv = listen(fd, SOMAXCONN);
  if (rv) {
    die("listen()");
  }
  while (true) {
    // accept
    struct sockaddr_in client_addr = {};
    socklen_t addr_len = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addr_len);
    if (connfd < 0) {
      msg("accept() error");
      continue;
    }
    while (true) {
      // here the server only serves one client connection at once
      int32_t err = one_request(connfd, k_max_msg);
      if (err) {
        break;
      }
    }
    close(connfd);
  }
}
