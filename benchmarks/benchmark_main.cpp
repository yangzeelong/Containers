#include <benchmark/benchmark.h>

// Expands to a main() that parses --benchmark_* flags and runs every registered
// benchmark. Keeping it in its own file leaves Benchmark*.cpp sources free of
// main() so they can be added incrementally.
BENCHMARK_MAIN();
