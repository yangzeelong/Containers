#include <benchmark/benchmark.h>

#include <containers/AtomicSharedPtr.hpp>

#include <memory>

namespace {

void BM_AtomicSharedPtrLoad(benchmark::State& state) {
    containers::AtomicSharedPtr<int> atom(std::make_shared<int>(1));

    for (auto _ : state) {
        auto value = atom.load();
        benchmark::DoNotOptimize(value);
    }
}

void BM_AtomicSharedPtrStore(benchmark::State& state) {
    containers::AtomicSharedPtr<int> atom;
    std::shared_ptr<int> value = std::make_shared<int>(1);

    for (auto _ : state) {
        atom.store(value);
    }
}

void BM_AtomicSharedPtrCompareExchange(benchmark::State& state) {
    containers::AtomicSharedPtr<int> atom(std::make_shared<int>(1));
    std::shared_ptr<int> desired = std::make_shared<int>(2);

    for (auto _ : state) {
        auto expected = atom.load();
        atom.compareExchange(expected, desired);
    }
}

} // namespace

BENCHMARK(BM_AtomicSharedPtrLoad);
BENCHMARK(BM_AtomicSharedPtrStore);
BENCHMARK(BM_AtomicSharedPtrCompareExchange);
