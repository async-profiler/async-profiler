# JFR Visualization

JFR recordings produced by async-profiler can be viewed using multiple options explained below.

## Built-in converter

async-profiler provides a built-in converter `jfrconv` which can be used to convert `jfr` output
to a flame graph or one of the other supported formats. More details on the built-in converter usage
can be found [here](ConverterUsage.md).

## JMC

[JDK Mission Control](https://www.oracle.com/java/technologies/jdk-mission-control.html) (JMC)
is a popular GUI tool to analyze JFR recordings.
It has been originally developed to work in conjunction with the JDK Flight Recorder,
however, async-profiler recordings are also fully compatible with JMC.

When viewing async-profiler recordings in JMC, information on some tabs may be missing.
Developers are typically interested in the following sections:

- Java Application
  - Method Profiling
  - Memory
  - Lock Instances
- JVM Internals
  - TLAB Allocations

## IntelliJ IDEA

IntelliJ IDEA Ultimate has built-in JFR viewer that works perfectly with async-profiler recordings.
For the Community Edition, there is an open-source profiler [plugin](https://plugins.jetbrains.com/plugin/20937-java-jfr-profiler)
that allows you to profile Java applications with JFR and async-profiler as well as
open JFR files obtained outside IDE.

## JFR command line tool

JDK distributions include the `jfr` command line utility to filter, summarize and output
flight recording files into human-readable format. The
[official documentation](https://docs.oracle.com/en/java/javase/21/docs/specs/man/jfr.html)
provides complete information on how to manipulate the contents and translate it as per
developers' needs to debug performance issues with their Java applications.
