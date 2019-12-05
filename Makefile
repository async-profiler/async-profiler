PROFILER_VERSION=2.0

PACKAGE_NAME=async-profiler-$(PROFILER_VERSION)-$(OS_TAG)-$(ARCH_TAG)
PACKAGE_DIR=/tmp/$(PACKAGE_NAME)

LIB_PROFILER=libasyncProfiler.$(SOEXT)
LIB_PROFILER_SO=libasyncProfiler.so
JATTACH=jattach
API_JAR=async-profiler.jar
CONVERTER_JAR=converter.jar

CFLAGS=-O3 -fno-omit-frame-pointer -fvisibility=hidden
CXXFLAGS=-O3 -fno-omit-frame-pointer -fvisibility=hidden
INCLUDES=-I$(JAVA_HOME)/include
LIBS=-ldl -lpthread

JAVAC=$(JAVA_HOME)/bin/javac
JAR=$(JAVA_HOME)/bin/jar

SOURCES := $(wildcard src/*.cpp)
HEADERS := $(wildcard src/*.h)
JAVA_HEADERS := $(patsubst %.java,%.class.h,$(wildcard src/helper/one/profiler/*.java))
API_SOURCES := $(wildcard src/api/one/profiler/*.java)
CONVERTER_SOURCES := $(shell find src/converter -name '*.java')

ifeq ($(JAVA_HOME),)
  export JAVA_HOME:=$(shell java -cp . JavaHome)
endif

ifeq ($(findstring release,$(MAKECMDGOALS)),release)
  JAVAC_RELEASE_VERSION=6
else
  JAVAC_RELEASE_VERSION=7
endif

OS:=$(shell uname -s)
ifeq ($(OS), Darwin)
  CXXFLAGS += -D_XOPEN_SOURCE -D_DARWIN_C_SOURCE
  INCLUDES += -I$(JAVA_HOME)/include/darwin
  SOEXT=dylib
  OS_TAG=macos
else
  LIBS += -lrt
  INCLUDES += -I$(JAVA_HOME)/include/linux
  SOEXT=so
  ifeq ($(findstring musl,$(shell ldd /bin/ls)),musl)
    OS_TAG=linux-musl
  else
    OS_TAG=linux
  endif
endif

ARCH:=$(shell uname -m)
ifeq ($(ARCH),x86_64)
  ARCH_TAG=x64
else
  ifeq ($(findstring arm,$(ARCH)),arm)
    ARCH_TAG=arm
  else
    ifeq ($(findstring aarch64,$(ARCH)),aarch64)
      ARCH_TAG=aarch64
    else
      ifeq ($(ARCH),ppc64le)
        ARCH_TAG=ppc64le
      else
        ARCH_TAG=x86
      endif
    endif
  endif
endif


.PHONY: all release test clean

all: build build/$(LIB_PROFILER) build/$(JATTACH) build/$(API_JAR) build/$(CONVERTER_JAR)

release: build $(PACKAGE_NAME).tar.gz

$(PACKAGE_NAME).tar.gz: build/$(LIB_PROFILER) build/$(JATTACH) \
                        build/$(API_JAR) build/$(CONVERTER_JAR) \
                        profiler.sh LICENSE *.md
	mkdir -p $(PACKAGE_DIR)
	cp -RP build profiler.sh LICENSE *.md $(PACKAGE_DIR)
	chmod -R 755 $(PACKAGE_DIR)
	chmod 644 $(PACKAGE_DIR)/LICENSE $(PACKAGE_DIR)/*.md $(PACKAGE_DIR)/build/*.jar
	tar cvzf $@ -C $(PACKAGE_DIR)/.. $(PACKAGE_NAME)
	rm -r $(PACKAGE_DIR)

%.$(SOEXT): %.so
	-ln -s $(<F) $@

build:
	mkdir -p build

build/$(LIB_PROFILER_SO): $(SOURCES) $(HEADERS) $(JAVA_HEADERS)
	$(CXX) $(CXXFLAGS) -DPROFILER_VERSION=\"$(PROFILER_VERSION)\" $(INCLUDES) -fPIC -shared -o $@ $(SOURCES) $(LIBS)

build/$(JATTACH): src/jattach/jattach.c
	$(CC) $(CFLAGS) -DJATTACH_VERSION=\"$(PROFILER_VERSION)-ap\" -o $@ $^

build/$(API_JAR): $(API_SOURCES)
	mkdir -p build/api
	$(JAVAC) -source $(JAVAC_RELEASE_VERSION) -target $(JAVAC_RELEASE_VERSION) -d build/api $^
	$(JAR) cvf $@ -C build/api .
	$(RM) -r build/api

build/$(CONVERTER_JAR): $(CONVERTER_SOURCES) src/converter/MANIFEST.MF
	mkdir -p build/converter
	$(JAVAC) -source 7 -target 7 -d build/converter $(CONVERTER_SOURCES)
	$(JAR) cvfm $@ src/converter/MANIFEST.MF -C build/converter .
	$(RM) -r build/converter

%.class.h: %.class
	hexdump -v -e '1/1 "%u,"' $^ > $@

%.class: %.java
	$(JAVAC) -g:none -source $(JAVAC_RELEASE_VERSION) -target $(JAVAC_RELEASE_VERSION) $(*D)/*.java

test: all
	test/smoke-test.sh
	test/thread-smoke-test.sh
	test/alloc-smoke-test.sh
	test/load-library-test.sh
	echo "All tests passed"

clean:
	$(RM) -r build
