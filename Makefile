LIB_PROFILER=libasyncProfiler.so
JATTACH=jattach
CC=gcc
CFLAGS=-O2
CPP=g++
CPPFLAGS=-O2
INCLUDES=-I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux

all: build build/$(LIB_PROFILER) build/$(JATTACH)

build:
	mkdir -p build

build/$(LIB_PROFILER): src/*.cpp src/*.h
	$(CPP) $(CPPFLAGS) $(INCLUDES) -fPIC -shared -o $@ src/*.cpp -ldl

build/$(JATTACH): src/jattach.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf build
