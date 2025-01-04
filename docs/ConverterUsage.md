# Converter Usage

async-profiler provides `jfrconv` utility to convert between different profile output formats.
`jfrconv` can be found at the same location as the `asprof` binary. Converter is also available
as a standalone Java application: [`jfr-converter.jar`](https://github.com/async-profiler/async-profiler/releases/download/v3.0/converter.jar).

## Supported conversions

| Source    | html | collapsed | pprof | pb.gz | heatmap |
| --------- | ---- | --------- | ----- | ----- | ------- |
| jfr       | ✅   | ✅        | ✅    | ✅    | ✅      |
| html      | ✅   | ✅        | ❌    | ❌    | ❌      |
| collapsed | ✅   | ✅        | ❌    | ❌    | ❌      |

## Usage

```
jfrconv [options] <input> [<input>...] <output>
```

The output format specified can be only one at a time for conversion from one format to another.

```
Conversion options:
  -o --output FORMAT, -o can be omitted if the output file extension unambiguously determines the format, e.g. profile.collapsed

  FORMAT can be any of the following:
  # collapsed: This is a collection of call stacks, where each line is a  semicolon separated
               list of frames followed by a counter. This is used by the FlameGraph script to
               generate the FlameGraph visualization of the profile data.

  # flamegraph: FlameGraph is a hierarchical representation of call traces of the profiled
                software in a color coded format that helps to identify a particular resource
                usage like CPU and memory for the application.

  # pprof: pprof is a profiling visualization and analysis tool from Google. More details on
           pprof  on the official github page https://github.com/google/pprof.

  # pb.gz: This is a compressed version of pprof output.

  # heatmap: A single page interactive heatmap that allows to explore profiling events
             on a timeline.


JFR options:
    --cpu              Generate only CPU profile during conversion
    --wall             Generate only Wall clock profile during conversion
    --alloc            Generate only Allocation profile during conversion
    --live             Build allocation profile from live objects only during conversion
    --nativemem        Generate native memory allocation profile
    --leak             Only include memory leaks in nativemem
    --lock             Generate only Lock contention profile during conversion
 -t --threads          Split stack traces by threads
 -s --state LIST       Filter thread states: runnable, sleeping, default. State name is case insensitive
                       and can be abbreviated, e.g. -s r
    --classify         Classify samples into predefined categories
    --total            Accumulate total value (time, bytes, etc.) instead of samples
    --lines            Show line numbers
    --bci              Show bytecode indices
    --simple           Simple class names instead of fully qualified names
    --norm             Normalize names of hidden classes/lambdas, e.g. Original JFR transforms
                       lambda names to something like pkg.ClassName$$Lambda+0x00007f8177090218/543846639
                       which gets normalized to pkg.ClassName$$Lambda
    --dot              Dotted class names, e.g. java.lang.String instead of java/lang/String
    --from TIME        Start time in ms (absolute or relative)
    --to TIME          End time in ms (absolute or relative)
                       TIME can be:
                       # an absolute timestamp specified in millis since epoch;
                       # an absolute time in hh:mm:ss or yyyy-MM-dd'T'hh:mm:ss format;
                       # a relative time from the beginning of recording;
                       # a relative time from the end of recording (a negative number).

Flame Graph options:
    --title STRING     Convert to Flame Graph with provided title
    --minwidth X       Skip frames smaller than X%
    --grain X          Coarsen Flame Graph to the given grain size
    --skip N           Skip N bottom frames
 -r --reverse          Reverse stack traces (icicle graph)
 -I --include REGEX    Include only stacks with the specified frames, e.g. -I 'MyApplication\.main' -I 'VMThread.*'
 -X --exclude REGEX    Exclude stacks with the specified frames, e.g. -X '.*pthread_cond_(wait|timedwait).*'
    --highlight REGEX  Highlight frames matching the given pattern
```

## jfrconv examples

`jfrconv` utility is provided in `bin` directory of the async-profiler package.
It requires JRE to be installed on the system.

### Generate Flame Graph from JFR

If no output file is specified, it defaults to a Flame Graph output.

```
jfrconv foo.jfr
```

Profiling in JFR mode allows multi-mode profiling. So the command above will generate a Flame Graph
output, however, for a multi-mode profile output with both `cpu` and `wall-clock` events, the
Flame Graph will have an aggregation of both in the view. Such a view wouldn't make much sense and
hence it is advisable to use JFR conversion filter options like `--cpu` to filter out events
during a conversion.

```
jfrconv --cpu foo.jfr

# which is equivalent to:
# jfrconv --cpu -o flamegraph foo.jfr foo.html
```

for HTML output as HTML is the default format for conversion from JFR.

#### Flame Graph options

To add a custom title to the generated Flame Graph, use `--title`, which has the default value `Flame Graph`:

```
jfrconv --cpu foo.jfr foo.html -r --title "Custom Title"
```

### Other formats

`jfrconv` supports converting a JFR file to `collapsed`, `pprof`, `pb.gz` and `heatmap` formats as well.

## Standalone converter examples

Standalone converter jar is provided in
[Download](https://github.com/async-profiler/async-profiler/?tab=readme-ov-file#Download).
It accepts the same parameters as `jfrconv`.

Below is an example usage:

```
java -jar jfr-converter.jar --cpu foo.jfr --reverse --title "Application CPU profile"
```
