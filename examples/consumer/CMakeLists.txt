cmake_minimum_required(VERSION 3.16)

# Standalone downstream project: it is NOT part of the Containers build tree
# (the top-level CMakeLists never adds this directory). Point CMAKE_PREFIX_PATH at a
# `cmake --install` prefix to build it outside the tree:
#
#   cmake -S examples/consumer -B consumer-build -DCMAKE_PREFIX_PATH=<prefix>
#   cmake --build consumer-build
#   ./consumer-build/consumer
project(ContainersConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Containers CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE Containers::containers)
