# Copyright The async-profiler authors
# SPDX-License-Identifier: Apache-2.0

#!/bin/bash

set -euo pipefail

VERSION_STRING="JFR converter PROFILER_VERSION built on $(date '+%b %d %Y')"

if [[ "$1" == "-v" || "$1" == "--version" ]]; then
    echo "$VERSION_STRING"
    exit 0
fi

if [[ "$OSTYPE" == "darwin"* ]]; then
    SCRIPT_PATH="$(cd "$(dirname "$0")" && pwd)/$(basename "$0")"
else
    SCRIPT_PATH="$(readlink -f "$0")"
fi

JAVA_CMD=("java" "-Xss2M" "-Dsun.misc.URLClassPath.disableJarChecking")

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

JAVA_CMD+=("-jar" "$SCRIPT_PATH" "$@")

find_java() {
    local java_path="$1"
    [[ -x "$java_path" ]]
}

run_java() {
    # 1. Try JAVA_HOME
    if [[ -n "$JAVA_HOME" ]] && find_java "$JAVA_HOME/bin/java"; then
        exec "$JAVA_HOME/bin/java" "${JAVA_CMD[@]:1}"
    fi
    
    # 2. Try PATH
    if command -v java >/dev/null 2>&1; then
        exec java "${JAVA_CMD[@]:1}"
    fi
    
    # 3. Try /etc/alternatives/java
    if find_java "/etc/alternatives/java"; then
        exec "/etc/alternatives/java" "${JAVA_CMD[@]:1}"
    fi
    
    # 4. Try common JVM directories
    if [[ "$OSTYPE" == "darwin"* ]]; then
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
                if find_java "$java_exe"; then
                    exec "$java_exe" "${JAVA_CMD[@]:1}"
                fi
            fi
        done
    fi
    
    echo "No JDK found. Set JAVA_HOME or ensure java executable is on the PATH" >&2
    exit 1
}

run_java