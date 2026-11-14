#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

# The versions and patches of aeron's dependencies; the variables build
# another version.
dependency_dir="${repo_root}/aeron/packaging/dependencies"
sdl_version="${SDL_VERSION:-$(cat "${dependency_dir}/sdl/version")}"
zstd_version="${ZSTD_VERSION:-$(cat "${dependency_dir}/zstd/version")}"
ffmpeg_version="${FFMPEG_VERSION:-$(cat "${dependency_dir}/ffmpeg/version")}"
shadercross_commit="${SHADERCROSS_COMMIT:-$(cat "${dependency_dir}/shadercross/version")}"
deployment_target="${XWA_MACOS_DEPLOYMENT_TARGET:-13.0}"
architecture="${XWA_MACOS_ARCHITECTURE:-$(uname -m)}"
application_version="${XWA_VERSION:-0.0.0-dev}"
build_version="${XWA_MACOS_BUILD_VERSION:-1}"
build_type="${XWA_BUILD_TYPE:-Release}"
cooker_enabled="${XWA_MACOS_BUILD_COOKER:-0}"
bundle_identifier="${XWA_MACOS_BUNDLE_IDENTIFIER:-org.openxwa.openxwa}"
sign_identity="${XWA_MACOS_SIGN_IDENTITY:--}"
notary_profile="${XWA_MACOS_NOTARY_PROFILE:-}"
notary_apple_id="${XWA_MACOS_NOTARY_APPLE_ID:-}"
notary_team_id="${XWA_MACOS_NOTARY_TEAM_ID:-}"
notary_password="${XWA_MACOS_NOTARY_PASSWORD:-}"
notary_key="${XWA_MACOS_NOTARY_KEY:-}"
notary_key_id="${XWA_MACOS_NOTARY_KEY_ID:-}"
notary_issuer="${XWA_MACOS_NOTARY_ISSUER:-}"

# The Debug variant enables the in-game debug UI and packages an OpenXWA.dSYM
# debug-symbol bundle beside the application. Dependencies are always built in
# Release mode; only OpenXWA changes configuration, so both variants share the
# dependency and shader-tool build trees.
case "${build_type}" in
    Release) build_variant=release; debug_ui=OFF ;;
    Debug) build_variant=debug; debug_ui=ON ;;
    *) echo "XWA_BUILD_TYPE must be Release or Debug" >&2; exit 2 ;;
esac

work_root="${XWA_MACOS_BUILD_ROOT:-${repo_root}/build/cache/macos-${architecture}}"
artifact_dir="${XWA_MACOS_ARTIFACT_DIR:-${repo_root}/build/artifacts}"
source_root="${work_root}/sources"
dependency_prefix="${work_root}/dependencies"
tool_prefix="${work_root}/tools"
build_root="${work_root}/build"
stage_root="${work_root}/stage-${build_variant}"
application="${stage_root}/OpenXWA.app"
importer="${stage_root}/OpenXWA Mod Importer.app"
dmg="${artifact_dir}/openxwa-${application_version}-macos-${architecture}-${build_variant}.dmg"
dmg_volume_name="OpenXWA"
dmg_extras="${script_dir}/dmg"
dsym_bundle=""
cooker_prefix="${work_root}/cooker-dependencies"
ispc_kernel_dir=""
cooker_dir="${artifact_dir}/aeron_gltf_cook-macos"

jobs=""
# Set in main: the SDK that xcrun reports. Its path is the MacOSX.sdk link,
# which stays the same when Xcode updates, so the cached build trees don't
# keep a versioned SDK path that the update removed.
sdk_path=""

# Shared signing, notarization, and DMG helpers.
source "${script_dir}/common.sh"

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

# Applies aeron's patches for a dependency to its cached checkout. The
# checkout is reset first, so that the patches always apply to the pristine
# source; a stamp beside it records the patches, to skip this when they
# haven't changed.
apply_dependency_patches() {
    local dependency="$1"
    local destination="$2"
    local stamp="${destination}.aeron-patches"
    local key

    key="$(cat "${dependency_dir}/${dependency}"/*.patch 2>/dev/null | shasum -a 256 | cut -d ' ' -f 1)"
    if [[ -f "${stamp}" && "$(cat "${stamp}")" == "${key}" ]]; then
        return
    fi
    git -C "${destination}" reset --hard --quiet
    git -C "${destination}" clean -fd --quiet
    "${dependency_dir}/apply-patches.sh" "${dependency_dir}/${dependency}" "${destination}"
    echo "${key}" > "${stamp}"
}

prepare_shadercross() {
    local destination="${source_root}/SDL_shadercross"
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

    apply_dependency_patches shadercross "${destination}"

    git -C "${destination}" submodule update --init --recursive
}

