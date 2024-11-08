![](https://github.com/async-profiler/async-profiler/blob/master/.assets/images/AsyncProfiler.png)

# Table of contents
- [About](#about)
- [Download](#download)
- [Supported platforms](#supported-platforms)
- [Getting started](#getting-started)
- [Output formats](#output-formats)
- [Profiling modes](#profiling-modes)
- [Converter usage & demo](#converter-usage--demo)
- [Profiler options](#profiler-options)
- [Profiling Java in a container](#profiling-java-in-a-container)
- [Profiling Non-Java applications](#profiling-non-java-applications)
- [Building](#building)
- [Troubleshooting](#troubleshooting)

# About

This project is a low overhead sampling profiler for Java
that does not suffer from [Safepoint bias problem](http://psy-lob-saw.blogspot.ru/2016/02/why-most-sampling-java-profilers-are.html).
It features HotSpot-specific APIs to collect stack traces
and to track memory allocations. The profiler works with
OpenJDK and other Java runtimes based on the HotSpot JVM.

async-profiler can trace the following kinds of events:
- CPU cycles
- Hardware and Software performance counters like cache misses, branch misses, page faults, context switches etc.
- Allocations in Java Heap
- Contented lock attempts, including both Java object monitors and ReentrantLocks

See our [3 hours playlist](https://www.youtube.com/playlist?list=PLNCLTEx3B8h4Yo_WvKWdLvI9mj1XpTKBr)
to learn about more features.

# Download

Current release (3.0):

- Linux x64: [async-profiler-3.0-linux-x64.tar.gz](https://github.com/async-profiler/async-profiler/releases/download/v3.0/async-profiler-3.0-linux-x64.tar.gz)
- Linux arm64: [async-profiler-3.0-linux-arm64.tar.gz](https://github.com/async-profiler/async-profiler/releases/download/v3.0/async-profiler-3.0-linux-arm64.tar.gz)
- macOS x64/arm64: [async-profiler-3.0-macos.zip](https://github.com/async-profiler/async-profiler/releases/download/v3.0/async-profiler-3.0-macos.zip)
- Converters between profile formats: [converter.jar](https://github.com/async-profiler/async-profiler/releases/download/v3.0/converter.jar)  
  (JFR to Flame Graph, JFR to pprof, collapsed stacks to Flame Graph)

[Previous releases](https://github.com/async-profiler/async-profiler/releases)

async-profiler also comes bundled with IntelliJ IDEA Ultimate 2018.3 and later.  
For more information refer to [IntelliJ IDEA documentation](https://www.jetbrains.com/help/idea/cpu-and-allocation-profiling-basic-concepts.html).

[Nightly releases](https://github.com/async-profiler/async-profiler/releases/tag/nightly) (published on each commit to master)

For the build corresponding to a previous commit, go to the corresponding `Publish Nightly Builds` Github Action and scroll down to the artifacts section. These binaries are kept for 30 days.

# Supported platforms

|           | Officially maintained builds | Other available ports                     |
|-----------|------------------------------|-------------------------------------------|
| **Linux** | x64, arm64                   | x86, arm32, ppc64le, riscv64, loongarch64 |
| **macOS** | x64, arm64                   |                                           |

# Getting started
In this section, we will get acquainted with using async-profiler to profile applications and
analyze the profile output in the Flame Graph format.

For the detailed walkthrough, please refer to the
[Getting Started Guide](https://github.com/async-profiler/async-profiler/blob/master/docs/GettingStarted.md).

# Output formats

async-profiler currently supports the below output formats:
* `collapsed` - This is a collection of call stacks, where each line is a  semicolon separated list of frames followed by a counter. This is used by the FlameGraph script to generate the FlameGraph visualization of the profile data.
* `flamegraph` - Flamegraph is a hierarchical representation of call traces of the profiled software in a color coded format that helps to identify a particular resource usage like CPU and memory for the application.
* `tree` - Profile output generated in an html format showing a tree view of resource usage beginning with the call stack with highest resource usage and then showing other  call stacks in descending order of resource usage. Expanding a parent frame follows the same hierarchical representation within that frame.
* `jfr` - Java Flight Recording(JFR) is a widely known tool for profiling Java applications. async-profiler can generate output in jfr format compatible with tools capable of viewing and analyzing `jfr` files. The `jfr` format collects data about the JVM as well as the Java application running on it. 
* `text` -  If no output format is specified with `-o` and filename has no extension provided, profiled output is generated in text format.

# Profiling modes
The [Getting Started](#getting-started) section focused mostly on CPU usage profiling. However,
async-profiler provides various other profiling modes like `Allocation`, `Wall Clock`, `Java Method`
and even a `Multiple Events` profiling mode.

For the detailed explanation on all profiling modes, please refer to the
[Profiling Modes Documentation](https://github.com/async-profiler/async-profiler/blob/master/docs/Profiling.md).

# Converter usage & demo
async-profiler provides profile outputs in formats like `collapsed`, `html`, `jfr`. Further,
async-profiler provider a converter utility to convert the profile output to other popular formats.

## Supported conversions

* collapsed -> html, collapsed
* html -> html, collapsed
* jfr -> html, collapsed, pprof, pb.gz

For the detailed usage instructions and demo, please refer to
[Converter Usage & Demo](https://github.com/async-profiler/async-profiler/blob/master/docs/ConverterUsage.md).

# Profiler options

async-profiler provides many options for both `asprof` binary and
[Launching as an agent](https://github.com/async-profiler/async-profiler/blob/master/docs/OtherUseCases.md#launching-as-an-agent)
to produce profiling outputs catering to specific needs.

For a detailed guide on all the available options, please refer to
[Profiler Options Details](https://github.com/async-profiler/async-profiler/blob/master/docs/ProfilerOptions.md)

# Profiling Java in a container

async-profiler provides the ability to profile Java processes running in a Docker or LXC
container both from within a container and from the host system.

For more details, please refer to
[Profiling In Container Documentation](https://github.com/async-profiler/async-profiler/blob/master/docs/ConverterUsage.md).

# Profiling Non-Java applications

Unlike traditional Java profilers, async-profiler monitors non-Java threads (e.g., GC threads)
and also shows native frames in Java stack traces. This enables it to work with C and C++
applications.

For more details, please refer to
[Profiling Non-Java Applications](https://github.com/async-profiler/async-profiler/blob/master/docs/ProfilingNonJavaApplications.md).

# Building

Build status: [![Build Status](https://github.com/async-profiler/async-profiler/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/async-profiler/async-profiler/actions/workflows/ci.yml)

## Build requirements
* JDK: 11+
* GCC: 7.5.0+

Make sure the `JAVA_HOME` environment variable points to your JDK installation,
and then run `make`. GCC or Clang is required. After building, the profiler binaries
will be in the `build` subdirectory.

# Troubleshooting

For known issues faced while running async-profiler and their detailed troubleshooting,
please refer [here](https://github.com/async-profiler/async-profiler/blob/master/docs/Troubleshooting.md).
