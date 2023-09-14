package test.pmu;

import one.profiler.test.Output;

import java.io.IOException;

import one.profiler.test.Assert;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.OsType;

// Tests require perfevents to be enabled to pass
public class PmuTests {

    @Test(mainClass = Dictionary.class, enabled = false, os = {OsType.LINUX})
    public void cycles(TestProcess p) throws Exception {
        p.profile("-e cycles -d 3 -o collapsed -f %f");
        Output out = p.readFile("%f");
        Assert.ratioGreater(out, "test/pmu/Dictionary.test128K", 0.4);
        Assert.ratioGreater(out, "test/pmu/Dictionary.test8M", 0.4);
    }

    @Test(mainClass = Dictionary.class, enabled = false, os = {OsType.LINUX})
    public void cacheMisses(TestProcess p) throws Exception {
        p.profile("-e cache-misses -d 3 -o collapsed -f %f");
        
        Output out = p.readFile("%f");
        Assert.ratioLess(out, "test/pmu/Dictionary.test128K", 0.2);
        Assert.ratioGreater(out, "test/pmu/Dictionary.test8M", 0.8);
    }

    @Test(mainClass = Dictionary.class, os = {OsType.MACOS, OsType.WINDOWS})
    public void pmuIncompatible(TestProcess p) throws Exception {
         try {
            p.profile("-e cache-misses -d 3 -o collapsed -f %f");
            throw new AssertionError("Somehow accessed PerfEvents on macOS???");
        } catch(IOException e) {
            Assert.contains(p.readPErr(), "PerfEvents are unsupported on macOS");
        }
    }
}
