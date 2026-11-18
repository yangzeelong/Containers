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
        : m_worker(std::forward<Function>(function), std::forward<Args>(args)...) {}

    Thread(Thread&& other) noexcept : m_worker(std::move(other.m_worker)) {}

    Thread& operator=(Thread&& other) {
        if (this != &other) {
            if (m_worker.joinable()) {
                m_worker.join();
            }
            m_worker = std::move(other.m_worker);
        }
        return *this;
    }

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    ~Thread() {
        if (m_worker.joinable()) {
            m_worker.join();
        }
    }

    bool joinable() const noexcept { return m_worker.joinable(); }

    // Joins the worker; throws std::system_error if the thread is not joinable.
    void join() { m_worker.join(); }

    // Releases the worker; the caller becomes responsible for its lifetime.
    void detach() { m_worker.detach(); }

    void swap(Thread& other) noexcept { m_worker.swap(other.m_worker); }

    static unsigned hardwareConcurrency() noexcept {
        return std::thread::hardware_concurrency();
    }

private:
    std::thread m_worker;
};

// Non-movable, non-detachable thread wrapper for structured concurrency: the
// worker is always joined before the enclosing scope ends.
class ScopedThread {
public:
    template <typename Function, typename... Args>
    explicit ScopedThread(Function&& function, Args&&... args)
        : m_worker(std::forward<Function>(function), std::forward<Args>(args)...) {}

    ScopedThread(const ScopedThread&) = delete;
    ScopedThread& operator=(const ScopedThread&) = delete;
    ScopedThread(ScopedThread&&) = delete;
    ScopedThread& operator=(ScopedThread&&) = delete;

    ~ScopedThread() {
        if (m_worker.joinable()) {
            m_worker.join();
        }
    }

    bool joinable() const noexcept { return m_worker.joinable(); }

    void join() { m_worker.join(); }

    static unsigned hardwareConcurrency() noexcept {
        return std::thread::hardware_concurrency();
    }

private:
    std::thread m_worker;
};

}  // namespace containers
