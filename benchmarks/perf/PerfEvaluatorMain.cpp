// Standalone performance evaluation runner. Unlike the Google Benchmark suites
// (which answer "how fast is this"), this executable answers "does this build
// still meet its targets" and exits non-zero on a regression.
//
// Cases either compare against a reference case measured in the same run (so
// the verdict survives different machines) or against an absolute ns/op limit.

#include <containers/collections/Bytes.hpp>
#include <containers/stdx/Cow.hpp>
#include <containers/message/Message.hpp>
#include <containers/stdx/OnceCell.hpp>
#include <containers/stdx/SharedStr.hpp>
#include <containers/collections/SmallVec.hpp>
#include <containers/bench/PerfEval.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using containers::bench::KeepAlive;
using containers::bench::PerfEvaluator;

const containers::Bytes& PayloadBytes64K() {
    static const containers::Bytes payload = [] {
        std::vector<std::uint8_t> data(64u * 1024u, 0x5a);
        return containers::Bytes::FromVector(std::move(data));
    }();
    return payload;
}

const std::vector<std::uint8_t>& PayloadVector64K() {
    static const std::vector<std::uint8_t> payload(64u * 1024u, 0x5a);
    return payload;
}

const containers::SharedStr& SharedStrPayload() {
    static const containers::SharedStr text =
        containers::SharedStr::From("the quick brown fox jumps over the lazy dog");
    return text;
}

const std::string& StringPayload() {
    static const std::string text("the quick brown fox jumps over the lazy dog");
    return text;
}

const containers::Message& MessageBytesSource() {
    static const containers::Message message(PayloadBytes64K());
    return message;
}

void SmallVecInlinePush() {
    containers::SmallVec<int, 16> values;
    for (int value = 0; value < 16; ++value) {
        values.push_back(value);
    }
    KeepAlive(values.data());
}

void VectorReservedPush() {
    std::vector<int> values;
    values.reserve(16);
    for (int value = 0; value < 16; ++value) {
        values.push_back(value);
    }
    KeepAlive(values.data());
}

void BytesClone64K() {
    auto clone = PayloadBytes64K().clone();
    KeepAlive(clone.data());
}

void BytesSlice4K() {
    auto slice = PayloadBytes64K().slice(1024, 4096);
    KeepAlive(slice.data());
}

void VectorCopy64K() {
    auto copy = PayloadVector64K();
    KeepAlive(copy.data());
}

void SharedStrClone() {
    auto clone = SharedStrPayload().clone();
    KeepAlive(clone.data());
}

void StringCopy() {
    auto copy = StringPayload();
    KeepAlive(copy.data());
}

void MessageBytesCopy64K() {
    auto copy = MessageBytesSource();
    KeepAlive(&copy);
}

void OnceCellHotGet() {
    static containers::OnceCell<int> cell;
    KeepAlive(&cell.getOrInit([] { return 7; }));
}

void CowBorrowedRead() {
    static const std::vector<int> payload(64, 1);
    auto cow = containers::Cow<std::vector<int>>::Borrowed(payload);
    KeepAlive(&cow.get());
}

void RegisterCases(PerfEvaluator& evaluator) {
    evaluator.AddCase("vector_reserved_push", VectorReservedPush);
    evaluator.AddCase("smallvec_inline_push", SmallVecInlinePush,
                      {0.0, "vector_reserved_push", 100.0});

    evaluator.AddCase("vector_copy_64k", VectorCopy64K);
    evaluator.AddCase("bytes_clone_64k", BytesClone64K, {0.0, "vector_copy_64k", 50.0});
    evaluator.AddCase("bytes_slice_4k", BytesSlice4K);

    evaluator.AddCase("string_copy", StringCopy);
    evaluator.AddCase("sharedstr_clone", SharedStrClone, {0.0, "string_copy", 100.0});

    evaluator.AddCase("message_bytes_copy_64k", MessageBytesCopy64K);
    evaluator.AddCase("oncecell_hot_get", OnceCellHotGet, {500.0, "", 0.0});
    evaluator.AddCase("cow_borrowed_read", CowBorrowedRead);
}

