# Stack Walking Modes

## Frame Pointer

The default stacking walking in async-profiler, `Frame Pointer (FP)` stack walking, is a technique for collecting call
stacks by tracking frame pointers in memory. Each function call maintains a pointer to its caller's stack frame, creating
a linked chain that can be traversed to reconstruct the program's execution path. It's particularly efficient as it is
very fast compared to other stack walking methods introducing less overhead but requires code to be compiled with frame
pointers enabled (`-fno-omit-frame-pointer`).

## DWARF

DWARF stack walking is a method to reconstruct call stacks using unwinding information embedded in executables
(typically in `.eh_frame` section). Unlike frame-pointer-based unwinding, it works reliably even with optimized code
where frame pointers are omitted.

DWARF unwinding requires extra memory (e.g. the lookup table for `libjvm.so` is about 2MB).
It is also slower than the traditional FP-based stack walker, but it's still fast enough for on-the-fly unwinding
due to being signal safe in async-profiler.

The feature can be enabled with the option `--cstack dwarf` (or its agent equivalent `cstack=dwarf`).

## LBR

Modern Intel CPUs can profile branch instructions, including `call`s and `ret`s, and store their source and destination
addresses (Last Branch Records) in hardware registers. Starting from Haswell, CPU can match these addresses to form a
branch stack. This branch stack will be effectively a call chain automatically collected by the hardware.

LBR stacks are not always complete or accurate, but they still appear much more helpful comparing to FP-based stack
walking, when a native library is compiled with omitted frame pointers. It works only with hardware events like
`-e cycles` (`instructions`, `cache-misses` etc.) and the maximum call chain depth is 32 (hardware limit).

The feature can be enabled with the option `--cstack lbr` (or its agent equivalent `cstack=lbr`).

## VM Structs

async-profiler can leverage JVM internal structures to replicate the logic of Java stack walking
in the profiler itself without depending on the unstable JVM API.

This mode of stack walking has been introduced in async-profiler due to issues with `AsyncCallGetTrace`.
AsyncGetCallTrace (AGCT) is a non-standard extension of HotSpot JVM to obtain Java stack traces outside safepoints.
async-profiler had been relying on AGCT heavily, and it even got its name after this function.

`AsyncGetCallTrace` being non-API, was never supported in OpenJDK well enough, it did not receive enough testing, it was
broken several times even in minor JDK updates, e.g. [JDK-8307549](https://bugs.openjdk.org/browse/JDK-8307549).

AsyncGetCallTrace is notorious for its inability to walk Java stack in different corner cases. There is a long-standing
bug [JDK-8178287](https://bugs.openjdk.org/browse/JDK-8178287) with several examples. But the worst aspect is that
AsyncGetCallTrace can crash JVM, and there is no reliable way to get around this outside the JVM.

Due to issues with AGCT from time to time, including random crashes and missing stack traces,
`vm` stack walking mode based on HotSpot VM Structs was introduced in async-profiler.
`vm` stack walker has the following advantages:

- Fully enclosed by the crash protection based on `setjmp`/`longjmp`.
- Can show all frames: Java, native and JVM stubs throughout the whole stack.
- Provides additional information on each frame, like JIT compilation type.

The feature can be enabled with the option `--cstack vm` (or its agent equivalent `cstack=vm`).

Another variant of this option: `--cstack vmx` activates an "expert" unwinding based on VM Structs.
With this option, async-profiler collects mixed stack traces that have Java and native frames interleaved.

The maximum stack depth for `vm` or `vmx` stack walking is controlled with `-j depth` option.
