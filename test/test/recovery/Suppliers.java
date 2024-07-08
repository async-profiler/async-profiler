/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.recovery;

import java.util.function.Supplier;

/**
 * In this test, a large amount of time is spent inside the itable stub.
 * <p>
 * Most sampling profilers, including AsyncGetCallTrace-based,
 * fail to traverse Java stack in these cases.
 * See https://bugs.openjdk.java.net/browse/JDK-8178287
 */
public class Suppliers {

    public static void main(String[] args) {
        Supplier[] suppliers = {
                () -> 0,
                () -> 1.0,
                () -> "abc",
                () -> true
        };

        while (true) {
            loop(suppliers);
        }
    }

    private static void loop(Supplier[] suppliers) {
        for (int i = 0; i >= 0; i++) {
            suppliers[i % suppliers.length].get();
        }
    }
}