#pragma once
#include <source_location>

#include "assert.hpp"

template <comptime::String ret_raw>
constexpr auto coop_marker_offset() -> std::size_t {
    constexpr auto gen   = comptime::find<ret_raw, "coop::CoGenerator<">;
    constexpr auto async = comptime::find<ret_raw, "coop::Async<">;
    if constexpr(gen != std::string_view::npos) {
        return gen + comptime::String("coop::CoGenerator<").size();
    } else if constexpr(async != std::string_view::npos) {
        return async + comptime::String("coop::Async<").size();
    } else {
        return std::string_view::npos;
    }
}

template <comptime::String func>
constexpr auto coop_is_void_async() -> bool {
    constexpr auto ret_raw = extract_return_type_raw<func>();
    constexpr auto start   = coop_marker_offset<ret_raw>();
    if constexpr(start == std::string_view::npos) {
        return true;
    } else {
        constexpr auto close = comptime::rfind<ret_raw, ">">;
        if constexpr(close == std::string_view::npos || close <= start) {
            return true;
        } else {
            constexpr auto inner = comptime::substr<ret_raw, start, close - start>;
            return normalize_return_type<inner>().str() == "void";
        }
    }
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
