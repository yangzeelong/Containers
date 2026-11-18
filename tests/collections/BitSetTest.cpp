#include "containers/collections/BitSet.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

using containers::BitSet;

TEST(BitSetTest, DefaultIsEmpty) {
    BitSet bits;
    EXPECT_TRUE(bits.empty());
    EXPECT_EQ(0u, bits.size());
    EXPECT_EQ(0u, bits.wordCount());
    EXPECT_EQ(0u, bits.count());
    EXPECT_TRUE(bits.none());
    EXPECT_TRUE(bits.all());
}

TEST(BitSetTest, InsertAndContains) {
    BitSet bits;
    EXPECT_TRUE(bits.insert(0));
    EXPECT_TRUE(bits.insert(3));
    EXPECT_TRUE(bits.insert(130));
    EXPECT_EQ(131u, bits.size());
    EXPECT_TRUE(bits.contains(0));
    EXPECT_TRUE(bits.contains(3));
    EXPECT_TRUE(bits.contains(130));
    EXPECT_FALSE(bits.contains(1));
    EXPECT_FALSE(bits.contains(200));
    EXPECT_EQ(3u, bits.count());
}

TEST(BitSetTest, InsertReportsWhetherBitWasNew) {
    BitSet bits;
    EXPECT_TRUE(bits.insert(7));
    EXPECT_FALSE(bits.insert(7));
    EXPECT_EQ(1u, bits.count());
}

TEST(BitSetTest, RemoveAndSet) {
    BitSet bits(20);
    bits.set(5, true);
    bits.set(19, true);
    EXPECT_TRUE(bits.contains(5));
    EXPECT_TRUE(bits.contains(19));
    EXPECT_TRUE(bits.remove(5));
    EXPECT_FALSE(bits.remove(5));
    EXPECT_FALSE(bits.remove(100));
    bits.set(19, false);
    EXPECT_FALSE(bits.contains(9));
    EXPECT_TRUE(bits.none());
}

TEST(BitSetTest, CountAnyNone) {
    BitSet bits(10);
    EXPECT_TRUE(bits.none());
    EXPECT_FALSE(bits.any());
    bits.insert(0);
    bits.insert(9);
    EXPECT_EQ(2u, bits.count());
    EXPECT_TRUE(bits.any());
    EXPECT_FALSE(bits.none());
}

TEST(BitSetTest, ResizeGrowAndShrink) {
    BitSet bits;
    bits.resize(100);
    EXPECT_EQ(100u, bits.size());
    EXPECT_EQ(2u, bits.wordCount());
    bits.insert(90);
    EXPECT_TRUE(bits.contains(90));

    bits.resize(50);
    EXPECT_EQ(50u, bits.size());
    EXPECT_EQ(1u, bits.wordCount());
    EXPECT_FALSE(bits.contains(90));

    bits.resize(80, true);
    EXPECT_EQ(80u, bits.size());
    EXPECT_EQ(30u, bits.count());
    EXPECT_FALSE(bits.all());
    for (std::size_t index = 0; index < 50; ++index) {
        bits.insert(index);
    }
    EXPECT_EQ(80u, bits.count());
    EXPECT_TRUE(bits.all());
}

TEST(BitSetTest, FlipAndFlipAll) {
    BitSet bits(10);
    bits.flip(0);
    EXPECT_TRUE(bits.contains(0));
    bits.flip(0);
    EXPECT_FALSE(bits.contains(0));
    bits.flip(12);
    EXPECT_EQ(13u, bits.size());

    BitSet all(10, true);
    EXPECT_EQ(10u, all.count());
    all.flipAll();
    EXPECT_EQ(0u, all.count());
    EXPECT_EQ(10u, all.size());
}

TEST(BitSetTest, BitwiseOperators) {
    BitSet left;
    left.insert(0);
    left.insert(1);
    left.insert(70);

    BitSet right;
    right.insert(1);
    right.insert(2);
    right.insert(70);

    BitSet anded = left & right;
    EXPECT_TRUE(anded.contains(1));
    EXPECT_TRUE(anded.contains(70));
    EXPECT_FALSE(anded.contains(0));
    EXPECT_FALSE(anded.contains(2));
    EXPECT_EQ(2u, anded.count());

    BitSet ored = left | right;
    EXPECT_EQ(4u, ored.count());
    EXPECT_TRUE(ored.contains(0));
    EXPECT_TRUE(ored.contains(2));

    BitSet xored = left ^ right;
    EXPECT_EQ(2u, xored.count());
    EXPECT_TRUE(xored.contains(0));
    EXPECT_TRUE(xored.contains(2));

    BitSet inverted = ~left;
    EXPECT_EQ(left.size(), inverted.size());
    EXPECT_FALSE(inverted.contains(0));
    EXPECT_TRUE(inverted.contains(2));
    EXPECT_EQ(left.size() - left.count(), inverted.count());
}

TEST(BitSetTest, EqualityAndCopy) {
    BitSet first;
    first.insert(1);
    BitSet second;
    second.insert(1);
    EXPECT_TRUE(first == second);
    EXPECT_FALSE(first != second);

    BitSet third = first;
    EXPECT_TRUE(third == first);
    third.insert(2);
    EXPECT_FALSE(third == first);
    EXPECT_FALSE(first.contains(2));
}

TEST(BitSetTest, ClearResetsEverything) {
    BitSet bits(40, true);
    EXPECT_EQ(40u, bits.count());
    bits.clear();
    EXPECT_TRUE(bits.empty());
    EXPECT_EQ(0u, bits.size());
    EXPECT_EQ(0u, bits.count());
}

TEST(BitSetTest, NextSetBitWalksSetBits) {
    BitSet bits(200);
    bits.insert(0);
    bits.insert(5);
    bits.insert(64);
    bits.insert(65);
    bits.insert(199);

    std::vector<std::size_t> seen;
    for (std::size_t index = bits.nextSetBit(); index < bits.size();
         index = bits.nextSetBit(index + 1)) {
        seen.push_back(index);
    }
    const std::vector<std::size_t> expected = {0, 5, 64, 65, 199};
    EXPECT_EQ(expected, seen);
    EXPECT_EQ(bits.size(), bits.nextSetBit(200));
    EXPECT_EQ(64u, bits.nextSetBit(6));
}

TEST(BitSetTest, AllAndTrailingBits) {
    BitSet bits(70, true);
    EXPECT_TRUE(bits.all());
    EXPECT_EQ(70u, bits.count());
    bits.remove(69);
    EXPECT_FALSE(bits.all());
    EXPECT_EQ(69u, bits.count());

    BitSet empty;
    EXPECT_TRUE(empty.all());
    empty.resize(1);
    EXPECT_FALSE(empty.all());
}

TEST(BitSetTest, WordsExposeStorageState) {
    BitSet bits(65);
    bits.insert(64);
    EXPECT_EQ(2u, bits.wordCount());
    EXPECT_EQ(2u, bits.words().size());
    EXPECT_EQ(1u, bits.words()[1]);

    bits.resize(64);
    EXPECT_EQ(1u, bits.wordCount());
    EXPECT_FALSE(bits.contains(64));
}
