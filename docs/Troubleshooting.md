# Troubleshooting

## Error Messages

### perf_event mmap failed: Operation not permitted

Profiler allocates 8 kB perf_event buffer for each thread of the target process.
The above error may appear if the total size of perf_event buffers (`8 * threads` kB)
exceeds locked memory limit. This limit is comprised of `ulimit -l` plus
the value of `kernel.perf_event_mlock_kb` sysctl multiplied by the number of CPU cores.
For example, on a 16-core machine, `ulimit -l 65536` and `kernel.perf_event_mlock_kb=516`
is enough for profiling `(65536 + 516*16) / 8 = 9224` threads.
If an application has more threads, increase one of the above limits, or native stacks
will not be collected for some threads.

A privileged process is not subject to the locked memory limit.

### Failed to change credentials to match the target process: Operation not permitted

Due to limitation of HotSpot Dynamic Attach mechanism, the profiler must be run
by exactly the same user (and group) as the owner of target JVM process.
If profiler is run by a different user, it will try to automatically change
current user and group. This will likely succeed for `root`, but not for
other users, resulting in the above error.

### Could not start attach mechanism: No such file or directory

The profiler cannot establish communication with the target JVM through UNIX domain socket.
Usually this happens in one of the following cases:

1. Attach socket `/tmp/.java_pidNNN` has been deleted. It is a common
   practice to clean `/tmp` automatically with some scheduled script.
   Configure the cleanup software to exclude `.java_pid*` files from deletion.

   - How to check: run `lsof -p PID | grep java_pid`. If it lists a socket file, but the file does not exist, then this is exactly
     the described problem.

2. JVM is started with `-XX:+DisableAttachMechanism` option.
3. `/tmp` directory of Java process is not physically the same directory
   as `/tmp` of your shell, because Java is running in a container or in
   `chroot` environment. `asprof` attempts to solve this automatically,
   but it might lack the required permissions to do so.
   - Check `strace asprof PID jcmd`
4. JVM is busy and cannot reach a safepoint. For instance,
   JVM is in the middle of long-running garbage collection.
   - How to check: run `kill -3 PID`. Healthy JVM process should print
     a thread dump and heap info in its console.

### Target JVM failed to load libasyncProfiler.so

The connection with the target JVM has been established, but JVM is unable to load profiler shared library.
Make sure the user of JVM process has permissions to access `libasyncProfiler.so` by exactly the same absolute path.
For more information see [#78](https://github.com/async-profiler/async-profiler/issues/78).

### Perf events unavailable

`perf_event_open()` syscall has failed. Typical reasons include:

1. `/proc/sys/kernel/perf_event_paranoid` is set to restricted mode (>=2).
2. seccomp disables `perf_event_open` API in a container.
3. OS runs under a hypervisor that does not virtualize performance counters.
4. perf_event_open API is not supported on this system, e.g. WSL.

<br>For permissions-related reasons (such as 1 and 2), using `--fdtransfer` while running the profiler
as a privileged user may solve the issue.

If changing the configuration is not possible, you may fall back to
`-e ctimer` profiling mode. It is similar to `cpu` mode, but does not
require perf_events support. As a drawback, there will be no kernel
stack traces.

### No AllocTracer symbols found. Are JDK debug symbols installed?

The OpenJDK debug symbols are required for allocation profiling for applications developed
with JDK prior to 11. See [Installing Debug Symbols](ProfilingModes.md#installing-debug-symbols) for more
details. If the error message persists after a successful installation of the debug symbols,
it is possible that the JDK was upgraded when installing the debug symbols.
In this case, profiling any Java process which had started prior to the installation
will continue to display this message, since the process had loaded
the older version of the JDK which lacked debug symbols.
Restarting the affected Java processes should resolve the issue.

### VMStructs unavailable. Unsupported JVM?

JVM shared library does not export `gHotSpotVMStructs*` symbols -
apparently this is not a HotSpot JVM. Sometimes the same message
can be also caused by an incorrectly built JDK
(see [#218](https://github.com/async-profiler/async-profiler/issues/218)).
In these cases installing JDK debug symbols may solve the problem.

### Could not parse symbols from <libname.so>

Async-profiler was unable to parse non-Java function names because of
the corrupted contents in `/proc/[pid]/maps`. The problem is known to
occur in a container when running Ubuntu with Linux kernel 5.x.
This is the OS bug, see <https://bugs.launchpad.net/ubuntu/+source/linux/+bug/1843018>.

### Could not open output file

Output file is written by the target JVM process, not by the profiler script.
Make sure the path specified in `-f` option is correct and is accessible by the JVM.

## Known Limitations

- No Java stacks will be collected if `-XX:MaxJavaStackTraceDepth` is zero
  or negative. The exception is `--cstack vm` mode, which does not take
  `MaxJavaStackTraceDepth` into account.

- Too short profiling interval may cause continuous interruption of heavy
  system calls like `clone()`, so that it will never complete;
  see [#97](https://github.com/async-profiler/async-profiler/issues/97).
  The workaround is simply to increase the interval.

- When agent is not loaded at JVM startup (by using -agentpath option) it is
  highly recommended to use `-XX:+UnlockDiagnosticVMOptions -XX:+DebugNonSafepoints` JVM flags.
  Without those flags the profiler will still work correctly but results might be
  less accurate. For example, without `-XX:+DebugNonSafepoints` there is a high chance
  that simple inlined methods will not appear in the profile. When the agent is attached at runtime,
  `CompiledMethodLoad` JVMTI event enables debug info, but only for methods compiled after attaching.

- On most Linux systems, `perf_events` captures call stacks with a maximum depth
  of 127 frames. On recent Linux kernels, this can be configured using
  `sysctl kernel.perf_event_max_stack` or by writing to the
  `/proc/sys/kernel/perf_event_max_stack` file.

- You will not see the non-Java frames _preceding_ the Java frames on the
  stack, unless `--cstack vmx` is specified.
  For example, if `start_thread` called `JavaMain` and then your Java
  code started running, you will not see the first two frames in the resulting
  stack. On the other hand, you _will_ see non-Java frames (user and kernel)
  invoked by your Java code.

- macOS profiling is limited to user space code only.
