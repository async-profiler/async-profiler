/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.api;

import one.profiler.AsyncProfiler;

import java.io.IOException;

public class OutputAtStart {

    public static void main(String[] args) throws IOException {
        if (args.length < 1) {
            throw new IllegalArgumentException("Expecting at least one argument for output file");
        }

        AsyncProfiler instance = AsyncProfiler.getInstance();
        instance.execute("start,event=cpu,file=" + args[0]);

        BusyLoops.method1();
        BusyLoops.method2();
        BusyLoops.method3();

        instance.execute("stop");
    }
}
