/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.nativemem;

public class CallsRealloc {

    private static final int MALLOC_SIZE = 1999993; // Prime size, useful in assertions.
    private static final int REALLOC_SIZE = 30000170;

    public static void main(String[] args) throws InterruptedException {
        final boolean once = args.length > 0 && args[0].equals("once");

        do {
            long addr = Native.malloc(MALLOC_SIZE);
            long reallocd = Native.realloc(addr, REALLOC_SIZE);

            // allocate every 1 second.
            Thread.sleep(1000);
        } while (!once);
    }
}
