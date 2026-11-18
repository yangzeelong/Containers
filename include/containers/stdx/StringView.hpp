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

    constexpr StringView() noexcept : m_data(nullptr), m_size(0) {}

    constexpr StringView(const char* data, size_type size) noexcept
        : m_data(data), m_size(size) {}

    StringView(const char* text)
        : m_data(text), m_size(text == nullptr ? 0 : std::strlen(text)) {}

    StringView(const std::string& text) noexcept
        : m_data(text.data()), m_size(text.size()) {}

    template <size_type N>
    constexpr StringView(const char (&text)[N]) noexcept
        : m_data(text), m_size(N > 0 && text[N - 1] == '\0' ? N - 1 : N) {}

    constexpr const char* data() const noexcept { return m_data; }
    constexpr size_type size() const noexcept { return m_size; }
    constexpr size_type length() const noexcept { return m_size; }
    constexpr bool empty() const noexcept { return m_size == 0; }

    constexpr const char* begin() const noexcept { return m_data; }
    constexpr const char* end() const noexcept { return m_data + m_size; }

    constexpr char operator[](size_type index) const noexcept { return m_data[index]; }

    char front() const {
        assert(!empty());
        return m_data[0];
    }

    char back() const {
        assert(!empty());
        return m_data[m_size - 1];
    }

    void removePrefix(size_type count) noexcept {
        assert(count <= m_size);
        m_data += count;
        m_size -= count;
    }

    void removeSuffix(size_type count) noexcept {
        assert(count <= m_size);
        m_size -= count;
    }

    StringView substr(size_type pos = 0, size_type count = npos) const {
        if (pos >= m_size) {
            return StringView();
        }
        const size_type available = m_size - pos;
        const size_type length = count < available ? count : available;
        return length == 0 ? StringView() : StringView(m_data + pos, length);
    }

    int compare(StringView other) const {
        const size_type common = m_size < other.m_size ? m_size : other.m_size;
        if (common != 0) {
            const int result = std::memcmp(m_data, other.m_data, common);
            if (result != 0) {
                return result < 0 ? -1 : 1;
            }
        }
        if (m_size == other.m_size) {
            return 0;
        }
        return m_size < other.m_size ? -1 : 1;
    }

    bool startsWith(StringView prefix) const {
        return prefix.m_size <= m_size &&
               (prefix.m_size == 0 || std::memcmp(m_data, prefix.m_data, prefix.m_size) == 0);
    }

    bool startsWith(char prefix) const { return !empty() && m_data[0] == prefix; }

    bool endsWith(StringView suffix) const {
        return suffix.m_size <= m_size &&
               (suffix.m_size == 0 ||
                std::memcmp(m_data + m_size - suffix.m_size, suffix.m_data, suffix.m_size) == 0);
    }

    bool endsWith(char suffix) const { return !empty() && m_data[m_size - 1] == suffix; }

    size_type find(char needle, size_type pos = 0) const {
        if (pos >= m_size) {
            return npos;
        }
        const void* hit =
            std::memchr(m_data + pos, static_cast<unsigned char>(needle), m_size - pos);
        return hit == nullptr ? npos
                              : static_cast<size_type>(static_cast<const char*>(hit) - m_data);
    }

    size_type find(StringView needle, size_type pos = 0) const {
        if (needle.empty()) {
            return pos <= m_size ? pos : npos;
        }
        if (pos >= m_size || needle.m_size > m_size - pos) {
            return npos;
        }
        for (size_type index = pos; index + needle.m_size <= m_size; ++index) {
            if (std::memcmp(m_data + index, needle.m_data, needle.m_size) == 0) {
                return index;
            }
        }
        return npos;
    }

    size_type rfind(char needle, size_type pos = npos) const {
        if (empty()) {
            return npos;
        }
        size_type index = pos >= m_size ? m_size - 1 : pos;
        while (true) {
            if (m_data[index] == needle) {
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
            return pos >= m_size ? m_size : pos;
        }
        if (needle.m_size > m_size) {
            return npos;
        }
        const size_type last = m_size - needle.m_size;
        size_type index = pos >= last ? last : pos;
        while (true) {
            if (std::memcmp(m_data + index, needle.m_data, needle.m_size) == 0) {
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
        return empty() ? std::string() : std::string(m_data, m_size);
    }

private:
    const char* m_data;
    size_type m_size;
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
