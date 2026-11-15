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

    size_type size() const noexcept { return storage_.size(); }
    bool empty() const noexcept { return storage_.empty(); }

    void reserve(size_type capacity) {
        storage_.reserve(capacity);
        index_.reserve(capacity);
    }

    // Returns true when the key was new, false when an existing value was
    // replaced (the original position is kept, like IndexMap::insert).
    bool insert(K key, V value) {
        auto found = index_.find(key);
        if (found != index_.end()) {
            storage_[found->second].second = std::move(value);
            return false;
        }
        storage_.emplace_back(std::move(key), std::move(value));
        index_.emplace(storage_.back().first, storage_.size() - 1);
        return true;
    }

    V* find(const K& key) {
        auto found = index_.find(key);
        if (found == index_.end()) {
            return nullptr;
        }
        return &storage_[found->second].second;
    }

    const V* find(const K& key) const {
        auto found = index_.find(key);
        if (found == index_.end()) {
            return nullptr;
        }
        return &storage_[found->second].second;
    }

    bool contains(const K& key) const { return index_.find(key) != index_.end(); }

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
        auto found = index_.find(key);
        if (found != index_.end()) {
            return storage_[found->second].second;
        }
        storage_.emplace_back(key, V());
        index_.emplace(storage_.back().first, storage_.size() - 1);
        return storage_.back().second;
    }

    // O(n): removes the entry and shifts later entries down, preserving order.
    bool erase(const K& key) {
        auto found = index_.find(key);
        if (found == index_.end()) {
            return false;
        }
        const size_type removed = found->second;
        storage_.erase(storage_.begin() + static_cast<std::ptrdiff_t>(removed));
        index_.erase(found);
        for (auto& entry : index_) {
            if (entry.second > removed) {
                --entry.second;
            }
        }
        return true;
    }

    // O(1): moves the last entry into the hole, so order changes.
    bool swapErase(const K& key) {
        auto found = index_.find(key);
        if (found == index_.end()) {
            return false;
        }
        const size_type removed = found->second;
        const size_type last = storage_.size() - 1;
        if (removed != last) {
            storage_[removed] = std::move(storage_[last]);
            index_[storage_[removed].first] = removed;
        }
        storage_.pop_back();
        index_.erase(found);
        return true;
    }

    void clear() noexcept {
        storage_.clear();
        index_.clear();
    }

    iterator begin() noexcept { return storage_.begin(); }
    iterator end() noexcept { return storage_.end(); }
    const_iterator begin() const noexcept { return storage_.begin(); }
    const_iterator end() const noexcept { return storage_.end(); }
    const_iterator cbegin() const noexcept { return storage_.cbegin(); }
    const_iterator cend() const noexcept { return storage_.cend(); }

    value_type& entryAt(size_type index) noexcept { return storage_[index]; }
    const value_type& entryAt(size_type index) const noexcept { return storage_[index]; }

    const K& keyAt(size_type index) const noexcept { return storage_[index].first; }

    V& valueAt(size_type index) noexcept { return storage_[index].second; }
    const V& valueAt(size_type index) const noexcept { return storage_[index].second; }

    void swap(IndexMap& other) noexcept {
        storage_.swap(other.storage_);
        index_.swap(other.index_);
    }

private:
    storage_type storage_;
    std::unordered_map<K, size_type, Hash, Equal> index_;
};

} // namespace containers
