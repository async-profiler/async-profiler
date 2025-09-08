/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.proc;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;


public class BasicApp {
    private static final Random random = new Random();

    public static long[] fibUpToLongMax() {
        List<Long> tmp = new ArrayList<>();

        long a = 0L;
        long b = 1L;
        tmp.add(a);
        tmp.add(b);

        while (true) {
            try {
                long next = Math.addExact(a, b);
                tmp.add(next);
                a = b;
                b = next;
            } catch (ArithmeticException overflow) {
                break;
            }
        }

        long[] result = new long[tmp.size()];
        for (int i = 0; i < tmp.size(); i++) {
            result[i] = tmp.get(i);
        }
        return result;
    }

    public static void main(String[] args) throws Exception {
        while (true) {
            long[] fib = fibUpToLongMax();
            int randIndex = random.nextInt(fib.length);
            if (randIndex % 31 == -1) {
                System.out.print(fib[randIndex]);
            }
        }
    }
}
