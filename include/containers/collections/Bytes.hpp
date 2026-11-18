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

    std::size_t size() const noexcept { return m_size; }
    bool empty() const noexcept { return m_size == 0; }

    const std::uint8_t* data() const noexcept {
        return m_storage ? m_storage->data() + m_offset : nullptr;
    }

    const std::uint8_t& operator[](std::size_t index) const {
        assert(index < m_size);
        return data()[index];
    }

    // Shares the underlying storage, like `Bytes::slice` in Rust.
    Bytes slice(std::size_t offset, std::size_t length) const {
        assert(offset <= m_size);
        assert(length <= m_size - offset);
        if (!m_storage) {
            return Bytes();
        }
        return Bytes(m_storage, m_offset + offset, length);
    }

    Bytes clone() const { return *this; }

    storage_type toVector() const {
        return m_storage ? storage_type(data(), data() + m_size) : storage_type();
    }

    std::string toString() const {
        return m_storage ? std::string(reinterpret_cast<const char*>(data()), m_size)
                        : std::string();
    }

private:
    friend class BytesMut;

    Bytes(std::shared_ptr<storage_type> storage, std::size_t offset, std::size_t size)
        : m_storage(std::move(storage)), m_offset(offset), m_size(size) {}

    std::shared_ptr<storage_type> m_storage;
    std::size_t m_offset = 0;
    std::size_t m_size = 0;
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

    explicit BytesMut(std::size_t capacity) { m_data.reserve(capacity); }

    static BytesMut WithCapacity(std::size_t capacity) { return BytesMut(capacity); }

    static BytesMut FromVector(storage_type data) {
        BytesMut buffer;
        buffer.m_data = std::move(data);
        return buffer;
    }

    std::size_t size() const noexcept { return m_data.size(); }
    bool empty() const noexcept { return m_data.empty(); }

    const std::uint8_t* data() const noexcept { return m_data.data(); }
    std::uint8_t* data() noexcept { return m_data.data(); }

    void resize(std::size_t size) { m_data.resize(size); }

    void clear() noexcept { m_data.clear(); }

    void append(const void* data, std::size_t size) {
        if (size == 0) {
            return;
        }
        const auto* begin = static_cast<const std::uint8_t*>(data);
        m_data.insert(m_data.end(), begin, begin + size);
    }

    void appendBytes(const Bytes& bytes) { append(bytes.data(), bytes.size()); }

    // Moves the buffer into a `Bytes`; this BytesMut becomes empty.
    Bytes freeze() {
        const std::size_t size = m_data.size();
        auto storage = std::make_shared<storage_type>(std::move(m_data));
        m_data.clear();
        return Bytes(storage, 0, size);
    }

    Bytes copyToBytes() const { return Bytes::Copy(m_data.data(), m_data.size()); }

private:
    storage_type m_data;
};

} // namespace containers
