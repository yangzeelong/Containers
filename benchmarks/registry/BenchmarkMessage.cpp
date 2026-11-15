#include "containers/Message.hpp"
#include "containers/SpscQueue.hpp"

#include <benchmark/benchmark.h>

#include <string>
#include <vector>

namespace {

void BM_MessageConstruction(benchmark::State& state) {
    std::vector<int> payload(state.range(0));
    for (auto _ : state) {
        containers::Message message(containers::MakeMessage(payload));
        benchmark::DoNotOptimize(message);
    }
}
BENCHMARK(BM_MessageConstruction)
    ->RangeMultiplier(4)
    ->Range(1 << 4, 1 << 11)
    ->Unit(benchmark::kMicrosecond);

void BM_MessageCopy(benchmark::State& state) {
    std::vector<int> payload(state.range(0));
    containers::Message original(containers::MakeMessage(payload));
    for (auto _ : state) {
        containers::Message copy = original;
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK(BM_MessageCopy)
    ->RangeMultiplier(4)
    ->Range(1 << 4, 1 << 11)
    ->Unit(benchmark::kMicrosecond);

void BM_MessageCOWDetach(benchmark::State& state) {
    std::vector<int> payload(state.range(0));
    containers::Message original(containers::MakeMessage(payload));
    for (auto _ : state) {
        containers::Message shared = original; // cheap shared copy
        // The mutable access detaches: a deep copy happens only for `shared`.
        auto& data = shared.Mut<std::vector<int>>();
        data.back() = 0;
        benchmark::DoNotOptimize(data);
    }
}
BENCHMARK(BM_MessageCOWDetach)
    ->RangeMultiplier(4)
    ->Range(1 << 4, 1 << 11)
    ->Unit(benchmark::kMicrosecond);

void BM_MessageClone(benchmark::State& state) {
    std::vector<int> payload(state.range(0));
    containers::Message original(containers::MakeMessage(payload));
    for (auto _ : state) {
        containers::Message clone = original.Clone();
        benchmark::DoNotOptimize(clone);
    }
}
BENCHMARK(BM_MessageClone)
    ->RangeMultiplier(4)
    ->Range(1 << 4, 1 << 11)
    ->Unit(benchmark::kMicrosecond);

void BM_SpscQueueMessageTransfer(benchmark::State& state) {
    const int capacity = 8;
    containers::SpscQueue<containers::Message> queue(capacity);
    for (auto _ : state) {
        queue.push(containers::MakeMessage(std::string("payload")));
        benchmark::DoNotOptimize(queue);
        containers::Message message;
        if (queue.waitAndPopFor(message, std::chrono::milliseconds(10))) {
            benchmark::DoNotOptimize(message);
        } else {
            state.SkipWithError("queue reported empty or timed out");
            break;
        }
    }
}
BENCHMARK(BM_SpscQueueMessageTransfer)->Unit(benchmark::kMicrosecond);

} // namespace
