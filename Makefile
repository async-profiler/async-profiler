LIB_PROFILER=libasyncProfiler.so
CPP=g++
CPPFLAGS=-O2

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	INCLUDES=-I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
endif
ifeq ($(UNAME_S),Darwin)
	INCLUDES=-I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/darwin
endif

all: build build/$(LIB_PROFILER)

build:
	mkdir -p build

build/$(LIB_PROFILER): src/*.cpp src/*.h
	$(CPP) $(CPPFLAGS) $(INCLUDES) -fPIC -shared -o $@ src/*.cpp

clean:
	rm -rf build
