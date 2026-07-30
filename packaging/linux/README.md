# Linux build

This Docker build produces a relocatable OpenXWA package containing the
executable, application resources, private shared libraries, and Vulkan SPIR-V
shaders. SDL3, zstd, FFmpeg, and SDL_shadercross are downloaded and built
inside Docker; they are not required on the host.

The package requires glibc 2.35 and the GCC 12 runtime or newer on the host
(Ubuntu 22.04 era). It uses the Linux host's Vulkan driver, window system,
input devices, and audio services. The GCC runtime is intentionally not
bundled; see the Dockerfile for the rationale.

The build requires the Docker Buildx plugin.

## Build a release package

Run from the repository root:

```sh
docker buildx build \
  --platform linux/amd64 \
  --file packaging/linux/Dockerfile \
  --build-arg XWA_BUILD_TYPE=Release \
  --target artifact \
  --output type=local,dest=build/artifacts \
  .
```

The output is:

```text
build/artifacts/openxwa-0.0.0-dev-linux-x86_64-release.tar.xz
```

## Build a debug package

```sh
docker buildx build \
  --platform linux/amd64 \
  --file packaging/linux/Dockerfile \
  --build-arg XWA_BUILD_TYPE=Debug \
  --target artifact \
  --output type=local,dest=build/artifacts \
  .
```

The output is:

```text
build/artifacts/openxwa-0.0.0-dev-linux-x86_64-debug.tar.xz
```

`XWA_BUILD_TYPE` defaults to `Release` and accepts only `Release` or `Debug`.
Debug packages contain compiler debug information and enable the in-game debug
UI. Release packages disable the debug UI.

`XWA_VERSION` sets the version in the package name and defaults to
`0.0.0-dev`. CI passes the short commit hash for development builds and the
tag version for releases (`--build-arg XWA_VERSION=1.2.3`).

The pinned third-party libraries and build-host tools are built in Release mode
for both configurations. Only OpenXWA changes configuration, keeping dependency
layers reusable between release and debug package builds.

Extract and run a release package with:

```sh
tar -xJf build/artifacts/openxwa-0.0.0-dev-linux-x86_64-release.tar.xz
cd openxwa-0.0.0-dev-linux-x86_64-release
./OpenXWA
```

The executable and bundled libraries use relative RPATHs, so the extracted
directory can be moved as a unit. On first launch, the game asks for the
original game-data directory and remembers the validated selection. Packaged
resources are resolved beside the executable and are not user-configured.
Original game assets are not included.

## Dependency versions

Dependency and shader-tool revisions are pinned as Docker build arguments in
the Dockerfile. Override a pin explicitly when testing an update:

```sh
docker buildx build \
  --platform linux/amd64 \
  --file packaging/linux/Dockerfile \
  --build-arg SDL_VERSION=3.4.0 \
  --target artifact \
  --output type=local,dest=build/artifacts \
  .
```
