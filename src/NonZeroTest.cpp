#include <containers/NonZero.hpp>

#include <gtest/gtest.h>

#include <cstdint>

TEST(NonZeroTest, TryNewRejectsZero) {
    const auto value = containers::NonZeroU32::TryNew(0);

    EXPECT_FALSE(value.hasValue());
}

TEST(NonZeroTest, TryNewKeepsValue) {
    const auto value = containers::NonZeroU32::TryNew(7);

    ASSERT_TRUE(value.hasValue());
    EXPECT_EQ(7u, value.value().get());
    EXPECT_EQ(7u, static_cast<std::uint32_t>(value.value()));
}

TEST(NonZeroTest, ComparesByValue) {
    const containers::NonZeroI32 small(1);
    const containers::NonZeroI32 same(1);
    const containers::NonZeroI32 large(9);

    EXPECT_EQ(small, same);
    EXPECT_NE(small, large);
    EXPECT_TRUE(small < large);
}

TEST(NonZeroTest, WorksWithSizeAlias) {
    const auto value = containers::NonZeroSize::TryNew(42);

    ASSERT_TRUE(value.hasValue());
    EXPECT_EQ(42u, value.value().get());
}
