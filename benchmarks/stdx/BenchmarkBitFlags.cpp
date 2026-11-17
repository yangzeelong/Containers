#include <benchmark/benchmark.h>

#include <containers/stdx/BitFlags.hpp>

#include <cstdint>

namespace {

enum class Stage : std::uint32_t {
    Parse = 1u << 0,
    Execute = 1u << 1,
    Serialize = 1u << 2,
};

using Stages = containers::BitFlags<Stage>;

void BM_BitFlagsContains(benchmark::State& state) {
    const Stages flags = Stages::Of(Stage::Parse) | Stages::Of(Stage::Serialize);

    for (auto _ : state) {
        bool contained = flags.contains(Stage::Parse);
        benchmark::DoNotOptimize(contained);
    }
}

void BM_BitFlagsCombine(benchmark::State& state) {
    for (auto _ : state) {
        Stages flags = Stages::Of(Stage::Parse) | Stages::Of(Stage::Execute);
        flags.insert(Stage::Serialize);
        benchmark::DoNotOptimize(flags);
    }
}

} // namespace

BENCHMARK(BM_BitFlagsContains);
BENCHMARK(BM_BitFlagsCombine);
