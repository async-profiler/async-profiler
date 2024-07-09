/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.api;

import one.profiler.AsyncProfiler;
import one.profiler.Events;

public class StopResume extends BusyLoops {

    public static void main(String[] args) throws Exception {

        for (int i = 0; i < 5; i++) {
            method1();
            AsyncProfiler.getInstance().resume(Events.CPU, 1_000_000);
            method2();
            AsyncProfiler.getInstance().stop();
            method3();
        }

        String profile = AsyncProfiler.getInstance().dumpTraces(100);
        System.out.println(profile);
    }
}
