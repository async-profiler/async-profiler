/*
 * Copyright 2021 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package one.profiler.test;

import java.util.Arrays;
import java.util.regex.Pattern;
import java.util.stream.Stream;

import one.jfr.JfrReader;
import one.jfr.StackTrace;
import one.jfr.event.Event;
import one.jfr.event.EventAggregator;

public class JfrOutput {
    private final JfrReader jfr;
    private final EventAggregator executionSampleAgg;
    private final EventAggregator allocationSampleAgg;
    private final EventAggregator liveObjectAgg;
    private final EventAggregator contendedLockAgg;
    private final EventAggregator customAgg;

    public JfrOutput(
        JfrReader jfr,
        EventAggregator executionSampleAgg,
        EventAggregator allocationSampleAgg,
        EventAggregator liveObjectAgg,
        EventAggregator contendedLockAgg,
        EventAggregator customAgg
    ) {
        this.jfr = jfr;
        this.executionSampleAgg = executionSampleAgg;
        this.allocationSampleAgg = allocationSampleAgg;
        this.liveObjectAgg = liveObjectAgg;
        this.contendedLockAgg = contendedLockAgg;
        this.customAgg = customAgg;
    }

    private boolean contains(EventAggregator aggregation, String method) {
        int seen = countMethod(aggregation, method);
        if (seen == 0) {
            return false;
        }
        return true;
    }

    public boolean contains(String method) {
        return contains(executionSampleAgg, method) ||
            contains(allocationSampleAgg, method) ||
            contains(liveObjectAgg, method) ||
            contains(contendedLockAgg, method) ||
            contains(customAgg, method);
    }

    public boolean contains(String method, String eventType) {
        switch (eventType) {
            case "ExecutionSample":
                return contains(executionSampleAgg, method);
            case "AllocationSample":
                return contains(allocationSampleAgg, method);
            case "LiveObject":
                return contains(liveObjectAgg, method);
            case "ContendedLock":
                return contains(contendedLockAgg, method);
            default: //customEvent case
                return contains(customAgg, method);
        }
    }

    private String getMethodName(long method, byte type){
        return jfr.getMethodName(method, type);
    }

    private StackTrace getStackTrace(int stackTraceId){
        return jfr.stackTraces.get(stackTraceId);
    }

    private int countMethod(EventAggregator aggregation, String method) { //counts STraces containing method
        final int[] counter = {0};
        Pattern pattern = Pattern.compile(method);
        aggregation.forEach(new EventAggregator.Visitor() {
            @Override
            public void visit(Event event, long value) {
                StackTrace stackTrace = getStackTrace(event.stackTraceId);
                if (stackTrace != null) {
                    long[] methods = stackTrace.methods;
                    byte[] types = stackTrace.types;
                    for (int i = 0; i < methods.length; i++) {
                        String methodName = getMethodName(methods[i], types[i]); //here
                        if (pattern.matcher(methodName).find()) {
                            counter[0]++;
                            break;
                        }
                    }
                }
            }
        });
        return counter[0];
    }

    public int countMethod() {
        return countMethod("");
    }

    public int countMethod(String method) {
        return countMethod(executionSampleAgg, method) + countMethod(allocationSampleAgg, method) + countMethod(liveObjectAgg, method) + countMethod(contendedLockAgg, method) + countMethod(customAgg, method);
    }

    public int countMethod(String method, String eventType) {
        switch (eventType) {
            case "ExecutionSample":
                return countMethod(executionSampleAgg, method);
            case "AllocationSample":
                return countMethod(allocationSampleAgg, method);
            case "LiveObject":
                return countMethod(liveObjectAgg, method);
            case "ContendedLock":
                return countMethod(contendedLockAgg, method);
            default: //customEvent case
                return countMethod(customAgg, method);
        }
    }

    public int countSTraces() {
        return countMethod("");
    }
}
