ARG IMAGE=prantlf/alpine-glibc

FROM $IMAGE
ENV JAVA_HOME=/usr/lib/jvm/java-11-openjdk/

COPY build_in_docker.sh /devtools/build_in_docker.sh

RUN apk update && apk add curl moreutils wget hexdump linux-headers bash make g++ git openjdk11 maven