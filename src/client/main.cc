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

#include "byoredis/common/log.hh"
#include "byoredis/client/api.hh"

int main(int argc, char **argv) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    die("socket()");
  }
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(1234);
  addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);  // connect to localhost
  int rv = connect(fd, (struct sockaddr const *)&addr, sizeof(addr));
  if (rv) {
    die("connect()");
  }

  // multi request
  if (argc == 2 && strcmp(argv[1], "--multi") == 0) {
    (void)multi_req(fd);
    close(fd);
    return 0;
  }

  std::vector<std::string> cmd;
  for (int i = 1; i < argc; i++) {
    cmd.push_back(argv[i]);
  }
  int32_t err = send_req(fd, cmd);
  if (err) {
    goto L_DONE;
  }
  err = read_res(fd);
  if (err) {
    goto L_DONE;
  }
L_DONE:
  close(fd);
  return 0;
}