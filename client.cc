#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include "common.hh"

const size_t k_max_msg = 4096;

int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    die("socket()");
  }
  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = ntohs(1234),
    .sin_addr = {
      .s_addr = ntohl(INADDR_LOOPBACK),  // connect to localhost
    },
  };
  int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (rv) {
    die("connect()");
  }
  // multi requests
  int32_t err = 0;
  err = query(fd, "hello1", k_max_msg);
  if (err) {
    goto L_DONE;
  }
  err = query(fd, "hello2", k_max_msg);
  if (err) {
    goto L_DONE;
  }
  err = query(fd, "hello3", k_max_msg);
  if (err) {
    goto L_DONE;
  }
L_DONE:
  close(fd);
  return 0;
}