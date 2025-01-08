# Getting started guide

## Before profiling

As of Linux 4.6, capturing kernel call stacks using `perf_events` from a non-root
process requires setting two kernel parameters. You can set them using sysctl as follows:

```
# sysctl kernel.perf_event_paranoid=1
# sysctl kernel.kptr_restrict=0
```

## Find a process to profile

Common ways to find the target process include using
[`jps`](https://docs.oracle.com/en/java/javase/21/docs/specs/man/jps.html) and
[`pgrep`](https://man7.org/linux/man-pages/man1/pgrep.1.html).
For example, to list all Java process IDs with their full command lines, run
`pgrep -a java`. The next section includes an example using `jps`.

## Start profiling

async-profiler works in the context of the target Java application,
i.e. it runs as an agent in the process being profiled.
`asprof` is a tool to attach and control the agent.

A typical workflow would be to launch your Java application, attach
the agent and start profiling, exercise your performance scenario, and
then stop profiling. The agent's output, including the profiling results, will
be displayed on the console where you've started `asprof`.

Example:

```
$ jps
9234 Jps
8983 Computey
$ asprof start 8983
$ asprof stop 8983
```

The following may be used in lieu of the `pid` (8983):

- The keyword `jps`, which will find `pid` automatically, if there is a single Java process running in the system.
- The application name as it appears in the `jps` output: e.g. `Computey`

Alternatively, you may specify `-d` (duration) argument to profile
the application for a fixed period of time with a single command.

```
$ asprof -d 30 8983
```

By default, the profiling frequency is 100Hz (every 10ms of CPU time).
Here is a sample output of `asprof`:

```
--- Execution profile ---
Total samples:           687
Unknown (native):        1 (0.15%)

--- 6790000000 (98.84%) ns, 679 samples
  [ 0] Primes.isPrime
  [ 1] Primes.primesThread
  [ 2] Primes.access$000
  [ 3] Primes$1.run
  [ 4] java.lang.Thread.run

... a lot of output omitted for brevity ...

          ns  percent  samples  top
  ----------  -------  -------  ---
  6790000000   98.84%      679  Primes.isPrime
    40000000    0.58%        4  __do_softirq

... more output omitted ...
```

This indicates that the hottest method was `Primes.isPrime`, and the hottest
call stack leading to it comes from `Primes.primesThread`.

## Other use cases

- [Launching as an agent](IntegratingAsyncProfiler.md#launching-as-an-agent)
- [Java API](IntegratingAsyncProfiler.md#using-java-api)
- [IntelliJ IDEA](IntegratingAsyncProfiler.md#intellij-idea)

## FlameGraph visualization

async-profiler provides out-of-the-box [Flame Graph](https://www.brendangregg.com/flamegraphs.html) support.
Specify `-o flamegraph` argument to dump profiling results as an interactive HTML Flame Graph.
Also, Flame Graph output format will be chosen automatically if the target filename ends with `.html`.

```
$ jps
9234 Jps
8983 Computey
$ asprof -d 30 -f /tmp/flamegraph.html 8983
```

[![Example](/.assets/images/flamegraph.png)](https://htmlpreview.github.io/?https://github.com/async-profiler/async-profiler/blob/master/.assets/html/flamegraph.html)

The flame graph html can be opened in any browser of your choice for further interpretation.

Please refer to [Interpreting a Flame Graph](FlamegraphInterpretation.md)
to understand more on how to interpret a Flame Graph.
