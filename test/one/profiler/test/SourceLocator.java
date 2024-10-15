/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;

public class SourceLocator {
    public static String tryGetFrom(Throwable e, int ignoreFrames) {
        StackTraceElement[] stackTrace = e.getStackTrace();

        if (stackTrace.length > ignoreFrames) {
            StackTraceElement element = stackTrace[ignoreFrames];
            String className = element.getClassName();
            String filePath = getFilePathFromClassLoader(className);

            return getSourceCodeLines(filePath, element.getLineNumber());
        }
        return "No stack trace available";
    }

    private static String getFilePathFromClassLoader(String className) {
        int dollar = className.lastIndexOf("$");
        if (dollar >= 0) {
            className = className.substring(0, dollar);
        }
        return "test/" + className.replace('.', '/') + ".java";
    }

    private static String getSourceCodeLines(String filePath, int lineNumber) {
        StringBuilder sourceCode = new StringBuilder();
        sourceCode.append("at ");
        sourceCode.append(filePath);
        sourceCode.append(":");
        sourceCode.append(lineNumber);
        sourceCode.append("\n");

        try (BufferedReader reader = new BufferedReader(new FileReader(filePath))) {
            String line;
            int currentLine = 1;

            while ((line = reader.readLine()) != null) {
                if (currentLine == lineNumber) {
                    sourceCode.append(" 👉 ").append(line.trim());
                    break;
                }
                currentLine++;
            }
        } catch (IOException ex) {
            return "Error reading source file: " + ex.getMessage();
        }

        return sourceCode.toString();
    }
}
