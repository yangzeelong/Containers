#include "containers/concurrency/Thread.hpp"

#include <benchmark/benchmark.h>

#include <thread>

static void BM_ThreadStartJoin(benchmark::State& state) {
    for (auto _ : state) {
        containers::Thread worker([] {});
        worker.join();
    }
}
BENCHMARK(BM_ThreadStartJoin);

static void BM_StdThreadStartJoin(benchmark::State& state) {
    for (auto _ : state) {
        std::thread worker([] {});
        worker.join();
    }
}
BENCHMARK(BM_StdThreadStartJoin);

static void BM_ScopedThreadStartJoin(benchmark::State& state) {
    for (auto _ : state) {
        containers::ScopedThread worker([] {});
    }
}
BENCHMARK(BM_ScopedThreadStartJoin);

static void BM_ThreadHardwareConcurrency(benchmark::State& state) {
    for (auto _ : state) {
        unsigned count = containers::Thread::hardwareConcurrency();
        benchmark::DoNotOptimize(count);
    }
}
BENCHMARK(BM_ThreadHardwareConcurrency);
