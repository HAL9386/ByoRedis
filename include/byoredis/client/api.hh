#pragma once

#include <stdint.h>
#include <string>
#include <vector>

// Client-side request/response helpers
int32_t send_req(int fd, std::vector<std::string> const &cmd);
int32_t read_res(int fd);
int32_t read_full(int fd, char *buf, size_t n);
int32_t write_all(int fd, char const *buf, size_t n);
// return deserialized response size
int32_t print_response(uint8_t const *data, size_t size);
// send multiple commands in sequence for quick testing
int32_t multi_req(int fd);
