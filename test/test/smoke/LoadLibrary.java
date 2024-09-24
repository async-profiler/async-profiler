/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.smoke;

import java.lang.management.ClassLoadingMXBean;
import java.lang.management.ManagementFactory;

public class LoadLibrary {

    public static void main(String[] args) throws Exception {
        for (int i = 0; i < 20; i++) {
            Thread.sleep(100);
        }

        // Late load of libmanagement.so
        ClassLoadingMXBean bean = ManagementFactory.getClassLoadingMXBean();

        long n = 0;
        while (n >= 0) {
            n += bean.getLoadedClassCount();
            n += bean.getTotalLoadedClassCount();
            n += bean.getUnloadedClassCount();
        }
    }
}
