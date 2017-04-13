#!/bin/bash

OPTIND=1
SCRIPT_DIR=$(dirname $0)
JATTACH=$SCRIPT_DIR/build/jattach
# realpath is not present on all distros, notably on the Travis CI image
PROFILER=$(readlink -f $SCRIPT_DIR/build/libasyncProfiler.so)
ACTION=""
PID=""
START_OPTIONS=""
FILE=""

while getopts ":h?a:p:o:f:" opt; do
    case $opt in
        h|\?)
            echo "Usage: $0 -a <start|stop|dump> -p <pid>"
            echo "       [-o start-options] [-f output-file]"
            echo ""
            echo "Example: $0 -p 8983 -a start"
            echo "         $0 -p 8983 -a stop"
            exit 1
            ;;
        a)
            ACTION=$OPTARG
            ;;
        p)
            PID=$OPTARG
            ;;
        o)
            START_OPTIONS=$OPTARG
            ;;
        f)
            FILE=$OPTARG
            ;;
    esac
done

if [[ "$PID" == "" ]]
then
    echo "Error: pid is required"
    exit 1
fi
if [[ "$ACTION" == "" ]]
then
    echo "Error: action is required (start, stop, or dump)"
    exit 1
fi

case $ACTION in
    "start")
        $JATTACH $PID load $PROFILER true $START_OPTIONS,start > /dev/null
        ;;
    "stop")
        $JATTACH $PID load $PROFILER true stop > /dev/null
        ;;
    "dump")
        if [[ "$FILE" == "" ]]
        then
            echo "Error: file required for dump action"
            exit 1
        fi
        $JATTACH $PID load $PROFILER true dumpRawTraces:$FILE > /dev/null
        ;;
esac
