/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.recovery;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.Base64;
import java.util.Random;
import java.util.zip.Adler32;
import java.util.zip.CRC32;
import java.util.zip.Checksum;

/**
 * This test runs popular checksum and digest algorithms
 * that have corresponding compiler intrinsics in HotSpot JVM.
 * These intrinsics do not maintain common frame layout
 * and thus are tricky for stack unwinding.
 */
public class CodingIntrinsics {
    static volatile long sink;

    public static void main(String[] args) {
        byte[][] arrays = new byte[1025][];

        Random random = new Random(123);
        for (int i = 0; i < arrays.length; i++) {
            arrays[i] = new byte[i];
            random.nextBytes(arrays[i]);
        }

        for (int i = 0; i < arrays.length; i++) {
            runTest(arrays[i]);
        }
    }

    static void runTest(byte[] input) {
        for (Codec codec : CODECS) {
            sink = runTestWithCodec(codec, input, 10_000);
        }
    }

    static long runTestWithCodec(Codec codec, byte[] input, int count) {
        long n = 0;
        for (int i = 0; i < count; i++) {
            byte[] output = codec.encode(input);
            n += output.length;
            if (output.length <= 16) {
                n += Arrays.hashCode(output);
            }
        }
        return n;
    }

    interface Codec {
        byte[] encode(byte[] input);
    }

    static final Codec[] CODECS = new Codec[]{
            input -> Base64.getEncoder().encode(input),
            input -> checksum(input, new CRC32()),
            input -> checksum(input, new Adler32()),
            input -> digest(input, "MD5"),
            input -> digest(input, "SHA-1"),
            // async-profiler cannot easily unwind sha256_implCompress intrinsic
            // on x86 machines that support AVX2 but not SHA instruction set.
            isArm64() ? input -> digest(input, "SHA-256") : input -> input
    };

    static boolean isArm64() {
        String arch = System.getProperty("os.arch").toLowerCase();
        return arch.equals("aarch64") || arch.contains("arm64");
    }

    static byte[] checksum(byte[] input, Checksum checksum) {
        checksum.update(input, 0, input.length);
        long value = checksum.getValue();
        return new byte[]{
                (byte) (value >>> 24),
                (byte) (value >>> 16),
                (byte) (value >>> 8),
                (byte) value
        };
    }

    static byte[] digest(byte[] input, String algorithm) {
        try {
            MessageDigest md = MessageDigest.getInstance(algorithm);
            return md.digest(input);
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }
    }
}
