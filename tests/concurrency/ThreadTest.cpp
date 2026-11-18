#include "containers/concurrency/Thread.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <type_traits>
#include <utility>

using containers::ScopedThread;
using containers::Thread;

TEST(ThreadTest, RunsCallableAndJoins) {
    std::atomic<int> value(0);
    Thread worker([&value] { value.fetch_add(1); });
    EXPECT_TRUE(worker.joinable());
    worker.join();
    EXPECT_FALSE(worker.joinable());
    EXPECT_EQ(1, value.load());
}

TEST(ThreadTest, JoinsInDestructor) {
    std::atomic<int> value(0);
    {
        Thread worker([&value] { value.fetch_add(1); });
        EXPECT_TRUE(worker.joinable());
    }
    EXPECT_EQ(1, value.load());
}

TEST(ThreadTest, DefaultConstructedIsNotJoinable) {
    Thread worker;
    EXPECT_FALSE(worker.joinable());
}

TEST(ThreadTest, MoveTransfersOwnership) {
    std::atomic<int> value(0);
    Thread first([&value] { value.fetch_add(1); });
    Thread second(std::move(first));
    EXPECT_FALSE(first.joinable());
    EXPECT_TRUE(second.joinable());
    second.join();
    EXPECT_EQ(1, value.load());
}

TEST(ThreadTest, MoveAssignmentJoinsExistingWorker) {
    std::atomic<int> value(0);
    Thread worker([&value] { value.fetch_add(1); });
    {
        Thread incoming([&value] { value.fetch_add(1); });
        worker = std::move(incoming);
        EXPECT_FALSE(incoming.joinable());
    }
    EXPECT_GE(value.load(), 1);
    worker.join();
    EXPECT_EQ(2, value.load());
}

TEST(ThreadTest, DetachReleasesOwnership) {
    std::promise<void> promise;
    std::future<void> signalled = promise.get_future();
    Thread worker([&promise] { promise.set_value(); });
    worker.detach();
    EXPECT_FALSE(worker.joinable());
    EXPECT_EQ(std::future_status::ready, signalled.wait_for(std::chrono::seconds(5)));
}

TEST(ThreadTest, JoinAfterJoinThrows) {
    Thread worker([] {});
    worker.join();
    EXPECT_THROW(worker.join(), std::system_error);
}

TEST(ThreadTest, HardwareConcurrencyIsQueryable) {
    const unsigned count = Thread::hardwareConcurrency();
    (void)count;
    SUCCEED();
}

TEST(ScopedThreadTest, JoinsOnScopeExit) {
    std::atomic<int> value(0);
    {
        ScopedThread worker([&value] { value.fetch_add(1); });
        EXPECT_TRUE(worker.joinable());
    }
    EXPECT_EQ(1, value.load());
}

TEST(ScopedThreadTest, ExplicitJoinIsAllowed) {
    std::atomic<int> value(0);
    ScopedThread worker([&value] { value.fetch_add(1); });
    worker.join();
    EXPECT_FALSE(worker.joinable());
    EXPECT_EQ(1, value.load());
}

TEST(ScopedThreadTest, IsNeitherCopyableNorMovable) {
    static_assert(!std::is_copy_constructible<ScopedThread>::value,
                  "ScopedThread must not be copyable");
    static_assert(!std::is_move_constructible<ScopedThread>::value,
                  "ScopedThread must not be movable");
    SUCCEED();
}

TEST(ScopedThreadTest, JoinAfterJoinThrows) {
    ScopedThread worker([] {});
    worker.join();
    EXPECT_THROW(worker.join(), std::system_error);
}
