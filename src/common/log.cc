#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

void msg(char const *msg) { fprintf(stderr, "%s\n", msg); }
void msg_errno(char const *msg) { fprintf(stderr, "[%d] %s\n", errno, msg); }
void die(char const *msg) {
  fprintf(stderr, "[%d] %s\n", errno, msg);
  abort();
}
