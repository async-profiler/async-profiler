/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.lock;

import java.net.InetSocketAddress;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.channels.DatagramChannel;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicLong;

/**
 * This test sends UDP datagrams in 10 threads simultaneously
 * and calculates the total throughput (packets/s).
 * <p>
 * The default DatagramChannel in Java NIO demonstrates poor performance
 * because of the synchronized block inside send() method.
 * <p>
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
            for (int i = 0; ; i++) {
                ((Buffer) buf).clear();
                ch.send(buf, remoteAddr);
                totalPackets.incrementAndGet();
                if ((i % 1000) == 0) {
                    Thread.yield();  // give other threads chance to acquire a lock
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static void main(String[] args) throws Exception {
        ch = DatagramChannel.open();
        ch.bind(new InetSocketAddress(0));
        ch.configureBlocking(false);

        Executor pool = Executors.newCachedThreadPool();
        for (int i = 0; i < 10; i++) {
            pool.execute(DatagramTest::sendLoop);
        }
    }
}
