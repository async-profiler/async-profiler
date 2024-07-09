/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.wall;

import java.io.OutputStream;
import java.net.ServerSocket;
import java.util.concurrent.ThreadLocalRandom;

/**
 * This test starts two similar clients. The code of the clients
 * is the same, except that BusyClient constantly receives new data,
 * while IdleClient does almost nothing but waiting for incoming data.
 * <p>
 * Most sampling profilers will not make difference between
 * BusyClient and IdleClient, because JVM does not know whether
 * a native method consumes CPU or just waits inside a blocking call.
 */
public class SocketTest {
    public static final String HOST = "127.0.0.1";
    public static final int PORT = 1234;

    public static void main(String[] args) throws Exception {
        ServerSocket s = new ServerSocket(PORT);

        new IdleClient().start();
        OutputStream idleClient = s.accept().getOutputStream();

        new BusyClient().start();
        OutputStream busyClient = s.accept().getOutputStream();

        byte[] buf = new byte[4096];
        ThreadLocalRandom.current().nextBytes(buf);

        for (int i = 0; ; i++) {
            if ((i % 10_000_000) == 0) {
                idleClient.write(buf, 0, 1);
            } else {
                busyClient.write(buf);
            }
        }
    }
}
