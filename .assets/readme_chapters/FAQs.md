# FAQs

## General
* You will not see the non-Java frames _preceding_ the Java frames on the
  stack, unless `--cstack vm` is specified.
  For example, if `start_thread` called `JavaMain` and then your Java
  code started running, you will not see the first two frames in the resulting
  stack. On the other hand, you _will_ see non-Java frames (user and kernel)
  invoked by your Java code.
* Too short profiling interval may cause continuous interruption of heavy
  system calls like `clone()`, so that it will never complete;
  see [#97](https://github.com/async-profiler/async-profiler/issues/97).
  The workaround is simply to increase the interval.

## Linux
* On most Linux systems, `perf_events` captures call stacks with a maximum depth
  of 127 frames. On recent Linux kernels, this can be configured using
  `sysctl kernel.perf_event_max_stack` or by writing to the
  `/proc/sys/kernel/perf_event_max_stack` file.

## MacOs
* macOS profiling is limited to user space code only.

## JVM Flags Usage
* No Java stacks will be collected if `-XX:MaxJavaStackTraceDepth` is zero
  or negative. The exception is `--cstack vm` mode, which does not take
  `MaxJavaStackTraceDepth` into account.

* When agent is not loaded at JVM startup (by using -agentpath option) it is
  highly recommended to use `-XX:+UnlockDiagnosticVMOptions -XX:+DebugNonSafepoints` JVM flags.
  Without those flags the profiler will still work correctly but results might be
  less accurate. For example, without `-XX:+DebugNonSafepoints` there is a high chance
  that simple inlined methods will not appear in the profile. When the agent is attached at runtime,
  `CompiledMethodLoad` JVMTI event enables debug info, but only for methods compiled after attaching.
  