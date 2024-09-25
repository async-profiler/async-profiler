package test.jfr;

import java.util.Random;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.TimeUnit;
import java.util.stream.IntStream;

/**
 * Process to simulate lock contention.
 */
public class JfrMutliModeProfiling {
    static Object sink;
    static volatile int count = 0;

    public static void main(String[] args) throws InterruptedException {
        ExecutorService executor = Executors.newFixedThreadPool(3);
        IntStream.range(0, 100_000).forEach(i -> executor.submit(JfrMutliModeProfiling::increment));
        stop(executor);
        allocate();
    }

    static synchronized void increment() {
        count += 1;
    }

    static void stop(ExecutorService executor) {
        try {
            executor.shutdown();
            executor.awaitTermination(10, TimeUnit.SECONDS);
        }
        catch (InterruptedException e) {
            System.err.println("Termination interrupted!");
        }
        finally {
            if (!executor.isTerminated()) {
                System.err.println("Killing non-finished!");
            }
            executor.shutdownNow();
        }
    }

    private static void allocate() {
        long start = System.currentTimeMillis();
        Random random = ThreadLocalRandom.current();
        while (System.currentTimeMillis() - start <= 1000) {
            if (random.nextBoolean()) {
                sink = new int[128 * 1000];
            } else {
                sink = new Integer[128 * 1000];
            }
        }
    }
}
