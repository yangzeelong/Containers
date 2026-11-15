include(FetchContent)

# yaml-cpp 0.7.0: the release that still builds cleanly under MSVC without
# C++17 features, and the version the registry format is validated against.
FetchContent_Declare(yaml
    URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# yaml-cpp declares cmake_minimum_required(VERSION 3.4), which CMake >= 4.0
# refuses to configure at all ("Compatibility with CMake < 3.5 has been
# removed"). Raise the policy floor for the dependencies only instead of
# patching upstream sources; an explicit -DCMAKE_POLICY_VERSION_MINIMUM wins.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Policy floor for legacy dependencies")
endif()

# Keep install rules on so a supervised `cmake --install` publishes a complete
# yaml-cpp target/headers alongside Containers.
set(YAML_CPP_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml)
