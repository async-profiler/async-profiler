/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.lang.reflect.Method;
import java.util.List;
import java.util.stream.Collectors;
import java.util.logging.Level;
import java.util.logging.Logger;

public class RunnerDeclaration {
    private static final Logger log = Logger.getLogger(RunnerDeclaration.class.getName());

    private final List<String> directories;
    private final List<String> classNames;
    private final List<Filter> filters;

    public RunnerDeclaration(List<String> directories, List<String> classNames, List<String> filters) {
        this.directories = directories;
        this.classNames = classNames;
        this.filters = filters.stream().map(Filter::from).collect(Collectors.toList());
    }

    public List<String> classNames() {
        return classNames;
    }

    public List<String> directories() {
        return directories;
    }

    private List<Filter> filters() {
        return filters;
    }

    public boolean isFilterMatch(Method m) {
        String name = (m.getDeclaringClass().getSimpleName() + '.' + m.getName());

        if (!filters().isEmpty()) {
            log.log(Level.FINE, "Filter test name " + name + " with: " + filters());
        }

        return filters().isEmpty() || filters().stream().anyMatch(f -> f.isMatch(name));
    }
}
