# Containers (cs): Header-Only C++14 Containers & Std Extensions

[![ci](https://github.com/yangzeelong/Containers/actions/workflows/ci.yml/badge.svg)](https://github.com/yangzeelong/Containers/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++_Version](https://img.shields.io/badge/C++-14%2B-blue.svg)]()

**Containers** (friends call it **cs**) is a header-only C++14 library of
Rust-inspired `std` extensions (`stdx/`), general-purpose containers
(`collections/`), concurrency primitives (`concurrency/`) and a copy-on-write
message type (`message/`). Other C++14 projects consume it as a package.
Everything lives in the `containers::` namespace and installs with
`find_package(Containers CONFIG)`.

The library is deliberately self-contained — it contains no framework runtime, no
pipeline, no executor and no scheduling. Every component is usable on its own, so
the whole surface (copies, detach, queues, blocking, wake-up) runs in unit tests,
in CI, or on a machine with nothing else installed. Third-party dependencies
(googletest for tests, Google Benchmark for benchmarks) are build-time only and
never enter the installed package.

## Features

The include tree mirrors the API groups — `message/`, `stdx/`, `collections/`,
`concurrency/` — while the namespace stays flat (`containers::`) and every header
is included individually.

### `message/` — copy-on-write messages

*   **Copy-On-Write message** — `containers::Message` holds any copy-constructible
    payload behind one `shared_ptr`. Copying a `Message` clones only the control
    block; the payload is deep-copied on the first mutable access (`Mut<T>()` /
    `MutPtr<T>()`), so broadcasting one message to many consumers is cheap while
    copies stay independent. It also carries per-stage timing metadata that is
    itself copy-on-write.
*   **Zero-copy Bytes payloads** — a `Message` can carry `containers::Bytes`;
    copying the message is one ref-count bump and `Borrow<Bytes>()` never copies
    the buffer (the `MessageBytesTest` suite pins this down).

### `stdx/` — Rust-inspired std extensions

*   **Type-erased variant** — `containers::Any` is a simplified, C++14-compatible
    `std::any`: typed lookup, deep copy on copy.
*   **Rust-style optional** — `Optional<T>` uses manual placement construction with
    exception-safe `reset()`/assignment (global namespace on purpose, to match the
    interface it was originally extracted from).
*   **Non-owning pointer view** — `ViewPtr<T>` is a zero-cost wrapper explicitly
    modelling "I don't own it", with `std::hash` support for unordered containers.
*   **Non-owning string view** — `containers::StringView` is a C++14 stand-in for
    `std::string_view`: a constexpr-friendly view over any character buffer with
    substring, compare, find/rfind and prefix/suffix checks. It never allocates,
    keeps embedded NUL bytes, and provides `std::hash` plus an `_sv` literal.
*   **Rust-style result** — `containers::Result<T, E>` with a `Result<void, E>`
    specialization, `map` / `mapErr` / `andThen` / `unwrapOr`, plus a `Unit` type
    for empty payloads. C++14, no exceptions required.
*   **Rust-style Cow** — `containers::Cow<T>` is either borrowed or owned; reads
    are free and the first `toMut()` deep-copies, the same contract as
    `std::borrow::Cow`.
*   **Lazy initialization** — `OnceCell<T>` initializes at most once through a
    factory (thread-safe `std::call_once`), `Lazy<T>` wraps it behind `*`/`->`.
*   **Interior mutability** — `Cell<T>` hands out copies and can be updated through
    a const reference, while `RefCell<T>` adds Rust-style runtime borrow checking:
    many shared borrows or one mutable borrow at a time, with throwing `borrow()` /
    `borrowMut()` and non-throwing `tryBorrow()` / `tryBorrowMut()` variants.
*   **Typed bit flags** — `BitFlags<Enum>` wraps an enum's bit pattern with
    `contains` / `insert` / `remove` and `|`, `&`, `^`, `~` operators.
*   **Non-zero integers** — `NonZero<T>` encodes the "never zero" invariant;
    `TryNew` returns an empty `Optional` instead of asserting.
*   **Shared strings** — `SharedStr` is `Arc<str>`: immutable, cheap to clone and
    slice, with content comparison and `SharedStrHash` for unordered containers.
*   **Scope guard** — `ScopedGuard` / `OnScopeExit` runs a callable on scope exit
    unless dismissed, for rollback and cleanup paths that must not be forgotten.

### `collections/` — general-purpose containers

*   **Ref-counted byte buffers** — `Bytes` clones and slices share storage,
    `BytesMut` grows mutably and `freeze()` hands the buffer over without copying.
*   **Small vectors** — `SmallVec<T, N>` stores up to `N` elements inline and
    promotes to a heap vector on the first overflow, like Rust's `smallvec`.
*   **Insertion-ordered map** — `IndexMap<K, V>` is a hash map that iterates in
    insertion order, with O(1) `swapErase` and order-preserving `erase`.
*   **Stable slots** — `SlotMap<T>` hands out generation-tagged `SlotKey`s, so a
    stale key can never alias a reused slot (`clear` invalidates every key).
*   **Growable bitset** — `BitSet` is a dynamically sized bit vector stored in
    64-bit words: `insert` / `remove` / `contains`, automatic growth, `count`,
    `nextSetBit` iteration and bitwise `&`, `|`, `^`, `~` operators.

### `concurrency/` — queues, threads and shared ownership

*   **Blocking concurrent queue** — `ConcurrentQueue<T>` is a mutex/condition-variable
    bounded or unbounded MPMC queue with shutdown wake-up, FIFO order and lifetime counters.
*   **Lock-free SPSC ring queue** — `SpscQueue<T>` is a bounded single-producer/
    single-consumer ring buffer with cache-line-padded hot indices, Dekker-correct
    wake-up handshake and a spin/condition-variable blocking path. A non-positive
    capacity transparently falls back to `ConcurrentQueue`.
*   **Atomic shared_ptr** — `AtomicSharedPtr<T>` is a C++14 stand-in for C++20's
    `std::atomic<std::shared_ptr<T>>` (mutex-based load/store/exchange/CAS).
*   **Bounded lock-free MPSC queue** — `MpscQueue<T>` is a multi-producer /
    single-consumer ring buffer: producers reserve slots through a CAS and publish
    with a release sequence word, the consumer recycles them. `waitAndPop()` parks
    the consumer on a condition variable that producers only touch when it is
    idle, and `close()` wakes a blocked consumer and rejects new pushes.
*   **Join-on-destroy threads** — `Thread` is a move-only RAII wrapper around
    `std::thread` that joins in its destructor instead of terminating the process;
    `ScopedThread` is the non-movable, non-detachable variant for structured
    concurrency, so a worker can never outlive its scope.

## Quick Start

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 16
ctest --test-dir build --output-on-failure
```

First-party test and benchmark targets compile warnings-as-errors
(`-Wall -Wextra -Werror` on GCC/Clang, `/W4 /WX` on MSVC); the exported header-only
target itself stays free of these flags.

The checked-in `examples/consumer/` project is a standalone downstream user: point it
at an install prefix with `find_package(Containers CONFIG)` — exactly what the CI does
for every change:

```bash
cmake --install build --prefix <dir>
cmake -S examples/consumer -B consumer-build -DCMAKE_PREFIX_PATH=<dir>
cmake --build consumer-build
./consumer-build/consumer
```

`examples/consumer/CMakeLists.txt` defaults the single-config build type to
Release (matching the shipped package); pass `-DCMAKE_BUILD_TYPE=Debug` etc. to
override.

## Repository Layout

| Path | Contents |
| --- | --- |
| `include/containers/message/` | `Message.hpp` — COW message + message metadata |
| `include/containers/stdx/` | Rust-inspired std extensions: `Any.hpp`, `Optional.hpp`, `Result.hpp`, `Unit.hpp`, `Cow.hpp`, `Cell.hpp`, `OnceCell.hpp`, `BitFlags.hpp`, `NonZero.hpp`, `ScopedGuard.hpp`, `SharedStr.hpp`, `StringView.hpp`, `ViewPtr.hpp` |
| `include/containers/collections/` | General-purpose containers: `Bytes.hpp`, `SmallVec.hpp`, `IndexMap.hpp`, `SlotMap.hpp`, `BitSet.hpp` |
| `include/containers/concurrency/` | Queues, threads and shared ownership: `ConcurrentQueue.hpp`, `SpscQueue.hpp`, `MpscQueue.hpp`, `AtomicSharedPtr.hpp`, `Thread.hpp` |
| `include/containers/Config.hpp` | `CS_NODISCARD` feature shim shared by the headers |
| `include/containers/bench/PerfEval.hpp` | Threshold-based performance evaluation harness |
| `tests/` | GoogleTest sources grouped by component (`message/`, `stdx/`, `collections/`, `concurrency/`, `bench/`) + `gtest_main.cpp` runner |
| `benchmarks/` | `benchmark_main.cpp` + Google Benchmark suites grouped by component (`message/`, `stdx/`, `collections/`, `concurrency/`) |
| `benchmarks/perf/` | `containers_perf_eval`: targets, verdicts and a Markdown report |
| `examples/consumer/` | Standalone downstream project: `find_package(Containers CONFIG)` + install prefix |
| `cmake/` | `include.cmake`, `Utils.cmake`, `Summary.cmake`, `3rdparty/*.cmake`, `ContainersConfig.cmake.in` |
| `.github/workflows/ci.yml` | Linux (gcc, clang) and Windows (MSVC) build + test + install-consume + benchmark smoke; Linux sanitizer jobs (ASan+UBSan, TSan) |

## Testing & Benchmarking

```bash
ctest --test-dir build --output-on-failure                            # 3 tests: tests + benchmarks + perf smoke
./build/benchmarks/containers_benchmarks --benchmark_min_time=0.05s   # standalone benchmark run
./build/benchmarks/containers_perf_eval --rounds 15 --iterations 2000  # targets + report
```

Current suites: `MessageTest`, `MessageMetaTest`, `AnyTest`,
`OptionalThrowingTest`, `ConcurrentQueueTest`, `SpscQueueTest`, `MpscQueueTest`,
`SpscMessageStress`, `ResultTest`, `UnitTest`, `ScopedGuardTest`, `CowTest`,
`OnceCellTest`, `CellTest`, `RefCellTest`, `LazyTest`, `BytesTest`,
`BytesMutTest`, `BitFlagsTest`, `NonZeroTest`, `SmallVecTest`,
`AtomicSharedPtrTest`, `ThreadTest`, `ScopedThreadTest`, `SharedStrTest`,
`IndexMapTest`, `SlotMapTest`, `BitSetTest`, `StringViewTest`,
`MessageBytesTest`, `PerfEvalTest`. Test sources
are found by suffix: any `*Test.cpp` under `tests/` is compiled into
`containers_tests`, so a new test file is picked up automatically.

Locally measured (Clang 19.1.5, Release, `--benchmark_min_time=0.05s`, 28 cores;
treat as a baseline, not a contract):

| Benchmark | 16 elems | 2048 elems |
| --- | --- | --- |
| `BM_MessageCopy` (shared copy: control block only) | 0.011 us | 0.011 us |
| `BM_MessageConstruction` (copy payload into a Message) | 0.093 us | 0.135 us |
| `BM_MessageCOWDetach` (copy + first `Mut`) | 0.074 us | 0.105 us |
| `BM_MessageClone` (explicit deep copy) | 0.065 us | 0.093 us |
| `BM_SpscQueueMessageTransfer` (push + waitAndPop, string payload) | 0.076 us | — |

The takeaway is the same at every payload size: `BM_MessageCopy` stays at ~0.011 us
while explicit deep copies (`Clone`, first `Mut`) grow with the payload.

### Performance evaluation (`containers_perf_eval`)

Google Benchmark answers "how fast is this case". The evaluation runner answers
"does this build still meet its targets" and exits non-zero when a case misses:

```bash
./build/benchmarks/containers_perf_eval --rounds 15 --iterations 2000 --warmup 2 \\
    --output perf_report.md
```

- Every case is measured over `--rounds` timed rounds of `--iterations` calls;
  the Markdown report lists mean/median/p95/min/max/stddev/CV.
- A target is either a **reference case in the same run** (for example
  `0.737x vector_reserved_push`) or an **absolute ns/op limit**. Same-run
  references are machine independent, so they are the checks CI can enforce;
  absolute limits are budget checks for a known machine.
- `--tolerance PCT` is the default allowed regression and each case can override
  it. Exit code is `1` when a case misses its target; `--no-check` downgrades the
  run to report-only. `--list` prints the registered cases.
- `ctest` runs a short `--no-check` pass so the runner itself cannot rot.
- The registered gates cover the newer components as well: `stringview_compare_1k`
  (vs `std::string`), `bitset_count_64k` (vs `std::vector<bool>`),
  `mpscqueue_push_pop_256` (vs `ConcurrentQueue`) and absolute budgets for
  `cell_get_set` / `refcell_borrow_mut`.

Adding a case is one call in `benchmarks/perf/PerfEvaluatorMain.cpp`:
`evaluator.AddCase("name", body, {baselineNs, "reference", tolerancePercent})`.
Bodies must have observable side effects, otherwise the optimizer is free to
delete them (the framework never talks to the optimizer on your behalf).

Offline builds can reuse a local google benchmark source tree with
`-DFETCHCONTENT_SOURCE_DIR_GOOGLEBENCHMARK=<googlebenchmark-src>` and a local
googletest tree with `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<googletest-src>`.

## Package

`cmake --install build --prefix <dir>` installs
`include/containers/**/*.hpp` (headers keep their group sub-directories),
`lib/cmake/Containers/ContainersConfig.cmake` (which exports
`Containers::containers`) and no third-party runtime dependency — the package is
pure C++ headers, so a downstream `find_package(Containers CONFIG)` pulls nothing
extra.

## License

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 yangzeelong.
