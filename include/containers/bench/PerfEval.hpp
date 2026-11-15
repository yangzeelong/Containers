#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace containers {
namespace bench {

// A tiny, dependency-free performance evaluation harness. It is deliberately
// separate from Google Benchmark:
//   * Google Benchmark answers "how fast is this one thing" with tight
//     statistics and a stable CLI;
//   * this harness answers "does the candidate meet its target" by measuring
//     named cases over several rounds, comparing them against absolute limits
//     or against another case in the *same* run (so the check survives
//     machine-to-machine variation), and emitting a Markdown report plus a
//     non-zero exit status on regressions.
//
// Cases must have observable side effects; the harness never talks to the
// optimizer, so an empty body may be optimized away.

struct PerfStats {
    std::size_t rounds = 0;
    double mean = 0.0;   // ns per operation
    double median = 0.0;
    double p95 = 0.0;
    double min = 0.0;
    double max = 0.0;
    double stddev = 0.0;
    double cvPercent = 0.0; // stddev / mean * 100

    static PerfStats FromSamples(std::vector<double> samples);
};

// Linear-interpolated percentile over an already sorted sample vector.
inline double Percentile(const std::vector<double>& sorted, double fraction) {
    if (sorted.empty()) {
        return 0.0;
    }
    if (fraction <= 0.0) {
        return sorted.front();
    }
    if (fraction >= 1.0) {
        return sorted.back();
    }
    const double position = fraction * static_cast<double>(sorted.size() - 1);
    const std::size_t lower = static_cast<std::size_t>(std::floor(position));
    const std::size_t upper = std::min(lower + 1, sorted.size() - 1);
    const double weight = position - static_cast<double>(lower);
    return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
}

inline PerfStats PerfStats::FromSamples(std::vector<double> samples) {
    PerfStats stats;
    if (samples.empty()) {
        return stats;
    }

    std::sort(samples.begin(), samples.end());
    stats.rounds = samples.size();
    stats.min = samples.front();
    stats.max = samples.back();
    stats.median = Percentile(samples, 0.5);
    stats.p95 = Percentile(samples, 0.95);

    double sum = 0.0;
    for (double sample : samples) {
        sum += sample;
    }
    stats.mean = sum / static_cast<double>(samples.size());

    double variance = 0.0;
    for (double sample : samples) {
        const double delta = sample - stats.mean;
        variance += delta * delta;
    }
    variance /= static_cast<double>(samples.size());
    stats.stddev = std::sqrt(variance);
    stats.cvPercent = stats.mean != 0.0 ? stats.stddev / stats.mean * 100.0 : 0.0;
    return stats;
}

struct PerfOptions {
    double baselineNs = 0.0;        // absolute target, disabled when <= 0
    std::string reference;          // case to compare against in the same run
    double tolerancePercent = 0.0;  // 0 means "use the evaluator default"
};

struct PerfCase {
    std::string name;
    std::function<void()> body;
    PerfOptions options;
};

struct PerfResult {
    std::string name;
    PerfStats stats;
    bool hasBaseline = false;
    double baselineNs = 0.0;
    bool hasReference = false;
    std::string reference;
    double referenceMeanNs = 0.0;
    double ratio = 0.0;
    double tolerancePercent = 0.0;
    bool passed = true;
};

class PerfEvaluator {
public:
    explicit PerfEvaluator(double defaultTolerancePercent = 10.0)
        : defaultTolerance_(defaultTolerancePercent > 0.0 ? defaultTolerancePercent : 0.0) {}

    void AddCase(std::string name, std::function<void()> body, PerfOptions options = PerfOptions()) {
        assert(!name.empty());
        assert(static_cast<bool>(body));
        cases_.push_back(PerfCase{std::move(name), std::move(body), std::move(options)});
    }

    const std::vector<PerfCase>& Cases() const noexcept { return cases_; }

