#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace containers {

// Bounded lock-free multi-producer / single-consumer queue.
//
// Producers reserve a slot with a CAS on the shared enqueue position, then
// publish the value with a release store on the slot's sequence word. The single
// consumer observes the sequence word with acquire, moves the value out and
// recycles the slot. No lock is taken on the fast path: a mutex and condition
// variable are used only to park and wake the consumer in waitAndPop().
template <typename T>
class MpscQueue {
public:
    explicit MpscQueue(std::size_t capacity)
        : capacity_(ValidateCapacity(capacity)),
          mask_(capacity_ - 1),
          buffer_(capacity_) {
        for (std::size_t index = 0; index < capacity_; ++index) {
            buffer_[index].sequence.store(index, std::memory_order_relaxed);
        }
    }

    MpscQueue(const MpscQueue&) = delete;
    MpscQueue& operator=(const MpscQueue&) = delete;
    MpscQueue(MpscQueue&&) = delete;
    MpscQueue& operator=(MpscQueue&&) = delete;

    ~MpscQueue() { Drain(); }

    std::size_t capacity() const noexcept { return capacity_; }

    // Approximate number of enqueued but not yet dequeued values.
    std::size_t size() const noexcept {
        const std::size_t enqueued = enqueuePos_.load(std::memory_order_acquire);
        const std::size_t dequeued = dequeuePos_.load(std::memory_order_acquire);
        return enqueued - dequeued;
    }

    bool empty() const noexcept { return size() == 0; }

    bool isClosed() const noexcept { return closed_.load(std::memory_order_acquire); }

    // Non-blocking push: returns false if the queue is full or closed.
    bool tryPush(T value) { return TryPushImpl(std::move(value)); }

    // Blocking push: spins until the value is enqueued; returns false if closed.
    bool push(T value) {
        while (!TryPushImpl(std::move(value))) {
            if (closed_.load(std::memory_order_acquire)) {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }

    // Non-blocking pop; only the single consumer thread may call this.
    bool tryPop(T& out) {
        std::size_t position = dequeuePos_.load(std::memory_order_relaxed);
        Cell& cell = buffer_[position & mask_];
        if (cell.sequence.load(std::memory_order_acquire) != position + 1) {
            return false;
        }
        T* value = reinterpret_cast<T*>(&cell.storage);
        out = std::move(*value);
        value->~T();
        dequeuePos_.store(position + 1, std::memory_order_relaxed);
        cell.sequence.store(position + mask_ + 1, std::memory_order_release);
        return true;
    }

    // Blocks until a value is available; returns false when the queue is closed
    // and drained.
    bool waitAndPop(T& out) {
        while (true) {
            if (tryPop(out)) {
                return true;
            }
            if (closed_.load(std::memory_order_acquire)) {
                return false;
            }
            std::unique_lock<std::mutex> lock(sleepMutex_);
            waiting_.store(true, std::memory_order_release);
            sleepCv_.wait(lock, [this] {
                return closed_.load(std::memory_order_acquire) || !empty();
            });
            waiting_.store(false, std::memory_order_relaxed);
        }
    }

    template <typename Rep, typename Period>
    bool waitAndPopFor(T& out, const std::chrono::duration<Rep, Period>& timeout) {
        const std::chrono::steady_clock::time_point deadline =
            std::chrono::steady_clock::now() + timeout;
        while (true) {
            if (tryPop(out)) {
                return true;
            }
            if (closed_.load(std::memory_order_acquire)) {
                return false;
            }
            std::unique_lock<std::mutex> lock(sleepMutex_);
            waiting_.store(true, std::memory_order_release);
            const bool signalled = sleepCv_.wait_until(lock, deadline, [this] {
                return closed_.load(std::memory_order_acquire) || !empty();
            });
            waiting_.store(false, std::memory_order_relaxed);
            if (!signalled) {
                return false;
            }
        }
    }

    // Closes the queue: no further pushes are accepted and a blocked consumer
    // wakes up. Values already in the queue remain poppable.
    void close() noexcept {
        closed_.store(true, std::memory_order_release);
        sleepCv_.notify_all();
    }

private:
    struct Cell {
        std::atomic<std::size_t> sequence{0};
        typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;
    };

    static std::size_t ValidateCapacity(std::size_t capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("MpscQueue capacity must be positive");
        }
        std::size_t result = 2;
        while (result < capacity) {
            result <<= 1;
        }
        return result;
    }

    bool TryPushImpl(T&& value) {
        if (closed_.load(std::memory_order_acquire)) {
            return false;
        }
        std::size_t position = enqueuePos_.load(std::memory_order_relaxed);
        while (true) {
            Cell& cell = buffer_[position & mask_];
            const std::size_t sequence = cell.sequence.load(std::memory_order_acquire);
            const std::ptrdiff_t difference = static_cast<std::ptrdiff_t>(sequence) -
                                              static_cast<std::ptrdiff_t>(position);
            if (difference == 0) {
                if (enqueuePos_.compare_exchange_weak(position, position + 1,
                                                      std::memory_order_relaxed,
                                                      std::memory_order_relaxed)) {
                    new (static_cast<void*>(&cell.storage)) T(std::move(value));
                    cell.sequence.store(position + 1, std::memory_order_release);
                    if (waiting_.load(std::memory_order_acquire)) {
                        sleepCv_.notify_one();
                    }
                    return true;
                }
            } else if (difference < 0) {
                return false;  // full
            } else {
                position = enqueuePos_.load(std::memory_order_relaxed);
            }
        }
    }

    void Drain() {
        std::size_t position = dequeuePos_.load(std::memory_order_relaxed);
        while (true) {
            Cell& cell = buffer_[position & mask_];
            if (cell.sequence.load(std::memory_order_acquire) != position + 1) {
                break;
            }
            reinterpret_cast<T*>(&cell.storage)->~T();
            dequeuePos_.store(position + 1, std::memory_order_relaxed);
            cell.sequence.store(position + mask_ + 1, std::memory_order_release);
            ++position;
        }
    }

    std::size_t capacity_;
    std::size_t mask_;
    std::vector<Cell> buffer_;
    std::atomic<std::size_t> enqueuePos_{0};
    std::atomic<std::size_t> dequeuePos_{0};
    std::atomic<bool> closed_{false};
    std::atomic<bool> waiting_{false};
    std::mutex sleepMutex_;
    std::condition_variable sleepCv_;
};

}  // namespace containers
