#include <benchmark/benchmark.h>

#include <containers/OnceCell.hpp>

namespace {

void BM_OnceCellGet(benchmark::State& state) {
    containers::OnceCell<int> cell;
    cell.getOrInit([] { return 1; });

    for (auto _ : state) {
        benchmark::DoNotOptimize(cell.getOrInit([] { return 2; }));
    }
}

void BM_LazyGet(benchmark::State& state) {
    containers::Lazy<int> lazy([] { return 1; });
    benchmark::DoNotOptimize(lazy.get());

    for (auto _ : state) {
        benchmark::DoNotOptimize(lazy.get());
    }
}

} // namespace

BENCHMARK(BM_OnceCellGet);
BENCHMARK(BM_LazyGet);
