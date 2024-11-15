# CPU Sampling Engines

Async-profiler has three options for CPU profiling: `-e cpu`, `-e itimer` and `-e ctimer`.

## cpu & itimer mode

Both cpu and itimer mode measure the CPU time spent by the running threads. For example, 
if an application uses 2 cpu cores, each with 30% utilization, and the sampling interval is 
10ms, then the profiler will collect about 2 * 0.3 * 100 = 60 samples per second.

In other words, 1 profiling sample means that one CPU was actively running for N nanoseconds, 
where N is the profiling interval.

- `itimer` mode is based on [setitimer(ITIMER_PROF)](https://man7.org/linux/man-pages/man2/setitimer.2.html)
syscall, which ideally generates a signal every given interval of the CPU time consumed by the process.
- `cpu` mode relies on [perf_events](https://man7.org/linux/man-pages/man2/perf_event_open.2.html). 
The idea is the same - to generate a signal every `N` nanoseconds of CPU time, which in this case 
is achieved by configuring PMU to generate an interrupt every `K` CPU cycles. `cpu` mode has few additional features:
  - `perf_events` availability is now automatically checked by trying to create a dummy perf_event.
  - If kernel-space profiling using `perf_events` is not available (including when restricted by `perf_event_paranoid`
    setting or by `seccomp`), async-profiler transparently falls back to `ctimer` mode.
  - If `perf_events` are available, but kernel symbols are hidden (e.g., by `kptr_resitrct` setting), async-profiler
    continues to use `perf_events`, emits a warning and does not show kernel stack traces.
  - To force using `perf_events` for user-space only profiling, specify `-e cpu-clock --all-user` instead of `-e cpu`.
  - `allkernel` option has been removed.
  - JFR recording now contains engine setting with the current profiling engine: `perf_events`, `ctimer`, `wall` etc.


Ideally, both `itimer` and `cpu` should collect the same number of samples. Typically, the 
profiles indeed look very similar. However, in [some cases](https://github.com/golang/go/issues/14434)
cpu profile appears a bit more accurate though, since the signal is delivered exactly to the thread
that overflowed a hardware counter.

## ctimer mode

[perf_events](https://man7.org/linux/man-pages/man2/perf_event_open.2.html) are not always available,
e.g., because of perf_event_paranoid settings or seccomp restrictions. Perf events are often disabled
in containers. Furthermore, async-profiler opens one perf_event descriptor per thread, which can be
problematic for an application with many threads running under a low
[ulimit](https://ss64.com/bash/ulimit.html) for the number of open file descriptors.

`itimer` works fine in containers, but may suffer from inaccuracies caused by the following limitations:

- only one `itimer` signal can be delivered to a process at a time.
- signals are not distributed evenly between running threads.
- sampling resolution is limited by the size of [jiffies](https://man7.org/linux/man-pages/man7/time.7.html).


`ctimer` aims to address these limitations of `perf_events` and `itimer`. `ctimer` relies on
[timer_create](https://man7.org/linux/man-pages/man2/timer_create.2.html). It combines benefits of
`-e cpu` and `-e itimer`, except that it does not allow collecting kernel stacks. `timer_create` is used
in [Go profiler](https://felixge.de/2022/02/11/profiling-improvements-in-go-1.18/). Below are some of
the benefits of `ctimer`:
- Works in containers by default.
- Does not suffer from `itimer` biases.
- Does not consume file descriptors.