ARG IMAGE

FROM $IMAGE

COPY build_in_docker.sh /devtools/build_in_docker.sh

RUN mkdir -p /usr/share/man/man1 # https://github.com/debuerreotype/docker-debian-artifacts/issues/24
RUN (apk update && apk add curl moreutils wget hexdump linux-headers bash make g++ git openjdk11 maven) || true
RUN (apt update && DEBIAN_FRONTEND=noninteractive apt install -y --no-install-recommends curl git moreutils build-essential make wget bsdmainutils maven openjdk-11-jdk-headless) || true