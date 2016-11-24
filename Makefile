CPP=g++
CPPFLAGS=-O2

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	LIB_PROFILER=libasyncProfiler.so
	INCLUDES=-I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
endif
ifeq ($(UNAME_S),Darwin)
	LIB_PROFILER=libasyncProfiler.dylib
	INCLUDES=-I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/darwin
endif

all: build build/$(LIB_PROFILER)

build:
	mkdir -p build

build/libasyncProfiler.so: src/*.cpp src/*.h
	$(CPP) $(CPPFLAGS) $(INCLUDES) -fPIC -shared -o $@ -Wl,-soname,$(LIB_PROFILER) src/*.cpp

build/libasyncProfiler.dylib: src/*.cpp src/*.h
	$(CPP) $(CPPFLAGS) $(INCLUDES) -fPIC -shared -o $@ -Wl,-install_name,$(LIB_PROFILER) src/*.cpp

clean:
	rm -rf build
