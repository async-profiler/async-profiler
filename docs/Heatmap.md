# Heatmap

Problems to be solved with a profiler can be divided into two large categories:

1. Optimization of overall resource usage.
2. Troubleshooting of intermittent performance issues.

While flame graphs are handy for the first type of problems, they are not very helpful
for analyzing transient anomalies because they provide an aggregated view that lacks
any timeline information. To address the second type of problems, async-profiler offers
a converter from JFR format to an interactive heatmap in the form of a single-page HTML file.

Heatmap is an alternative representation of profile data that preserves timestamps
of particular samples. Essentially, it's a two-dimensional timeline composed of
colored blocks. Each block represents a short period of time (usually in the range of
milliseconds to seconds) with its color being the third dimension: the more intense
the color, the more events happened in a given period of time.

![](/.assets/images/heatmap.png)

The idea of heatmaps was borrowed from [FlameScope](https://github.com/Netflix/flamescope),
however, FlameScope targets short profiling intervals up to a few minutes, whereas
async-profiler implementation is capable of visualizing 24-hour recordings
with the granularity of 20 milliseconds. Moreover, heatmaps produced by async-profiler
are serverless: they are standalone self-contained HTML files that can be easily shared
and viewed without additional software besides a browser.

## Heatmap features

### Whole day profile

Heatmaps are optimized for information density. Full day of continuous profiling
can be presented on a single image, where an engineer can spot regular activity
patterns as well as anomalies at a glance.

Heatmaps are also optimized for footprint. Specialized compression algorithms
can pack 1 GB original JFR recording to an HTML page of 10-15 MB in size.

![](/.assets/images/heatmap1.png)

### Scale / zoom

Depending on the recording duration and level of detail you are interested in,
you can switch between 3 available scales. On the largest scale, each vertical line
represents 5 minutes of wall clock time, with each square corresponding to
5 second interval. On the finest scale, each square corresponds to 20 milliseconds,
allowing you to analyze profiling samples with a high resolution.

![](/.assets/images/heatmap2.png)

### Instant flame graphs

A click on any heatmap square displays a flame graph for this specific time interval.

![](/.assets/images/heatmap3.png)

Hold mouse button to select an arbitrary time range on a heatmap.
A flame graph for the given time range will be built automatically.

![](/.assets/images/heatmap4.png)

### Compare time ranges

Select target time range as described above. Holding `Ctrl` key,
move mouse pointer to choose another time range that will serve as a baseline.
You will then get a differential flame graph highlighting stacks
that were seen more often in the target time range comparing to the baseline.

![](/.assets/images/heatmap5.png)

### Search

Press `Ctrl+F` and enter a regex to search on the entire heatmap.
Time intervals containing matched stacks will be highlighted on a heatmap in blue.
Matching frames, if any, will be also highlighted on a flame graph.

`Ctrl+Shift+F` does the same, except that a flame graph will
retain stacks with matching frames only. All other stacks will be filtered out.

![](/.assets/images/heatmap6.png)

## Producing heatmaps

Heatmaps can only be generated from recordings in JFR format.
Run [`jfrconv`](ConverterUsage.md) tool with `-o heatmap` option.

Standard `jfrconv` options (`--cpu`, `--alloc`, `--from`/`--to`, `--simple`, etc.)
are also applicable to heatmaps.

Example:

```
jfrconv --cpu -o heatmap profiler.jfr heatmap-cpu.html
```
