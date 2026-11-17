#include "containers/collections/BitSet.hpp"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstddef>
#include <vector>

static void BM_BitSetSetAll(benchmark::State& state) {
    const std::size_t bits = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        containers::BitSet set(bits);
        for (std::size_t index = 0; index < bits; ++index) {
            set.set(index, true);
        }
        benchmark::DoNotOptimize(set);
    }
}
BENCHMARK(BM_BitSetSetAll)->Arg(1024)->Arg(65536);

static void BM_VectorBoolSetAll(benchmark::State& state) {
    const std::size_t bits = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        std::vector<bool> set(bits, false);
        for (std::size_t index = 0; index < bits; ++index) {
            set[index] = true;
        }
        benchmark::DoNotOptimize(set);
    }
}
BENCHMARK(BM_VectorBoolSetAll)->Arg(1024)->Arg(65536);

static void BM_BitSetContains(benchmark::State& state) {
    const std::size_t bits = static_cast<std::size_t>(state.range(0));
    containers::BitSet set(bits, true);
    std::size_t hits = 0;
    for (auto _ : state) {
        for (std::size_t index = 0; index < bits; ++index) {
            hits += set.contains(index) ? 1 : 0;
        }
    }
    benchmark::DoNotOptimize(hits);
}
BENCHMARK(BM_BitSetContains)->Arg(1024)->Arg(65536);

static void BM_VectorBoolRead(benchmark::State& state) {
    const std::size_t bits = static_cast<std::size_t>(state.range(0));
    std::vector<bool> set(bits, true);
    std::size_t hits = 0;
    for (auto _ : state) {
        for (std::size_t index = 0; index < bits; ++index) {
            hits += set[index] ? 1 : 0;
        }
    }
    benchmark::DoNotOptimize(hits);
}
BENCHMARK(BM_VectorBoolRead)->Arg(1024)->Arg(65536);

static void BM_BitSetCount(benchmark::State& state) {
    const std::size_t bits = static_cast<std::size_t>(state.range(0));
    containers::BitSet set(bits, true);
    for (auto _ : state) {
        std::size_t count = set.count();
        benchmark::DoNotOptimize(count);
    }
}
BENCHMARK(BM_BitSetCount)->Arg(1024)->Arg(65536);

static void BM_VectorBoolCount(benchmark::State& state) {
    const std::size_t bits = static_cast<std::size_t>(state.range(0));
    std::vector<bool> set(bits, true);
    for (auto _ : state) {
        std::size_t count = static_cast<std::size_t>(std::count(set.begin(), set.end(), true));
        benchmark::DoNotOptimize(count);
    }
}
BENCHMARK(BM_VectorBoolCount)->Arg(1024)->Arg(65536);
