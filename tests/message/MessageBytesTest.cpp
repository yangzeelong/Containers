#include <containers/collections/Bytes.hpp>
#include <containers/message/Message.hpp>

#include <gtest/gtest.h>

#include <string>

TEST(MessageBytesTest, MessageCopiesBytesWithoutCopyingPayload) {
    const containers::Bytes payload = containers::Bytes::Copy(std::string("zero-copy payload"));

    containers::Message message = containers::MakeMessage(payload);

    ASSERT_TRUE(message.HasType<containers::Bytes>());
    const containers::Bytes& stored = message.Borrow<containers::Bytes>();
    EXPECT_EQ(payload.data(), stored.data());
    EXPECT_EQ(payload, stored);
}

TEST(MessageBytesTest, SharingMessageKeepsOneBuffer) {
    const containers::Bytes payload = containers::Bytes::Copy(std::string("shared buffer"));
    containers::Message message = containers::MakeMessage(payload);

    containers::Message copy = message;

    EXPECT_EQ(message.BorrowPtr<containers::Bytes>(), copy.BorrowPtr<containers::Bytes>());
    EXPECT_EQ(payload.data(), copy.Borrow<containers::Bytes>().data());
}

TEST(MessageBytesTest, MutDetachesTheMessageButNotTheBytesBuffer) {
    const containers::Bytes payload = containers::Bytes::Copy(std::string("detach me"));
    containers::Message message = containers::MakeMessage(payload);
    containers::Message copy = message;

    const containers::Bytes* before = copy.BorrowPtr<containers::Bytes>();
    containers::Bytes& detached = copy.Mut<containers::Bytes>();

    // Message-level COW: the Bytes object itself is copied into a new model.
    EXPECT_NE(before, &detached);
    // Bytes-level COW: the underlying buffer is still shared, no bytes copied.
    EXPECT_EQ(payload.data(), detached.data());
    EXPECT_EQ(std::string("detach me"), detached.toString());
}

TEST(MessageBytesTest, CloneKeepsBytesStorageShared) {
    const containers::Bytes payload = containers::Bytes::Copy(std::string("clone"));
    containers::Message message = containers::MakeMessage(payload);

    containers::Message clone = message.Clone();

    ASSERT_TRUE(clone.HasType<containers::Bytes>());
    EXPECT_NE(message.BorrowPtr<containers::Bytes>(), clone.BorrowPtr<containers::Bytes>());
    EXPECT_EQ(payload.data(), clone.Borrow<containers::Bytes>().data());
}

TEST(MessageBytesTest, FrozenBytesMutTravelsThroughMessage) {
    containers::BytesMut buffer;
    buffer.append("frozen", 6);

    containers::Bytes frozen = buffer.freeze();
    containers::Message message = containers::MakeMessage(frozen);

    EXPECT_EQ(std::string("frozen"), message.Borrow<containers::Bytes>().toString());
    EXPECT_EQ(frozen.data(), message.Borrow<containers::Bytes>().data());
}
