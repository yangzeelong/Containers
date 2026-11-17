#pragma once

#include <cassert>
#include <functional>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

namespace containers {

// Rust's `once_cell::sync::OnceCell` / `OnceLock`: a value initialized at most
// once, thread-safely, through a factory. The factory runs under
// std::call_once, so exactly one thread materializes the value and every other
// caller observes the initialized cell.
template <typename T>
class OnceCell {
public:
    OnceCell() noexcept : init_(false) {}

    OnceCell(const OnceCell&) = delete;
    OnceCell& operator=(const OnceCell&) = delete;

    ~OnceCell() {
        if (init_) {
            ptr()->~T();
        }
    }

    // Runs `factory` once and returns the value; later calls ignore the factory.
    template <typename F>
    T& getOrInit(F&& factory) {
        std::call_once(once_, [this, &factory] {
            new (&storage_) T(std::forward<F>(factory)());
            init_ = true;
        });
        return *ptr();
    }

    // Returns true when this call initialized the cell, false when it was
    // already initialized (or another thread won the race).
    template <typename F>
    bool tryInit(F&& factory) {
        bool inserted = false;
        std::call_once(once_, [this, &factory, &inserted] {
            new (&storage_) T(std::forward<F>(factory)());
            init_ = true;
            inserted = true;
        });
        return inserted;
    }

    bool isInit() const noexcept { return init_; }

    T* get() noexcept { return init_ ? ptr() : nullptr; }
    const T* get() const noexcept { return init_ ? ptr() : nullptr; }

    T& value() {
        assert(init_);
        return *ptr();
    }

    const T& value() const {
        assert(init_);
        return *ptr();
    }

private:
    T* ptr() noexcept { return reinterpret_cast<T*>(&storage_); }
    const T* ptr() const noexcept { return reinterpret_cast<const T*>(&storage_); }

    typename std::aligned_storage<sizeof(T), alignof(T)>::type storage_;
    std::once_flag once_;
    bool init_;
};

// Rust's `once_cell::sync::Lazy`: a value built by a factory on first use.
// `Lazy<T> config([]{ return T{...}; });` then `config->field` anywhere.
template <typename T>
class Lazy {
public:
    template <typename F>
    explicit Lazy(F&& factory) : factory_(std::forward<F>(factory)) {}

    Lazy(const Lazy&) = delete;
    Lazy& operator=(const Lazy&) = delete;

    T& get() {
        ensure();
        return cell_.value();
    }

    const T& get() const {
        ensure();
        return cell_.value();
    }

    T& operator*() { return get(); }
    const T& operator*() const { return get(); }

    T* operator->() { return &get(); }
    const T* operator->() const { return &get(); }

    bool isInit() const noexcept { return cell_.isInit(); }

private:
    void ensure() const {
        cell_.getOrInit([this] { return factory_(); });
    }

    std::function<T()> factory_;
    mutable OnceCell<T> cell_;
};

} // namespace containers
