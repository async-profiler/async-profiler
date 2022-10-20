#!/bin/bash

set -e  # exit on any failure
set -u
#set -x  # print all executed lines

if [ -z "${JAVA_HOME}" ]; then
  echo "JAVA_HOME is not set"
  exit 1
fi

function assertSamples() {
  CPU_ARG=
  WALL_ARG=
  FILTER_ARG=
  if [ "$1" == "true" ]; then
    CPU_ARG="cpu=1ms"
  fi
  if [ "$2" == "true" ]; then
    WALL_ARG="wall=1ms"
  fi
  if [ "$3" == "true" ]; then
    FILTER_ARG="filter"
  fi

  echo "! Test: cpu sampler: $1, wall sampler: $2, context filter: $3"
  echo "---"
  FILENAME=/tmp/java-context-smoke.jfr

  ${JAVA_HOME}/bin/java -cp .:../build/async-profiler.jar -agentpath:../build/libasyncProfiler.so=start,${CPU_ARG},${WALL_ARG},${FILTER_ARG},context,jfr,threads,file=$FILENAME ContextTarget
  CPU_SAMPLES=$(jfr summary $FILENAME | grep datadog\.ExecutionSample | tr -s " " | cut -f 3 -d ' ')
  WALL_SAMPLES=$(jfr summary $FILENAME | grep datadog\.MethodSample | tr -s " " | cut -f 3 -d ' ')

  echo "---"
  echo "Received $CPU_SAMPLES cpu samples and $WALL_SAMPLES wall samples"
  echo

  if [ -z "$CPU_ARG" ]; then
    if [ ! -z "$CPU_SAMPLES" ] && [ $CPU_SAMPLES -ne 0 ]; then
      echo "Expected zero datadog.ExecutionSample events"
      exit 1
    fi
  else 
    if [ -z "$CPU_SAMPLES" ] || [ $CPU_SAMPLES -eq 0 ]; then
      echo "Expected non-zero number of datadog.ExecutionSample events"
      exit 1
    fi
  fi

  if [ -z "$WALL_ARG" ]; then
    if [ ! -z "$WALL_SAMPLES" ] && [ $WALL_SAMPLES -ne 0 ]; then
      echo "Expected zero datadog.MethodSample events"
      exit 1
    fi
  else
    if [ -z "$WALL_SAMPLES" ] || [ $WALL_SAMPLES -eq 0 ]; then
      echo "Expected non-zero number of datadog.MethodSample events"
      exit 1
    fi
  fi
}

(
  cd $(dirname $0)

  if [ "ContextTarget.class" -ot "ContextTarget.java" ]; then
     ${JAVA_HOME}/bin/javac -cp .:../build/async-profiler.jar ContextTarget.java
  fi

  assertSamples "true" "false" "false"
  assertSamples "false" "true" "false"
  assertSamples "true" "true" "false"
  assertSamples "true" "false" "true"
  assertSamples "false" "true" "true"
  assertSamples "true" "true" "true"
)
