# JFR Visualization

JFR output produced by async-profiler can be viewed using multiple options which are explained
below:

## Built-in converter

async-profiler provides a built-in converter which can be used to convert `jfr` output to 
readable formats like `FlameGraph` visualization. More details on the built-in converter usage 
can be found [here](https://github.com/async-profiler/async-profiler/blob/master/docs/ConverterUsage.md).

## JMC

Java Mission Control or `jmc` is part of the OpenJDK distribution. It is a GUI tool where a `jfr`
output from async-profiler can be fed to visualize the profiled events in a human-readable format.
It has various sections which helps developers to see various resource usages. The
`Analyze a Flight Recording Using JMC` section in the
[official user guide](https://docs.oracle.com/en/java/java-components/jdk-mission-control/9/user-guide/using-jdk-flight-recorder.html)
provides details on how a `jfr` output can be interpreted using `JMC`.

## IntelliJ IDEA

An open-source profiler 
[plugin](https://plugins.jetbrains.com/plugin/20937-java-jfr-profiler) for JDK 11+ allows us to 
profile your Java application with JFR and async-profiler and view the results in IntelliJ IDEA, 
as well as opening JFR files.

## JFR command line

`jfr` provides a command line option to filter, summarize and output flight recording files 
into human-readable format. The
[official documentation](https://docs.oracle.com/en/java/javase/21/docs/specs/man/jfr.html)
provides complete information on how to manipulate the contents and translate them as per
developers' needs to debug performance issues with their Java applications. 
