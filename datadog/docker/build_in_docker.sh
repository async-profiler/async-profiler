#! /bin/bash

export PROFILER_VERSION=$1
FORCE_TESTS=$2

cd /data/src/async-profiler
# make sure all previously compiled classes are gone to prevent any bytecode incompatibility
find . -name '*.class' | xargs -I {} rm -f {}
# build the library
make clean all
if [ "yes" = "$FORCE_TESTS" ]; then
  make test
fi
cp build/libasyncProfiler.so /data/libs/libasyncProfiler.so