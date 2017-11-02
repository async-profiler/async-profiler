#!/bin/bash

usage() {
    echo "Usage: $0 [action] [options] <pid>"
    echo "Actions:"
    echo "  start             start profiling and return immediately"
    echo "  stop              stop profiling"
    echo "  status            print profiling status"
    echo "  list              list profiling events supported by the target JVM"
    echo "  collect           collect profile for the specified period of time"
    echo "                    and then stop (default action)"
    echo "Options:"
    echo "  -e event          profiling event: cpu|alloc|lock|cache-misses etc."
    echo "  -d duration       run profiling for <duration> seconds"
    echo "  -f filename       dump output to <filename>"
    echo "  -i interval       sampling interval in nanoseconds"
    echo "  -b bufsize        frame buffer size"
    echo "  -t                profile different threads separately"
    echo "  -o fmt[,fmt...]   output format: summary|traces|flat|collapsed"
    echo ""
    echo "<pid> is a numeric process ID of the target JVM"
    echo "      or 'jps' keyword to find running JVM automatically using jps tool"
    echo ""
    echo "Example: $0 -d 30 -f profile.fg -o collapsed 3456"
    echo "         $0 start -i 999000 jps"
    echo "         $0 stop -o summary,flat jps"
    exit 1
}

jattach() {
    $JATTACH $PID load $PROFILER true $1 > /dev/null
    RET=$?

    # Check if jattach failed
    if [ $RET -ne 0 ]; then
        if [ $RET -eq 255 ]; then
            echo "Failed to inject profiler into $PID"
            UNAME_S=$(uname -s)
            if [ "$UNAME_S" == "Darwin" ]; then
                otool -L $PROFILER
            else
                ldd $PROFILER
            fi
        fi
        exit $RET
    fi

    # Duplicate output from temporary file to local terminal
    if [[ $USE_TMP ]]; then
        if [[ -f $FILE ]]; then
            cat $FILE
            rm $FILE
        fi
    fi
}

function abspath() {
    UNAME_S=$(uname -s)
    if [ "$UNAME_S" == "Darwin" ]; then
        perl -MCwd -e 'print Cwd::abs_path shift' $1
    else
        readlink -f $1
    fi
}


OPTIND=1
SCRIPT_DIR=$(dirname $0)
JATTACH=$SCRIPT_DIR/build/jattach
PROFILER=$(abspath $SCRIPT_DIR/build/libasyncProfiler.so)
ACTION="collect"
EVENT="cpu"
DURATION="60"
FILE=""
USE_TMP="true"
INTERVAL=""
FRAMEBUF=""
THREADS=""
OUTPUT="summary,traces=200,flat=200"

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|"-?")
            usage
            ;;
        start|stop|status|list|collect)
            ACTION="$1"
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
            unset USE_TMP
            shift
            ;;
        -i)
            INTERVAL=",interval=$2"
            shift
            ;;
        -b)
            FRAMEBUF=",framebuf=$2"
            shift
            ;;
        -t)
            THREADS=",threads"
            ;;
        -o)
            OUTPUT="$2"
            shift
            ;;
        [0-9]*)
            PID="$1"
            ;;
        jps)
            # A shortcut for getting PID of a running Java application
            # -XX:+PerfDisableSharedMem prevents jps from appearing in its own list
            PID=$(jps -q -J-XX:+PerfDisableSharedMem)
            ;;
        *)
        	echo "Unrecognized option: $1"
        	usage
        	;;
    esac
    shift
done

[[ "$PID" == "" ]] && usage

# if no -f argument is given, use temporary file to transfer output to caller terminal
if [[ $USE_TMP ]]; then
    FILE=$(mktemp /tmp/async-profiler.XXXXXXXX)
fi

case $ACTION in
    start)
        jattach start,event=$EVENT,file=$FILE$INTERVAL$FRAMEBUF$THREADS
        ;;
    stop)
        jattach stop,file=$FILE,$OUTPUT
        ;;
    status)
        jattach status,file=$FILE
        ;;
    list)
        jattach list,file=$FILE
        ;;
    collect)
        jattach start,event=$EVENT,file=$FILE$INTERVAL$FRAMEBUF$THREADS
        sleep $DURATION
        jattach stop,file=$FILE,$OUTPUT
        ;;
esac
