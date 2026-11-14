# Shared helpers for the macOS packaging scripts. Sourced by
# build-package.sh and sign-package.sh after they define the globals used
# here: application, importer (the mod importer's bundle, left out of the DMG
# when it doesn't exist), artifact_dir, dmg, dmg_extras, dmg_volume_name,
# sign_identity, notary_profile, notary_apple_id, notary_team_id,
# notary_password, notary_key, notary_key_id, notary_issuer, work_root, and
# optionally dsym_bundle (a debug-symbol bundle packaged beside the
# application in the DMG).

notary_arguments=()
notarize_enabled=0
dsym_bundle="${dsym_bundle:-}"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Required command not found: $1" >&2
        exit 1
    fi
}

# Notarization uses a stored keychain profile, an App Store Connect API key,
# or an Apple ID with an app-specific password. Checked before any build work
# so that a configuration error fails early.
resolve_notary_arguments() {
    if [[ -n "${notary_profile}" ]]; then
        notary_arguments=(--keychain-profile "${notary_profile}")
        notarize_enabled=1
    elif [[ -n "${notary_key}${notary_key_id}${notary_issuer}" ]]; then
        if [[ -z "${notary_key}" || -z "${notary_key_id}" ||
                -z "${notary_issuer}" ]]; then
            echo "XWA_MACOS_NOTARY_KEY, XWA_MACOS_NOTARY_KEY_ID, and" >&2
            echo "XWA_MACOS_NOTARY_ISSUER must be provided together" >&2
            exit 1
        fi
        if [[ ! -f "${notary_key}" ]]; then
            echo "Notary API key not found: ${notary_key}" >&2
            exit 1
        fi
        notary_arguments=(--key "${notary_key}" --key-id "${notary_key_id}"
            --issuer "${notary_issuer}")
        notarize_enabled=1
    elif [[ -n "${notary_apple_id}${notary_team_id}${notary_password}" ]]; then
        if [[ -z "${notary_apple_id}" || -z "${notary_team_id}" ||
                -z "${notary_password}" ]]; then
            echo "XWA_MACOS_NOTARY_APPLE_ID, XWA_MACOS_NOTARY_TEAM_ID, and" >&2
            echo "XWA_MACOS_NOTARY_PASSWORD must be provided together" >&2
            exit 1
        fi
        notary_arguments=(--apple-id "${notary_apple_id}"
            --team-id "${notary_team_id}" --password "${notary_password}")
        notarize_enabled=1
    fi

    if [[ "${notarize_enabled}" -eq 1 && "${sign_identity}" == "-" ]]; then
        echo "Notarization requires XWA_MACOS_SIGN_IDENTITY" >&2
        exit 1
    fi
}

# Signs the bundle given, after the libraries in it.
sign_bundle() {
    local bundle="$1"
    local sign_arguments=(--force --sign "${sign_identity}")
    local item

    if [[ "${sign_identity}" != "-" ]]; then
        sign_arguments+=(--options runtime --timestamp)
    fi

    while IFS= read -r item; do
        if file "${item}" | grep -q "Mach-O"; then
            codesign "${sign_arguments[@]}" "${item}"
        fi
    done < <(find "${bundle}/Contents/Frameworks" -type f -print)

    codesign "${sign_arguments[@]}" "${bundle}"
    codesign --verify --deep --strict --verbose=2 "${bundle}"
}

create_dmg() {
    local staging="${work_root}/dmg-root"
    local attempt

    rm -rf "${staging}" "${dmg}"
    mkdir -p "${staging}" "${artifact_dir}"
    ditto "${application}" "${staging}/OpenXWA.app"
    if [[ -d "${importer}" ]]; then
        ditto "${importer}" "${staging}/OpenXWA Mod Importer.app"
    fi
    ln -s /Applications "${staging}/Applications"

    if [[ -n "${dsym_bundle}" ]]; then
        ditto "${dsym_bundle}" "${staging}/OpenXWA.dSYM"
    fi

    # Optional pre-baked Finder layout, authored once from a styled volume
    # named "${dmg_volume_name}"; see packaging/macos/README.md.
    if [[ -f "${dmg_extras}/DS_Store" ]]; then
        cp "${dmg_extras}/DS_Store" "${staging}/.DS_Store"
    fi
    if [[ -d "${dmg_extras}/background" ]]; then
        ditto "${dmg_extras}/background" "${staging}/.background"
    fi

    # hdiutil create intermittently fails with "resource busy" on hosts where
    # Spotlight or quarantine services touch the temporary volume.
    for attempt in 1 2 3; do
        if hdiutil create \
                -volname "${dmg_volume_name}" \
                -srcfolder "${staging}" \
                -fs APFS \
                -format ULMO \
                -ov "${dmg}"; then
            break
        fi
        if [[ "${attempt}" -eq 3 ]]; then
            echo "hdiutil create failed after ${attempt} attempts" >&2
            exit 1
        fi
        sleep 5
    done

    if [[ "${sign_identity}" != "-" ]]; then
        codesign --force --sign "${sign_identity}" --timestamp "${dmg}"
    fi
}

# Notarize and staple each application first so a copy dragged out of the disk
# image validates offline. The DMG built from the stapled applications is then
# submitted and stapled itself; its contents are already known to Apple, so
# the second submission is quick.
notarize_application() {
    local bundle="$1"
    local notary_zip

    notary_zip="${work_root}/notary/$(basename "${bundle}" .app).zip"
    mkdir -p "${work_root}/notary"
    rm -f "${notary_zip}"
    ditto -c -k --sequesterRsrc --keepParent "${bundle}" "${notary_zip}"
    xcrun notarytool submit "${notary_zip}" "${notary_arguments[@]}" --wait
    xcrun stapler staple "${bundle}"
    xcrun stapler validate "${bundle}"
    spctl --assess --type execute --verbose=4 "${bundle}"
}

notarize_dmg() {
    xcrun notarytool submit "${dmg}" "${notary_arguments[@]}" --wait
    xcrun stapler staple "${dmg}"
    xcrun stapler validate "${dmg}"
}
