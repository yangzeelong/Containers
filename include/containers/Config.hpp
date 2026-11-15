#pragma once

// Compiler feature shims that keep the library buildable as C++14 while still
// using standard attributes when the toolchain understands them.
//
// CS_NODISCARD expands to [[nodiscard]] on compilers that support it (C++17
// attribute, accepted as an extension in C++14 by clang/GCC/MSVC) and to
// nothing elsewhere. The macro is part of the public surface: it is applied
// where ignoring a return value is almost always a bug.

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(nodiscard)
#define CS_NODISCARD [[nodiscard]]
#endif
#endif

#ifndef CS_NODISCARD
#define CS_NODISCARD
#endif

// CS_CPP14_ASSERT-based helpers deliberately stay in <cassert>; no macro
// wrapper is needed until a second consumer asks for one.
