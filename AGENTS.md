# AGENTS.md — Containers 仓库开发规则

面向**在本仓库工作的 agent**。人类向总览见 `README.md`。

---

## 1. 这是什么项目

**Containers**：C++14 的容器编排清单（compose 风格 YAML）解析/校验/排序库——
`include/containers/` + `src/`。核心能力：加载 `services`/`networks`/`volumes`，
一次性收集结构性问题（`Error`/`Warning` + YAML 路径），解析 `extends` 继承，
按 `depends_on` 做确定性拓扑排序得到启动顺序。

**它不接触任何容器运行时**：没有 Docker socket、没有守护进程、不联网。清单就是
数据，所以整条链路可以在单测、CI、甚至没装 Docker 的机器上跑。

约定与 `C:\Code\Nexusflow` 保持一致：`*Test.cpp` 后缀自动归入测试目标、
`Benchmark*.cpp` 归入基准目标、第三方依赖走 `cmake/3rdparty/*.cmake` 的
FetchContent。

| 路径 | 作用 |
| --- | --- |
| `include/containers/engine.hpp` | 公开 API：`Manifest`、`Service`、`Network`、`Volume`、`Issue`、加载器、`parse_port`、`split_command` |
| `src/engine.cpp` | YAML 加载、校验、`extends` 解析、Kahn 排序 |
| `src/engineTest.cpp` | gtest 用例（18 个 / 7 个 suite） |
| `benchmarks/registry/` | Google Benchmark：解析校验、排序、命令切分 |
| `tools/containers_cli.cpp` | 命令行校验器/计划器 |
| `examples/basic-goal/` | 最小库使用者示例 |
| `configs/stack.yaml` | 仓库自带清单，测试、CLI、CI、示例共用 |
| `cmake/` | `include.cmake`、`Utils.cmake`、`Summary.cmake`、`3rdparty/*.cmake` |
| `.github/workflows/ci.yml` | Linux（gcc/clang）与 Windows（MSVC）构建 + ctest |

### 代码地图（改哪里找哪里）

| 想改的东西 | 文件 |
| --- | --- |
| 清单字段 / 模型结构 | `include/containers/engine.hpp` |
| YAML 字段读取（`read_env`、`read_port_node`、`read_mount_node`、`read_health`、`read_resources`） | `src/engine.cpp` |
| 校验规则 | `Manifest::validate()`（`src/engine.cpp`） |
| 继承合并语义 | `Service::merge_from()` + `Manifest::resolve_inheritance()` |
| 启动顺序 | `Manifest::launch_order()`（Kahn + 声明序破平） |
| 端口 / 命令解析 | `parse_port()`、`split_command()` |
| 单测 | `src/*Test.cpp`（任何 `*Test.cpp` 都会被自动编译进 `containers_tests`） |
| 基准 | `benchmarks/**/Benchmark*.cpp` |

---

## 2. 构建

**C++14**（根 `CMakeLists.txt:7` → `set(CMAKE_CXX_STANDARD 14)`）。构建目录统一叫
`build`，不要用 `build-release` 之类名字。

```bash
# 配置（默认 WITH_TESTING/WITH_BENCHMARK/WITH_EXAMPLES/WITH_TOOLS 都是 ON）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# 增量构建
cmake --build build --parallel 16
```

产物：`build/src/containers.lib|a`、`build/tests/containers_tests(.exe)`、
`build/benchmarks/containers_benchmarks`、`build/tools/containers_cli`、
`build/examples/basic-goal/example_basic_goal`。

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

Windows + Clang 的 GNU 驱动有两个已修好的坑，别回退：

1. **CRT 不匹配**：clang 默认静态 CRT，googletest 强制共享 CRT，链接报
   `lld-link: error: /failifmismatch: mismatch detected for 'RuntimeLibrary'`。
   根 `CMakeLists.txt` 给 `WIN32 + Clang + 非 MSVC` 加了
   `add_compile_options(-fms-runtime-lib=dll)` 与 `add_link_options(-fms-runtime-lib=dll)`。
2. **benchmark 1.8.0 的 `-Winvalid-offsetof`**：MSVC 兼容模式下该警告被升级为错误，
   上游的 `#pragma` 压不住。`cmake/3rdparty/gbenchmark.cmake` 只对 `benchmark`
   目标加 `-Wno-invalid-offsetof`。

### 离线构建

依赖走 `FetchContent`：yaml-cpp 0.7.0（`yaml`）、googletest 固定 commit
`03597a01ee50ed33e9dfd640b249b4be3799d395`、google benchmark v1.8.0
（`GoogleBenchmark`）。离线时显式指向本地源码：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DFETCHCONTENT_SOURCE_DIR_YAML=/path/to/yaml-cpp \
      -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest \
      -DFETCHCONTENT_SOURCE_DIR_GOOGLEBENCHMARK=/path/to/benchmark
```

`-DFETCHCONTENT_FULLY_DISCONNECTED=ON` **不行**：别名目标
`yaml-cpp::yaml-cpp` / `GTest::gtest` / `benchmark::benchmark` 不会生成。

---

## 3. 测试与基准

```bash
ctest --test-dir build --output-on-failure
./build/tests/containers_tests                 # 直接跑
./build/benchmarks/containers_benchmarks --benchmark_min_time=0.05s
```

- 当前 **18 个用例 / 7 个 suite**：EngineParse、EngineValidation、PortParsing、
  CommandSplit、Inheritance、LaunchOrder、Repository（校验仓库自带的
  `configs/stack.yaml`）。
- 改引擎（解析、校验、继承、排序）**必须**跑测试；不要为了变绿去改测试语义，
  要么修实现，要么先说明测试断言本身错了。
- 实测基线（Clang 19.1.5 / Release / 28 线程机）：`BM_ParseAndValidate` 8 服务
  178 µs、64 服务 934 µs 级、256 服务 6.6 ms；`BM_LaunchOrder` 64 服务 6.5 µs。

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
6. **清单格式变更要同步**：`configs/stack.yaml`、`README.md` 的字段表、
   `Repository.CheckedInConfigIsValid` 三处必须一起改。
7. 汇报带**数字 + 命令原文**；拿不准标"未确认"，不要猜。
