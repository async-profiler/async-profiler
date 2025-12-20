/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.api;

import java.lang.management.ManagementFactory;

import javax.management.AttributeNotFoundException;
import javax.management.InstanceNotFoundException;
import javax.management.MBeanException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.ReflectionException;

import one.profiler.Counter;

public class JavaAgent extends BusyLoops {
    public static void main(String[] args) throws Exception {
        MBeanServer mbs = ManagementFactory.getPlatformMBeanServer();
        ObjectName obj = new ObjectName("one.profiler:type=AsyncProfiler");

        String version = (String) mbs.getAttribute(obj, "Version");
        System.out.println(String.format("async-profiler version: %s", version));

        for (int i = 0; i < 5; i++) {
            method1();
            method2();
            method3();
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
