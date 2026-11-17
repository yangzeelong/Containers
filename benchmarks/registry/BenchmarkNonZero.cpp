#include <benchmark/benchmark.h>

#include <containers/stdx/NonZero.hpp>

#include <cstdint>

namespace {

void BM_NonZeroTryNew(benchmark::State& state) {
    std::uint32_t seed = 1;
    for (auto _ : state) {
        auto value = containers::NonZeroU32::TryNew(++seed);
        benchmark::DoNotOptimize(value);
    }
}

void BM_NonZeroGet(benchmark::State& state) {
    const containers::NonZeroU32 value(7);
    for (auto _ : state) {
        std::uint32_t raw = value.get();
        benchmark::DoNotOptimize(raw);
    }
}

} // namespace

BENCHMARK(BM_NonZeroTryNew);
BENCHMARK(BM_NonZeroGet);
