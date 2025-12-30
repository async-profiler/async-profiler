/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.api;

import one.profiler.Counter;

import javax.management.MBeanServer;
import javax.management.ObjectName;
import java.lang.management.ManagementFactory;

public class JavaAgent {

    public static void main(String[] args) throws Exception {
        MBeanServer mbs = ManagementFactory.getPlatformMBeanServer();
        ObjectName obj = new ObjectName("one.profiler:type=AsyncProfiler");

        String version = (String) mbs.getAttribute(obj, "Version");
        System.out.println(String.format("async-profiler version: %s", version));

        for (int i = 0; i < 3; i++) {
            BusyLoops.method1();
            BusyLoops.method2();
            BusyLoops.method3();
        }

        String profile = (String) mbs.invoke(
                obj,
                "dumpCollapsed",
                new String[] { Counter.SAMPLES.name() },
                new String[] { String.class.getName() }
            );
        System.out.println(profile);
    }
}
