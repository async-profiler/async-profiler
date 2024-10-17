/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;

public class SourceCode {
    public static String tryGet(int ignoreFrames) {
        return tryGet(new Exception(), ignoreFrames + 1);
    }

    public static String tryGet(Throwable e, int ignoreFrames) {
        StackTraceElement[] stackTrace = e.getStackTrace();

        if (stackTrace.length > ignoreFrames) {
            StackTraceElement element = stackTrace[ignoreFrames];
            String className = element.getClassName();
            String filePath = getFilePath(className);

            return getSourceCodeAt(filePath, element.getLineNumber());
        }
        return "No stack trace available";
    }

    private static String getFilePath(String className) {
        int dollar = className.lastIndexOf('$');
        if (dollar >= 0) {
            className = className.substring(0, dollar);
        }
        return "test/" + className.replace('.', '/') + ".java";
    }

    private static String getSourceCodeAt(String filePath, int lineNumber) {
        String result = "\t>  " + filePath + ":" + lineNumber;

        try (BufferedReader reader = new BufferedReader(new FileReader(filePath))) {
            String line;
            int currentLine = 1;

            while ((line = reader.readLine()) != null) {
                if (currentLine == lineNumber) {
                    return result + "\n\t>  " + line.trim();
                }
                currentLine++;
            }
        } catch (IOException ex) {
            return "Error reading source file: " + ex.getMessage();
        }

        return result;
    }
}
