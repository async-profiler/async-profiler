/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import one.jfr.JfrReader;

public class ProcessSample extends Event {
    public final int pid;
    public final int ppid;
    public final String name;
    public final String cmdLine;
    public final int uid;
    public final byte state;
    public final long processStartTime;
    public final float cpuUser;
    public final float cpuSystem;
    public final float cpuPercent;
    public final int threads;
    public final long vmSize;
    public final long vmRss;
    public final long rssAnon;
    public final long rssFiles;
    public final long rssShmem;
    public final long minorFaults;
    public final long majorFaults;
    public final long ioRead;
    public final long ioWrite;

    public ProcessSample(JfrReader jfr) {
        super(jfr.getVarlong(), 0, 0);
        this.pid = jfr.getVarint();
        this.ppid = jfr.getVarint();
        this.name = jfr.getString();
        this.cmdLine = jfr.getString();
        this.uid = jfr.getVarint();
        this.state = jfr.getByte();
        this.processStartTime = jfr.getVarlong();
        this.cpuUser = jfr.getFloat();
        this.cpuSystem = jfr.getFloat();
        this.cpuPercent = jfr.getFloat();
        this.threads = jfr.getVarint();
        this.vmSize = jfr.getVarlong();
        this.vmRss = jfr.getVarlong();
        this.rssAnon = jfr.getVarlong();
        this.rssFiles = jfr.getVarlong();
        this.rssShmem = jfr.getVarlong();
        this.minorFaults = jfr.getVarlong();
        this.majorFaults = jfr.getVarlong();
        this.ioRead = jfr.getVarlong();
        this.ioWrite = jfr.getVarlong();
    }
}
