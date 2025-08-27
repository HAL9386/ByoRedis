#pragma once

#include <string>
#include <vector>

struct Buffer;
void do_get(std::vector<std::string> &cmd, Buffer &buffer);
void do_set(std::vector<std::string> &cmd, Buffer &buffer);
void do_del(std::vector<std::string> &cmd, Buffer &buffer);
void do_keys(std::vector<std::string> &cmd, Buffer &buffer);
void do_zadd(std::vector<std::string> &cmd, Buffer &buffer);
void do_zrem(std::vector<std::string> &cmd, Buffer &buffer);
void do_zscore(std::vector<std::string> &cmd, Buffer &buffer);
void do_zquery(std::vector<std::string> &cmd, Buffer &buffer);
