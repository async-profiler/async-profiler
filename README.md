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

See our [Wiki](https://github.com/async-profiler/async-profiler/wiki) or
[3 hours playlist](https://www.youtube.com/playlist?list=PLNCLTEx3B8h4Yo_WvKWdLvI9mj1XpTKBr)
to learn about all features.

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

## Supported platforms

|           | Officially maintained builds | Other available ports                     |
|-----------|------------------------------|-------------------------------------------|
| **Linux** | x64, arm64                   | x86, arm32, ppc64le, riscv64, loongarch64 |
| **macOS** | x64, arm64                   |                                           |

## [Getting started](https://github.com/async-profiler/async-profiler/blob/master/docs/GettingStarted.md)

## [Profiling modes](https://github.com/async-profiler/async-profiler/blob/master/docs/Profiling.md)

## Profiler Options

`asprof` command-line options.

* `start` - starts profiling in semi-automatic mode, i.e. profiler will run
  until `stop` command is explicitly called.

* `resume` - starts or resumes earlier profiling session that has been stopped.
  All the collected data remains valid. The profiling options are not preserved
  between sessions, and should be specified again.

* `stop` - stops profiling and prints the report.

* `dump` - dump collected data without stopping profiling session.

* `check` - check if the specified profiling event is available.

* `status` - prints profiling status: whether profiler is active and
  for how long.

* `meminfo` - prints used memory statistics.

* `list` - show the list of profiling events available for the target process
  (if PID is specified) or for the default JVM.

* `-d N` - the profiling duration, in seconds. If no `start`, `resume`, `stop`
  or `status` option is given, the profiler will run for the specified period
  of time and then automatically stop.  
  Example: `asprof -d 30 8983`

* `-e event` - the profiling event: `cpu`, `alloc`, `lock`, `cache-misses` etc.
  Use `list` to see the complete list of available events.

  In allocation profiling mode the top frame of every call trace is the class
  of the allocated object, and the counter is the heap pressure (the total size
  of allocated TLABs or objects outside TLAB).

  In lock profiling mode the top frame is the class of lock/monitor, and
  the counter is number of nanoseconds it took to enter this lock/monitor.

  Two special event types are supported on Linux: hardware breakpoints
  and kernel tracepoints:
  - `-e mem:<func>[:rwx]` sets read/write/exec breakpoint at function
    `<func>`. The format of `mem` event is the same as in `perf-record`.
    Execution breakpoints can be also specified by the function name,
    e.g. `-e malloc` will trace all calls of native `malloc` function.
  - `-e trace:<id>` sets a kernel tracepoint. It is possible to specify
    tracepoint symbolic name, e.g. `-e syscalls:sys_enter_open` will trace
    all `open` syscalls.

* `-i N` - sets the profiling interval in nanoseconds or in other units,
  if N is followed by `ms` (for milliseconds), `us` (for microseconds),
  or `s` (for seconds). Only CPU active time is counted. No samples
  are collected while CPU is idle. The default is 10000000 (10ms).  
  Example: `asprof -i 500us 8983`

* `--alloc N` - allocation profiling interval in bytes or in other units,
  if N is followed by `k` (kilobytes), `m` (megabytes), or `g` (gigabytes).

* `--live` - retain allocation samples with live objects only
  (object that have not been collected by the end of profiling session).
  Useful for finding Java heap memory leaks.

* `--lock N` - lock profiling threshold in nanoseconds (or other units).
  In lock profiling mode, sample contended locks when total lock duration
  overflows the threshold.

* `-j N` - sets the maximum stack depth. The default is 2048.  
  Example: `asprof -j 30 8983`

* `-t` - profile threads separately. Each stack trace will end with a frame
  that denotes a single thread.  
  Example: `asprof -t 8983`

* `-s` - print simple class names instead of FQN.

* `-n` - normalize names of hidden classes / lambdas.

* `-g` - print method signatures.

* `-a` - annotate JIT compiled methods with `_[j]`, inlined methods with `_[i]`, interpreted methods with `_[0]` and C1 compiled methods with `_[1]`.

* `-l` - prepend library names to symbols, e.g. ``libjvm.so`JVM_DefineClassWithSource``.

* `-o fmt` - specifies what information to dump when profiling ends.
  `fmt` can be one of the following options:
  - `traces[=N]` - dump call traces (at most N samples);
  - `flat[=N]` - dump flat profile (top N hot methods);  
    can be combined with `traces`, e.g. `traces=200,flat=200`
  - `jfr` - dump events in Java Flight Recorder format readable by Java Mission Control.
    This *does not* require JDK commercial features to be enabled.
  - `collapsed` - dump collapsed call traces in the format used by
    [FlameGraph](https://github.com/brendangregg/FlameGraph) script. This is
    a collection of call stacks, where each line is a semicolon separated list
    of frames followed by a counter.
  - `flamegraph` - produce Flame Graph in HTML format.
  - `tree` - produce Call Tree in HTML format.  
    `--reverse` option will generate backtrace view.

* `--total` - count the total value of the collected metric instead of the number of samples,
  e.g. total allocation size.

* `--chunksize N`, `--chunktime N` - approximate size and time limits for a single JFR chunk.
  A new chunk will be started whenever either limit is reached.
  The default `chunksize` is 100MB, and the default `chunktime` is 1 hour.  
  Example: `asprof -f profile.jfr --chunksize 100m --chunktime 1h 8983`

* `-I include`, `-X exclude` - filter stack traces by the given pattern(s).
  `-I` defines the name pattern that *must* be present in the stack traces,
  while `-X` is the pattern that *must not* occur in any of stack traces in the output.
  `-I` and `-X` options can be specified multiple times. A pattern may begin or end with
  a star `*` that denotes any (possibly empty) sequence of characters.  
  Example: `asprof -I 'Primes.*' -I 'java/*' -X '*Unsafe.park*' 8983`

* `-L level` - log level: `debug`, `info`, `warn`, `error` or `none`.

* `-F features` - comma separated list of HotSpot-specific features
  to include in stack traces. Supported features are:
  - `vtable` - display targets of megamorphic virtual calls as an extra frame
    on top of `vtable stub` or `itable stub`.
  - `comptask` - display current compilation task (a Java method being compiled)
    in a JIT compiler stack trace.

* `--title TITLE`, `--minwidth PERCENT`, `--reverse` - FlameGraph parameters.  
  Example: `asprof -f profile.html --title "Sample CPU profile" --minwidth 0.5 8983`

* `-f FILENAME` - the file name to dump the profile information to.  
  `%p` in the file name is expanded to the PID of the target JVM;  
  `%t` - to the timestamp;  
  `%n{MAX}` - to the sequence number;  
  `%{ENV}` - to the value of the given environment variable.  
  Example: `asprof -o collapsed -f /tmp/traces-%t.txt 8983`

* `--loop TIME` - run profiler in a loop (continuous profiling).
  The argument is either a clock time (`hh:mm:ss`) or
  a loop duration in `s`econds, `m`inutes, `h`ours, or `d`ays.
  Make sure the filename includes a timestamp pattern, or the output
  will be overwritten on each iteration.  
  Example: `asprof --loop 1h -f /var/log/profile-%t.jfr 8983`

* `--all-user` - include only user-mode events. This option is helpful when kernel profiling
  is restricted by `perf_event_paranoid` settings.

* `--sched` - group threads by Linux-specific scheduling policy: BATCH/IDLE/OTHER.

* `--cstack MODE` - how to walk native frames (C stack). Possible modes are
  `fp` (Frame Pointer), `dwarf` (DWARF unwind info),
  `lbr` (Last Branch Record, available on Haswell since Linux 4.1),
  `vm` (HotSpot VM Structs) and `no` (do not collect C stack).

  By default, C stack is shown in cpu, ctimer, wall-clock and perf-events profiles.
  Java-level events like `alloc` and `lock` collect only Java stack.

* `--signal NUM` - use alternative signal for cpu or wall clock profiling.
  To change both signals, specify two numbers separated by a slash: `--signal SIGCPU/SIGWALL`.

* `--clock SOURCE` - clock source for JFR timestamps: `tsc` (default)
  or `monotonic` (equivalent for `CLOCK_MONOTONIC`).

* `--begin function`, `--end function` - automatically start/stop profiling
  when the specified native function is executed.

* `--ttsp` - time-to-safepoint profiling. An alias for  
  `--begin SafepointSynchronize::begin --end RuntimeService::record_safepoint_synchronized`  
  It is not a separate event type, but rather a constraint. Whatever event type
  you choose (e.g. `cpu` or `wall`), the profiler will work as usual, except that
  only events between the safepoint request and the start of the VM operation
  will be recorded.

* `--jfropts OPTIONS` - comma separated list of JFR recording options.
  Currently, the only available option is `mem` supported on Linux 3.17+.
  `mem` enables accumulating events in memory instead of flushing
  synchronously to a file.

* `--jfrsync CONFIG` - start Java Flight Recording with the given configuration
  synchronously with the profiler. The output .jfr file will include all regular
  JFR events, except that execution samples will be obtained from async-profiler.
  This option implies `-o jfr`.
  - `CONFIG` is a predefined JFR profile or a JFR configuration file (.jfc)
    or a list of JFR events started with `+`

  Example: `asprof -e cpu --jfrsync profile -f combined.jfr 8983`

* `--fdtransfer` - runs a background process that provides access to perf_events
  to an unprivileged process. `--fdtransfer` is useful for profiling a process
  in a container (which lacks access to perf_events) from the host.
  See [Profiling Java in a container](#profiling-java-in-a-container).

* `-v`, `--version` - prints the version of profiler library. If PID is specified,
  gets the version of the library loaded into the given process.

## Profiling Java in a container

It is possible to profile Java processes running in a Docker or LXC container
both from within a container and from the host system.

When profiling from the host, `pid` should be the Java process ID in the host
namespace. Use `ps aux | grep java` or `docker top <container>` to find
the process ID.

async-profiler should be run from the host by a privileged user - it will
automatically switch to the proper pid/mount namespace and change
user credentials to match the target process. Also make sure that
the target container can access `libasyncProfiler.so` by the same
absolute path as on the host.

By default, Docker container restricts the access to `perf_event_open`
syscall. There are 3 alternatives to allow profiling in a container:
1. You can modify the [seccomp profile](https://docs.docker.com/engine/security/seccomp/)
   or disable it altogether with `--security-opt seccomp=unconfined` option. In
   addition, `--cap-add SYS_ADMIN` may be required.
2. You can use "fdtransfer": see the help for `--fdtransfer`.
3. Last, you may fall back to `-e ctimer` profiling mode, see [Troubleshooting](#troubleshooting).

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

```
Failed to change credentials to match the target process: Operation not permitted
```
Due to limitation of HotSpot Dynamic Attach mechanism, the profiler must be run
by exactly the same user (and group) as the owner of target JVM process.
If profiler is run by a different user, it will try to automatically change
current user and group. This will likely succeed for `root`, but not for
other users, resulting in the above error.

```
Could not start attach mechanism: No such file or directory
```
The profiler cannot establish communication with the target JVM through UNIX domain socket.

Usually this happens in one of the following cases:
1. Attach socket `/tmp/.java_pidNNN` has been deleted. It is a common
   practice to clean `/tmp` automatically with some scheduled script.
   Configure the cleanup software to exclude `.java_pid*` files from deletion.  
   How to check: run `lsof -p PID | grep java_pid`  
   If it lists a socket file, but the file does not exist, then this is exactly
   the described problem.
2. JVM is started with `-XX:+DisableAttachMechanism` option.
3. `/tmp` directory of Java process is not physically the same directory
   as `/tmp` of your shell, because Java is running in a container or in
   `chroot` environment. `jattach` attempts to solve this automatically,
   but it might lack the required permissions to do so.  
   Check `strace build/jattach PID properties`
4. JVM is busy and cannot reach a safepoint. For instance,
   JVM is in the middle of long-running garbage collection.  
   How to check: run `kill -3 PID`. Healthy JVM process should print
   a thread dump and heap info in its console.

```
Target JVM failed to load libasyncProfiler.so
```
The connection with the target JVM has been established, but JVM is unable to load profiler shared library.
Make sure the user of JVM process has permissions to access `libasyncProfiler.so` by exactly the same absolute path.
For more information see [#78](https://github.com/async-profiler/async-profiler/issues/78).

```
No access to perf events. Try --fdtransfer or --all-user option or 'sysctl kernel.perf_event_paranoid=1'
```
or
```
Perf events unavailable
```
`perf_event_open()` syscall has failed.

Typical reasons include:
1. `/proc/sys/kernel/perf_event_paranoid` is set to restricted mode (>=2).
2. seccomp disables `perf_event_open` API in a container.
3. OS runs under a hypervisor that does not virtualize performance counters.
4. perf_event_open API is not supported on this system, e.g. WSL.

For permissions-related reasons (such as 1 and 2), using `--fdtransfer` while running the profiler
as a privileged user may solve the issue.

If changing the configuration is not possible, you may fall back to
`-e ctimer` profiling mode. It is similar to `cpu` mode, but does not
require perf_events support. As a drawback, there will be no kernel
stack traces.

```
No AllocTracer symbols found. Are JDK debug symbols installed?
```
The OpenJDK debug symbols are required for allocation profiling.
See [Installing Debug Symbols](#installing-debug-symbols) for more details.
If the error message persists after a successful installation of the debug symbols,
it is possible that the JDK was upgraded when installing the debug symbols.
In this case, profiling any Java process which had started prior to the installation
will continue to display this message, since the process had loaded
the older version of the JDK which lacked debug symbols.
Restarting the affected Java processes should resolve the issue.

```
VMStructs unavailable. Unsupported JVM?
```
JVM shared library does not export `gHotSpotVMStructs*` symbols -
apparently this is not a HotSpot JVM. Sometimes the same message
can be also caused by an incorrectly built JDK
(see [#218](https://github.com/async-profiler/async-profiler/issues/218)).
In these cases installing JDK debug symbols may solve the problem.

```
Could not parse symbols from <libname.so>
```
Async-profiler was unable to parse non-Java function names because of
the corrupted contents in `/proc/[pid]/maps`. The problem is known to
occur in a container when running Ubuntu with Linux kernel 5.x.
This is the OS bug, see https://bugs.launchpad.net/ubuntu/+source/linux/+bug/1843018.

```
Could not open output file
```
Output file is written by the target JVM process, not by the profiler script.
Make sure the path specified in `-f` option is correct and is accessible by the JVM.
