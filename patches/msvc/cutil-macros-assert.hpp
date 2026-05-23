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
constexpr auto extract_return_type_raw() -> auto {
    constexpr auto str010 = comptime::remove_prefix<func, "static ">;
    constexpr auto str020 = comptime::remove_prefix<str010, "virtual ">;
    constexpr auto str030 = comptime::remove_prefix<str020, "const ">;
    constexpr auto arrow  = comptime::find<str030, "->">;
    if constexpr(arrow != std::string_view::npos) {
        return msft_ltrim_space<comptime::substr<str030, arrow + 2>>();
    } else {
        // MSVC prefix form: "class std::optional<...> __cdecl ns::func(...)"
        constexpr auto cdecl_pos = comptime::find<str030, " __cdecl ">;
        if constexpr(cdecl_pos != std::string_view::npos) {
            return msft_ltrim_space<comptime::remove_suffix<comptime::substr<str030, 0, cdecl_pos>, " ">>();
        }
        constexpr auto paren = comptime::find<str030, "(">;
        if constexpr(paren != std::string_view::npos) {
            constexpr auto chunk = comptime::substr<str030, 0, paren>;
            constexpr auto last_space = comptime::rfind<chunk, " ">;
            if constexpr(last_space == std::string_view::npos) {
                return chunk;
            } else {
                return msft_ltrim_space<comptime::substr<chunk, 0, last_space>>();
            }
        }
        constexpr auto space = comptime::find<str030, " ">;
        if constexpr(space == std::string_view::npos) {
            return comptime::String("");
        } else {
            return comptime::substr<str030, 0, space>;
        }
    }
}

template <comptime::String ret>
constexpr auto normalize_return_type() -> auto {
    constexpr auto s1 = comptime::remove_prefix<ret, "class ">;
    constexpr auto s2 = comptime::remove_prefix<s1, "struct ">;
    constexpr auto s3 = comptime::remove_prefix<s2, "enum ">;
    constexpr auto s4 = msft_strip_templates_fn<s3>();
    return comptime::remove_suffix<s4, " ">;
}

template <comptime::String func>
constexpr auto bail_is_void_function() -> bool {
    constexpr auto norm = normalize_return_type<extract_return_type_raw<func>()>();
    return norm.empty() || norm.str() == "void";
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
