# CPU Sampling Engines

Async-profiler has three options for CPU profiling: `-e cpu`, `-e itimer` and `-e ctimer`.

## cpu

`cpu` mode measures CPU time spent by the running threads. For example,
if an application uses 2 cpu cores, each with 30% utilization, and the sampling interval is
10ms, then the profiler will collect about `2 * 0.3 * 100 = 60` samples per second.
In other words, 1 profiling sample means that one CPU core was actively running for N nanoseconds,
where N is the profiling interval.

On Linux, `cpu` mode relies on [perf_events](https://man7.org/linux/man-pages/man2/perf_event_open.2.html).
One `perf_event` descriptor is created for each running thread and configured to generate a signal
every `N` nanoseconds of CPU time. This is the most accurate CPU sampler available in async-profiler
and the only one that can obtain kernel stack traces. It, however, comes with certain restrictions.

Most importantly, OS configuration may limit access to `perf_events` API, e.g.,
by `kernel.perf_event_paranoid` sysctl or by seccomp (which is often the case in a Docker container).
If `perf_events` are available, but kernel symbols are hidden (e.g., by `kernel.kptr_resitrct` setting),
async-profiler continues to use `perf_events`, emits a warning and does not show kernel stack traces.

Another important thing to consider is that `cpu` sampling engine allocates a descriptor per thread.
This means, if an application has too many threads and OS limit for the maximum number of open descriptors
(`ulimit -n`) is too low, an application may run out of file descriptors. The workaround
is to simply increase file descriptor limit.

## itimer

`itimer` mode is based on [setitimer(ITIMER_PROF)](https://man7.org/linux/man-pages/man2/setitimer.2.html)
syscall, which ideally generates a signal every given interval of CPU time consumed by the process.
Ideally, both `itimer` and `cpu` should collect the same number of samples. Typically,
profiles indeed look very similar. However, in [some cases](https://github.com/golang/go/issues/14434),
`cpu` profile appears more accurate, since a signal is delivered exactly to the thread
that overflowed a hardware counter. In contrast, `itimer` has the following limitations:

- Only one `itimer` signal can be delivered to a process at a time.
- Signals are not distributed evenly between running threads.
- Sampling resolution is limited by the size of [jiffies](https://man7.org/linux/man-pages/man7/time.7.html).

`itimer` profiles may be even less accurate on macOS, where `itimer` signals are often biased
towards system calls.

The main advantage of `itimer` is that it works in containers and does not consume file descriptors.

## ctimer

`ctimer` is a Linux-specific alternative for `cpu` profiling mode to overcome limitations
of `perf_events`, such as `perf_event_paraniod` setting, seccomp restriction or a low limit
for the number of open file descriptors. `ctimer` mode relies on
[timer_create](https://man7.org/linux/man-pages/man2/timer_create.2.html) API.
It combines benefits of `-e cpu` and `-e itimer`, except that it does not allow collecting kernel stacks.

Like with `itimer`, `ctimer` resolution is limited by the size of the jiffy -
kernel `HZ` constant, which is typically equal to 100 or 250, meaning that the minimum supported
profiling interval is 10ms or 4ms respectively.

## Summary

Here is a summary of advantages and drawbacks of all CPU profiling engines:

| Attribute                         | cpu (perf_events) | itimer | ctimer |
| --------------------------------- | :---------------: | :----: | :----: |
| Can collect kernel stack traces   |        ✅         |   ❌   |   ❌   |
| High resolution                   |        ✅         |   ❌   |   ❌   |
| Accuracy / fairness               |        ✅         |   ❌   |   🆗   |
| Works in containers by default    |        ❌         |   ✅   |   ✅   |
| Does not consume file descriptors |        ❌         |   ✅   |   ✅   |
| macOS support                     |        ❌         |   ✅   |   ❌   |

When using `-e cpu` on Linux, async-profiler automatically checks for `perf_events` availability
by trying to create a dummy perf_event. If kernel-space profiling is not available,
async-profiler transparently falls back to `ctimer` mode. To force using `perf_events`
for user-space only profiling, specify `-e cpu-clock --all-user` instead of `-e cpu`.

The actual profiling engine (`perf_events`, `ctimer`, etc.) is now recorded in `jfr` output.
