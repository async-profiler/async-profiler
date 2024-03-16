/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;
import java.util.zip.DataFormatException;
import java.util.zip.Inflater;

/**
 * Loads classes from a JAR file embedded in a shared library.
 */
public class EmbeddedClassLoader extends ClassLoader {
    private final ByteBuffer jar;
    private final Map<String, int[]> directory;

    public EmbeddedClassLoader(ClassLoader parent, ByteBuffer jar) {
        super(parent);
        this.jar = jar;
        this.directory = new HashMap<>();

        String specVersion = System.getProperty("java.specification.version");
        int currentVersion = specVersion == null || specVersion.startsWith("1.") ? 8 : Integer.parseInt(specVersion);

        jar.order(ByteOrder.LITTLE_ENDIAN);
        int eocd = jar.limit() - 22;
        if (jar.getInt(eocd) != 0x06054b50) {
            throw new IllegalStateException("EOCD signature not found");
        }

        int chdr = jar.getInt(eocd + 16);
        while (jar.getInt(chdr) == 0x02014b50) {
            int compressedSize = jar.getInt(chdr + 20);
            int uncompressedSize = jar.getInt(chdr + 24);
            int fileNameLength = jar.getShort(chdr + 28) & 0xffff;
            int extraLength = jar.getInt(chdr + 30);
            int fileHeaderStart = jar.getInt(chdr + 42);

            byte[] fileNameBytes = new byte[fileNameLength];
            ((Buffer) jar).position(chdr + 46);
            jar.get(fileNameBytes);

            String fileName = new String(fileNameBytes, StandardCharsets.UTF_8);
            if (!fileName.endsWith("/")) {
                int[] entry = {fileHeaderStart, compressedSize, uncompressedSize};
                if (fileName.startsWith("META-INF/versions/")) {
                    int p = fileName.indexOf('/', 18);
                    if (p > 0 && Integer.parseInt(fileName.substring(18, p)) <= currentVersion) {
                        directory.put(fileName.substring(p + 1), entry);
                    }
                } else {
                    directory.putIfAbsent(fileName, entry);
                }
            }

            chdr += 46 + fileNameLength + (extraLength & 0xffff) + (extraLength >>> 16);
        }
    }

    @Override
    protected Class<?> findClass(String name) throws ClassNotFoundException {
        byte[] data = unzip(name.replace('.', '/').concat(".class"));
        if (data == null) {
            throw new ClassNotFoundException(name);
        }
        return defineClass(name, data, 0, data.length);
    }

    @Override
    public InputStream getResourceAsStream(String name) {
        byte[] data = unzip(name.startsWith("/") ? name.substring(1) : name);
        if (data == null) {
            return super.getResourceAsStream(name);
        }
        return new ByteArrayInputStream(data);
    }

    private byte[] unzip(String name) {
        int[] entry = directory.get(name);
        if (entry == null) {
            return null;
        }

        int loc = entry[0];
        if (jar.getInt(loc) != 0x04034b50) {
            throw new IllegalStateException("LOC signature not found");
        }

        byte[] compressed = new byte[entry[1]];
        int extraLength = jar.getInt(loc + 26);
        ByteBuffer jarCopy = jar.duplicate();
        ((Buffer) jarCopy).position(loc + 30 + (extraLength & 0xffff) + (extraLength >>> 16));
        jarCopy.get(compressed);

        short method = jar.getShort(loc + 8);
        if (method == 0) {
            return compressed;
        } else if (method != 8) {
            throw new IllegalStateException("Unsupported compression algorithm");
        }

        byte[] uncompressed = new byte[entry[2]];
        Inflater inf = new Inflater(true);
        try {
            inf.setInput(compressed);
            if (inf.inflate(uncompressed) != uncompressed.length) {
                throw new IllegalStateException("Uncompressed size mismatch");
            }
            return uncompressed;
        } catch (DataFormatException e) {
            throw new IllegalStateException("Invalid compressed data");
        } finally {
            inf.end();
        }
    }

    public static Class<?> loadMainClass(ByteBuffer jar) throws ClassNotFoundException {
        EmbeddedClassLoader loader = new EmbeddedClassLoader(EmbeddedClassLoader.class.getClassLoader(), jar);
        byte[] manifest = loader.unzip("META-INF/MANIFEST.MF");
        if (manifest == null) {
            throw new IllegalStateException("MANIFEST.MF not found");
        }

        String s = new String(manifest, StandardCharsets.UTF_8);
        int p = s.indexOf("Main-Class:");
        if (p < 0) {
            throw new IllegalStateException("Main-Class attribute not found");
        }

        int q = s.indexOf('\n', p += 11);
        String mainClass = (q >= 0 ? s.substring(p, q) : s.substring(p)).trim();
        return loader.findClass(mainClass);
    }
}
