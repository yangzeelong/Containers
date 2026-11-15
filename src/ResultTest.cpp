#include <containers/Result.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

namespace {

using IntResult = containers::Result<int, std::string>;

} // namespace

TEST(ResultTest, OkCarriesValue) {
    IntResult result = IntResult::Ok(42);

    EXPECT_TRUE(result.isOk());
    EXPECT_FALSE(result.isErr());
    EXPECT_EQ(42, result.value());
    EXPECT_EQ(42, result.unwrap());
}

TEST(ResultTest, ErrCarriesError) {
    IntResult result = IntResult::Err(std::string("boom"));

    EXPECT_TRUE(result.isErr());
    EXPECT_FALSE(result.isOk());
    EXPECT_EQ("boom", result.error());
    EXPECT_EQ("boom", result.unwrapErr());
    EXPECT_EQ(-1, result.unwrapOr(-1));
}

TEST(ResultTest, UnwrapOrKeepsValueWhenOk) {
    IntResult result = IntResult::Ok(5);

    EXPECT_EQ(5, result.unwrapOr(-1));
}

TEST(ResultTest, MapTransformsValue) {
    auto doubled = IntResult::Ok(21).map([](int value) { return value * 2; });

    ASSERT_TRUE(doubled.isOk());
    EXPECT_EQ(42, doubled.value());

    auto failed = IntResult::Err(std::string("nope")).map([](int value) { return value * 2; });

    ASSERT_TRUE(failed.isErr());
    EXPECT_EQ("nope", failed.error());
}

TEST(ResultTest, MapCanReturnVoid) {
    auto result = IntResult::Ok(1).map([](int) {});

    EXPECT_TRUE(result.isOk());
    result.unwrap();
}

TEST(ResultTest, MapErrTransformsError) {
    auto kept = IntResult::Ok(1).mapErr([](const std::string& error) {
        return error.size();
    });

    ASSERT_TRUE(kept.isOk());
    EXPECT_EQ(1, kept.value());

    auto mapped = IntResult::Err(std::string("abc")).mapErr([](const std::string& error) {
        return error.size();
    });

    ASSERT_TRUE(mapped.isErr());
    EXPECT_EQ(3u, mapped.error());
}

TEST(ResultTest, AndThenChainsAndPropagates) {
    auto chained = IntResult::Ok(20).andThen([](int value) {
        return IntResult::Ok(value + 22);
    });

    ASSERT_TRUE(chained.isOk());
    EXPECT_EQ(42, chained.value());

    auto stopped = IntResult::Err(std::string("stop")).andThen([](int value) {
        return IntResult::Ok(value);
    });

    ASSERT_TRUE(stopped.isErr());
    EXPECT_EQ("stop", stopped.error());
}

TEST(ResultTest, CopyIsIndependent) {
    IntResult original = IntResult::Ok(1);
    IntResult copy = original;
    copy.value() = 2;

    EXPECT_EQ(1, original.value());
    EXPECT_EQ(2, copy.value());

    IntResult failed = IntResult::Err(std::string("first"));
    IntResult failedCopy = failed;
    failedCopy.error() = "second";

    EXPECT_EQ("first", failed.error());
    EXPECT_EQ("second", failedCopy.error());
}

TEST(ResultTest, MoveCarriesState) {
    IntResult original = IntResult::Ok(9);
    IntResult moved = std::move(original);

    EXPECT_EQ(9, moved.value());
}

TEST(ResultTest, AssignmentSwitchesState) {
    IntResult result = IntResult::Ok(1);

    result = IntResult::Err(std::string("assigned"));
    ASSERT_TRUE(result.isErr());
    EXPECT_EQ("assigned", result.error());

    result = IntResult::Ok(3);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(3, result.value());
}

TEST(ResultTest, SupportsMoveOnlyValues) {
    using PointerResult = containers::Result<std::unique_ptr<int>, int>;

    PointerResult result = PointerResult::Ok(std::unique_ptr<int>(new int(7)));

    ASSERT_TRUE(result.isOk());
    ASSERT_NE(nullptr, result.value().get());
    EXPECT_EQ(7, *result.value());

    PointerResult moved = std::move(result);
    EXPECT_EQ(7, *moved.value());
}

TEST(ResultTest, VoidOkAndErr) {
    using VoidResult = containers::Result<void, std::string>;

    VoidResult ok = VoidResult::Ok();
    EXPECT_TRUE(ok.isOk());
    ok.unwrap();

    VoidResult failed = VoidResult::Err("bad");
    ASSERT_TRUE(failed.isErr());
    EXPECT_EQ("bad", failed.error());
    EXPECT_EQ("bad", failed.unwrapErr());
}

TEST(ResultTest, VoidMapProducesValue) {
    using VoidResult = containers::Result<void, std::string>;

    auto mapped = VoidResult::Ok().map([] { return 3; });

    ASSERT_TRUE(mapped.isOk());
    EXPECT_EQ(3, mapped.value());

    auto failed = VoidResult::Err("bad").map([] { return 3; });

    ASSERT_TRUE(failed.isErr());
    EXPECT_EQ("bad", failed.error());
}

TEST(ResultTest, VoidAndThenChains) {
    using VoidResult = containers::Result<void, std::string>;

    auto chained = VoidResult::Ok().andThen([] {
        return IntResult::Ok(41);
    });

    ASSERT_TRUE(chained.isOk());
    EXPECT_EQ(41, chained.value());

    auto stopped = VoidResult::Err("stop").andThen([] {
        return IntResult::Ok(1);
    });

    ASSERT_TRUE(stopped.isErr());
    EXPECT_EQ("stop", stopped.error());
}
