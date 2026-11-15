// Copyright (c) 2026 yangzeelong
//
// Micro benchmarks for the manifest hot paths: parse+validate of a generated
// N-service document and the launch-order topological sort.
#include "containers/engine.hpp"

#include <benchmark/benchmark.h>

#include <sstream>
#include <string>
#include <vector>

namespace {

/// Build a valid manifest with `n` services. Each service depends on the
/// previous one every 8th service, producing a moderately wide DAG.
std::string make_manifest(int n) {
    std::ostringstream os;
    os << "version: \"3.9\"\nservices:\n";
    for (int i = 0; i < n; ++i) {
        os << "  svc" << i << ":\n"
           << "    image: demo/svc" << i << ":1.0.0\n"
           << "    environment:\n"
           << "      - ROLE=worker\n"
           << "      - ID=" << i << "\n"
           << "    ports: [\"" << (10000 + i) << ":8080\"]\n"
           << "    networks: [net]\n";
        if (i >= 8) os << "    depends_on: [svc" << (i - 8) << "]\n";
    }
    os << "networks:\n  net: {}\n";
    return os.str();
}

void BM_ParseAndValidate(benchmark::State& state) {
    const std::string text = make_manifest(static_cast<int>(state.range(0)));
    for (auto _ : state) {
        std::vector<containers::Issue> issues;
        containers::Manifest m = containers::load_manifest_string(text, &issues);
        if (!containers::Manifest::is_ok(issues)) {
            state.SkipWithError(issues.front().message.c_str());
            return;
        }
        auto count = m.services().size();
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(text.size()));
}
BENCHMARK(BM_ParseAndValidate)->Arg(8)->Arg(64)->Arg(256);

void BM_LaunchOrder(benchmark::State& state) {
    std::vector<containers::Issue> issues;
    containers::Manifest m =
        containers::load_manifest_string(make_manifest(static_cast<int>(state.range(0))), &issues);
    for (auto _ : state) {
        std::vector<containers::Issue> order_issues;
        const auto order = m.launch_order(&order_issues);
        auto count       = order.size();
        benchmark::DoNotOptimize(count);
    }
}
BENCHMARK(BM_LaunchOrder)->Arg(64)->Arg(256);

void BM_SplitCommand(benchmark::State& state) {
    std::string cmd;
    const int n = static_cast<int>(state.range(0));
    for (int i = 0; i < n; ++i) {
        if (i) cmd += ' ';
        cmd += (i % 5 == 0) ? "\"quoted token\"" : "token";
    }
    for (auto _ : state) {
        const auto tokens = containers::split_command(cmd);
        auto count        = tokens.size();
        benchmark::DoNotOptimize(count);
    }
}
BENCHMARK(BM_SplitCommand)->Arg(16)->Arg(128);

}    // namespace
