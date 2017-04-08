#!/bin/bash

java -cp $JAVA_HOME/lib/tools.jar:./build Attach ./build/libasyncProfiler.so "$@"
