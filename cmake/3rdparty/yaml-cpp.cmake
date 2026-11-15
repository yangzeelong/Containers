include(FetchContent)

# yaml-cpp 0.7.0: the release that still builds cleanly under MSVC without
# C++17 features, and the version the registry format is validated against.
FetchContent_Declare(yaml
    URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.tar.gz)
# Keep install rules on so a supervised `cmake --install` publishes a complete
# yaml-cpp target/headers alongside Containers.
set(YAML_CPP_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml)
