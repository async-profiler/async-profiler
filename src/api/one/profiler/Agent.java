/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler;

import javax.management.ObjectName;
import java.lang.management.ManagementFactory;

public class Agent {

    public static void premain(String args) throws Exception {
        agentmain(args);
    }

    public static void agentmain(String args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();
        ManagementFactory.getPlatformMBeanServer().registerMBean(
                profiler,
                new ObjectName(AsyncProfilerMXBean.OBJECT_NAME));
        if (args != null && !args.isEmpty()) {
            profiler.execute(args);
        }
    }
}
