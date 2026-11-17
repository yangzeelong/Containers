#include <containers/collections/Bytes.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

TEST(BytesTest, CopyOwnsPayload) {
    const std::string source = "hello bytes";
    containers::Bytes bytes = containers::Bytes::Copy(source);

    EXPECT_EQ(source.size(), bytes.size());
    EXPECT_FALSE(bytes.empty());
    EXPECT_EQ(0, std::memcmp(source.data(), bytes.data(), source.size()));
    EXPECT_EQ(source, bytes.toString());
}

TEST(BytesTest, CloneSharesStorage) {
    containers::Bytes bytes = containers::Bytes::Copy(std::string("payload"));
    containers::Bytes clone = bytes.clone();

    EXPECT_EQ(bytes.data(), clone.data());
    EXPECT_EQ(bytes, clone);
}

TEST(BytesTest, SliceSharesStorage) {
    containers::Bytes bytes = containers::Bytes::Copy(std::string("abcdef"));
    containers::Bytes slice = bytes.slice(2, 3);

    EXPECT_EQ(3u, slice.size());
    EXPECT_EQ(bytes.data() + 2, slice.data());
    EXPECT_EQ("cde", slice.toString());

    containers::Bytes empty = bytes.slice(6, 0);
    EXPECT_TRUE(empty.empty());
}

TEST(BytesTest, ToVectorCopies) {
    containers::Bytes bytes = containers::Bytes::Copy(std::string("copy"));
    std::vector<std::uint8_t> copy = bytes.toVector();

    EXPECT_EQ(4u, copy.size());
    EXPECT_EQ('c', copy[0]);
    EXPECT_EQ('y', copy[3]);
}

TEST(BytesTest, EqualityComparesContent) {
    const auto first = containers::Bytes::Copy(std::string("same"));
    const auto second = containers::Bytes::Copy(std::string("same"));
    const auto other = containers::Bytes::Copy(std::string("other"));

    EXPECT_EQ(first, second);
    EXPECT_NE(first, other);

    const containers::Bytes empty;
    const containers::Bytes emptyCopy;

    EXPECT_EQ(empty, emptyCopy);
    EXPECT_NE(empty, first);
}

TEST(BytesMutTest, AppendAndFreeze) {
    containers::BytesMut buffer;
    buffer.append("abc", 3);
    buffer.append("de", 2);

    EXPECT_EQ(5u, buffer.size());

    containers::Bytes bytes = buffer.freeze();

    EXPECT_EQ("abcde", bytes.toString());
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(0u, buffer.size());
}

TEST(BytesMutTest, ResizeAndCopy) {
    containers::BytesMut buffer(16);
    buffer.resize(4);
    buffer.data()[0] = 'x';

    EXPECT_EQ(4u, buffer.size());

    containers::Bytes bytes = buffer.copyToBytes();
    EXPECT_EQ(4u, bytes.size());
    EXPECT_EQ('x', bytes.toString()[0]);
}

TEST(BytesMutTest, FreezeSharesWithoutCopy) {
    containers::BytesMut buffer;
    buffer.append("keep", 4);
    const std::uint8_t* raw = buffer.data();

    containers::Bytes bytes = buffer.freeze();

    EXPECT_EQ("keep", bytes.toString());
    EXPECT_EQ(raw, bytes.data());
}
