#pragma once
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define assume(expr) \
    if (!(expr)) __builtin_unreachable()

// __builtin___clear_cache