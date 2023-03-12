#!/bin/bash

set -e  # exit on any failure
set -x  # print all executed lines

if [ -z "${JAVA_HOME}" ]; then
  echo "JAVA_HOME is not set"
  exit 1
fi

(
  cd $(dirname $0)

  if [ "AllocatingTarget.class" -ot "AllocatingTarget.java" ]; then
     ${JAVA_HOME}/bin/javac AllocatingTarget.java
  fi

  ${JAVA_HOME}/bin/java AllocatingTarget &

  FILENAME=/tmp/java.trace
  JAVAPID=$!

  sleep 1     # allow the Java runtime to initialize
  ../build/bin/asprof -f $FILENAME -o collapsed -d 5 -e alloc -t $JAVAPID

  kill $JAVAPID

  function assert_string() {
    if ! grep -q "$1" $FILENAME; then
      exit 1
    fi
  }

  assert_string "\[AllocThread-1 tid=[0-9]\+\];.*AllocatingTarget.allocate;.*java.lang.Integer\[\]"
  assert_string "\[AllocThread-2 tid=[0-9]\+\];.*AllocatingTarget.allocate;.*int\[\]"
)
