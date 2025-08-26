#include "byoredis/proto/tlv.hh"
#include "byoredis/proto/buffer.hh"
#include <string.h>

bool read_u32(uint8_t const *&cur, uint8_t const *end, uint32_t &out) {
  if (cur + 4 > end) {
    return false;
  }
  memcpy(&out, cur, 4);
  cur += 4;
  return true;
}

bool read_str(uint8_t const *&cur, uint8_t const *end, size_t n, std::string &out) {
  if (cur + n > end) {
    return false;
  }
  out.assign(cur, cur + n);
  cur += n;
  return true;
}

void buf_append_u8(Buffer &buf, uint8_t data) {
  buf.append(&data, 1);
}

void buf_append_u32(Buffer &buf, uint32_t data) {
  buf.append((uint8_t const *)&data, 4);  // assume little-endian
}

void buf_append_i64(Buffer &buf, int64_t data) {
  buf.append((uint8_t const *)&data, 8);
}

void buf_append_dbl(Buffer &buf, double data) {
  buf.append((uint8_t const *)&data, 8);
}

void buf_append_str(Buffer &buf, uint8_t const *data, size_t len) {
  buf.append(data, len);
}

void out_nil(Buffer &buf) {
  buf_append_u8(buf, TAG_NIL);
}

void out_str(Buffer &buf, char const *s, size_t size) {
  buf_append_u8(buf, TAG_STR);                    // tag
  buf_append_u32(buf, (uint32_t)size);            // len
  buf_append_str(buf, (uint8_t const *)s, size);  // val
}

void out_int(Buffer &buf, int64_t val) {
  buf_append_u8(buf, TAG_INT);
  buf_append_i64(buf, val);
}

void out_dbl(Buffer &buf, double val) {
  buf_append_u8(buf, TAG_DBL);
  buf_append_dbl(buf, val);
}

void out_arr(Buffer &buf, uint32_t n) {
  buf_append_u8(buf, TAG_ARR);
  buf_append_u32(buf, n);
}

void out_err(Buffer &buf, uint32_t code, std::string const &msg) {
  buf_append_u8(buf, TAG_ERR);
  buf_append_u32(buf, code);
  buf_append_u32(buf, (uint32_t)msg.size());
  buf_append_str(buf, (uint8_t const *)msg.data(), msg.size());
}