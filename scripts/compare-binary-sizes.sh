#!/usr/bin/env bash
# Copyright The async-profiler authors
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

main() {
    if [[ ${#} -ne 2 ]]; then
        echo "Compare binary file sizes between 2 builds (treatment and base)"
        echo "Prints a markdown report to stdout."
        echo "Usage: compare-binary-sizes.sh <base-sha> <treatment-sha>"
        echo "Expects build artifacts at: artifacts/async-profiler-{platform}-{sha7}/"
        exit 0;
    fi
    local base_sha="${1:0:7}"
    local treatment_sha="${2:0:7}"

    echo "## Binary Size Comparison"
    echo ""
    echo "Comparing \`${base_sha}\` against \`${treatment_sha}\`"
    echo ""

    local platform treatment_dir base_dir
    for platform in linux-x64 linux-arm64 macos; do
        treatment_dir="artifacts/async-profiler-${platform}-${treatment_sha}"
        base_dir="artifacts/async-profiler-${platform}-${base_sha}"
        bsdtar xf "${treatment_dir}"/* -C "${treatment_dir}"
        bsdtar xf "${base_dir}"/* -C "${base_dir}"

        echo "### ${platform}"
        echo ""
        echo "| File | Base (KiB) | Treatment (KiB) | Delta (KiB) | % Change |"
        echo "| --- | ---: | ---: | ---: | ---: |"

        local f rel treatment_size base_size diff
        for f in $(find "${treatment_dir}" -type f \( -path '*/bin/*' -o -path '*/lib/*' \) | sort); do
            rel="${f#"${treatment_dir}"/*/}"
            treatment_size="$(stat -c%s "${f}")"
            base_size="$(stat -c%s "${base_dir}"/*/"${rel}")"
            diff="$((treatment_size - base_size))"
            awk "BEGIN { printf \"| \`${rel}\` | %.2f | %.2f | %+.2f | %+d%% |\n\", \
                ${base_size}/1024, ${treatment_size}/1024, ${diff}/1024, ${diff}*100/${base_size} }"
        done
        echo ""
    done
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
