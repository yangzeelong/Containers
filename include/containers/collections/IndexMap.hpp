#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace containers {

// Rust's `indexmap`: a hash map that preserves insertion order. Entries live in
// a contiguous vector (iteration is cache-friendly and ordered) while a hash
// index points into that vector for O(1) lookup.
//
// Erase has two flavours: `erase` keeps the insertion order and costs O(n) to
// renumber the index, `swapErase` is O(1) but moves the last entry into the hole.
template <typename K, typename V, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>>
class IndexMap {
public:
    using key_type = K;
    using mapped_type = V;
    using value_type = std::pair<K, V>;
    using storage_type = std::vector<value_type>;
    using size_type = std::size_t;
    using iterator = typename storage_type::iterator;
    using const_iterator = typename storage_type::const_iterator;

    IndexMap() = default;

    size_type size() const noexcept { return m_storage.size(); }
    bool empty() const noexcept { return m_storage.empty(); }

    void reserve(size_type capacity) {
        m_storage.reserve(capacity);
        m_index.reserve(capacity);
    }

    // Returns true when the key was new, false when an existing value was
    // replaced (the original position is kept, like IndexMap::insert).
    bool insert(K key, V value) {
        auto found = m_index.find(key);
        if (found != m_index.end()) {
            m_storage[found->second].second = std::move(value);
            return false;
        }
        m_storage.emplace_back(std::move(key), std::move(value));
        m_index.emplace(m_storage.back().first, m_storage.size() - 1);
        return true;
    }

    V* find(const K& key) {
        auto found = m_index.find(key);
        if (found == m_index.end()) {
            return nullptr;
        }
        return &m_storage[found->second].second;
    }

    const V* find(const K& key) const {
        auto found = m_index.find(key);
        if (found == m_index.end()) {
            return nullptr;
        }
        return &m_storage[found->second].second;
    }

    bool contains(const K& key) const { return m_index.find(key) != m_index.end(); }

    V& at(const K& key) {
        V* value = find(key);
        assert(value != nullptr);
        return *value;
    }

    const V& at(const K& key) const {
        const V* value = find(key);
        assert(value != nullptr);
        return *value;
    }

    // Inserts V{} when the key is absent, like a std::map::operator[] (but keeps
    // insertion order). Requires K to be copyable and V default constructible.
    V& operator[](const K& key) {
        auto found = m_index.find(key);
        if (found != m_index.end()) {
            return m_storage[found->second].second;
        }
        m_storage.emplace_back(key, V());
        m_index.emplace(m_storage.back().first, m_storage.size() - 1);
        return m_storage.back().second;
    }

    // O(n): removes the entry and shifts later entries down, preserving order.
    bool erase(const K& key) {
        auto found = m_index.find(key);
        if (found == m_index.end()) {
            return false;
        }
        const size_type removed = found->second;
        m_storage.erase(m_storage.begin() + static_cast<std::ptrdiff_t>(removed));
        m_index.erase(found);
        for (auto& entry : m_index) {
            if (entry.second > removed) {
                --entry.second;
            }
        }
        return true;
    }

    // O(1): moves the last entry into the hole, so order changes.
    bool swapErase(const K& key) {
        auto found = m_index.find(key);
        if (found == m_index.end()) {
            return false;
        }
        const size_type removed = found->second;
        const size_type last = m_storage.size() - 1;
        if (removed != last) {
            m_storage[removed] = std::move(m_storage[last]);
            m_index[m_storage[removed].first] = removed;
        }
        m_storage.pop_back();
        m_index.erase(found);
        return true;
    }

    void clear() noexcept {
        m_storage.clear();
        m_index.clear();
    }

    iterator begin() noexcept { return m_storage.begin(); }
    iterator end() noexcept { return m_storage.end(); }
    const_iterator begin() const noexcept { return m_storage.begin(); }
    const_iterator end() const noexcept { return m_storage.end(); }
    const_iterator cbegin() const noexcept { return m_storage.cbegin(); }
    const_iterator cend() const noexcept { return m_storage.cend(); }

    value_type& entryAt(size_type index) noexcept { return m_storage[index]; }
    const value_type& entryAt(size_type index) const noexcept { return m_storage[index]; }

    const K& keyAt(size_type index) const noexcept { return m_storage[index].first; }

    V& valueAt(size_type index) noexcept { return m_storage[index].second; }
    const V& valueAt(size_type index) const noexcept { return m_storage[index].second; }

    void swap(IndexMap& other) noexcept {
        m_storage.swap(other.m_storage);
        m_index.swap(other.m_index);
    }

private:
    storage_type m_storage;
    std::unordered_map<K, size_type, Hash, Equal> m_index;
};

} // namespace containers
