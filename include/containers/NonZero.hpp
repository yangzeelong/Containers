#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "containers/Optional.hpp"

namespace containers {

// Rust's `std::num::NonZero*`: an integer that can never be zero, so callers
// that need "a valid handle/id" can encode the invariant in the type.
// Construction asserts; `TryNew` is the non-asserting factory and returns an
// empty Optional for zero.
template <typename T>
class NonZero {
    static_assert(std::is_integral<T>::value, "NonZero<T> requires an integral type");

public:
    using value_type = T;

    NonZero() = delete;

    explicit NonZero(T value) : value_(value) {
        assert(value != T());
    }

    static Optional<NonZero> TryNew(T value) {
        if (value == T()) {
            return nullOpt;
        }
        return NonZero(value);
    }

    T get() const noexcept { return value_; }
    explicit operator T() const noexcept { return value_; }

    bool operator==(NonZero other) const noexcept { return value_ == other.value_; }
    bool operator!=(NonZero other) const noexcept { return value_ != other.value_; }
    bool operator<(NonZero other) const noexcept { return value_ < other.value_; }

private:
    T value_;
};

using NonZeroU32 = NonZero<std::uint32_t>;
using NonZeroU64 = NonZero<std::uint64_t>;
using NonZeroI32 = NonZero<std::int32_t>;
using NonZeroI64 = NonZero<std::int64_t>;
using NonZeroSize = NonZero<std::size_t>;

} // namespace containers
