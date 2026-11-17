#include <containers/stdx/Cow.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

TEST(CowTest, BorrowedReadsWithoutOwning) {
    const std::string source = "borrowed";
    containers::Cow<std::string> cow = containers::Cow<std::string>::Borrowed(source);

    EXPECT_TRUE(cow.isBorrowed());
    EXPECT_FALSE(cow.isOwned());
    EXPECT_EQ("borrowed", cow.get());
    EXPECT_EQ(8u, cow->size());
    EXPECT_EQ("borrowed", *cow);
}

TEST(CowTest, ToMutDetachesBorrowedValue) {
    std::vector<int> source(4, 1);
    auto cow = containers::Cow<std::vector<int>>::Borrowed(source);

    cow.toMut()[0] = 42;

    EXPECT_TRUE(cow.isOwned());
    EXPECT_EQ(1, source[0]);
    EXPECT_EQ(42, cow.get()[0]);
}

TEST(CowTest, CopiedOwnedCowSharesUntilMutation) {
    auto original = containers::Cow<std::string>::Owned(std::string("shared"));
    auto copy = original;

    EXPECT_EQ(original.get().c_str(), copy.get().c_str());

    copy.toMut() = "detached";

    EXPECT_EQ("shared", original.get());
    EXPECT_EQ("detached", copy.get());
}

TEST(CowTest, IntoOwnedMovesUniqueOwner) {
    auto cow = containers::Cow<std::string>::Owned(std::string("move me"));

    const std::string moved = cow.intoOwned();

    EXPECT_EQ("move me", moved);
}

TEST(CowTest, IntoOwnedCopiesSharedOwner) {
    auto original = containers::Cow<std::string>::Owned(std::string("copy me"));
    auto copy = original;

    const std::string copied = copy.intoOwned();

    EXPECT_EQ("copy me", copied);
    EXPECT_EQ("copy me", original.get());
}

TEST(CowTest, SharedKeepsOwnerAlive) {
    auto owner = std::make_shared<std::string>("owned elsewhere");
    auto cow = containers::Cow<std::string>::Shared(owner);

    owner.reset();

    EXPECT_TRUE(cow.isOwned());
    EXPECT_EQ("owned elsewhere", cow.get());
}
