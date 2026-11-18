#pragma once

#include <type_traits>
#include <utility>

#include "containers/Config.hpp"

namespace containers {

// Rust's `scopeguard` / C++ `scope_exit`: runs a callable when the guard leaves
// scope unless the guard was dismissed first. Move-only so a guard can be
// returned from factory functions without double-running.
//
// The callable must not throw: the destructor runs it during stack unwinding
// and a throwing callable terminates the process.
template <typename F>
class ScopedGuard {
public:
    explicit ScopedGuard(F callable) : m_callable(std::move(callable)), m_active(true) {}

    ScopedGuard(ScopedGuard&& other) noexcept(
        std::is_nothrow_move_constructible<F>::value)
        : m_callable(std::move(other.m_callable)), m_active(other.m_active) {
        other.m_active = false;
    }

    ScopedGuard(const ScopedGuard&) = delete;
    ScopedGuard& operator=(const ScopedGuard&) = delete;
    ScopedGuard& operator=(ScopedGuard&&) = delete;

    ~ScopedGuard() {
        if (m_active) {
            m_callable();
        }
    }

    // Disarms the guard: the callable will not run on scope exit.
    void dismiss() noexcept { m_active = false; }

    CS_NODISCARD bool active() const noexcept { return m_active; }

private:
    F m_callable;
    bool m_active;
};

// Deduces the callable type, so callers write `OnScopeExit([&] { ... });`.
template <typename F>
ScopedGuard<typename std::decay<F>::type> OnScopeExit(F&& callable) {
    return ScopedGuard<typename std::decay<F>::type>(std::forward<F>(callable));
}

} // namespace containers
