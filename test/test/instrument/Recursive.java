/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.instrument;

import java.time.Duration;
import java.util.Random;

public class Recursive {
    private static final long MAX_RECURSION = 3;

    static void recursive(long recursionCount) throws InterruptedException {
        if (recursionCount > MAX_RECURSION) return;
        Thread.sleep((MAX_RECURSION - recursionCount) * 200);
        recursive(recursionCount+1);
    }

    public static void main(String[] args) throws InterruptedException {
        recursive(0);
    }
}
