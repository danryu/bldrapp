#pragma once
#include <optional>
#include <source_location>

#include "print.hpp"
#include "util/assert.hpp"

#ifndef CUTIL_MACROS_PRINT_FUNC
#define CUTIL_MACROS_PRINT_FUNC WARN
#endif

#define PANIC(...)                                                       \
    CUTIL_MACROS_PRINT_FUNC("fatal error" __VA_OPT__(": ") __VA_ARGS__); \
    panic("");

#define ASSERT(cond, ...)   \
    if(!(cond)) {           \
        PANIC(__VA_ARGS__); \
    }

//
// returns automatically detected error value
//
// MSVC __FUNCSIG__ parsing is only used to distinguish void from non-void
// returns. Non-void error paths use return {} (nullopt/nullptr/false/0).

template <comptime::String s>
constexpr auto msft_ltrim_space() -> auto {
    if constexpr(comptime::starts_with<s, " ">) {
        return msft_ltrim_space<comptime::remove_prefix<s, " "> >();
    } else {
        return s;
    }
}

template <comptime::String ret>
constexpr auto msft_strip_templates_fn() -> auto {
    constexpr auto open = comptime::find<ret, "<">;
    if constexpr(open == std::string_view::npos) {
        return ret;
    } else {
        return comptime::substr<ret, 0, open>;
    }
}

template <comptime::String func>
constexpr auto msft_func_sig() -> auto {
    constexpr auto s1 = comptime::remove_prefix<func, "static ">;
    constexpr auto s2 = comptime::remove_prefix<s1, "virtual ">;
    return comptime::remove_prefix<s2, "const ">();
}

template <comptime::String ret>
constexpr auto msft_return_type_is_void() -> bool {
    constexpr auto s1 = comptime::remove_prefix<ret, "class ">;
    constexpr auto s2 = comptime::remove_prefix<s1, "struct ">;
    constexpr auto s3 = comptime::remove_prefix<s2, "enum ">;
    constexpr auto s4 = msft_strip_templates_fn<s3>();
    constexpr auto norm = comptime::remove_suffix<s4, " ">();
    return norm.empty() || norm.str() == "void";
}

template <comptime::String func>
constexpr auto bail_is_void_function() -> bool {
    constexpr auto sig = msft_func_sig<func>();
    constexpr auto arrow = comptime::find<sig, "->">;
    if constexpr(arrow != std::string_view::npos) {
        return msft_return_type_is_void<msft_ltrim_space<comptime::substr<sig, arrow + 2>>()>();
    } else if constexpr(comptime::find<sig, " __cdecl "> != std::string_view::npos) {
        constexpr auto cdecl_pos = comptime::find<sig, " __cdecl ">;
        return msft_return_type_is_void<
            msft_ltrim_space<comptime::remove_suffix<comptime::substr<sig, 0, cdecl_pos>, " ">>()>();
    } else {
        constexpr auto space = comptime::find<sig, " ">;
        if constexpr(space == std::string_view::npos) {
            return true;
        } else {
            return msft_return_type_is_void<comptime::substr<sig, 0, space>()>();
        }
    }
}

#define bail(...)                                                                                         \
    CUTIL_MACROS_PRINT_FUNC(__VA_ARGS__);                                                                  \
    if constexpr(bail_is_void_function<CUTIL_COMPSTR(std::source_location::current().function_name())>()) { \
        return;                                                                                            \
    } else {                                                                                               \
        return {};                                                                                         \
    }

#define ensure(cond, ...)                                      \
    if(!(cond)) {                                              \
        bail("assertion failed" __VA_OPT__(": ") __VA_ARGS__); \
    }

//
// manual action
//

struct VoidErrorType {};

template <class T>
constexpr auto return_error_v(T error_value) -> T {
    return error_value;
}

constexpr auto return_error_v(VoidErrorType) -> void {
    return;
}

constexpr auto error_value = VoidErrorType{};

#define generic_bail(error_act, ...)                      \
    {                                                     \
        __VA_OPT__(CUTIL_MACROS_PRINT_FUNC(__VA_ARGS__);) \
        error_act;                                        \
    }
#define generic_ensure(bail, cond, ...)                        \
    if(!(cond)) {                                              \
        bail("assertion failed" __VA_OPT__(": ") __VA_ARGS__); \
    }

#define bail_v(...)         generic_bail(return return_error_v(error_value), __VA_ARGS__)
#define ensure_v(cond, ...) generic_ensure(bail_v, cond, __VA_ARGS__)

#define co_bail_v(...)         generic_bail(co_return return_error_v(error_value), __VA_ARGS__)
#define co_ensure_v(cond, ...) generic_ensure(co_bail_v, cond, __VA_ARGS__)

#define bail_a(...)         generic_bail(error_act, __VA_ARGS__)
#define ensure_a(cond, ...) generic_ensure(bail_a, cond, __VA_ARGS__)
