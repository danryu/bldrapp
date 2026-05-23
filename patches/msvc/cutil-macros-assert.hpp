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
// MSVC __FUNCSIG__ is only scanned for obvious void markers. Non-void error
// paths use return {} (nullopt/nullptr/false/0).

template <comptime::String func>
constexpr auto msft_func_sig() -> auto {
    constexpr auto s1 = comptime::remove_prefix<func, "static ">;
    constexpr auto s2 = comptime::remove_prefix<s1, "virtual ">;
    return comptime::remove_prefix<s2, "const ">();
}

template <comptime::String sig>
constexpr auto msft_sig_has_void_return() -> bool {
    if constexpr(comptime::find<sig, "-> void"> != std::string_view::npos) {
        return true;
    }
    if constexpr(comptime::find<sig, "->void"> != std::string_view::npos) {
        return true;
    }
    if constexpr(comptime::starts_with<sig, "void __cdecl ">) {
        return true;
    }
    if constexpr(comptime::starts_with<sig, "void __stdcall ">) {
        return true;
    }
    if constexpr(comptime::starts_with<sig, "void __fastcall ">) {
        return true;
    }
    return false;
}

template <comptime::String func>
constexpr auto bail_is_void_function() -> bool {
    return msft_sig_has_void_return<msft_func_sig<func>()>();
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
