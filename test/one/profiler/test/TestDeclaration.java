/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.io.File;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

public class TestDeclaration {
    private static final Logger log = Logger.getLogger(TestDeclaration.class.getName());

    private final List<String> allDirs;
    private final List<Pattern> includeGlobs;
    private final List<Pattern> skipGlobs;

    public TestDeclaration(List<String> allDirs, List<String> globs, List<String> skipGlobs) {
        this.allDirs = allDirs;
        this.includeGlobs = filterFromGlobs(globs);
        this.skipGlobs = filterFromGlobs(skipGlobs);

        log.log(Level.FINE, "Test Directories: " + allDirs);
        log.log(Level.FINE, "Test Filters: " + globs);
        log.log(Level.FINE, "Skip Filters: " + skipGlobs);
    }

    public static TestDeclaration parse(String[] args) {
        // Glob filters matching "ClassName.methodName".
        List<String> filters = Arrays.asList(args);

        List<String> skipFilters = new ArrayList<>();
        String skipProperty = System.getProperty("skip");
        if (skipProperty != null && !skipProperty.isEmpty()) {
            skipFilters.addAll(Arrays.asList(skipProperty.split(" ")));
        }

        List<String> allTestDirs = new ArrayList<>();
        File[] files = new File("test/test").listFiles();
        if (files != null) {
            for (File file : files) {
                if (file.isDirectory()) {
                    allTestDirs.add(file.getName());
                }
            }
        }

        return new TestDeclaration(allTestDirs, filters, skipFilters);
    }

    private static List<RunnableTest> getRunnableTests(String dir) {
        String className = "test." + dir + "." + Character.toUpperCase(dir.charAt(0)) + dir.substring(1) + "Tests";
        try {
            List<RunnableTest> rts = new ArrayList<>();
            for (Method m : Class.forName(className).getMethods()) {
                for (Test t : m.getAnnotationsByType(Test.class)) {
                    rts.add(new RunnableTest(m, t));
                }
            }
            return rts;
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    public List<RunnableTest> getRunnableTests() {
        return allDirs.stream()
            .flatMap(g -> getRunnableTests(g).stream())
            .sorted(Comparator.comparing(RunnableTest::testName))
            .collect(Collectors.toList());
    }

    public boolean matches(Method m) {
        String name = m.getDeclaringClass().getSimpleName() + '.' + m.getName();

        if (includeGlobs.isEmpty() || includeGlobs.stream().anyMatch(f -> f.matcher(name).matches())) {
            return skipGlobs.isEmpty() || skipGlobs.stream().noneMatch(f -> f.matcher(name).matches());
        }

        return false;
    }

    private static Pattern filterFrom(String s) {
        if (s.startsWith("*") && s.endsWith("*")) {
            // contains.
            return Pattern.compile(".*" + Pattern.quote(s.substring(1, s.length() - 1)) + ".*");
        }
        if (s.startsWith("*")) {
            // ends with.
            return Pattern.compile(".*" + Pattern.quote(s.substring(1)) + "$");
        }
        if (s.endsWith("*")) {
            // starts with.
            return Pattern.compile("^" + Pattern.quote(s.substring(0, s.length() - 1)) + ".*");
        }

        // equals
        return Pattern.compile("^" + Pattern.quote(s) + "$");
    }

    private List<Pattern> filterFromGlobs(List<String> globs) {
        Set<String> result = new HashSet<>();
        for (String g : globs) {
            if (allDirs.contains(g)) {
                // Convert test directory name to class name.
                result.add(g.substring(0, 1).toUpperCase() + g.substring(1).toLowerCase() + "Tests.*");
            } else if (g.contains(".") || g.contains("*")) {
                result.add(g);
            } else if (Character.isUpperCase(g.charAt(0))) {
                // Looks like class name.
                result.add(g + ".*");
            } else if (Character.isLowerCase(g.charAt(0))) {
                // Looks like method name.
                result.add("*." + g);
            } else {
                throw new RuntimeException("Unknown glob type: " + g);
            }
        }
        return result.stream().map(TestDeclaration::filterFrom).collect(Collectors.toList());
    }
}
