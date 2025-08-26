#pragma once

#include <stdint.h>
#include <string>

size_t const k_max_msg  = 32 << 20;
size_t const k_max_args = 200 * 1000;

// Tag-Length-Value (TLV) encoding
//  nil     int64           str                 array
// +-----+  +-----+-----+  +-----+-----+-----+  +-----+-----+-----+
// | tag |  | tag | int |  | tag | len | val |  | tag | len | ... |
// +-----+  +-----+-----+  +-----+-----+-----+  +-----+-----+-----+
//   1B        1B    8B      1B    4B    ...       1B    4B   ...

// data types of serialized data
enum TAG {
  TAG_NIL = 0,  // nil
  TAG_ERR = 1,  // error code + msg
  TAG_STR = 2,  // string
  TAG_INT = 3,  // int64
  TAG_DBL = 4,  // double
  TAG_ARR = 5,  // array
};

// error code for TAG_ERR
enum TAG_ERR_CODE {
  ERR_UNKNOWN = 1,  // unknown command
  ERR_TOO_BIG = 2,  // response too big
};

struct Buffer;
void buf_append_u8(Buffer &buf, uint8_t data);
void buf_append_u32(Buffer &buf, uint32_t data);
void buf_append_i64(Buffer &buf, int64_t data);
void buf_append_dbl(Buffer &buf, double data);
void buf_append_str(Buffer &buf, uint8_t const *data, size_t len);

void out_nil(Buffer &buf);
void out_str(Buffer &buf, char const *s, size_t size);
void out_int(Buffer &buf, int64_t val);
void out_dbl(Buffer &buf, double val);
void out_arr(Buffer &buf, uint32_t n);
void out_err(Buffer &buf, uint32_t code, std::string const &msg);

// read 4 bytes as uint32_t, and move cur forward by 4
bool read_u32(uint8_t const *&cur, uint8_t const *end, uint32_t &out);
// read n bytes as string, and move cur forward by n
bool read_str(uint8_t const *&cur, uint8_t const *end, size_t n, std::string &out);
