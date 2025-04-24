FROM amazonlinux:2

RUN amazon-linux-extras enable python3.8

RUN yum update -y
RUN yum install -y git make python38 gcc10 gcc10-c++ binutils

RUN git clone --branch v22.x --depth 1 https://github.com/nodejs/node.git
ENV CC=gcc10-cc
ENV CXX=gcc10-c++
RUN cd node && ./configure
RUN cd node && make -j4 -s > /dev/null
RUN cd node && make install

FROM amazonlinux:2

COPY --from=0 /usr/local/bin/node /usr/local/bin/node
RUN yum update -y && \
    yum install -y gcc-c++ binutils make java-11-amazon-corretto.x86_64 patchelf tar && \
    yum clean all
