// Standalone performance evaluation runner. Unlike the Google Benchmark suites
// (which answer "how fast is this"), this executable answers "does this build
// still meet its targets" and exits non-zero on a regression.
//
// Cases either compare against a reference case measured in the same run (so
// the verdict survives different machines) or against an absolute ns/op limit.

#include <containers/collections/BitSet.hpp>
#include <containers/collections/Bytes.hpp>
#include <containers/collections/SmallVec.hpp>
#include <containers/concurrency/ConcurrentQueue.hpp>
#include <containers/concurrency/MpscQueue.hpp>
#include <containers/message/Message.hpp>
#include <containers/stdx/Cell.hpp>
#include <containers/stdx/Cow.hpp>
#include <containers/stdx/OnceCell.hpp>
#include <containers/stdx/SharedStr.hpp>
#include <containers/stdx/StringView.hpp>
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

// Read-only cases can be folded out of the timed loop (the compiler may hoist a
// pure computation whose only consumer is a local address). Feed their results
// through a volatile sink so every iteration still pays the measured work.
volatile std::uint64_t g_perfSink = 0;
volatile std::size_t g_perfRotate = 0;

void Consume(std::uint64_t value) { g_perfSink = g_perfSink + value; }

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

std::string& MutableCompareLeft() {
    static std::string text(1024, 'x');
    return text;
}

std::string& MutableCompareRight() {
    static std::string text(1024, 'x');
    return text;
}

std::vector<bool>& MutableVectorBool64K() {
    static std::vector<bool> bits(64u * 1024u, true);
    return bits;
}

containers::BitSet& MutableBitSet64K() {
    static containers::BitSet bits = [] {
        containers::BitSet value;
        value.resize(64u * 1024u, true);
        return value;
    }();
    return bits;
}

void StringCompare1K() {
    std::string& right = MutableCompareRight();
    right.back() = right.back() == 'x' ? 'y' : 'x';
    Consume(static_cast<std::uint64_t>(MutableCompareLeft().compare(right)));
}

void StringViewCompare1K() {
    std::string& right = MutableCompareRight();
    right.back() = right.back() == 'x' ? 'y' : 'x';
    Consume(static_cast<std::uint64_t>(containers::StringView(MutableCompareLeft())
                                           .compare(containers::StringView(right))));
}

void VectorBoolCount64K() {
    std::vector<bool>& bits = MutableVectorBool64K();
    const std::size_t index = static_cast<std::size_t>(g_perfRotate++) % bits.size();
    bits[index] = !bits[index];
    std::size_t count = 0;
    for (std::size_t cursor = 0; cursor < bits.size(); ++cursor) {
        if (bits[cursor]) {
            ++count;
        }
    }
    Consume(static_cast<std::uint64_t>(count));
}

void BitSetCount64K() {
    containers::BitSet& bits = MutableBitSet64K();
    const std::size_t index = static_cast<std::size_t>(g_perfRotate++) % bits.size();
    bits.flip(index);
    Consume(static_cast<std::uint64_t>(bits.count()));
}

void ConcurrentQueuePushPop256() {
    ConcurrentQueue<int> queue(1024);
    int popped = 0;
    bool ok = true;
    for (int value = 0; value < 256; ++value) {
        ok = queue.tryPush(value) && ok;
        ok = queue.tryPop(popped) && ok;
    }
    KeepAlive(&ok);
}

void MpscQueuePushPop256() {
    containers::MpscQueue<int> queue(1024);
    int popped = 0;
    bool ok = true;
    for (int value = 0; value < 256; ++value) {
        ok = queue.tryPush(value) && ok;
        ok = queue.tryPop(popped) && ok;
    }
    KeepAlive(&ok);
}

void CellGetSet() {
    static containers::Cell<int> cell(0);
    cell.set(cell.get() + 1);
    const int value = cell.get();
    KeepAlive(&value);
}

void RefCellBorrowMut() {
    static containers::RefCell<int> cell(0);
    containers::RefCell<int>::RefMut guard = cell.borrowMut();
    *guard += 1;
    KeepAlive(&guard);
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

    evaluator.AddCase("string_compare_1k", StringCompare1K);
    evaluator.AddCase("stringview_compare_1k", StringViewCompare1K,
                      {0.0, "string_compare_1k", 25.0});

    evaluator.AddCase("vector_bool_count_64k", VectorBoolCount64K);
    evaluator.AddCase("bitset_count_64k", BitSetCount64K,
                      {0.0, "vector_bool_count_64k", 50.0});

    evaluator.AddCase("concurrentqueue_push_pop_256", ConcurrentQueuePushPop256);
    evaluator.AddCase("mpscqueue_push_pop_256", MpscQueuePushPop256,
                      {0.0, "concurrentqueue_push_pop_256", 150.0});

    evaluator.AddCase("cell_get_set", CellGetSet, {50.0, "", 0.0});
    evaluator.AddCase("refcell_borrow_mut", RefCellBorrowMut, {50.0, "", 0.0});
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
