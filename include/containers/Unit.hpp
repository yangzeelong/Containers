#pragma once

namespace containers {

// The C++ spelling of Rust's `()`: a single value of a single type, used to
// give `Result<void, E>`-style empty payloads a concrete value type and to
// keep combinators that ignore their input regular.
struct Unit {
    constexpr Unit() noexcept {}
};

constexpr bool operator==(Unit, Unit) noexcept { return true; }
constexpr bool operator!=(Unit, Unit) noexcept { return false; }
constexpr bool operator<(Unit, Unit) noexcept { return false; }
constexpr bool operator>(Unit, Unit) noexcept { return false; }
constexpr bool operator<=(Unit, Unit) noexcept { return true; }
constexpr bool operator>=(Unit, Unit) noexcept { return true; }

} // namespace containers
