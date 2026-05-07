#pragma once

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define likely_p(x, probability) __builtin_expect_with_probability(!!(x), 1, probability)
#define unlikely_p(x, probability) __builtin_expect_with_probability(!!(x), 0, probability)

#define assume(expr) \
    if (!(expr)) __builtin_unreachable()

// __builtin___clear_cache