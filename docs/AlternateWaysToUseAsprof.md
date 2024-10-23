# Alternate ways to use async-profiler

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

## Using Java API
async-profiler Java API is published to maven central. Like any other dependency, we have to
just include the below dependency.
```
<!-- https://mvnrepository.com/artifact/tools.profiler/async-profiler -->
<dependency>
    <groupId>tools.profiler</groupId>
    <artifactId>async-profiler</artifactId>
    <version>3.0</version>
</dependency>
```

### Example usage with the API

```
AsyncProfiler profiler = AsyncProfiler.getInstance();
```

The above gives us an instance of `AsyncProfiler` object which can be further used to start 
actual profiling.

```
profiler.execute(String.format("start,jfr,event=cpu,file=%s", path));
// do some meaningful work
profiler.execute(String.format("stop,file=%s", path));
```

## Intellij IDEA

Intellij IDEA comes bundled with async-profiler, which can be further configured to our needs
by selecting the `Java Profiler` menu option at `Settings/Preferences > Build, Execution, Deployment`
Agent options can be modified for specific use cases and also `Collect native calls` can be checked
to monitor non-java threads and native frames in Java stack traces.