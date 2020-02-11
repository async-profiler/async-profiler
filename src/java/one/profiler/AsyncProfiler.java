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

import java.io.IOException;

/**
 * Java API for in-process profiling. Serves as a wrapper around
 * async-profiler native library. This class is a singleton.
 * The first call to {@link #getInstance()} initiates loading of
 * libasyncProfiler.so.
 */
public class AsyncProfiler implements AsyncProfilerMXBean {
    private static AsyncProfiler instance;

    private final String version;

    private AsyncProfiler() {
        this.version = version0();
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

    /**
     * Start profiling
     *
     * @param event Profiling event, see {@link Events}
     * @param interval Sampling interval, e.g. nanoseconds for Events.CPU
     * @throws IllegalStateException If profiler is already running
     */
    @Override
    public void start(String event, long interval) throws IllegalStateException {
        start0(event, interval, true);
    }

    /**
     * Start or resume profiling without resetting collected data.
     * Note that event and interval may change since the previous profiling session.
     *
     * @param event Profiling event, see {@link Events}
     * @param interval Sampling interval, e.g. nanoseconds for Events.CPU
     * @throws IllegalStateException If profiler is already running
     */
    @Override
    public void resume(String event, long interval) throws IllegalStateException {
        start0(event, interval, false);
    }

    /**
     * Stop profiling (without dumping results)
     *
     * @throws IllegalStateException If profiler is not running
     */
    @Override
    public void stop() throws IllegalStateException {
        stop0();
    }

    /**
     * Get the number of samples collected during the profiling session
     *
     * @return Number of samples
     */
    @Override
    public native long getSamples();

    /**
     * Get profiler agent version, e.g. "1.0"
     *
     * @return Version string
     */
    @Override
    public String getVersion() {
        return version;
    }

    /**
     * Execute an agent-compatible profiling command -
     * the comma-separated list of arguments described in arguments.cpp
     *
     * @param command Profiling command
     * @return The command result
     * @throws IllegalArgumentException If failed to parse the command
     * @throws IOException If failed to create output file
     */
    @Override
    public String execute(String command) throws IllegalArgumentException, IOException {
        return execute0(command);
    }

    /**
     * Dump profile in 'collapsed stacktraces' format
     *
     * @param counter Which counter to display in the output
     * @return Textual representation of the profile
     */
    @Override
    public String dumpCollapsed(Counter counter) {
        return dumpCollapsed0(counter.ordinal());
    }

    /**
     * Dump collected stack traces
     *
     * @param maxTraces Maximum number of stack traces to dump. 0 means no limit
     * @return Textual representation of the profile
     */
    @Override
    public String dumpTraces(int maxTraces) {
        return dumpTraces0(maxTraces);
    }

    /**
     * Dump flat profile, i.e. the histogram of the hottest methods
     *
     * @param maxMethods Maximum number of methods to dump. 0 means no limit
     * @return Textual representation of the profile
     */
    @Override
    public String dumpFlat(int maxMethods) {
        return dumpFlat0(maxMethods);
    }

    /**
     * Get OS thread ID of the current Java thread. On Linux, this is the same number
     * as gettid() returns. The result ID matches 'tid' in the profiler output.
     *
     * @return 64-bit integer that matches native (OS level) thread ID
     */
    public long getNativeThreadId() {
        return getNativeThreadId0();
    }

    /**
     * Add or remove the given thread to the set of profiled threads
     *
     * @param thread A thread to add or remove; null means current thread
     * @param enable true to enable profiling of the given thread, or
     *               false to disable profiling
     * @throws IllegalStateException If thread has not yet started or has already finished
     */
    public void filterThread(Thread thread, boolean enable) throws IllegalStateException {
        if (thread == null) {
            filterThread0(null, enable);
        } else {
            synchronized (thread) {
                Thread.State state = thread.getState();
                if (state == Thread.State.NEW || state == Thread.State.TERMINATED) {
                    throw new IllegalStateException("Thread must be running");
                }
                filterThread0(thread, enable);
            }
        }
    }

    private native void start0(String event, long interval, boolean reset) throws IllegalStateException;
    private native void stop0() throws IllegalStateException;
    private native String execute0(String command) throws IllegalArgumentException, IOException;
    private native String dumpCollapsed0(int counter);
    private native String dumpTraces0(int maxTraces);
    private native String dumpFlat0(int maxMethods);
    private native String version0();
    private native long getNativeThreadId0();
    private native void filterThread0(Thread thread, boolean enable);
}
