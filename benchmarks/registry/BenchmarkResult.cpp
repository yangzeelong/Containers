#include <benchmark/benchmark.h>

#include <containers/Result.hpp>

#include <string>

namespace {

using IntResult = containers::Result<int, std::string>;

void BM_ResultOk(benchmark::State& state) {
    for (auto _ : state) {
        auto result = IntResult::Ok(42);
        benchmark::DoNotOptimize(result.value());
    }
}

void BM_ResultErr(benchmark::State& state) {
    for (auto _ : state) {
        auto result = IntResult::Err(std::string("payload error"));
        benchmark::DoNotOptimize(result.error());
    }
}

void BM_ResultMapChain(benchmark::State& state) {
    const auto ok = IntResult::Ok(1);
    for (auto _ : state) {
        auto mapped = ok.map([](int value) { return value * 2; })
                          .map([](int value) { return value + 1; });
        benchmark::DoNotOptimize(mapped.value());
    }
}

void BM_ResultAndThenChain(benchmark::State& state) {
    const auto ok = IntResult::Ok(1);
    for (auto _ : state) {
        auto chained = ok.andThen([](int value) {
                             return IntResult::Ok(value + 1);
                         });
        benchmark::DoNotOptimize(chained.value());
    }
}

} // namespace

BENCHMARK(BM_ResultOk);
BENCHMARK(BM_ResultErr);
BENCHMARK(BM_ResultMapChain);
BENCHMARK(BM_ResultAndThenChain);
