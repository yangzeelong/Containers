#include <containers/Unit.hpp>

#include <gtest/gtest.h>

#include <set>

TEST(UnitTest, ComparesEqual) {
    containers::Unit a;
    containers::Unit b;

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_FALSE(a < b);
    EXPECT_FALSE(a > b);
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a >= b);
}

TEST(UnitTest, IsUsableAsMapValue) {
    std::set<containers::Unit> units;
    units.insert(containers::Unit{});
    units.insert(containers::Unit{});

    EXPECT_EQ(1u, units.size());
}
