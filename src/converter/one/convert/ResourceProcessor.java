/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintStream;

public class ResourceProcessor {

    public static String getResource(String name) {
        try (InputStream stream = ResourceProcessor.class.getResourceAsStream(name)) {
            if (stream == null) {
                throw new IOException("No resource found");
            }

            ByteArrayOutputStream result = new ByteArrayOutputStream();
            byte[] buffer = new byte[32768];
            for (int length; (length = stream.read(buffer)) != -1; ) {
                result.write(buffer, 0, length);
            }
            return result.toString("UTF-8");
        } catch (IOException e) {
            throw new IllegalStateException("Can't load resource with name " + name);
        }
    }

    public static String printTill(PrintStream out, String data, String till) {
        int index = data.indexOf(till);
        out.print(data.substring(0, index));
        return data.substring(index + till.length());
    }

}
