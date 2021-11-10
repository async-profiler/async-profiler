#!/bin/sh
set -eu

usage() {
    echo "Usage: $0 [action] [options] <pid>"
    echo "Actions:"
    echo "  start             start profiling and return immediately"
    echo "  resume            resume profiling without resetting collected data"
    echo "  stop              stop profiling"
    echo "  dump              dump collected data without stopping profiling session"
    echo "  check             check if the specified profiling event is available"
    echo "  status            print profiling status"
    echo "  list              list profiling events supported by the target JVM"
    echo "  collect           collect profile for the specified period of time"
    echo "                    and then stop (default action)"
    echo "Options:"
    echo "  -e event          profiling event: cpu|alloc|lock|cache-misses etc."
    echo "  -d duration       run profiling for <duration> seconds"
    echo "  -f filename       dump output to <filename>"
    echo "  -i interval       sampling interval in nanoseconds"
    echo "  -j jstackdepth    maximum Java stack depth"
    echo "  -t                profile different threads separately"
    echo "  -s                simple class names instead of FQN"
    echo "  -g                print method signatures"
    echo "  -a                annotate Java method names"
    echo "  -l                prepend library names"
    echo "  -o fmt            output format: flat|traces|collapsed|flamegraph|tree|jfr"
    echo "  -I include        output only stack traces containing the specified pattern"
    echo "  -X exclude        exclude stack traces with the specified pattern"
    echo "  -v, --version     display version string"
    echo ""
    echo "  --title string    FlameGraph title"
    echo "  --minwidth pct    skip frames smaller than pct%"
    echo "  --reverse         generate stack-reversed FlameGraph / Call tree"
    echo ""
    echo "  --alloc bytes     allocation profiling interval in bytes"
    echo "  --lock duration   lock profiling threshold in nanoseconds"
    echo "  --total           accumulate the total value (time, bytes, etc.)"
    echo "  --all-user        only include user-mode events"
    echo "  --sched           group threads by scheduling policy"
    echo "  --cstack mode     how to traverse C stack: fp|lbr|no"
    echo "  --begin function  begin profiling when function is executed"
    echo "  --end function    end profiling when function is executed"
    echo "  --ttsp            time-to-safepoint profiling"
    echo "  --jfrsync config  synchronize profiler with JFR recording"
    echo "  --fdtransfer      use fdtransfer to serve perf requests"
    echo "                    from the non-privileged target"
    echo ""
    echo "<pid> is a numeric process ID of the target JVM"
    echo "      or 'jps' keyword to find running JVM automatically"
    echo "      or the application's name as it would appear in the jps tool"
    echo ""
    echo "Example: $0 -d 30 -f profile.html 3456"
    echo "         $0 start -i 999000 jps"
    echo "         $0 stop -o flat jps"
    echo "         $0 -d 5 -e alloc MyAppName"
    exit 1
}

mirror_output() {
    # Mirror output from temporary file to local terminal
    if [ "$USE_TMP" = true ]; then
        if [ -f "$FILE" ]; then
            cat "$FILE"
            rm "$FILE"
        elif [ -f "$ROOT_PREFIX$FILE" ]; then
            cat "$ROOT_PREFIX$FILE"
            rm "$ROOT_PREFIX$FILE"
        fi
    fi
}

mirror_log() {
    # Try to access the log file both directly and through /proc/[pid]/root,
    # in case the target namespace differs
    if [ -f "$LOG" ]; then
        cat "$LOG" >&2
        rm "$LOG"
    elif [ -f "$ROOT_PREFIX$LOG" ]; then
        cat "$ROOT_PREFIX$LOG" >&2
        rm "$ROOT_PREFIX$LOG"
    fi
}

check_if_terminated() {
    if ! kill -0 "$PID" 2> /dev/null; then
        mirror_output
        exit 0
    fi
}

fdtransfer() {
    if [ "$USE_FDTRANSFER" = "true" ]; then
        "$FDTRANSFER" "$PID"
    fi
}

jattach() {
    set +e
    "$JATTACH" "$PID" load "$PROFILER" true "$1,log=$LOG" > /dev/null
    RET=$?

    # Check if jattach failed
    if [ $RET -ne 0 ]; then
        if [ $RET -eq 255 ]; then
            echo "Failed to inject profiler into $PID"
            if [ "$UNAME_S" = "Darwin" ]; then
                otool -L "$PROFILER"
            else
                LD_PRELOAD="$PROFILER" /bin/true
            fi
        fi

        mirror_log
        exit $RET
    fi

    mirror_log
    mirror_output
    set -e
}

SCRIPT_BIN="$0"
while [ -h "$SCRIPT_BIN" ]; do
    SCRIPT_BIN="$(readlink "$SCRIPT_BIN")"
done
SCRIPT_DIR="$(cd "$(dirname "$SCRIPT_BIN")" > /dev/null 2>&1; pwd -P)"

JATTACH=$SCRIPT_DIR/build/jattach
FDTRANSFER=$SCRIPT_DIR/build/fdtransfer
USE_FDTRANSFER="false"
PROFILER=$SCRIPT_DIR/build/libasyncProfiler.so
ACTION="collect"
DURATION="60"
FILE=""
USE_TMP="true"
OUTPUT=""
FORMAT=""
PARAMS=""
PID=""

