#pragma once

#ifdef ENABLE_EXPECT
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define BRANCH_PROB(x, probability) __builtin_expect_with_probability(!!(x), 1, probability)
#else
#define likely(x) x
#define unlikely(x) x
#define BRANCH_PROB(x, probability) x
#endif

#define assume(expr) \
    if (!(expr)) __builtin_unreachable()

// __builtin___clear_cache