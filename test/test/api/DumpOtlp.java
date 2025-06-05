/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.api;

import java.nio.ByteBuffer;
import one.profiler.AsyncProfiler;
import one.profiler.Counter;
import one.profiler.Events;

public class DumpOtlp extends BusyLoops {

    public static void main(String[] args) throws Exception {
        AsyncProfiler.getInstance().start(Events.CPU, 1_000_000);

        for (int i = 0; i < 5; i++) {
            method1();
            method2();
            method3();
        }

        // TODO: Should we test this further?
        byte[] profile = AsyncProfiler.getInstance().dumpOtlp();
        System.out.println(profile.length);
    }
}
