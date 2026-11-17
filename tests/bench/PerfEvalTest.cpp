#include <containers/bench/PerfEval.hpp>

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

TEST(PerfEvalTest, StatsFromOddSamples) {
    std::vector<double> samples{5.0, 1.0, 3.0, 2.0, 4.0};

    const auto stats = containers::bench::PerfStats::FromSamples(std::move(samples));

    EXPECT_EQ(5u, stats.rounds);
    EXPECT_DOUBLE_EQ(3.0, stats.mean);
    EXPECT_DOUBLE_EQ(3.0, stats.median);
    EXPECT_DOUBLE_EQ(4.8, stats.p95);
    EXPECT_DOUBLE_EQ(1.0, stats.min);
    EXPECT_DOUBLE_EQ(5.0, stats.max);
    EXPECT_NEAR(1.414213562, stats.stddev, 1e-9);
    EXPECT_NEAR(47.140452, stats.cvPercent, 1e-6);
}

TEST(PerfEvalTest, StatsFromEvenSamples) {
    std::vector<double> samples{8.0, 2.0, 6.0, 4.0};

    const auto stats = containers::bench::PerfStats::FromSamples(std::move(samples));

    EXPECT_DOUBLE_EQ(5.0, stats.mean);
    EXPECT_DOUBLE_EQ(5.0, stats.median);
    EXPECT_DOUBLE_EQ(7.7, stats.p95);
}

TEST(PerfEvalTest, EmptySamplesStayZero) {
    const auto stats = containers::bench::PerfStats::FromSamples({});

    EXPECT_EQ(0u, stats.rounds);
    EXPECT_DOUBLE_EQ(0.0, stats.mean);
    EXPECT_DOUBLE_EQ(0.0, stats.median);
    EXPECT_DOUBLE_EQ(0.0, stats.p95);
}

TEST(PerfEvalTest, WithinTargetHonoursTolerance) {
    EXPECT_TRUE(containers::bench::PerfEvaluator::WithinTarget(105.0, 100.0, 5.0));
    EXPECT_FALSE(containers::bench::PerfEvaluator::WithinTarget(106.0, 100.0, 5.0));
    EXPECT_TRUE(containers::bench::PerfEvaluator::WithinTarget(80.0, 100.0, 0.0));
    EXPECT_TRUE(containers::bench::PerfEvaluator::WithinTarget(1000.0, 0.0, -5.0));
}

TEST(PerfEvalTest, RunMeasuresEveryCase) {
    containers::bench::PerfEvaluator evaluator(50.0);
    evaluator.AddCase("reference_work", [] {
        volatile int counter = 0;
        for (int step = 0; step < 16; ++step) {
            counter = counter + step;
        }
        (void)counter;
    });
    evaluator.AddCase(
        "candidate_work",
        [] {
            volatile int counter = 1;
            for (int step = 0; step < 16; ++step) {
                counter = counter + step;
            }
            (void)counter;
        },
        {0.0, "reference_work", 50.0});

    const auto results = evaluator.Run(3, 50, 1);

    ASSERT_EQ(2u, results.size());
    EXPECT_EQ("reference_work", results[0].name);
    EXPECT_EQ(3u, results[0].stats.rounds);
    EXPECT_GT(results[0].stats.mean, 0.0);
    EXPECT_TRUE(results[1].hasReference);
    EXPECT_EQ("reference_work", results[1].reference);
    EXPECT_GT(results[1].ratio, 0.0);

    const std::string report = containers::bench::PerfEvaluator::ToMarkdown(results);
    EXPECT_NE(std::string::npos, report.find("| Case |"));
    EXPECT_NE(std::string::npos, report.find("candidate_work"));
    EXPECT_NE(std::string::npos, report.find("PASS"));
}

TEST(PerfEvalTest, MissingReferenceFailsTheCase) {
    containers::bench::PerfEvaluator evaluator;
    evaluator.AddCase("candidate", [] { volatile int value = 1; (void)value; },
                      {0.0, "not_registered", 10.0});

    const auto results = evaluator.Run(2, 10);

    ASSERT_EQ(1u, results.size());
    EXPECT_TRUE(results[0].hasReference);
    EXPECT_FALSE(results[0].passed);
    EXPECT_EQ(1u, containers::bench::PerfEvaluator::FailedCount(results));
    EXPECT_FALSE(containers::bench::PerfEvaluator::AllPassed(results));
}
