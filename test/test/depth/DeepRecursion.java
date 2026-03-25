/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.depth;

import java.util.concurrent.ThreadLocalRandom;

public class DeepRecursion {
    private static int BASE_DEPTH;
    private static int VAR_DEPTH;

    private final int[] count = new int[4];
    private int depth = 1;

    private void m0() {
        count[0]++;
        dispatch();
    }

    private void m1() {
        count[1]++;
        dispatch();
    }

    private void m2() {
        count[2]++;
        dispatch();
    }

    private void m3() {
        count[3]++;
        dispatch();
    }

    private void dispatch() {
        if (depth + 2 > BASE_DEPTH + ThreadLocalRandom.current().nextInt(VAR_DEPTH + 1)) {
            return;
        }

        depth += 2;
        switch (ThreadLocalRandom.current().nextInt(4)) {
            case 0:
                m0();
                break;
            case 1:
                m1();
                break;
            case 2:
                m2();
                break;
            case 3:
                m3();
                break;
        }
        depth -= 2;
    }

    public static void main(String[] args) throws Exception {
        BASE_DEPTH = args.length > 0 ? Integer.parseInt(args[0]) : 100;
        VAR_DEPTH = args.length > 1 ? Integer.parseInt(args[1]) : 0;
        boolean print = args.length > 2 && Boolean.parseBoolean(args[2]);

        DeepRecursion test = new DeepRecursion();
        for (int i = 0; ; i++) {
            test.dispatch();
            if (print && i % 1000000 == 0) {
                System.out.println("Made " + i + " calls");
            }
        }
    }
}
