#include "containers/concurrency/MpscQueue.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

using containers::MpscQueue;

namespace {

bool WaitForValue(MpscQueue<int>& queue, int& value) {
    return queue.waitAndPopFor(value, std::chrono::seconds(5));
}

}  // namespace

TEST(MpscQueueTest, RejectsZeroCapacity) {
    EXPECT_THROW(MpscQueue<int>(0), std::invalid_argument);
}

TEST(MpscQueueTest, RoundsCapacityUpToPowerOfTwo) {
    MpscQueue<int> queue(3);
    EXPECT_EQ(4u, queue.capacity());
    MpscQueue<int> exact(8);
    EXPECT_EQ(8u, exact.capacity());
}

TEST(MpscQueueTest, PushPopPreservesFifoOnSingleThread) {
    MpscQueue<int> queue(8);
    EXPECT_TRUE(queue.empty());
    EXPECT_TRUE(queue.tryPush(1));
    EXPECT_TRUE(queue.tryPush(2));
    EXPECT_TRUE(queue.tryPush(3));
    EXPECT_EQ(3u, queue.size());

    int value = 0;
    EXPECT_TRUE(queue.tryPop(value));
    EXPECT_EQ(1, value);
    EXPECT_TRUE(queue.tryPop(value));
    EXPECT_EQ(2, value);
    EXPECT_TRUE(queue.tryPop(value));
    EXPECT_EQ(3, value);
    EXPECT_FALSE(queue.tryPop(value));
    EXPECT_TRUE(queue.empty());
}

TEST(MpscQueueTest, TryPushFailsWhenFull) {
    MpscQueue<int> queue(2);
    EXPECT_TRUE(queue.tryPush(1));
    EXPECT_TRUE(queue.tryPush(2));
    EXPECT_FALSE(queue.tryPush(3));

    int value = 0;
    EXPECT_TRUE(queue.tryPop(value));
    EXPECT_EQ(1, value);
    EXPECT_TRUE(queue.tryPush(3));
}

TEST(MpscQueueTest, WrapAroundReusesSlots) {
    MpscQueue<int> queue(4);
    int value = 0;
    for (int index = 0; index < 10000; ++index) {
        EXPECT_TRUE(queue.tryPush(index));
        EXPECT_TRUE(queue.tryPop(value));
        EXPECT_EQ(index, value);
    }
    EXPECT_TRUE(queue.empty());
}

TEST(MpscQueueTest, MultiProducerSingleConsumer) {
    const int producerCount = 4;
    const int perProducer = 1000;
    MpscQueue<int> queue(1024);
    std::vector<std::thread> producers;
    for (int producer = 0; producer < producerCount; ++producer) {
        producers.emplace_back([&, producer] {
            for (int index = 0; index < perProducer; ++index) {
                EXPECT_TRUE(queue.push(producer * 100000 + index));
            }
        });
    }

    std::vector<int> last(producerCount, -1);
    int received = 0;
    while (received < producerCount * perProducer) {
        int value = 0;
        ASSERT_TRUE(WaitForValue(queue, value));
        const int producer = value / 100000;
        const int sequence = value % 100000;
        ASSERT_GE(producer, 0);
        ASSERT_LT(producer, producerCount);
        ASSERT_EQ(last[producer] + 1, sequence);
        last[producer] = sequence;
        ++received;
    }
    for (std::thread& producer : producers) {
        producer.join();
    }
    EXPECT_TRUE(queue.empty());
    EXPECT_FALSE(queue.tryPop(received));
}

TEST(MpscQueueTest, PushBlocksWhenFull) {
    MpscQueue<int> queue(2);
    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));

    std::promise<void> finished;
    std::future<void> done = finished.get_future();
    std::thread producer([&queue, &finished] {
        EXPECT_TRUE(queue.push(3));
        finished.set_value();
    });

    EXPECT_EQ(std::future_status::timeout,
              done.wait_for(std::chrono::milliseconds(50)));

    int value = 0;
    EXPECT_TRUE(queue.tryPop(value));
    EXPECT_EQ(1, value);
    EXPECT_EQ(std::future_status::ready, done.wait_for(std::chrono::seconds(5)));
    EXPECT_TRUE(queue.tryPop(value));
    EXPECT_EQ(2, value);
    EXPECT_TRUE(queue.tryPop(value));
    EXPECT_EQ(3, value);
    producer.join();
}

TEST(MpscQueueTest, CloseWakesBlockedConsumer) {
    MpscQueue<int> queue(8);
    std::promise<int> popped;
    std::future<int> result = popped.get_future();
    std::thread consumer([&queue, &popped] {
        int value = 0;
        const bool ok = queue.waitAndPop(value);
        popped.set_value(ok ? value : -1);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    queue.close();
    EXPECT_EQ(std::future_status::ready, result.wait_for(std::chrono::seconds(5)));
    EXPECT_EQ(-1, result.get());
    consumer.join();
}

TEST(MpscQueueTest, CloseRejectsNewPushes) {
    MpscQueue<int> queue(8);
    queue.close();
    EXPECT_TRUE(queue.isClosed());
    EXPECT_FALSE(queue.tryPush(1));
    EXPECT_FALSE(queue.push(1));
}

TEST(MpscQueueTest, WaitAndPopForTimesOutOnEmptyQueue) {
    MpscQueue<int> queue(8);
    int value = 0;
    EXPECT_FALSE(queue.waitAndPopFor(value, std::chrono::milliseconds(10)));
}

TEST(MpscQueueTest, SupportsMoveOnlyValues) {
    MpscQueue<std::unique_ptr<int>> queue(8);
    EXPECT_TRUE(queue.tryPush(std::unique_ptr<int>(new int(41))));
    std::unique_ptr<int> value;
    EXPECT_TRUE(queue.tryPop(value));
    ASSERT_NE(nullptr, value.get());
    EXPECT_EQ(41, *value);
}

TEST(MpscQueueTest, IsNeitherCopyableNorMovable) {
    static_assert(!std::is_copy_constructible<MpscQueue<int>>::value,
                  "MpscQueue must not be copyable");
    static_assert(!std::is_move_constructible<MpscQueue<int>>::value,
                  "MpscQueue must not be movable");
    SUCCEED();
}

TEST(MpscQueueTest, DestructorDrainsPendingValues) {
    {
        MpscQueue<std::unique_ptr<int>> queue(8);
        EXPECT_TRUE(queue.tryPush(std::unique_ptr<int>(new int(1))));
        EXPECT_TRUE(queue.tryPush(std::unique_ptr<int>(new int(2))));
    }
    SUCCEED();
}
