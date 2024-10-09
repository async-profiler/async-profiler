/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfr;

import java.util.Comparator;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

public class Cache {
    private final Map<Long, ValueWithTime> map = new ConcurrentHashMap<>();
    private final ScheduledExecutorService executor = Executors.newSingleThreadScheduledExecutor();
    private volatile ValueWithTime[] top;

    public Cache() {
        executor.scheduleAtFixedRate(this::calculateTop, 4, 4, TimeUnit.SECONDS);
    }

    public void put(Long key, String value) {
        ValueWithTime vt = new ValueWithTime(value, System.currentTimeMillis());
        map.put(key, vt);
    }

    public String get(Long key) {
        ValueWithTime vt = map.get(key);
        if (vt == null) {
            return null;
        }
        vt.time = System.currentTimeMillis();
        return vt.value;
    }

    private void calculateTop() {
        long deadline = System.currentTimeMillis() - 1000;

        ValueWithTime[] top = map.values()
                .parallelStream()
                .filter(vt -> vt.time > deadline)
                .sorted(Comparator.comparing(vt -> Long.parseLong(vt.value)))
                .limit(10000)
                .toArray(ValueWithTime[]::new);

        this.top = top;
    }

    public ValueWithTime[] getTop() {
        return top;
    }

    private static class ValueWithTime {
        private final String value;
        private volatile long time;

        ValueWithTime(String value, long time) {
            this.value = value;
            this.time = time;
        }
    }
}
