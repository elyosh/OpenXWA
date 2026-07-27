#!/bin/bash

# Re-signs, notarizes, and staples a previously built OpenXWA DMG on a
# maintainer's Mac, so no signing material has to exist in CI. Takes the
# ad-hoc-signed DMG produced by build-package.sh (locally or by GitHub
# Actions), extracts the application, signs it with a Developer ID identity,
# notarizes and staples both the application and the rebuilt DMG, and
# optionally replaces the corresponding GitHub release asset.
#
# Usage:
#   sign-package.sh <input-dmg> [output-dmg]
#
# output-dmg defaults to rewriting the input in place. A debug-symbol bundle
# packaged in a Debug DMG is preserved.
#
# Required environment:
#   XWA_MACOS_SIGN_IDENTITY   Developer ID Application identity
#   XWA_MACOS_NOTARY_PROFILE  notarytool keychain profile, or the direct
#                             XWA_MACOS_NOTARY_APPLE_ID/_TEAM_ID/_PASSWORD
#                             credentials
# Optional:
#   XWA_MACOS_RELEASE_TAG     replace the DMG asset on this GitHub release
#                             with `gh release upload --clobber`

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "Usage: sign-package.sh <input-dmg> [output-dmg]" >&2
    exit 2
fi

input_dmg="$1"
output_dmg="${2:-${input_dmg}}"

sign_identity="${XWA_MACOS_SIGN_IDENTITY:--}"
notary_profile="${XWA_MACOS_NOTARY_PROFILE:-}"
notary_apple_id="${XWA_MACOS_NOTARY_APPLE_ID:-}"
notary_team_id="${XWA_MACOS_NOTARY_TEAM_ID:-}"
notary_password="${XWA_MACOS_NOTARY_PASSWORD:-}"
release_tag="${XWA_MACOS_RELEASE_TAG:-}"

work_root="$(mktemp -d "${TMPDIR:-/tmp}/openxwa-sign.XXXXXX")"
mount_point="${work_root}/mount"
mounted=0

# Globals consumed by the shared helpers.
application="${work_root}/OpenXWA.app"
artifact_dir="$(dirname "${output_dmg}")"
dmg="${output_dmg}"
dmg_volume_name="OpenXWA"
dmg_extras="${script_dir}/dmg"

# Shared signing, notarization, and DMG helpers.
source "${script_dir}/common.sh"

cleanup() {
    if [[ "${mounted}" -eq 1 ]]; then
        hdiutil detach "${mount_point}" -force >/dev/null 2>&1 || true
    fi
    rm -rf "${work_root}"
}

trap cleanup EXIT

extract_application() {
    mkdir -p "${mount_point}"
    hdiutil attach "${input_dmg}" -readonly -nobrowse \
        -mountpoint "${mount_point}"
    mounted=1

    if [[ ! -d "${mount_point}/OpenXWA.app" ]]; then
        echo "No OpenXWA.app found in ${input_dmg}" >&2
        exit 1
    fi

    ditto "${mount_point}/OpenXWA.app" "${application}"
    if [[ -d "${mount_point}/OpenXWA.dSYM" ]]; then
        ditto "${mount_point}/OpenXWA.dSYM" "${work_root}/OpenXWA.dSYM"
        dsym_bundle="${work_root}/OpenXWA.dSYM"
    fi
    hdiutil detach "${mount_point}"
    mounted=0
}

main() {
    for command in codesign ditto file hdiutil spctl xcrun; do
        require_command "${command}"
    done

    resolve_notary_arguments

    if [[ "${sign_identity}" == "-" ]]; then
        echo "XWA_MACOS_SIGN_IDENTITY must name a Developer ID identity" >&2
        exit 1
    fi
    if [[ "${notarize_enabled}" -ne 1 ]]; then
        echo "Notarization credentials are required: set" >&2
        echo "XWA_MACOS_NOTARY_PROFILE or XWA_MACOS_NOTARY_APPLE_ID," >&2
        echo "XWA_MACOS_NOTARY_TEAM_ID, and XWA_MACOS_NOTARY_PASSWORD" >&2
        exit 1
    fi
    if [[ -n "${release_tag}" ]]; then
        require_command gh
    fi
    if [[ ! -f "${input_dmg}" ]]; then
        echo "Input disk image not found: ${input_dmg}" >&2
        exit 1
    fi

    extract_application
    sign_bundle
    notarize_application
    create_dmg
    notarize_dmg

    if [[ -n "${release_tag}" ]]; then
        gh release upload "${release_tag}" "${output_dmg}" --clobber
        echo "Replaced $(basename "${output_dmg}") on release ${release_tag}"
    fi

    echo "Created ${output_dmg}"
}

main "$@"
