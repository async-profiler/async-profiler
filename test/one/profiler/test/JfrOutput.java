/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.regex.Pattern;

import one.jfr.JfrReader;
import one.jfr.StackTrace;
import one.jfr.event.Event;

public class JfrOutput {
    private final JfrReader jfr;
    private final List<Event> eventList = new ArrayList<>();
    final double nanosToTicks;

    public JfrOutput(
            JfrReader jfr
    ) throws IOException {
        this.jfr = jfr;
        this.nanosToTicks = jfr.ticksPerSec / 1e9;
        for (Event event; (event = jfr.readEvent(null)) != null; ) {
            eventList.add(event);
        }
        Collections.sort(eventList);
    }

    public boolean contains(String method) {
        return countMethod(method) > 0;
    }

    private String getMethodName(long method, byte type) {
        return jfr.getMethodName(method, type);
    }

    private StackTrace getStackTrace(int stackTraceId) {
        return jfr.stackTraces.get(stackTraceId);
    }

    private List<Event> filterByTime(int low, int high) {
        low = (int) (low * nanosToTicks);
        high = (int) (high * nanosToTicks);
        int left = 0;
        int right = eventList.size() - 1;
        int result = -1;

        // Binary search for a time within the bounds
        while (left <= right) {
            int mid = left + (right - left) / 2;
            if (eventList.get(mid).time >= low && eventList.get(mid).time <= high) {
                result = mid;
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }

        // If no events found, return null
        // Else iterate on either side of the discovered event to find all events within the bounds
        if (result == -1) {
            return null;
        } else {
            int i = result;
            int j = result +1;
            for (; i >= 0; i--) {
                if (eventList.get(i).time < low) {
                    break;
                }
            }
            for (; j < eventList.size(); j++) {
                if (eventList.get(j).time > high) {
                    break;
                }
            }
            return eventList.subList(i + 1, j);
        }
    }

    private int countMethod(String method) {
        return countMethod(method, eventList);
    }

    private int countMethod(String method, int low, int high) {
        List<Event> filteredList = filterByTime(low, high);
        return countMethod(method, filteredList);
    }
    private int countMethod(String method, List<Event> eventList) { //counts STraces containing method
        Pattern pattern = Pattern.compile(method);
        int appearances = 0;

        for (Event event : eventList) {
            StackTrace stackTrace = getStackTrace(event.stackTraceId);
            if (stackTrace != null) {
                long[] methods = stackTrace.methods;
                byte[] types = stackTrace.types;
                for (int i = 0; i < methods.length; i++) {
                    String methodName = getMethodName(methods[i], types[i]); //here
                    if (pattern.matcher(methodName).find()) {
                        appearances++;
                        break;
                    }
                }
            }
        }

        return appearances;
    }

    private int countEvents() {
        return eventList.size();
    }

    public double ratio(String method, int low, int high) {
        return (double) countMethod(method, low, high) / countEvents();
    }
}
