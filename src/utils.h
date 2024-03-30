#ifndef UTILS_H
#define UTILS_H

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define READ_BIT(x, i) (((x) & (1 << (i))) ? 1 : 0)

#define CORE0_DATA __scratch_x("core0_data")
#define CORE0_CODE __scratch_x("core0_code")

#define CORE1_DATA __scratch_y("core1_data")
#define CORE1_CODE __scratch_y("core1_code")

#endif