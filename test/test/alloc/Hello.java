/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.alloc;

/**
 * How many objects are created when running "Hello World" application?
 * Where are these objects are created?
 * <p>
 * async-profiler can trace every single created object -
 * just run allocation profiling with G1 GC and TLAB disabled:
 * `java -XX:+UseG1GC -XX:-UseTLAB -agentlib:asyncProfiler=start,event=alloc,file=alloc.html`
 * <p>
 * Add `cstack=fp` option to include native stack traces.
 */
public class Hello {

    public static void main(String[] args) {
        System.out.println("It works!");
    }
}
