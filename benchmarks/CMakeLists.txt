# Collect every Benchmark*.cpp under benchmarks/ (including subdirectories).
file(GLOB_RECURSE ALL_BENCHMARK_SOURCES CONFIGURE_DEPENDS "Benchmark*.cpp")

if(NOT ALL_BENCHMARK_SOURCES)
    message(WARNING "No benchmark sources found - will not build benchmarks")
    return()
endif()

add_executable(containers_benchmarks benchmark_main.cpp ${ALL_BENCHMARK_SOURCES})
containers_enable_warnings(containers_benchmarks)
target_link_libraries(containers_benchmarks PRIVATE containers benchmark::benchmark)

add_test(NAME containers_benchmarks COMMAND containers_benchmarks)
