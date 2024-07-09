/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.api;

import one.profiler.AsyncProfiler;
import one.profiler.Counter;
import one.profiler.Events;

public class DumpCollapsed extends BusyLoops {

    public static void main(String[] args) throws Exception {
        AsyncProfiler.getInstance().start(Events.CPU, 1_000_000);

        for (int i = 0; i < 5; i++) {
            method1();
            method2();
            method3();
        }

        String profile = AsyncProfiler.getInstance().dumpCollapsed(Counter.SAMPLES);
        System.out.println(profile);
    }
}
