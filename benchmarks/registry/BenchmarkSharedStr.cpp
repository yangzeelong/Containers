#include <benchmark/benchmark.h>

#include <containers/stdx/SharedStr.hpp>

#include <string>

namespace {

const char* kPayload =
    "the quick brown fox jumps over the lazy dog and keeps running through the field";

void BM_SharedStrCopy(benchmark::State& state) {
    const std::string source(kPayload);
    for (auto _ : state) {
        auto text = containers::SharedStr::Copy(source);
        benchmark::DoNotOptimize(text);
    }
}

void BM_SharedStrClone(benchmark::State& state) {
    const auto text = containers::SharedStr::From(kPayload);
    for (auto _ : state) {
        auto clone = text.clone();
        benchmark::DoNotOptimize(clone);
    }
}

void BM_SharedStrSlice(benchmark::State& state) {
    const auto text = containers::SharedStr::From(kPayload);
    for (auto _ : state) {
        auto slice = text.slice(4, 19);
        benchmark::DoNotOptimize(slice);
    }
}

void BM_SharedStrCompare(benchmark::State& state) {
    const auto text = containers::SharedStr::From(kPayload);
    const auto other = containers::SharedStr::From(kPayload);
    for (auto _ : state) {
        bool equal = text == other;
        benchmark::DoNotOptimize(equal);
    }
}

} // namespace

BENCHMARK(BM_SharedStrCopy);
BENCHMARK(BM_SharedStrClone);
BENCHMARK(BM_SharedStrSlice);
BENCHMARK(BM_SharedStrCompare);
