/* Copyright The async-profiler authors. SPDX-License-Identifier: Apache-2.0 */
package probe;
import com.sun.jna.*;
import java.io.*;
import java.lang.management.ManagementFactory;

public final class Demo {
    public interface LibC extends Library {
        long mmap(long addr, long length, int prot, int flags, int fd, long offset);
        int munmap(long addr, long length);
        int getpagesize();
    }
    private static LibC libc;
    private static long page, retained;
    private static long map(long bytes) {
        int anon = Platform.isMac() ? 0x1000 : 0x20;
        long p = libc.mmap(0, bytes, 3, 2 | anon, -1, 0);
        if (p == -1) throw new AssertionError("mmap errno=" + Native.getLastError());
        new Pointer(p).setByte(0, (byte) 42);
        if (new Pointer(p).getByte(0) != 42) throw new AssertionError("memory access");
        return p;
    }
    private static void unmap(long p, long bytes) {
        if (libc.munmap(p, bytes) != 0) throw new AssertionError("munmap errno=" + Native.getLastError());
    }
    private static void initialize() {
        libc = Native.load(Platform.C_LIBRARY_NAME, LibC.class);
        page = libc.getpagesize();
        long warmup = map(page);
        unmap(warmup, page);
        System.out.println("WARMED JNA=" + Native.VERSION + " page=" + page);
    }
    public static void main(String[] args) throws Exception {
        boolean cold = args.length > 0 && args[0].equals("cold");
        if (!cold) initialize();
        System.out.println("READY pid=" + ManagementFactory.getRuntimeMXBean().getName().split("@")[0]);
        BufferedReader in = new BufferedReader(new InputStreamReader(System.in));
        if (!"go".equals(in.readLine())) throw new AssertionError("expected go");
        if (cold) initialize();
        long p = map(page); unmap(p, page);
        long q = map(page * 3);
        unmap(q + page, page); unmap(q, page); unmap(q + page * 2, page);
        int anon = Platform.isMac() ? 0x1000 : 0x20;
        if (libc.mmap(0, 0, 3, 2 | anon, -1, 0) != -1) throw new AssertionError("mmap should fail");
        int mmapErrno = Native.getLastError();
        if (libc.munmap(1, page) != -1) throw new AssertionError("munmap should fail");
        int unmapErrno = Native.getLastError();
        if (mmapErrno != 22 || unmapErrno != 22) throw new AssertionError("errno not preserved");
        retained = map(page);
        System.out.println("DONE retained=0x" + Long.toHexString(retained) + " bytes=" + page
            + " mmapErrno=" + mmapErrno + " munmapErrno=" + unmapErrno);
        if (!"cleanup".equals(in.readLine())) throw new AssertionError("expected cleanup");
        unmap(retained, page);
        long afterStop = map(page); unmap(afterStop, page);
        System.out.println("CLEANED");
    }
}
