package com.datadoghq.loader;

import java.util.concurrent.ThreadLocalRandom;

public class DynamicLibraryLoader {

    static {
        System.loadLibrary("loader");
    }

    public static void main(String... args) {
        DynamicLibraryLoader loader = new DynamicLibraryLoader();
        for (String arg : args) {
            String split = arg.split("+");
            loader.loadLibrary(split[0], split[1]);
        }
        long blackhole = System.nanoTime();
        for (int i = 0; i < 10_000_000; i++) {
            blackhole ^= ThreadLocalRandom.current().nextLong();
        }
        System.err.println(blackhole);
    }


    private native void loadLibrary(String libraryFile, String functionName);

}