build_dependencies() {
    local common_cmake=(
        -G Ninja
        -DCMAKE_BUILD_TYPE=Release
        "-DCMAKE_OSX_ARCHITECTURES=${architecture}"
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=${deployment_target}"
        "-DCMAKE_OSX_SYSROOT=${sdk_path}"
    )

    clone_tag https://github.com/libsdl-org/SDL.git \
        "release-${sdl_version}" "${source_root}/SDL-${sdl_version}"
    apply_dependency_patches sdl "${source_root}/SDL-${sdl_version}"
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
            --enable-bsf=vp9_superframe_split \
            --enable-decoder=aac \
            --enable-decoder=adpcm_vima \
            --enable-decoder=h264 \
            --enable-decoder=sanm \
            --enable-decoder=vp9 \
            --enable-demuxer=matroska \
            --enable-demuxer=mov \
            --enable-demuxer=smush \
            --enable-parser=aac \
            --enable-parser=h264 \
            --enable-parser=vp9 \
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

    # LLVM_APPEND_VC_REV makes the vendored DXC configure run git describe,
    # which can fail transiently (5-second timeout) and change the generated
    # version headers, forcing a full DXC rebuild despite warm build trees.
    cmake -S "${source_root}/SDL_shadercross" \
        -B "${build_root}/shadercross" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        "-DCMAKE_INSTALL_PREFIX=${tool_prefix}" \
        "-DCMAKE_PREFIX_PATH=${dependency_prefix}" \
        "-DCMAKE_OSX_ARCHITECTURES=${architecture}" \
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=${deployment_target}" \
        "-DCMAKE_OSX_SYSROOT=${sdk_path}" \
        -DLLVM_APPEND_VC_REV=OFF \
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

# The ISPC texture compressor kernel takes minutes to compile, and it only
# changes with kernel.ispc, ispc_kernel.cmake and the ISPC version. It is
# compiled into a folder named after them, which CI keeps between runs, and
# the folders of other versions are deleted.
build_ispc_kernel() {
    local key
    key="$({ cat "${repo_root}/aeron/third_party/ispc_texcomp/kernel.ispc" \
        "${repo_root}/aeron/tools/imgbake/ispc_kernel.cmake"; ispc --version; } |
        shasum -a 256 | cut -c1-16)"
    ispc_kernel_dir="${work_root}/ispc-kernel/${key}"
    if [[ ! -d "${ispc_kernel_dir}" ]]; then
        cmake -E remove_directory "${ispc_kernel_dir}.partial"
        cmake "-DISPC=$(command -v ispc)" -DSYSTEM=Darwin "-DPROCESSOR=${architecture}" \
            -DEXTENSION=.o "-DOUT=${ispc_kernel_dir}.partial" \
            -P "${repo_root}/aeron/tools/imgbake/ispc_kernel.cmake"
        mv "${ispc_kernel_dir}.partial" "${ispc_kernel_dir}"
    fi
    find "${work_root}/ispc-kernel" -mindepth 1 -maxdepth 1 ! -name "${key}" -exec rm -rf {} +
}

# OpenXWA and the mod importer. The importer is built from the tool
# libraries, so the tools are on, and their BC6H encoder needs the ISPC
# kernel.
build_application() {
    export PKG_CONFIG_PATH="${dependency_prefix}/lib/pkgconfig"

    cmake -S "${repo_root}" \
        -B "${build_root}/xwa-${build_variant}" \
        -G Ninja \
        "-DCMAKE_BUILD_TYPE=${build_type}" \
        "-DCMAKE_INSTALL_PREFIX=${stage_root}" \
        "-DCMAKE_PREFIX_PATH=${dependency_prefix}" \
        "-DCMAKE_OSX_ARCHITECTURES=${architecture}" \
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=${deployment_target}" \
        "-DCMAKE_OSX_SYSROOT=${sdk_path}" \
        "-DAERON_SHADERCROSS_EXECUTABLE=${build_root}/shadercross/shadercross" \
        "-DAERON_ISPC_KERNEL_DIR=${ispc_kernel_dir}" \
        -DXWA_BUILD_TOOLS=ON \
        "-DXWA_ENABLE_DEBUG_UI=${debug_ui}" \
        "-DXWA_MACOS_BUILD_VERSION=${build_version}" \
        "-DXWA_MACOS_BUNDLE_IDENTIFIER=${bundle_identifier}" \
        "-DXWA_VERSION=${application_version}"
    cmake --build "${build_root}/xwa-${build_variant}" --target xwa xwa_mod_importer \
        --parallel "${jobs}"

    cmake -E remove_directory "${stage_root}"
    cmake --install "${build_root}/xwa-${build_variant}"

    # On Apple platforms DWARF stays in the object files; a distributable
    # debug package needs the linked debug info extracted into a dSYM bundle.
    if [[ "${build_type}" == Debug ]]; then
        dsym_bundle="${stage_root}/OpenXWA.dSYM"
        dsymutil "${application}/Contents/MacOS/OpenXWA" -o "${dsym_bundle}"
    fi
}

