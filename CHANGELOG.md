# Changelog

## [3.0] - 2024-01-20

### Features
 - #724: Binary launcher `asprof`
 - #751: Profile non-Java processes
 - #795: AsyncGetCallTrace replacement
 - #719: Classify execution samples into categories in JFR converter
 - #855: `ctimer` mode for accurate profiling without perf_events
 - #740: Profile CPU + Wall clock together
 - #736: Show targets of vtable/itable calls
 - #777: Show JIT compilation task
 - #644: RISC-V port
 - #770: LoongArch64 port

### Improvements
 - #733: Make the same `libasyncProfiler` work with both glibc and musl
 - #734: Support raw PMU event descriptors
 - #759: Configure alternative profiling signal
 - #761: Parse dynamic linking structures
 - #723: `--clock` option to select JFR timestamp source
 - #750: `--jfrsync` may specify a list of JFR events
 - #849: Parse concatenated multi-chunk JFRs
 - #833: Time-to-safepoint JFR event
 - #832: Normalize names of hidden classes / lambdas
 - #864: Reduce size of HTML Flame Graph
 - #783: Shutdown asprof gracefully on SIGTERM
 - Better demangling of C++ and Rust symbols
 - DWARF unwinding for ARM64
 - `JfrReader` can parse in-memory buffer
 - Support custom events in `JfrReader`
 - An option to read JFR file by chunks
 - Record `GCHeapSummary` events in JFR

### Bug fixes
 - Workaround macOS crashes in SafeFetch
 - Fixed attach to OpenJ9 on macOS
 - Support `UseCompressedObjectHeaders` aka Lilliput
 - Fixed allocation profiling on JDK 20.0.x
 - Fixed context-switches profiling
 - Prefer ObjectSampler to TLAB hooks for allocation profiling
 - Improved accuracy of ObjectSampler in `--total` mode
 - Make Flame Graph status line and search results always visible
 - `loop` and `timeout` options did not work in some modes
 - Restart interrupted poll/epoll_wait syscalls
 - Fixed stack unwinding issues on ARM64
 - Workaround for stale jmethodIDs
 - Calculate ELF base address correctly
 - Do not dump redundant threads in a JFR chunk
 - `check` action prints result to a file
 - Annotate JFR unit types with `@ContentType`

## [2.9] - 2022-11-27

### Features
 - Java Heap leak profiler
 - `meminfo` command to print profiler's memory usage
 - Profiler API with embedded agent as a Maven artifact

### Improvements
 - `--include`/`--exclude` options in the FlameGraph converter
 - `--simple` and `--dot` options in jfr2flame converter
 - An option for agressive recovery of `[unknown_Java]` stack traces
 - Do not truncate signatures in collapsed format
 - Display inlined frames under a runtime stub

### Bug fixes
 - Profiler did not work with Homebrew JDK
 - Fixed allocation profiling on Zing
 - Various `jfrsync` fixes
 - Symbol parsing fixes
 - Attaching to a container on Linux 3.x could fail

## [2.8.3] - 2022-07-16

### Improvements
 - Support virtualized ARM64 macOS
 - A switch to generate auxiliary events by async-profiler or FlightRecorder in jfrsync mode

### Bug fixes
 - Could not recreate perf_events after the first failure
 - Handle different versions of Zing properly
 - Do not call System.loadLibrary, when libasyncProfiler is preloaded

## [2.8.2] - 2022-07-13

### Bug fixes
 - The same .so works with glibc and musl
 - dlopen hook did not work on Arch Linux
 - Fixed JDK 7 crash
 - Fixed CPU profiling on Zing

### Changes
 - Mark interpreted frames with `_[0]` in collapsed output
 - Double click selects a method name on a flame graph

## [2.8.1] - 2022-06-10

### Improvements
 - JFR to pprof converter (contributed by @NeQuissimus)
 - JFR converter improvements: time range, collapsed output, pattern highlighting
 - `%n` pattern in file names; limit number of output files
 - `--lib` to customize profiler library path in a container
 - `profiler.sh list` command now works without PID

### Bug fixes
 - Fixed crashes related to continuous profiling
 - Fixed Alpine/musl compatibility issues
 - Fixed incomplete collapsed output due to weird locale settings
 - Workaround for JDK-8185348

## [2.8] - 2022-05-09

### Features
 - Mark top methods as interpreted, compiled (C1/C2), or inlined
 - JVM TI based allocation profiling for JDK 11+
 - Embedded HTTP management server

### Improvements
 - Re-implemented stack recovery for better reliability
 - Add `loglevel` argument
 - Do not mmap perf page in `--all-user` mode
 - Distinguish runnable/sleeping threads in OpenJ9 wall-clock profiler
 - `--cpu` converter option to extract CPU profile from the wall-clock output

