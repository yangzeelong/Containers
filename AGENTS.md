# AGENTS.md — Containers 仓库开发规则

面向**在本仓库工作的 agent**。人类向总览见 `README.md`。

---

## 1. 这是什么项目

**Containers（朋友们简称 cs）**：C++14 的**通用容器库 + std 扩展**，纯 header-only
对外分发，供其他 C++14 项目按需引用（`find_package(Containers CONFIG)`）。头文件按
类别放在子目录，组件覆盖四组：

- `message/`：**Copy-On-Write 消息容器**（`containers::Message`，共享指针深拷贝语义）。
- `stdx/`：Rust 风格的基础类型与 std 扩展——`Any`、`Optional`、`ViewPtr`、`StringView`、
  `Result`、`Unit`、`Cow`、`OnceCell`/`Lazy`、`Cell`/`RefCell`、`BitFlags`、`NonZero`、`SharedStr`、`ScopedGuard`。
- `collections/`：通用容器——`Bytes`/`BytesMut`、`SmallVec`、`IndexMap`、`SlotMap`、`BitSet`。
- `concurrency/`：并发原语——阻塞式并发队列 `ConcurrentQueue`、无锁单生产者单消费者
  环形队列 `SpscQueue`、无锁多生产者单消费者队列 `MpscQueue`、`AtomicSharedPtr`、
  `Thread`/`ScopedThread`。

**它不依赖任何框架运行时**：没有 pipeline、没有消息执行、没有调度——每个组件都能
独立使用，全部行为（拷贝、分离、进出队、阻塞、唤醒）可以在单测、CI、甚至没装任何
框架的机器上验证。

测试代码统一放在 `tests/` 下（按组件分组），`*Test.cpp` 后缀自动归入测试目标。
第三方依赖只剩 googletest（单测）与 google benchmark（基准），都通过
`cmake/3rdparty/*.cmake` 的 FetchContent 引入。

| 路径 | 作用 |
| --- | --- |
| `include/containers/message/Message.hpp` | COW 消息容器 + 消息元数据 |
| `include/containers/stdx/` | Rust 风格基础类型 / std 扩展（Any、Optional、ViewPtr、StringView、Result、Unit、Cow、OnceCell、Cell、RefCell、BitFlags、NonZero、SharedStr、ScopedGuard） |
| `include/containers/collections/` | 通用容器（Bytes、SmallVec、IndexMap、SlotMap、BitSet） |
| `include/containers/concurrency/` | 并发原语（ConcurrentQueue、SpscQueue、MpscQueue、AtomicSharedPtr、Thread/ScopedThread） |
| `tests/` | 单测（按组件分组：message/stdx/collections/concurrency/bench，自动编译进 `containers_tests`） |
| `benchmarks/` | Google Benchmark（同样按组件分组）+ `benchmark_main.cpp`；`benchmarks/perf/` 是 perf-eval runner |
| `benchmarks/perf/` | `containers_perf_eval`：目标阈值判定 + Markdown 报告 |
| `examples/consumer/` | 独立下游工程：`find_package(Containers CONFIG)`，只对着 `cmake --install` 的前缀构建，不进入本树 |
| `cmake/` | `include.cmake`、`Utils.cmake`、`Summary.cmake`、`3rdparty/*.cmake`、`ContainersConfig.cmake.in` |
| `.github/workflows/ci.yml` | Linux（gcc/clang）与 Windows（MSVC）构建 + 单测 + 安装消费 + 基准冒烟 |

### 代码地图（改哪里找哪里）

| 想改的东西 | 文件 |
| --- | --- |
| Message 模型 / COW 行为 | `include/containers/message/Message.hpp` |
| Any / Optional / ViewPtr / StringView | `include/containers/stdx/Any.hpp`、`include/containers/stdx/Optional.hpp`、`include/containers/stdx/ViewPtr.hpp`、`include/containers/stdx/StringView.hpp` |
| Result / Unit / Config（Rust 风格） | `include/containers/stdx/Result.hpp`、`include/containers/stdx/Unit.hpp`、`include/containers/Config.hpp` |
| ScopedGuard / OnScopeExit | `include/containers/stdx/ScopedGuard.hpp` |
| Cow（Borrowed/Owned） | `include/containers/stdx/Cow.hpp` |
| OnceCell / Lazy | `include/containers/stdx/OnceCell.hpp` |
| Cell / RefCell（内部可变性） | `include/containers/stdx/Cell.hpp` |
| Bytes / BytesMut | `include/containers/collections/Bytes.hpp` |
| BitFlags | `include/containers/stdx/BitFlags.hpp` |
| NonZero | `include/containers/stdx/NonZero.hpp` |
| SmallVec | `include/containers/collections/SmallVec.hpp` |
| AtomicSharedPtr | `include/containers/concurrency/AtomicSharedPtr.hpp` |
| SharedStr | `include/containers/stdx/SharedStr.hpp` |
| IndexMap | `include/containers/collections/IndexMap.hpp` |
| SlotMap | `include/containers/collections/SlotMap.hpp` |
| BitSet（动态位集） | `include/containers/collections/BitSet.hpp` |
| 性能评估框架 | `include/containers/bench/PerfEval.hpp` + `benchmarks/perf/PerfEvaluatorMain.cpp` |
| 并发队列 / 无锁 SPSC 环形队列 | `include/containers/concurrency/ConcurrentQueue.hpp` / `SpscQueue.hpp` |
| 无锁 MPSC 环形队列 | `include/containers/concurrency/MpscQueue.hpp` |
| Thread / ScopedThread（RAII 线程） | `include/containers/concurrency/Thread.hpp` |
| 发布规则（包导出） | `cmake/ContainersConfig.cmake.in` + 根 `CMakeLists.txt` 末尾 install 部分 |
| 单测 | `tests/**/*Test.cpp`（按组件分组，任何 `*Test.cpp` 都会被自动编译进 `containers_tests`） |
| 基准 | `benchmarks/**/Benchmark*.cpp` |
| 性能评估用例 / 阈值 | `benchmarks/perf/PerfEvaluatorMain.cpp`（`AddCase` 一处注册） |

