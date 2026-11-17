#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "containers/stdx/Optional.hpp"

namespace containers {

// Rust's `slotmap`: stable slots plus generation-tagged keys. Erasing a slot
// bumps its generation, so stale keys can never observe a reused slot. This is
// the "entity handle" pattern: store the key, not a pointer.
struct SlotKey {
    static constexpr std::uint32_t kInvalidIndex = 0xffffffffu;

    std::uint32_t index = kInvalidIndex;
    std::uint32_t generation = 0;

    bool valid() const noexcept { return index != kInvalidIndex; }
};

inline bool operator==(SlotKey lhs, SlotKey rhs) noexcept {
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

inline bool operator!=(SlotKey lhs, SlotKey rhs) noexcept { return !(lhs == rhs); }

template <typename T>
class SlotMap {
public:
    using key_type = SlotKey;
    using size_type = std::size_t;

    SlotMap() = default;

    key_type insert(T value) {
        if (!free_.empty()) {
            const std::uint32_t index = free_.back();
            free_.pop_back();
            Slot& slot = slots_[index];
            assert(!slot.value.hasValue());
            slot.value.emplace(std::move(value));
            ++slot.generation;
            ++size_;
            return key_type{index, slot.generation};
        }

        slots_.emplace_back();
        Slot& slot = slots_.back();
        slot.value.emplace(std::move(value));
        ++size_;
        return key_type{static_cast<std::uint32_t>(slots_.size() - 1), slot.generation};
    }

    T* get(key_type key) {
        if (!isAlive(key)) {
            return nullptr;
        }
        return &slots_[key.index].value.value();
    }

    const T* get(key_type key) const {
        if (!isAlive(key)) {
            return nullptr;
        }
        return &slots_[key.index].value.value();
    }

    bool contains(key_type key) const { return isAlive(key); }

    T& operator[](key_type key) {
        T* value = get(key);
        assert(value != nullptr);
        return *value;
    }

    const T& operator[](key_type key) const {
        const T* value = get(key);
        assert(value != nullptr);
        return *value;
    }

    bool erase(key_type key) {
        if (!isAlive(key)) {
            return false;
        }
        Slot& slot = slots_[key.index];
        slot.value.reset();
        ++slot.generation;
        free_.push_back(key.index);
        --size_;
        return true;
    }

    size_type size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    size_type capacity() const noexcept { return slots_.size(); }

    // Invalidates every outstanding key, but keeps the slots for reuse (old
    // keys stay dead because generations are bumped).
    void clear() {
        for (size_type index = 0; index < slots_.size(); ++index) {
            Slot& slot = slots_[index];
            if (slot.value.hasValue()) {
                slot.value.reset();
                ++slot.generation;
                free_.push_back(static_cast<std::uint32_t>(index));
            }
        }
        size_ = 0;
    }

    template <typename F>
    void forEach(F&& visitor) {
        for (auto& slot : slots_) {
            if (slot.value.hasValue()) {
                visitor(slot.value.value());
            }
        }
    }

    template <typename F>
    void forEach(F&& visitor) const {
        for (const auto& slot : slots_) {
            if (slot.value.hasValue()) {
                visitor(slot.value.value());
            }
        }
    }

private:
    struct Slot {
        Optional<T> value;
        std::uint32_t generation = 0;
    };

    bool isAlive(key_type key) const {
        return key.index < slots_.size() &&
               slots_[key.index].generation == key.generation &&
               slots_[key.index].value.hasValue();
    }

    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_;
    size_type size_ = 0;
};

} // namespace containers
