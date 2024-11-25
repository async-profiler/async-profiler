# FlameGraph interpretation

To interpret a flame graph, the best way forward is to understand how they are created. Sampling
profiling results are a set of stack traces.

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
Profiling starts by taking samples x times per second. Whenever a sample is taken, the current call stack for it is saved. The diagram below shows the unsorted sampling view before the sorting and aggregation takes place.

![](https://github.com/async-profiler/async-profiler/blob/master/.assets/images/ProfilerSamplings.png)

Below are the sampling numbers:
* `func3()->func7()`: 3 samples
* `func4()`: 1 sample
* `func1()->func5()`: 2 samples
* `func2()->func8()`: 4 samples
* `func2()->func6()`: 1 sample

## Sorting samples
Samples are then alphabetically sorted at the base level just after root(or main method) of the application.

![](https://github.com/async-profiler/async-profiler/blob/master/.assets/images/SortedSamplings.png)

## Aggregated view
For the aggregated view, the blocks for the same functions at each
level of stack depth are stitched together to get the aggregated
view of the flame graph.
![](https://github.com/async-profiler/async-profiler/blob/master/.assets/images/AggregatedView.png)

In this example, except func4() no other function actually consumes
any resource at the base level of stack depth. func5(), func6(),
func7() and func8() are the ones consuming resources, with func8()
being a likely candidate for performance optimization.

CPU utilization is the most common use case for flame graphs, however
there are other modes of profiling like allocation profiling to view
heap utilization and wall-clock profiling to view latency.

[More on various modes of profiling](https://github.com/async-profiler/async-profiler/?tab=readme-ov-file#profiling-modes)

## Understanding FlameGraph colors
The various colours in a FlameGraph output with their relation to underlying code for a Java application:

![](https://github.com/async-profiler/async-profiler/blob/master/.assets/images/flamegraph_colors.png)

Please note the colours in the example diagrams above have no relation to the official FlameGraph colour palette.
