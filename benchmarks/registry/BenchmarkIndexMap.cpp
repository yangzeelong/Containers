#include <benchmark/benchmark.h>

#include <containers/collections/IndexMap.hpp>

#include <unordered_map>

namespace {

constexpr int kEntries = 128;

void BM_IndexMapInsertLookup(benchmark::State& state) {
    for (auto _ : state) {
        containers::IndexMap<int, int> map;
        map.reserve(kEntries);
        for (int key = 0; key < kEntries; ++key) {
            map.insert(key, key * 2);
        }

        int sum = 0;
        for (int key = 0; key < kEntries; ++key) {
            sum += *map.find(key);
        }
        benchmark::DoNotOptimize(sum);
    }
}

void BM_UnorderedMapInsertLookup(benchmark::State& state) {
    for (auto _ : state) {
        std::unordered_map<int, int> map;
        map.reserve(kEntries);
        for (int key = 0; key < kEntries; ++key) {
            map.emplace(key, key * 2);
        }

        int sum = 0;
        for (int key = 0; key < kEntries; ++key) {
            sum += map.at(key);
        }
        benchmark::DoNotOptimize(sum);
    }
}

void BM_IndexMapIterate(benchmark::State& state) {
    containers::IndexMap<int, int> map;
    for (int key = 0; key < kEntries; ++key) {
        map.insert(key, key);
    }

    for (auto _ : state) {
        int sum = 0;
        for (const auto& entry : map) {
            sum += entry.second;
        }
        benchmark::DoNotOptimize(sum);
    }
}

} // namespace

BENCHMARK(BM_IndexMapInsertLookup);
BENCHMARK(BM_UnorderedMapInsertLookup);
BENCHMARK(BM_IndexMapIterate);
