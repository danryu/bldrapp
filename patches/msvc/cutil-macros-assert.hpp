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
// Non-void error paths use msft_bail_empty (converts to nullopt/nullptr/false/0).

template <class T>
struct msft_is_optional : std::false_type {};
template <class U>
struct msft_is_optional<std::optional<U>> : std::true_type {};
template <class T>
inline constexpr bool msft_is_optional_v = msft_is_optional<std::remove_cvref_t<T>>::value;

template <comptime::String sig>
constexpr auto msft_sig_returns_optional() -> bool {
    if constexpr(comptime::starts_with<sig, "class std::optional">) {
        return true;
    }
    if constexpr(comptime::starts_with<sig, "struct std::optional">) {
        return true;
    }
    if constexpr(comptime::find<sig, "-> class std::optional"> != std::string_view::npos) {
        return true;
    }
    if constexpr(comptime::find<sig, "-> struct std::optional"> != std::string_view::npos) {
        return true;
    }
    if constexpr(comptime::find<sig, "-> std::optional"> != std::string_view::npos) {
        return true;
    }
    return false;
}

struct msft_bail_empty {
    template <class T>
    operator T() const {
        if constexpr(msft_is_optional_v<T>) {
            return std::nullopt;
        } else if constexpr(std::is_default_constructible_v<T>) {
            return T{};
        } else {
            static_assert(sizeof(T) == 0, "msft_bail_empty: unsupported return type");
        }
    }
};

template <comptime::String sig>
constexpr auto msft_bail_result() {
    if constexpr(msft_sig_returns_optional<sig>()) {
        return std::nullopt;
    } else {
        return msft_bail_empty{};
    }
}

template <comptime::String sig>
constexpr auto msft_sig_is_void_fn() -> bool {
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

#define bail(...)                                                                                \
    do {                                                                                           \
        CUTIL_MACROS_PRINT_FUNC(__VA_ARGS__);                                                      \
        return msft_bail_result<CUTIL_COMPSTR(__FUNCSIG__)>();                                     \
    } while(0)

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
