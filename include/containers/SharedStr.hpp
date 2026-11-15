#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

namespace containers {

// Rust's `Arc<str>`: an immutable ref-counted string. Clones and slices are
// pointer copies; slicing shares the underlying buffer and never copies the
// characters. Comparisons and hashing look through the view.
class SharedStr {
public:
    using storage_type = std::string;

    SharedStr() noexcept = default;

    static SharedStr Copy(const std::string& text) {
        return Copy(text.data(), text.size());
    }

    static SharedStr Copy(const char* data, std::size_t size) {
        if (size == 0) {
            return SharedStr();
        }
        return SharedStr(std::make_shared<const storage_type>(data, size), 0, size);
    }

    static SharedStr Owned(storage_type text) {
        const std::size_t size = text.size();
        return SharedStr(std::make_shared<const storage_type>(std::move(text)), 0, size);
    }

    static SharedStr From(const char* text) {
        return Copy(text, std::strlen(text));
    }

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    const char* data() const noexcept {
        return storage_ ? storage_->data() + offset_ : nullptr;
    }

    std::string toString() const {
        return storage_ ? std::string(data(), size_) : std::string();
    }

    SharedStr slice(std::size_t offset, std::size_t length) const {
        if (!storage_ || length == 0) {
            return SharedStr();
        }
        assert(offset + length <= size_);
        return SharedStr(storage_, offset_ + offset, length);
    }

    SharedStr clone() const { return *this; }

private:
    SharedStr(std::shared_ptr<const storage_type> storage, std::size_t offset, std::size_t size)
        : storage_(std::move(storage)), offset_(offset), size_(size) {}

    std::shared_ptr<const storage_type> storage_;
    std::size_t offset_ = 0;
    std::size_t size_ = 0;
};

inline bool operator==(const SharedStr& lhs, const SharedStr& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    return lhs.size() == 0 || std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

inline bool operator!=(const SharedStr& lhs, const SharedStr& rhs) {
    return !(lhs == rhs);
}

inline bool operator==(const SharedStr& lhs, const std::string& rhs) {
    return lhs.size() == rhs.size() &&
           (lhs.size() == 0 || std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0);
}

inline bool operator==(const std::string& lhs, const SharedStr& rhs) {
    return rhs == lhs;
}

inline bool operator!=(const SharedStr& lhs, const std::string& rhs) {
    return !(lhs == rhs);
}

inline bool operator!=(const std::string& lhs, const SharedStr& rhs) {
    return !(rhs == lhs);
}

// Hashes the visible characters (FNV-1a), so slices hash like their contents.
struct SharedStrHash {
    std::size_t operator()(const SharedStr& value) const noexcept {
        std::size_t hash = 1469598103934665603ull;
        const char* data = value.data();
        for (std::size_t index = 0; index < value.size(); ++index) {
            hash ^= static_cast<unsigned char>(data[index]);
            hash *= 1099511628211ull;
        }
        return hash;
    }
};

} // namespace containers
