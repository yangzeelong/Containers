#ifndef CONTAINERS_COMMON_SPSC_QUEUE_HPP
#define CONTAINERS_COMMON_SPSC_QUEUE_HPP

#include "containers/ConcurrentQueue.hpp"
#include "containers/Optional.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>    // _mm_pause()
#endif

namespace containers {

/**
 * @class SpscQueue
 * @brief Bounded single-producer / single-consumer queue used for graph edges.
 *
 * Every edge of a pipeline has exactly one producer (the upstream module's
 * worker thread) and one consumer (the downstream module's worker thread), so
 * the per-edge queue can use a lock-free ring buffer instead of taking a mutex
 * on every push/pop. Only the blocking paths (a full queue or an empty queue)
 * touch the mutex and the condition variables, so idle workers still sleep
 * instead of busy-spinning.
 *
 * The push side is the fast path: a producer stores the item and publishes the
 * new tail with a release store. The consumer only has to notice that tail from
 * a back-off loop before it takes the wait lock, and the producer publishes to
 * the consumer the same way. The wake-up handshake therefore goes through one
 * release store / acquire load instead of two independent relaxed flags, which
 * is what a partially ordered relaxed flag pair cannot guarantee on x86.
 *
 * A non-positive capacity falls back to ConcurrentQueue, which keeps the
 * unbounded case (-1) and its exact semantics. The ring recycles slots as soon
 * as the consumer has moved the head, so a bounded capacity N needs exactly
 * N * sizeof(T) bytes and never grows.
 *
 * A waiting thread announces itself under the wait mutex *before* it checks the
 * queue one last time, and the fast path takes that same mutex after it has
 * published the item and before it notifies. A notification can therefore never
 * slip into the window between the waiter's last check and its actual wait.
 *
 * Announcing is a store followed by a load of the ring index, and waking is a
 * store of that index followed by a load of the flag: a store-buffer (Dekker)
 * handshake, which is only correct when both sides put a StoreLoad barrier
 * between their own store and their own load. The waiter's barrier is the
 * seq_cst store of the flag (an xchg on x86), the fast path's barrier is the
 * seq_cst store of the ring index it publishes anyway (also an xchg on x86,
 * on a line the producer/consumer already owns). Without that barrier the fast
 * path may read a stale "nobody is waiting" while the waiter's "I am waiting"
 * store is still in its store buffer, and the item stays in the ring with both
 * threads asleep.
 *
 * The hot indices are padded to a full cache line each: head is written by the
 * consumer and tail by the producer, so letting them share a line would make
 * every message invalidate the other core's copy (false sharing) and the
 * lock-free ring would lose against a mutex-protected deque.
 *
 * @tparam T The type of elements stored in the queue.
 */
template <typename T>
class SpscQueue {
public:
    struct Stats {
        std::size_t size          = 0;
        std::size_t capacity      = 0;    // 0 means unbounded
        std::size_t highWaterMark = 0;
        std::uint64_t pushed      = 0;
        std::uint64_t popped      = 0;
    };

    /**
     * @brief Constructs the queue.
     * @param capacity The maximum capacity. A value of -1 (default) means
     *        unbounded; other non-positive values are treated the same way.
     */
    explicit SpscQueue(int capacity = -1) : m_capacity(capacity) {
        if (capacity <= 0) {
            m_fallback.reset(new ConcurrentQueue<T>(-1));
            return;
        }
        m_slotCount = static_cast<std::size_t>(capacity);
        m_slots.reset(new Slot[m_slotCount]);
    }

    ~SpscQueue() {
        if (m_fallback || !m_slots) {
            return;
        }
        const std::size_t head = m_head.load(std::memory_order_relaxed);
        const std::size_t tail = m_tail.load(std::memory_order_relaxed);
        for (std::size_t index = head; index != tail; ++index) {
            ElementAt(index % m_slotCount)->~T();
        }
    }

    SpscQueue(const SpscQueue&)            = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;

