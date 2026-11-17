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

    SmallVec() noexcept : size_(0), heapMode_(false) {}

    SmallVec(const SmallVec& other) : size_(0), heapMode_(false) { copyFrom(other); }

    SmallVec(SmallVec&& other) : size_(0), heapMode_(false) { moveFrom(std::move(other)); }

    ~SmallVec() { clear(); }

    SmallVec& operator=(const SmallVec& other) {
        if (this != &other) {
            clear();
            heap_.clear();
            heapMode_ = false;
            copyFrom(other);
        }
        return *this;
    }

    SmallVec& operator=(SmallVec&& other) {
        if (this != &other) {
            clear();
            heap_.clear();
            heapMode_ = false;
            moveFrom(std::move(other));
        }
        return *this;
    }

    size_type size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    size_type capacity() const noexcept { return heapMode_ ? heap_.capacity() : InlineCapacity; }
    bool isInline() const noexcept { return !heapMode_; }

    T* data() noexcept { return heapMode_ ? heap_.data() : inlinePtr(); }
    const T* data() const noexcept {
        return heapMode_ ? heap_.data() : reinterpret_cast<const T*>(inline_);
    }

    iterator begin() noexcept { return data(); }
    iterator end() noexcept { return data() + size_; }
    const_iterator begin() const noexcept { return data(); }
    const_iterator end() const noexcept { return data() + size_; }

    T& operator[](size_type index) noexcept { return data()[index]; }
    const T& operator[](size_type index) const noexcept { return data()[index]; }

    T& front() noexcept { return data()[0]; }
    const T& front() const noexcept { return data()[0]; }
    T& back() noexcept { return data()[size_ - 1]; }
    const T& back() const noexcept { return data()[size_ - 1]; }

    template <typename... Args>
    T& emplace_back(Args&&... args) {
        if (!heapMode_ && size_ == InlineCapacity) {
            promote();
        }
        if (heapMode_) {
            heap_.emplace_back(std::forward<Args>(args)...);
            size_ = heap_.size();
            return heap_.back();
        }
        new (inlinePtr() + size_) T(std::forward<Args>(args)...);
        ++size_;
        return inlinePtr()[size_ - 1];
    }

    void push_back(const T& value) { emplace_back(value); }
    void push_back(T&& value) { emplace_back(std::move(value)); }

    void pop_back() {
        if (heapMode_) {
            heap_.pop_back();
            size_ = heap_.size();
        } else {
            inlinePtr()[--size_].~T();
        }
    }

    void clear() noexcept {
        if (heapMode_) {
            heap_.clear();
            size_ = 0;
        } else {
            while (size_ > 0) {
                inlinePtr()[--size_].~T();
            }
        }
    }

    void reserve(size_type capacity) {
        if (capacity <= InlineCapacity) {
            return;
        }
        if (!heapMode_) {
            promote();
        }
        heap_.reserve(capacity);
    }

private:
    T* inlinePtr() noexcept { return reinterpret_cast<T*>(inline_); }
    const T* inlinePtr() const noexcept { return reinterpret_cast<const T*>(inline_); }

    void promote() {
        heap_.reserve(InlineCapacity * 2);
        for (size_type index = 0; index < size_; ++index) {
            heap_.push_back(std::move(inlinePtr()[index]));
            inlinePtr()[index].~T();
        }
        heapMode_ = true;
    }

    void copyFrom(const SmallVec& other) {
        if (other.heapMode_) {
            heap_ = other.heap_;
            heapMode_ = true;
            size_ = heap_.size();
        } else {
            for (size_type index = 0; index < other.size_; ++index) {
                new (inlinePtr() + index) T(other.inlinePtr()[index]);
            }
            size_ = other.size_;
            heapMode_ = false;
        }
    }

    void moveFrom(SmallVec&& other) {
        if (other.heapMode_) {
            heap_ = std::move(other.heap_);
            heapMode_ = true;
            size_ = other.size_;
            other.size_ = 0;
            other.heapMode_ = false;
            other.heap_.clear();
        } else {
            for (size_type index = 0; index < other.size_; ++index) {
                new (inlinePtr() + index) T(std::move(other.inlinePtr()[index]));
            }
            size_ = other.size_;
            other.size_ = 0;
            heapMode_ = false;
        }
    }

    typename std::aligned_storage<sizeof(T), alignof(T)>::type inline_[InlineCapacity];
    std::vector<T> heap_;
    size_type size_;
    bool heapMode_;
};

} // namespace containers
