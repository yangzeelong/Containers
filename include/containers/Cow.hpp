#pragma once

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

#include "containers/Config.hpp"

namespace containers {

// Rust's `Cow<'a, T>`: a value that is either borrowed (a view that must
// outlive the Cow) or owned (shared_ptr). Reading is free; the first call to
// `toMut()` deep-copies the payload when the value is borrowed or shared.
//
// `Borrowed` does not take ownership and does not extend the referent's
// lifetime; `Shared` requires a non-null pointer. Copied Cows share the owned
// payload until one of them mutates.
template <typename T>
class Cow {
public:
    using value_type = T;

    static Cow Borrowed(const T& value) { return Cow(&value); }

    static Cow Owned(T value) { return Cow(std::make_shared<T>(std::move(value))); }

    static Cow Shared(std::shared_ptr<T> owner) {
        assert(owner);
        return Cow(std::move(owner));
    }

    CS_NODISCARD bool isBorrowed() const noexcept { return !owner_; }
    CS_NODISCARD bool isOwned() const noexcept { return static_cast<bool>(owner_); }

    const T& get() const noexcept { return owner_ ? *owner_ : *view_; }
    const T& operator*() const noexcept { return get(); }
    const T* operator->() const noexcept { return &get(); }

    // Returns a mutable reference, deep-copying first when this Cow only
    // borrowed the value or shares its owner with another Cow.
    T& toMut() {
        if (!owner_ || owner_.use_count() > 1) {
            owner_ = std::make_shared<T>(get());
            view_ = nullptr;
        }
        return *owner_;
    }

    // Moves the payload out when this is the unique owner, copies otherwise.
    T intoOwned() {
        if (owner_ && owner_.use_count() == 1) {
            T value(std::move(*owner_));
            owner_.reset();
            view_ = nullptr;
            return value;
        }
        return T(get());
    }

private:
    explicit Cow(const T* view) : owner_(), view_(view) {}
    explicit Cow(std::shared_ptr<T> owner) : owner_(std::move(owner)), view_(nullptr) {}

    std::shared_ptr<T> owner_;
    const T* view_;
};

} // namespace containers
