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
        : m_capacity(ValidateCapacity(capacity)),
          m_mask(m_capacity - 1),
          m_buffer(m_capacity) {
        for (std::size_t index = 0; index < m_capacity; ++index) {
            m_buffer[index].sequence.store(index, std::memory_order_relaxed);
        }
    }

    MpscQueue(const MpscQueue&) = delete;
    MpscQueue& operator=(const MpscQueue&) = delete;
    MpscQueue(MpscQueue&&) = delete;
    MpscQueue& operator=(MpscQueue&&) = delete;

    ~MpscQueue() { Drain(); }

    std::size_t capacity() const noexcept { return m_capacity; }

    // Approximate number of enqueued but not yet dequeued values.
    std::size_t size() const noexcept {
        const std::size_t enqueued = m_enqueuePos.load(std::memory_order_acquire);
        const std::size_t dequeued = m_dequeuePos.load(std::memory_order_acquire);
        return enqueued - dequeued;
    }

    bool empty() const noexcept { return size() == 0; }

    bool isClosed() const noexcept { return m_closed.load(std::memory_order_acquire); }

    // Non-blocking push: returns false if the queue is full or closed.
    bool tryPush(T value) { return TryPushImpl(std::move(value)); }

    // Blocking push: spins until the value is enqueued; returns false if closed.
    bool push(T value) {
        while (!TryPushImpl(std::move(value))) {
            if (m_closed.load(std::memory_order_acquire)) {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }

    // Non-blocking pop; only the single consumer thread may call this.
    bool tryPop(T& out) {
        std::size_t position = m_dequeuePos.load(std::memory_order_relaxed);
        Cell& cell = m_buffer[position & m_mask];
        if (cell.sequence.load(std::memory_order_acquire) != position + 1) {
            return false;
        }
        T* value = reinterpret_cast<T*>(&cell.storage);
        out = std::move(*value);
        value->~T();
        m_dequeuePos.store(position + 1, std::memory_order_relaxed);
        cell.sequence.store(position + m_mask + 1, std::memory_order_release);
        return true;
    }

    // Blocks until a value is available; returns false when the queue is closed
    // and drained.
    bool waitAndPop(T& out) {
        while (true) {
            if (tryPop(out)) {
                return true;
            }
            if (m_closed.load(std::memory_order_acquire)) {
                return false;
            }
            std::unique_lock<std::mutex> lock(m_sleepMutex);
            m_waiting.store(true, std::memory_order_release);
            m_sleepCv.wait(lock, [this] {
                return m_closed.load(std::memory_order_acquire) || !empty();
            });
            m_waiting.store(false, std::memory_order_relaxed);
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
            if (m_closed.load(std::memory_order_acquire)) {
                return false;
            }
            std::unique_lock<std::mutex> lock(m_sleepMutex);
            m_waiting.store(true, std::memory_order_release);
            const bool signalled = m_sleepCv.wait_until(lock, deadline, [this] {
                return m_closed.load(std::memory_order_acquire) || !empty();
            });
            m_waiting.store(false, std::memory_order_relaxed);
            if (!signalled) {
                return false;
            }
        }
    }

    // Closes the queue: no further pushes are accepted and a blocked consumer
    // wakes up. Values already in the queue remain poppable.
    void close() noexcept {
        m_closed.store(true, std::memory_order_release);
        m_sleepCv.notify_all();
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
        if (m_closed.load(std::memory_order_acquire)) {
            return false;
        }
        std::size_t position = m_enqueuePos.load(std::memory_order_relaxed);
        while (true) {
            Cell& cell = m_buffer[position & m_mask];
            const std::size_t sequence = cell.sequence.load(std::memory_order_acquire);
            const std::ptrdiff_t difference = static_cast<std::ptrdiff_t>(sequence) -
                                              static_cast<std::ptrdiff_t>(position);
            if (difference == 0) {
                if (m_enqueuePos.compare_exchange_weak(position, position + 1,
                                                      std::memory_order_relaxed,
                                                      std::memory_order_relaxed)) {
                    new (static_cast<void*>(&cell.storage)) T(std::move(value));
                    cell.sequence.store(position + 1, std::memory_order_release);
                    if (m_waiting.load(std::memory_order_acquire)) {
                        m_sleepCv.notify_one();
                    }
                    return true;
                }
            } else if (difference < 0) {
                return false;  // full
            } else {
                position = m_enqueuePos.load(std::memory_order_relaxed);
            }
        }
    }

    void Drain() {
        std::size_t position = m_dequeuePos.load(std::memory_order_relaxed);
        while (true) {
            Cell& cell = m_buffer[position & m_mask];
            if (cell.sequence.load(std::memory_order_acquire) != position + 1) {
                break;
            }
            reinterpret_cast<T*>(&cell.storage)->~T();
            m_dequeuePos.store(position + 1, std::memory_order_relaxed);
            cell.sequence.store(position + m_mask + 1, std::memory_order_release);
            ++position;
        }
    }

    std::size_t m_capacity;
    std::size_t m_mask;
    std::vector<Cell> m_buffer;
    std::atomic<std::size_t> m_enqueuePos{0};
    std::atomic<std::size_t> m_dequeuePos{0};
    std::atomic<bool> m_closed{false};
    std::atomic<bool> m_waiting{false};
    std::mutex m_sleepMutex;
    std::condition_variable m_sleepCv;
};

}  // namespace containers