### 命名约定

- 类的私有 / 受保护数据成员：`m_` + lowerCamelCase（例如 `m_value`、`m_heapMode`、
  `m_sharedBorrows`、`m_writePos`），不再使用尾部下划线（`value_`）。
- 公开聚合 struct 的字段（`MessageMeta`、队列的 `Stats`、`PerfStats`/`PerfOptions`/
  `PerfCase`/`PerfResult`）保持无前缀 lowerCamelCase，它们是对外数据 API。
- 函数、参数、局部变量：lowerCamelCase，无前缀。类内静态常量用 `k` + CamelCase
  （如 `kBitsPerWord`）；类型名用 CamelCase。

特别注意：`SpscQueue` 的环形缓冲里 head/tail 按 cache line 隔开（wake-up handshake
是 Dekker 型 StoreLoad），改热点索引时必须同步改 padding，否则会退化成
false-sharing 且竞态唤醒泄漏，动它们之前先读类注释再动测试里的压力用例。

---

## 2. 构建

**C++14**（根 `CMakeLists.txt:7` → `set(CMAKE_CXX_STANDARD 14)`）。构建目录统一叫
`build`，不要用 `build-release` 之类名字。

```bash
# 配置（默认 WITH_TESTING/WITH_BENCHMARK 都是 ON）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# 增量构建
cmake --build build --parallel 16
```

产物：`build/tests/containers_tests(.exe)`（单测）、
`build/benchmarks/containers_benchmarks`（基准，联网或离线源码）、
`build/benchmarks/containers_perf_eval`（目标阈值判定）。header-only 库本身没有
产物；`examples/consumer` 是独立工程，永远不进入树内构建，只由 CI 在
`cmake --install` 之后用 `find_package` 消费。

### 安装 / 包导出

`cmake --install build --prefix <dir>` 必须产出
`include/containers/**/*.hpp`（保留分组子目录）、`lib/cmake/Containers/ContainersConfig.cmake`：它由
`cmake/ContainersConfig.cmake.in` 加根 `CMakeLists.txt` 末尾的
`install(EXPORT containersTargets ...)` 生成，导出 `Containers::containers`。
**没有任何第三方运行时依赖**（纯 C++ 头）。**这条路径有验收工程**：
`examples/consumer/`（独立工程，不在本树构建）就是用 `<prefix>` 做
`CMAKE_PREFIX_PATH` 编译并加载 Message/SpscQueue，CI 每个 job 都跑一遍"安装 +
消费"，改导出规则后要保证它仍然通过。

### Windows（Clang 19 + Ninja，已实测）

CMake **不接受** MSYS 风格路径（`/c/Tools/ninja.exe`），必须传 Windows 路径：

```powershell
cmake -S . -B build -G Ninja `
      -DCMAKE_MAKE_PROGRAM=C:/Tools/ninja.exe `
      -DCMAKE_C_COMPILER=C:/Tools/clang+llvm-19.1.5-x86_64-pc-windows-msvc/bin/clang.exe `
      -DCMAKE_CXX_COMPILER=C:/Tools/clang+llvm-19.1.5-x86_64-pc-windows-msvc/bin/clang++.exe `
      -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Windows + Clang 的 GNU 驱动有一个已修好的坑，别回退：**CRT 不匹配**——clang 默认
静态 CRT，googletest 强制共享 CRT，链接报 `lld-link: error: /failifmismatch:
mismatch detected for 'RuntimeLibrary'`。根 `CMakeLists.txt` 给 `WIN32 + Clang +
非 MSVC` 加了 `add_compile_options(-fms-runtime-lib=dll)` 与
`add_link_options(-fms-runtime-lib=dll)`。

### 离线构建

依赖走 `FetchContent`：googletest 固定 commit `03597a01ee50ed33e9dfd640b249b4be3799d395`
（URL 下载 zip）、google benchmark v1.8.0（`GoogleBenchmark`）。
`-DFETCHCONTENT_FULLY_DISCONNECTED=ON` **不行**：别名目标
`GTest::gtest` / `benchmark::benchmark` 不会生成。离线时显式指向本地源码：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=C:/Code/3rdparty/googletest-1.15.2 \
      -DFETCHCONTENT_SOURCE_DIR_GOOGLEBENCHMARK=C:/Code/3rdparty/benchmark-1.8.0
```

