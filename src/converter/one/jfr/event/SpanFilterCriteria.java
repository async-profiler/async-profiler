/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import java.util.Arrays;
import java.util.Map;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

public class SpanFilterCriteria {
    private final String tag;
    private final String operator;
    private final Long duration;

    public SpanFilterCriteria(String filter) {
        String[] parts = filter.split(",");

        // Parse tag=value
        String[] tagPart = parts[0].split("=");
        if (tagPart.length != 2 || !tagPart[0].trim().equals("tag")) {
            throw new IllegalArgumentException("Invalid tag format. Expected: tag=value");
        }
        this.tag = tagPart[1].trim();

        // Parse duration(op)value
        String durationStr = parts[1].trim();
        if (!durationStr.startsWith("duration")) {
            throw new IllegalArgumentException("Invalid duration format. Expected: duration>=value, duration<=value, etc.");
        }

        durationStr = durationStr.substring("duration".length()).trim();

        if (durationStr.startsWith(">=")) {
            this.operator = ">=";
            durationStr = durationStr.substring(2);
        } else if (durationStr.startsWith("<=")) {
            this.operator = "<=";
            durationStr = durationStr.substring(2);
        } else if (durationStr.startsWith(">")) {
            this.operator = ">";
            durationStr = durationStr.substring(1);
        } else if (durationStr.startsWith("<")) {
            this.operator = "<";
            durationStr = durationStr.substring(1);
        } else if (durationStr.startsWith("=")) {
            this.operator = "=";
            durationStr = durationStr.substring(1);
        } else {
            throw new IllegalArgumentException("Invalid operator. Expected: >=, <=, >, <, or =");
        }

        try {
            this.duration = Long.parseLong(durationStr.trim());
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException("Invalid duration value. Expected a number!", e);
        }
    }

    public boolean matches(SpanEvent event) {
        boolean isMatch = true;

        if (tag != null && !event.tag.equals(tag)) {
            return false;
        }

        if (duration != null) {
            Long durationMs = event.duration;
            switch (operator) {
                case ">=": isMatch = durationMs >= duration; break;
                case "<=": isMatch = durationMs <= duration; break;
                case ">":  isMatch = durationMs > duration; break;
                case "<":  isMatch = durationMs < duration; break;
                case "=":  isMatch = durationMs == duration; break;
                default:   isMatch = false;
            }
        }

        return isMatch;
    }
}

