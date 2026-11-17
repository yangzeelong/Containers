#include <containers/collections/IndexMap.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

TEST(IndexMapTest, KeepsInsertionOrder) {
    containers::IndexMap<std::string, int> map;
    map.insert("first", 1);
    map.insert("second", 2);
    map.insert("third", 3);

    ASSERT_EQ(3u, map.size());

    std::vector<std::string> keys;
    for (const auto& entry : map) {
        keys.push_back(entry.first);
    }

    ASSERT_EQ(3u, keys.size());
    EXPECT_EQ("first", keys[0]);
    EXPECT_EQ("second", keys[1]);
    EXPECT_EQ("third", keys[2]);
    EXPECT_EQ("second", map.keyAt(1));
}

TEST(IndexMapTest, InsertReplacesValueWithoutMovingPosition) {
    containers::IndexMap<std::string, int> map;
    EXPECT_TRUE(map.insert("a", 1));
    EXPECT_TRUE(map.insert("b", 2));
    EXPECT_FALSE(map.insert("a", 10));

    EXPECT_EQ(10, map.at("a"));
    EXPECT_EQ("a", map.keyAt(0));
    EXPECT_EQ("b", map.keyAt(1));
}

TEST(IndexMapTest, FindAndContains) {
    containers::IndexMap<int, int> map;
    map.insert(7, 70);

    ASSERT_NE(nullptr, map.find(7));
    EXPECT_EQ(70, *map.find(7));
    EXPECT_EQ(nullptr, map.find(8));
    EXPECT_TRUE(map.contains(7));
    EXPECT_FALSE(map.contains(8));
}

TEST(IndexMapTest, SubscriptInsertsDefaultValue) {
    containers::IndexMap<std::string, int> map;
    map["a"] = 1;
    map["b"] = 2;

    EXPECT_EQ(2u, map.size());
    EXPECT_EQ(1, map.at("a"));
    EXPECT_EQ(2, map.at("b"));
}

TEST(IndexMapTest, ErasePreservesOrder) {
    containers::IndexMap<std::string, int> map;
    map.insert("a", 1);
    map.insert("b", 2);
    map.insert("c", 3);

    EXPECT_TRUE(map.erase("b"));
    EXPECT_FALSE(map.erase("b"));
    EXPECT_EQ(2u, map.size());
    EXPECT_EQ("a", map.keyAt(0));
    EXPECT_EQ("c", map.keyAt(1));
    EXPECT_EQ(3, map.at("c"));
}

TEST(IndexMapTest, SwapEraseMovesLastEntryIntoHole) {
    containers::IndexMap<std::string, int> map;
    map.insert("a", 1);
    map.insert("b", 2);
    map.insert("c", 3);

    EXPECT_TRUE(map.swapErase("a"));
    EXPECT_EQ(2u, map.size());
    EXPECT_EQ("c", map.keyAt(0));
    EXPECT_EQ("b", map.keyAt(1));
    EXPECT_EQ(3, map.at("c"));
    EXPECT_EQ(2, map.at("b"));
}

TEST(IndexMapTest, ClearAndEmpty) {
    containers::IndexMap<int, int> map;
    map.insert(1, 1);
    map.insert(2, 2);
    map.clear();

    EXPECT_TRUE(map.empty());
    EXPECT_EQ(0u, map.size());
    EXPECT_FALSE(map.contains(1));
}
