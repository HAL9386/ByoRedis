#ifndef COMMON_HH
#define COMMON_HH

#include <unistd.h>
#include <stdint.h>

void msg(const char *msg);
void die(const char *msg);
int32_t read_full(int fd, char *buf, size_t n);
int32_t write_all(int fd, const char *buf, size_t n);
int32_t one_request(int fd, uint32_t MAX_MSG_LEN);
int32_t query(int fd, const char *text, uint32_t MAX_MSG_LEN);

#endif
