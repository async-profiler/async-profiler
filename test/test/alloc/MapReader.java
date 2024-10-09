/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.alloc;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.Base64;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ThreadLocalRandom;

public class MapReader {
    private final byte[] input;

    public MapReader(int size) throws IOException {
        this.input = generate(size);
    }

    private static byte[] generate(int size) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream(size * 17);
        for (int i = 0; i < size; i++) {
            int length = ThreadLocalRandom.current().nextInt(1, 9);
            byte[] b = new byte[length];
            ThreadLocalRandom.current().nextBytes(b);
            String key = Base64.getEncoder().encodeToString(b);
            long value = ThreadLocalRandom.current().nextLong(1000000);
            out.write((key + ": " + value + "\n").getBytes(StandardCharsets.ISO_8859_1));
        }
        return out.toByteArray();
    }

    public Map<String, Long> readMap(InputStream in) throws IOException {
        Map<String, Long> map = new HashMap<>();

        try (BufferedReader br = new BufferedReader(new InputStreamReader(in))) {
            for (String line; (line = br.readLine()) != null; ) {
                String[] kv = line.split(":", 2);
                String key = kv[0].trim();
                String value = kv[1].trim();
                map.put(key, Long.parseLong(value));
            }
        }

        return map;
    }

    public void benchmark() throws IOException {
        while (true) {
            long start = System.nanoTime();
            readMap(new ByteArrayInputStream(input));
            long end = System.nanoTime();
            System.out.println((end - start) / 1e9);
        }
    }

    public static void main(String[] args) throws Exception {
        new MapReader(2000000).benchmark();
    }
}