## [2.7] - 2022-02-14

### Features
 - Experimental support for OpenJ9 VM
 - DWARF stack unwinding

### Improvements
 - Better handling of VM threads (fixed missing JIT threads)
 - More reliable recovery from `not_walkable` AGCT failures
 - Do not accept unknown agent arguments

## [2.6] - 2022-01-09

### Features
 - Continuous profiling; `loop` and `timeout` options

### Improvements
 - Reliability improvements: avoid certain crashes and deadlocks
 - Smaller and faster agent library
 - Minor `jfr` and `jfrsync` enhancements (see the commit log)

## [2.5.1] - 2021-12-05

### Bug fixes
 - Prevent early unloading of libasyncProfiler.so
 - Read kernel symbols only for perf_events
 - Escape backslashes in flame graphs
 - Avoid duplicate categories in `jfrsync` mode
 - Fixed stack overflow in RedefineClasses
 - Fixed deadlock when flushing JFR

### Improvements
 - Support OpenJDK C++ Interpreter (aka Zero)
 - Allow reading incomplete JFR recordings

## [2.5] - 2021-10-01

### Features
 - macOS/ARM64 (aka Apple M1) port
 - PPC64LE port (contributed by @ghaug)
 - Profile low-privileged processes with perf_events (contributed by @Jongy)
 - Raw PMU events; kprobes & uprobes
 - Dump results in the middle of profiling session
 - Chunked JFR; support JFR files larger than 2 GB
 - Integrate async-profiler events with JDK Flight Recordings

### Improvements
 - Use RDTSC for JFR timestamps when possible
 - Show line numbers and bci in Flame Graphs
 - jfr2flame can produce Allocation and Lock flame graphs
 - Flame Graph title depends on the event and `--total`
 - Include profiler logs and native library list in JFR output
 - Lock profiling no longer requires JVM symbols
 - Better container support
 - Native function profiler can count the specified argument
 - An option to group threads by scheduling policy
 - An option to prepend library name to native symbols

### Notes
 - macOS build is provided as a fat binary that works both on x86-64 and ARM64
 - 32-bit binaries are no longer shipped. It is still possible to build them from sources
 - Dropped JDK 6 support (may still work though)

## [2.0] - 2021-03-14

### Features
 - Profile multiple events together (cpu + alloc + lock)
 - HTML 5 Flame Graphs: faster rendering, smaller size
 - JFR v2 output format, compatible with FlightRecorder API
 - JFR to Flame Graph converter
 - Automatically turn profiling on/off at `--begin`/`--end` functions
 - Time-to-safepoint profiling: `--ttsp`

### Improvements
 - Unlimited frame buffer. Removed `-b` option and 64K stack traces limit
 - Additional JFR events: OS, CPU, and JVM information; CPU load
 - Record bytecode indices / line numbers
 - Native stack traces for Java events
 - Improved CLI experience
 - Better error handling; an option to log warnings/errors to a dedicated stream
 - Reduced the amount of unknown stack traces

### Changes
 - Removed non-ASL code. No more CDDL license

## [1.8.4] - 2021-02-24

### Improvements
 - Smaller and faster agent library

### Bug fixes
 - Fixed JDK 7 crash during wall-clock profiling

## [1.8.3] - 2021-01-06

### Improvements
 - libasyncProfiler.dylib symlink on macOS

### Bug fixes
 - Fixed possible deadlock on non-HotSpot JVMs
 - Gracefully stop profiler when terminating JVM
 - Fixed GetStackTrace problem after RedefineClasses

## [1.8.2] - 2020-11-02

### Improvements
 - AArch64 build is now provided out of the box
 - Compatibility with JDK 15 and JDK 16

### Bug fixes
 - More careful native stack walking in wall-clock mode
 - `resume` command is not compatible with JFR format
 - Wrong allocation sizes on JDK 8u262

## [1.8.1] - 2020-09-05

### Improvements
 - Possibility to specify application name instead of `pid` (contributed by @yuzawa-san)

### Bug fixes
 - Fixed long attach time and slow class loading on JDK 8
 - `UnsatisfiedLinkError` during Java method profiling
 - Avoid reading `/proc/kallsyms` when `--all-user` is specified

## [1.8] - 2020-08-10

### Features
 - Converters between different output formats:
   - JFR -> nflx (FlameScope)
   - Collapsed stacks -> HTML 5 Flame Graph 

### Improvements
 - `profiler.sh` no longer requires bash (contributed by @cfstras)
 - Fixed long attach time and slow class loading on JDK 8
 - Fixed deadlocks in wall-clock profiling mode
 - Per-thread reverse Flame Graph and Call Tree
 - ARM build now works with ARM and THUMB flavors of JDK

### Changes
 - Release package is extracted into a separate folder

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
