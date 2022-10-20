package utils;

import java.io.File;

public class Utils {
        public static String getAsyncProfilerLib() {
            try {
                File root = new File(Utils.class
                    .getResource("Utils.class").toURI()).getParentFile();
                while (!root.getName().equals("async-profiler")) {
                    root = root.getParentFile();
                }
                return root.toPath().resolve("build/libasyncProfiler.so").toAbsolutePath().toString();
            } catch (Throwable t) {
                 throw new RuntimeException("Could not find asyncProfiler lib", t);
            }
        }
}
