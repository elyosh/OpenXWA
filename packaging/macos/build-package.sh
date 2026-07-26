#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

sdl_version="${SDL_VERSION:-3.4.0}"
zstd_version="${ZSTD_VERSION:-1.5.7}"
ffmpeg_version="${FFMPEG_VERSION:-8.0.1}"
shadercross_commit="${SHADERCROSS_COMMIT:-6b06e55c7c5d7e7a09a8a14f76e866dcfad5ab99}"
deployment_target="${XWA_MACOS_DEPLOYMENT_TARGET:-13.0}"
architecture="${XWA_MACOS_ARCHITECTURE:-$(uname -m)}"
application_version="${XWA_VERSION:-0.1.0}"
build_version="${XWA_MACOS_BUILD_VERSION:-1}"
bundle_identifier="${XWA_MACOS_BUNDLE_IDENTIFIER:-org.openxwa.openxwa}"
sign_identity="${XWA_MACOS_SIGN_IDENTITY:--}"
notary_profile="${XWA_MACOS_NOTARY_PROFILE:-}"

work_root="${XWA_MACOS_BUILD_ROOT:-${repo_root}/build/macos-${architecture}}"
artifact_dir="${XWA_MACOS_ARTIFACT_DIR:-${repo_root}/build/macos-package}"
source_root="${work_root}/sources"
dependency_prefix="${work_root}/dependencies"
tool_prefix="${work_root}/tools"
build_root="${work_root}/build"
stage_root="${work_root}/stage"
application="${stage_root}/OpenXWA.app"
archive="${artifact_dir}/openxwa-macos-${architecture}.zip"

jobs=""

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Required command not found: $1" >&2
        exit 1
    fi
}

clone_tag() {
    local repository="$1"
    local tag="$2"
    local destination="$3"

    if [[ ! -d "${destination}/.git" ]]; then
        if [[ -e "${destination}" ]]; then
            echo "Source path exists but is not a Git checkout: ${destination}" >&2
            exit 1
        fi
        git clone --branch "${tag}" --depth 1 "${repository}" "${destination}"
    fi
}

prepare_shadercross() {
    local destination="${source_root}/SDL_shadercross"
    local patch="${repo_root}/packaging/common/shadercross-fsr3.patch"
    local checkout

    if [[ ! -d "${destination}/.git" ]]; then
        if [[ -e "${destination}" ]]; then
            echo "Source path exists but is not a Git checkout: ${destination}" >&2
            exit 1
        fi
        git clone https://github.com/libsdl-org/SDL_shadercross.git "${destination}"
        git -C "${destination}" checkout "${shadercross_commit}"
    fi

    checkout="$(git -C "${destination}" rev-parse HEAD)"
    if [[ "${checkout}" != "${shadercross_commit}" ]]; then
        echo "SDL_shadercross checkout is ${checkout}, expected ${shadercross_commit}" >&2
        exit 1
    fi

    if git -C "${destination}" apply --check "${patch}" >/dev/null 2>&1; then
        git -C "${destination}" apply "${patch}"
    elif ! git -C "${destination}" apply --reverse --check "${patch}" >/dev/null 2>&1; then
        echo "SDL_shadercross patch cannot be applied cleanly" >&2
        exit 1
    fi

    git -C "${destination}" submodule update --init --recursive
}