# aeron_gltf_cook and opt2gltf for the Blender add-on's macOS package (with
# XWA_MACOS_BUILD_COOKER=1, as the release does): universal binaries, with
# zstd linked statically and no SDL (aeron/tools/gltf_cook/CMakeLists.txt).
# Configuring Aeron needs SDL for both architectures, so the tools have their
# own universal dependencies.
build_cooker() {
    local universal=(
        -G Ninja
        -DCMAKE_BUILD_TYPE=Release
        "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=${deployment_target}"
        "-DCMAKE_OSX_SYSROOT=${sdk_path}"
        "-DCMAKE_INSTALL_PREFIX=${cooker_prefix}"
    )
    local arch

    cmake -S "${source_root}/SDL-${sdl_version}" \
        -B "${build_root}/cooker-sdl" \
        "${universal[@]}" \
        -DSDL_HIDAPI_LIBUSB=OFF \
        -DSDL_INSTALL_DOCS=OFF \
        -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF \
        -DSDL_TESTS=OFF
    cmake --build "${build_root}/cooker-sdl" --parallel "${jobs}"
    cmake --install "${build_root}/cooker-sdl"

    cmake -S "${source_root}/zstd-${zstd_version}/build/cmake" \
        -B "${build_root}/cooker-zstd" \
        "${universal[@]}" \
        -DZSTD_BUILD_PROGRAMS=OFF \
        -DZSTD_BUILD_SHARED=OFF \
        -DZSTD_BUILD_STATIC=ON \
        -DZSTD_BUILD_TESTS=OFF
    cmake --build "${build_root}/cooker-zstd" --parallel "${jobs}"
    cmake --install "${build_root}/cooker-zstd"

    cmake -S "${repo_root}/aeron" \
        -B "${build_root}/cooker" \
        "${universal[@]}" \
        "-DCMAKE_PREFIX_PATH=${cooker_prefix}" \
        "-DAERON_SHADERCROSS_EXECUTABLE=${build_root}/shadercross/shadercross" \
        -DAERON_BUILD_TOOLS=ON \
        -DAERON_ENABLE_VIDEO=OFF \
        -DAERON_GLTF_COOK_STANDALONE=ON
    cmake --build "${build_root}/cooker" --target aeron_gltf_cook opt2gltf --parallel "${jobs}"

    cmake -E remove_directory "${cooker_dir}"
    mkdir -p "${cooker_dir}"
    cp "${build_root}/cooker/aeron_gltf_cook" "${build_root}/cooker/opt2gltf" "${cooker_dir}/"
    cp "${source_root}/zstd-${zstd_version}/LICENSE" "${cooker_dir}/zstd.txt"

    local tool
    for tool in aeron_gltf_cook opt2gltf; do
        for arch in arm64 x86_64; do
            lipo "${cooker_dir}/${tool}" -verify_arch "${arch}"
        done
        # Per architecture, a "<file> (architecture <arch>):" line, then its
        # libraries.
        if otool -L "${cooker_dir}/${tool}" | grep -v -E ':$|^[[:space:]]+/usr/lib/'; then
            echo "${tool} links libraries outside the system's" >&2
            exit 1
        fi
    done
}

stage_licenses() {
    local bundle
    local license_dir

    for bundle in "${application}" "${importer}"; do
        license_dir="${bundle}/Contents/Resources/licenses"
        mkdir -p "${license_dir}"
        cp "${source_root}/SDL-${sdl_version}/LICENSE.txt" "${license_dir}/SDL3.txt"
        cp "${source_root}/zstd-${zstd_version}/LICENSE" "${license_dir}/zstd.txt"
        cp "${source_root}/FFmpeg-${ffmpeg_version}/COPYING.LGPLv2.1" \
            "${license_dir}/FFmpeg-LGPL-2.1.txt"
        cp "${repo_root}/aeron/third_party/bc7enc/LICENSE" "${license_dir}/bc7enc.txt"
        cp "${repo_root}/aeron/third_party/fidelityfx-fsr3/LICENSE.txt" \
            "${license_dir}/FidelityFX-FSR3.txt"
        cp "${repo_root}/aeron/third_party/libyaml/License" "${license_dir}/libyaml.txt"
    done
    # Code that only the importer uses.
    license_dir="${importer}/Contents/Resources/licenses"
    cp "${repo_root}/aeron/third_party/ispc_texcomp/LICENSE" \
        "${license_dir}/ISPC-Texture-Compressor.txt"
    cp "${repo_root}/aeron/third_party/bcdec/LICENSE" "${license_dir}/bcdec.txt"
    cp "${repo_root}/packaging/macos/PACKAGE-README.md" \
        "${application}/Contents/Resources/README.md"
}

