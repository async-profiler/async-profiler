/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;

import java.lang.management.ManagementFactory;
import java.util.Arrays;

public class Ttsp {
    static volatile int sink;

    // String.indexOf is a JVM intrinsic. When JIT-compiled, it has no safepoint check inside
    // and therefore may delay safepoint start.
    static int indexOfTest(int length) {
        char[] chars = new char[length * length];
        Arrays.fill(chars, 'a');
        String haystack = new String(chars);
        String needle = haystack.substring(0, length) + 'b' + haystack.substring(0, length);
        return haystack.indexOf(needle);
    }

    static void spoiler(int length, long count) {
        for (long i = 0; i < count; i++) {
            sink = indexOfTest(length);
        }
    }

    static void requestSafepoint() {
        ManagementFactory.getThreadMXBean().dumpAllThreads(false, false);
    }

    public static void main(String[] args) throws Exception {
        // Warmup with small input to force JIT-compilation of indexOfTest
        spoiler(10, 1000000);

        // Run actual workload with large input to cause long time-to-safepoint pauses
        new Thread(() -> spoiler(1000, Long.MAX_VALUE)).start();

        while (true) {
            requestSafepoint();
            Thread.sleep(200);
        }
    }
}
