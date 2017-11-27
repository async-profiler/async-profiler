#!/bin/bash

usage() {
    echo "This utility allows to run async-profiler's (https://github.com/jvm-profiling-tools/async-profiler)"
    echo "  profiling iterations (sessions) infinitely while the target process is available."
    echo ""
    echo "Usage: ${0} [options] <pid>"
    echo "Options:"
    echo "  --profiler profilerScript   Path to 'profiler.sh'."
    echo "  --history preserveN         Number of files with gathered data to preserve."
    echo "                              <preserveN> * <duration> is the length of the history in seconds."
    echo "  -e event                    Profiling event: cpu|alloc|lock|cache-misses etc."
    echo "  -d duration                 Duration of a single profiling session in seconds."
    echo "  -f filenamePrefix           Output filename prefix."
    echo "  -i interval                 Sampling interval in nanoseconds."
    echo "  -b bufsize                  Frame buffer size in bytes."
    echo "  -t                          Profile different threads separately."
    echo "  -o fmt[,fmt...]             Output format: summary|traces|flat|collapsed."
    echo ""
    echo "<pid> is a numeric process ID of the target JVM,"
    echo "  or 'jps' keyword to find running JVM automatically using jps tool."
    echo ""
    echo "Example:"
    echo "  ${0} --profiler profiler.sh --history 5 -d 30 -i 1000000 -f /home/me/prof_ -o summary,traces=200,flat=200,collapsed=samples -e cpu -t -b 5000000 1234"
    echo "  ${0} jps"
    exit 1
}

# Exits with status 1 if the specified file is not executable or does not exist, otherwise does nothing.
#
# @param $1 - PID of the process to test.
ensure_executable() {
    if ! [[ -x "${1}" ]]; then
        echo "File ${1} is not executable or does not exist"
        exit 1
    fi
}

# Exits with status 1 if the specified process is not running, otherwise does nothing.
#
# @param $1 - PID of the process to test.
ensure_running() {
    if ! kill -0 ${1} 2> /dev/null; then
        echo "Process with PID=${1} is not running, or you don't have enough permissions to attach to it"
        exit 1
    fi
}

SCRIPT_DIR=$(pwd)
PROFILER="${SCRIPT_DIR}/profiler.sh"
PRESERVE="5"
DURATION="30"
INTERVAL="1000000"
FILE_PREFIX="${SCRIPT_DIR}/prof_"
FILE_POSTFIX=".txt"
OUTPUT="summary,traces=200,flat=200,collapsed=samples"
EVENT="cpu"
THREADS=""
FRAMEBUF="5000000"
while [[ $# -gt 0 ]]; do
    case ${1} in
        -h|"-?")
            usage
            ;;
        --profiler)
            PROFILER="${2}"
            shift
            ;;
        --history)
            PRESERVE="${2}"
            shift
            ;;
        -e)
            EVENT="${2}"
            shift
            ;;
        -d)
            DURATION="${2}"
            shift
            ;;
        -f)
            FILE_PREFIX="${2}"
            shift
            ;;
        -i)
            INTERVAL="${2}"
            shift
            ;;
        -b)
            FRAMEBUF="${2}"
            shift
            ;;
        -t)
            THREADS="-t"
            ;;
        -o)
            OUTPUT="${2}"
            shift
            ;;
        [0-9]*)
            PID="${1}"
            ;;
        jps)
            # A shortcut for getting PID of a running Java application
            # -XX:+PerfDisableSharedMem prevents jps from appearing in its own list.
            PID=$(jps -q -J-XX:+PerfDisableSharedMem)
            ;;
        *)
            echo "Unrecognized option: ${1}"
            usage
            ;;
    esac
    shift
done
ensure_executable ${PROFILER}
while true; do
    # stop if the target process is not running
    ensure_running ${PID}
    # start next profiling session
    timestamp=$(date +"%FT%H-%M-%S")
    ${PROFILER} -d ${DURATION} -i ${INTERVAL} -f ${FILE_PREFIX}${timestamp}${FILE_POSTFIX} -o ${OUTPUT} -e ${EVENT} ${THREADS} -b ${FRAMEBUF} ${PID}
    if [ ${?} -eq 1 ]; then
        echo "Failed to start profiling session"
        exit 1
    fi
    # delete outdated gathered data
    ls ${FILE_PREFIX}*${FILE_POSTFIX} -t | tail -n +${PRESERVE} | xargs -d '\n' rm 2>&1 /dev/null
done