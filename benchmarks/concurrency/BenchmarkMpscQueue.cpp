#include "containers/concurrency/ConcurrentQueue.hpp"
#include "containers/concurrency/MpscQueue.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <thread>
#include <vector>

namespace {

void ConsumeMpsc(containers::MpscQueue<int>& queue, int items, int& last) {
    for (int index = 0; index < items; ++index) {
        while (!queue.tryPop(last)) {
            std::this_thread::yield();
        }
    }
}

}  // namespace

static void BM_MpscQueueOneProducer(benchmark::State& state) {
    const int items = static_cast<int>(state.range(0));
    for (auto _ : state) {
        containers::MpscQueue<int> queue(static_cast<std::size_t>(items));
        std::thread producer([&queue, items] {
            for (int index = 0; index < items; ++index) {
                queue.push(index);
            }
        });
        int value = 0;
        ConsumeMpsc(queue, items, value);
        producer.join();
        benchmark::DoNotOptimize(value);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * items);
}
BENCHMARK(BM_MpscQueueOneProducer)->Arg(4096)->Arg(65536);

static void BM_ConcurrentQueueOneProducer(benchmark::State& state) {
    const int items = static_cast<int>(state.range(0));
    for (auto _ : state) {
        ConcurrentQueue<int> queue(items);
        std::thread producer([&queue, items] {
            for (int index = 0; index < items; ++index) {
                queue.push(index);
            }
        });
        int value = 0;
        for (int index = 0; index < items; ++index) {
            while (!queue.tryPop(value)) {
                std::this_thread::yield();
            }
        }
        producer.join();
        benchmark::DoNotOptimize(value);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * items);
}
BENCHMARK(BM_ConcurrentQueueOneProducer)->Arg(4096)->Arg(65536);

static void BM_MpscQueueMultiProducer(benchmark::State& state) {
    const int totalItems = static_cast<int>(state.range(0));
    const int producerCount = 4;
    const int perProducer = totalItems / producerCount;
    for (auto _ : state) {
        containers::MpscQueue<int> queue(static_cast<std::size_t>(totalItems));
        std::vector<std::thread> producers;
        for (int producer = 0; producer < producerCount; ++producer) {
            producers.emplace_back([&queue, producer, perProducer] {
                for (int index = 0; index < perProducer; ++index) {
                    queue.push(producer * perProducer + index);
                }
            });
        }
        int value = 0;
        ConsumeMpsc(queue, totalItems, value);
        for (std::thread& producer : producers) {
            producer.join();
        }
        benchmark::DoNotOptimize(value);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * totalItems);
}
BENCHMARK(BM_MpscQueueMultiProducer)->Arg(4096)->Arg(65536);

static void BM_ConcurrentQueueMultiProducer(benchmark::State& state) {
    const int totalItems = static_cast<int>(state.range(0));
    const int producerCount = 4;
    const int perProducer = totalItems / producerCount;
    for (auto _ : state) {
        ConcurrentQueue<int> queue(totalItems);
        std::vector<std::thread> producers;
        for (int producer = 0; producer < producerCount; ++producer) {
            producers.emplace_back([&queue, producer, perProducer] {
                for (int index = 0; index < perProducer; ++index) {
                    queue.push(producer * perProducer + index);
                }
            });
        }
        int value = 0;
        for (int index = 0; index < totalItems; ++index) {
            while (!queue.tryPop(value)) {
                std::this_thread::yield();
            }
        }
        for (std::thread& producer : producers) {
            producer.join();
        }
        benchmark::DoNotOptimize(value);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * totalItems);
}
BENCHMARK(BM_ConcurrentQueueMultiProducer)->Arg(4096)->Arg(65536);
