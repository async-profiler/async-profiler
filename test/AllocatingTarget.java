import java.util.Random;

public class AllocatingTarget implements Runnable {
    public static volatile Object sink;

    public static void main(String[] args) {
        new Thread(new AllocatingTarget(), "AllocThread-1").start();
        new Thread(new AllocatingTarget(), "AllocThread-2").start();
    }

    @Override
    public void run() {
        Random random = new Random();
        while (true) {
            allocate(random);
        }
    }

    private static void allocate(Random random) {
        if (random.nextBoolean()) {
            sink = new int[128 * 1000];
        } else {
            sink = new Integer[128 * 1000];
        }
    }
}
