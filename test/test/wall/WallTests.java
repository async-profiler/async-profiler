package test.wall;

import one.profiler.test.Output;
<<<<<<< HEAD
import one.profiler.test.Assert;
=======
>>>>>>> 2a8a0c7 (add back testing framework and update tests to fix most assertion failus)
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import java.time.LocalTime;

public class WallTests {

    @Test(mainClass = SocketTest.class)
    public void cpuWall(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -o collapsed");
<<<<<<< HEAD
        Assert.ratioGreater(out, "test/wall/SocketTest.main", 0.25);
        Assert.ratioGreater(out, "test/wall/BusyClient.run", 0.25);
        Assert.ratioLess(out, "test/wall/IdleClient.run", 0.05);
=======
        Thread.sleep(5000);
        assert out.ratio("test/wall/SocketTest.main") > 0.25;
        assert out.ratio("test/wall/BusyClient.run") > 0.25;
        assert out.ratio("test/wall/IdleClient.run") < 0.05;
>>>>>>> 2a8a0c7 (add back testing framework and update tests to fix most assertion failus)

        out = p.profile("-e wall -d 3 -o collapsed");
        long s1 = out.samples("test/wall/SocketTest.main");
        long s2 = out.samples("test/wall/BusyClient.run");
        long s3 = out.samples("test/wall/IdleClient.run");
        assert s1 > 10 && s2 > 10 && s3 > 10;
        assert Math.abs(s1 - s2) < 5 && Math.abs(s2 - s3) < 5 && Math.abs(s3 - s1) < 5;
    }
}