struct CommandLine {
    std::size_t rounds = 15;
    std::size_t iterations = 2000;
    std::size_t warmup = 2;
    double tolerance = 10.0;
    std::string output;
    bool noCheck = false;
    bool listOnly = false;
};

void PrintUsage() {
    std::cout
        << "usage: containers_perf_eval [options]\n"
        << "  --rounds N        timed rounds per case (default 15)\n"
        << "  --iterations M    calls per timed round (default 2000)\n"
        << "  --warmup W        untimed rounds before measuring (default 2)\n"
        << "  --tolerance PCT   default regression tolerance (default 10)\n"
        << "  --output PATH     write the Markdown report to PATH\n"
        << "  --no-check        never fail the process on a missed target\n"
        << "  --list            list the registered cases and exit\n";
}

bool ParseUnsigned(const char* text, std::size_t& value) {
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }
    value = static_cast<std::size_t>(parsed);
    return true;
}

bool ParseArgs(int argc, char** argv, CommandLine& commandLine, std::string& error) {
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--rounds") {
            if (index + 1 >= argc || !ParseUnsigned(argv[++index], commandLine.rounds)) {
                error = "--rounds needs a number";
                return false;
            }
        } else if (argument == "--iterations") {
            if (index + 1 >= argc || !ParseUnsigned(argv[++index], commandLine.iterations)) {
                error = "--iterations needs a number";
                return false;
            }
        } else if (argument == "--warmup") {
            if (index + 1 >= argc || !ParseUnsigned(argv[++index], commandLine.warmup)) {
                error = "--warmup needs a number";
                return false;
            }
        } else if (argument == "--tolerance") {
            if (index + 1 >= argc) {
                error = "--tolerance needs a number";
                return false;
            }
            commandLine.tolerance = std::strtod(argv[++index], nullptr);
        } else if (argument == "--output") {
            if (index + 1 >= argc) {
                error = "--output needs a path";
                return false;
            }
            commandLine.output = argv[++index];
        } else if (argument == "--no-check") {
            commandLine.noCheck = true;
        } else if (argument == "--list") {
            commandLine.listOnly = true;
        } else if (argument == "--help" || argument == "-h") {
            PrintUsage();
            std::exit(0);
        } else {
            error = "unknown argument: " + argument;
            return false;
        }
    }

    if (commandLine.rounds == 0 || commandLine.iterations == 0) {
        error = "--rounds and --iterations must be positive";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    CommandLine commandLine;
    std::string error;
    if (!ParseArgs(argc, argv, commandLine, error)) {
        std::cerr << error << "\n\n";
        PrintUsage();
        return 2;
    }

    PerfEvaluator evaluator(commandLine.tolerance);
    RegisterCases(evaluator);

    if (commandLine.listOnly) {
        for (const auto& current : evaluator.Cases()) {
            std::cout << current.name << "\n";
        }
        return 0;
    }

    const auto results = evaluator.Run(commandLine.rounds, commandLine.iterations, commandLine.warmup);
    const std::string report = PerfEvaluator::ToMarkdown(results);

    std::cout << report;
    std::cout << "\n" << results.size() << " case(s), "
              << PerfEvaluator::FailedCount(results) << " failed, rounds="
              << commandLine.rounds << ", iterations=" << commandLine.iterations << "\n";

    if (!commandLine.output.empty()) {
        std::ofstream file(commandLine.output);
        if (!file) {
            std::cerr << "cannot write report to " << commandLine.output << "\n";
            return 2;
        }
        file << report;
    }

    if (commandLine.noCheck) {
        return 0;
    }
    return PerfEvaluator::AllPassed(results) ? 0 : 1;
}
