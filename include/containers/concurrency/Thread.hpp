#pragma once

#include <thread>
#include <utility>

namespace containers {

// Move-only RAII wrapper around std::thread. Unlike std::thread, destroying a
// joinable Thread joins the worker instead of terminating the process.
class Thread {
public:
    Thread() noexcept = default;

    template <typename Function, typename... Args>
    explicit Thread(Function&& function, Args&&... args)
        : worker_(std::forward<Function>(function), std::forward<Args>(args)...) {}

    Thread(Thread&& other) noexcept : worker_(std::move(other.worker_)) {}

    Thread& operator=(Thread&& other) {
        if (this != &other) {
            if (worker_.joinable()) {
                worker_.join();
            }
            worker_ = std::move(other.worker_);
        }
        return *this;
    }

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    ~Thread() {
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    bool joinable() const noexcept { return worker_.joinable(); }

    // Joins the worker; throws std::system_error if the thread is not joinable.
    void join() { worker_.join(); }

    // Releases the worker; the caller becomes responsible for its lifetime.
    void detach() { worker_.detach(); }

    void swap(Thread& other) noexcept { worker_.swap(other.worker_); }

    static unsigned hardwareConcurrency() noexcept {
        return std::thread::hardware_concurrency();
    }

private:
    std::thread worker_;
};

// Non-movable, non-detachable thread wrapper for structured concurrency: the
// worker is always joined before the enclosing scope ends.
class ScopedThread {
public:
    template <typename Function, typename... Args>
    explicit ScopedThread(Function&& function, Args&&... args)
        : worker_(std::forward<Function>(function), std::forward<Args>(args)...) {}

    ScopedThread(const ScopedThread&) = delete;
    ScopedThread& operator=(const ScopedThread&) = delete;
    ScopedThread(ScopedThread&&) = delete;
    ScopedThread& operator=(ScopedThread&&) = delete;

    ~ScopedThread() {
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    bool joinable() const noexcept { return worker_.joinable(); }

    void join() { worker_.join(); }

    static unsigned hardwareConcurrency() noexcept {
        return std::thread::hardware_concurrency();
    }

private:
    std::thread worker_;
};

}  // namespace containers
