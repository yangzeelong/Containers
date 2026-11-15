# Containers (cs): Common Message & Queue Containers for C++14

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/yangzeelong/Containers/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++_Version](https://img.shields.io/badge/C++-14%2B-blue.svg)]()

**Containers** (friends call it **cs**) is a header-only, C++14 common library of
the message and queue containers that a family of frameworks reuse. It takes its
components from the NexusFlow test bed and publishes them as a standalone
package: a type-erased **Copy-On-Write message**, a `std::any`-style variant, a
Rust-style optional, a non-owning pointer view, a blocking concurrent queue and a
lock-free single-producer/single-consumer ring queue.

It deliberately **never reaches for a framework runtime**: there is no pipeline, no
message executor, no scheduling — a message is just data. The whole library is
header-only, so the entire chain — copy, detach, queue, enqueue, block, wake — runs
in unit tests, in CI, or offline on any machine.

## Features

*   **Copy-On-Write message** — `containers::Message` holds any copy-constructible
    payload behind one `shared_ptr`. Copying a `Message` clones only the control
    block; the payload is deep-copied on the first mutable access (`Mut<T>()` /
    `MutPtr<T>()`), so broadcast-into-many-pipelines is cheap while copies stay
    independent. It also carries per-stage timing metadata that is itself copy-on-write.
*   **Type-erased variant** — `containers::Any` is a simplified, C++14-compatible
    `std::any`: typed lookup, deep copy on copy.
*   **Rust-style result** — `containers::Result<T, E>` with a `Result<void, E>`
    specialization, `map` / `mapErr` / `andThen` / `unwrapOr`, plus a `Unit` type
    for empty payloads. C++14, no exceptions required.
*   **Rust-style optional** — `Optional<T>` uses manual placement construction with
    exception-safe `reset()`/assignment (global namespace, as in the upstream test bed).
*   **Non-owning pointer view** — `ViewPtr<T>` is a zero-cost wrapper explicitly
    modelling "I don't own it", with `std::hash` support for unordered containers.
*   **Scope guard** — `ScopedGuard` / `OnScopeExit` runs a callable on scope exit
    unless dismissed, for rollback and cleanup paths that must not be forgotten.
*   **Rust-style Cow** — `containers::Cow<T>` is either borrowed or owned; reads
    are free and the first `toMut()` deep-copies, the same contract as
    `std::borrow::Cow`.
*   **Blocking concurrent queue** — `ConcurrentQueue<T>` is a mutex/condition-variable
    bounded or unbounded MPMC queue with shutdown wake-up, FIFO order and lifetime counters.
*   **Lock-free SPSC ring queue** — `SpscQueue<T>` is a bounded single-producer/
    single-consumer ring buffer with cache-line-padded hot indices, Dekker-correct
    wake-up handshake and a spin/condition-variable blocking path. A non-positive
    capacity transparently falls back to `ConcurrentQueue`.

## Quick Start

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 16
ctest --test-dir build --output-on-failure
```

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
| `include/containers/Message.hpp` | Copy-on-write message + message metadata |
| `include/containers/Any.hpp` | Type-erased variant |
| `include/containers/Optional.hpp` | Rust-style optional |
| `include/containers/ViewPtr.hpp` | Non-owning pointer view + std::hash |
| `include/containers/Result.hpp` | Rust-style `Result<T, E>` + combinators |
| `include/containers/Unit.hpp` | `()`-style empty value type |
| `include/containers/Config.hpp` | `CS_NODISCARD` feature shim |
| `include/containers/ScopedGuard.hpp` | Scope-exit callable (`OnScopeExit`) |
| `include/containers/Cow.hpp` | Borrowed/owned `Cow<T>` with copy-on-write `toMut()` |
| `include/containers/ConcurrentQueue.hpp` | Blocking MPMC queue |
| `include/containers/SpscQueue.hpp` | Lock-free SPSC ring queue |
| `src/*Test.cpp` | GoogleTest cases colocated in src (auto-discovered via *Test.cpp) |
| `benchmarks/registry/` | Google Benchmark suites for copy, COW detach, clone, queue transfer, Result |
| `examples/consumer/` | Standalone downstream project: `find_package(Containers CONFIG)` + install prefix |
| `configs/` | Removed when the compose engine left |
| `tools/` | Removed with the compose engine |
| `cmake/` | `include.cmake`, `Utils.cmake`, `Summary.cmake`, `3rdparty/*.cmake`, `ContainersConfig.cmake.in` |
| `.github/workflows/ci.yml` | Linux (gcc, clang) and Windows (MSVC) build + test + install-consume + benchmark smoke |

## Testing & Benchmarking

```bash
ctest --test-dir build --output-on-failure                            # 2 tests: containers_tests + containers_benchmarks
./build/benchmarks/containers_benchmarks --benchmark_min_time=0.05s   # standalone run
```

Current suites: `MessageTest`, `MessageMetaTest`, `AnyTest`,
`OptionalThrowingTest`, `ConcurrentQueueTest`, `SpscQueueTest`,
`SpscMessageStress`, `ResultTest`, `UnitTest`, `ScopedGuardTest`, `CowTest`.
Test sources are found by suffix:
any `*Test.cpp` is compiled into `containers_tests`, mirroring Nexusflow's convention.

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

Offline builds can reuse a local google benchmark source tree with
`-DFETCHCONTENT_SOURCE_DIR_GOOGLEBENCHMARK=<googlebenchmark-src>` and a local
googletest tree with `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<googletest-src>`.

## Package

`cmake --install build --prefix <dir>` installs
`include/containers/*.hpp`, `lib/cmake/Containers/ContainersConfig.cmake` (which
exports `Containers::containers`) and no third-party runtime dependency — the
package is pure C++ headers, so a downstream `find_package(Containers CONFIG)`
pulls nothing extra.

## License

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 yangzeelong.
