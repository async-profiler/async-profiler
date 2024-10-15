/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package validator;

import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.LineNumberReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Java class to validate copyright header presence in all source and test files.
 */
public class CopyrightValidator {

    private static final String expectedAsyncProfilerCopyrightHeader = "/*\n" +
                                " * Copyright The async-profiler authors\n" +
                                " * SPDX-License-Identifier: Apache-2.0\n" +
                                " */\n";

    private static final String expectedJattachCopyrightHeader = "/*\n" +
                                " * Copyright The jattach authors\n" +
                                " * SPDX-License-Identifier: Apache-2.0\n" +
                                " */\n";

    private static final Set<String> fileExtensionsToCheck = new HashSet<>(Arrays.asList("c", "h", "cpp", "java"));

    public static void main(String[] args) throws IOException {
        String[] dirsToCheck = new String[] {"src", "test"};
        for (String dirToCheck : dirsToCheck) {
            Path dir = Paths.get(dirToCheck);
            Files.walk(dir).forEach(path -> {
                try {
                    verifyFile(path.toFile());
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            });
        }
    }

    private static void verifyFile(File file) throws IOException {
        Path filePath = file.toPath();
        if (!file.isFile()) {
            return;
        }

        String fileName = String.valueOf(filePath.getFileName());
        String fileExtension = fileName.substring(fileName.lastIndexOf('.') + 1);
        if (!fileExtensionsToCheck.contains(fileExtension)) {
            return;
        }

        StringBuilder copyrightBuilder = new StringBuilder();
        try (LineNumberReader reader = new LineNumberReader
                (new InputStreamReader(Files.newInputStream(filePath), StandardCharsets.UTF_8))) {
            String line;
            while (((line = reader.readLine()) != null) && reader.getLineNumber() <= 4) {
                copyrightBuilder.append(line).append("\n");
            }
        }

        CopyrightClassification copyrightClassification = String.valueOf(filePath.toAbsolutePath()).contains("jattach") ?
                CopyrightClassification.JATTACH : CopyrightClassification.ASYNC_PROFILER;
        validate(copyrightClassification, copyrightBuilder, filePath);
    }

    private static void validate(
            CopyrightClassification copyrightClassification,
            StringBuilder copyrightBuilder,
            Path filePath) {
        String expectedHeader = copyrightClassification.equals(CopyrightClassification.ASYNC_PROFILER) ?
                expectedAsyncProfilerCopyrightHeader : expectedJattachCopyrightHeader;
        if (!copyrightBuilder.toString().equals(expectedHeader)) {
            throw new RuntimeException(
                    String.format(
                            "Either %s copyright header is missing or is not in the expected format for file %s!",
                            copyrightClassification, filePath));
        }
    }

    private enum CopyrightClassification {
        ASYNC_PROFILER,
        JATTACH
    }
}