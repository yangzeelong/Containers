#include <benchmark/benchmark.h>

#include <containers/Bytes.hpp>
#include <containers/Message.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace {

containers::Bytes MakePayload(std::int64_t size) {
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(size), 0x5a);
    return containers::Bytes::FromVector(std::move(buffer));
}

void BM_MessageBytesCopy(benchmark::State& state) {
    const containers::Bytes payload = MakePayload(state.range(0));
    const containers::Message source(payload);

    for (auto _ : state) {
        containers::Message copy = source;
        benchmark::DoNotOptimize(copy);
    }
}

void BM_MessageBytesBorrow(benchmark::State& state) {
    const containers::Bytes payload = MakePayload(state.range(0));
    const containers::Message source(payload);

    for (auto _ : state) {
        const containers::Bytes& borrowed = source.Borrow<containers::Bytes>();
        const containers::Bytes* raw = &borrowed;
        benchmark::DoNotOptimize(raw);
    }
}

} // namespace

BENCHMARK(BM_MessageBytesCopy)->RangeMultiplier(4)->Range(64, 65536);
BENCHMARK(BM_MessageBytesBorrow)->RangeMultiplier(4)->Range(64, 65536);
