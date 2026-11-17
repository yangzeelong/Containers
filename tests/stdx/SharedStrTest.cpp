#include <containers/stdx/SharedStr.hpp>

#include <gtest/gtest.h>

#include <string>
#include <unordered_set>

TEST(SharedStrTest, CopyOwnsText) {
    containers::SharedStr text = containers::SharedStr::Copy(std::string("hello"));

    EXPECT_EQ(5u, text.size());
    EXPECT_FALSE(text.empty());
    EXPECT_EQ("hello", text.toString());
}

TEST(SharedStrTest, FromLiteral) {
    containers::SharedStr text = containers::SharedStr::From("literal");

    EXPECT_EQ("literal", text.toString());
}

TEST(SharedStrTest, CloneSharesStorage) {
    containers::SharedStr text = containers::SharedStr::From("payload");
    containers::SharedStr clone = text.clone();

    EXPECT_EQ(text.data(), clone.data());
    EXPECT_EQ(text, clone);
}

TEST(SharedStrTest, SliceSharesStorage) {
    containers::SharedStr text = containers::SharedStr::From("abcdef");
    containers::SharedStr slice = text.slice(2, 3);

    EXPECT_EQ(3u, slice.size());
    EXPECT_EQ(text.data() + 2, slice.data());
    EXPECT_EQ("cde", slice.toString());
}

TEST(SharedStrTest, ComparesAgainstString) {
    containers::SharedStr text = containers::SharedStr::From("compare");

    EXPECT_EQ(std::string("compare"), text);
    EXPECT_TRUE(text == std::string("compare"));
    EXPECT_TRUE(text != std::string("other"));

    const containers::SharedStr empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(std::string(), empty);
}

TEST(SharedStrTest, HashesByContent) {
    containers::SharedStr first = containers::SharedStr::From("key");
    containers::SharedStr second = containers::SharedStr::Copy(std::string("ke") + "y");

    std::unordered_set<containers::SharedStr, containers::SharedStrHash> values;
    values.insert(first);
    values.insert(second);

    EXPECT_EQ(1u, values.size());
    EXPECT_EQ(first, second);
}

TEST(SharedStrTest, OwnedMovesText) {
    std::string source = "owned";
    containers::SharedStr text = containers::SharedStr::Owned(std::move(source));

    EXPECT_EQ("owned", text.toString());
}
