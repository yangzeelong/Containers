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
    OnceCell() noexcept : m_init(false) {}

    OnceCell(const OnceCell&) = delete;
    OnceCell& operator=(const OnceCell&) = delete;

    ~OnceCell() {
        if (m_init) {
            ptr()->~T();
        }
    }

    // Runs `factory` once and returns the value; later calls ignore the factory.
    template <typename F>
    T& getOrInit(F&& factory) {
        std::call_once(m_once, [this, &factory] {
            new (&m_storage) T(std::forward<F>(factory)());
            m_init = true;
        });
        return *ptr();
    }

    // Returns true when this call initialized the cell, false when it was
    // already initialized (or another thread won the race).
    template <typename F>
    bool tryInit(F&& factory) {
        bool inserted = false;
        std::call_once(m_once, [this, &factory, &inserted] {
            new (&m_storage) T(std::forward<F>(factory)());
            m_init = true;
            inserted = true;
        });
        return inserted;
    }

    bool isInit() const noexcept { return m_init; }

    T* get() noexcept { return m_init ? ptr() : nullptr; }
    const T* get() const noexcept { return m_init ? ptr() : nullptr; }

    T& value() {
        assert(m_init);
        return *ptr();
    }

    const T& value() const {
        assert(m_init);
        return *ptr();
    }

private:
    T* ptr() noexcept { return reinterpret_cast<T*>(&m_storage); }
    const T* ptr() const noexcept { return reinterpret_cast<const T*>(&m_storage); }

    typename std::aligned_storage<sizeof(T), alignof(T)>::type m_storage;
    std::once_flag m_once;
    bool m_init;
};

// Rust's `once_cell::sync::Lazy`: a value built by a factory on first use.
// `Lazy<T> config([]{ return T{...}; });` then `config->field` anywhere.
template <typename T>
class Lazy {
public:
    template <typename F>
    explicit Lazy(F&& factory) : m_factory(std::forward<F>(factory)) {}

    Lazy(const Lazy&) = delete;
    Lazy& operator=(const Lazy&) = delete;

    T& get() {
        ensure();
        return m_cell.value();
    }

    const T& get() const {
        ensure();
        return m_cell.value();
    }

    T& operator*() { return get(); }
    const T& operator*() const { return get(); }

    T* operator->() { return &get(); }
    const T* operator->() const { return &get(); }

    bool isInit() const noexcept { return m_cell.isInit(); }

private:
    void ensure() const {
        m_cell.getOrInit([this] { return m_factory(); });
    }

    std::function<T()> m_factory;
    mutable OnceCell<T> m_cell;
};

} // namespace containers
