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
        if (!m_free.empty()) {
            const std::uint32_t index = m_free.back();
            m_free.pop_back();
            Slot& slot = m_slots[index];
            assert(!slot.value.hasValue());
            slot.value.emplace(std::move(value));
            ++slot.generation;
            ++m_size;
            return key_type{index, slot.generation};
        }

        m_slots.emplace_back();
        Slot& slot = m_slots.back();
        slot.value.emplace(std::move(value));
        ++m_size;
        return key_type{static_cast<std::uint32_t>(m_slots.size() - 1), slot.generation};
    }

    T* get(key_type key) {
        if (!isAlive(key)) {
            return nullptr;
        }
        return &m_slots[key.index].value.value();
    }

    const T* get(key_type key) const {
        if (!isAlive(key)) {
            return nullptr;
        }
        return &m_slots[key.index].value.value();
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
        Slot& slot = m_slots[key.index];
        slot.value.reset();
        ++slot.generation;
        m_free.push_back(key.index);
        --m_size;
        return true;
    }

    size_type size() const noexcept { return m_size; }
    bool empty() const noexcept { return m_size == 0; }
    size_type capacity() const noexcept { return m_slots.size(); }

    // Invalidates every outstanding key, but keeps the slots for reuse (old
    // keys stay dead because generations are bumped).
    void clear() {
        for (size_type index = 0; index < m_slots.size(); ++index) {
            Slot& slot = m_slots[index];
            if (slot.value.hasValue()) {
                slot.value.reset();
                ++slot.generation;
                m_free.push_back(static_cast<std::uint32_t>(index));
            }
        }
        m_size = 0;
    }

    template <typename F>
    void forEach(F&& visitor) {
        for (auto& slot : m_slots) {
            if (slot.value.hasValue()) {
                visitor(slot.value.value());
            }
        }
    }

    template <typename F>
    void forEach(F&& visitor) const {
        for (const auto& slot : m_slots) {
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
        return key.index < m_slots.size() &&
               m_slots[key.index].generation == key.generation &&
               m_slots[key.index].value.hasValue();
    }

    std::vector<Slot> m_slots;
    std::vector<std::uint32_t> m_free;
    size_type m_size = 0;
};

} // namespace containers
