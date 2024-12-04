/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;


class Ttsp {
    static private double loop(int iterations, double arg) {
        double total = 0;
        for (int i = 0; i < iterations; i++) {
            total += Math.sin(Math.cos(Math.tan(arg)));
        }
        return total;
    }
    public static void main(String[] args) throws Exception {
        new Thread(() -> {
            while (true) {
                loop(10000, 1);
                try {
                    Thread.sleep(10);
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
            }
        }).start();
        new Thread(() -> {
            while (true) {
                System.gc();
                try {
                    Thread.sleep(10);
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
            }
        }).start();
    }
}

