#include "containers/stdx/Cell.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>

TEST(CellTest, DefaultConstructsToValueInitializedValue) {
    const containers::Cell<int> cell;
    EXPECT_EQ(0, cell.get());
}

TEST(CellTest, GetSetAndReplace) {
    containers::Cell<int> cell(1);
    EXPECT_EQ(1, cell.get());

    cell.set(42);
    EXPECT_EQ(42, cell.get());

    const int old = cell.replace(7);
    EXPECT_EQ(42, old);
    EXPECT_EQ(7, cell.get());
}

TEST(CellTest, ConstCellIsInteriorMutable) {
    containers::Cell<std::string> cell(std::string("initial"));
    const containers::Cell<std::string>& constCell = cell;

    constCell.set(std::string("changed"));
    EXPECT_EQ("changed", cell.get());
}

TEST(CellTest, TakeResetsToDefault) {
    containers::Cell<std::string> cell(std::string("payload"));
    EXPECT_EQ("payload", cell.take());
    EXPECT_EQ("", cell.get());
}

TEST(CellTest, CopyAndAssignment) {
    containers::Cell<int> source(5);
    containers::Cell<int> copy(source);
    copy.set(6);
    EXPECT_EQ(5, source.get());
    EXPECT_EQ(6, copy.get());

    containers::Cell<int> assigned(0);
    assigned = source;
    assigned.set(7);
    EXPECT_EQ(5, source.get());
    EXPECT_EQ(7, assigned.get());
}

TEST(RefCellTest, SharedBorrowsAreReadable) {
    const containers::RefCell<std::string> cell(std::string("hello"));
    const auto first = cell.borrow();
    const auto second = cell.borrow();

    EXPECT_EQ("hello", *first);
    EXPECT_EQ(5u, first->size());
    EXPECT_EQ("hello", second.get());
    EXPECT_EQ(2u, cell.sharedBorrowCount());
    EXPECT_TRUE(cell.isBorrowed());
    EXPECT_FALSE(cell.isMutablyBorrowed());
}

TEST(RefCellTest, MutableBorrowWritesThrough) {
    containers::RefCell<std::string> cell(std::string("ab"));
    {
        const auto mutableBorrow = cell.borrowMut();
        *mutableBorrow += "cd";
        mutableBorrow->append("ef");
        EXPECT_TRUE(cell.isMutablyBorrowed());
    }
    EXPECT_FALSE(cell.isBorrowed());
    EXPECT_EQ("abcdef", *cell.borrow());
}

TEST(RefCellTest, SharedBorrowRejectsMutableBorrow) {
    containers::RefCell<int> cell(1);
    const auto shared = cell.borrow();

    EXPECT_THROW(cell.borrowMut(), std::logic_error);

    const auto tryMutable = cell.tryBorrowMut();
    EXPECT_FALSE(tryMutable.hasValue());
    EXPECT_EQ(1u, cell.sharedBorrowCount());
}

TEST(RefCellTest, MutableBorrowRejectsSharedBorrow) {
    containers::RefCell<int> cell(1);
    const auto mutableBorrow = cell.borrowMut();

    EXPECT_THROW(cell.borrow(), std::logic_error);

    const auto tryShared = cell.tryBorrow();
    EXPECT_FALSE(tryShared.hasValue());
    EXPECT_TRUE(cell.isMutablyBorrowed());
}

TEST(RefCellTest, TryBorrowSucceedsWhenAvailable) {
    containers::RefCell<int> cell(9);

    auto shared = cell.tryBorrow();
    ASSERT_TRUE(shared.hasValue());
    EXPECT_EQ(9, shared.value().get());
    EXPECT_EQ(1u, cell.sharedBorrowCount());
    shared.reset();
    EXPECT_FALSE(cell.isBorrowed());

    auto mutableBorrow = cell.tryBorrowMut();
    ASSERT_TRUE(mutableBorrow.hasValue());
    *mutableBorrow.value() = 10;
    mutableBorrow.reset();
    EXPECT_EQ(10, *cell.borrow());
}

TEST(RefCellTest, BorrowGuardsReleaseOnScopeExit) {
    containers::RefCell<int> cell(3);
    {
        const auto shared = cell.borrow();
        EXPECT_TRUE(cell.isBorrowed());
    }
    EXPECT_FALSE(cell.isBorrowed());

    {
        const auto mutableBorrow = cell.borrowMut();
        EXPECT_TRUE(cell.isMutablyBorrowed());
    }
    EXPECT_FALSE(cell.isBorrowed());
}

TEST(RefCellTest, MovedGuardReleasesExactlyOnce) {
    containers::RefCell<std::string> cell(std::string("ab"));
    {
        auto first = cell.borrow();
        auto second = std::move(first);
        EXPECT_EQ(1u, cell.sharedBorrowCount());
        EXPECT_EQ(2u, second->size());
    }
    EXPECT_FALSE(cell.isBorrowed());
}
