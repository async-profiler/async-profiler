package test.cpu;

import one.profiler.test.Output;
import one.profiler.test.Assert;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.OsType;
import one.profiler.test.JvmType;

public class CpuTests {

    @Test(mainClass = RegularPeak.class, jvmArgs = "-XX:+UseG1GC -Xmx1g -Xms1g", jvm = {JvmType.HOTSPOT})
    public void regularPeak(TestProcess p) throws Exception {
        p.profile("-e cpu -d 3 -f %f.jfr");
        System.out.println("it closed already");
        Output out = p.readFile("%f");
        Assert.contains(out, "java/util/stream/SpinedBuffer\\.accept");
    }

}