build_dependencies() {
    local common_cmake=(
        -G Ninja
        -DCMAKE_BUILD_TYPE=Release
        "-DCMAKE_OSX_ARCHITECTURES=${architecture}"
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=${deployment_target}"
    )

    clone_tag https://github.com/libsdl-org/SDL.git \
        "release-${sdl_version}" "${source_root}/SDL-${sdl_version}"
    clone_tag https://github.com/facebook/zstd.git \
        "v${zstd_version}" "${source_root}/zstd-${zstd_version}"
    clone_tag https://github.com/FFmpeg/FFmpeg.git \
        "n${ffmpeg_version}" "${source_root}/FFmpeg-${ffmpeg_version}"

    cmake -S "${source_root}/SDL-${sdl_version}" \
        -B "${build_root}/sdl" \
        "${common_cmake[@]}" \
        "-DCMAKE_INSTALL_PREFIX=${dependency_prefix}" \
        -DSDL_HIDAPI_LIBUSB=OFF \
        -DSDL_INSTALL_DOCS=OFF \
        -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF \
        -DSDL_TESTS=OFF
    cmake --build "${build_root}/sdl" --parallel "${jobs}"
    cmake --install "${build_root}/sdl"

    cmake -S "${source_root}/zstd-${zstd_version}/build/cmake" \
        -B "${build_root}/zstd" \
        "${common_cmake[@]}" \
        "-DCMAKE_INSTALL_PREFIX=${dependency_prefix}" \
        -DZSTD_BUILD_PROGRAMS=OFF \
        -DZSTD_BUILD_SHARED=ON \
        -DZSTD_BUILD_STATIC=OFF \
        -DZSTD_BUILD_TESTS=OFF
    cmake --build "${build_root}/zstd" --parallel "${jobs}"
    cmake --install "${build_root}/zstd"

    mkdir -p "${build_root}/ffmpeg"
    (
        cd "${build_root}/ffmpeg"
        "${source_root}/FFmpeg-${ffmpeg_version}/configure" \
            "--prefix=${dependency_prefix}" \
            --target-os=darwin \
            "--arch=${architecture}" \
            --cc=clang \
            --cxx=clang++ \
            --disable-autodetect \
            --disable-avdevice \
            --disable-avfilter \
            --disable-debug \
            --disable-doc \
            --disable-everything \
            --disable-network \
            --disable-programs \
            --disable-static \
            --enable-decoder=adpcm_vima \
            --enable-decoder=sanm \
            --enable-demuxer=smush \
            --enable-pic \
            --enable-pthreads \
            --enable-shared \
            "--extra-cflags=-arch ${architecture} -mmacosx-version-min=${deployment_target}" \
            "--extra-ldflags=-arch ${architecture} -mmacosx-version-min=${deployment_target}"
        make -j"${jobs}"
        make install
    )
}

build_shadercross() {
    prepare_shadercross

    cmake -S "${source_root}/SDL_shadercross" \
        -B "${build_root}/shadercross" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        "-DCMAKE_INSTALL_PREFIX=${tool_prefix}" \
        "-DCMAKE_PREFIX_PATH=${dependency_prefix}" \
        "-DCMAKE_OSX_ARCHITECTURES=${architecture}" \
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=${deployment_target}" \
        -DSDLSHADERCROSS_CLI=ON \
        -DSDLSHADERCROSS_DXC=ON \
        -DSDLSHADERCROSS_INSTALL=ON \
        -DSDLSHADERCROSS_INSTALL_RUNTIME=OFF \
        -DSDLSHADERCROSS_SHARED=ON \
        -DSDLSHADERCROSS_SPIRVCROSS_SHARED=ON \
        -DSDLSHADERCROSS_STATIC=OFF \
        -DSDLSHADERCROSS_VENDORED=ON
    cmake --build "${build_root}/shadercross" --parallel "${jobs}"
    cmake --install "${build_root}/shadercross"
}

build_application() {
    export PKG_CONFIG_PATH="${dependency_prefix}/lib/pkgconfig"

    cmake -S "${repo_root}" \
        -B "${build_root}/xwa" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        "-DCMAKE_INSTALL_PREFIX=${stage_root}" \
        "-DCMAKE_PREFIX_PATH=${dependency_prefix}" \
        "-DCMAKE_OSX_ARCHITECTURES=${architecture}" \
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=${deployment_target}" \
        "-DAERON_SHADERCROSS_EXECUTABLE=${build_root}/shadercross/shadercross" \
        -DXWA_BUILD_TOOLS=OFF \
        -DXWA_ENABLE_DEBUG_UI=OFF \
        "-DXWA_MACOS_BUILD_VERSION=${build_version}" \
        "-DXWA_MACOS_BUNDLE_IDENTIFIER=${bundle_identifier}" \
        "-DXWA_VERSION=${application_version}"
    cmake --build "${build_root}/xwa" --target xwa --parallel "${jobs}"

    cmake -E remove_directory "${stage_root}"
    cmake --install "${build_root}/xwa"
}

