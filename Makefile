LIB_PROFILER=libasyncProfiler.so
CPP=g++
CPPFLAGS=-O2
INCLUDES=-I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux

all: build build/$(LIB_PROFILER)

build:
	mkdir -p build

build/$(LIB_PROFILER): src/*.cpp src/*.h
	$(CPP) $(CPPFLAGS) $(INCLUDES) -fPIC -shared -o $@ src/*.cpp -ldl

clean:
	rm -rf build
