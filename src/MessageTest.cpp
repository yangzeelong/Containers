#include "containers/Message.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace containers;

class MessageTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MessageTest, BasicFunctionality) {
    Message empty_msg;
    EXPECT_FALSE(empty_msg.HasData());
    EXPECT_FALSE(empty_msg.HasType<int>());

    std::string test_string = "hello world";
    Message str_msg = MakeMessage(test_string, "TestSender");

    EXPECT_TRUE(str_msg.HasData());
    EXPECT_TRUE(str_msg.HasType<std::string>());
    EXPECT_FALSE(str_msg.HasType<int>());

    auto meta = str_msg.GetMetaData();
    EXPECT_EQ(meta.sourceName, "TestSender");
    EXPECT_NE(meta.messageId, (uint64_t)-1);
    EXPECT_GT(meta.timestamp, 0);
}

TEST_F(MessageTest, DataAccess) {
    auto msg = MakeMessage(std::vector<int>{10, 20});

    const auto& vec_ref = msg.Borrow<std::vector<int>>();
    ASSERT_EQ(vec_ref.size(), 2);
    EXPECT_EQ(vec_ref[0], 10);

    EXPECT_THROW(msg.Borrow<std::string>(), std::runtime_error);

    const auto* vec_ptr = msg.BorrowPtr<std::vector<int>>();
    ASSERT_NE(vec_ptr, nullptr);
    EXPECT_EQ((*vec_ptr)[1], 20);

    const auto* str_ptr = msg.BorrowPtr<std::string>();
    EXPECT_EQ(str_ptr, nullptr);

    msg.Mut<std::vector<int>>()[0] = 11;
    EXPECT_EQ(msg.Borrow<std::vector<int>>()[0], 11);

    EXPECT_THROW(msg.Mut<double>(), std::runtime_error);

    auto* mut_vec_ptr = msg.MutPtr<std::vector<int>>();
    ASSERT_NE(mut_vec_ptr, nullptr);
    mut_vec_ptr->push_back(30);
    EXPECT_EQ(msg.Borrow<std::vector<int>>().size(), 3);

    auto* mut_double_ptr = msg.MutPtr<double>();
    EXPECT_EQ(mut_double_ptr, nullptr);
}

TEST_F(MessageTest, CopyOnWrite) {
    auto original_msg = MakeMessage(std::vector<int>{1, 2, 3});
    auto shared_copy = original_msg;

    EXPECT_EQ(original_msg.Borrow<std::vector<int>>()[1], 2);
    EXPECT_EQ(shared_copy.Borrow<std::vector<int>>()[1], 2);

    shared_copy.Mut<std::vector<int>>()[1] = 99;

    EXPECT_EQ(shared_copy.Borrow<std::vector<int>>()[1], 99);
    EXPECT_EQ(original_msg.Borrow<std::vector<int>>()[1], 2);
}

TEST_F(MessageTest, CopyOnWriteWithPtr) {
    auto original_msg = MakeMessage(std::string("original"));
    auto shared_copy = original_msg;

    auto* str_ptr = shared_copy.MutPtr<std::string>();
    ASSERT_NE(str_ptr, nullptr);
    *str_ptr += " (modified)";

    EXPECT_EQ(shared_copy.Borrow<std::string>(), "original (modified)");
    EXPECT_EQ(original_msg.Borrow<std::string>(), "original");
}

TEST_F(MessageTest, Clone) {
    auto original_msg = MakeMessage(42, "SourceA");
    auto cloned_msg = original_msg.Clone();

    ASSERT_TRUE(cloned_msg.HasData());
    ASSERT_TRUE(cloned_msg.HasType<int>());

    cloned_msg.Mut<int>() = 99;

    EXPECT_EQ(cloned_msg.Borrow<int>(), 99);
    EXPECT_EQ(original_msg.Borrow<int>(), 42);

    EXPECT_EQ(original_msg.GetMetaData().sourceName, cloned_msg.GetMetaData().sourceName);
    EXPECT_EQ(original_msg.GetMetaData().messageId, cloned_msg.GetMetaData().messageId);
}

TEST_F(MessageTest, OwnershipAndLifecycle) {
    auto original_ptr = std::make_shared<int>(10);
    std::weak_ptr<int> tracker = original_ptr;

    {
        Message msg1 = MakeMessage(original_ptr);
        original_ptr.reset();
        EXPECT_FALSE(tracker.expired());

        {
            Message msg2 = msg1;
            Message msg3;
            msg3 = msg2;
            EXPECT_FALSE(tracker.expired());
        }

        EXPECT_FALSE(tracker.expired());
    }

    EXPECT_TRUE(tracker.expired());
}

TEST_F(MessageTest, Multithreading) {
    auto shared_msg = MakeMessage(std::vector<int>{0});
    std::atomic<int> read_sum{0};
    const int num_readers = 10;

    std::vector<std::thread> threads;

    for (int i = 0; i < num_readers; ++i) {
        threads.emplace_back([&shared_msg, &read_sum]() {
            for (int j = 0; j < 100; ++j) {
                if (const auto* vec = shared_msg.BorrowPtr<std::vector<int>>()) {
                    if (!vec->empty()) {
                        read_sum += (*vec)[0];
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    threads.emplace_back([&shared_msg]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        auto* vec = shared_msg.MutPtr<std::vector<int>>();
        if (vec) {
            (*vec)[0] = 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        vec = shared_msg.MutPtr<std::vector<int>>();
        if (vec) {
            (*vec)[0] = 2;
        }
    });

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(shared_msg.Borrow<std::vector<int>>()[0], 2);
    SUCCEED();
}
