# Integrating async-profiler

## Launching as an agent

If you need to profile some code as soon as the JVM starts up, instead of using `asprof`,
it is possible to attach async-profiler as an agent on the command line. For example:

```
$ java -agentpath:/path/to/libasyncProfiler.so=start,event=cpu,file=profile.html ...
```

On macOS, the library name is `libasyncProfiler.dylib` instead of `libasyncProfiler.so`.

Agent library is configured through the JVMTI argument interface.
The argument string is a comma-separated list of [profiler options](ProfilerOptions.md):

```
option[=value],option[=value]...
```

`asprof` internally converts command line arguments to the above format and attaches
`libasyncProfiler.so` agent to a running process.

Another important use of attaching async-profiler as an agent is for continuous profiling.

## Using Java API

async-profiler can be controlled programmatically using Java API. The corresponding Java library
is published to Maven Central. You can [include it](https://mvnrepository.com/artifact/tools.profiler/async-profiler/latest)
just like any other Maven dependency:

```
<dependency>
    <groupId>tools.profiler</groupId>
    <artifactId>async-profiler</artifactId>
    <version>X.Y</version>
</dependency>
```

### Example usage with the API

```
AsyncProfiler profiler = AsyncProfiler.getInstance();
```

The above gives us an instance of `AsyncProfiler` object which can be further used to start
actual profiling.

```
profiler.execute("start,jfr,event=cpu,file=/path/to/%p.jfr");
// do some meaningful work
profiler.execute("stop");
```

`%p` equates to the PID of the process. Filename may include other placeholders which
can be found in [Profiler Options](ProfilerOptions.md).
`file` should be specified only once, either in
`start` command with `jfr` output or in `stop` command with any other format.

## Span API

The Span API lets you record latency-sensitive intervals - typically requests or
transactions - into the same JFR recording as the profiling samples. The profile can then be
filtered to the spans of interest, e.g. to see what the application was doing during the slowest
requests, or only during a particular operation.

A span is a `[start, end]` interval on the current thread, labeled with a tag (an operation name,
endpoint, etc.). Wrap the code you want to measure:

```java
import one.profiler.Span;

long span = Span.start();
try {
    handleRequest();
} finally {
    Span.end(span, "GET /api/orders");
}
```

The API is static and allocation-free, so it is cheap enough to leave in production code.
When async-profiler is not loaded, all calls are no-ops.

- `Span.start()` returns a start timestamp; pass it to `end` to record the span.
- For very frequent operations, use `Span.endIfProfiled(span, tag)` instead of `end`. It records
  the span only if at least one profiling sample was taken on the thread while it was open - spans
  that enclose no sample are skipped.
- `Span.emit(start, end, tag)` and `Span.emitIfProfiled(start, end, tag)` record a span with
  explicit timestamps obtained from `Recording.timestamp()`, for cases where the interval is not
  bounded by the current call.

Spans appear as `profiler.Span` JFR events with `startTime`, `duration`, `eventThread` and `tag`
fields. Tags are deduplicated automatically in the JFR recording.
Span timestamps use the same clock as the profiling samples, so they line up exactly.

`one.profiler.Span` can be used independently of the rest of the API; async-profiler connects to it
automatically whenever it is loaded (via `-agentpath`, dynamic attach, or `System.loadLibrary`).

## Intellij IDEA

Intellij IDEA comes bundled with async-profiler, which can be further configured to our needs
by selecting the `Java Profiler` menu option at `Settings/Preferences > Build, Execution, Deployment`.
Agent options can be modified for the specific use cases and also `Collect native calls` can be checked
to monitor non-java threads and native frames in Java stack traces.
