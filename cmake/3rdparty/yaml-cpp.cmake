include(FetchContent)

# yaml-cpp 0.9.0: current release. It declares
# cmake_minimum_required(VERSION 3.5...3.30), so CMake >= 4.0 configures it
# without a policy shim, and its sources carry the <cstdint> include that
# 0.7.0/0.8.0 were missing (their src/emitterutils.cpp uses uint16_t/uint32_t
# and fails on GCC 14+/Clang 18+, where libstdc++ no longer pulls it in).
FetchContent_Declare(yaml
    URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.9.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# Keep install rules on so a supervised `cmake --install` publishes a complete
# yaml-cpp target/headers alongside Containers. Skip the parse tools, the
# contrib module and the format target to keep the dependency build small.
set(YAML_CPP_INSTALL ON CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(yaml)