    bool push(T&& item) {
        if (m_fallback) {
            return m_fallback->push(std::move(item));
        }
        if (m_shutdown.load(std::memory_order_relaxed)) {
            return false;
        }
        if (PutIfSpace(item)) {
            WakeConsumer();
            return true;
        }
        const int spun = SpinAttempt([this, &item]() { return PutIfSpace(item); },
                                     [this]() { return m_head.load(std::memory_order_acquire); });
        if (spun > 0) {
            WakeConsumer();
            return true;
        }
        if (spun < 0) {
            return false;
        }
        std::unique_lock<std::mutex> lock(m_waitMutex);
        // seq_cst, not release: this store must be globally visible before the
        // last PutIfSpace() below reads the tail, see the class comment.
        m_producerWaiting.store(true, std::memory_order_seq_cst);
        for (;;) {
            if (PutIfSpace(item)) {
                m_producerWaiting.store(false, std::memory_order_relaxed);
                m_condNotEmpty.notify_one();
                return true;
            }
            if (m_shutdown.load(std::memory_order_relaxed)) {
                m_producerWaiting.store(false, std::memory_order_relaxed);
                return false;
            }
            m_condNotFull.wait(lock);
        }
    }

    bool push(const T& item) {
        T temp = item;
        return push(std::move(temp));
    }

    template <class Rep, class Per>
    bool pushFor(T&& item, const std::chrono::duration<Rep, Per>& timeout) {
        if (m_fallback) {
            return m_fallback->pushFor(std::move(item), timeout);
        }
        if (m_shutdown.load(std::memory_order_relaxed)) {
            return false;
        }
        if (PutIfSpace(item)) {
            WakeConsumer();
            return true;
        }
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        const int spun = timeout < std::chrono::microseconds(kSpinBudgetMicros)
                             ? 0
                             : SpinAttempt([this, &item]() { return PutIfSpace(item); },
                                           [this]() { return m_head.load(std::memory_order_acquire); });
        if (spun > 0) {
            WakeConsumer();
            return true;
        }
        if (spun < 0) {
            return false;
        }
        std::unique_lock<std::mutex> lock(m_waitMutex);
        m_producerWaiting.store(true, std::memory_order_seq_cst);
        for (;;) {
            if (PutIfSpace(item)) {
                m_producerWaiting.store(false, std::memory_order_relaxed);
                m_condNotEmpty.notify_one();
                return true;
            }
            if (m_shutdown.load(std::memory_order_relaxed)) {
                m_producerWaiting.store(false, std::memory_order_relaxed);
                return false;
            }
            if (m_condNotFull.wait_until(lock, deadline) == std::cv_status::timeout) {
                const bool pushed = PutIfSpace(item);
                m_producerWaiting.store(false, std::memory_order_relaxed);
                if (pushed) {
                    m_condNotEmpty.notify_one();
                }
                return pushed;
            }
        }
    }

    template <class Rep, class Per>
    bool pushFor(const T& item, const std::chrono::duration<Rep, Per>& timeout) {
        T temp = item;
        return pushFor(std::move(temp), timeout);
    }

    bool tryPush(T&& item) {
        if (m_fallback) {
            return m_fallback->tryPush(std::move(item));
        }
        if (m_shutdown.load(std::memory_order_relaxed)) {
            return false;
        }
        if (!PutIfSpace(item)) {
            return false;
        }
        WakeConsumer();
        return true;
    }

    bool tryPush(const T& item) {
        T temp = item;
        return tryPush(std::move(temp));
    }

    bool waitAndPop(T& itemRef) {
        if (m_fallback) {
            return m_fallback->waitAndPop(itemRef);
        }
        if (PopIfAvailable(itemRef)) {
            WakeProducer();
            return true;
        }
        const int spun = SpinAttempt([this, &itemRef]() { return PopIfAvailable(itemRef); },
                                     [this]() { return m_tail.load(std::memory_order_acquire); });
        if (spun > 0) {
            WakeProducer();
            return true;
        }
        if (spun < 0) {
            return false;
        }
        std::unique_lock<std::mutex> lock(m_waitMutex);
        m_consumerWaiting.store(true, std::memory_order_seq_cst);
        for (;;) {
            if (PopIfAvailable(itemRef)) {
                m_consumerWaiting.store(false, std::memory_order_relaxed);
                m_condNotFull.notify_one();
                return true;
            }
            if (m_shutdown.load(std::memory_order_acquire)) {
                m_consumerWaiting.store(false, std::memory_order_relaxed);
                return false;
            }
            m_condNotEmpty.wait(lock);
        }
    }

