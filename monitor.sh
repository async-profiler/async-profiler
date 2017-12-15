#!/bin/bash

usage() {
    echo "This utility allows running async-profiler's (https://github.com/jvm-profiling-tools/async-profiler)"
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
    echo "  -ff filenamePostfix         Output filename postfix. Default: '.txt'"
    echo "  -i interval                 Sampling interval in nanoseconds."
    echo "  -b bufsize                  Frame buffer size in bytes."
    echo "  -t                          Profile different threads separately."
    echo "  -o fmt[,fmt...]             Output format: summary|traces|flat|collapsed."
    echo ""
    echo "<pid> is a numeric process ID of the target JVM,"
    echo "  or 'jps' keyword to find running JVM automatically using jps tool."
    echo ""
    echo "Example:"
    echo "  ${0} -d 600 -e cpu -o summary,traces=200,flat=200,collapsed=samples -f /home/me/prof_ -ff _pid<pid>.txt --history 450 <pid>"
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

CURRERNT_WORKING_DIR=$(pwd)
CURRENT_SCRIPT=$(readlink -f "${0}")
CURRENT_SCRIPT_DIR=$(dirname "${CURRENT_SCRIPT}")
PROFILER="${CURRENT_SCRIPT_DIR}/profiler.sh"
PRESERVE="5"
DURATION="30"
INTERVAL="1000000"
FILE_PREFIX="${CURRERNT_WORKING_DIR}/prof_"
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
        -ff)
            FILE_POSTFIX="${2}"
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
            # A shortcut for getting PID of a running Java application.
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
    if ! [ ${?} -eq 0 ]; then
        echo "Failed to start profiling session"
        exit 1
    fi
    # delete outdated gathered data
    ls ${FILE_PREFIX}* -t | tail -n +${PRESERVE} | xargs -d '\n' rm >/dev/null 2>&1
done
