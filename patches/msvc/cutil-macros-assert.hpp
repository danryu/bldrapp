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
// Upstream cutil-macros parses std::source_location::current().function_name() at
// compile time to synthesize the correct error value (nullopt, nullptr, false, …).
// That works on GCC/Clang but MSVC __FUNCSIG__ parsing is fragile. For the MSVC probe
// use return {} / co_return {} which yields the correct error value for every return
// type used in gstjitsimeet (optional, pointers, bool, int, unique_ptr, void).

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

#define bail(...)                         \
    CUTIL_MACROS_PRINT_FUNC(__VA_ARGS__); \
    return {}

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
