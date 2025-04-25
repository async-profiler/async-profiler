FROM amazonlinux:2

RUN amazon-linux-extras enable python3.8

RUN yum update -y && yum install -y git make python38 gcc10 gcc10-c++ binutils tar

RUN curl -L --output node.tar.gz https://github.com/nodejs/node/archive/refs/tags/v22.15.0.tar.gz && \
    mkdir /node && \
    tar xf node.tar.gz -C /node --strip-components=1
WORKDIR /node

ENV CC=gcc10-cc
ENV CXX=gcc10-c++
RUN ./configure
RUN make -j4 -s > /dev/null
RUN make install

FROM amazonlinux:2

COPY --from=0 /usr/local/bin/node /usr/local/bin/node
RUN yum update -y && \
    yum install -y gcc-c++ binutils make java-11-amazon-corretto patchelf tar && \
    yum clean all
