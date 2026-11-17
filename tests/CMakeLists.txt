file(GLOB_RECURSE CUR_TEST_SOURCES CONFIGURE_DEPENDS "**Test.cpp")

if(NOT CUR_TEST_SOURCES)
    message(WARNING "No test sources found - will not build unit tests")
    return()
endif()

add_executable(containers_tests gtest_main.cpp ${CUR_TEST_SOURCES})
containers_enable_warnings(containers_tests)
target_link_libraries(containers_tests PRIVATE containers GTest::gtest)

add_test(NAME containers_tests COMMAND containers_tests)
