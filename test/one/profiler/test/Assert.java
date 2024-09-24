/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

public class Assert {

    public static void isGreater(double value, double threshold) {
        if (value <= threshold) {
            throw new AssertionError("Expected " + value + " > " + threshold);
        }
    }

    public static void isGreaterOrEqual(double value, double threshold) {
        if (value < threshold) {
            throw new AssertionError("Expected " + value + " >= " + threshold);
        }
    }

    public static void isLess(double value, double threshold) {
        if (value >= threshold) {
            throw new AssertionError("Expected " + value + " < " + threshold);
        }
    }

    public static void isLessOrEqual(double value, double threshold) {
        if (value > threshold) {
            throw new AssertionError("Expected " + value + " <= " + threshold);
        }
    }
}
