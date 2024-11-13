# Advanced Stacktrace Features

## Display JIT compilation task

Async-profiler samples JIT compiler threads as well as Java threads, and hence can show
CPU percentage spent on JIT compilation. At the same time, Java methods are different:
some take more resources to compile, other take less. Furthermore, there are cases when
a bug in C2 compiler causes a JIT thread to stuck in an infinite loop consuming 100% CPU.
Async-profiler can highlight which particular Java methods take most CPU time to compile.

The feature can be enabled with the option `-F comptask` (or its agent equivalent `features=comptask`).

## Display instruction addresses

Sometimes, for low-level performance analysis, it is important to know where exactly
CPU time is spent inside a method. As an intermediate step to the instruction-level
profiling, async-profiler provides an option to record PC address of the currently
running method for each execution sample. In this case, each stack trace will include
a synthetic frame with the address at the top of every stack trace.

The feature can be enabled with the option `-F pcaddr` (or its agent equivalent `features=pcaddr`).