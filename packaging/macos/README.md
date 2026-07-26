# macOS package build

This native build produces a relocatable `OpenXWA.app` and a ZIP archive for the
current Mac architecture. It downloads and builds pinned SDL3, zstd, FFmpeg,
and SDL_shadercross sources into a private build directory. Homebrew libraries
are not embedded in the application.

The default package targets macOS 13 or later. The generated application
contains only the minimal FFmpeg decoders and demuxer used by OpenXWA.

## Requirements

- macOS with Xcode or the Xcode command-line tools
- CMake 3.20 or later
- Ninja
- pkg-config
- Git and Make

The non-Apple build tools can be installed with Homebrew:

```sh
brew install cmake ninja pkg-config
```

## Build

Run from the repository root:

```sh
./packaging/macos/build-package.sh
```

On Apple silicon the output is:

```text
build/macos-package/openxwa-macos-arm64.zip
build/macos-arm64/stage/OpenXWA.app
```

The script performs an ad-hoc signature when no signing identity is supplied.
This is suitable for local testing but not for public distribution through
Gatekeeper.

The script intentionally builds only the native architecture. Build the arm64
package on an Apple silicon Mac and the x86-64 package on an Intel Mac. This
keeps every host shader tool and embedded library on the same architecture.

## Deployment target and package metadata

The defaults can be overridden through environment variables:

```sh
XWA_MACOS_DEPLOYMENT_TARGET=13.0 \
XWA_VERSION=0.1.0 \
XWA_MACOS_BUILD_VERSION=1 \
XWA_MACOS_BUNDLE_IDENTIFIER=org.openxwa.openxwa \
./packaging/macos/build-package.sh
```

The same deployment target is applied to OpenXWA and every bundled dependency.
Compatibility must be tested on a clean installation of the oldest supported
macOS version.

## Developer ID signing and notarization

Store notarization credentials once:

```sh
xcrun notarytool store-credentials XWA_NOTARY \
  --apple-id "developer@example.com" \
  --team-id "TEAMID" \
  --password "app-specific-password"
```

Then build, sign, and notarize:

```sh
XWA_MACOS_SIGN_IDENTITY="Developer ID Application: Name (TEAMID)" \
XWA_MACOS_NOTARY_PROFILE=XWA_NOTARY \
./packaging/macos/build-package.sh
```

The script signs embedded Mach-O libraries before the application, enables the
hardened runtime for Developer ID builds, submits the ZIP with `notarytool`,
staples the accepted ticket, validates it, and recreates the final archive.