validate_bundle_paths() {
    local bundle
    local item
    local invalid=0

    for bundle in "${application}" "${importer}"; do
        while IFS= read -r item; do
            if file "${item}" | grep -q "Mach-O"; then
                if otool -L "${item}" |
                        grep -E -q "${dependency_prefix}|${tool_prefix}|/opt/homebrew|/usr/local"; then
                    echo "Non-relocatable library reference in ${item}:" >&2
                    otool -L "${item}" >&2
                    invalid=1
                fi
            fi
        done < <(find "${bundle}" -type f -print)
    done

    if [[ "${invalid}" -ne 0 ]]; then
        exit 1
    fi
}

main() {
    local host_architecture
    local application_uuid
    local dsym_uuid

    for command in clang cmake codesign ditto file git hdiutil ispc make \
            ninja otool pkg-config plutil sysctl xcrun; do
        require_command "${command}"
    done
    if [[ "${cooker_enabled}" == 1 ]]; then
        require_command lipo
    fi
    if [[ "${build_type}" == Debug ]]; then
        require_command dsymutil
        require_command dwarfdump
    fi

    resolve_notary_arguments

    host_architecture="$(uname -m)"
    if [[ "${architecture}" != "${host_architecture}" ]]; then
        echo "This package script builds native host tools and target binaries together." >&2
        echo "Run it on a ${architecture} Mac instead of ${host_architecture}." >&2
        exit 1
    fi
    jobs="$(sysctl -n hw.logicalcpu)"
    sdk_path="$(xcrun --show-sdk-path)"

    mkdir -p "${source_root}" "${dependency_prefix}" "${tool_prefix}" \
        "${build_root}" "${artifact_dir}"

    build_dependencies
    build_shadercross
    build_ispc_kernel
    build_application
    if [[ "${cooker_enabled}" == 1 ]]; then
        build_cooker
    fi
    stage_licenses

    test -x "${application}/Contents/MacOS/OpenXWA"
    test -f "${application}/Contents/Resources/resources/remaster/config.yaml"
    test -f "${application}/Contents/Resources/resources/assets/fonts/menu.fnt"
    test -f "${application}/Contents/Resources/resources/assets/fonts/menu.ktx2"
    test -f "${application}/Contents/Resources/shaders/hyperspace_streak.vert.msl"
    test -f "${application}/Contents/Resources/shaders/hyperspace_tunnel.vert.msl"
    test -f "${application}/Contents/Resources/shaders/hyperspace_tunnel.frag.msl"
    plutil -lint "${application}/Contents/Info.plist"
    test -x "${importer}/Contents/MacOS/OpenXWA Mod Importer"
    test -f "${importer}/Contents/Resources/resources/assets/fonts/menu.fnt"
    test -f "${importer}/Contents/Resources/resources/assets/fonts/menu.ktx2"
    test -f "${importer}/Contents/Resources/shaders/fullscreen.frag.msl"
    plutil -lint "${importer}/Contents/Info.plist"
    if [[ "${build_type}" == Debug ]]; then
        application_uuid="$(dwarfdump --uuid \
            "${application}/Contents/MacOS/OpenXWA" | awk 'NR == 1 {print $2}')"
        dsym_uuid="$(dwarfdump --uuid "${dsym_bundle}" \
            | awk 'NR == 1 {print $2}')"
        if [[ -z "${application_uuid}" ||
                "${application_uuid}" != "${dsym_uuid}" ]]; then
            echo "OpenXWA.dSYM does not match the built executable" >&2
            exit 1
        fi
    fi
    validate_bundle_paths
    sign_bundle "${application}"
    sign_bundle "${importer}"

    if [[ "${notarize_enabled}" -eq 1 ]]; then
        notarize_application "${application}"
        notarize_application "${importer}"
    fi

    create_dmg

    if [[ "${notarize_enabled}" -eq 1 ]]; then
        notarize_dmg
    fi

    echo "Created ${dmg}"
    echo "Application bundles: ${application}, ${importer}"
    if [[ "${cooker_enabled}" == 1 ]]; then
        echo "Add-on tools: ${cooker_dir}"
    fi
}

main "$@"
