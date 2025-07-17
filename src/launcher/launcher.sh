#!/bin/sh

# Copyright The async-profiler authors
# SPDX-License-Identifier: Apache-2.0

if [[ "$1" == "-v" || "$1" == "--version" ]]; then
    echo "JFR converter PROFILER_VERSION built on BUILD_DATE"
    exit 0
fi

JAVA_CMD=("-Xss2M" "-Dsun.misc.URLClassPath.disableJarChecking")

while [[ $# -gt 0 ]]; do
    case "$1" in
        -D*|-X*|-agent*)
            if [[ ${#1} -gt 2 ]]; then
                JAVA_CMD+=("$1")
            fi
            ;;
        -J*)
            JAVA_CMD+=("${1:2}")
            ;;
        *)
            break
            ;;
    esac
    shift
done

JAVA_CMD+=("-jar" "$0" "$@")

# 1. Try JAVA_HOME
if [[ -n "$JAVA_HOME" ]] && [[ -x "$JAVA_HOME/bin/java" ]]; then
    exec "$JAVA_HOME/bin/java" "${JAVA_CMD[@]}"
fi

# 2. Try PATH
if command -v java >/dev/null 2>&1; then
    exec java "${JAVA_CMD[@]}"
fi

# 3. Try /etc/alternatives/java
if [[ -x "/etc/alternatives/java" ]]; then
    exec "/etc/alternatives/java" "${JAVA_CMD[@]}"
fi

# 4. Try common JVM directories
if [[ "$(uname -s)" == "darwin"* ]]; then
    JVM_DIR="/Library/Java/JavaVirtualMachines"
    CONTENTS_HOME="/Contents/Home"
else
    JVM_DIR="/usr/lib/jvm"
    CONTENTS_HOME=""
fi

if [[ -d "$JVM_DIR" ]]; then
    for jvm in "$JVM_DIR"/*; do
        if [[ -d "$jvm" ]]; then
            java_exe="$jvm$CONTENTS_HOME/bin/java"
            if [[ -x "$java_exe" ]]; then
                exec "$java_exe" "${JAVA_CMD[@]}"
            fi
        fi
    done
fi

echo "No JDK found. Set JAVA_HOME or ensure java executable is on the PATH" >&2
exit 1
