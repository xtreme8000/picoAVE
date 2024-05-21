#ifndef UTILS_H
#define UTILS_H

#include "pico/stdlib.h"

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define READ_BIT(x, i) (((x) & (1 << (i))) ? 1 : 0)

#define CORE0_DATA __scratch_x("core0_data")
#define CORE0_CODE __scratch_x("core0_code") __attribute__((optimize("Os")))

#define CORE1_DATA __scratch_y("core1_data")
#define CORE1_CODE __scratch_y("core1_code") __attribute__((optimize("Os")))

static inline uint32_t next_pow2(uint32_t x) {
	return (x == 1) ? 1 : 1 << (32 - __builtin_clz(x - 1));
}

static inline size_t min_n(size_t a, size_t b) {
	return a < b ? a : b;
}

#endif