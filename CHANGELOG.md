# Changelog

## [1.7.1] - 2020-05-14

### Features
 - LBR call stack support (available since Haswell)

### Improvements
 - `--filter` to profile only specified thread IDs in wall-clock mode
 - `--safe-mode` to disable selected stack recovery techniques

## [1.7] - 2020-03-17

### Features
 - Profile invocations of arbitrary Java methods
 - Filter stack traces by the given name pattern
 - Java API to filter monitored threads
 - `--cstack`/`--no-cstack` option

### Improvements
 - Thread names and Java thread IDs in JFR output
 - Wall clock profiler distinguishes RUNNABLE vs. SLEEPING threads
 - Stable profiling interval in wall clock mode
 - C++ function names as events, e.g. `-e VMThread::execute`
 - `check` command to test event availability
 - Allow shading of AsyncProfiler API
 - Enable CPU profiling on WSL
 - Enable allocation profiling on Zing
 - Reduce the amount of `unknown_Java` samples

## [1.6] - 2019-09-09

### Features
 - Pause/resume profiling
 - Allocation profiling support for JDK 12, 13 (contributed by @rraptorr)

### Improvements
 - Include all AsyncGetCallTrace failures in the profile
 - Parse symbols of JNI libraries loaded in runtime
 - The agent autodetects output format by the file extension
 - Output file name patterns: `%p` and `%t`
 - `-g` option to print method signatures
 - `-j` can increase the maximum Java stack depth
 - Allocaton sampling rate can be adjusted with `-i`
 - Improved reliability on macOS

### Changes
 - `-f` file names are now relative to the current shell directory

## [1.5] - 2019-01-08

### Features
 - Wall-clock profiler: `-e wall`
 - `-e itimer` mode for systems that do not support perf_events
 - Native stack traces on macOS
 - Support for Zing runtime, except allocation profiling

### Improvements
 - `--all-user` option to allow profiling with restricted
   `perf_event_paranoid` (contributed by @jpbempel)
 - `-a` option to annotate method names
 - Improved attach to containerized and chroot'ed JVMs
 - Native function profiling now accepts non-public symbols
 - Better mapping of Java thread names (contributed by @KirillTim)

### Changes
 - Changed default profiling engine on macOS
 - Fixed the order of stack frames in JFR format

## [1.4] - 2018-06-24

### Features
 - Interactive Call tree and Backtrace tree in HTML format (contributed by @rpulle)
 - Experimental support for Java Flight Recorder (JFR) compatible output
 
### Improvements
 - Added units: `ms`, `us`, `s` and multipliers: `K`, `M`, `G` for interval argument
 - API and command-line option `-v` for profiler version
 - Allow profiling containerized JVMs on older kernels

### Changes
 - Default CPU sampling interval reduced to 10 ms
 - Changed the text format of flat profile

## [1.3] - 2018-05-13

### Features
 - Profiling of native functions, e.g. malloc
 
### Improvements
 - JDK 9, 10, 11 support for heap profiling with accurate stack traces
 - `root` can now profile Java processes of any user
 - `-j` option for limiting Java stack depth

## [1.2] - 2018-03-05

### Features
 - Produce SVG files out of the box; flamegraph.pl is no longer needed
 - Profile ReentrantLock contention
 - Java API
 
### Improvements
 - Allocation and Lock profiler now works on JDK 7, too
 - Faster dumping of results

### Changes
 - `total` counter of allocation profiler now measures heap pressure (like JMC)

## [1.1] - 2017-12-03

### Features
 - Linux Perf Events profiling: CPU cycles, cache misses, branch misses, page faults, context switches etc.
 - Kernel tracepoints support
 - Contended monitor (aka intrinsic lock) profiling
 - Individual thread profiles

### Improvements
 - Profiler can engage at JVM start and automatically dump results on exit
 - `list` command-line option to list supported events
 - Automatically find target process ID with `jps` tool
 - An option to include counter value in `collapsed` output
 - Friendly class names in allocation profile
 - Split allocations in new TLAB vs. outside TLAB

### Changes
 - Replaced `-m` modes with `-e` events
 - Interval changed from `int` to `long`

## [1.0] - 2017-10-09

### Features
 - CPU profiler without Safepoint bias
 - Lightweight Allocation profiler
 - Java, native and kernel stack traces
 - FlameGraph compatible output
