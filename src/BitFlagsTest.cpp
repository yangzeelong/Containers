#include <containers/BitFlags.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace {

enum class Permission : std::uint32_t {
    Read = 1u << 0,
    Write = 1u << 1,
    Execute = 1u << 2,
};

using Permissions = containers::BitFlags<Permission>;

} // namespace

TEST(BitFlagsTest, StartsEmpty) {
    const Permissions permissions;

    EXPECT_TRUE(permissions.empty());
    EXPECT_EQ(0u, static_cast<std::uint32_t>(permissions.bits()));
    EXPECT_FALSE(permissions.contains(Permission::Read));
}

TEST(BitFlagsTest, InsertsAndRemovesFlags) {
    Permissions permissions;
    permissions.insert(Permission::Read);
    permissions.insert(Permission::Write);

    EXPECT_TRUE(permissions.contains(Permission::Read));
    EXPECT_TRUE(permissions.contains(Permission::Write));
    EXPECT_FALSE(permissions.contains(Permission::Execute));

    permissions.remove(Permission::Write);

    EXPECT_TRUE(permissions.contains(Permission::Read));
    EXPECT_FALSE(permissions.contains(Permission::Write));
}

TEST(BitFlagsTest, CombinesWithOperators) {
    const Permissions readWrite = Permissions::Of(Permission::Read) | Permissions::Of(Permission::Write);

    EXPECT_EQ(3u, static_cast<std::uint32_t>(readWrite.bits()));
    EXPECT_TRUE(readWrite.containsAll(Permissions::Of(Permission::Read) | Permissions::Of(Permission::Write)));
    EXPECT_FALSE(readWrite.contains(Permission::Execute));
}

TEST(BitFlagsTest, FlagsHelperCombinesEnums) {
    const auto readWrite = containers::Flags(Permission::Read, Permission::Write);

    EXPECT_TRUE(readWrite.contains(Permission::Read));
    EXPECT_TRUE(readWrite.contains(Permission::Write));
    EXPECT_EQ(3u, static_cast<std::uint32_t>(readWrite.bits()));
}

TEST(BitFlagsTest, DifferenceAndIntersection) {
    const Permissions all = Permissions::Of(Permission::Read) | Permissions::Of(Permission::Write) |
                            Permissions::Of(Permission::Execute);
    const Permissions readOnly = Permissions::Of(Permission::Read);

    const Permissions withoutRead = all & ~readOnly;
    EXPECT_FALSE(withoutRead.contains(Permission::Read));
    EXPECT_TRUE(withoutRead.contains(Permission::Write));

    const Permissions overlap = all & readOnly;
    EXPECT_TRUE(overlap.contains(Permission::Read));
    EXPECT_FALSE(overlap.contains(Permission::Write));
}

TEST(BitFlagsTest, ComparesByBits) {
    const Permissions lhs = Permissions::Of(Permission::Read);
    const Permissions rhs = Permissions::Of(Permission::Execute);

    EXPECT_EQ(lhs, Permissions::Of(Permission::Read));
    EXPECT_NE(lhs, rhs);
}
