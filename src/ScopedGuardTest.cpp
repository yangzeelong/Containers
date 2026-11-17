#include <containers/stdx/ScopedGuard.hpp>

#include <gtest/gtest.h>

#include <utility>

TEST(ScopedGuardTest, RunsOnScopeExit) {
    int calls = 0;
    {
        auto guard = containers::OnScopeExit([&calls] { ++calls; });
        EXPECT_EQ(0, calls);
        EXPECT_TRUE(guard.active());
    }

    EXPECT_EQ(1, calls);
}

TEST(ScopedGuardTest, DismissPreventsCall) {
    int calls = 0;
    {
        auto guard = containers::OnScopeExit([&calls] { ++calls; });
        guard.dismiss();
        EXPECT_FALSE(guard.active());
    }

    EXPECT_EQ(0, calls);
}

TEST(ScopedGuardTest, MoveDisarmsSource) {
    int calls = 0;
    {
        auto moved = containers::OnScopeExit([&calls] { ++calls; });
        {
            auto receiver = std::move(moved);
            EXPECT_FALSE(moved.active());
            EXPECT_TRUE(receiver.active());
        }
        EXPECT_EQ(1, calls);
    }

    EXPECT_EQ(1, calls);
}

TEST(ScopedGuardTest, NestedScopesRunInReverseOrder) {
    int calls = 0;
    {
        auto outer = containers::OnScopeExit([&calls] { calls = calls * 10 + 1; });
        {
            auto inner = containers::OnScopeExit([&calls] { calls = calls * 10 + 2; });
            (void)inner;
        }
        EXPECT_EQ(2, calls);
    }

    EXPECT_EQ(21, calls);
}
