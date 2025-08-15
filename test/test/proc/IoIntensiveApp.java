/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.proc;

import java.util.Random;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.SecureRandom;
import com.sun.nio.file.ExtendedOpenOption;

public class IoIntensiveApp {
    private static final Random random = new Random();
    private static final int BLOCK = 64 * 1024 * 1024;

    public static void main(String[] args) throws Exception {
        Path tmp = Files.createTempFile("proc-test", ".tmp");

        // burn some cpu to pass the min cpu % threshold
        new Thread(() -> {
            while (true) {
                long n = random.nextLong();
                if (Long.toString(n).hashCode() == 0) {
                    System.out.println(n);
                }
            }
        }).start();

        // write
        byte[] payload = new byte[BLOCK];
        new SecureRandom().nextBytes(payload);

        try (FileChannel ch = FileChannel.open(tmp, StandardOpenOption.CREATE,
                StandardOpenOption.TRUNCATE_EXISTING, StandardOpenOption.WRITE, ExtendedOpenOption.DIRECT)) {
            ByteBuffer buf = ByteBuffer.wrap(payload);
            while (buf.hasRemaining()) {
                ch.write(buf);
            }

            ch.force(true);
        }

        // read
        try (FileChannel f = FileChannel.open(tmp, ExtendedOpenOption.DIRECT)) {
            ByteBuffer b = ByteBuffer.allocate(BLOCK);
            f.read(b);
        }

        Thread.sleep(20000);
    }
}
