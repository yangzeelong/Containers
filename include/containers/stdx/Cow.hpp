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

    CS_NODISCARD bool isBorrowed() const noexcept { return !m_owner; }
    CS_NODISCARD bool isOwned() const noexcept { return static_cast<bool>(m_owner); }

    const T& get() const noexcept { return m_owner ? *m_owner : *m_view; }
    const T& operator*() const noexcept { return get(); }
    const T* operator->() const noexcept { return &get(); }

    // Returns a mutable reference, deep-copying first when this Cow only
    // borrowed the value or shares its owner with another Cow.
    T& toMut() {
        if (!m_owner || m_owner.use_count() > 1) {
            m_owner = std::make_shared<T>(get());
            m_view = nullptr;
        }
        return *m_owner;
    }

    // Moves the payload out when this is the unique owner, copies otherwise.
    T intoOwned() {
        if (m_owner && m_owner.use_count() == 1) {
            T value(std::move(*m_owner));
            m_owner.reset();
            m_view = nullptr;
            return value;
        }
        return T(get());
    }

private:
    explicit Cow(const T* view) : m_owner(), m_view(view) {}
    explicit Cow(std::shared_ptr<T> owner) : m_owner(std::move(owner)), m_view(nullptr) {}

    std::shared_ptr<T> m_owner;
    const T* m_view;
};

} // namespace containers
