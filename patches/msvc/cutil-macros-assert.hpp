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

template <comptime::String str>
constexpr auto type_string_to_type() -> auto {
    if constexpr(str.str() == "std::unique_ptr" || str.str() == "std::shared_ptr") {
        return nullptr;
    } else if constexpr(str.str() == "void") {
        return;
    } else if constexpr(str.str() == "bool") {
        return false;
    } else if constexpr(str.str() == "int") {
        return -1;
    } else if constexpr(str.str() == "std::optional") {
        return std::nullopt;
    } else {
        return;
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

template <comptime::String s>
constexpr auto msft_ltrim_space() -> auto {
    if constexpr(comptime::starts_with<s, " ">) {
        return msft_ltrim_space<comptime::remove_prefix<s, " "> >();
    } else {
        return s;
    }
}

// MSVC __FUNCSIG__ uses trailing-return syntax ("auto __cdecl foo(...) -> ReturnType").
template <comptime::String func>
constexpr auto extract_return_type_raw() -> auto {
    constexpr auto str010 = comptime::remove_prefix<func, "static ">;
    constexpr auto str020 = comptime::remove_prefix<str010, "virtual ">;
    constexpr auto str030 = comptime::remove_prefix<str020, "const ">;
    constexpr auto arrow  = comptime::find<str030, "->">;
    if constexpr(arrow != std::string_view::npos) {
        return msft_ltrim_space<comptime::substr<str030, arrow + 2>>();
    } else {
        // Fallback: first token before a space (GCC-style signatures).
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
constexpr auto bail_return_type() -> auto {
    return normalize_return_type<extract_return_type_raw<func>()>();
}

template <comptime::String func>
constexpr auto bail_is_ptr_return() -> bool {
    constexpr auto ret = bail_return_type<func>();
    return !ret.empty() && ret[-1] == '*';
}

template <comptime::String func>
constexpr auto bail_is_optional_return() -> bool {
    return bail_return_type<func>().str() == "std::optional";
}

template <comptime::String func>
constexpr auto bail_is_bool_return() -> bool {
    return bail_return_type<func>().str() == "bool";
}

template <comptime::String func>
constexpr auto bail_is_int_return() -> bool {
    return bail_return_type<func>().str() == "int";
}

template <comptime::String func>
constexpr auto bail_is_smart_ptr_return() -> bool {
    constexpr auto ret = bail_return_type<func>().str();
    return ret == "std::unique_ptr" || ret == "std::shared_ptr";
}

#define bail(...)                                                                                              \
    CUTIL_MACROS_PRINT_FUNC(__VA_ARGS__);                                                                       \
    if constexpr(bail_is_ptr_return<CUTIL_COMPSTR(std::source_location::current().function_name())>()) {       \
        return nullptr;                                                                                        \
    } else if constexpr(bail_is_smart_ptr_return<CUTIL_COMPSTR(std::source_location::current().function_name())>()) { \
        return nullptr;                                                                                        \
    } else if constexpr(bail_is_optional_return<CUTIL_COMPSTR(std::source_location::current().function_name())>()) { \
        return std::nullopt;                                                                                     \
    } else if constexpr(bail_is_bool_return<CUTIL_COMPSTR(std::source_location::current().function_name())>()) { \
        return false;                                                                                          \
    } else if constexpr(bail_is_int_return<CUTIL_COMPSTR(std::source_location::current().function_name())>()) { \
        return -1;                                                                                               \
    } else {                                                                                                   \
        return;                                                                                                \
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