——本机 `C:/Code/3rdparty/googletest-1.15.2` 当 GOOGLETEST 源，
`C:/Code/3rdparty/benchmark-1.8.0` 当 GOOGLEBENCHMARK 源，两者都离线可用，所以本机
`WITH_BENCHMARK=ON` 也能编、能跑。注意 benchmark 1.8.0 没有
`benchmark::SkipWithError`，要写 `state.SkipWithError(...)`；`MakeMessage` 不接受
字符串字面量（`std::decay` 后是指针，被 SFINAE 排除），基准里用 `std::string`。

---

## 3. 测试与基准

```bash
ctest --test-dir build --output-on-failure
./build/tests/containers_tests                 # 直接跑
./build/benchmarks/containers_benchmarks --benchmark_min_time=0.05s   # 联网/离线源码时
./build/benchmarks/containers_perf_eval --rounds 15 --iterations 2000 # 目标判定 + 报告
```

- 当前 suite：`MessageTest`、`MessageMetaTest`、`AnyTest`、`OptionalThrowingTest`、
  `ConcurrentQueueTest`、`SpscQueueTest`、`MpscQueueTest`、`SpscMessageStress`（含 20 万消息 SPSC
  双线程压力用例）、`ResultTest`、`UnitTest`、`ScopedGuardTest`、`CowTest`、
  `OnceCellTest`、`CellTest`、`RefCellTest`、`LazyTest`、`BytesTest`、`BytesMutTest`、`BitFlagsTest`、
  `NonZeroTest`、`SmallVecTest`、`AtomicSharedPtrTest`、`ThreadTest`、`ScopedThreadTest`、`SharedStrTest`、
  `IndexMapTest`、`SlotMapTest`、`BitSetTest`、`StringViewTest`、`MessageBytesTest`、`PerfEvalTest`。
- 改头（解析、COW、队列、唤醒）**必须**跑测试；不要为了变绿去改测试语义，
  要么修实现，要么先说明测试断言本身错了。
- 性能评估框架（`containers_perf_eval`）：`include/containers/bench/PerfEval.hpp`
  提供 rounds/iterations 采样与 mean/median/p95/min/max/stddev/CV 统计；用例在
  `benchmarks/perf/PerfEvaluatorMain.cpp` 注册，目标可以是同一次 run 内的参照用例
  （机器无关，例 `0.737x vector_reserved_push`）或绝对 ns/op 上限；
  `--tolerance` 给默认容差，超标退出码 1，`--no-check` 只出报告。`ctest` 跑一条
  `--no-check` 短冒烟，保证框架本身不腐化。
- 本地实测（Clang 19.1.5 Release）：`ctest` 3/3 通过——`containers_tests` 1.9s
  （31 个 suite，含 20 万消息 SPSC 压力）、`containers_benchmarks` 76s。基准数字
  基线（16/2048 元素）：`BM_MessageCopy` 0.011/0.011 us、`BM_MessageConstruction`
  0.093/0.135 us、`BM_MessageCOWDetach` 0.074/0.105 us、`BM_MessageClone`
  0.065/0.093 us、`BM_SpscQueueMessageTransfer` 0.076 us；拷贝成本与 payload 大小
  无关正是 COW 的验收点。
- 基准冒烟（CI 同款）：
  `./build/benchmarks/containers_benchmarks --benchmark_min_time=0.01s --benchmark_filter=BM_SpscQueueMessageTransfer`。

---

## 4. 硬规则（务必遵守）

1. **不要自动 `git commit` / `git push`**，除非用户明确说"提交/推送"。
   本仓库的首次提交是因为用户在初始化时明确要求建仓库。
2. **C++14**，不用 C++17+ 语法（结构化绑定、`if constexpr`、`std::optional`、
   `std::string_view`、inline 变量、if-init 等）。
3. **构建目录固定为 `build`**。
4. 改完必须验证：构建退出码 0 + `containers_tests` 全绿；**跑了就说跑了，没跑就
明确说没跑**。
5. **行尾**：提交前比对 `git diff --stat` 与 `git diff --stat --ignore-cr-at-eol`，
   避免 CRLF 假 diff。
6. **清单格式变更要同步**：改头接口时同步更新 `README.md`、`AGENTS.md`、
   单测（`tests/**/*Test.cpp` 三处必须一起改）。
7. 汇报带**数字 + 命令原文**；拿不准标"未确认"，不要猜。
