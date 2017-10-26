# Changelog

## [Unreleased]

### Features
 - Linux Perf Events profiling: CPU cycles, cache misses, branch misses, page faults, context switches etc.
 - Kernel tracepoints support
 - Contended monitor (aka intrinsic lock) profiling

### Improvements
 - `list` command-line option to list supported events
 - Automatically find target process ID with `jps` tool
 - An option to include counter value in `collapsed` output
 - Friendly class names in allocation profile
 - Split allocations in new TLAB vs. outside TLAB

### Changes
 - Replaced `-m` modes with `-e` events

## [1.0] - 2017-10-09

### Features
 - CPU profiler without Safepoint bias
 - Lightweight Allocation profiler
 - Java, native and kernel stack traces
 - FlameGraph compatible output
