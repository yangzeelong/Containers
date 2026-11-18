#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace containers {

// Growable bit vector backed by 64-bit words. Bits are addressed by index and
// indices beyond size() grow the set automatically, the same contract as the
// Rust bit-set crate. Bits beyond size() in the last word are always zero.
class BitSet {
public:
    using size_type = std::size_t;
    using word_type = std::uint64_t;

    enum : size_type { kBitsPerWord = 64 };

    BitSet() noexcept = default;

    explicit BitSet(size_type size, bool value = false) { resize(size, value); }

    size_type size() const noexcept { return m_size; }
    bool empty() const noexcept { return m_size == 0; }
    size_type wordCount() const noexcept { return m_words.size(); }
    const std::vector<word_type>& words() const noexcept { return m_words; }

    void resize(size_type size, bool value = false) {
        const size_type oldSize = m_size;
        const size_type wordCount = WordCount(size);
        m_words.resize(wordCount, value ? ~word_type(0) : word_type(0));
        if (value && size > oldSize) {
            for (size_type bit = oldSize; bit < size; ++bit) {
                m_words[bit / kBitsPerWord] |= Mask(bit);
            }
        }
        m_size = size;
        MaskLastWord();
    }

    void clear() noexcept {
        m_words.clear();
        m_size = 0;
    }

    bool insert(size_type index) {
        if (index >= m_size) {
            resize(index + 1, false);
        }
        word_type& word = m_words[index / kBitsPerWord];
        const word_type mask = Mask(index);
        const bool wasSet = (word & mask) != 0;
        word |= mask;
        return !wasSet;
    }

    bool remove(size_type index) {
        if (index >= m_size) {
            return false;
        }
        word_type& word = m_words[index / kBitsPerWord];
        const word_type mask = Mask(index);
        const bool wasSet = (word & mask) != 0;
        word &= ~mask;
        return wasSet;
    }

    bool contains(size_type index) const {
        return index < m_size && (m_words[index / kBitsPerWord] & Mask(index)) != 0;
    }

    void set(size_type index, bool value) {
        if (value) {
            insert(index);
        } else {
            remove(index);
        }
    }

    void flip(size_type index) {
        if (index >= m_size) {
            resize(index + 1, false);
        }
        m_words[index / kBitsPerWord] ^= Mask(index);
    }

    void flipAll() noexcept {
        for (word_type& word : m_words) {
            word = ~word;
        }
        MaskLastWord();
    }

    size_type count() const noexcept {
        size_type total = 0;
        for (word_type word : m_words) {
            total += PopCount(word);
        }
        return total;
    }

    bool any() const noexcept { return count() != 0; }

    bool none() const noexcept { return count() == 0; }

    bool all() const noexcept {
        if (m_size == 0) {
            return true;
        }
        const size_type last = m_words.size() - 1;
        for (size_type index = 0; index < m_words.size(); ++index) {
            const word_type expected =
                index == last ? MaskedLastWord() : ~word_type(0);
            if (m_words[index] != expected) {
                return false;
            }
        }
        return true;
    }

    // Returns the smallest set bit index >= from, or size() if there is none.
    size_type nextSetBit(size_type from = 0) const {
        if (from >= m_size) {
            return m_size;
        }
        size_type wordIndex = from / kBitsPerWord;
        word_type word = m_words[wordIndex] & (~word_type(0) << (from % kBitsPerWord));
        while (true) {
            if (word != 0) {
                size_type bit = 0;
                while ((word & 1) == 0) {
                    word >>= 1;
                    ++bit;
                }
                return wordIndex * kBitsPerWord + bit;
            }
            ++wordIndex;
            if (wordIndex >= m_words.size()) {
                return m_size;
            }
            word = m_words[wordIndex];
        }
    }

    BitSet& operator&=(const BitSet& other) {
        const size_type common = m_words.size() < other.m_words.size()
                                     ? m_words.size()
                                     : other.m_words.size();
        for (size_type index = 0; index < common; ++index) {
            m_words[index] &= other.m_words[index];
        }
        for (size_type index = common; index < m_words.size(); ++index) {
            m_words[index] = 0;
        }
        MaskLastWord();
        return *this;
    }

    BitSet& operator|=(const BitSet& other) {
        if (other.m_size > m_size) {
            resize(other.m_size, false);
        }
        const size_type common = m_words.size() < other.m_words.size()
                                     ? m_words.size()
                                     : other.m_words.size();
        for (size_type index = 0; index < common; ++index) {
            m_words[index] |= other.m_words[index];
        }
        MaskLastWord();
        return *this;
    }

    BitSet& operator^=(const BitSet& other) {
        if (other.m_size > m_size) {
            resize(other.m_size, false);
        }
        const size_type common = m_words.size() < other.m_words.size()
                                     ? m_words.size()
                                     : other.m_words.size();
        for (size_type index = 0; index < common; ++index) {
            m_words[index] ^= other.m_words[index];
        }
        MaskLastWord();
        return *this;
    }

    void swap(BitSet& other) noexcept {
        m_words.swap(other.m_words);
        const size_type size = m_size;
        m_size = other.m_size;
        other.m_size = size;
    }

    friend bool operator==(const BitSet& lhs, const BitSet& rhs) {
        return lhs.m_size == rhs.m_size && lhs.m_words == rhs.m_words;
    }

    friend bool operator!=(const BitSet& lhs, const BitSet& rhs) {
        return !(lhs == rhs);
    }

private:
    static size_type WordCount(size_type bits) noexcept {
        return (bits + kBitsPerWord - 1) / kBitsPerWord;
    }

    static word_type Mask(size_type index) noexcept {
        return word_type(1) << (index % kBitsPerWord);
    }

    static size_type PopCount(word_type word) noexcept {
        // SWAR popcount, portable to every C++14 compiler.
        word = word - ((word >> 1) & 0x5555555555555555ull);
        word = (word & 0x3333333333333333ull) +
               ((word >> 2) & 0x3333333333333333ull);
        word = (word + (word >> 4)) & 0x0f0f0f0f0f0f0f0full;
        return static_cast<size_type>((word * 0x0101010101010101ull) >> 56);
    }

    static word_type LastWordMask(size_type size) noexcept {
        const size_type remainder = size % kBitsPerWord;
        return remainder == 0 ? ~word_type(0) : (word_type(1) << remainder) - 1;
    }

    word_type MaskedLastWord() const noexcept {
        return m_words.empty() ? word_type(0) : LastWordMask(m_size);
    }

    void MaskLastWord() noexcept {
        if (!m_words.empty()) {
            m_words.back() &= LastWordMask(m_size);
        }
    }

    std::vector<word_type> m_words;
    size_type m_size = 0;
};

inline BitSet operator&(BitSet lhs, const BitSet& rhs) {
    lhs &= rhs;
    return lhs;
}

inline BitSet operator|(BitSet lhs, const BitSet& rhs) {
    lhs |= rhs;
    return lhs;
}

inline BitSet operator^(BitSet lhs, const BitSet& rhs) {
    lhs ^= rhs;
    return lhs;
}

inline BitSet operator~(BitSet value) {
    value.flipAll();
    return value;
}

}  // namespace containers
