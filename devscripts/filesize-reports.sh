# shellcheck shell=bash
# Modular functions for devscripts. Source this, don't execute it.

[[ -n "${_DEVSCRIPTS_FILESIZE_REPORTS_SOURCED:-}" ]] && return
_DEVSCRIPTS_FILESIZE_REPORTS_SOURCED=1

source "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/common.sh"

function filesize-report() {
    local tmpdir
    tmpdir="$(mktemp -d)"
    trap "rm -rf '${tmpdir}'" EXIT

    local pkg_name
    pkg_name="$(invoke_make print-PACKAGE_NAME)"
    local pkg_ext
    pkg_ext="$(invoke_make print-PACKAGE_EXT)"
    local prof_ver
    prof_ver="$(invoke_make print-PROFILER_VERSION)"

    local rel_ver
    rel_ver="$(gh release view --repo async-profiler/async-profiler --json tagName --jq '.tagName[1:]')"
    local rel_pkg
    rel_pkg="${pkg_name/${prof_ver}/${rel_ver}}"

    gh release download --repo async-profiler/async-profiler -p "${rel_pkg}.${pkg_ext}" -O "${tmpdir}/release.${pkg_ext}"

    mkdir "${tmpdir}/rel_files" "${tmpdir}/loc_files"
    bsdtar xf "${tmpdir}/release.${pkg_ext}" --strip-components=1 -C "${tmpdir}/rel_files"
    bsdtar xf "${pkg_name}.${pkg_ext}" --strip-components=1 -C "${tmpdir}/loc_files"
    (cd "${tmpdir}/rel_files" && find * -type f -exec wc -c {} \; | sort -k2) > "${tmpdir}/rel"
    (cd "${tmpdir}/loc_files" && find * -type f -exec wc -c {} \; | sort -k2) > "${tmpdir}/loc"

    set +x
    echo "### ${pkg_name} vs ${rel_pkg}"
    echo "| File | Release | Local | Diff | % |"
    echo "| --- | ---: | ---: | ---: | ---: |"
    join -j2 -a1 -a2 -e 1 -o 0,1.1,2.1 "${tmpdir}/rel" "${tmpdir}/loc" | \
    while read -r file rel loc; do
        local diff
        diff="$((loc - rel))"
        local pct
        pct="$((diff * 100 / rel))"
        printf '| %s | %d KiB | %d KiB | %+d KiB | %+d%% |\n' "${file}" "$((rel/1024))" "$((loc/1024))" "$((diff/1024))" "${pct}"
    done
}

function local-filesize-report() {
    reproducible-build "${@}"
    filesize-report "${@}"
}
