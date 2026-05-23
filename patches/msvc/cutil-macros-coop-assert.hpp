#pragma once
#include <source_location>

#include "assert.hpp"

#define coop_bail(...)                    \
    CUTIL_MACROS_PRINT_FUNC(__VA_ARGS__); \
    co_return {}

#define coop_ensure(cond, ...)                                      \
    if(!(cond)) {                                                   \
        coop_bail("assertion failed" __VA_OPT__(": ") __VA_ARGS__); \
    }
