#!/usr/bin/env bash
# Copyright The async-profiler authors
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

function list-archive() {
    case "${1}" in
        *.tar.gz) tar -tzvf "${1}" --wildcards '*/bin/*' '*/lib/*' | awk '$1 ~ /^-/ { print $NF, $3 }' ;;
        *.zip)    zipinfo -l "${1}"           '*/bin/*' '*/lib/*'  | awk '$1 ~ /^-/ { print $NF, $4 }' ;;
    esac | cut -d/ -f2- | sort
}

function main() {
    if [[ ${#} -ne 2 ]]; then
        printf 'Usage: compare-binary-sizes.sh <base-archive> <treatment-archive>\n' >&2
        exit 1
    fi

    [[ ${1} =~ (linux-x64|linux-arm64|macos) ]]
    printf '### %s\n\n| File | Base (KiB) | Treatment (KiB) | Delta (KiB) | %% Change |\n| --- | ---: | ---: | ---: | ---: |\n' "${BASH_REMATCH[1]}"
    join -a1 -a2 -e0 -o '0,1.2,2.2' \
         <(list-archive "${1}") \
         <(list-archive "${2}") \
        | awk '{
            b = $2; t = $3; d = t - b; pct = b > 0 ? d * 100 / b : 0
            printf "| `%s` | %.2f | %.2f | %+.2f | %+.2f%% |\n", $1, b/1024, t/1024, d/1024, pct
        }'
}

[[ "${BASH_SOURCE[0]}" == "${0}" ]] && main "$@"
