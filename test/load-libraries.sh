#!/bin/bash

set -e  # exit on any failure
set -x  # print all executed lines

if [ -z "${JAVA_HOME}" ]; then
  echo "JAVA_HOME is not set"
  exit 1
fi

cd loadlibs

# build some dynamic libraries to load
g++ -ldl -c -fPIC -o increment.o increment.cpp
gcc -shared -o libincrement.so increment.o

g++ -ldl -c -fPIC -I$JAVA_HOME/include/linux/ -I$JAVA_HOME/include/ com_datadoghq_loader_DynamicLibraryLoader.cpp -o com_datadoghq_loader_DynamicLibraryLoader.o
g++ -shared -fPIC -o libloader.so com_datadoghq_loader_DynamicLibraryLoader.o -lc

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.
$JAVA_HOME/bin/javac com/datadoghq/loader/DynamicLibraryLoader.java
$JAVA_HOME/bin/java -cp . -agentpath:../../build/libasyncProfiler.so=start,cpu=1m,wall=~1ms,filter=0 -Djava.library.path=. com.datadoghq.loader.DynamicLibraryLoader ./libincrement.so+increment
