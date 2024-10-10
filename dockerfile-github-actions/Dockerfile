# Image for building async-profiler release packages for x64 and arm64 using github actions runner
# Stage 0: download musl and glibc 2.28 sources
FROM public.ecr.aws/lts/ubuntu:18.04

# bison, python3, gawk needed to compile glibc
RUN apt-get update && apt-get install -y --no-install-recommends \
    sudo patchelf bison python3 gawk curl make g++ openjdk-11-jdk-headless && \
    rm -rf /var/cache/apt /var/lib/apt/lists/*

# need newer glibc for node20 to run github actions
ARG glibc_version=2.28
ADD https://ftp.gnu.org/gnu/glibc/glibc-${glibc_version}.tar.gz /

RUN tar -zxf glibc-${glibc_version}.tar.gz && \
cd /glibc-${glibc_version} && \
mkdir build && cd build && \
../configure --prefix=/opt/glibc-for-node20 && \
make -j`nproc` && \
make install

# the github actions script must call patchelf on node20 to use this glibc

ARG musl_src=musl-1.2.5
ARG musl_sha256=a9a118bbe84d8764da0ea0d28b3ab3fae8477fc7e4085d90102b8596fc7c75e4

ADD https://musl.libc.org/releases/${musl_src}.tar.gz /
RUN echo ${musl_sha256} ${musl_src}.tar.gz | sha256sum -c

RUN ["/bin/bash", "-c", "\
    tar xfz ${musl_src}.tar.gz && \
    cd /${musl_src} && \
    ./configure --disable-shared --prefix=/usr/local/musl && \
    make -j`nproc` && make install && make clean && \
    ln -s /usr/include/$(arch)-linux-gnu/asm /usr/include/{asm-generic,linux} /usr/local/musl/include/"]
# Stage 1: install build tools + copy musl and glibc toolchain from the previous step
FROM public.ecr.aws/lts/ubuntu:18.04

# libicu-dev is needed for the github actions runner
RUN apt-get update && apt-get install -y --no-install-recommends \
    sudo libicu-dev patchelf curl make g++ openjdk-11-jdk-headless && \
    rm -rf /var/cache/apt /var/lib/apt/lists/*

COPY --from=0 /usr/local/musl /usr/local/musl
COPY --from=0 /opt/glibc-for-node20 /opt/glibc-for-node20
