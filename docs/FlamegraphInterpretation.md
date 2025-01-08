# FlameGraph interpretation

To interpret a flame graph, the best way forward is to understand how it is created.

## Example application to profile

Let's take the below example:

```
main() {
     // some business logic
    func3() {
        // some business logic
        func7();
    }

    // some business logic
    func4();

    // some business logic
    func1() {
        // some business logic
        func5();
    }

    // some business logic
    func2() {
        // some business logic
        func6() {
            // some business logic
            func8(); // cpu intensive work here
    }
}
```

## Profiler sampling

Profiling starts by taking samples `X` times per second. Whenever a sample is taken,
the current call stack for it is saved. The diagram below shows the unsorted sampling view
before the sorting and aggregation takes place.

![](/.assets/images/ProfilerSamplings.png)

Below are the sampling numbers:

- `func3()->func7()`: 3 samples
- `func4()`: 1 sample
- `func1()->func5()`: 2 samples
- `func2()->func8()`: 4 samples
- `func2()->func6()`: 1 sample

## Sorting samples

Samples are then alphabetically sorted at the base level just after root (or main method) of the application.

![](/.assets/images/SortedSamplings.png)

Note that X-axis is no longer a timeline. Flame graph does not preserve information
on _when_ a particular stack trace was taken, it only indicates _how often_
a stack trace was observed during profiling.

## Aggregated view

The blocks for the same functions at each level of stack depth are then stitched together
to get an aggregated view of the flame graph.
![](/.assets/images/AggregatedView.png)

In this example, except `func4()`, no other function actually consumes
any resource at the base level of stack depth. `func5()`, `func6()`,
`func7()` and `func8()` are the ones consuming resources, with `func8()`
being a likely candidate for performance optimization.

CPU utilization is the most common use case for flame graphs, however,
there are other modes of profiling like allocation profiling to view
heap utilization and wall-clock profiling to view latency.

[More on various modes of profiling](ProfilingModes.md)

## Understanding FlameGraph colors

Color is another flame graph dimension that may be used to encode additional information
about each frame. Colors may have different meaning in various flame graph implementations.
async-profiler uses the following palette to differentiate frame types:

![](/.assets/images/flamegraph_colors.png)
