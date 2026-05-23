#pragma once
#include <source_location>

#include "assert.hpp"

template <comptime::String sig>
constexpr auto msft_sig_has_trailing_void_return() -> bool {
    if constexpr(comptime::ends_with<sig, "-> void">) {
        return true;
    }
    if constexpr(comptime::find<sig, "-> void "> != std::string_view::npos) {
        return true;
    }
    if constexpr(comptime::find<sig, "-> void__ptr64"> != std::string_view::npos) {
        return true;
    }
    return false;
}

template <comptime::String sig>
constexpr auto msft_sig_is_void_coop() -> bool {
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
    if constexpr(msft_sig_has_trailing_void_return<sig>()) {
        return true;
    }
    return msft_sig_is_void_fn<sig>();
}

#define coop_bail(...)                                                                          \
    CUTIL_MACROS_PRINT_FUNC(__VA_ARGS__);                                                         \
    if constexpr(msft_sig_is_void_coop<CUTIL_COMPSTR(__FUNCSIG__)>()) {                          \
        co_return;                                                                                \
    } else if constexpr(msft_sig_returns_optional<CUTIL_COMPSTR(__FUNCSIG__)>()) {              \
        co_return std::nullopt;                                                                     \
    } else {                                                                                      \
        co_return msft_bail_empty{};                                                                \
    }

#define coop_ensure(cond, ...)                                      \
    if(!(cond)) {                                                   \
        coop_bail("assertion failed" __VA_OPT__(": ") __VA_ARGS__); \
    }
