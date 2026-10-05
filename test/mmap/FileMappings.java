/* Copyright The async-profiler authors. SPDX-License-Identifier: Apache-2.0 */
package probe;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.lang.reflect.Field;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import sun.misc.Unsafe;

public class FileMappings {
    static final Unsafe UNSAFE;
    static {
        try {
            Field field = Unsafe.class.getDeclaredField("theUnsafe");
            field.setAccessible(true);
            UNSAFE = (Unsafe) field.get(null);
        } catch (Exception e) { throw new ExceptionInInitializerError(e); }
    }
    static MappedByteBuffer retained;

    static MappedByteBuffer mapFile(FileChannel file, long offset, long size) throws Exception {
        return file.map(FileChannel.MapMode.READ_WRITE, offset, size);
    }

    public static void main(String[] args) throws Exception {
        BufferedReader input = new BufferedReader(new InputStreamReader(System.in));
        Path path = Files.createTempFile("asprof-mmap-", ".dat");
        long page = UNSAFE.pageSize();
        try (FileChannel file = FileChannel.open(path, StandardOpenOption.READ, StandardOpenOption.WRITE)) {
            System.out.println("READY");
            input.readLine();
            // Offset adds one extra native page, so the native area is 3 pages.
            MappedByteBuffer released = mapFile(file, 1, page * 2);
            retained = mapFile(file, page * 8, page);
            Thread cleaner = new Thread(() -> UNSAFE.invokeCleaner(released), "other-thread-unmap");
            cleaner.start();
            cleaner.join();
            // Exercise address reuse without relying on the OS choosing a particular address.
            for (int i = 0; i < 100; i++) {
                MappedByteBuffer transientMapping = mapFile(file, 0, page);
                UNSAFE.invokeCleaner(transientMapping);
            }
            System.out.println("DONE bytes=" + page);
            input.readLine();
            UNSAFE.invokeCleaner(retained);
            System.out.println("CLEANED");
            input.readLine();
        } finally { Files.deleteIfExists(path); }
    }
}
