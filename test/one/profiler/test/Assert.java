/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.util.HashMap;
import java.util.Map;
import java.util.function.BiPredicate;
import java.util.logging.Level;
import java.util.logging.Logger;


enum Comparison {
    GT, GTE, LT, LTE,
}

public class Assert {
    private static final Logger log = Logger.getLogger(Assert.class.getName());
    private static final Map<Comparison, String> operator = new HashMap<>();
    private static final Map<Comparison, BiPredicate<Double, Double>> comparator = new HashMap<>();

    static {
        operator.put(Comparison.GT, ">");
        operator.put(Comparison.GTE, ">=");
        operator.put(Comparison.LT, "<");
        operator.put(Comparison.LTE, "<=");

        comparator.put(Comparison.GT, (a, b) -> a > b);
        comparator.put(Comparison.GTE, (a, b) -> a >= b);
        comparator.put(Comparison.LT, (a, b) -> a < b);
        comparator.put(Comparison.LTE, (a, b) -> a <= b);
    }

    private static void assertComparison(Comparison comparison, double left, double right, String message) {
        boolean asserted = !comparator.get(comparison).test(left, right);
        String operation = left + " " + operator.get(comparison) + " " + right;

        log.log(Level.FINE, "isAsserted " + asserted + ", "
                + (message == null ? "" : "'" + message + "'") + ": "
                + operation);

        if (asserted) {
            throw new AssertionError("Expected " + operation + (message != null ? (": " + message) : ""));
        }
    }

    public static void isGreater(double left, double right) {
        isGreater(left, right, null);
    }

    public static void isGreater(double left, double right, String message) {
        assertComparison(Comparison.GT, left, right, message);
    }

    public static void isGreaterOrEqual(double left, double right) {
        isGreaterOrEqual(left, right, null);
    }

    public static void isGreaterOrEqual(double left, double right, String message) {
        assertComparison(Comparison.GTE, left, right, message);
    }

    public static void isLess(double left, double right) {
        isLess(left, right, null);
    }

    public static void isLess(double left, double right, String message) {
        assertComparison(Comparison.LT, left, right, message);
    }

    public static void isLessOrEqual(double left, double right) {
        isLessOrEqual(left, right, null);
    }

    public static void isLessOrEqual(double left, double right, String message) {
        assertComparison(Comparison.LTE, left, right, message);
    }
}
