/* Copyright The async-profiler authors. SPDX-License-Identifier: Apache-2.0 */
package probe;
import com.sun.jna.*;
import java.io.*;
public class ChunkMappings {
    public static void main(String[] args) throws Exception {
        Demo.LibC c = Native.load(Platform.C_LIBRARY_NAME, Demo.LibC.class);
        long page = c.getpagesize();
        int flags = 2 | (Platform.isMac() ? 0x1000 : 0x20);
        long warm = c.mmap(0, page, 3, flags, -1, 0);
        if (warm == -1 || c.munmap(warm, page) != 0) throw new AssertionError();
        BufferedReader in = new BufferedReader(new InputStreamReader(System.in));
        System.out.println("READY");
        if (!"go".equals(in.readLine())) throw new AssertionError();
        long p = c.mmap(0, 3 * page, 3, flags, -1, 0);
        if (p == -1) throw new AssertionError();
        Thread.sleep(7000); // --chunktime 5 must split allocation and partial unmap
        if (c.munmap(p + page, page) != 0) throw new AssertionError();
        System.out.println("DONE retained=0x" + Long.toHexString(p) + " bytes=" + page * 2 + " mapped=" + page * 3);
        if (!"cleanup".equals(in.readLine()) || c.munmap(p, page) != 0 || c.munmap(p + page * 2, page) != 0)
            throw new AssertionError();
        System.out.println("CLEANED");
    }
}
