# Other Use Cases

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

Another important use of attaching async-profiler as an agent is for continuous profiling.

## Using Java API
async-profiler Java API is published to maven central. Like any other dependency, we have to
just include the 
[dependency](https://mvnrepository.com/artifact/tools.profiler/async-profiler/latest) 
from maven.

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

`%p` equates to the PID of the process. There are other options as well for filename which
can be found in [Profiler Options](https://github.com/async-profiler/async-profiler/blob/master/docs/ProfilerOptions.md).
`file` should be specified only once, either in 
`start` command with `jfr` output or in `stop` command with any other format.

## Intellij IDEA

Intellij IDEA comes bundled with async-profiler, which can be further configured to our needs
by selecting the `Java Profiler` menu option at `Settings/Preferences > Build, Execution, Deployment`
Agent options can be modified for specific use cases and also `Collect native calls` can be checked
to monitor non-java threads and native frames in Java stack traces.