#include <containers/AtomicSharedPtr.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <thread>
#include <vector>

TEST(AtomicSharedPtrTest, LoadStoreRoundTrip) {
    containers::AtomicSharedPtr<int> atom;

    EXPECT_EQ(nullptr, atom.load());

    atom.store(std::make_shared<int>(7));

    ASSERT_NE(nullptr, atom.load());
    EXPECT_EQ(7, *atom.load());
}

TEST(AtomicSharedPtrTest, ExchangeReturnsPrevious) {
    containers::AtomicSharedPtr<int> atom(std::make_shared<int>(1));

    auto previous = atom.exchange(std::make_shared<int>(2));

    ASSERT_NE(nullptr, previous);
    EXPECT_EQ(1, *previous);
    EXPECT_EQ(2, *atom.load());
}

TEST(AtomicSharedPtrTest, CompareExchangeSucceedsOnMatch) {
    containers::AtomicSharedPtr<int> atom(std::make_shared<int>(3));

    auto expected = atom.load();
    const bool swapped = atom.compareExchange(expected, std::make_shared<int>(4));

    EXPECT_TRUE(swapped);
    EXPECT_EQ(4, *atom.load());
}

TEST(AtomicSharedPtrTest, CompareExchangeUpdatesExpectedOnMismatch) {
    containers::AtomicSharedPtr<int> atom(std::make_shared<int>(5));
    auto expected = std::make_shared<int>(999);

    const bool swapped = atom.compareExchange(expected, std::make_shared<int>(6));

    EXPECT_FALSE(swapped);
    ASSERT_NE(nullptr, expected);
    EXPECT_EQ(5, *expected);
    EXPECT_EQ(5, *atom.load());
}

TEST(AtomicSharedPtrTest, ConcurrentIncrementLoopConverges) {
    containers::AtomicSharedPtr<int> atom(std::make_shared<int>(0));

    const int threadCount = 4;
    const int incrementsPerThread = 500;
    std::vector<std::thread> threads;
    for (int index = 0; index < threadCount; ++index) {
        threads.emplace_back([&atom] {
            for (int step = 0; step < incrementsPerThread; ++step) {
                auto current = atom.load();
                auto next = std::make_shared<int>(*current + 1);
                while (!atom.compareExchange(current, next)) {
                    next = std::make_shared<int>(*current + 1);
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(threadCount * incrementsPerThread, *atom.load());
}
