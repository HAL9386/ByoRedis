#ifndef COMMON_HH
#define COMMON_HH

#include <vector>
#include <unistd.h>
#include <stdint.h>

size_t const k_max_msg = 32 << 20;

// Shared utilities
void msg(const char *msg);
void msg_errno(char const *msg);
void die(const char *msg);
void fd_set_nb(int fd);
void buf_append(std::vector<uint8_t> &buf, uint8_t const *data, size_t len);
void buf_consume(std::vector<uint8_t> &buf, size_t len);
int32_t read_full(int fd, uint8_t *buf, size_t n);
int32_t write_all(int fd, uint8_t const *buf, size_t n);

#endif
