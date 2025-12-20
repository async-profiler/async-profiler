/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler;

import java.lang.management.ManagementFactory;

import javax.management.ObjectName;

public class Agent {

    public static void premain(String args) throws Exception {
        agentmain(args);
    }

    public static void agentmain(String args) throws Exception {
        AsyncProfiler instance = AsyncProfiler.getInstance();
            ManagementFactory.getPlatformMBeanServer().registerMBean(
                    instance,
                    new ObjectName(AsyncProfilerMXBean.OBJECT_NAME));
        if (args != null && !args.isEmpty()) {
            instance.execute(args);
        }
    }

}
