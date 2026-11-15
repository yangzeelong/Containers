/*
 * Main entry point for the test runner. Initializes googletest, then runs every
 * TEST()/TEST_F() compiled into this executable. Returns non-zero on failure so
 * CI and ctest can gate on it.
 */
#include <gtest/gtest.h>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
