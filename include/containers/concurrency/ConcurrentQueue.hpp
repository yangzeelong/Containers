#ifndef CONTAINERS_CONCURRENT_QUEUE_HPP
#define CONTAINERS_CONCURRENT_QUEUE_HPP

#include "containers/stdx/Optional.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>

/**
 * @class ConcurrentQueue
 * @brief A thread-safe, blocking queue for producer-consumer scenarios.
 *
 * This queue can be configured as bounded (with a fixed capacity) or unbounded.
 * It uses condition variables to block producing threads when the queue is full
 * and consuming threads when the queue is empty, preventing busy-waiting.
 *
 * @tparam T The type of elements stored in the queue.
 */
template <typename T>
class ConcurrentQueue {
public:
    /**
     * @brief Constructs a ConcurrentQueue.
     * @param capacity The maximum capacity of the queue. A value of -1 (default)
     *                 indicates an unbounded queue.
     */
    explicit ConcurrentQueue(int capacity = -1) : m_capacity(capacity), m_shutdown(false) {}

    /**
     * @brief Snapshot of the queue size and its lifetime counters.
     */
    struct Stats {
        std::size_t size = 0;
        std::size_t capacity = 0; // 0 means unbounded
        std::size_t highWaterMark = 0;
        std::uint64_t pushed = 0;
        std::uint64_t popped = 0;
    };

    /**
     * @brief Returns a consistent snapshot, sampled under the queue lock.
     */
    Stats GetStats() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        Stats stats;
        stats.size = m_queue.size();
        stats.capacity = m_capacity < 0 ? 0 : static_cast<std::size_t>(m_capacity);
        stats.highWaterMark = m_highWaterMark;
        stats.pushed = m_pushed;
        stats.popped = m_popped;
        return stats;
    }

    // Disable copy and move semantics to ensure a single owner.
    ConcurrentQueue(const ConcurrentQueue&) = delete;
    ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;

    bool push(T&& item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condNotFull.wait(lock, [this] { return m_shutdown || !isFull(); });

        if (m_shutdown) {
            return false;
        }

        PushLocked(std::move(item));
        return true;
    }

    bool push(const T& item) {
        T temp = item;
        return push(std::move(temp));
    }

    template <class Rep, class Per>
    bool pushFor(T&& item, const std::chrono::duration<Rep, Per>& timeout) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_condNotFull.wait_for(lock, timeout, [this] { return m_shutdown || !isFull(); })) {
            return false;
        }
        if (m_shutdown) return false;
        PushLocked(std::move(item));
        return true;
    }

    template <class Rep, class Per>
    bool pushFor(const T& item, const std::chrono::duration<Rep, Per>& timeout) {
        return pushFor(T(item), timeout);
    }

    bool tryPush(T&& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_shutdown || isFull()) {
            return false;
        }

        PushLocked(std::move(item));
        return true;
    }

    bool tryPush(const T& item) {
        T temp = item;
        return tryPush(std::move(temp));
    }

    template <class Rep, class Per>
    bool waitAndPopFor(T& itemRef, const std::chrono::duration<Rep, Per>& timeout) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_condNotEmpty.wait_for(lock, timeout, [this] { return m_shutdown || !m_queue.empty(); })) {
            return false;
        }
        if (m_shutdown && m_queue.empty()) return false;
        itemRef = PopLocked();
        return true;
    }

    bool waitAndPop(T& itemRef) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condNotEmpty.wait(lock, [this] { return m_shutdown || !m_queue.empty(); });

        if (m_shutdown && m_queue.empty()) {
            return false;
        }

        itemRef = PopLocked();
        return true;
    }

    Optional<T> tryPop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return nullOpt;
        }
        return PopLocked();
    }

    bool tryPop(T& itemRef) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return false;
        }
        itemRef = PopLocked();
        return true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shutdown = true;
        m_condNotEmpty.notify_all();
        m_condNotFull.notify_all();
    }

    bool isEmpty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    std::size_t getSize() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

private:
    bool isFull() const {
        if (m_capacity == -1) {
            return false;
        }
        return m_queue.size() >= static_cast<std::size_t>(m_capacity);
    }

    void PushLocked(T&& item) {
        m_queue.push(std::move(item));
        ++m_pushed;
        if (m_queue.size() > m_highWaterMark) {
            m_highWaterMark = m_queue.size();
        }
        m_condNotEmpty.notify_one();
    }

    T PopLocked() {
        T item = std::move(m_queue.front());
        m_queue.pop();
        ++m_popped;
        m_condNotFull.notify_one();
        return item;
    }

    mutable std::mutex m_mutex;
    std::condition_variable m_condNotEmpty;
    std::condition_variable m_condNotFull;
    std::queue<T> m_queue;
    std::uint64_t m_pushed = 0;
    std::uint64_t m_popped = 0;
    std::size_t m_highWaterMark = 0;
    const int m_capacity;
    bool m_shutdown;
};

#endif // CONTAINERS_CONCURRENT_QUEUE_HPP
