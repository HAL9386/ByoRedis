#pragma once

#include <time.h>
#include <stdint.h>

uint64_t const k_idle_timeout_ms = 5 * 1000;  // 5 seconds
size_t   const k_max_works       = 2000;      // TTL timers using a heap 

uint64_t get_monotonic_msec();
int32_t next_timer_ms();
void process_timers();
