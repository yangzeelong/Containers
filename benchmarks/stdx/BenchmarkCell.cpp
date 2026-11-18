#include "containers/stdx/Cell.hpp"

#include <benchmark/benchmark.h>

#include <string>

static void BM_CellGet(benchmark::State& state) {
    const containers::Cell<int> cell(static_cast<int>(state.range(0)));
    for (auto _ : state) {
        int value = cell.get();
        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_CellGet)->Arg(0)->Arg(42);

static void BM_PlainGet(benchmark::State& state) {
    const int plain = static_cast<int>(state.range(0));
    for (auto _ : state) {
        int value = plain;
        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_PlainGet)->Arg(0)->Arg(42);

static void BM_CellSet(benchmark::State& state) {
    containers::Cell<int> cell(0);
    for (auto _ : state) {
        cell.set(static_cast<int>(state.range(0)));
        benchmark::DoNotOptimize(cell);
    }
}
BENCHMARK(BM_CellSet)->Arg(1)->Arg(42);

static void BM_PlainSet(benchmark::State& state) {
    int plain = 0;
    for (auto _ : state) {
        plain = static_cast<int>(state.range(0));
        benchmark::DoNotOptimize(plain);
    }
}
BENCHMARK(BM_PlainSet)->Arg(1)->Arg(42);

static void BM_RefCellBorrowMut(benchmark::State& state) {
    containers::RefCell<std::string> cell(std::string("benchmark"));
    for (auto _ : state) {
        const auto mutableBorrow = cell.borrowMut();
        mutableBorrow->append("x");
        mutableBorrow->pop_back();
    }
}
BENCHMARK(BM_RefCellBorrowMut);

static void BM_DirectStringMutation(benchmark::State& state) {
    std::string text("benchmark");
    for (auto _ : state) {
        text.append("x");
        text.pop_back();
    }
}
BENCHMARK(BM_DirectStringMutation);
