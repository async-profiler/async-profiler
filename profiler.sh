#!/bin/sh
set -eu

usage() {
    echo "Usage: $0 [action] [options] <pid>"
    echo "Actions:"
    echo "  start             start profiling and return immediately"
    echo "  resume            resume profiling without resetting collected data"
    echo "  stop              stop profiling"
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
    echo "  -b bufsize        frame buffer size"
    echo "  -t                profile different threads separately"
    echo "  -s                simple class names instead of FQN"
    echo "  -g                print method signatures"
    echo "  -a                annotate Java method names"
    echo "  -o fmt            output format: summary|traces|flat|collapsed|svg|tree|jfr"
    echo "  -I include        output only stack traces containing the specified pattern"
    echo "  -X exclude        exclude stack traces with the specified pattern"
    echo "  -v, --version     display version string"
    echo ""
    echo "  --title string    SVG title"
    echo "  --width px        SVG width"
    echo "  --height px       SVG frame height"
    echo "  --minwidth px     skip frames smaller than px"
    echo "  --reverse         generate stack-reversed FlameGraph / Call tree"
    echo ""
    echo "  --all-kernel      only include kernel-mode events"
    echo "  --all-user        only include user-mode events"
    echo "  --cstack mode     how to traverse C stack: fp|lbr|no"
    echo ""
    echo "<pid> is a numeric process ID of the target JVM"
    echo "      or 'jps' keyword to find running JVM automatically"
    echo ""
    echo "Example: $0 -d 30 -f profile.svg 3456"
    echo "         $0 start -i 999000 jps"
    echo "         $0 stop -o summary,flat jps"
    exit 1
}

mirror_output() {
    # Mirror output from temporary file to local terminal
    if [ "$USE_TMP" = true ]; then
        if [ -f "$FILE" ]; then
            cat "$FILE"
            rm "$FILE"
        fi
    fi
}

check_if_terminated() {
    if ! kill -0 "$PID" 2> /dev/null; then
        mirror_output
        exit 0
    fi
}

jattach() {
    set +e
    "$JATTACH" "$PID" load "$PROFILER" true "$1" > /dev/null
    RET=$?
    set -e

    # Check if jattach failed
    if [ $RET -ne 0 ]; then
        if [ $RET -eq 255 ]; then
            echo "Failed to inject profiler into $PID"
            if [ "$(uname -s)" = "Darwin" ]; then
                otool -L "$PROFILER"
            else
                ldd "$PROFILER"
            fi
        fi
        exit $RET
    fi

    mirror_output
}

OPTIND=1
SCRIPT_DIR="$(cd "$(dirname "$0")" > /dev/null 2>&1; pwd -P)"
JATTACH=$SCRIPT_DIR/build/jattach
PROFILER=$SCRIPT_DIR/build/libasyncProfiler.so
ACTION="collect"
EVENT="cpu"
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
        start|resume|stop|check|status|list|collect)
            ACTION="$1"
            ;;
        -v|--version)
            ACTION="version"
            ;;
        -e)
            EVENT="$2"
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
        -b)
            PARAMS="$PARAMS,framebuf=$2"
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
        --all-kernel)
            PARAMS="$PARAMS,allkernel"
            ;;
        --all-user)
            PARAMS="$PARAMS,alluser"
            ;;
        --cstack|--call-graph)
            PARAMS="$PARAMS,cstack=$2"
            shift
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
            ;;
        *)
            echo "Unrecognized option: $1"
            usage
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

case $ACTION in
    start|resume|check)
        jattach "$ACTION,event=$EVENT,file=$FILE,$OUTPUT$FORMAT$PARAMS"
        ;;
    stop)
        jattach "stop,file=$FILE,$OUTPUT$FORMAT"
        ;;
    status)
        jattach "status,file=$FILE"
        ;;
    list)
        jattach "list,file=$FILE"
        ;;
    collect)
        jattach "start,event=$EVENT,file=$FILE,$OUTPUT$FORMAT$PARAMS"
        while [ "$DURATION" -gt 0 ]; do
            DURATION=$(( DURATION-1 ))
            check_if_terminated
            sleep 1
        done
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
