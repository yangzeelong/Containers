#pragma once

#include <cstdint>
#include <type_traits>

namespace containers {

// Rust's `bitflags`: a strong type over an enum's bit pattern, so flag sets are
// not interchangeable with bare integers. Enum values are expected to be single
// bits (1, 2, 4, ...); combining them is what BitFlags is for.
template <typename Enum>
class BitFlags {
    static_assert(std::is_enum<Enum>::value, "BitFlags requires an enum type");

public:
    using enum_type = Enum;
    using underlying_type = typename std::underlying_type<Enum>::type;

    BitFlags() noexcept : m_bits(0) {}

    explicit BitFlags(underlying_type bits) noexcept : m_bits(bits) {}

    static BitFlags Of(Enum flag) noexcept {
        return BitFlags(static_cast<underlying_type>(flag));
    }

    underlying_type bits() const noexcept { return m_bits; }
    bool empty() const noexcept { return m_bits == 0; }

    bool contains(Enum flag) const noexcept {
        const underlying_type mask = static_cast<underlying_type>(flag);
        return (m_bits & mask) == mask;
    }

    bool containsAll(BitFlags other) const noexcept {
        return (m_bits & other.m_bits) == other.m_bits;
    }

    bool containsAny(BitFlags other) const noexcept {
        return (m_bits & other.m_bits) != 0;
    }

    void insert(Enum flag) noexcept { m_bits |= static_cast<underlying_type>(flag); }
    void insert(BitFlags other) noexcept { m_bits |= other.m_bits; }
    void remove(Enum flag) noexcept { m_bits &= static_cast<underlying_type>(~static_cast<underlying_type>(flag)); }
    void clear() noexcept { m_bits = 0; }

    BitFlags& operator|=(BitFlags other) noexcept {
        m_bits |= other.m_bits;
        return *this;
    }

    BitFlags& operator&=(BitFlags other) noexcept {
        m_bits &= other.m_bits;
        return *this;
    }

    BitFlags& operator^=(BitFlags other) noexcept {
        m_bits ^= other.m_bits;
        return *this;
    }

    bool operator==(BitFlags other) const noexcept { return m_bits == other.m_bits; }
    bool operator!=(BitFlags other) const noexcept { return m_bits != other.m_bits; }

    BitFlags operator~() const noexcept { return BitFlags(static_cast<underlying_type>(~m_bits)); }

private:
    underlying_type m_bits;
};

template <typename Enum>
BitFlags<Enum> operator|(BitFlags<Enum> lhs, BitFlags<Enum> rhs) noexcept {
    lhs |= rhs;
    return lhs;
}

template <typename Enum>
BitFlags<Enum> operator&(BitFlags<Enum> lhs, BitFlags<Enum> rhs) noexcept {
    lhs &= rhs;
    return lhs;
}

template <typename Enum>
BitFlags<Enum> operator^(BitFlags<Enum> lhs, BitFlags<Enum> rhs) noexcept {
    lhs ^= rhs;
    return lhs;
}

// Builds a flag set from enum values: `Flags(Permission::Read, Permission::Write)`.
// A free `Enum | Enum` operator cannot be provided meaningfully: unqualified
// lookup in the enum's namespace would not find an operator declared here.
template <typename Enum, typename... Rest>
BitFlags<Enum> Flags(Enum first, Rest... rest) {
    BitFlags<Enum> flags = BitFlags<Enum>::Of(first);
    const int expand[] = {0, (flags.insert(rest), 0)...};
    static_cast<void>(expand);
    return flags;
}

} // namespace containers
