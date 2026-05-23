#pragma once
#include <source_location>

#include "assert.hpp"

// MSVC expands coop::Async<T> to coop::CoGenerator<T> in __FUNCSIG__.
template <comptime::String func>
constexpr auto coop_async_inner_type() -> auto {
    constexpr auto ret_raw = extract_return_type_raw<func>();
    constexpr auto async   = comptime::find<ret_raw, "coop::Async<">;
    constexpr auto gen     = comptime::find<ret_raw, "coop::CoGenerator<">;
    if constexpr(async == std::string_view::npos && gen == std::string_view::npos) {
        return comptime::String("");
    } else {
        constexpr auto region = comptime::find_region<ret_raw, 60, 62>;
        if constexpr(region.first == std::string_view::npos) {
            return comptime::String("");
        } else {
            constexpr auto inner = comptime::substr<ret_raw, region.first + 1, region.second - 2>;
            return normalize_return_type<inner>();
        }
    }
}

template <comptime::String func>
constexpr auto coop_is_void_async() -> bool {
    return coop_async_inner_type<func>().str() == "void";
}

template <comptime::String func>
constexpr auto coop_is_bool_async() -> bool {
    return coop_async_inner_type<func>().str() == "bool";
}

template <comptime::String func>
constexpr auto coop_is_ptr_async() -> bool {
    constexpr auto inner = coop_async_inner_type<func>();
    return !inner.empty() && inner[-1] == '*';
}

#define coop_bail(...)                                                                                \
    CUTIL_MACROS_PRINT_FUNC(__VA_ARGS__);                                                             \
    if constexpr(coop_is_void_async<CUTIL_COMPSTR(std::source_location::current().function_name())>()) { \
        co_return;                                                                                    \
    } else if constexpr(coop_is_bool_async<CUTIL_COMPSTR(std::source_location::current().function_name())>()) { \
        co_return false;                                                                              \
    } else if constexpr(coop_is_ptr_async<CUTIL_COMPSTR(std::source_location::current().function_name())>()) { \
        co_return nullptr;                                                                            \
    } else {                                                                                          \
        co_return std::nullopt;                                                                       \
    }

#define coop_ensure(cond, ...)                                      \
    if(!(cond)) {                                                   \
        coop_bail("assertion failed" __VA_OPT__(": ") __VA_ARGS__); \
    }
