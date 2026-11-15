#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace containers {

class BytesMut;

// Rust's `bytes::Bytes`: an immutable, ref-counted byte buffer whose clones are
// pointer copies and whose slices share the same storage. The payload is only
// copied by `Copy`, `toVector` and `toString`; `BytesMut::freeze` hands a
// mutable buffer over to it without copying.
class Bytes {
public:
    using storage_type = std::vector<std::uint8_t>;

    Bytes() noexcept = default;

    static Bytes Copy(const void* data, std::size_t size) {
        if (size == 0) {
            return Bytes();
        }
        const auto* begin = static_cast<const std::uint8_t*>(data);
        return Bytes(std::make_shared<storage_type>(begin, begin + size), 0, size);
    }

    static Bytes Copy(const std::string& text) {
        return Copy(text.data(), text.size());
    }

    static Bytes FromVector(storage_type data) {
        const std::size_t size = data.size();
        return Bytes(std::make_shared<storage_type>(std::move(data)), 0, size);
    }

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    const std::uint8_t* data() const noexcept {
        return storage_ ? storage_->data() + offset_ : nullptr;
    }

    const std::uint8_t& operator[](std::size_t index) const {
        assert(index < size_);
        return data()[index];
    }

    // Shares the underlying storage, like `Bytes::slice` in Rust.
    Bytes slice(std::size_t offset, std::size_t length) const {
        assert(offset <= size_);
        assert(length <= size_ - offset);
        if (!storage_) {
            return Bytes();
        }
        return Bytes(storage_, offset_ + offset, length);
    }

    Bytes clone() const { return *this; }

    storage_type toVector() const {
        return storage_ ? storage_type(data(), data() + size_) : storage_type();
    }

    std::string toString() const {
        return storage_ ? std::string(reinterpret_cast<const char*>(data()), size_)
                        : std::string();
    }

private:
    friend class BytesMut;

    Bytes(std::shared_ptr<storage_type> storage, std::size_t offset, std::size_t size)
        : storage_(std::move(storage)), offset_(offset), size_(size) {}

    std::shared_ptr<storage_type> storage_;
    std::size_t offset_ = 0;
    std::size_t size_ = 0;
};

inline bool operator==(const Bytes& lhs, const Bytes& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    return lhs.size() == 0 || std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

inline bool operator!=(const Bytes& lhs, const Bytes& rhs) {
    return !(lhs == rhs);
}

// Rust's `bytes::BytesMut`: a growable mutable buffer that can be frozen into
// an immutable `Bytes` without copying the payload.
class BytesMut {
public:
    using storage_type = std::vector<std::uint8_t>;

    BytesMut() = default;

    explicit BytesMut(std::size_t capacity) { data_.reserve(capacity); }

    static BytesMut WithCapacity(std::size_t capacity) { return BytesMut(capacity); }

    static BytesMut FromVector(storage_type data) {
        BytesMut buffer;
        buffer.data_ = std::move(data);
        return buffer;
    }

    std::size_t size() const noexcept { return data_.size(); }
    bool empty() const noexcept { return data_.empty(); }

    const std::uint8_t* data() const noexcept { return data_.data(); }
    std::uint8_t* data() noexcept { return data_.data(); }

    void resize(std::size_t size) { data_.resize(size); }

    void clear() noexcept { data_.clear(); }

    void append(const void* data, std::size_t size) {
        if (size == 0) {
            return;
        }
        const auto* begin = static_cast<const std::uint8_t*>(data);
        data_.insert(data_.end(), begin, begin + size);
    }

    void appendBytes(const Bytes& bytes) { append(bytes.data(), bytes.size()); }

    // Moves the buffer into a `Bytes`; this BytesMut becomes empty.
    Bytes freeze() {
        const std::size_t size = data_.size();
        auto storage = std::make_shared<storage_type>(std::move(data_));
        data_.clear();
        return Bytes(storage, 0, size);
    }

    Bytes copyToBytes() const { return Bytes::Copy(data_.data(), data_.size()); }

private:
    storage_type data_;
};

} // namespace containers
