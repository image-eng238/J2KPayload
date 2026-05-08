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

#define LOAD_INTO_CACHE(ptr, rw, locality) __builtin_prefetch((ptr), (rw), (locality))
namespace opt_macro {
    constexpr int READ              = 0;
    constexpr int WRITE             = 1;
    constexpr int SHARED_READ       = 2;
    constexpr int NO_TEMPORAL       = 0;
    constexpr int LOW_TEMPORAL      = 1;
    constexpr int MODERATE_TEMPORAL = 2;
    constexpr int HIGH_TEMPORAL     = 3;
}