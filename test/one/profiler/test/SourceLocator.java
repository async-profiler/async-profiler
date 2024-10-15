package one.profiler.test;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;

public class SourceLocator {
    public static String tryGetFrom(Throwable e, int ignoreFrames) {
        StackTraceElement[] stackTrace = e.getStackTrace();

        if (stackTrace.length > ignoreFrames) {
            StackTraceElement element = stackTrace[ignoreFrames];
            String fileName = element.getFileName();
            int lineNumber = element.getLineNumber();
            String className = element.getClassName();

            try {
                Class<?> clazz = Class.forName(className);
                String filePath = getFilePathFromClassLoader(fileName, clazz);

                return getSourceCodeLines(filePath, lineNumber);
            } catch (ClassNotFoundException ex) {
                return "Error: Class not found - " + className;
            }
        }
        return "No stack trace available";
    }

    private static String getFilePathFromClassLoader(String fileName, Class<?> clazz) {
        String packagePath = clazz.getPackage().getName().replace('.', File.separatorChar);

        return "test" + File.separator + packagePath + File.separator + fileName;
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
            int startLine = Math.max(1, lineNumber - 1);
            int endLine = lineNumber + 1;

            // Read lines, and append the target lines (-1, 0, +1)
            while ((line = reader.readLine()) != null) {
                if (currentLine >= startLine && currentLine <= endLine) {
                    sourceCode.append(":").append(currentLine)
                            .append(currentLine == lineNumber ? " 👉 " : "    ")
                            .append(line.trim()).append("\n");
                }
                if (currentLine > endLine) {
                    break;
                }
                currentLine++;
            }
        } catch (IOException ex) {
            return "Error reading source file: " + ex.getMessage();
        }

        return sourceCode.toString().trim();
    }
}
