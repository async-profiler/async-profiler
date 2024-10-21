# Getting started

## Before we start profiling
As of Linux 4.6, capturing kernel call stacks using `perf_events` from a non-root
process requires setting two runtime variables. You can set them using
sysctl or as follows:

```
# sysctl kernel.perf_event_paranoid=1
# sysctl kernel.kptr_restrict=0
```

## Find process to profile
Common ways to find the target process include using `jps` and `ps`. The next section includes an 
example using `jps`. `ps` can also be used along with necessary flags and subsequently using `grep` 
with `java` for example to find all running java processes and find the target process id from the
list.

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

- The keyword `jps`, which will use the most recently launched Java process.
- The application name as it appears in the `jps` output: e.g. `Computey`

Alternatively, you may specify `-d` (duration) argument to profile
the application for a fixed period of time with a single command.

```
$ asprof -d 30 8983
```

By default, the profiling frequency is 100Hz (every 10ms of CPU time).
Here is a sample of the output printed to the Java application's terminal:

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

## Launching as an Agent

If you need to profile some code as soon as the JVM starts up, instead of using the `asprof`,
it is possible to attach async-profiler as an agent on the command line. For example:

```
$ java -agentpath:/path/to/libasyncProfiler.so=start,event=cpu,file=profile.html ...
```

Agent library is configured through the JVMTI argument interface.
The format of the arguments string is described
[in the source code](https://github.com/async-profiler/async-profiler/blob/v3.0/src/arguments.cpp#L44).
`asprof` actually converts command line arguments to that format.

## Flame Graph visualization

async-profiler provides out-of-the-box [Flame Graph](https://github.com/BrendanGregg/FlameGraph) support.
Specify `-o flamegraph` argument to dump profiling results as an interactive HTML Flame Graph.
Also, Flame Graph output format will be chosen automatically if the target filename ends with `.html`.

```
$ jps
9234 Jps
8983 Computey
$ asprof -d 30 -f /tmp/flamegraph.html 8983
```

[![Example](https://github.com/async-profiler/async-profiler/blob/master/demo/flamegraph.png)](https://htmlpreview.github.io/?https://github.com/async-profiler/async-profiler/blob/master/demo/flamegraph.html)

The flame graph html can be opened in any browser of your choice for further interpretation.

[Interpreting a flame graph](https://github.com/async-profiler/async-profiler/blob/master/docs/FlamegraphInterpretation.md)