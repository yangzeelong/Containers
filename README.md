# Containers: A Declarative Planner for Compose-Style YAML

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/yangzeelong/Containers/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++_Version](https://img.shields.io/badge/C++-14%2B-blue.svg)]()

**Containers** is a small, dependency-light C++14 library that turns a compose-style
YAML manifest into validated, queryable model objects. It parses services, networks and
named volumes, reports every structural problem with an actionable path, resolves
`extends` inheritance, and computes a deterministic launch order.

It deliberately **never talks to a container runtime**. There is no Docker socket, no
daemon, no network access: a manifest is just data, so the whole pipeline — parse,
validate, inherit, order — runs in unit tests, in CI, or on a machine with no Docker
installed at all. Execution is somebody else's job; this library is the planning and
validation half of it.

## Features

*   **Compose-style manifest subset** — `services`, `networks`, `volumes`, environment,
    ports, mounts, healthchecks, resource limits, restart policies and labels.
*   **Strict validation, collected not thrown** — every problem comes back as an
    `Issue` with `Error`/`Warning` severity and a dotted path such as
    `services.web.ports[0]`. Duplicate names, unknown dependencies/networks/volumes,
    host-port collisions (protocol aware), relative mount targets and `extends` cycles
    are all caught before anything is launched.
*   **`extends` inheritance with cycle detection** — parents merge into children with
    compose 3.x semantics: explicit fields win, list-valued fields concatenate.
*   **Deterministic launch order** — a Kahn topological sort over `depends_on` with
    declaration-order tie-breaking, so the same manifest always yields the same plan.
*   **Linux-free, daemon-free** — the only third-party dependency is yaml-cpp; tests use
    GoogleTest and the hot paths are measured with Google Benchmark.

## Quick Start

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 16

./build/tools/containers_cli configs/stack.yaml
```

```text
configs/stack.yaml: 5 service(s), valid
   1. db               postgres:16.4
   2. cache            redis:7.4
   3. api-base         my-org/api:1.4.0
   4. api              my-org/api:1.4.0
   5. web              nginx:1.27.2
```

## The Manifest Format

A manifest is a subset of the compose file format. `configs/stack.yaml` is a complete
worked example; the supported keys are:

| Section | Keys |
| --- | --- |
| `services.<name>` | `image`, `extends`, `command`, `depends_on`, `environment`, `ports`, `volumes`, `networks`, `healthcheck`, `resources`, `restart`, `labels` |
| `services.<name>.healthcheck` | `test` (list or string), `interval`, `timeout`, `retries`, `start_period` (seconds) |
| `services.<name>.resources` | `cpus` (fractional cores), `memory` (e.g. `512Mi`, `1Gi`) |
| `services.<name>.ports` | `"8080:80"`, `"127.0.0.1:5432:5432"`, `"5000/udp"`, `5000:5000` |
| `services.<name>.volumes` | `"pgdata:/var/lib/postgresql/data"`, `"static:/usr/share/nginx/html:ro"` |
| `networks.<name>` | `internal`, `driver` |
| `volumes.<name>` | `driver`, `read_only` |
| `version` | Manifest version string (defaults to `3.9`) |

Checked-in rules, all reported as `Issue`s rather than exceptions:

| Rule | Severity |
| --- | --- |
| Service needs an `image` unless it inherits one via `extends` | Error |
| Duplicate service / network / volume names | Error |
| `depends_on` must name a defined service; no self-dependency | Error |
| `networks` must name a defined network | Error |
| Two services publishing the same host port + protocol | Error |
| Mount target must be an absolute path | Error |
| Mount source must be a declared volume or an absolute host path | Error |
| `extends` parent must exist; inheritance cycles are rejected | Error |
| Image without an explicit tag | Warning |
| Service with no network while the manifest declares networks | Warning |

## Library Usage

```cpp
#include <containers/engine.hpp>

std::vector<containers::Issue> issues;
auto manifest = containers::load_manifest_file("configs/stack.yaml", &issues);
if (!containers::Manifest::is_ok(issues)) { /* print issues[].path + .message */ }

issues.clear();
for (const auto& issue : manifest.resolve_inheritance()) issues.push_back(issue);
auto order = manifest.launch_order(&issues);   // db, cache, api-base, api, web
```

`load_manifest_file` also runs `Manifest::validate()` for you, so a manifest that loads
is structurally sound. `resolve_inheritance()` stays explicit because it mutates the
model. Issue paths point at YAML locations (`services.api.ports[0]`), which makes the
CLI output directly greppable in an editor.

Port and command parsing are exposed as free functions for callers that build a model by
hand:

```cpp
containers::Port port;
containers::parse_port("127.0.0.1:5432:5432", &port);   // host=5432, container=5432
auto argv = containers::split_command("sh -c \"echo hi\"");  // {"sh", "-c", "echo hi"}
```

## Command-Line Tool

```bash
containers_cli <manifest.yaml> [--json]
```

Exit codes: `0` — manifest is valid; `1` — validation errors (all printed to stderr);
`2` — bad usage. `--json` emits the resolved launch plan as
`{"services":[{"name":...,"image":...}]}` for consumption by scripts.

## Building

C++14 is deliberate (`set(CMAKE_CXX_STANDARD 14)`): the library stays usable from older
toolchains and can share headers with the Nexusflow project. Build directory is always
`build`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 16
```

Options: `WITH_TESTING`, `WITH_BENCHMARK`, `WITH_EXAMPLES`, `WITH_TOOLS` — all `ON` by
default, so a plain build produces the library, tests, benchmarks, examples and CLI.

### Windows (Clang 19 + Ninja, verified)

CMake rejects MSYS-style paths here; pass native paths. The build also needs CMake's
`-fms-runtime-lib=dll` handling that the root `CMakeLists.txt` adds for the GNU-style
Clang driver on Windows (googletest is forced to the shared CRT):

```bash
cmake -S . -B build -G Ninja ^
      -DCMAKE_MAKE_PROGRAM=C:/Tools/ninja.exe ^
      -DCMAKE_C_COMPILER=C:/Tools/clang+llvm-19.1.5-x86_64-pc-windows-msvc/bin/clang.exe ^
      -DCMAKE_CXX_COMPILER=C:/Tools/clang+llvm-19.1.5-x86_64-pc-windows-msvc/bin/clang++.exe ^
      -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Builds `build/src/containers.lib`, `build/tests/containers_tests.exe`,
`build/benchmarks/containers_benchmarks.exe`, `build/tools/containers_cli.exe`,
`build/examples/basic-goal/example_basic_goal.exe`.

### Dependencies and offline builds

Third-party code is fetched with `FetchContent` (`cmake/3rdparty/`): yaml-cpp 0.7.0,
googletest pinned to commit `03597a01…`, google benchmark v1.8.0. To build without
network access, point each dependency at a local checkout:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DFETCHCONTENT_SOURCE_DIR_YAML=/path/to/yaml-cpp \
      -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest \
      -DFETCHCONTENT_SOURCE_DIR_GOOGLEBENCHMARK=/path/to/benchmark
```

`-DFETCHCONTENT_FULLY_DISCONNECTED=ON` does **not** work: it leaves the
`yaml-cpp::yaml-cpp`, `GTest::gtest` and `benchmark::benchmark` alias targets undefined.

### Install

```bash
cmake --install build --prefix <dir>
```

Installs `lib/libcontainers.a` (or `containers.lib`), `include/containers/**` and
`lib/cmake/Containers/ContainersConfig.cmake`, which exports `Containers::containers`
for downstream `find_package(Containers CONFIG)`.

## Testing

```bash
ctest --test-dir build --output-on-failure     # or ./build/tests/containers_tests
```

18 cases in 7 suites (parse, validation, port parsing, command splitting, inheritance,
launch order, and a `Repository` case that validates the checked-in
`configs/stack.yaml`). Test sources are found by suffix: any `*Test.cpp` under `src/` is
compiled into `containers_tests` instead of the library, mirroring Nexusflow.

## Benchmarking

```bash
./build/benchmarks/containers_benchmarks --benchmark_min_time=0.05s
```

Representative results (Clang 19.1.5, Ryzen-class 28-thread machine, Release):

| Benchmark | Arg | Time |
| --- | --- | --- |
| `BM_ParseAndValidate` | 8 services | 178 µs |
| `BM_ParseAndValidate` | 64 services | 1.54 ms (≈6.8 MB/s) |
| `BM_ParseAndValidate` | 256 services | 6.63 ms |
| `BM_LaunchOrder` | 64 services | 6.5 µs |
| `BM_LaunchOrder` | 256 services | 25.9 µs |
| `BM_SplitCommand` | 16 tokens | 514 ns |
| `BM_SplitCommand` | 128 tokens | 2.88 µs |

## Repository Layout

| Path | Contents |
| --- | --- |
| `include/containers/engine.hpp` | Public API: `Manifest`, `Service`, `Network`, `Volume`, `Issue`, loaders |
| `src/engine.cpp` | YAML loading, validation, `extends` resolution, Kahn ordering |
| `src/engineTest.cpp` | GoogleTest cases (auto-discovered via `*Test.cpp`) |
| `benchmarks/registry/` | Google Benchmark suites for parse/validate/order/split |
| `tools/containers_cli.cpp` | `containers_cli` command-line validator/planner |
| `examples/basic-goal/` | Minimal library consumer example |
| `configs/stack.yaml` | Checked-in sample manifest used by tests, CLI and CI |
| `cmake/` | `include.cmake`, `Utils.cmake`, `Summary.cmake`, `3rdparty/*.cmake` |
| `.github/workflows/ci.yml` | Linux (gcc, clang) and Windows (MSVC) build + ctest |

## License

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 yangzeelong.
