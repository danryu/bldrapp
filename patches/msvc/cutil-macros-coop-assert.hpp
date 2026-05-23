#pragma once
#include <source_location>

#include "assert.hpp"

template <comptime::String sig>
constexpr auto msft_sig_has_void_coop_async() -> bool {
    if constexpr(comptime::find<sig, "coop::Async<void>"> != std::string_view::npos) {
        return true;
    }
    if constexpr(comptime::find<sig, "coop::Async<void >"> != std::string_view::npos) {
        return true;
    }
    if constexpr(comptime::find<sig, "coop::CoGenerator<void>"> != std::string_view::npos) {
        return true;
    }
    if constexpr(comptime::find<sig, "coop::CoGenerator<void >"> != std::string_view::npos) {
        return true;
    }
    return false;
}

template <comptime::String func>
constexpr auto coop_is_void_async() -> bool {
    constexpr auto sig = msft_func_sig<func>();
    if constexpr(msft_sig_has_void_coop_async<sig>()) {
        return true;
    }
    return msft_sig_has_void_return<sig>();
}

#define coop_bail(...)                                                                                \
    CUTIL_MACROS_PRINT_FUNC(__VA_ARGS__);                                                             \
    if constexpr(coop_is_void_async<CUTIL_COMPSTR(std::source_location::current().function_name())>()) { \
        co_return;                                                                                    \
    } else {                                                                                          \
        co_return {};                                                                                 \
    }

#define coop_ensure(cond, ...)                                      \
    if(!(cond)) {                                                   \
        coop_bail("assertion failed" __VA_OPT__(": ") __VA_ARGS__); \
    }
