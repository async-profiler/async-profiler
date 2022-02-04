#! /bin/bash

SCRIPT_DIR=$(dirname ${BASH_SOURCE[0]})
HERE=$(cd "${SCRIPT_DIR}" &>/dev/null && printf "%s/%s" "$PWD")
ASYNC_PROFILER_DIR="${HERE}/../.."
MAVEN_DIR="${HERE}/../maven"

set -euo pipefail
IFS=$'\n\t'

# Helper functions
print_help() {
  echo "build async-profiler binary artifacts

  -a
    architecture (linux-x64, linux-x64-musl and linux-arm64 are currently supported with linux-x64 being the default)
  -t
    force the tests to be run
  -f
    force docker image rebuild
  -h
    print this help and exit
"
}

FORCE_REBUILD="no"
ARCH="linux-x64"
PLATFORM="linux/amd64"
FORCE_TESTS="no"
while getopts "a:tfh" arg; do
  case $arg in
    a)
      ARCH=${OPTARG}
      ;;
    f)
      FORCE_REBUILD="yes"
      ;;
    t)
      FORCE_TESTS="yes"
      ;;
    h)
      print_help
      exit 0
      ;;
    *)
      print_help
      exit 0
      ;;
  esac
done

case $ARCH in
  linux-x64)
    IMAGE="prantlf/alpine-glibc"
    ;;
  linux-x64-musl)
    IMAGE="alpine"
    ;;
  linux-arm64)
    IMAGE="arm64v8/alpine"
    ;;
  *)
    echo "Only linux-x64, linux-x64-musl and linux-arm64 are valid arch options."
    exit 0
    ;;
esac

TAG="async-profiler-build:$ARCH"

if [ -z "$(docker images | grep async-profiler-build | grep ${ARCH})" ] || [ "yes" = "${FORCE_REBUILD}" ];then
  docker build --build-arg IMAGE=${IMAGE} -t ${TAG} ${ASYNC_PROFILER_DIR}/datadog/docker
fi

# create the native lib arch specific directory
mkdir -p ${MAVEN_DIR}/resources/native-libs/${ARCH}

# figure out the active branch to properly set the async-profiler version
pushd ${ASYNC_PROFILER_DIR} >> /dev/null
BRANCH=$(git branch --show-current)
HASH=$(git rev-parse HEAD)
BASE_VERSION=$(mvn org.apache.maven.plugins:maven-help-plugin:2.1.1:evaluate -Dexpression=project.version | grep -E '^[0-9]+\..*')
VERSION=""
if [ "${BRANCH}" = "main" ]; then
  VERSION=${BASE_VERSION}-DD-${HASH}
else
  VERSION=${BASE_VERSION}-DD-$(echo ${BRANCH} | tr '/' '_')-${HASH}
fi
echo "=== Building Async Profiler"
echo "==    Version     : ${VERSION}"
echo "==    Architecture: ${ARCH}"
echo "==    With tests  : ${FORCE_TESTS}"
popd >> /dev/null

echo "-> Building native library"
# run the native build
docker run --rm -it --platform linux/arm64 -v ${ASYNC_PROFILER_DIR}:/data/src/async-profiler -v ${MAVEN_DIR}/resources/native-libs/${ARCH}:/data/libs -v ${HERE}/../maven/repository:/root/.m2/repository ${TAG} /devtools/build_in_docker.sh ${VERSION} ${FORCE_TESTS} > /tmp/docker.log

if [ 0 -ne $? ]; then
  cat /tmp/docker.log
  exit 1
fi

echo "-> Building maven artifact"
# now copy the master pom.xml into a temp location as not to create git tracked change
mkdir -p ${MAVEN_DIR}/tmp
cp ${MAVEN_DIR}/pom.xml ${MAVEN_DIR}/tmp/pom.xml
mvn -f ${MAVEN_DIR}/tmp/pom.xml versions:set -DnewVersion=${VERSION} > /dev/null
mvn -DskipTests "-Dasync.profiler.dir=${ASYNC_PROFILER_DIR}" "-Dnative.resource.dir=${MAVEN_DIR}/resources" --no-transfer-progress -f ${MAVEN_DIR}/tmp/pom.xml clean package > /tmp/maven.log
if [ 0 -ne $? ]; then
  cat /tmp/maven.log
  exit 1
fi

echo "-> Build done : Artifacts available |"
(cd ${HERE}/../maven/tmp/target && find $(pwd) -name '*.jar' | xargs -I {} echo "*  file://{}")