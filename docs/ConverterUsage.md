# Converter usage & demo

async-profiler provides a converter utility to convert the profile output to other popular formats. async-profiler
provides `jfrconv` as part of the compressed package which is found in the same location as the `asprof` binary. A
standalone converter binary is also available [here](ttps://github.com/async-profiler/async-profiler/releases/download/v3.0/converter.jar).

## Supported conversions

* collapsed -> html, collapsed
* html -> html, collapsed
* jfr -> html, collapsed, pprof, pb.gz

## Usage

`jfrconv [options] <input> [<input>...] <output>`

The output format specified can be only one at a time for conversion from one format to another.

### Available arguments

```
Conversion options: 
  -o --output FORMAT, -o can be omitted if the output file extension unambiguously determines the format, e.g. profile.collapsed
  
  FORMAT can be any of the following:
  # collapsed: This is a collection of call stacks, where each line is a  semicolon separated 
               list of frames followed by a counter. This is used by the FlameGraph script to 
               generate the FlameGraph visualization of the profile data.
      
  # flamegraph: Flamegraph is a hierarchical representation of call traces of the profiled 
                software in a color coded format that helps to identify a particular resource
                usage like CPU and memory for the application.
      
  # pprof: pprof is a profiling visualization and analysis tool from Google. More details on 
           pprof  on the official github page https://github.com/google/pprof.
      
  # pb.gz: This is a compressed version of pprof output.
       
       
JFR options:
    --cpu              Generate only CPU profile during conversion
    --wall             Generate only Wall clock profile during conversion
    --alloc            Generate only Allocation profile during conversion
    --live             Build allocation profile from live objects only during conversion
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
 -I --include REGEX    Include only stacks with the specified frames, e.g. -I 'jdk\.GC.*' -I 'jdk\.Thread.*'
 -X --exclude REGEX    Exclude stacks with the specified frames, e.g.  -X 'jdk\.GC.*'
    --highlight REGEX  Highlight frames matching the given pattern
```

### Example usages with `jfrconv`

This section explains how the binary `jfrconv` can be used which exists in the same bin folder as
`asprof`binary.

The below command will generate a foo.html. If no output file is specified, it defaults to a 
Flame Graph output. 

```
jfrconv foo.jfr
```

Profiling in JFR mode allows multi-mode profiling. So the command above will generate a Flame Graph 
output, however, for a multi-mode profile output with both `cpu` and `wall-clock` events, the 
Flame Graph will have an aggregation of both in the view. Such a view wouldn't make much sense and 
hence it is advisable to use JFR conversion filter options like `--cpu` to filter out events 
during a conversion.

```
jfrconv --cpu foo.jfr -o foo.html
```
or
```
jfrconv --cpu foo.jfr
```
for HTML output as HTML is the default format for conversion from JFR.

In case the conversion output is a Flame Graph, it can be further formatted with the use of flags 
specified above under `Flame Graph options`. The below command(s) will add a title string named `Title` 
to the Flame Graph instead of the default `Flame Graph` title and also will reverse the graph view 
by reversing the stack traces.
```
jfrconv --cpu foo.jfr foo.html -r --title Title
```
or
```
jfrconv --cpu foo.jfr --reverse --title Title
```

These are few common use cases. Similarly, a JFR output can be converted to `collapsed`, `pprof` and
`pb.gz` formats based on specific needs.

### Example usages with standalone converter

The usage with standalone converter jar provided in
[Download](https://github.com/async-profiler/async-profiler/?tab=readme-ov-file#Download)
section is very similar to `jfrconv`.

Below is an example usage:

`java -cp /path/to/standalone-converter-jar --cpu foo.jfr --reverse --title Application CPU profile`

The only difference lies in how the binary is used.
