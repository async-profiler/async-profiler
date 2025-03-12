/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.cpu;

import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;

import javax.management.MBeanServerConnection;
import javax.management.ObjectName;
import javax.management.remote.JMXConnector;
import javax.management.remote.JMXConnectorFactory;
import javax.management.remote.JMXServiceURL;
import java.io.IOException;

/**
 * Obtain the CPU time of a process using the Attach API
 */
class CpuTimeService implements AutoCloseable {

    private final VirtualMachine vm;
    private final MBeanServerConnection mbeanServer;
    private final JMXConnector jmxConnector;

    CpuTimeService(long pid) throws IOException, AttachNotSupportedException {
        vm = VirtualMachine.attach(pid + "");
        String connectorAddress = vm.startLocalManagementAgent();
        JMXServiceURL url = new JMXServiceURL(connectorAddress);
        jmxConnector = JMXConnectorFactory.connect(url);
        mbeanServer = jmxConnector.getMBeanServerConnection();
    }

    long getProcessCpuTimeNanos() throws Exception {
       return (long)mbeanServer.getAttribute(new ObjectName("java.lang:type=OperatingSystem"), "ProcessCpuTime");
    }

    @Override
    public void close() throws IOException {
        jmxConnector.close();
        vm.detach();
    }
}
