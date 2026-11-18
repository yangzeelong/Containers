#pragma once

#include "containers/stdx/Optional.hpp"

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace containers {

// Rust's `std::cell::Cell`: interior mutability for small, copy-oriented values.
// `get()` returns a copy, so the value can be replaced even through a const
// reference; `get()`/`set()` are deliberately not thread-safe (use
// std::atomic for that).
template <typename T>
class Cell {
    static_assert(std::is_copy_constructible<T>::value,
                  "Cell<T> requires a copy-constructible T");

public:
    Cell() : m_value() {}

    explicit Cell(const T& value) : m_value(value) {}

    Cell(const Cell&) = default;
    Cell& operator=(const Cell&) = default;

    T get() const { return m_value; }

    void set(const T& value) const { m_value = value; }

    T replace(const T& value) const {
        T old = m_value;
        m_value = value;
        return old;
    }

    T take() const {
        static_assert(std::is_default_constructible<T>::value,
                      "Cell<T>::take() requires a default-constructible T");
        T old = std::move(m_value);
        m_value = T();
        return old;
    }

private:
    mutable T m_value;
};

// Rust's `std::cell::RefCell`: interior mutability with runtime borrow
// checking. Any number of shared borrows or exactly one mutable borrow may be
// active at a time; conflicting borrows throw std::logic_error (the throw-less
// `tryBorrow`/`tryBorrowMut` variants return an empty Optional instead).
// RefCell is deliberately neither copyable nor movable: borrow guards point at
// the cell, so moving it while borrowed would leave them dangling.
template <typename T>
class RefCell {
public:
    class Ref;
    class RefMut;

    explicit RefCell(T value) : m_value(std::move(value)) {}

    RefCell(const RefCell&) = delete;
    RefCell& operator=(const RefCell&) = delete;
    RefCell(RefCell&&) = delete;
    RefCell& operator=(RefCell&&) = delete;

    ~RefCell() { assert(!isBorrowed()); }

    Ref borrow() const {
        if (m_mutablyBorrowed) {
            throw std::logic_error("RefCell: value is already mutably borrowed");
        }
        ++m_sharedBorrows;
        return Ref(*this);
    }

    RefMut borrowMut() {
        if (isBorrowed()) {
            throw std::logic_error("RefCell: value is already borrowed");
        }
        m_mutablyBorrowed = true;
        return RefMut(*this);
    }

    Optional<Ref> tryBorrow() const {
        if (m_mutablyBorrowed) {
            return nullOpt;
        }
        ++m_sharedBorrows;
        return Optional<Ref>(Ref(*this));
    }

    Optional<RefMut> tryBorrowMut() {
        if (isBorrowed()) {
            return nullOpt;
        }
        m_mutablyBorrowed = true;
        return Optional<RefMut>(RefMut(*this));
    }

    bool isBorrowed() const noexcept { return m_sharedBorrows != 0 || m_mutablyBorrowed; }

    bool isMutablyBorrowed() const noexcept { return m_mutablyBorrowed; }

    std::size_t sharedBorrowCount() const noexcept { return m_sharedBorrows; }

private:
    friend class Ref;
    friend class RefMut;

    void releaseShared() const noexcept {
        assert(m_sharedBorrows > 0);
        --m_sharedBorrows;
    }

    void releaseMutable() noexcept {
        assert(m_mutablyBorrowed);
        m_mutablyBorrowed = false;
    }

    mutable T m_value;
    mutable std::size_t m_sharedBorrows = 0;
    mutable bool m_mutablyBorrowed = false;
};

template <typename T>
class RefCell<T>::Ref {
public:
    Ref(Ref&& other) noexcept : m_owner(other.m_owner) { other.m_owner = nullptr; }

    Ref& operator=(Ref&& other) noexcept {
        if (this != &other) {
            release();
            m_owner = other.m_owner;
            other.m_owner = nullptr;
        }
        return *this;
    }

    Ref(const Ref&) = delete;
    Ref& operator=(const Ref&) = delete;

    ~Ref() { release(); }

    const T& get() const {
        assert(m_owner != nullptr);
        return m_owner->m_value;
    }

    const T& operator*() const { return get(); }

    const T* operator->() const { return &get(); }

private:
    friend class RefCell;
    friend class RefCell<T>;

    explicit Ref(const RefCell<T>& owner) noexcept : m_owner(&owner) {}

    void release() noexcept {
        if (m_owner != nullptr) {
            m_owner->releaseShared();
            m_owner = nullptr;
        }
    }

    const RefCell<T>* m_owner;
};

template <typename T>
class RefCell<T>::RefMut {
public:
    RefMut(RefMut&& other) noexcept : m_owner(other.m_owner) { other.m_owner = nullptr; }

    RefMut& operator=(RefMut&& other) noexcept {
        if (this != &other) {
            release();
            m_owner = other.m_owner;
            other.m_owner = nullptr;
        }
        return *this;
    }

    RefMut(const RefMut&) = delete;
    RefMut& operator=(const RefMut&) = delete;

    ~RefMut() { release(); }

    T& get() const {
        assert(m_owner != nullptr);
        return m_owner->m_value;
    }

    T& operator*() const { return get(); }

    T* operator->() const { return &get(); }

private:
    friend class RefCell;
    friend class RefCell<T>;

    explicit RefMut(RefCell<T>& owner) noexcept : m_owner(&owner) {}

    void release() noexcept {
        if (m_owner != nullptr) {
            m_owner->releaseMutable();
            m_owner = nullptr;
        }
    }

    RefCell<T>* m_owner;
};

}  // namespace containers
