import java.util.concurrent.ThreadLocalRandom;

public class AllocatingTarget implements Runnable {
    public static volatile Object sink;

    public static void main(String[] args) {
        new Thread(new AllocatingTarget(), "AllocThread-1").start();
        new Thread(new AllocatingTarget(), "AllocThread-2").start();
    }

    @Override
    public void run() {
        while (true) {
            allocate();
        }
    }

    private static void allocate() {
        if (ThreadLocalRandom.current().nextBoolean()) {
            sink = new int[128 * 1000];
        } else {
            sink = new Integer[128 * 1000];
        }
    }
}
