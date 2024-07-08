/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.recovery;

/**
 * This demo shows that most sampling profilers are misleading.
 * The given program appends 5 symbols to the end of StringBuilder
 * and deletes 5 symbols from the beginning of StringBuilder.
 * <p>
 * The real bottleneck here is delete(), since it involves moving
 * of 1 million characters. However, safepoint-based profilers
 * will display Thread.isAlive() as the hottest method.
 * JFR will not report anything useful at all, since it cannot
 * traverse stack traces when JVM is running System.arraycopy().
 */
public class StringBuilderTest {

    public static void main(String[] args) {
        StringBuilder sb = new StringBuilder();
        sb.append(new char[1_000_000]);

        do {
            sb.append(12345);
            sb.delete(0, 5);
        } while (Thread.currentThread().isAlive());
    }
}
