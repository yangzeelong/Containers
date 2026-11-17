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
    Cell() : value_() {}

    explicit Cell(const T& value) : value_(value) {}

    Cell(const Cell&) = default;
    Cell& operator=(const Cell&) = default;

    T get() const { return value_; }

    void set(const T& value) const { value_ = value; }

    T replace(const T& value) const {
        T old = value_;
        value_ = value;
        return old;
    }

    T take() const {
        static_assert(std::is_default_constructible<T>::value,
                      "Cell<T>::take() requires a default-constructible T");
        T old = std::move(value_);
        value_ = T();
        return old;
    }

private:
    mutable T value_;
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

    explicit RefCell(T value) : value_(std::move(value)) {}

    RefCell(const RefCell&) = delete;
    RefCell& operator=(const RefCell&) = delete;
    RefCell(RefCell&&) = delete;
    RefCell& operator=(RefCell&&) = delete;

    ~RefCell() { assert(!isBorrowed()); }

    Ref borrow() const {
        if (mutablyBorrowed_) {
            throw std::logic_error("RefCell: value is already mutably borrowed");
        }
        ++sharedBorrows_;
        return Ref(*this);
    }

    RefMut borrowMut() {
        if (isBorrowed()) {
            throw std::logic_error("RefCell: value is already borrowed");
        }
        mutablyBorrowed_ = true;
        return RefMut(*this);
    }

    Optional<Ref> tryBorrow() const {
        if (mutablyBorrowed_) {
            return nullOpt;
        }
        ++sharedBorrows_;
        return Optional<Ref>(Ref(*this));
    }

    Optional<RefMut> tryBorrowMut() {
        if (isBorrowed()) {
            return nullOpt;
        }
        mutablyBorrowed_ = true;
        return Optional<RefMut>(RefMut(*this));
    }

    bool isBorrowed() const noexcept { return sharedBorrows_ != 0 || mutablyBorrowed_; }

    bool isMutablyBorrowed() const noexcept { return mutablyBorrowed_; }

    std::size_t sharedBorrowCount() const noexcept { return sharedBorrows_; }

private:
    friend class Ref;
    friend class RefMut;

    void releaseShared() const noexcept {
        assert(sharedBorrows_ > 0);
        --sharedBorrows_;
    }

    void releaseMutable() noexcept {
        assert(mutablyBorrowed_);
        mutablyBorrowed_ = false;
    }

    mutable T value_;
    mutable std::size_t sharedBorrows_ = 0;
    mutable bool mutablyBorrowed_ = false;
};

template <typename T>
class RefCell<T>::Ref {
public:
    Ref(Ref&& other) noexcept : owner_(other.owner_) { other.owner_ = nullptr; }

    Ref& operator=(Ref&& other) noexcept {
        if (this != &other) {
            release();
            owner_ = other.owner_;
            other.owner_ = nullptr;
        }
        return *this;
    }

    Ref(const Ref&) = delete;
    Ref& operator=(const Ref&) = delete;

    ~Ref() { release(); }

    const T& get() const {
        assert(owner_ != nullptr);
        return owner_->value_;
    }

    const T& operator*() const { return get(); }

    const T* operator->() const { return &get(); }

private:
    friend class RefCell;
    friend class RefCell<T>;

    explicit Ref(const RefCell<T>& owner) noexcept : owner_(&owner) {}

    void release() noexcept {
        if (owner_ != nullptr) {
            owner_->releaseShared();
            owner_ = nullptr;
        }
    }

    const RefCell<T>* owner_;
};

template <typename T>
class RefCell<T>::RefMut {
public:
    RefMut(RefMut&& other) noexcept : owner_(other.owner_) { other.owner_ = nullptr; }

    RefMut& operator=(RefMut&& other) noexcept {
        if (this != &other) {
            release();
            owner_ = other.owner_;
            other.owner_ = nullptr;
        }
        return *this;
    }

    RefMut(const RefMut&) = delete;
    RefMut& operator=(const RefMut&) = delete;

    ~RefMut() { release(); }

    T& get() const {
        assert(owner_ != nullptr);
        return owner_->value_;
    }

    T& operator*() const { return get(); }

    T* operator->() const { return &get(); }

private:
    friend class RefCell;
    friend class RefCell<T>;

    explicit RefMut(RefCell<T>& owner) noexcept : owner_(&owner) {}

    void release() noexcept {
        if (owner_ != nullptr) {
            owner_->releaseMutable();
            owner_ = nullptr;
        }
    }

    RefCell<T>* owner_;
};

}  // namespace containers
