/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.api;

import java.security.SecureRandom;
import java.time.Instant;

class BusyLoops {

    static long method1() {
        long result = 0;
        for (int i = 0; i < 1_000_000; i++) {
            result += Integer.toString(i).hashCode();
        }
        return result;
    }

    static long method2() {
        long iterations = 0;
        Instant end = Instant.now().plusMillis(100);
        while (Instant.now().isBefore(end)) {
            iterations++;
        }
        return iterations;
    }

    static long method3() {
        long iterations = 0;
        SecureRandom r = new SecureRandom();
        while ((r.nextInt() % 1_000_000) != 0) {
            iterations++;
        }
        return iterations;
    }
}
