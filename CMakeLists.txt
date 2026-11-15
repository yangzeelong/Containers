cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(Containers VERSION 0.1.0 LANGUAGES CXX)

# C++14 deliberately: the checked-in configs, tests and benchmarks are exercised
# on the same toolchain as Nexusflow, so header-only code can be shared.
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(INSTALL_ROOT ${PROJECT_ROOT}/install)

### Options
option(WITH_TESTING "Enable testing (googletest)" ON)
option(WITH_BENCHMARK "Enable benchmark (google benchmark)" ON)
option(WITH_EXAMPLES "Enable examples" ON)
option(WITH_TOOLS "Enable command-line tools (containers_cli)" ON)

if(NOT DEFINED CMAKE_BUILD_TYPE)
    message(STATUS "No build type specified, defaulting to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(CMAKE_INSTALL_PREFIX STREQUAL "" OR CMAKE_INSTALL_PREFIX STREQUAL "/usr/local"
   OR CMAKE_INSTALL_PREFIX MATCHES "^[A-Za-z]:/Program Files")
    set(CMAKE_INSTALL_PREFIX ${INSTALL_ROOT})
endif()

# The Clang GNU-style driver on Windows defaults to the static CRT while
# googletest is forced to the shared CRT, which makes lld-link reject the link
# with /failifmismatch. Pin every target (ours and FetchContent's) to the DLL
# runtime so the ABI matches across the whole build.
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT MSVC)
    add_compile_options(-fms-runtime-lib=dll)
    add_link_options(-fms-runtime-lib=dll)
endif()

### Custom cmake (pulls in yaml-cpp, googletest, google benchmark via FetchContent)
include("cmake/include.cmake")
include("cmake/Utils.cmake")

# Library sources living next to their own *Test.cpp get appended here.
set_property(GLOBAL PROPERTY GLOBAL_TEST_SOURCES "")

add_subdirectory(src)

if(WITH_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

if(WITH_BENCHMARK)
    add_subdirectory(benchmarks)
endif()

if(WITH_EXAMPLES)
    add_subdirectory(examples)
endif()

if(WITH_TOOLS)
    add_subdirectory(tools)
endif()
