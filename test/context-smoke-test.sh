#!/bin/bash

set -e  # exit on any failure
set -u
set -x  # print all executed lines

if [ -z "${JAVA_HOME}" ]; then
  echo "JAVA_HOME is not set"
  exit 1
fi

(
  cd $(dirname $0)

  if [ "ContextTarget.class" -ot "ContextTarget.java" ]; then
     ${JAVA_HOME}/bin/javac -cp .:../build/async-profiler.jar ContextTarget.java
  fi

  FILENAME=/tmp/java-context-smoke.jfr

  ${JAVA_HOME}/bin/java -cp .:../build/async-profiler.jar -agentpath:../build/libasyncProfiler.so=start,cpu=1ms,wall=1ms,context,jfr,threads,file=$FILENAME ContextTarget
)
