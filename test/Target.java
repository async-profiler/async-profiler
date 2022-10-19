import java.io.File;
import java.nio.file.Paths;

import one.profiler.AsyncProfiler;

class Target {
    private static volatile int value;

    private static void method1() {
        for (int i = 0; i < 1000000; ++i) {
            ++value;
            tryDump();
        }
    }

    private static void method2() {
        for (int i = 0; i < 1000000; ++i) {
            ++value;
            tryDump();
        }
    }

    private static void method3() throws Exception {
        for (int i = 0; i < 1000; ++i) {
            for (String s : new File("/tmp").list()) {
                value += s.hashCode();
            }
            tryDump();
        }
    }

    private static String dumpPath;
    private static int dumpAfterSecs;
    private static long ts;

    public static void main(String[] args) throws Exception {
        System.out.println("Starting target app ...");
        dumpPath = args.length > 0 ? args[0] : null;
        dumpAfterSecs = args.length > 1 ? Integer.parseInt(args[1]) : -1;

        if (dumpPath != null && dumpAfterSecs == -1) {
            throw new IllegalArgumentException("Dump config requires both path and the interal. Only path was provided.");
        }
 
        ts = System.nanoTime();
        while (true) {
            method1();
            method2();
            method3();
        }
    }

    private static void tryDump() {
        if (dumpPath != null && ts > 0) {
            if (System.nanoTime() - ts > (dumpAfterSecs * 1_000_000_000L)) {
                if (!AsyncProfiler.getInstance().dumpJfr(Paths.get(dumpPath))) {
                    throw new IllegalStateException("Unable to dump JFR data to " + dumpPath);
                }
                ts = -1;
            }
        }
    }
}
