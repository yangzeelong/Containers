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
    explicit ScopedGuard(F callable) : callable_(std::move(callable)), active_(true) {}

    ScopedGuard(ScopedGuard&& other) noexcept(
        std::is_nothrow_move_constructible<F>::value)
        : callable_(std::move(other.callable_)), active_(other.active_) {
        other.active_ = false;
    }

    ScopedGuard(const ScopedGuard&) = delete;
    ScopedGuard& operator=(const ScopedGuard&) = delete;
    ScopedGuard& operator=(ScopedGuard&&) = delete;

    ~ScopedGuard() {
        if (active_) {
            callable_();
        }
    }

    // Disarms the guard: the callable will not run on scope exit.
    void dismiss() noexcept { active_ = false; }

    CS_NODISCARD bool active() const noexcept { return active_; }

private:
    F callable_;
    bool active_;
};

// Deduces the callable type, so callers write `OnScopeExit([&] { ... });`.
template <typename F>
ScopedGuard<typename std::decay<F>::type> OnScopeExit(F&& callable) {
    return ScopedGuard<typename std::decay<F>::type>(std::forward<F>(callable));
}

} // namespace containers
