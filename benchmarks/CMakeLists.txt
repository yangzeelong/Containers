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

# Performance evaluation runner: threshold checks + Markdown report. It is kept
# separate from Google Benchmark because its job is "does this build still meet
# its targets", not "measure this case precisely".
add_executable(containers_perf_eval perf/PerfEvaluatorMain.cpp)
containers_enable_warnings(containers_perf_eval)
target_link_libraries(containers_perf_eval PRIVATE containers)

add_test(NAME containers_perf_eval
         COMMAND containers_perf_eval --rounds 3 --iterations 100 --warmup 1 --no-check
                 --output "${CMAKE_BINARY_DIR}/perf_report.md")
