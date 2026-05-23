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

template <comptime::String ret_raw>
constexpr auto coop_async_inner_is_void() -> bool {
    constexpr auto start = coop_marker_offset<ret_raw>();
    if constexpr(start == std::string_view::npos) {
        return true;
    } else {
        constexpr auto close = comptime::rfind<ret_raw, ">">;
        if constexpr(close == std::string_view::npos || close <= start) {
            return true;
        } else {
            return msft_return_type_is_void<comptime::substr<ret_raw, start, close - start>()>();
        }
    }
}

template <comptime::String func, bool TrailingReturn, bool HasCdecl>
struct msft_coop_is_void_async_impl {
    static constexpr bool value = true;
};

template <comptime::String func>
struct msft_coop_is_void_async_impl<func, true, false> {
    static constexpr auto sig  = msft_func_sig<func>();
    static constexpr auto arrow = comptime::find<sig, "->">;
    static constexpr bool value =
        coop_async_inner_is_void<msft_ltrim_space<comptime::substr<sig, arrow + 2>>()>();
};

template <comptime::String func>
struct msft_coop_is_void_async_impl<func, false, true> {
    static constexpr auto sig       = msft_func_sig<func>();
    static constexpr auto cdecl_pos = comptime::find<sig, " __cdecl ">;
    static constexpr bool value     = coop_async_inner_is_void<
        msft_ltrim_space<comptime::remove_suffix<comptime::substr<sig, 0, cdecl_pos>, " ">>()>();
};

template <comptime::String func>
constexpr auto coop_is_void_async() -> bool {
    constexpr auto sig = msft_func_sig<func>();
    return msft_coop_is_void_async_impl<
        func,
        comptime::find<sig, "->"> != std::string_view::npos,
        comptime::find<sig, " __cdecl "> != std::string_view::npos>::value;
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
