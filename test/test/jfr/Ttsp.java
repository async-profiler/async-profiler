/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;

import java.io.File;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;
import java.io.BufferedOutputStream;
import java.io.FileOutputStream;

class Ttsp {
    public static void main(String[] args) throws Exception {
        new Thread(() -> {
            while (true) {
                Thread.getAllStackTraces();
                try {
                    Thread.sleep(50);
                } catch (InterruptedException e) {
                    return;
                }
            }
        }).start();

        byte[] data = new byte[1024 * 1024 * 1024];
        Arrays.fill(data, (byte) 'A');
        while (true) {
            File temp = File.createTempFile("temp", null);
            try {
                temp.deleteOnExit();
            } catch (IllegalStateException e) {
                // the JVM is already exiting, delete the file and exit
                temp.delete();
                break;
            }
            Files.write(temp.toPath(), data);
            temp.delete();
        }
    }
}

