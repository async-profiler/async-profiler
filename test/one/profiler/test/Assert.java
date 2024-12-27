/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.util.function.BiPredicate;
import java.util.logging.Level;
import java.util.logging.Logger;

public class Assert {
    enum Comparison {
        GT(">", (a, b) -> a > b),
        GTE(">=", (a, b) -> a >= b),
        LT("<", (a, b) -> a < b),
        LTE("<=", (a, b) -> a <= b),
        EQ("==", Double::equals),
        NE("!=", (a, b) -> !Double.isNaN(a) && !Double.isNaN(b) && !a.equals(b));

        public final String operator;
        public final BiPredicate<Double, Double> comparator;

        Comparison(String operator, BiPredicate<Double, Double> comparator) {
            this.operator = operator;
            this.comparator = comparator;
        }
    }

    private static final Logger log = Logger.getLogger(Assert.class.getName());

    private static void assertComparison(Comparison comparison, double left, double right, @SuppressWarnings("unused") String message) {
        boolean asserted = !comparison.comparator.test(left, right);

        // message parameter will be part of the source code line.
        String assertionMessage = String.format("%s %s %s\n%s", left, comparison.operator, right, SourceCode.tryGet(2));
        log.log(Level.FINE, String.format("isAsserted %s: %s", asserted, assertionMessage));
        if (asserted) {
            throw new AssertionError("Expected " + assertionMessage);
        }
    }

    public static void isEqual(double left, double right) {
        assertComparison(Comparison.EQ, left, right, null);
    }

    public static void isEqual(double left, double right, String message) {
        assertComparison(Comparison.EQ, left, right, message);
    }

    public static void isNotEqual(double left, double right) {
        assertComparison(Comparison.NE, left, right, null);
    }

    public static void isNotEqual(double left, double right, String message) {
        assertComparison(Comparison.NE, left, right, message);
    }

    public static void isGreater(double left, double right) {
        assertComparison(Comparison.GT, left, right, null);
    }

    public static void isGreater(double left, double right, String message) {
        assertComparison(Comparison.GT, left, right, message);
    }

    public static void isGreaterOrEqual(double left, double right) {
        assertComparison(Comparison.GTE, left, right, null);
    }

    public static void isGreaterOrEqual(double left, double right, String message) {
        assertComparison(Comparison.GTE, left, right, message);
    }

    public static void isLess(double left, double right) {
        assertComparison(Comparison.LT, left, right, null);
    }

    public static void isLess(double left, double right, String message) {
        assertComparison(Comparison.LT, left, right, message);
    }

    public static void isLessOrEqual(double left, double right) {
        assertComparison(Comparison.LTE, left, right, null);
    }

    public static void isLessOrEqual(double left, double right, String message) {
        assertComparison(Comparison.LTE, left, right, message);
    }
}
