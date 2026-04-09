# shellcheck shell=bash
# Modular functions for devscripts. Source this, don't execute it.

[[ -n "${_DEVSCRIPTS_BUILDTOOLS_SOURCED:-}" ]] && return
_DEVSCRIPTS_BUILDTOOLS_SOURCED=1

source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/common.sh"
source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/filesize-reports.sh"

function release-build() {
    invoke_make release
}

function container-build() {
    local arch_tag
    arch_tag="$(invoke_make print-ARCH_TAG)"
    local suffix
    suffix="$(yq ".jobs.\"build-linux-${arch_tag}\".with.container-image" "$(get_project_dir)/.github/workflows/test-and-publish-nightly.yml")"
    { echo 'set -eux'; yq ".jobs.build.steps[] | select(.id == \"build\") | .run | sub(\"\\\$\\{\\{ inputs\\.platform \\}\\}\", \"linux-${arch_tag}\")" "$(get_project_dir)/.github/workflows/build.yml"; } | \
    docker run --rm -i \
        --user "$(id -u):$(id -g)" \
        -v "$(get_project_dir):/src" -w /src \
        -e "GITHUB_SHA=$(invoke_make print-COMMIT_TAG)" \
        -e GITHUB_OUTPUT=/dev/null \
        "public.ecr.aws/async-profiler/asprof-builder-${suffix}" bash
}

function reproducible-build() {
    invoke_make clean
    [[ "$(invoke_make print-OS_TAG)" == "macos" ]] && { release-build; true; } || container-build
}
