#include <benchmark/benchmark.h>

#include <containers/Bytes.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> MakePayload(std::int64_t size) {
    return std::vector<std::uint8_t>(static_cast<std::size_t>(size), 0x5a);
}

void BM_BytesCopy(benchmark::State& state) {
    const auto payload = MakePayload(state.range(0));
    for (auto _ : state) {
        auto bytes = containers::Bytes::Copy(payload.data(), payload.size());
        benchmark::DoNotOptimize(bytes);
    }
}

void BM_BytesClone(benchmark::State& state) {
    const auto payload = MakePayload(state.range(0));
    const auto bytes = containers::Bytes::Copy(payload.data(), payload.size());
    for (auto _ : state) {
        auto clone = bytes.clone();
        benchmark::DoNotOptimize(clone);
    }
}

void BM_BytesSlice(benchmark::State& state) {
    const auto payload = MakePayload(state.range(0));
    const auto bytes = containers::Bytes::Copy(payload.data(), payload.size());
    const std::size_t half = bytes.size() / 2;
    for (auto _ : state) {
        auto slice = bytes.slice(half / 2, half);
        benchmark::DoNotOptimize(slice);
    }
}

void BM_BytesMutFreeze(benchmark::State& state) {
    const auto payload = MakePayload(state.range(0));
    for (auto _ : state) {
        containers::BytesMut buffer = containers::BytesMut::FromVector(payload);
        auto frozen = buffer.freeze();
        benchmark::DoNotOptimize(frozen);
    }
}

} // namespace

BENCHMARK(BM_BytesCopy)->RangeMultiplier(4)->Range(64, 65536);
BENCHMARK(BM_BytesClone)->RangeMultiplier(4)->Range(64, 65536);
BENCHMARK(BM_BytesSlice)->RangeMultiplier(4)->Range(64, 65536);
BENCHMARK(BM_BytesMutFreeze)->RangeMultiplier(4)->Range(64, 65536);
