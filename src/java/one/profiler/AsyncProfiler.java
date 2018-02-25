/*
 * Copyright 2018 Andrei Pangin
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

package one.profiler;

/**
 * Java API for in-process profiling. Serves as a wrapper around
 * async-profiler native library. This class is a singleton.
 * The first call to {@link #getInstance()} initiates loading of
 * libasyncProfiler.so.
 */
public class AsyncProfiler implements AsyncProfilerMXBean {
    private static AsyncProfiler instance;

    private AsyncProfiler() {
    }

    public static AsyncProfiler getInstance() {
        return getInstance(null);
    }

    public static synchronized AsyncProfiler getInstance(String libPath) {
        if (instance != null) {
            return instance;
        }

        if (libPath == null) {
            System.loadLibrary("asyncProfiler");
        } else {
            System.load(libPath);
        }

        instance = new AsyncProfiler();
        return instance;
    }

    @Override
    public void start(String event, long interval) {
        start0(event, interval);
    }

    @Override
    public void stop() {
        stop0();
    }

    @Override
    public native long getSamples();

    @Override
    public String execute(String command) {
        return execute0(command);
    }

    @Override
    public String dumpCollapsed(Counter counter) {
        return dumpCollapsed0(counter.ordinal());
    }

    @Override
    public String dumpTraces(int maxTraces) {
        return dumpTraces0(maxTraces);
    }

    @Override
    public String dumpFlat(int maxMethods) {
        return dumpFlat0(maxMethods);
    }

    private native void start0(String event, long interval);
    private native void stop0();
    private native String execute0(String command);
    private native String dumpCollapsed0(int counter);
    private native String dumpTraces0(int maxTraces);
    private native String dumpFlat0(int maxMethods);
}
