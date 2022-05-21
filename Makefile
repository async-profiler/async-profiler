PROFILER_VERSION=2.8

PACKAGE_NAME=async-profiler-$(PROFILER_VERSION)-$(OS_TAG)-$(ARCH_TAG)
PACKAGE_DIR=/tmp/$(PACKAGE_NAME)
RES_DIR=src/res
COMMON_RES_DIR=$(RES_DIR)/common
CPP_RES_DIR=$(RES_DIR)/cpp

LIB_PROFILER=libasyncProfiler.$(SOEXT)
LIB_PROFILER_SO=libasyncProfiler.so
JATTACH=jattach
API_JAR=async-profiler.jar
CONVERTER_JAR=converter.jar

CFLAGS=-O3
CXXFLAGS=-O3 -fno-omit-frame-pointer -fvisibility=hidden
INCLUDES=-I$(JAVA_HOME)/include -I$(COMMON_RES_DIR) -I$(CPP_RES_DIR)
LIBS=-ldl -lpthread
MERGE=true

JAVAC=$(JAVA_HOME)/bin/javac
JAR=$(JAVA_HOME)/bin/jar
JAVAC_RELEASE_VERSION=7

SOURCES := $(wildcard src/*.cpp)
HEADERS := $(wildcard src/*.h src/fdtransfer/*.h)
JAVA_HELPER_SOURCES := $(wildcard src/helper/one/profiler/*.java)
JAVA_HELPER_CLASSES := $(patsubst src/helper/%.java,$(CPP_RES_DIR)/%.class,$(JAVA_HELPER_SOURCES))
API_SOURCES := $(wildcard src/api/one/profiler/*.java)
CONVERTER_SOURCES := $(shell find src/converter -name '*.java')
CPP_RESOURCES := $(shell find $(COMMON_RES_DIR) -name '*') $(shell find $(CPP_RES_DIR) -name '*')
JAVA_RESOURCES := $(shell find $(COMMON_RES_DIR) -name '*')

ifeq ($(JAVA_HOME),)
  export JAVA_HOME:=$(shell java -cp . JavaHome)
endif

OS:=$(shell uname -s)
ifeq ($(OS),Darwin)
  CXXFLAGS += -D_XOPEN_SOURCE -D_DARWIN_C_SOURCE
  INCLUDES += -I$(JAVA_HOME)/include/darwin
  FDTRANSFER_BIN=
  SOEXT=dylib
  PACKAGE_EXT=zip
  OS_TAG=macos
  ifeq ($(FAT_BINARY),true)
    FAT_BINARY_FLAGS=-arch x86_64 -arch arm64 -mmacos-version-min=10.12
    CFLAGS += $(FAT_BINARY_FLAGS)
    CXXFLAGS += $(FAT_BINARY_FLAGS)
    PACKAGE_NAME=async-profiler-$(PROFILER_VERSION)-$(OS_TAG)
    MERGE=false
  endif
else
  CXXFLAGS += -Wl,-z,defs -Wl,-z,nodelete
  ifeq ($(MERGE),true)
    CXXFLAGS += -fwhole-program
  endif
  LIBS += -lrt
  INCLUDES += -I$(JAVA_HOME)/include/linux
  FDTRANSFER_BIN=build/fdtransfer
  SOEXT=so
  PACKAGE_EXT=tar.gz
  ifeq ($(findstring musl,$(shell ldd /bin/ls)),musl)
    OS_TAG=linux-musl
    CXXFLAGS += -D__musl__
  else
    OS_TAG=linux
  endif
endif

ARCH:=$(shell uname -m)
ifeq ($(ARCH),x86_64)
  ARCH_TAG=x64
else
  ifeq ($(findstring arm,$(ARCH)),arm)
    ifeq ($(findstring 64,$(ARCH)),64)
      ARCH_TAG=arm64
    else
      ARCH_TAG=arm32
    endif
  else
    ifeq ($(findstring aarch64,$(ARCH)),aarch64)
      ARCH_TAG=arm64
    else
      ifeq ($(ARCH),ppc64le)
        ARCH_TAG=ppc64le
      else
        ARCH_TAG=x86
      endif
    endif
  endif
endif

ifneq ($(ARCH),ppc64le)
  ifneq ($(ARCH_TAG),arm32)
    CXXFLAGS += -momit-leaf-frame-pointer
  endif
endif


.PHONY: all release test clean

all: build build/$(LIB_PROFILER) build/$(JATTACH) $(FDTRANSFER_BIN) build/$(API_JAR) build/$(CONVERTER_JAR)

release: build $(PACKAGE_NAME).$(PACKAGE_EXT)

$(PACKAGE_NAME).tar.gz: $(PACKAGE_DIR)
	tar czf $@ -C $(PACKAGE_DIR)/.. $(PACKAGE_NAME)
	rm -r $(PACKAGE_DIR)

$(PACKAGE_NAME).zip: $(PACKAGE_DIR)
	codesign -s "Developer ID" -o runtime --timestamp -v $(PACKAGE_DIR)/build/$(JATTACH) $(PACKAGE_DIR)/build/$(LIB_PROFILER_SO)
	ditto -c -k --keepParent $(PACKAGE_DIR) $@
	rm -r $(PACKAGE_DIR)

$(PACKAGE_DIR): build/$(LIB_PROFILER) build/$(JATTACH) $(FDTRANSFER_BIN) \
                build/$(API_JAR) build/$(CONVERTER_JAR) \
                profiler.sh LICENSE *.md
	mkdir -p $(PACKAGE_DIR)
	cp -RP build profiler.sh LICENSE *.md $(PACKAGE_DIR)
	chmod -R 755 $(PACKAGE_DIR)
	chmod 644 $(PACKAGE_DIR)/LICENSE $(PACKAGE_DIR)/*.md $(PACKAGE_DIR)/build/*.jar

%.$(SOEXT): %.so
	rm -f $@
	-ln -s $(<F) $@

build:
	mkdir -p build

build/$(LIB_PROFILER_SO): $(SOURCES) $(HEADERS) $(CPP_RESOURCES) $(JAVA_HELPER_CLASSES)
ifeq ($(MERGE),true)
	for f in src/*.cpp; do echo '#include "'$$f'"'; done |\
	$(CXX) $(CXXFLAGS) -DPROFILER_VERSION=\"$(PROFILER_VERSION)\" $(INCLUDES) -fPIC -shared -o $@ -xc++ - $(LIBS)
else
	$(CXX) $(CXXFLAGS) -DPROFILER_VERSION=\"$(PROFILER_VERSION)\" $(INCLUDES) -fPIC -shared -o $@ $(SOURCES) $(LIBS)
endif

build/$(JATTACH): src/jattach/*.c src/jattach/*.h
	$(CC) $(CFLAGS) -DJATTACH_VERSION=\"$(PROFILER_VERSION)-ap\" -o $@ src/jattach/*.c

build/fdtransfer: src/fdtransfer/*.cpp src/fdtransfer/*.h src/jattach/psutil.c src/jattach/psutil.h
	$(CXX) $(CFLAGS) -o $@ src/fdtransfer/*.cpp src/jattach/psutil.c

build/$(API_JAR): $(API_SOURCES)
	mkdir -p build/api
	$(JAVAC) -source $(JAVAC_RELEASE_VERSION) -target $(JAVAC_RELEASE_VERSION) -d build/api $^
	$(JAR) cf $@ -C build/api .
	$(RM) -r build/api

build/$(CONVERTER_JAR): $(CONVERTER_SOURCES) $(JAVA_RESOURCES) src/converter/MANIFEST.MF
	mkdir -p build/converter
	cp -r $(COMMON_RES_DIR)/* build/converter
	#cp -r $(JAVA_RES_DIR)/* build/converter
	$(JAVAC) -source 7 -target 7 -d build/converter $(CONVERTER_SOURCES)
	$(JAR) cfm $@ src/converter/MANIFEST.MF -C build/converter .
	$(RM) -r build/converter

$(CPP_RES_DIR)/%.class: src/helper/%.java
	$(JAVAC) -g:none -source $(JAVAC_RELEASE_VERSION) -target $(JAVAC_RELEASE_VERSION) -d $(CPP_RES_DIR) $^

test: all
	test/smoke-test.sh
	test/thread-smoke-test.sh
	test/alloc-smoke-test.sh
	test/load-library-test.sh
	test/fdtransfer-smoke-test.sh
	echo "All tests passed"

clean:
	$(RM) -r build
