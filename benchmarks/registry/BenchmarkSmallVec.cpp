#include <benchmark/benchmark.h>

#include <containers/collections/SmallVec.hpp>

#include <vector>

namespace {

void BM_SmallVecInlinePush(benchmark::State& state) {
    for (auto _ : state) {
        containers::SmallVec<int, 16> values;
        for (int value = 0; value < 16; ++value) {
            values.push_back(value);
        }
        benchmark::DoNotOptimize(values);
    }
}

void BM_SmallVecSpillPush(benchmark::State& state) {
    for (auto _ : state) {
        containers::SmallVec<int, 4> values;
        for (int value = 0; value < 64; ++value) {
            values.push_back(value);
        }
        benchmark::DoNotOptimize(values);
    }
}

void BM_VectorPush(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<int> values;
        values.reserve(16);
        for (int value = 0; value < 16; ++value) {
            values.push_back(value);
        }
        benchmark::DoNotOptimize(values);
    }
}

} // namespace

BENCHMARK(BM_SmallVecInlinePush);
BENCHMARK(BM_SmallVecSpillPush);
BENCHMARK(BM_VectorPush);
