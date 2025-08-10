#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>

#include "common.hh"

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
  int rv = connect(fd, (struct sockaddr const *)&addr, sizeof(addr));
  if (rv) {
    die("connect()");
  }
  // multiple pipelined requests
  std::vector<std::string> query_list = {
    "hello1", "hello2", "hello3",
    // a large message requires multiple event loop iterations
    std::string(k_max_msg, 'z'),
    "hello5",
  };
  for (std::string const &s : query_list) {
    int32_t err = send_req(fd, (uint8_t *)s.data(), s.size());
    if (err) {
      goto L_DONE;
    }
  }
  for (size_t i = 0; i < query_list.size(); i++) {
    int32_t err = read_res(fd);
    if (err) {
      goto L_DONE;
    }
  }
L_DONE:
  close(fd);
  return 0;
}