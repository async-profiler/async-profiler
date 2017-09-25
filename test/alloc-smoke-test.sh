#!/bin/bash

set -e  # exit on any failure
set -x  # print all executed lines

pushd $(dirname $0)

javac AllocatingTarget.java
java AllocatingTarget &

FILENAME=/tmp/java.trace
JAVAPID=$!

sleep 1     # allow the Java runtime to initialize
../profiler.sh -f $FILENAME -o collapsed -d 5 -m heap $JAVAPID

kill $JAVAPID

function assert_string() {
  if ! grep -q "$1" $FILENAME; then
    exit 1
  fi
}

assert_string "AllocatingTarget.allocate;.*\[Ljava/lang/Integer"
assert_string "AllocatingTarget.allocate;.*\[I"

popd
