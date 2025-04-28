FROM public.ecr.aws/amazonlinux/amazonlinux:2

RUN amazon-linux-extras enable python3.8

RUN yum update -y && yum install -y git make python38 gcc10 gcc10-c++ binutils tar

ARG node_version=22.15.0
ARG node_sha256=4f2515e143ffd73f069916ecc5daf503e7a05166c0ae4f1c1f8afdc8ab2f8a82
RUN curl -L --output node.tar.gz https://github.com/nodejs/node/archive/refs/tags/v${node_version}.tar.gz
RUN echo ${node_sha256} node.tar.gz | sha256sum -c
RUN mkdir /node
RUN tar xf node.tar.gz -C /node --strip-components=1
WORKDIR /node

ENV CC=gcc10-cc
ENV CXX=gcc10-c++
RUN ./configure
RUN make -j4 -s > /dev/null
RUN make install

FROM public.ecr.aws/amazonlinux/amazonlinux:2

COPY --from=0 /usr/local/bin/node /usr/local/bin/node
RUN yum update -y && \
    yum install -y gcc-c++ binutils make java-11-amazon-corretto patchelf tar && \
    yum clean all && \
    python3 -m ensurepip && \
    python3 -m pip install gcovr
