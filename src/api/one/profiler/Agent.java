/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler;

import java.io.IOException;
import java.lang.management.ManagementFactory;

import javax.management.InstanceAlreadyExistsException;
import javax.management.MBeanRegistrationException;
import javax.management.MalformedObjectNameException;
import javax.management.NotCompliantMBeanException;
import javax.management.ObjectName;

public class Agent {

    public static void premain(String args) throws InstanceAlreadyExistsException,
           MBeanRegistrationException,
           NotCompliantMBeanException,
           MalformedObjectNameException,
           IllegalArgumentException,
           IllegalStateException,
           IOException {
        agentmain(args);
    }

    public static void agentmain(String args) throws InstanceAlreadyExistsException,
           MBeanRegistrationException,
           NotCompliantMBeanException,
           MalformedObjectNameException,
           IllegalArgumentException,
           IllegalStateException,
           IOException {
        AsyncProfiler instance = AsyncProfiler.getInstance();
            ManagementFactory.getPlatformMBeanServer().registerMBean(
                    instance,
                    new ObjectName(AsyncProfilerMXBean.OBJECT_NAME));
        if (!(args == null || "".equals(args))) {
            instance.execute(args);
        }
    }

}
