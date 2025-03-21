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
        if (filter == null || filter.trim().isEmpty()) {
            throw new IllegalArgumentException("Filter cannot be null or empty");
        }

        String tempTag = null;
        String tempOperator = null;
        Long tempDuration = null;

        String[] parts = filter.split(",");

        for (String part : parts) {
            part = part.trim();

            // Parse tag=value
            if (part.startsWith("tag=")) {
                if (tempTag != null) {
                    throw new IllegalArgumentException("Tag parameter specified multiple times");
                }
                String[] tagPart = part.split("=");
                if (tagPart.length != 2) {
                    throw new IllegalArgumentException("Invalid tag format. Expected: tag=value");
                }
                tempTag = tagPart[1].trim();
            }
            // Parse duration(op)value
            else if (part.startsWith("duration")) {
                if (tempDuration != null) {
                    throw new IllegalArgumentException("Duration parameter specified multiple times");
                }
                String durationStr = part.substring("duration".length()).trim();

                if (durationStr.startsWith(">=")) {
                    tempOperator = ">=";
                    durationStr = durationStr.substring(2);
                } else if (durationStr.startsWith("<=")) {
                    tempOperator = "<=";
                    durationStr = durationStr.substring(2);
                } else if (durationStr.startsWith(">")) {
                    tempOperator = ">";
                    durationStr = durationStr.substring(1);
                } else if (durationStr.startsWith("<")) {
                    tempOperator = "<";
                    durationStr = durationStr.substring(1);
                } else if (durationStr.startsWith("=")) {
                    tempOperator = "=";
                    durationStr = durationStr.substring(1);
                } else {
                    throw new IllegalArgumentException("Invalid operator. Expected: >=, <=, >, <, or =");
                }

                try {
                    tempDuration = Long.parseLong(durationStr.trim());
                } catch (NumberFormatException e) {
                    throw new IllegalArgumentException("Invalid duration value. Expected a number!", e);
                }
            } else {
                throw new IllegalArgumentException("Invalid filter part. Expected: tag=value or duration(op)value");
            }
        }

        this.tag = tempTag;
        this.operator = tempOperator;
        this.duration = tempDuration;
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