    template <class Rep, class Per>
    bool waitAndPopFor(T& itemRef, const std::chrono::duration<Rep, Per>& timeout) {
        if (m_fallback) {
            return m_fallback->waitAndPopFor(itemRef, timeout);
        }
        if (PopIfAvailable(itemRef)) {
            WakeProducer();
            return true;
        }
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        const int spun = timeout < std::chrono::microseconds(kSpinBudgetMicros)
                             ? 0
                             : SpinAttempt([this, &itemRef]() { return PopIfAvailable(itemRef); },
                                           [this]() { return m_tail.load(std::memory_order_acquire); });
        if (spun > 0) {
            WakeProducer();
            return true;
        }
        if (spun < 0) {
            return false;
        }
        std::unique_lock<std::mutex> lock(m_waitMutex);
        m_consumerWaiting.store(true, std::memory_order_seq_cst);
        for (;;) {
            if (PopIfAvailable(itemRef)) {
                m_consumerWaiting.store(false, std::memory_order_relaxed);
                m_condNotFull.notify_one();
                return true;
            }
            if (m_shutdown.load(std::memory_order_acquire)) {
                m_consumerWaiting.store(false, std::memory_order_relaxed);
                return false;
            }
            if (m_condNotEmpty.wait_until(lock, deadline) == std::cv_status::timeout) {
                const bool popped = PopIfAvailable(itemRef);
                m_consumerWaiting.store(false, std::memory_order_relaxed);
                if (popped) {
                    m_condNotFull.notify_one();
                }
                return popped;
            }
        }
    }

    Optional<T> tryPop() {
        if (m_fallback) {
            return m_fallback->tryPop();
        }
        T item;
        if (!PopIfAvailable(item)) {
            return nullOpt;
        }
        WakeProducer();
        return Optional<T>(std::move(item));
    }

    bool tryPop(T& itemRef) {
        if (m_fallback) {
            return m_fallback->tryPop(itemRef);
        }
        if (!PopIfAvailable(itemRef)) {
            return false;
        }
        WakeProducer();
        return true;
    }

    void shutdown() {
        if (m_fallback) {
            m_fallback->shutdown();
            return;
        }
        m_shutdown.store(true, std::memory_order_release);
        {
            // Pairs with the producers/consumers that announce themselves under
            // the same lock: once it is released here, every waiter that already
            // holds the lock has seen the flag, and the notify below reaches it.
            std::lock_guard<std::mutex> lock(m_waitMutex);
        }
        m_condNotEmpty.notify_all();
        m_condNotFull.notify_all();
    }

    bool isEmpty() const { return getSize() == 0; }

    std::size_t getSize() const {
        if (m_fallback) {
            return m_fallback->getSize();
        }
        // The two indices are published by different threads, so a sample can
        // see a newer head than tail. The counters only ever grow, so retrying
        // is enough to keep the result sane instead of returning a huge wrap.
        for (int attempt = 0; attempt < 4; ++attempt) {
            const std::size_t head = m_head.load(std::memory_order_acquire);
            const std::size_t tail = m_tail.load(std::memory_order_acquire);
            if (tail >= head) {
                return tail - head;
            }
        }
        return 0;
    }

    Stats GetStats() const {
        Stats stats;
        if (m_fallback) {
            const typename ConcurrentQueue<T>::Stats fallbackStats = m_fallback->GetStats();
            stats.size                                             = fallbackStats.size;
            stats.capacity                                         = fallbackStats.capacity;
            stats.highWaterMark                                    = fallbackStats.highWaterMark;
            stats.pushed                                          = fallbackStats.pushed;
            stats.popped                                          = fallbackStats.popped;
            return stats;
        }
        stats.size          = getSize();
        stats.capacity      = static_cast<std::size_t>(m_capacity);
        stats.highWaterMark = m_highWaterMark.load(std::memory_order_relaxed);
        // The ring keeps no separate counters: every successful push/pop moves
        // the respective index by exactly one, and the hot path must not touch a
        // third cache line for statistics.
        stats.pushed = m_tail.load(std::memory_order_relaxed);
        stats.popped = m_head.load(std::memory_order_relaxed);
        return stats;
    }

    int capacity() const { return m_capacity; }

    bool hasLockFreeStorage() const { return m_fallback == nullptr; }

private:
    /// Padding unit for the hot indices: one cache line, doubled to also keep
    /// the adjacent-line prefetcher of recent cores from pulling in the
    /// neighbouring index.
    static constexpr std::size_t kCacheLineSize = 128;

