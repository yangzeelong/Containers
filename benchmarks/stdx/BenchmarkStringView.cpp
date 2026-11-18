#include "containers/stdx/StringView.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <string>

namespace {

std::string MakeText(std::size_t size, char fill) { return std::string(size, fill); }

}  // namespace

static void BM_StringViewCompare(benchmark::State& state) {
    const std::string left = MakeText(static_cast<std::size_t>(state.range(0)), 'x');
    std::string right = left;
    right.back() = 'y';
    const containers::StringView leftView(left);
    const containers::StringView rightView(right);

    for (auto _ : state) {
        int result = leftView.compare(rightView);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_StringViewCompare)->Arg(16)->Arg(256);

static void BM_StdStringCompare(benchmark::State& state) {
    const std::string left = MakeText(static_cast<std::size_t>(state.range(0)), 'x');
    std::string right = left;
    right.back() = 'y';

    for (auto _ : state) {
        int result = left.compare(right);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_StdStringCompare)->Arg(16)->Arg(256);

static void BM_StringViewFindChar(benchmark::State& state) {
    const std::string text = MakeText(static_cast<std::size_t>(state.range(0)), 'x');
    const containers::StringView view(text);

    for (auto _ : state) {
        std::size_t result = view.find('x', 8);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_StringViewFindChar)->Arg(16)->Arg(256);

static void BM_StdStringFindChar(benchmark::State& state) {
    const std::string text = MakeText(static_cast<std::size_t>(state.range(0)), 'x');

    for (auto _ : state) {
        std::size_t result = text.find('x', 8);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_StdStringFindChar)->Arg(16)->Arg(256);

static void BM_StringViewSubstr(benchmark::State& state) {
    const std::string text = MakeText(static_cast<std::size_t>(state.range(0)), 'x');
    const containers::StringView view(text);

    for (auto _ : state) {
        containers::StringView result = view.substr(1, view.size() - 1);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_StringViewSubstr)->Arg(16)->Arg(256);

static void BM_StdStringSubstr(benchmark::State& state) {
    const std::string text = MakeText(static_cast<std::size_t>(state.range(0)), 'x');

    for (auto _ : state) {
        std::string result = text.substr(1, text.size() - 1);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_StdStringSubstr)->Arg(16)->Arg(256);

static void BM_StringViewConstructFromString(benchmark::State& state) {
    const std::string text = MakeText(static_cast<std::size_t>(state.range(0)), 'x');

    for (auto _ : state) {
        containers::StringView result(text);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_StringViewConstructFromString)->Arg(16)->Arg(256);

static void BM_StdStringCopy(benchmark::State& state) {
    const std::string text = MakeText(static_cast<std::size_t>(state.range(0)), 'x');

    for (auto _ : state) {
        std::string result(text);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_StdStringCopy)->Arg(16)->Arg(256);
