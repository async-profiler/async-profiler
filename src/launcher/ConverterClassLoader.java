/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

import java.io.*;
import java.util.jar.*;
import java.util.zip.*;

public class ConverterClassLoader extends ClassLoader {
    private byte[] jarData;
    
    public ConverterClassLoader(byte[] jarData) {
        this.jarData = jarData;
    }
    
    @Override
    protected Class<?> findClass(String name) throws ClassNotFoundException {
        try {
            byte[] classData = getClassData(name);
            if (classData != null) {
                return defineClass(name, classData, 0, classData.length);
            }
        } catch (IOException e) {
            throw new ClassNotFoundException("Failed to load class: " + name, e);
        }
        throw new ClassNotFoundException(name);
    }
    
    private byte[] getClassData(String className) throws IOException {
        String classPath = className.replace('.', '/') + ".class";
        
        try (ByteArrayInputStream bis = new ByteArrayInputStream(jarData);
             ZipInputStream zis = new ZipInputStream(bis)) {
            
            ZipEntry entry;
            while ((entry = zis.getNextEntry()) != null) {
                if (entry.getName().equals(classPath)) {
                    ByteArrayOutputStream baos = new ByteArrayOutputStream();
                    byte[] buffer = new byte[1024];
                    int len;
                    while ((len = zis.read(buffer)) > 0) {
                        baos.write(buffer, 0, len);
                    }
                    return baos.toByteArray();
                }
            }
        }
        return null;
    }
}