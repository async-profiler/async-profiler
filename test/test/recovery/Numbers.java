/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.recovery;

/**
 * In this test, a large amount of time is spent inside the vtable stub
 * and inside the method prologue/epilogue.
 * <p>
 * Most sampling profilers, including AsyncGetCallTrace-based,
 * fail to traverse Java stack in these cases.
 * See https://bugs.openjdk.java.net/browse/JDK-8178287
 */
public class Numbers {
    static volatile double x;

    public static void main(String[] args) {
        loop(123, 45.67, 890L, 33.3f, 999, 787878L, 10.11f, 777L, 0);
    }

    private static void loop(Number... numbers) {
        while (true) {
            x = avg(numbers);
        }
    }

    private static double avg(Number... numbers) {
        double sum = 0;
        for (Number n : numbers) {
            sum += n.doubleValue();
        }
        return sum / numbers.length;
    }
}
