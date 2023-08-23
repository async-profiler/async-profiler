package test.lock;

import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.DatagramChannel;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicLong;

/**
 * This test sends UDP datagrams in 10 threads simultaneously
 * and calculates the total throughput (packets/s).
 *
 * The default DatagramChannel in Java NIO demonstrates poor performance
 * because of the synchronized block inside send() method.
 *
 * Use async-profiler's lock profiling mode (-e lock)
 * to find the source of lock contention.
 */
public class DatagramTest {
    private static final AtomicLong totalPackets = new AtomicLong();
    private static DatagramChannel ch;

    public static void sendLoop() {
        final ByteBuffer buf = ByteBuffer.allocateDirect(1000);
        final InetSocketAddress remoteAddr = new InetSocketAddress("127.0.0.1", 5556);

        try {
            while (true) {
                buf.clear();
                ch.send(buf, remoteAddr);
                totalPackets.incrementAndGet();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static void main(String[] args) throws Exception {
        ch = DatagramChannel.open();
        ch.bind(new InetSocketAddress(5555));
        ch.configureBlocking(false);

        Executor pool = Executors.newCachedThreadPool();
        for (int i = 0; i < 10; i++) {
            pool.execute(DatagramTest::sendLoop);
        }
    }
}
