/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.alloc;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.HashMap;
import java.util.Map;

public class MapReaderOpt extends MapReader {

    public MapReaderOpt(int size) throws IOException {
        super(size);
    }

    @Override
    public Map<String, Long> readMap(InputStream in) throws IOException {
        Map<String, Long> map = new HashMap<>();

        try (BufferedReader br = new BufferedReader(new InputStreamReader(in))) {
            for (String line; (line = br.readLine()) != null; ) {
                int sep = line.indexOf(':');
                String key = trim(line, 0, sep);
                String value = trim(line, sep + 1, line.length());
                map.put(key, Long.parseLong(value));
            }
        }

        return map;
    }

    private static String trim(String line, int from, int to) {
        while (from < to && line.charAt(from) <= ' ') {
            from++;
        }
        while (to > from && line.charAt(to - 1) <= ' ') {
            to--;
        }
        return line.substring(from, to);
    }

    public static void main(String[] args) throws Exception {
        new MapReaderOpt(2000000).benchmark();
    }
}
