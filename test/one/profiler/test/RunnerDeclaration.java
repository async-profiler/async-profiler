/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.lang.reflect.Method;
import java.util.List;

public class RunnerDeclaration {
    private final List<String> directories;
    private final List<String> classNames;
    private final List<String> filters;

    public RunnerDeclaration(List<String> directories, List<String> classNames, List<String> filters) {
        this.directories = directories;
        this.classNames = classNames;
        this.filters = filters;
    }

    public List<String> classNames() {
        return classNames;
    }

    public List<String> directories() {
        return directories;
    }

    public List<String> filters() {
        return filters;
    }

    public boolean isFilterMatch(Method m) {
        String fullNameLower = (m.getDeclaringClass().getName() + '.' + m.getName()).toLowerCase();
        return filters().isEmpty() || filters().stream().anyMatch(f -> fullNameLower.contains(f.toLowerCase()));
    }
}
