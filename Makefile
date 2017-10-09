LIB_PROFILER=libasyncProfiler.so
JATTACH=jattach
CC=gcc
CFLAGS=-O2
CPP=g++
CPPFLAGS=-O2 -D_XOPEN_SOURCE
INCLUDES=-I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux -I$(JAVA_HOME)/include/darwin

.PHONY: test

all: build build/$(LIB_PROFILER) build/$(JATTACH)

build:
	mkdir -p build

build/$(LIB_PROFILER): src/*.cpp src/*.h
	$(CPP) $(CPPFLAGS) $(INCLUDES) -fPIC -shared -o $@ src/*.cpp -ldl -lpthread

build/$(JATTACH): src/jattach.c
	$(CC) $(CFLAGS) -o $@ $^

test: all
	test/smoke-test.sh
	test/alloc-smoke-test.sh

clean:
	rm -rf build
