cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(Containers VERSION 0.1.0 LANGUAGES CXX)

# C++14 deliberately: every public header, test and benchmark targets the same
# C++14 baseline, so downstream projects on C++14 can adopt the library as-is.
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(INSTALL_ROOT ${PROJECT_ROOT}/install)

### Options
option(WITH_TESTING "Enable testing (googletest)" ON)
option(WITH_BENCHMARK "Enable benchmark (google benchmark)" ON)

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

### Custom cmake (pulls in googletest and google benchmark via FetchContent)
include("cmake/include.cmake")
include("cmake/Utils.cmake")

# The library is header-only: one INTERFACE target carries the include directory
# and nothing else to build; tests and benchmarks link against it.
include(GNUInstallDirs)

add_library(containers INTERFACE)
target_include_directories(containers INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(WITH_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

if(WITH_BENCHMARK)
    add_subdirectory(benchmarks)
endif()

### Install / package config
#
# Downstream projects find this package with find_package(Containers CONFIG) and
# link Containers::containers. The headers are header-only with no third-party
# runtime dependency.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CONTAINERS_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/Containers)

install(TARGETS containers EXPORT containersTargets)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/containers
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT containersTargets
    FILE ContainersTargets.cmake
    NAMESPACE Containers::
    DESTINATION ${CONTAINERS_CMAKE_INSTALL_DIR})

configure_package_config_file(
    cmake/ContainersConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ContainersConfig.cmake
    INSTALL_DESTINATION ${CONTAINERS_CMAKE_INSTALL_DIR})

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ContainersConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ContainersConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ContainersConfigVersion.cmake
    DESTINATION ${CONTAINERS_CMAKE_INSTALL_DIR})
