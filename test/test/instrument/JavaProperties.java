/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.instrument;

public class JavaProperties {

    public static void main(String[] main) {
        int count = 0;
        for (int i = 0; i < 1_000_000; i++) {
            count += System.getProperties().hashCode();
        }
        System.out.println(count);
    }
}
