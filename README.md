# async-profiler

[![Build Status](https://travis-ci.org/jvm-profiling-tools/async-profiler.svg?branch=master)](https://travis-ci.org/jvm-profiling-tools/async-profiler)

This project is a low overhead sampling profiler for Java
that does not suffer from [Safepoint bias problem](http://psy-lob-saw.blogspot.ru/2016/02/why-most-sampling-java-profilers-are.html).
It features HotSpot-specific APIs to collect stack traces
and to track memory allocations. The profiler works with
OpenJDK, Oracle JDK and other Java runtimes based on the HotSpot JVM.

async-profiler can trace the following kinds of events:
 - CPU cycles
 - Hardware and Software performance counters like cache misses, branch misses, page faults, context switches etc.
 - Allocations in Java Heap
 - Contented lock attempts, including both Java object monitors and ReentrantLocks
 
## Usage

See our [Wiki](https://github.com/jvm-profiling-tools/async-profiler/wiki) or [3 hours playlist](https://www.youtube.com/playlist?list=PLNCLTEx3B8h4Yo_WvKWdLvI9mj1XpTKBr) to learn about all set of features. 

## Download

Latest release (1.8.3):

 - Linux x64 (glibc): [async-profiler-1.8.3-linux-x64.tar.gz](https://github.com/jvm-profiling-tools/async-profiler/releases/download/v1.8.3/async-profiler-1.8.3-linux-x64.tar.gz)
 - Linux x86 (glibc): [async-profiler-1.8.3-linux-x86.tar.gz](https://github.com/jvm-profiling-tools/async-profiler/releases/download/v1.8.3/async-profiler-1.8.3-linux-x86.tar.gz)
 - Linux x64 (musl): [async-profiler-1.8.3-linux-musl-x64.tar.gz](https://github.com/jvm-profiling-tools/async-profiler/releases/download/v1.8.3/async-profiler-1.8.3-linux-musl-x64.tar.gz)
 - Linux ARM: [async-profiler-1.8.3-linux-arm.tar.gz](https://github.com/jvm-profiling-tools/async-profiler/releases/download/v1.8.3/async-profiler-1.8.3-linux-arm.tar.gz)
 - Linux AArch64: [async-profiler-1.8.3-linux-aarch64.tar.gz](https://github.com/jvm-profiling-tools/async-profiler/releases/download/v1.8.3/async-profiler-1.8.3-linux-aarch64.tar.gz)
 - macOS x64: [async-profiler-1.8.3-macos-x64.tar.gz](https://github.com/jvm-profiling-tools/async-profiler/releases/download/v1.8.3/async-profiler-1.8.3-macos-x64.tar.gz)

[Early access](https://github.com/jvm-profiling-tools/async-profiler/releases/tag/v2.0-b1) (2.0-b1):

 - Linux x64 (glibc): [async-profiler-2.0-b1-linux-x64.tar.gz](https://github.com/jvm-profiling-tools/async-profiler/releases/download/v2.0-b1/async-profiler-2.0-b1-linux-x64.tar.gz)
 - macOS x64: [async-profiler-2.0-b1-macos-x64.tar.gz](https://github.com/jvm-profiling-tools/async-profiler/releases/download/v2.0-b1/async-profiler-2.0-b1-macos-x64.tar.gz)

[Previous releases](https://github.com/jvm-profiling-tools/async-profiler/releases)

## Supported platforms

- **Linux** / x64 / x86 / ARM / AArch64
- **macOS** / x64

Note: macOS profiling is limited to user space code only.

## Building

Make sure the `JAVA_HOME` environment variable points to your JDK installation,
and then run `make`. GCC is required. After building, the profiler agent binary
will be in the `build` subdirectory. Additionally, a small application `jattach`
that can load the agent into the target process will also be compiled to the
`build` subdirectory.

