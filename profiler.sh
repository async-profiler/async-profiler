#!/bin/bash

usage() {
    echo "Usage: $0 [action] [options] <pid>"
    echo "Actions:"
    echo "  start             start profiling and return immediately"
    echo "  stop              stop profiling"
    echo "  status            print profiling status"
    echo "  collect           collect profile for the specified period of time"
    echo "                    and then stop (default action)"
    echo "Options:"
    echo "  -m mode           profiling mode: cpu|heap"
    echo "  -d duration       run profiling for <duration> seconds"
    echo "  -f filename       dump output to <filename>"
    echo "  -i interval       sampling interval in nanoseconds"
    echo "  -b bufsize        frame buffer size"
    echo "  -o fmt[,fmt...]   output format: summary|traces|flat|collapsed"
    echo ""
    echo "Example: $0 -d 30 -f profile.fg -o collapsed 3456"
    echo "         $0 start -i 999000 3456"
    echo "         $0 stop -o summary,flat 3456"
    exit 1
}

show_agent_output() {
    if [[ $USE_TMP ]]; then
        if [[ -f $FILE ]]; then
            cat $FILE
            rm $FILE
        fi
    fi
}


OPTIND=1
SCRIPT_DIR=$(dirname $0)
JATTACH=$SCRIPT_DIR/build/jattach
PROFILER=$SCRIPT_DIR/build/libasyncProfiler.so
ACTION="collect"
MODE="cpu"
DURATION="60"
FILE=""
USE_TMP="true"
INTERVAL=""
FRAMEBUF=""
OUTPUT="summary,traces=200,flat=200"

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|"-?")
            usage
            ;;
        start|stop|status|collect)
            ACTION="$1"
            ;;
        -m)
            MODE="$2"
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
        -o)
            OUTPUT="$2"
            shift
            ;;
        [0-9]*)
            PID="$1"
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
        $JATTACH $PID load $PROFILER true start,$MODE,file=$FILE$INTERVAL$FRAMEBUF > /dev/null
        ;;
    stop)
        $JATTACH $PID load $PROFILER true stop,file=$FILE,$OUTPUT > /dev/null
        ;;
    status)
        $JATTACH $PID load $PROFILER true status,file=$FILE > /dev/null
        ;;
    collect)
        $JATTACH $PID load $PROFILER true start,$MODE,file=$FILE$INTERVAL$FRAMEBUF > /dev/null
        if [ $? -ne 0 ]; then
            exit 1
        fi
        show_agent_output
        sleep $DURATION
        $JATTACH $PID load $PROFILER true stop,file=$FILE,$OUTPUT > /dev/null
        ;;
esac

show_agent_output
