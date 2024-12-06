/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;

import java.util.Random;

class Ttsp {
    static private double loop(){
        byte[] byteArray = new byte[1024 * 1024 * 1024];
        for (int i = 0; i < 10000; i++) {
            new Random().nextBytes(byteArray);
        }
        return 1;
    }
    public static void main(String[] args) throws Exception {
        new Thread(() -> {
            while (true) {
                System.gc();
                try {
                    Thread.sleep(20);
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
            }
        }).start();
        Thread.sleep(1000);
        new Thread(() -> {
            while (true) {
                loop();
            }
        }).start();
    }
}

