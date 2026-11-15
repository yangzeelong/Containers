#include <containers/OnceCell.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

TEST(OnceCellTest, InitializesOnce) {
    containers::OnceCell<int> cell;

    EXPECT_FALSE(cell.isInit());
    EXPECT_EQ(nullptr, cell.get());

    int calls = 0;
    EXPECT_EQ(7, cell.getOrInit([&calls] {
        ++calls;
        return 7;
    }));
    EXPECT_EQ(7, cell.getOrInit([&calls] {
        ++calls;
        return 8;
    }));

    EXPECT_EQ(1, calls);
    EXPECT_TRUE(cell.isInit());
    EXPECT_EQ(7, cell.value());
}

TEST(OnceCellTest, TryInitReportsWinner) {
    containers::OnceCell<int> cell;

    EXPECT_TRUE(cell.tryInit([] { return 1; }));
    EXPECT_FALSE(cell.tryInit([] { return 2; }));
    EXPECT_EQ(1, cell.value());
}

TEST(OnceCellTest, ConcurrentInitRunsFactoryOnce) {
    containers::OnceCell<int> cell;
    std::atomic<int> calls(0);

    std::vector<std::thread> threads;
    for (int index = 0; index < 8; ++index) {
        threads.emplace_back([&cell, &calls] {
            cell.getOrInit([&calls] {
                ++calls;
                return 42;
            });
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(1, calls.load());
    EXPECT_EQ(42, cell.value());
}

TEST(LazyTest, BuildsOnFirstUse) {
    int calls = 0;
    containers::Lazy<std::string> lazy([&calls] {
        ++calls;
        return std::string("built");
    });

    EXPECT_FALSE(lazy.isInit());
    EXPECT_EQ("built", *lazy);
    EXPECT_EQ(std::string("built"), lazy->c_str());
    EXPECT_TRUE(lazy.isInit());
    EXPECT_EQ(1, calls);
}

TEST(LazyTest, ConcurrentFirstUseBuildsOnce) {
    std::atomic<int> calls(0);
    containers::Lazy<int> lazy([&calls] {
        ++calls;
        return 99;
    });

    std::vector<std::thread> threads;
    for (int index = 0; index < 8; ++index) {
        threads.emplace_back([&lazy] { EXPECT_EQ(99, lazy.get()); });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(1, calls.load());
}
