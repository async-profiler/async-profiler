/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativemem;

public class CallsMallocCalloc {

    private static final int MALLOC_SIZE = 1999993; // Prime size, useful in assertions.
    private static final int CALLOC_SIZE = 2000147;

    public static void main(String[] args) throws InterruptedException {
        final boolean once = args.length > 0 && args[0].equals("once");

        do {
            Native.malloc(MALLOC_SIZE);
            Native.calloc(1, CALLOC_SIZE);

            // allocate every 1 second.
            Thread.sleep(1000);
        } while (!once);
    }
}