stage_licenses() {
    local license_dir="${application}/Contents/Resources/licenses"

    mkdir -p "${license_dir}"
    cp "${source_root}/SDL-${sdl_version}/LICENSE.txt" "${license_dir}/SDL3.txt"
    cp "${source_root}/zstd-${zstd_version}/LICENSE" "${license_dir}/zstd.txt"
    cp "${source_root}/FFmpeg-${ffmpeg_version}/COPYING.LGPLv2.1" \
        "${license_dir}/FFmpeg-LGPL-2.1.txt"
    cp "${repo_root}/aeron/third_party/bc7enc/LICENSE" "${license_dir}/bc7enc.txt"
    cp "${repo_root}/aeron/third_party/fidelityfx-fsr3/LICENSE.txt" \
        "${license_dir}/FidelityFX-FSR3.txt"
    cp "${repo_root}/aeron/third_party/libyaml/License" "${license_dir}/libyaml.txt"
    cp "${repo_root}/packaging/macos/PACKAGE-README.md" \
        "${application}/Contents/Resources/README.md"
}

validate_bundle_paths() {
    local item
    local invalid=0

    while IFS= read -r item; do
        if file "${item}" | grep -q "Mach-O"; then
            if otool -L "${item}" |
                    grep -E -q "${dependency_prefix}|${tool_prefix}|/opt/homebrew|/usr/local"; then
                echo "Non-relocatable library reference in ${item}:" >&2
                otool -L "${item}" >&2
                invalid=1
            fi
        fi
    done < <(find "${application}" -type f -print)

    if [[ "${invalid}" -ne 0 ]]; then
        exit 1
    fi
}

sign_bundle() {
    local sign_arguments=(--force --sign "${sign_identity}")
    local item

    if [[ "${sign_identity}" != "-" ]]; then
        sign_arguments+=(--options runtime --timestamp)
    fi

    while IFS= read -r item; do
        if file "${item}" | grep -q "Mach-O"; then
            codesign "${sign_arguments[@]}" "${item}"
        fi
    done < <(find "${application}/Contents/Frameworks" -type f -print)

    codesign "${sign_arguments[@]}" "${application}"
    codesign --verify --deep --strict --verbose=2 "${application}"
}

create_archive() {
    mkdir -p "${artifact_dir}"
    rm -f "${archive}"
    ditto -c -k --sequesterRsrc --keepParent "${application}" "${archive}"
}

main() {
    local host_architecture

    for command in clang cmake codesign ditto file git make ninja otool \
            pkg-config plutil sysctl xcrun; do
        require_command "${command}"
    done

    host_architecture="$(uname -m)"
    if [[ "${architecture}" != "${host_architecture}" ]]; then
        echo "This package script builds native host tools and target binaries together." >&2
        echo "Run it on a ${architecture} Mac instead of ${host_architecture}." >&2
        exit 1
    fi
    jobs="$(sysctl -n hw.logicalcpu)"

    mkdir -p "${source_root}" "${dependency_prefix}" "${tool_prefix}" \
        "${build_root}" "${artifact_dir}"

    build_dependencies
    build_shadercross
    build_application
    stage_licenses

    test -x "${application}/Contents/MacOS/OpenXWA"
    test -f "${application}/Contents/Resources/resources/remaster/flight/render.yaml"
    test -f "${application}/Contents/Resources/shaders/hyperspace_streak.vert.msl"
    plutil -lint "${application}/Contents/Info.plist"
    validate_bundle_paths
    sign_bundle
    create_archive

    if [[ -n "${notary_profile}" ]]; then
        if [[ "${sign_identity}" == "-" ]]; then
            echo "Notarization requires XWA_MACOS_SIGN_IDENTITY" >&2
            exit 1
        fi
        xcrun notarytool submit "${archive}" \
            --keychain-profile "${notary_profile}" \
            --wait
        xcrun stapler staple "${application}"
        xcrun stapler validate "${application}"
        spctl --assess --type execute --verbose=4 "${application}"
        create_archive
    fi

    echo "Created ${archive}"
    echo "Application bundle: ${application}"
}

main "$@"
