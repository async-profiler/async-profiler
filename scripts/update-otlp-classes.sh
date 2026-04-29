#!/usr/bin/env bash
# Copyright The async-profiler authors
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

function main() {
  local java_target otel_version pb_tag pb_java_version tmp_dir
  otel_version=$(gh api repos/open-telemetry/opentelemetry-proto/releases/latest --jq .tag_name)
  pb_tag=$(gh release view --repo protocolbuffers/protobuf --json tagName --jq .tagName)
  pb_java_version="4.${pb_tag#v}"
  tmp_dir="$(mktemp -d)"
  java_target="$(grep '^JAVA_TARGET=' Makefile | cut -d= -f2)"
  mkdir -p test/deps
  mkdir -p "${tmp_dir}"/gen/java "${tmp_dir}"/build test/gen
  curl -fL -o "test/deps/protobuf-java-${pb_java_version}.jar" \
      "https://repo1.maven.org/maven2/com/google/protobuf/protobuf-java/${pb_java_version}/protobuf-java-${pb_java_version}.jar"
  git -c advice.detachedHead=false clone --depth 1 --branch "${otel_version}" https://github.com/open-telemetry/opentelemetry-proto.git "${tmp_dir}/otel"
  (
    cd "${tmp_dir}/otel"
    # protoc will break if the results of this `find` are not split
    # shellcheck disable=SC2046
    protoc --java_out="${tmp_dir}"/gen/java \
      $(find . -type f -name '*.proto' -not \( -name 'logs*.proto' -o -name 'metrics*.proto' -o -name 'trace*.proto' -o -name '*service.proto' \))
  )
  # javac will break if the results of this `find` are not split
  # shellcheck disable=SC2046
  javac -source "${java_target}" -target "${java_target}" -Xlint:-options -cp test/deps/* -d "${tmp_dir}"/build $(find "${tmp_dir}"/gen/java -name '*.java')
  jar cf test/gen/opentelemetry-gen-classes.jar -C "${tmp_dir}"/build .
  rm -rf "${tmp_dir}"
}

[[ "${BASH_SOURCE[0]}" == "${0}" ]] && main "$@"
