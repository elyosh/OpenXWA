#!/bin/bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: ./packaging/build-packages.sh

Build Linux x86_64, Windows x86_64, and native macOS packages using the CI
packaging entry points. Requires macOS, Xcode command-line tools, CMake,
Ninja, pkg-config, and Docker Buildx with linux/amd64 support.

Environment:
  XWA_VERSION       Package version (default: current short Git commit hash)
  XWA_BUILD_TYPE    Release (default) or Debug

Artifacts are written to build/artifacts. macOS architecture, dependency
cache, and signing options from packaging/macos/README.md also apply.
Submodules must already be initialized. Existing source edits and submodule
checkouts are used as-is; generated build files and packages may be replaced.
Run builds sequentially because macOS builds share caches.
EOF
}

if [[ "$#" -eq 1 && ( "$1" == --help || "$1" == -h ) ]]; then
    usage
    exit 0
elif [[ "$#" -ne 0 ]]; then
    usage >&2
    exit 2
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
cd "${repo_root}"

export XWA_BUILD_TYPE="${XWA_BUILD_TYPE:-Release}"
case "${XWA_BUILD_TYPE}" in
    Release|Debug) ;;
    *) echo "XWA_BUILD_TYPE must be Release or Debug" >&2; exit 2 ;;
esac

if [[ "$(uname -s)" != Darwin ]]; then
    echo "Building all three packages requires macOS for the native DMG build." >&2
    echo "See packaging/linux/README.md and packaging/windows/README.md for the Docker builds." >&2
    exit 1
fi

for command in git docker clang cmake ninja pkg-config xcrun; do
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

# Inspect only: updating submodules could change a developer's checkout.
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

# Check the selected builder before starting dependency downloads or compilation.
docker buildx inspect --bootstrap
xcrun --find clang >/dev/null

# Keep every platform's output in the same directory, including the DMG.
export XWA_MACOS_ARTIFACT_DIR="${repo_root}/build/artifacts"

for platform in linux windows; do
    echo "Building ${platform} ${XWA_BUILD_TYPE} package (${XWA_VERSION})"
    docker buildx build \
        --platform linux/amd64 \
        -f "packaging/${platform}/Dockerfile" \
        --build-arg "XWA_VERSION=${XWA_VERSION}" \
        --build-arg "XWA_BUILD_TYPE=${XWA_BUILD_TYPE}" \
        --target artifact \
        --output type=local,dest=build/artifacts .
done

echo "Building macOS ${XWA_BUILD_TYPE} package (${XWA_VERSION})"
"${script_dir}/macos/build-package.sh"

echo "All three packages built successfully: ${repo_root}/build/artifacts"
