#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace containers {

// Rust's `smallvec`: a vector with inline storage for a small number of
// elements. Pushing beyond InlineCapacity moves the payload into a heap vector
// once, after which the SmallVec behaves like a normal vector (capacity grows,
// inline storage is unused).
template <typename T, std::size_t InlineCapacity>
class SmallVec {
    static_assert(InlineCapacity > 0, "SmallVec needs a non-zero inline capacity");
    static_assert(!std::is_reference<T>::value, "SmallVec does not hold references");

public:
    using value_type = T;
    using size_type = std::size_t;
    using iterator = T*;
    using const_iterator = const T*;

    static constexpr size_type kInlineCapacity = InlineCapacity;

    SmallVec() noexcept : m_size(0), m_heapMode(false) {}

    SmallVec(const SmallVec& other) : m_size(0), m_heapMode(false) { copyFrom(other); }

    SmallVec(SmallVec&& other) : m_size(0), m_heapMode(false) { moveFrom(std::move(other)); }

    ~SmallVec() { clear(); }

    SmallVec& operator=(const SmallVec& other) {
        if (this != &other) {
            clear();
            m_heap.clear();
            m_heapMode = false;
            copyFrom(other);
        }
        return *this;
    }

    SmallVec& operator=(SmallVec&& other) {
        if (this != &other) {
            clear();
            m_heap.clear();
            m_heapMode = false;
            moveFrom(std::move(other));
        }
        return *this;
    }

    size_type size() const noexcept { return m_size; }
    bool empty() const noexcept { return m_size == 0; }
    size_type capacity() const noexcept { return m_heapMode ? m_heap.capacity() : InlineCapacity; }
    bool isInline() const noexcept { return !m_heapMode; }

    T* data() noexcept { return m_heapMode ? m_heap.data() : inlinePtr(); }
    const T* data() const noexcept {
        return m_heapMode ? m_heap.data() : reinterpret_cast<const T*>(m_inline);
    }

    iterator begin() noexcept { return data(); }
    iterator end() noexcept { return data() + m_size; }
    const_iterator begin() const noexcept { return data(); }
    const_iterator end() const noexcept { return data() + m_size; }

    T& operator[](size_type index) noexcept { return data()[index]; }
    const T& operator[](size_type index) const noexcept { return data()[index]; }

    T& front() noexcept { return data()[0]; }
    const T& front() const noexcept { return data()[0]; }
    T& back() noexcept { return data()[m_size - 1]; }
    const T& back() const noexcept { return data()[m_size - 1]; }

    template <typename... Args>
    T& emplace_back(Args&&... args) {
        if (!m_heapMode && m_size == InlineCapacity) {
            promote();
        }
        if (m_heapMode) {
            m_heap.emplace_back(std::forward<Args>(args)...);
            m_size = m_heap.size();
            return m_heap.back();
        }
        new (inlinePtr() + m_size) T(std::forward<Args>(args)...);
        ++m_size;
        return inlinePtr()[m_size - 1];
    }

    void push_back(const T& value) { emplace_back(value); }
    void push_back(T&& value) { emplace_back(std::move(value)); }

    void pop_back() {
        if (m_heapMode) {
            m_heap.pop_back();
            m_size = m_heap.size();
        } else {
            inlinePtr()[--m_size].~T();
        }
    }

    void clear() noexcept {
        if (m_heapMode) {
            m_heap.clear();
            m_size = 0;
        } else {
            while (m_size > 0) {
                inlinePtr()[--m_size].~T();
            }
        }
    }

    void reserve(size_type capacity) {
        if (capacity <= InlineCapacity) {
            return;
        }
        if (!m_heapMode) {
            promote();
        }
        m_heap.reserve(capacity);
    }

private:
    T* inlinePtr() noexcept { return reinterpret_cast<T*>(m_inline); }
    const T* inlinePtr() const noexcept { return reinterpret_cast<const T*>(m_inline); }

    void promote() {
        m_heap.reserve(InlineCapacity * 2);
        for (size_type index = 0; index < m_size; ++index) {
            m_heap.push_back(std::move(inlinePtr()[index]));
            inlinePtr()[index].~T();
        }
        m_heapMode = true;
    }

    void copyFrom(const SmallVec& other) {
        if (other.m_heapMode) {
            m_heap = other.m_heap;
            m_heapMode = true;
            m_size = m_heap.size();
        } else {
            for (size_type index = 0; index < other.m_size; ++index) {
                new (inlinePtr() + index) T(other.inlinePtr()[index]);
            }
            m_size = other.m_size;
            m_heapMode = false;
        }
    }

    void moveFrom(SmallVec&& other) {
        if (other.m_heapMode) {
            m_heap = std::move(other.m_heap);
            m_heapMode = true;
            m_size = other.m_size;
            other.m_size = 0;
            other.m_heapMode = false;
            other.m_heap.clear();
        } else {
            for (size_type index = 0; index < other.m_size; ++index) {
                new (inlinePtr() + index) T(std::move(other.inlinePtr()[index]));
            }
            m_size = other.m_size;
            other.m_size = 0;
            m_heapMode = false;
        }
    }

    typename std::aligned_storage<sizeof(T), alignof(T)>::type m_inline[InlineCapacity];
    std::vector<T> m_heap;
    size_type m_size;
    bool m_heapMode;
};

} // namespace containers
