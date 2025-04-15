/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.stack;

import test.cstack.LongInitializer;

import one.profiler.test.Jvm;
import one.profiler.test.Os;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public final class StackTests {
    private static final int TRUNCATING_STACK_DEPTH = 5;
    private static final int NON_TRUNCATING_STACK_DEPTH = 4096;
    private static final String PROFILE_COMMAND = "-e cpu -i 10ms -d 1 -a -o traces ";

    @Test(mainClass = LongInitializer.class, jvm = Jvm.HOTSPOT, os = Os.LINUX)
    public void truncated(TestProcess p) throws Exception {
        for (String mode : new String[]{"no", "fp", "dwarf", "vm", "vmx"}) {
            System.out.println("=== Testing cstack mode: " + mode);
            Output out = p.profile(PROFILE_COMMAND + "-j " + TRUNCATING_STACK_DEPTH +" --cstack " + mode);
            long nonTruncatedTraces = out.stream("\\[ " + TRUNCATING_STACK_DEPTH + "\\]").filter(l -> !l.endsWith("[truncated]")).count();

            assert nonTruncatedTraces == 0 : "Expected all traces to be truncated, but found: " + nonTruncatedTraces + " non-truncated traces";
        }
    }

    @Test(mainClass = LongInitializer.class, jvm = Jvm.HOTSPOT, os = Os.LINUX)
    public void nonTruncated(TestProcess p) throws Exception {
        for (String mode : new String[]{"no", "fp", "dwarf", "vm", "vmx"}) {
            System.out.println("=== Testing cstack mode: " + mode);
            Output out = p.profile(PROFILE_COMMAND + "-j " + NON_TRUNCATING_STACK_DEPTH +" --cstack " + mode);
            long truncatedTraces = out.stream("\\[ " + NON_TRUNCATING_STACK_DEPTH + "\\]").filter(l -> l.endsWith("[truncated]")).count();

            assert truncatedTraces == 0 : "Expected no traces to be truncated, but found: " + truncatedTraces + " truncated traces";
        }
    }
}