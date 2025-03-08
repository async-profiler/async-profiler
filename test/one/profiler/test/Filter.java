/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

public abstract class Filter {
    protected final String filter;

    protected Filter(String filter) {
        this.filter = filter;
    }

    public static Filter from(String s) {
        if (s.startsWith("*") && s.endsWith("*")) {
            return new ContainsFilter(s.substring(1, s.length() - 1));
        } else if (s.startsWith("*")) {
            return new EndsWithFilter(s.substring(1));
        } else if (s.endsWith("*")) {
            return new StartsWithFilter(s.substring(0, s.length() - 1));
        } else {
            return new EqualsFilter(s);
        }
    }

    public abstract boolean isMatch(String s);

    @Override
    public String toString() {
        return getClass().getSimpleName() + "(" + filter + ")";
    }
}

class StartsWithFilter extends Filter {
    public StartsWithFilter(String filter) {
        super(filter);
    }

    @Override
    public boolean isMatch(String s) {
        return s.startsWith(filter);
    }
}

class EndsWithFilter extends Filter {
    public EndsWithFilter(String filter) {
        super(filter);
    }

    @Override
    public boolean isMatch(String s) {
        return s.endsWith(filter);
    }
}

class ContainsFilter extends Filter {
    public ContainsFilter(String filter) {
        super(filter);
    }

    @Override
    public boolean isMatch(String s) {
        return s.contains(filter);
    }
}

class EqualsFilter extends Filter {
    public EqualsFilter(String filter) {
        super(filter);
    }

    @Override
    public boolean isMatch(String s) {
        return s.equals(filter);
    }
}