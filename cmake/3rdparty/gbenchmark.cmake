include(FetchContent)

FetchContent_Declare(
    GoogleBenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.0
)
# Benchmarks are build-time only; don't install benchmark headers/libs with us.
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
# v1.8.0 trips -Winvalid-offsetof under clang 19; upstream's own -Werror is not
# meant for downstream toolchains.
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(GoogleBenchmark)

# Under the MSVC-compatibility flag set, clang promotes offsetof-on-non-POD to a
# hard error, which breaks benchmark 1.8.0's own static_assert. Upstream's
# #pragma suppression does not cover that promotion, so disable the diagnostic
# for the third-party target only.
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND TARGET benchmark)
    target_compile_options(benchmark PRIVATE -Wno-invalid-offsetof)
endif()
