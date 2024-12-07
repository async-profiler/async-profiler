/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.cstack;

import java.io.FileInputStream;
import java.util.Arrays;

public class LongInitializer {

    static class InnerClass {
        static {
            byte[] bytes = new byte[16];
            byte[] empty = new byte[16];
            try (FileInputStream fis = new FileInputStream("/dev/urandom")) {
                long count = 0;
                while (fis.read(bytes) > 0 && !Arrays.equals(bytes, empty)) {
                    count++;
                }
                System.out.println("You are lucky! " + count);
            } catch (Exception e) {
                throw new IllegalStateException(e);
            }
        }
    }

    public static void main(String[] args) throws Exception {
        Class.forName(InnerClass.class.getName());
    }
}