while [ $# -gt 0 ]; do
    case $1 in
        -h|"-?")
            usage
            ;;
        start|resume|stop|dump|check|status|list|collect)
            ACTION="$1"
            ;;
        -v|--version)
            ACTION="version"
            ;;
        -e)
            PARAMS="$PARAMS,event=$2"
            shift
            ;;
        -d)
            DURATION="$2"
            shift
            ;;
        -f)
            FILE="$2"
            USE_TMP=false
            shift
            ;;
        -i)
            PARAMS="$PARAMS,interval=$2"
            shift
            ;;
        -j)
            PARAMS="$PARAMS,jstackdepth=$2"
            shift
            ;;
        -t)
            PARAMS="$PARAMS,threads"
            ;;
        -s)
            FORMAT="$FORMAT,simple"
            ;;
        -g)
            FORMAT="$FORMAT,sig"
            ;;
        -a)
            FORMAT="$FORMAT,ann"
            ;;
        -l)
            FORMAT="$FORMAT,lib"
            ;;
        -o)
            OUTPUT="$2"
            shift
            ;;
        -I|--include)
            FORMAT="$FORMAT,include=$2"
            shift
            ;;
        -X|--exclude)
            FORMAT="$FORMAT,exclude=$2"
            shift
            ;;
        --filter)
            FILTER="$(echo "$2" | sed 's/,/;/g')"
            FORMAT="$FORMAT,filter=$FILTER"
            shift
            ;;
        --title)
            # escape XML special characters and comma
            TITLE="$(echo "$2" | sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g; s/,/\&#44;/g')"
            FORMAT="$FORMAT,title=$TITLE"
            shift
            ;;
        --width|--height|--minwidth)
            FORMAT="$FORMAT,${1#--}=$2"
            shift
            ;;
        --reverse)
            FORMAT="$FORMAT,reverse"
            ;;
        --samples|--total)
            FORMAT="$FORMAT,${1#--}"
            ;;
        --alloc|--lock|--chunksize|--chunktime)
            PARAMS="$PARAMS,${1#--}=$2"
            shift
            ;;
        --all-user)
            PARAMS="$PARAMS,alluser"
            ;;
        --sched)
            PARAMS="$PARAMS,sched"
            ;;
        --cstack|--call-graph)
            PARAMS="$PARAMS,cstack=$2"
            shift
            ;;
        --begin|--end)
            PARAMS="$PARAMS,${1#--}=$2"
            shift
            ;;
        --ttsp)
            PARAMS="$PARAMS,begin=SafepointSynchronize::begin,end=RuntimeService::record_safepoint_synchronized"
            ;;
        --jfrsync)
            OUTPUT="jfr"
            PARAMS="$PARAMS,jfrsync=$2"
            shift
            ;;
        --fdtransfer)
            PARAMS="$PARAMS,fdtransfer"
            USE_FDTRANSFER="true"
            ;;
        --safe-mode)
            PARAMS="$PARAMS,safemode=$2"
            shift
            ;;
        [0-9]*)
            PID="$1"
            ;;
        jps)
            # A shortcut for getting PID of a running Java application
            # -XX:+PerfDisableSharedMem prevents jps from appearing in its own list
            PID=$(pgrep -n java || jps -q -J-XX:+PerfDisableSharedMem)
            if [ "$PID" = "" ]; then
                echo "No Java process could be found!"
            fi
            ;;
        -*)
            echo "Unrecognized option: $1"
            usage
            ;;
        *)
            if [ $# -eq 1 ]; then
                # the last argument is the application name as it would appear in the jps tool
                PID=$(jps -J-XX:+PerfDisableSharedMem | grep " $1$" | head -n 1 | cut -d ' ' -f 1)
                if [ "$PID" = "" ]; then
                    echo "No Java process '$1' could be found!"
                fi
            else
                echo "Unrecognized option: $1"
                usage
            fi
            ;;
    esac
    shift
done

if [ "$PID" = "" ] && [ "$ACTION" != "version" ]; then
    usage
fi

# If no -f argument is given, use temporary file to transfer output to caller terminal.
# Let the target process create the file in case this script is run by superuser.
if [ "$USE_TMP" = true ]; then
    FILE=/tmp/async-profiler.$$.$PID
else
    case "$FILE" in
        /*)
            # Path is absolute
            ;;
        *)
            # Output file is written by the target process. Make the path absolute to avoid confusion.
            FILE=$PWD/$FILE
            ;;
    esac
fi
LOG=/tmp/async-profiler-log.$$.$PID

UNAME_S=$(uname -s)
if [ "$UNAME_S" = "Linux" ]; then
    ROOT_PREFIX="/proc/$PID/root"
else
    ROOT_PREFIX=""
fi

case $ACTION in
    start|resume)
        fdtransfer
        jattach "$ACTION,file=$FILE,$OUTPUT$FORMAT$PARAMS"
        ;;
    check)
        jattach "$ACTION,file=$FILE,$OUTPUT$FORMAT$PARAMS"
        ;;
    stop|dump)
        jattach "$ACTION,file=$FILE,$OUTPUT$FORMAT"
        ;;
    status|list)
        jattach "$ACTION,file=$FILE"
        ;;
    collect)
        fdtransfer
        jattach "start,file=$FILE,$OUTPUT$FORMAT$PARAMS"
        echo Profiling for "$DURATION" seconds >&2
        set +e
        trap 'DURATION=0' INT

        while [ "$DURATION" -gt 0 ]; do
            DURATION=$(( DURATION-1 ))
            check_if_terminated
            sleep 1
        done

        set -e
        trap - INT
        echo Done >&2
        jattach "stop,file=$FILE,$OUTPUT$FORMAT"
        ;;
    version)
        if [ "$PID" = "" ]; then
            java "-agentpath:$PROFILER=version=full" -version 2> /dev/null
        else
            jattach "version=full,file=$FILE"
        fi
        ;;
esac
