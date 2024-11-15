![](https://github.com/async-profiler/async-profiler/blob/master/.assets/images/AsyncProfiler.png)

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

For the build corresponding to a previous commit, go to
[Publish Nightly Builds](https://github.com/async-profiler/async-profiler/actions/workflows/test-and-publish-nightly.yml),
click the desired build and scroll down to the artifacts section. These binaries are kept for 30 days.

# Supported platforms

|           | Officially maintained builds | Other available ports                     |
|-----------|------------------------------|-------------------------------------------|
| **Linux** | x64, arm64                   | x86, arm32, ppc64le, riscv64, loongarch64 |
| **macOS** | x64, arm64                   |                                           |

# Getting started

In a typical use case, profiling a Java application is just a matter of a running `asprof` with a PID of a
running Java process.
```
$ asprof -d 30 -f /tmp/flamegraph.html <PID>
```

[![Sample FlameGraph](https://github.com/async-profiler/async-profiler/blob/master/demo/flamegraph.png)](https://htmlpreview.github.io/?https://github.com/async-profiler/async-profiler/blob/master/demo/flamegraph.html)

For the detailed walkthrough, please refer to the
[Getting Started Guide](https://github.com/async-profiler/async-profiler/blob/master/docs/GettingStarted.md).

# Output formats

async-profiler can generate profile outputs in multiple formats which have been explained in details in the
[Output Formats Documentation](https://github.com/async-profiler/async-profiler/blob/master/docs/OutputFormats.md)

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

# JFR Visualization

`jfr` output from async-profiler can be visualized in a human-readable format using multiple
options, listed and explained in the
[JFR Visualization Documentation](https://github.com/async-profiler/async-profiler/blob/master/docs/JfrVisualization.md).

# Troubleshooting

For known issues faced while running async-profiler and their detailed troubleshooting,
please refer [here](https://github.com/async-profiler/async-profiler/blob/master/docs/Troubleshooting.md).

# List of all async-profiler documentation

The below list includes all documentation related to async-profiler, most of which are part of `README`:
* [Advanced Stacktrace Features](https://github.com/async-profiler/async-profiler/blob/master/docs/AdvancedStacktraceFeatures.md)
* [Converter Usage](https://github.com/async-profiler/async-profiler/blob/master/docs/ConverterUsage.md)
* [CPU Sampling Engines](https://github.com/async-profiler/async-profiler/blob/master/docs/CpuSamplingEngines.md)
* [FlameGraph Interpretation](https://github.com/async-profiler/async-profiler/blob/master/docs/FlamegraphInterpretation.md)
* [Getting Started](https://github.com/async-profiler/async-profiler/blob/master/docs/GettingStarted.md)
* [JFR Visualization](https://github.com/async-profiler/async-profiler/blob/master/docs/JfrVisualization.md)
* [Other Use Cases](https://github.com/async-profiler/async-profiler/blob/master/docs/OtherUseCases.md)
* [Output Formats](https://github.com/async-profiler/async-profiler/blob/master/docs/OutputFormats.md)
* [Profiler Options](https://github.com/async-profiler/async-profiler/blob/master/docs/ProfilerOptions.md)
* [Profiling In Container](https://github.com/async-profiler/async-profiler/blob/master/docs/ProfilingInContainer.md)
* [Profiling Modes](https://github.com/async-profiler/async-profiler/blob/master/docs/ProfilingModes.md)
* [Profiling Non-Java Applications](https://github.com/async-profiler/async-profiler/blob/master/docs/ProfilingNonJavaApplications.md)
* [StackWalkingModes](https://github.com/async-profiler/async-profiler/blob/master/docs/StackWalkingModes.md)
* [Troubleshooting](https://github.com/async-profiler/async-profiler/blob/master/docs/Troubleshooting.md)