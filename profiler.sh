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
    echo "  -j jstackdepth    maximum Java stack depth"
    echo "  -b bufsize        frame buffer size"
    echo "  -t                profile different threads separately"
    echo "  -s                simple class names instead of FQN"
    echo "  -o fmt[,fmt...]   output format: summary|traces|flat|collapsed|svg|tree|jfr"
    echo ""
    echo "  --title string    SVG title"
    echo "  --width px        SVG width"
    echo "  --height px       SVG frame height"
    echo "  --minwidth px     skip frames smaller than px"
    echo "  --reverse         generate stack-reversed FlameGraph / Call tree"
    echo ""
    echo "  --all-kernel      only include kernel-mode events"
    echo "  --all-user        only include user-mode events"
    echo "  -v, --version     display version string"
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
    if [[ $USE_TMP ]]; then
        if [[ -f $FILE ]]; then
            cat $FILE
            rm $FILE
        fi
    fi
}

check_if_terminated() {
    if ! kill -0 $PID 2> /dev/null; then
        mirror_output
        exit 0
    fi
}

jattach() {
    $JATTACH $PID load "$PROFILER" true "$1" > /dev/null
    RET=$?

    # Check if jattach failed
    if [ $RET -ne 0 ]; then
        if [ $RET -eq 255 ]; then
            echo "Failed to inject profiler into $PID"
            if [ "$UNAME_S" == "Darwin" ]; then
                otool -L "$PROFILER"
            else
                ldd "$PROFILER"
            fi
        fi
        exit $RET
    fi

    mirror_output
}

function abspath() {
    if [ "$UNAME_S" == "Darwin" ]; then
        perl -MCwd -e 'print Cwd::abs_path shift' $1
    else
        readlink -f $1
    fi
}


OPTIND=1
UNAME_S=$(uname -s)
SCRIPT_DIR=$(dirname $(abspath $0))
JATTACH=$SCRIPT_DIR/build/jattach
PROFILER=$SCRIPT_DIR/build/libasyncProfiler.so
ACTION="collect"
EVENT="cpu"
DURATION="60"
FILE=""
USE_TMP="true"
INTERVAL=""
JSTACKDEPTH=""
FRAMEBUF=""
THREADS=""
OUTPUT=""
FORMAT=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|"-?")
            usage
            ;;
        start|stop|status|list|collect)
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
            unset USE_TMP
            shift
            ;;
        -i)
            INTERVAL=",interval=$2"
            shift
            ;;
        -j)
            JSTACKDEPTH=",jstackdepth=$2"
            shift
            ;;
        -b)
            FRAMEBUF=",framebuf=$2"
            shift
            ;;
        -t)
            THREADS=",threads"
            ;;
        -s)
            FORMAT="$FORMAT,simple"
            ;;
        -o)
            OUTPUT="$2"
            shift
            ;;
        --title)
            # escape XML special characters and comma
            TITLE=${2//&/&amp;}
            TITLE=${TITLE//</&lt;}
            TITLE=${TITLE//>/&gt;}
            TITLE=${TITLE//,/&#44;}
            FORMAT="$FORMAT,title=$TITLE"
            shift
            ;;
        --width|--height|--minwidth)
            FORMAT="$FORMAT,${1:2}=$2"
            shift
            ;;
        --reverse)
            FORMAT="$FORMAT,reverse"
            ;;
        [0-9]*)
            PID="$1"
            ;;
        jps)
            # A shortcut for getting PID of a running Java application
            # -XX:+PerfDisableSharedMem prevents jps from appearing in its own list
            PID=$(pgrep -n java || jps -q -J-XX:+PerfDisableSharedMem)
            ;;
	--all-kernel)
	    ALLKERNEL=",allkernel"
	    ;;
	--all-user)
	    ALLUSER=",alluser"
	    ;;
        *)
        	echo "Unrecognized option: $1"
        	usage
        	;;
    esac
    shift
done

if [[ "$PID" == "" && "$ACTION" != "version" ]]; then
    usage
fi

# If no -f argument is given, use temporary file to transfer output to caller terminal.
# Let the target process create the file in case this script is run by superuser.
if [[ $USE_TMP ]]; then
    FILE=/tmp/async-profiler.$$.$PID
fi

# select default output format
if [[ "$OUTPUT" == "" ]]; then
    if [[ $FILE == *.svg ]]; then
        OUTPUT="svg"
    elif [[ $FILE == *.html ]]; then
        OUTPUT="tree"
    elif [[ $FILE == *.jfr ]]; then
        OUTPUT="jfr"
    elif [[ $FILE == *.collapsed ]] || [[ $FILE == *.folded ]]; then
        OUTPUT="collapsed"
    else
        OUTPUT="summary,traces=200,flat=200"
    fi
fi

case $ACTION in
    start)
        jattach "start,event=$EVENT,file=$FILE$INTERVAL$JSTACKDEPTH$FRAMEBUF$THREADS$ALLKERNEL$ALLUSER,$OUTPUT$FORMAT"
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
        jattach "start,event=$EVENT,file=$FILE$INTERVAL$JSTACKDEPTH$FRAMEBUF$THREADS$ALLKERNEL$ALLUSER,$OUTPUT$FORMAT"
        while (( DURATION-- > 0 )); do
            check_if_terminated
            sleep 1
        done
        jattach "stop,file=$FILE,$OUTPUT$FORMAT"
        ;;
    version)
        if [[ "$PID" == "" ]]; then
            java "-agentpath:$PROFILER=version" -version 2> /dev/null
        else
            jattach "version,file=$FILE"
        fi
        ;;
esac
