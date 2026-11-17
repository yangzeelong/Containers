#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <ostream>
#include <string>

namespace containers {

// C++14 stand-in for std::string_view: a non-owning, copyable view over a
// contiguous character buffer. It never allocates and never copies characters;
// the caller must keep the underlying buffer alive while the view is used.
class StringView {
public:
    using value_type = char;
    using size_type = std::size_t;

    static constexpr size_type npos = static_cast<size_type>(-1);

    constexpr StringView() noexcept : data_(nullptr), size_(0) {}

    constexpr StringView(const char* data, size_type size) noexcept
        : data_(data), size_(size) {}

    StringView(const char* text)
        : data_(text), size_(text == nullptr ? 0 : std::strlen(text)) {}

    StringView(const std::string& text) noexcept
        : data_(text.data()), size_(text.size()) {}

    template <size_type N>
    constexpr StringView(const char (&text)[N]) noexcept
        : data_(text), size_(N > 0 && text[N - 1] == '\0' ? N - 1 : N) {}

    constexpr const char* data() const noexcept { return data_; }
    constexpr size_type size() const noexcept { return size_; }
    constexpr size_type length() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }

    constexpr const char* begin() const noexcept { return data_; }
    constexpr const char* end() const noexcept { return data_ + size_; }

    constexpr char operator[](size_type index) const noexcept { return data_[index]; }

    char front() const {
        assert(!empty());
        return data_[0];
    }

    char back() const {
        assert(!empty());
        return data_[size_ - 1];
    }

    void removePrefix(size_type count) noexcept {
        assert(count <= size_);
        data_ += count;
        size_ -= count;
    }

    void removeSuffix(size_type count) noexcept {
        assert(count <= size_);
        size_ -= count;
    }

    StringView substr(size_type pos = 0, size_type count = npos) const {
        if (pos >= size_) {
            return StringView();
        }
        const size_type available = size_ - pos;
        const size_type length = count < available ? count : available;
        return length == 0 ? StringView() : StringView(data_ + pos, length);
    }

    int compare(StringView other) const {
        const size_type common = size_ < other.size_ ? size_ : other.size_;
        if (common != 0) {
            const int result = std::memcmp(data_, other.data_, common);
            if (result != 0) {
                return result < 0 ? -1 : 1;
            }
        }
        if (size_ == other.size_) {
            return 0;
        }
        return size_ < other.size_ ? -1 : 1;
    }

    bool startsWith(StringView prefix) const {
        return prefix.size_ <= size_ &&
               (prefix.size_ == 0 || std::memcmp(data_, prefix.data_, prefix.size_) == 0);
    }

    bool startsWith(char prefix) const { return !empty() && data_[0] == prefix; }

    bool endsWith(StringView suffix) const {
        return suffix.size_ <= size_ &&
               (suffix.size_ == 0 ||
                std::memcmp(data_ + size_ - suffix.size_, suffix.data_, suffix.size_) == 0);
    }

    bool endsWith(char suffix) const { return !empty() && data_[size_ - 1] == suffix; }

    size_type find(char needle, size_type pos = 0) const {
        if (pos >= size_) {
            return npos;
        }
        const void* hit =
            std::memchr(data_ + pos, static_cast<unsigned char>(needle), size_ - pos);
        return hit == nullptr ? npos
                              : static_cast<size_type>(static_cast<const char*>(hit) - data_);
    }

    size_type find(StringView needle, size_type pos = 0) const {
        if (needle.empty()) {
            return pos <= size_ ? pos : npos;
        }
        if (pos >= size_ || needle.size_ > size_ - pos) {
            return npos;
        }
        for (size_type index = pos; index + needle.size_ <= size_; ++index) {
            if (std::memcmp(data_ + index, needle.data_, needle.size_) == 0) {
                return index;
            }
        }
        return npos;
    }

    size_type rfind(char needle, size_type pos = npos) const {
        if (empty()) {
            return npos;
        }
        size_type index = pos >= size_ ? size_ - 1 : pos;
        while (true) {
            if (data_[index] == needle) {
                return index;
            }
            if (index == 0) {
                break;
            }
            --index;
        }
        return npos;
    }

    size_type rfind(StringView needle, size_type pos = npos) const {
        if (needle.empty()) {
            return pos >= size_ ? size_ : pos;
        }
        if (needle.size_ > size_) {
            return npos;
        }
        const size_type last = size_ - needle.size_;
        size_type index = pos >= last ? last : pos;
        while (true) {
            if (std::memcmp(data_ + index, needle.data_, needle.size_) == 0) {
                return index;
            }
            if (index == 0) {
                break;
            }
            --index;
        }
        return npos;
    }

    bool contains(StringView needle) const { return find(needle) != npos; }

    std::string toString() const {
        return empty() ? std::string() : std::string(data_, size_);
    }

private:
    const char* data_;
    size_type size_;
};

inline bool operator==(StringView lhs, StringView rhs) {
    return lhs.size() == rhs.size() &&
           (lhs.size() == 0 || std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0);
}

inline bool operator!=(StringView lhs, StringView rhs) { return !(lhs == rhs); }

inline bool operator<(StringView lhs, StringView rhs) { return lhs.compare(rhs) < 0; }

inline bool operator<=(StringView lhs, StringView rhs) { return lhs.compare(rhs) <= 0; }

inline bool operator>(StringView lhs, StringView rhs) { return lhs.compare(rhs) > 0; }

inline bool operator>=(StringView lhs, StringView rhs) { return lhs.compare(rhs) >= 0; }

inline std::ostream& operator<<(std::ostream& stream, StringView view) {
    if (!view.empty()) {
        stream.write(view.data(), static_cast<std::streamsize>(view.size()));
    }
    return stream;
}

// FNV-1a, so hashing is deterministic across runs and platforms.
struct StringViewHash {
    std::size_t operator()(StringView view) const noexcept {
        std::uint64_t hash = 14695981039346656037ull;
        for (std::size_t index = 0; index < view.size(); ++index) {
            hash ^= static_cast<unsigned char>(view[index]);
            hash *= 1099511628211ull;
        }
        return static_cast<std::size_t>(hash);
    }
};

inline namespace literals {
inline namespace string_view_literals {

constexpr StringView operator"" _sv(const char* data, std::size_t size) noexcept {
    return StringView(data, size);
}

}  // namespace string_view_literals
}  // namespace literals

}  // namespace containers

namespace std {

template <>
struct hash<containers::StringView> {
    std::size_t operator()(containers::StringView view) const noexcept {
        return containers::StringViewHash()(view);
    }
};

}  // namespace std
