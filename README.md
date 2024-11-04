![](https://github.com/async-profiler/async-profiler/blob/master/.assets/images/AsyncProfiler.png)

## About

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

## Download

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

## Supported platforms

|           | Officially maintained builds | Other available ports                     |
|-----------|------------------------------|-------------------------------------------|
| **Linux** | x64, arm64                   | x86, arm32, ppc64le, riscv64, loongarch64 |
| **macOS** | x64, arm64                   |                                           |

## Getting Started
In this section, we will get acquainted with using async-profiler to profile applications and
analyze the profile output in the Flame Graph format.

For the detailed walkthrough, please refer to the
[Getting Started Guide](https://github.com/async-profiler/async-profiler/blob/master/docs/GettingStarted.md).

## Profiling Modes
The [Getting Started](#getting-started) section focused mostly on CPU usage profiling. However,
async-profiler provides various other profiling modes like `Allocation`, `Wall Clock`, `Java Method`
and even a `Multiple Events` profiling mode.

For the detailed explanation on all profiling modes, please refer to the
[Profiling Modes Documentation](https://github.com/async-profiler/async-profiler/blob/master/docs/Profiling.md).

## Converter Usage & Demo
async-profiler provides profile outputs in formats like `collapsed`, `html`, `jfr`. Further,
async-profiler provider a converter utility to convert the profile output to other popular formats.

### Supported conversions

* collapsed -> html, collapsed
* html -> html, collapsed
* jfr -> html, collapsed, pprof, pb.gz

For the detailed usage instructions and demo, please refer to
[Converter Usage & Demo](https://github.com/async-profiler/async-profiler/blob/master/docs/ConverterUsage.md).

## Profiler Options

async-profiler provides many options for both `asprof` binary and
[Launching as an agent](https://github.com/async-profiler/async-profiler/blob/master/docs/OtherUseCases.md#launching-as-an-agent)
to produce profiling outputs catering to specific needs.

For a detailed guide on all the available options, please refer to
[Profiler Options Details](https://github.com/async-profiler/async-profiler/blob/master/docs/ProfilerOptions.md)

## Profiling Java in a container

async-profiler provides the ability to profile Java processes running in a Docker or LXC
container both from within a container and from the host system.

For more details, please refer to
[Profiling In Container Documentation](https://github.com/async-profiler/async-profiler/blob/master/docs/ConverterUsage.md).

## Profiling Non-Java Applications

Unlike traditional Java profilers, async-profiler monitors non-Java threads (e.g., GC threads)
and also shows native frames in Java stack traces. This enables it to work with C and C++
applications.

For more details, please refer to
[Profiling Non-Java Applications](https://github.com/async-profiler/async-profiler/blob/master/docs/ProfilingNonJavaApplications.md).

## Building

Build status: [![Build Status](https://github.com/async-profiler/async-profiler/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/async-profiler/async-profiler/actions/workflows/ci.yml)

### Build requirements
* JDK: 11+
* GCC: 4.7+
* C++ Standard: 17-

Make sure the `JAVA_HOME` environment variable points to your JDK installation,
and then run `make`. GCC or Clang is required. After building, the profiler binaries
will be in the `build` subdirectory.

## Restrictions/Limitations

* macOS profiling is limited to user space code only.

* On most Linux systems, `perf_events` captures call stacks with a maximum depth
  of 127 frames. On recent Linux kernels, this can be configured using
  `sysctl kernel.perf_event_max_stack` or by writing to the
  `/proc/sys/kernel/perf_event_max_stack` file.

* Profiler allocates 8kB perf_event buffer for each thread of the target process.
  Make sure `/proc/sys/kernel/perf_event_mlock_kb` value is large enough
  (more than `8 * threads`) when running under unprivileged user.
  Otherwise the message _"perf_event mmap failed: Operation not permitted"_
  will be printed, and no native stack traces will be collected.

* You will not see the non-Java frames _preceding_ the Java frames on the
  stack, unless `--cstack vm` is specified.
  For example, if `start_thread` called `JavaMain` and then your Java
  code started running, you will not see the first two frames in the resulting
  stack. On the other hand, you _will_ see non-Java frames (user and kernel)
  invoked by your Java code.

* No Java stacks will be collected if `-XX:MaxJavaStackTraceDepth` is zero
  or negative. The exception is `--cstack vm` mode, which does not take
  `MaxJavaStackTraceDepth` into account.

* Too short profiling interval may cause continuous interruption of heavy
  system calls like `clone()`, so that it will never complete;
  see [#97](https://github.com/async-profiler/async-profiler/issues/97).
  The workaround is simply to increase the interval.

* When agent is not loaded at JVM startup (by using -agentpath option) it is
  highly recommended to use `-XX:+UnlockDiagnosticVMOptions -XX:+DebugNonSafepoints` JVM flags.
  Without those flags the profiler will still work correctly but results might be
  less accurate. For example, without `-XX:+DebugNonSafepoints` there is a high chance
  that simple inlined methods will not appear in the profile. When the agent is attached at runtime,
  `CompiledMethodLoad` JVMTI event enables debug info, but only for methods compiled after attaching.

## Troubleshooting

For known issues faced while running async-profiler and their detailed troubleshooting,
please refer [here](https://github.com/async-profiler/async-profiler/blob/master/docs/Troubleshooting.md).
