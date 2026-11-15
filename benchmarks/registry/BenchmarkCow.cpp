#include <benchmark/benchmark.h>

#include <containers/Cow.hpp>

#include <cstddef>
#include <vector>

namespace {

void BM_CowBorrowedRead(benchmark::State& state) {
    const std::vector<int> payload(static_cast<std::size_t>(state.range(0)), 1);
    for (auto _ : state) {
        auto cow = containers::Cow<std::vector<int>>::Borrowed(payload);
        benchmark::DoNotOptimize(cow.get());
    }
}

void BM_CowCopyShared(benchmark::State& state) {
    const auto cow = containers::Cow<std::vector<int>>::Owned(
        std::vector<int>(static_cast<std::size_t>(state.range(0)), 1));
    for (auto _ : state) {
        auto copy = cow;
        benchmark::DoNotOptimize(copy.get());
    }
}

void BM_CowToMut(benchmark::State& state) {
    const std::vector<int> payload(static_cast<std::size_t>(state.range(0)), 1);
    for (auto _ : state) {
        auto cow = containers::Cow<std::vector<int>>::Borrowed(payload);
        auto& mutated = cow.toMut();
        benchmark::DoNotOptimize(mutated);
    }
}

} // namespace

BENCHMARK(BM_CowBorrowedRead)->RangeMultiplier(4)->Range(16, 2048);
BENCHMARK(BM_CowCopyShared)->RangeMultiplier(4)->Range(16, 2048);
BENCHMARK(BM_CowToMut)->RangeMultiplier(4)->Range(16, 2048);
