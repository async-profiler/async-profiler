# shellcheck shell=bash
# Shared setup for devscripts. Source this, don't execute it.

[[ -n "${_DEVSCRIPTS_COMMON_SOURCED:-}" ]] && return
_DEVSCRIPTS_COMMON_SOURCED=1

_PROJECT_DIR="$(readlink -f "$(dirname "${BASH_SOURCE[0]}")/..")"
_COMMIT_TAG="${COMMIT_TAG:-$(git -C "${_PROJECT_DIR}" rev-parse --short=7 HEAD)}"

get_project_dir() { echo "${_PROJECT_DIR}"; }
invoke_make() { make -C "${_PROJECT_DIR}" --no-print-directory COMMIT_TAG="${_COMMIT_TAG}" "$@"; }
