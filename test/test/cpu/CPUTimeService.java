package test.cpu;

import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;

import javax.management.MBeanServerConnection;
import javax.management.ObjectName;
import javax.management.remote.JMXConnector;
import javax.management.remote.JMXConnectorFactory;
import javax.management.remote.JMXServiceURL;
import java.io.IOException;
import java.util.Set;

/**
 * Obtain the CPU time of a process using the Attach API
 */
class CPUTimeService implements AutoCloseable {

    private final VirtualMachine vm;
    private final MBeanServerConnection mbeanServer;
    private final JMXConnector jmxConnector;

    CPUTimeService(long pid) throws IOException, AttachNotSupportedException {
        vm = VirtualMachine.attach(pid + "");
        String connectorAddress = vm.startLocalManagementAgent();
        JMXServiceURL url = new JMXServiceURL(connectorAddress);
        jmxConnector = JMXConnectorFactory.connect(url);
        mbeanServer = jmxConnector.getMBeanServerConnection();
    }

    long getProcessCPUTimeNanos() throws Exception {
        Set<ObjectName> mbeans = mbeanServer.queryNames(new ObjectName("java.lang:type=OperatingSystem"), null);
        if (mbeans.isEmpty()) {
            throw new Exception("No OperatingSystem MBean found");
        }
        ObjectName osMBean = mbeans.iterator().next();
        return (long) mbeanServer.getAttribute(osMBean, "ProcessCpuTime");
    }

    @Override
    public void close() throws IOException {
        jmxConnector.close();
        vm.detach();
    }
}
