# Windows build with GCC MinGW-w64

This Docker build produces a self-contained x86-64 Windows package using GCC
MinGW-w64. SDL3, zstd, and the required FFmpeg components are downloaded and
built inside Docker; they are not vendored in this repository or required on
the host.

The build requires the Docker Buildx plugin.

## Build a release package

Run from the repository root:

```sh
docker buildx build \
  --file packaging/windows/Dockerfile \
  --build-arg XWA_BUILD_TYPE=Release \
  --target artifact \
  --output type=local,dest=build/artifacts \
  .
```

The output is:

```text
build/artifacts/openxwa-0.0.0-dev-windows-x86_64-release.zip
```

## Build a debug package

```sh
docker buildx build \
  --file packaging/windows/Dockerfile \
  --build-arg XWA_BUILD_TYPE=Debug \
  --target artifact \
  --output type=local,dest=build/artifacts \
  .
```

The output is:

```text
build/artifacts/openxwa-0.0.0-dev-windows-x86_64-debug.zip
```

`XWA_BUILD_TYPE` defaults to `Release` and accepts only `Release` or `Debug`.
Debug packages contain MinGW DWARF debug information and enable the in-game
debug UI. Release packages disable the debug UI.

`XWA_VERSION` sets the version in the package name and defaults to
`0.0.0-dev`. CI passes the short commit hash for development builds and the
tag version for releases (`--build-arg XWA_VERSION=1.2.3`).

The pinned third-party libraries and build-host tools are built in Release mode
for both configurations. Only OpenXWA changes configuration, keeping dependency
layers reusable between release and debug package builds.

After extraction, the package contains:

```text
openxwa-0.0.0-dev-windows-x86_64-release/
├── OpenXWA.exe
├── *.dll
├── licenses/
├── resources/
└── shaders/
```

The DLL set includes SDL3, the minimal FFmpeg runtime, zstd, and the GCC
MinGW-w64 runtime libraries imported by `OpenXWA.exe`. On first launch, OpenXWA
asks for the original game-data directory and remembers the validated
selection. The selected directory must contain `ALLIANCE`, `FLIGHTMODELS`, and
`MISSIONS`. Application resources are resolved from the packaged `resources`
directory; original game assets are not included in the package.

## Dependency versions

Dependency and shader-tool revisions are pinned as Docker build arguments in
the Dockerfile. Override a pin explicitly when testing an update:

```sh
docker buildx build \
  --file packaging/windows/Dockerfile \
  --build-arg SDL_VERSION=3.4.0 \
  --target artifact \
  --output type=local,dest=build/artifacts \
  .
```

## Icons

`OpenXWA.exe` embeds `cmake/windows/openxwa.ico` and a `VERSIONINFO` resource
generated from `cmake/windows/xwa.rc.in`. See `packaging/icons/README.md` for
regenerating icon assets from the master image.

## Applied dependency patches

Patches under `packaging/common/` are applied to the cloned sources before the
dependency builds. Re-verify them when a pinned revision changes; `git apply`
fails the build if a patch no longer applies.

| Patch | Applies to | Purpose |
|-|-|-|
| `shadercross-fsr3.patch` | SDL_shadercross (host) | FSR3 shader generation support |
| `sdl-gpu-d3d12-hdr.patch` | SDL3 (Windows target) | Lets the D3D12 SDL_GPU backend report and enable HDR swapchain compositions |

SDL_shadercross is a build-host tool. The Dockerfile builds it for Linux and
uses it to generate DXIL and SPIR-V while compiling OpenXWA and its runtime
dependencies for Windows. The FSR shader generator is likewise built for the
Linux host before the MinGW-w64 target build.
