# Converter Usage & Demo

## Usage

`jfrconv [options] <input> [<input>...] <output>`

The output format specified can be only one at a time for conversion from one format to another.

### Available arguments

```
Conversion options: 
  -o --output FORMAT   Output format: <filename> followed by any of the extensions: html, collapsed, pprof, pb.gz
  
JFR options:
    --cpu              CPU profile
    --wall             Wall clock profile
    --alloc            Allocation profile
    --live             Live object profile
    --lock             Lock contention profile
 -t --threads          Split stack traces by threads
 -s --state LIST       Filter thread states: runnable, sleeping
    --classify         Classify samples into predefined categories
    --total            Accumulate total value (time, bytes, etc.)
    --lines            Show line numbers
    --bci              Show bytecode indices
    --simple           Simple class names instead of FQN
    --norm             Normalize names of hidden classes / lambdas
    --dot              Dotted class names
    --from TIME        Start time in ms (absolute or relative)
    --to TIME          End time in ms (absolute or relative)
    
Flame Graph options:
    --title STRING     Flame Graph title
    --minwidth X       Skip frames smaller than X%
    --grain X          Coarsen Flame Graph to the given grain size
    --skip N           Skip N bottom frames
 -r --reverse          Reverse stack traces (icicle graph)
 -I --include REGEX    Include only stacks with the specified frames
 -X --exclude REGEX    Exclude stacks with the specified frames
    --highlight REGEX  Highlight frames matching the given pattern
```

### Example usages with `jfrconv`

Let's look at how we can use the binary `jfrconv` which exists in the same bin folder as `asprof` 
binary.



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

### Example usages with `jfr-converter.jar`

The usage with `convereter.jar` provided in
[Download](https://github.com/async-profiler/async-profiler/?tab=readme-ov-file#Download)
section is very similar to `jfrconv`.

Let's look at an example usage.

`java -cp /path/to/converter.jar --cpu foo.jfr --reverse --title Title`

As we can see, the only difference lies in how the binary is used.
