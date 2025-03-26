/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

public class SpanFilterCriteria {
    private final String tag;
    private final String operator;
    private final long duration;

    public SpanFilterCriteria(String filter) {
        validateFilter(filter);
        String[] parts = filter.split(",");

        String parsedTag = null;
        String parsedOperator = null;
        long parsedDuration = 0;

        for (String part : parts) {
            part = part.trim();
            if (part.startsWith("tag=")) {
                if (parsedTag != null) {
                    throw new IllegalArgumentException("Tag parameter specified multiple times");
                }
                parsedTag = parseTag(part);
            } else if (part.startsWith("duration")) {
                if (parsedOperator != null) {
                    throw new IllegalArgumentException("Duration parameter specified multiple times");
                }
                parsedOperator = parseOperator(part);
                parsedDuration = parseDuration(part, parsedOperator);
            } else {
                throw new IllegalArgumentException("Invalid filter part. Expected: tag=value or duration(op)value");
            }
        }

        this.tag = parsedTag;
        this.operator = parsedOperator;
        this.duration = parsedDuration;
    }

    private void validateFilter(String filter) {
        if (filter == null || filter.trim().isEmpty()) {
            throw new IllegalArgumentException("Filter cannot be null or empty");
        }
    }

    private String parseTag(String part) {
        String[] tagPart = part.split("=");
        if (tagPart.length != 2) {
            throw new IllegalArgumentException("Invalid tag format. Expected: tag=value");
        }
        return tagPart[1].trim();
    }

    private String parseOperator(String part) {
        String durationStr = part.substring("duration".length()).trim();
        if (durationStr.startsWith(">=")) return ">=";
        if (durationStr.startsWith("<=")) return "<=";
        if (durationStr.startsWith(">")) return ">";
        if (durationStr.startsWith("<")) return "<";
        if (durationStr.startsWith("=")) return "=";
        throw new IllegalArgumentException("Invalid operator. Expected: >=, <=, >, <, or =");
    }

    private Long parseDuration(String part, String operator) {
        String durationStr = part.substring("duration".length()).trim();
        String value = durationStr.substring(operator.length()).trim();

        try {
            return Long.parseLong(value);
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException("Invalid duration value. Expected a number!", e);
        }
    }

    public boolean matches(SpanEvent event) {
        if (tag != null && !event.tag.equals(tag)) {
            return false;
        }

        if (duration > 0) {
            Long durationMs = event.duration;
            switch (operator) {
                case ">=": return durationMs >= duration;
                case "<=": return durationMs <= duration;
                case ">":  return durationMs > duration;
                case "<":  return durationMs < duration;
                case "=":  return durationMs == duration;
                default:   return false;
            }
        }

        return true;
    }
}