    // Runs every case: `warmup` untimed rounds, then `rounds` timed rounds of
    // `iterations` calls each. The sample for a round is the mean ns/op.
    std::vector<PerfResult> Run(std::size_t rounds, std::size_t iterations,
                                std::size_t warmup = 0) const {
        assert(rounds > 0);
        assert(iterations > 0);

        std::vector<PerfResult> results;
        results.reserve(cases_.size());

        for (const PerfCase& current : cases_) {
            for (std::size_t warm = 0; warm < warmup; ++warm) {
                for (std::size_t index = 0; index < iterations; ++index) {
                    current.body();
                }
            }

            std::vector<double> samples;
            samples.reserve(rounds);
            for (std::size_t round = 0; round < rounds; ++round) {
                samples.push_back(Measure(current.body, iterations));
            }

            PerfResult result;
            result.name = current.name;
            result.stats = PerfStats::FromSamples(std::move(samples));
            result.hasBaseline = current.options.baselineNs > 0.0;
            result.baselineNs = current.options.baselineNs;
            result.hasReference = !current.options.reference.empty();
            result.reference = current.options.reference;
            result.tolerancePercent =
                current.options.tolerancePercent > 0.0 ? current.options.tolerancePercent : defaultTolerance_;
            results.push_back(std::move(result));
        }

        for (PerfResult& result : results) {
            if (result.hasReference) {
                const PerfResult* reference = FindByName(results, result.reference);
                if (reference == nullptr) {
                    result.passed = false;
                } else {
                    result.referenceMeanNs = reference->stats.mean;
                    result.ratio = reference->stats.mean > 0.0
                                       ? result.stats.mean / reference->stats.mean
                                       : 0.0;
                    result.passed = WithinTarget(result.stats.mean, reference->stats.mean,
                                                 result.tolerancePercent);
                }
            }
            if (result.hasBaseline) {
                result.passed = result.passed &&
                                WithinTarget(result.stats.mean, result.baselineNs, result.tolerancePercent);
            }
        }
        return results;
    }

    static bool WithinTarget(double measured, double target, double tolerancePercent) {
        if (target <= 0.0) {
            return true;
        }
        return measured <= target * (1.0 + tolerancePercent / 100.0);
    }

    static std::size_t FailedCount(const std::vector<PerfResult>& results) {
        std::size_t failed = 0;
        for (const PerfResult& result : results) {
            if (!result.passed) {
                ++failed;
            }
        }
        return failed;
    }

    static bool AllPassed(const std::vector<PerfResult>& results) {
        return FailedCount(results) == 0;
    }

    static std::string ToMarkdown(const std::vector<PerfResult>& results) {
        std::ostringstream report;
        report << "| Case | mean (ns) | median (ns) | p95 (ns) | min (ns) | max (ns) | CV % | "
                  "reference | baseline (ns) | verdict |\n";
        report << "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n";
        for (const PerfResult& result : results) {
            report << "| " << result.name << " | " << FormatNumber(result.stats.mean) << " | "
                   << FormatNumber(result.stats.median) << " | " << FormatNumber(result.stats.p95)
                   << " | " << FormatNumber(result.stats.min) << " | "
                   << FormatNumber(result.stats.max) << " | "
                   << FormatNumber(result.stats.cvPercent) << " | ";
            if (result.hasReference) {
                report << FormatNumber(result.ratio) << "x " << result.reference;
            } else {
                report << "-";
            }
            report << " | ";
            if (result.hasBaseline) {
                report << FormatNumber(result.baselineNs);
            } else {
                report << "-";
            }
            report << " | " << (result.passed ? "PASS" : "FAIL") << " |\n";
        }
        return report.str();
    }

private:
    static double Measure(const std::function<void()>& body, std::size_t iterations) {
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t index = 0; index < iterations; ++index) {
            body();
        }
        const auto end = std::chrono::steady_clock::now();
        const double nanos = std::chrono::duration<double, std::nano>(end - start).count();
        return nanos / static_cast<double>(iterations);
    }

    static const PerfResult* FindByName(const std::vector<PerfResult>& results,
                                        const std::string& name) {
        for (const PerfResult& result : results) {
            if (result.name == name) {
                return &result;
            }
        }
        return nullptr;
    }

    static std::string FormatNumber(double value) {
        std::ostringstream text;
        text << std::fixed << std::setprecision(3) << value;
        return text.str();
    }

    double defaultTolerance_;
    std::vector<PerfCase> cases_;
};

// Keeps a computed address observable so trivial bodies are not optimized away.
// Real cases usually mutate a container, which is a stronger barrier already.
inline void KeepAlive(const void* pointer) {
#if defined(_MSC_VER)
    volatile const void* sink = pointer;
    (void)sink;
#else
    asm volatile("" : : "g"(pointer) : "memory");
#endif
}

} // namespace bench
} // namespace containers
