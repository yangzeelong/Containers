#pragma once

#include <memory>
#include <mutex>
#include <utility>

namespace containers {

// A C++14 stand-in for C++20's `std::atomic<std::shared_ptr<T>>`: atomic
// load/store/exchange/compare-exchange over a shared_ptr, implemented with a
// mutex (portable and safe, but not lock-free). Handy for handing an immutable
// snapshot between threads without a pipeline-wide lock.
//
// As with `std::atomic<shared_ptr<T>>`, `compareExchange` updates `expected`
// with the current value when the comparison fails, so callers can retry:
//
//   auto current = atom.load();
//   while (!atom.compareExchange(current, makeNext(current))) {}
template <typename T>
class AtomicSharedPtr {
public:
    AtomicSharedPtr() noexcept = default;

    explicit AtomicSharedPtr(std::shared_ptr<T> value) : value_(std::move(value)) {}

    AtomicSharedPtr(const AtomicSharedPtr&) = delete;
    AtomicSharedPtr& operator=(const AtomicSharedPtr&) = delete;

    std::shared_ptr<T> load() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return value_;
    }

    void store(std::shared_ptr<T> value) {
        std::lock_guard<std::mutex> guard(mutex_);
        value_ = std::move(value);
    }

    std::shared_ptr<T> exchange(std::shared_ptr<T> value) {
        std::lock_guard<std::mutex> guard(mutex_);
        std::shared_ptr<T> previous = value_;
        value_ = std::move(value);
        return previous;
    }

    bool compareExchange(std::shared_ptr<T>& expected, std::shared_ptr<T> desired) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (value_ != expected) {
            expected = value_;
            return false;
        }
        value_ = std::move(desired);
        return true;
    }

private:
    mutable std::mutex mutex_;
    std::shared_ptr<T> value_;
};

} // namespace containers
