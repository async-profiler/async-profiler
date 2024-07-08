/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

public class Assert {

    public static void contains(Output out, String regex) throws AssertionError {
        if (!out.contains(regex)) {
            throw new AssertionError("Expected to contain: " + regex);
        }
    }

    public static void notContains(Output out, String regex) throws AssertionError {
        if (out.contains(regex)) {
            throw new AssertionError("Expected to not contain: " + regex);
        }
    }

    public static void isGreater(double value, double threshold) {
        if (value < threshold) {
            throw new AssertionError("Expected ratio > " + threshold + ", but Actual: " + value);
        }
    }

    public static void isLess(double value, double threshold) {
        if (value > threshold) {
            throw new AssertionError("Expected ratio < " + threshold + "but Actual: " + value);
        }
    }
}
