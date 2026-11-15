#include <benchmark/benchmark.h>

#include <containers/SlotMap.hpp>

#include <vector>

namespace {

constexpr int kEntries = 1024;

void BM_SlotMapInsertGet(benchmark::State& state) {
    for (auto _ : state) {
        containers::SlotMap<int> map;
        std::vector<containers::SlotKey> keys;
        keys.reserve(kEntries);

        for (int value = 0; value < kEntries; ++value) {
            keys.push_back(map.insert(value));
        }

        int sum = 0;
        for (const auto& key : keys) {
            sum += *map.get(key);
        }
        benchmark::DoNotOptimize(sum);
    }
}

void BM_SlotMapInsertErase(benchmark::State& state) {
    for (auto _ : state) {
        containers::SlotMap<int> map;
        std::vector<containers::SlotKey> keys;
        keys.reserve(kEntries);

        for (int value = 0; value < kEntries; ++value) {
            keys.push_back(map.insert(value));
        }
        for (const auto& key : keys) {
            map.erase(key);
        }
        benchmark::DoNotOptimize(map);
    }
}

} // namespace

BENCHMARK(BM_SlotMapInsertGet);
BENCHMARK(BM_SlotMapInsertErase);
