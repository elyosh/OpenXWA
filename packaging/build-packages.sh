#!/bin/bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: ./packaging/build-packages.sh [PLATFORM...]

Build OpenXWA packages using the CI packaging entry points.

Platforms:
  linux             Linux x86_64 package, built in Docker
  windows           Windows x86_64 package, built in Docker
  macos             Native macOS DMG, built on this machine
  all               All three platforms (default when no platform is given)

You can list more than one platform, for example:
  ./packaging/build-packages.sh linux macos

Requirements:
  linux, windows    Docker Buildx with linux/amd64 support
  macos             macOS, Xcode command-line tools, CMake, Ninja, pkg-config

Environment:
  XWA_VERSION       Package version (default: current short Git commit hash)
  XWA_BUILD_TYPE    Release (default) or Debug

Artifacts are written to build/artifacts. The macOS architecture, dependency
cache, and signing options in packaging/macos/README.md also apply.
Submodules must already be initialized. The script uses your source edits and
submodule checkouts as they are, but it may replace generated build files and
packages. Don't run several builds at the same time, because macOS builds share
caches.
EOF
}

build_linux=0
build_windows=0
build_macos=0

for arg in "$@"; do
    case "${arg}" in
        -h|--help) usage; exit 0 ;;
        linux) build_linux=1 ;;
        windows) build_windows=1 ;;
        macos) build_macos=1 ;;
        all) build_linux=1; build_windows=1; build_macos=1 ;;
        *)
            echo "Unknown platform: ${arg}" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ "$#" -eq 0 ]]; then
    build_linux=1
    build_windows=1
    build_macos=1
fi

docker_platforms=()
[[ "${build_linux}" -eq 1 ]] && docker_platforms+=(linux)
[[ "${build_windows}" -eq 1 ]] && docker_platforms+=(windows)

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
cd "${repo_root}"

export XWA_BUILD_TYPE="${XWA_BUILD_TYPE:-Release}"
case "${XWA_BUILD_TYPE}" in
    Release|Debug) ;;
    *) echo "XWA_BUILD_TYPE must be Release or Debug" >&2; exit 2 ;;
esac

if [[ "${build_macos}" -eq 1 && "$(uname -s)" != Darwin ]]; then
    echo "The macOS package can only be built on macOS." >&2
    echo "Pick only linux and/or windows to build the Docker packages on this machine." >&2
    exit 1
fi

required_commands=(git)
if [[ "${#docker_platforms[@]}" -gt 0 ]]; then
    required_commands+=(docker)
fi
if [[ "${build_macos}" -eq 1 ]]; then
    required_commands+=(clang cmake ninja pkg-config xcrun)
fi
for command in "${required_commands[@]}"; do
    if ! command -v "${command}" >/dev/null 2>&1; then
        echo "Required command not found: ${command}" >&2
        exit 1
    fi
done

XWA_VERSION="${XWA_VERSION:-$(git rev-parse --short HEAD)}"
export XWA_VERSION
case "${XWA_VERSION}" in
    *[!a-zA-Z0-9._-]*|.|..)
        echo "XWA_VERSION must be a filename-safe version using letters, digits, '.', '_' or '-'." >&2
        exit 2
        ;;
esac

# Only check the submodules. Updating them could change a developer's checkout.
submodule_status="$(git submodule status --recursive)"
while IFS= read -r entry; do
    case "${entry}" in
        -*)
            echo "Submodule is not initialized: ${entry}" >&2
            echo "Initialize missing submodules manually before building packages." >&2
            exit 1
            ;;
        U*)
            echo "Resolve the submodule conflict before building: ${entry}" >&2
            exit 1
            ;;
    esac
done <<< "${submodule_status}"

# Check the tools before downloading dependencies or compiling anything.
if [[ "${#docker_platforms[@]}" -gt 0 ]]; then
    docker buildx inspect --bootstrap
fi
if [[ "${build_macos}" -eq 1 ]]; then
    xcrun --find clang >/dev/null
fi

for platform in "${docker_platforms[@]+"${docker_platforms[@]}"}"; do
    echo "Building ${platform} ${XWA_BUILD_TYPE} package (${XWA_VERSION})"
    docker buildx build \
        --platform linux/amd64 \
        -f "packaging/${platform}/Dockerfile" \
        --build-arg "XWA_VERSION=${XWA_VERSION}" \
        --build-arg "XWA_BUILD_TYPE=${XWA_BUILD_TYPE}" \
        --target artifact \
        --output type=local,dest=build/artifacts .
done

if [[ "${build_macos}" -eq 1 ]]; then
    # Put the DMG in the same directory as the other packages.
    export XWA_MACOS_ARTIFACT_DIR="${repo_root}/build/artifacts"
    echo "Building macOS ${XWA_BUILD_TYPE} package (${XWA_VERSION})"
    "${script_dir}/macos/build-package.sh"
fi

echo "Packages built successfully: ${repo_root}/build/artifacts"
