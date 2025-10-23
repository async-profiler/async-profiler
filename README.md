# Async-profiler

This project is a low overhead sampling profiler for Java
that does not suffer from the [Safepoint bias problem](http://psy-lob-saw.blogspot.ru/2016/02/why-most-sampling-java-profilers-are.html).
It features HotSpot-specific API to collect stack traces
and to track memory allocations. The profiler works with
OpenJDK and other Java runtimes based on the HotSpot JVM.

Unlike traditional Java profilers, async-profiler monitors non-Java threads
(e.g., GC and JIT compiler threads) and shows native and kernel frames in stack traces.

What can be profiled:

- CPU time
- Allocations in Java Heap
- Native memory allocations and leaks
- Contended locks
- Hardware and software performance counters like cache misses, page faults, context switches
- and [more](docs/ProfilingModes.md).

See our [3 hours playlist](https://www.youtube.com/playlist?list=PLNCLTEx3B8h4Yo_WvKWdLvI9mj1XpTKBr)
to learn about more features.

# Download

### Stable release: [4.2](https://github.com/async-profiler/async-profiler/releases/tag/v4.2)

- Linux x64: [async-profiler-4.2-linux-x64.tar.gz](https://github.com/async-profiler/async-profiler/releases/download/v4.2/async-profiler-4.2-linux-x64.tar.gz)
- Linux arm64: [async-profiler-4.2-linux-arm64.tar.gz](https://github.com/async-profiler/async-profiler/releases/download/v4.2/async-profiler-4.2-linux-arm64.tar.gz)
- macOS arm64/x64: [async-profiler-4.2-macos.zip](https://github.com/async-profiler/async-profiler/releases/download/v4.2/async-profiler-4.2-macos.zip)
- Profile converters: [jfr-converter.jar](https://github.com/async-profiler/async-profiler/releases/download/v4.2/jfr-converter.jar)

### Nightly builds

[The most recent binaries](https://github.com/async-profiler/async-profiler/releases/tag/nightly) corresponding
to the latest successful commit in `master`.

For a build corresponding to one of the previous commits, go to
[Nightly Builds](https://github.com/async-profiler/async-profiler/actions/workflows/test-and-publish-nightly.yml),
click the desired build and scroll down to the artifacts section. These binaries are kept for 30 days.

# Quick start

In a typical use case, profiling a Java application is just a matter of a running `asprof` with a PID of a
running Java process.

```
$ asprof -d 30 -f flamegraph.html <PID>
```

The above command translates to: run profiler for 30 seconds and save results to `flamegraph.html`
as an interactive [Flame Graph](docs/FlamegraphInterpretation.md) that can be viewed in a browser.

[![FlameGraph](/.assets/images/flamegraph.png)](https://htmlpreview.github.io/?https://github.com/async-profiler/async-profiler/blob/master/.assets/html/flamegraph.html)

Find more details in the [Getting started guide](docs/GettingStarted.md).

# Building

### Build status

[![Build Status](https://github.com/async-profiler/async-profiler/actions/workflows/test-and-publish-nightly.yml/badge.svg?branch=master)](https://github.com/async-profiler/async-profiler/actions/workflows/test-and-publish-nightly.yml)

### Minimum requirements

- make
- GCC 7.5.0+ or Clang 7.0.0+
- Static version of libstdc++ (e.g. on Amazon Linux 2023: `yum install libstdc++-static`)
- JDK 11+

### How to build

Make sure `gcc`, `g++` and `java` are available on the `PATH`.
Navigate to the root directory with async-profiler sources and run `make`.
async-profiler launcher will be available at `build/bin/asprof`.

Other Makefile targets:

- `make test` - run unit and integration tests;
- `make release` - package async-profiler binaries as `.tar.gz` (Linux) or `.zip` (macOS).

### Supported platforms

|           | Officially maintained builds | Other available ports                     |
| --------- | ---------------------------- | ----------------------------------------- |
| **Linux** | x64, arm64                   | x86, arm32, ppc64le, riscv64, loongarch64 |
| **macOS** | x64, arm64                   |                                           |

# Documentation

## Basic usage

- [Getting Started](docs/GettingStarted.md)
- [Profiler Options](docs/ProfilerOptions.md)
- [Profiling Modes](docs/ProfilingModes.md)
- [Integrating async-profiler](docs/IntegratingAsyncProfiler.md)
- [Profiling In Container](docs/ProfilingInContainer.md)

## Profiler output

- [Output Formats](docs/OutputFormats.md)
- [FlameGraph Interpretation](docs/FlamegraphInterpretation.md)
- [JFR Visualization](docs/JfrVisualization.md)
- [Converter Usage](docs/ConverterUsage.md)
- [Heatmap](docs/Heatmap.md)

## Advanced usage

- [CPU Sampling Engines](docs/CpuSamplingEngines.md)
- [Stack Walking Modes](docs/StackWalkingModes.md)
- [Advanced Stacktrace Features](docs/AdvancedStacktraceFeatures.md)
- [Profiling Non-Java Applications](docs/ProfilingNonJavaApplications.md)

## Troubleshooting

For known issues faced while running async-profiler and their detailed troubleshooting,
please refer [here](docs/Troubleshooting.md).
