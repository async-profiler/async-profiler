/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.proc;

import java.util.Random;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.security.SecureRandom;

public class IoIntensiveApp {
    private static final Random random = new Random();
    private static final int BLOCK = 64 * 1024 * 1024;

    public static void main(String[] args) throws Exception {
        Path tmp = Files.createTempFile("proc-test", ".tmp");

        // burn some cpu so we jfr includes it
        new Thread(() -> {
                while (true) {
                    long n = random.nextLong();
                    if (Long.toString(n).hashCode() == 0) {
                        System.out.println(n);
                    }
                }
        }).start();

        final int SIZE = 64 * 1024 * 1024;
        byte[] payload = new byte[SIZE];
        new SecureRandom().nextBytes(payload);

        try (FileChannel ch = FileChannel.open(
                 tmp,
                 StandardOpenOption.CREATE,
                 StandardOpenOption.TRUNCATE_EXISTING,
                 StandardOpenOption.WRITE)) {

            ByteBuffer buf = ByteBuffer.wrap(payload);
            while (buf.hasRemaining()) {
                ch.write(buf);
            }

            ch.force(true);
        }

        byte[] reread = Files.readAllBytes(tmp);
        System.out.println("Read " + reread.length + " bytes.");
        Thread.sleep(20000);
    }
}
