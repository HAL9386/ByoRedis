#pragma once

#include <stddef.h>

#define container_of(ptr, T, member) \
  ((T *)( (char *)ptr - offsetof(T, member) ))