    /// How long a blocking path re-checks the ring before it parks on the
    /// condition variable. Both ends of an edge are usually only a processing
    /// batch apart, while parking costs a round trip through the OS scheduler.
    static constexpr int kSpinBudgetMicros = 20;
    static constexpr int kSpinClockMask    = 15;

    static void SpinPause() {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
        _mm_pause();
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
        __builtin_ia32_pause();
#else
        std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
    }

    /**
     * @brief Retries @p attempt while the other side keeps making progress.
     * @return 1 on success, 0 when the budget expired, -1 on shutdown.
     */
    template <typename Attempt, typename Progress>
    int SpinAttempt(Attempt attempt, Progress progress) {
        const auto budget    = std::chrono::microseconds(kSpinBudgetMicros);
        std::size_t observed = progress();
        auto deadline        = std::chrono::steady_clock::now() + budget;
        for (int tick = 0;; ++tick) {
            if (attempt()) {
                return 1;
            }
            if (m_shutdown.load(std::memory_order_acquire)) {
                return -1;
            }
            if ((tick & kSpinClockMask) == 0) {
                const auto now            = std::chrono::steady_clock::now();
                const std::size_t current = progress();
                if (current != observed) {
                    observed = current;
                    deadline = now + budget;
                } else if (now >= deadline) {
                    return 0;
                }
            }
            SpinPause();
        }
    }

    struct Slot {
        typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;
    };

    T* ElementAt(std::size_t index) const {
        return reinterpret_cast<T*>(
            const_cast<typename std::aligned_storage<sizeof(T), alignof(T)>::type*>(&m_slots[index].storage));
    }

    bool PutIfSpace(T& item) {
        const std::size_t head = m_head.load(std::memory_order_acquire);
        const std::size_t tail = m_tail.load(std::memory_order_relaxed);
        const std::size_t size = tail - head;
        if (size >= m_slotCount) {
            return false;
        }
        new (ElementAt(tail % m_slotCount)) T(std::move(item));
        // seq_cst, not release: this store publishes the item *and* acts as the
        // StoreLoad barrier before WakeConsumer() reads the waiter flag.
        m_tail.store(tail + 1, std::memory_order_seq_cst);
        if (size + 1 > m_highWaterMark.load(std::memory_order_relaxed)) {
            m_highWaterMark.store(size + 1, std::memory_order_relaxed);
        }
        return true;
    }

    bool PopIfAvailable(T& itemRef) {
        const std::size_t head = m_head.load(std::memory_order_relaxed);
        const std::size_t tail = m_tail.load(std::memory_order_acquire);
        if (head == tail) {
            return false;
        }
        T* slot = ElementAt(head % m_slotCount);
        itemRef = std::move(*slot);
        slot->~T();
        m_head.store(head + 1, std::memory_order_seq_cst);
        return true;
    }

    void WakeConsumer() {
        if (m_consumerWaiting.load(std::memory_order_seq_cst)) {
            std::lock_guard<std::mutex> lock(m_waitMutex);
            m_condNotEmpty.notify_one();
        }
    }

    void WakeProducer() {
        if (m_producerWaiting.load(std::memory_order_seq_cst)) {
            std::lock_guard<std::mutex> lock(m_waitMutex);
            m_condNotFull.notify_one();
        }
    }

    int m_capacity          = -1;
    std::size_t m_slotCount = 0;
    std::unique_ptr<Slot[]> m_slots;
    std::unique_ptr<ConcurrentQueue<T>> m_fallback;

    // Producer-owned hot line.
    std::atomic<std::size_t> m_tail{0};
    std::atomic<std::size_t> m_highWaterMark{0};
    char m_producerLine[kCacheLineSize - 2 * sizeof(std::atomic<std::size_t>)];

    // Consumer-owned hot line.
    std::atomic<std::size_t> m_head{0};
    char m_consumerLine[kCacheLineSize - sizeof(std::atomic<std::size_t>)];

    std::atomic<bool> m_consumerWaiting{false};
    char m_consumerWaitingLine[kCacheLineSize - sizeof(std::atomic<bool>)];
    std::atomic<bool> m_producerWaiting{false};
    char m_producerWaitingLine[kCacheLineSize - sizeof(std::atomic<bool>)];

    std::atomic<bool> m_shutdown{false};
    mutable std::mutex m_waitMutex;
    std::condition_variable m_condNotEmpty;
    std::condition_variable m_condNotFull;
};

} // namespace containers

#endif // CONTAINERS_COMMON_SPSC_QUEUE_HPP
