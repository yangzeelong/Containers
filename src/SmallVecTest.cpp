#include <containers/collections/SmallVec.hpp>

#include <gtest/gtest.h>

#include <iterator>
#include <memory>
#include <string>
#include <utility>

TEST(SmallVecTest, StaysInlineWithinCapacity) {
    containers::SmallVec<int, 4> values;
    values.push_back(1);
    values.push_back(2);

    EXPECT_TRUE(values.isInline());
    EXPECT_EQ(2u, values.size());
    EXPECT_EQ(4u, values.capacity());
    EXPECT_FALSE(values.empty());
    EXPECT_EQ(1, values.front());
    EXPECT_EQ(2, values.back());
    EXPECT_EQ(1, values[0]);
}

TEST(SmallVecTest, SpillsToHeapAndKeepsValues) {
    containers::SmallVec<int, 2> values;
    for (int value = 0; value < 6; ++value) {
        values.push_back(value);
    }

    EXPECT_FALSE(values.isInline());
    ASSERT_EQ(6u, values.size());
    for (int index = 0; index < 6; ++index) {
        EXPECT_EQ(index, values[static_cast<std::size_t>(index)]);
    }
    EXPECT_GE(values.capacity(), 6u);
}

TEST(SmallVecTest, PopAndClear) {
    containers::SmallVec<int, 4> values;
    values.push_back(1);
    values.push_back(2);

    values.pop_back();
    EXPECT_EQ(1u, values.size());
    EXPECT_EQ(1, values.back());

    values.clear();
    EXPECT_TRUE(values.empty());
    EXPECT_TRUE(values.isInline());
}

TEST(SmallVecTest, CopyIsIndependent) {
    containers::SmallVec<int, 2> original;
    original.push_back(1);
    original.push_back(2);
    original.push_back(3);

    containers::SmallVec<int, 2> copy = original;
    copy.push_back(4);

    EXPECT_EQ(3u, original.size());
    EXPECT_EQ(4u, copy.size());
    EXPECT_EQ(3, original.back());
    EXPECT_EQ(4, copy.back());
}

TEST(SmallVecTest, MoveTransfersInlineValues) {
    containers::SmallVec<std::string, 2> original;
    original.push_back(std::string("x"));
    original.push_back(std::string("y"));

    containers::SmallVec<std::string, 2> moved = std::move(original);

    EXPECT_EQ(2u, moved.size());
    EXPECT_EQ("x", moved[0]);
    EXPECT_EQ(0u, original.size());
}

TEST(SmallVecTest, SupportsMoveOnlyTypes) {
    containers::SmallVec<std::unique_ptr<int>, 2> values;
    values.push_back(std::unique_ptr<int>(new int(1)));
    values.push_back(std::unique_ptr<int>(new int(2)));
    values.push_back(std::unique_ptr<int>(new int(3)));

    ASSERT_EQ(3u, values.size());
    EXPECT_EQ(1, *values[0]);
    EXPECT_EQ(3, *values[2]);
}

TEST(SmallVecTest, IteratesInOrder) {
    containers::SmallVec<int, 2> values;
    values.push_back(1);
    values.push_back(2);
    values.push_back(3);

    int sum = 0;
    for (int value : values) {
        sum += value;
    }

    EXPECT_EQ(6, sum);
    EXPECT_EQ(3, static_cast<int>(std::distance(values.begin(), values.end())));
}

TEST(SmallVecTest, ReservePromotesAheadOfPush) {
    containers::SmallVec<int, 2> values;
    values.reserve(8);

    EXPECT_FALSE(values.isInline());
    EXPECT_GE(values.capacity(), 8u);
    EXPECT_TRUE(values.empty());
}
