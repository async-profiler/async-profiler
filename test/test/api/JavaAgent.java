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
import javax.management.ObjectName;
import javax.management.ReflectionException;

public class JavaAgent {

    public static void main(String[] args) {
        try {
            String version = (String) ManagementFactory.getPlatformMBeanServer().getAttribute(new ObjectName("one.profiler:type=AsyncProfiler"), "Version");
            System.out.println(String.format("async-profiler version: %s", version));
        } catch (InstanceNotFoundException
                | AttributeNotFoundException
                | MalformedObjectNameException
                | ReflectionException
                | MBeanException e) {
            e.printStackTrace();
        }
    }

}
