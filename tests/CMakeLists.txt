get_property(ALL_TEST_SOURCES GLOBAL PROPERTY GLOBAL_TEST_SOURCES)

file(GLOB_RECURSE CUR_TEST_SOURCES CONFIGURE_DEPENDS "**Test.cpp")

if(NOT ALL_TEST_SOURCES AND NOT CUR_TEST_SOURCES)
    message(WARNING "No test sources found - will not build unit tests")
    return()
endif()

add_executable(containers_tests gtest_main.cpp ${CUR_TEST_SOURCES} ${ALL_TEST_SOURCES})
containers_enable_warnings(containers_tests)
target_link_libraries(containers_tests PRIVATE containers GTest::gtest)

add_test(NAME containers_tests COMMAND containers_tests)
