/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.lang.reflect.Method;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

public class RunnerDeclaration {
    private static final Logger log = Logger.getLogger(RunnerDeclaration.class.getName());

    private final Set<String> includeDirs;
    private final List<Pattern> filters;
    private final Set<String> looksLikeClassName;
    private final Set<String> looksLikeMethodName;

    private final List<Pattern> skipFilters;
    private final Set<String> skipLooksLikeClassName;
    private final Set<String> skipLooksLikeMethodName;

    public RunnerDeclaration(List<String> includeDirs, List<String> globs, List<String> skipGlobs) {
        this.includeDirs = new HashSet<>(includeDirs);
        this.filters = globs.stream().map(Filter::from).collect(Collectors.toList());
        this.looksLikeClassName = globs.stream().filter(f -> !f.contains(".") && !f.contains("*") && Character.isUpperCase(f.charAt(0))).collect(Collectors.toSet());
        this.looksLikeMethodName = globs.stream().filter(f -> !f.contains(".") && !f.contains("*") && Character.isLowerCase(f.charAt(0))).collect(Collectors.toSet());

        this.skipFilters = skipGlobs.stream().map(Filter::from).collect(Collectors.toList());
        this.skipLooksLikeClassName = skipGlobs.stream().filter(f -> !f.contains(".") && !f.contains("*") && Character.isUpperCase(f.charAt(0))).collect(Collectors.toSet());
        this.skipLooksLikeMethodName = skipGlobs.stream().filter(f -> !f.contains(".") && !f.contains("*") && Character.isLowerCase(f.charAt(0))).collect(Collectors.toSet());
    }

    public Set<String> includeDirs() {
        return includeDirs;
    }

    private List<Pattern> filters() {
        return filters;
    }

    public boolean matches(Method m) {
        String className = m.getDeclaringClass().getSimpleName();
        String methodName = m.getName();
        String name = className + '.' + methodName;

        if (!includeDirs.isEmpty()) {
            String packageName = m.getDeclaringClass().getPackage().getName();
            if (packageName.startsWith("test.") && includeDirs.contains(packageName.substring(5))) {
                // exact dir match, include all tests regardless of other filters.
                return !skip(m);
            }

            if (filters.isEmpty()) {
                // only directory match.
                return false;
            }
        }

        if (filters().isEmpty()) {
            return !skip(m);
        }

        log.log(Level.FINE, "Filter test name " + name + " with: " + filters());

        boolean isMatch = filters().stream().anyMatch(f -> f.matcher(name).matches());
        if (isMatch) {
            return !skip(m);
        }

        // Be "smart", try to match class name.
        if (this.looksLikeClassName.contains(className)) {
            return !skip(m);
        }

        // Be "smarter", try to match method name.
        if (this.looksLikeMethodName.contains(methodName)) {
            return !skip(m);
        }

        return false;
    }

    private boolean skip(Method m) {
        if (skipFilters.isEmpty()) {
            return false;
        }

        String className = m.getDeclaringClass().getSimpleName();
        String methodName = m.getName();
        String name = className + '.' + methodName;

        log.log(Level.FINE, "Skip test name " + name + " with: " + skipFilters);

        boolean isMatch = skipFilters.stream().anyMatch(f -> f.matcher(name).matches());
        if (isMatch) {
            return true;
        }

        // Be "smart", try to match class name.
        if (this.skipLooksLikeClassName.contains(className)) {
            return true;
        }

        // Be "smarter", try to match method name.
        if (this.skipLooksLikeMethodName.contains(methodName)) {
            return true;
        }

        return false;
    }
}
