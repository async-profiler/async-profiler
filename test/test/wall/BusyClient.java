/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.wall;

import java.io.InputStream;
import java.net.Socket;

class BusyClient extends Thread {

    @Override
    public void run() {
        try {
            byte[] buf = new byte[4096];

            Socket s = new Socket(SocketTest.HOST, SocketTest.PORT);

            InputStream in = s.getInputStream();
            while (in.read(buf) >= 0) {
                // keep reading
            }
            System.out.println(Thread.currentThread().getName() + " stopped");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
