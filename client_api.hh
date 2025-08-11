#ifndef CLIENT_API_HH
#define CLIENT_API_HH
#include <stdint.h>
#include <stddef.h>

// Client-side request/response helpers
int32_t send_req(int fd, uint8_t const *text, size_t len);
int32_t read_res(int fd);

#endif