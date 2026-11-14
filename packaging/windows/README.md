# Windows build with GCC MinGW-w64

This Docker build produces a self-contained x86-64 Windows package using GCC
MinGW-w64, with OpenXWA and the OpenXWA Mod Importer. It uses Debian 13's
MinGW-w64 toolchain for UCRT, the C runtime of Windows 10 and later, so the
package needs Windows 10 or later. SDL3, zstd, and the
required FFmpeg components are downloaded and built inside Docker; they are
not vendored in this repository or required on the host. ISPC's Windows release
is downloaded too, and runs under Wine: it compiles the importer's BC6H
encoder. That kernel takes minutes to compile, so it has a stage of its own,
which the build cache keeps until the kernel or the ISPC version changes.

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
build/artifacts/openxwa-0.0.0-dev-windows-x86_64-release-symbols.zip
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
debug UI. Release packages disable the debug UI. They are compiled with debug
information too, but it is removed from the programs and the bundled DLLs and
published in the separate `-symbols` archive. Each shipped file keeps a
`.gnu_debuglink` to its `.debug` file.

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
├── OpenXWA Mod Importer.exe
├── *.dll
├── licenses/
├── resources/
└── shaders/
```

The DLL set includes SDL3, the minimal FFmpeg runtime, zstd, and the GCC
MinGW-w64 runtime libraries imported by `OpenXWA.exe` and
`OpenXWA Mod Importer.exe`. The importer imports a modded X-Wing Alliance
installation into an OpenXWA mod folder, and chooses the mod that OpenXWA
uses. On first launch, OpenXWA asks for the original game-data directory and
remembers the validated selection. The selected directory must contain
`ALLIANCE`, `FLIGHTMODELS`, and `MISSIONS`. Application resources are resolved from the packaged `resources`
directory; original game assets are not included in the package.

## Crash reports

Logs, crash stacks and crash dumps are written to the `diagnostics` folder in
the user folder, `%APPDATA%\TotallyOpen\OpenXWA`. Setting
`AERON_TEST_CRASH=access|abort|stack` makes the program crash at startup, to
test this.

To find the source lines of a crash stack, extract the symbols archive of the
same build. A frame such as `OpenXWA.exe+0x2d9dbb` is an offset from the
program's base address, `0x140000000`. Add the two and pass the result to
`llvm-symbolizer`, run from the symbols folder:

```sh
llvm-symbolizer --obj=OpenXWA.exe 0x1402d9dbb
```

## Dependency versions

The versions of SDL, zstd, FFmpeg and SDL_shadercross, and the patches that
SDL and SDL_shadercross need, are aeron's: see
`aeron/packaging/dependencies/README.md`. The other dependency and
shader-tool revisions are pinned as Docker build arguments in the Dockerfile.
Set a build argument to test another version:

```sh
docker buildx build \
  --file packaging/windows/Dockerfile \
  --build-arg SDL_VERSION=3.4.18 \
  --target artifact \
  --output type=local,dest=build/artifacts \
  .
```

## Icons

`OpenXWA.exe` embeds `cmake/windows/openxwa.ico` and a `VERSIONINFO` resource
generated from `cmake/windows/xwa.rc.in`. See `packaging/icons/README.md` for
regenerating icon assets from the master image.

## Applied dependency patches

The SDL and SDL_shadercross patches are aeron's, applied to the cloned sources
before the dependency builds; `aeron/packaging/dependencies/README.md` lists
what each one fixes. `git apply` fails the build if a patch no longer applies
to the version being built.

SDL_shadercross is a build-host tool. The Dockerfile builds it for Linux and
uses it to generate DXIL and SPIR-V while compiling OpenXWA and its runtime
dependencies for Windows. The FSR shader generator is likewise built for the
Linux host before the MinGW-w64 target build.
