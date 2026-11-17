#include <containers/collections/SlotMap.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

TEST(SlotMapTest, InsertAndGet) {
    containers::SlotMap<std::string> map;
    const auto key = map.insert("hello");

    ASSERT_TRUE(key.valid());
    EXPECT_EQ(1u, map.size());
    ASSERT_TRUE(map.contains(key));
    ASSERT_NE(nullptr, map.get(key));
    EXPECT_EQ("hello", *map.get(key));
    EXPECT_EQ("hello", map[key]);
}

TEST(SlotMapTest, EraseInvalidatesKey) {
    containers::SlotMap<int> map;
    const auto key = map.insert(7);

    EXPECT_TRUE(map.erase(key));
    EXPECT_FALSE(map.erase(key));
    EXPECT_FALSE(map.contains(key));
    EXPECT_EQ(nullptr, map.get(key));
    EXPECT_EQ(0u, map.size());
}

TEST(SlotMapTest, ReusedSlotGetsNewGeneration) {
    containers::SlotMap<int> map;
    const auto oldKey = map.insert(1);
    map.erase(oldKey);

    const auto newKey = map.insert(2);

    EXPECT_EQ(oldKey.index, newKey.index);
    EXPECT_NE(oldKey.generation, newKey.generation);
    EXPECT_FALSE(map.contains(oldKey));
    EXPECT_TRUE(map.contains(newKey));
    EXPECT_EQ(2, *map.get(newKey));
}

TEST(SlotMapTest, ClearInvalidatesEveryKey) {
    containers::SlotMap<int> map;
    const auto first = map.insert(1);
    const auto second = map.insert(2);

    map.clear();

    EXPECT_TRUE(map.empty());
    EXPECT_FALSE(map.contains(first));
    EXPECT_FALSE(map.contains(second));

    const auto third = map.insert(3);
    EXPECT_TRUE(map.contains(third));
    EXPECT_EQ(3, *map.get(third));
}

TEST(SlotMapTest, ForEachVisitsLiveValuesOnly) {
    containers::SlotMap<int> map;
    map.insert(1);
    const auto removed = map.insert(2);
    map.insert(3);
    map.erase(removed);

    std::vector<int> values;
    map.forEach([&values](int value) { values.push_back(value); });

    ASSERT_EQ(2u, values.size());
    EXPECT_EQ(1, values[0]);
    EXPECT_EQ(3, values[1]);
}

TEST(SlotMapTest, GrowsBeyondInitialSlots) {
    containers::SlotMap<int> map;
    std::vector<containers::SlotKey> keys;
    for (int value = 0; value < 32; ++value) {
        keys.push_back(map.insert(value));
    }

    ASSERT_EQ(32u, map.size());
    for (int value = 0; value < 32; ++value) {
        EXPECT_EQ(value, *map.get(keys[static_cast<std::size_t>(value)]));
    }
}
