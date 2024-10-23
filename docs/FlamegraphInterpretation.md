# Flame Graph Interpretation

To interpret a flame graph, the best way forward is to understand how they are created. Sampling
profiling results are a set of stack traces.

## Example application to profile
Let's take the below example:
```
main() {
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
    
    // some business logic
    func3() {
        // some business logic
        func7();
    }
    
    // some business logic
    func4();
}
```

## Profiler Sampling
Now let's understand profiler samplings for the above program. The
diagram below shows samplings before anyone sees the aggregated view.
![](https://github.com/async-profiler/async-profiler/blob/master/.assets/images/ProfilerSampling.png)

Below are the sample numbers:
* `func1()->func5()`: 2 samples
* `func2()->func6()`: 1 sample
* `func2()->func8()`: 4 samples
* `func3()->func7()`: 3 samples
* `func4()`: 1 sample

## Aggregated View
For the aggregated view, the blocks for the same functions at each
level of stack depth are stitched together to get the aggregated 
view of the flame graph.
![](https://github.com/async-profiler/async-profiler/blob/master/.assets/images/SamplingAggregation.png)

In this example, except func4() no other function actually consumes
any resource at the base level of stack depth. func5(), func6(),
func7() and func8() are the ones consuming resources, with func8()
being a likely candidate for performance optimization.

CPU utilization is the most common use case for flame graphs, however
there are other modes of profiling like allocation profiling to view
heap utilization and wall-clock profiling to view latency.

[More on various modes of profiling](https://github.com/async-profiler/async-profiler/?tab=readme-ov-file#profiling-modes)
