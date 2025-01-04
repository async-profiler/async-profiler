# Integrating async-profiler

## Launching as an agent

If you need to profile some code as soon as the JVM starts up, instead of using `asprof`,
it is possible to attach async-profiler as an agent on the command line. For example:

```
$ java -agentpath:/path/to/libasyncProfiler.so=start,event=cpu,file=profile.html ...
```

Agent library is configured through the JVMTI argument interface.
The format of the arguments string is described
[in the source code](https://github.com/async-profiler/async-profiler/blob/v3.0/src/arguments.cpp#L44).
`asprof` actually converts command line arguments to that format.

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

## Intellij IDEA

Intellij IDEA comes bundled with async-profiler, which can be further configured to our needs
by selecting the `Java Profiler` menu option at `Settings/Preferences > Build, Execution, Deployment`.
Agent options can be modified for the specific use cases and also `Collect native calls` can be checked
to monitor non-java threads and native frames in Java stack traces.
