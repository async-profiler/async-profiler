FROM public.ecr.aws/amazonlinux/amazonlinux:2

RUN amazon-linux-extras enable python3.8

RUN yum update -y && yum install -y git make python38 gcc10 gcc10-c++ binutils tar

ARG node_version=20.19.1
ARG node_sha256=babcd5b9e3216510b89305e6774bcdb2905ca98ff60028b67f163eb8296b6665
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
RUN amazon-linux-extras enable python3.8 && \
    yum update -y && \
    yum install -y gcc-c++ libstdc++-static binutils make java-11-amazon-corretto patchelf tar sudo python38 && \
    yum clean all && \
    rm -rf /var/cache/yum && \
    python -m ensurepip && \
    python -m pip install gcovr

ENV NODE_JS_LOCATION=/__e/node20
RUN cat <<EOF > /root/setup.sh
#!/bin/sh
mkdir -p "$NODE_JS_LOCATION/bin"
ln --force --symbolic "/usr/local/bin/node" "$NODE_JS_LOCATION/bin/node"
EOF
RUN chmod +x /root/setup.sh
