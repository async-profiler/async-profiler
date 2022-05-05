/*
 * Copyright 2020 Andrei Pangin
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

#include "jfrMetadata.h"


std::map<std::string, int> Element::_string_map;
std::vector<std::string> Element::_strings;

JfrMetadata JfrMetadata::_root;

JfrMetadata::JfrMetadata() : Element("root") {
    *this
        << (element("metadata")

            << type("boolean", T_BOOLEAN)
            << type("char", T_CHAR)
            << type("float", T_FLOAT)
            << type("double", T_DOUBLE)
            << type("byte", T_BYTE)
            << type("short", T_SHORT)
            << type("int", T_INT)
            << type("long", T_LONG)

            << type("java.lang.String", T_STRING)

            << (type("java.lang.Class", T_CLASS, "Java Class")
                << field("classLoader", T_CLASS_LOADER, "Class Loader", F_CPOOL)
                << field("name", T_SYMBOL, "Name", F_CPOOL)
                << field("package", T_PACKAGE, "Package", F_CPOOL)
                << field("modifiers", T_INT, "Access Modifiers"))

            << (type("java.lang.Thread", T_THREAD, "Thread")
                << field("osName", T_STRING, "OS Thread Name")
                << field("osThreadId", T_LONG, "OS Thread Id")
                << field("javaName", T_STRING, "Java Thread Name")
                << field("javaThreadId", T_LONG, "Java Thread Id"))

            << (type("jdk.types.ClassLoader", T_CLASS_LOADER, "Java Class Loader")
                << field("type", T_CLASS, "Type", F_CPOOL)
                << field("name", T_SYMBOL, "Name", F_CPOOL))

            << (type("jdk.types.FrameType", T_FRAME_TYPE, "Frame type", true)
                << field("description", T_STRING, "Description"))

            << (type("jdk.types.ThreadState", T_THREAD_STATE, "Java Thread State", true)
                << field("name", T_STRING, "Name"))

            << (type("jdk.types.StackTrace", T_STACK_TRACE, "Stacktrace")
                << field("truncated", T_BOOLEAN, "Truncated")
                << field("frames", T_STACK_FRAME, "Stack Frames", F_ARRAY))

            << (type("jdk.types.StackFrame", T_STACK_FRAME)
                << field("method", T_METHOD, "Java Method", F_CPOOL)
                << field("lineNumber", T_INT, "Line Number")
                << field("bytecodeIndex", T_INT, "Bytecode Index")
                << field("type", T_FRAME_TYPE, "Frame Type", F_CPOOL))

            << (type("jdk.types.Method", T_METHOD, "Java Method")
                << field("type", T_CLASS, "Type", F_CPOOL)
                << field("name", T_SYMBOL, "Name", F_CPOOL)
                << field("descriptor", T_SYMBOL, "Descriptor", F_CPOOL)
                << field("modifiers", T_INT, "Access Modifiers")
                << field("hidden", T_BOOLEAN, "Hidden"))

            << (type("jdk.types.Package", T_PACKAGE, "Package")
                << field("name", T_SYMBOL, "Name", F_CPOOL))

            << (type("jdk.types.Symbol", T_SYMBOL, "Symbol", true)
                << field("string", T_STRING, "String"))

            << (type("profiler.types.LogLevel", T_LOG_LEVEL, "Log Level", true)
                << field("name", T_STRING, "Name"))

            << (type("jdk.ExecutionSample", T_EXECUTION_SAMPLE, "Method Profiling Sample")
                << category("Java Virtual Machine", "Profiling")
                << field("startTime", T_LONG, "Start Time", F_TIME_TICKS)
                << field("sampledThread", T_THREAD, "Thread", F_CPOOL)
                << field("stackTrace", T_STACK_TRACE, "Stack Trace", F_CPOOL)
                << field("state", T_THREAD_STATE, "Thread State", F_CPOOL)
                << field("contextId", T_LONG, "Context ID"))

            << (type("jdk.ObjectAllocationInNewTLAB", T_ALLOC_IN_NEW_TLAB, "Allocation in new TLAB")
                << category("Java Application")
                << field("startTime", T_LONG, "Start Time", F_TIME_TICKS)
                << field("eventThread", T_THREAD, "Event Thread", F_CPOOL)
                << field("stackTrace", T_STACK_TRACE, "Stack Trace", F_CPOOL)
                << field("objectClass", T_CLASS, "Object Class", F_CPOOL)
                << field("allocationSize", T_LONG, "Allocation Size", F_BYTES)
                << field("tlabSize", T_LONG, "TLAB Size", F_BYTES)
                << field("contextId", T_LONG, "Context ID"))

            << (type("jdk.ObjectAllocationOutsideTLAB", T_ALLOC_OUTSIDE_TLAB, "Allocation outside TLAB")
                << category("Java Application")
                << field("startTime", T_LONG, "Start Time", F_TIME_TICKS)
                << field("eventThread", T_THREAD, "Event Thread", F_CPOOL)
                << field("stackTrace", T_STACK_TRACE, "Stack Trace", F_CPOOL)
                << field("objectClass", T_CLASS, "Object Class", F_CPOOL)
                << field("allocationSize", T_LONG, "Allocation Size", F_BYTES)
                << field("contextId", T_LONG, "Context ID"))

            << (type("jdk.JavaMonitorEnter", T_MONITOR_ENTER, "Java Monitor Blocked")
                << category("Java Application")
                << field("startTime", T_LONG, "Start Time", F_TIME_TICKS)
                << field("duration", T_LONG, "Duration", F_DURATION_TICKS)
                << field("eventThread", T_THREAD, "Event Thread", F_CPOOL)
                << field("stackTrace", T_STACK_TRACE, "Stack Trace", F_CPOOL)
                << field("monitorClass", T_CLASS, "Monitor Class", F_CPOOL)
                << field("previousOwner", T_THREAD, "Previous Monitor Owner", F_CPOOL)
                << field("address", T_LONG, "Monitor Address", F_ADDRESS)
                << field("contextId", T_LONG, "Context ID"))

            << (type("jdk.ThreadPark", T_THREAD_PARK, "Java Thread Park")
                << category("Java Application")
                << field("startTime", T_LONG, "Start Time", F_TIME_TICKS)
                << field("duration", T_LONG, "Duration", F_DURATION_TICKS)
                << field("eventThread", T_THREAD, "Event Thread", F_CPOOL)
                << field("stackTrace", T_STACK_TRACE, "Stack Trace", F_CPOOL)
                << field("parkedClass", T_CLASS, "Class Parked On", F_CPOOL)
                << field("timeout", T_LONG, "Park Timeout", F_DURATION_NANOS)
                << field("until", T_LONG, "Park Until", F_TIME_MILLIS)
                << field("address", T_LONG, "Address of Object Parked", F_ADDRESS))

            << (type("jdk.CPULoad", T_CPU_LOAD, "CPU Load")
                << category("Operating System", "Processor")
                << field("startTime", T_LONG, "Start Time", F_TIME_TICKS)
                << field("jvmUser", T_FLOAT, "JVM User", F_PERCENTAGE)
                << field("jvmSystem", T_FLOAT, "JVM System", F_PERCENTAGE)
                << field("machineTotal", T_FLOAT, "Machine Total", F_PERCENTAGE))

            << (type("jdk.ActiveRecording", T_ACTIVE_RECORDING, "Async-profiler Recording")
                << category("Flight Recorder")
                << field("startTime", T_LONG, "Start Time", F_TIME_TICKS)
                << field("duration", T_LONG, "Duration", F_DURATION_TICKS)
                << field("eventThread", T_THREAD, "Event Thread", F_CPOOL)
                << field("id", T_LONG, "Id")
                << field("name", T_STRING, "Name")
                << field("destination", T_STRING, "Destination")
                << field("maxAge", T_LONG, "Max Age", F_DURATION_MILLIS)
                << field("maxSize", T_LONG, "Max Size", F_BYTES)
                << field("recordingStart", T_LONG, "Start Time", F_TIME_MILLIS)
                << field("recordingDuration", T_LONG, "Recording Duration", F_DURATION_MILLIS))

            << (type("jdk.ActiveSetting", T_ACTIVE_SETTING, "Async-profiler Setting")
                << category("Flight Recorder")
                << field("startTime", T_LONG, "Start Time", F_TIME_TICKS)
                << field("duration", T_LONG, "Duration", F_DURATION_TICKS)
                << field("eventThread", T_THREAD, "Event Thread", F_CPOOL)
                << field("stackTrace", T_STACK_TRACE, "Stack Trace", F_CPOOL)
                << field("id", T_LONG, "Event Id")
                << field("name", T_STRING, "Setting Name")
                << field("value", T_STRING, "Setting Value"))

            << (type("jdk.OSInformation", T_OS_INFORMATION, "OS Information")
                << category("Operating System")
                << field("startTime", T_LONG, "Start Time", F_TIME_TICKS)
                << field("osVersion", T_STRING, "OS Version"))

            << (type("jdk.CPUInformation", T_CPU_INFORMATION, "CPU Information")
                << category("Operating System", "Processor")
                << field("startTime", T_LONG, "Start Time", F_TIME_TICKS)
                << field("cpu", T_STRING, "Type")
                << field("description", T_STRING, "Description")
                << field("sockets", T_INT, "Sockets", F_UNSIGNED)
                << field("cores", T_INT, "Cores", F_UNSIGNED)
                << field("hwThreads", T_INT, "Hardware Threads", F_UNSIGNED))

            << (type("jdk.JVMInformation", T_JVM_INFORMATION, "JVM Information")
                << category("Java Virtual Machine")
                << field("startTime", T_LONG, "Start Time", F_TIME_TICKS)
                << field("jvmName", T_STRING, "JVM Name")
                << field("jvmVersion", T_STRING, "JVM Version")
                << field("jvmArguments", T_STRING, "JVM Command Line Arguments")
                << field("jvmFlags", T_STRING, "JVM Settings File Arguments")
                << field("javaArguments", T_STRING, "Java Application Arguments")
                << field("jvmStartTime", T_LONG, "JVM Start Time", F_TIME_MILLIS)
                << field("pid", T_LONG, "Process Identifier"))

            << (type("jdk.InitialSystemProperty", T_INITIAL_SYSTEM_PROPERTY, "Initial System Property")
                << category("Java Virtual Machine")
                << field("startTime", T_LONG, "Start Time", F_TIME_TICKS)
                << field("key", T_STRING, "Key")
                << field("value", T_STRING, "Value"))

            << (type("jdk.NativeLibrary", T_NATIVE_LIBRARY, "Native Library")
                << category("Java Virtual Machine", "Runtime")
                << field("startTime", T_LONG, "Start Time", F_TIME_TICKS)
                << field("name", T_STRING, "Name")
                << field("baseAddress", T_LONG, "Base Address", F_ADDRESS)
                << field("topAddress", T_LONG, "Top Address", F_ADDRESS))

            << (type("profiler.Log", T_LOG, "Log Message")
                << category("Profiler")
                << field("startTime", T_LONG, "Start Time", F_TIME_TICKS)
                << field("level", T_LOG_LEVEL, "Level", F_CPOOL)
                << field("message", T_STRING, "Message"))

            << (type("jdk.jfr.Label", T_LABEL, NULL)
                << field("value", T_STRING))

            << (type("jdk.jfr.Category", T_CATEGORY, NULL)
                << field("value", T_STRING, NULL, F_ARRAY))

            << (type("jdk.jfr.Timestamp", T_TIMESTAMP, "Timestamp")
                << field("value", T_STRING))

            << (type("jdk.jfr.Timespan", T_TIMESPAN, "Timespan")
                << field("value", T_STRING))

            << (type("jdk.jfr.DataAmount", T_DATA_AMOUNT, "Data Amount")
                << field("value", T_STRING))

            << type("jdk.jfr.MemoryAddress", T_MEMORY_ADDRESS, "Memory Address")

            << type("jdk.jfr.Unsigned", T_UNSIGNED, "Unsigned Value")

            << type("jdk.jfr.Percentage", T_PERCENTAGE, "Percentage"))

        << element("region").attribute("locale", "en_US").attribute("gmtOffset", "0");

    // The map is used only during construction
    _string_map.clear();
}
