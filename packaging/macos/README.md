# macOS package build

This native build produces a relocatable `OpenXWA.app` and a DMG disk image
for the current Mac architecture. It downloads and builds pinned SDL3, zstd,
FFmpeg, and SDL_shadercross sources into a private build directory. Homebrew
libraries are not embedded in the application.

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
build/artifacts/openxwa-0.0.0-dev-macos-arm64-release.dmg
build/cache/macos-arm64/stage-release/OpenXWA.app
```

`build/artifacts` holds final packages; `build/cache/macos-<architecture>`
holds the dependency sources, install prefixes, and build trees. The cache is
safe to delete but expensive to rebuild — it includes the vendored DXC
compiler.

The DMG volume contains `OpenXWA.app` and an `/Applications` symlink for
drag-and-drop installation. It is an APFS image with LZMA compression, which
requires macOS 10.15 or later to open — well below the 13.0 deployment target.

The script performs an ad-hoc signature when no signing identity is supplied,
and leaves the DMG itself unsigned in that case. This is suitable for local
testing but not for public distribution through Gatekeeper.

## Build variants

`XWA_BUILD_TYPE` defaults to `Release` and accepts only `Release` or `Debug`,
matching the Linux and Windows packages:

```sh
XWA_BUILD_TYPE=Debug ./packaging/macos/build-package.sh
```

The Debug output is `build/artifacts/openxwa-<version>-macos-arm64-debug.dmg`.
Debug packages enable the in-game debug UI and include an `OpenXWA.dSYM`
debug-symbol bundle beside the application in the DMG; on Apple platforms
DWARF stays in the object files, so the dSYM — validated against the
executable's UUID — is what makes crash reports symbolizable. Dependencies are
always built in Release mode; only OpenXWA changes configuration, so both
variants share the cached dependency builds.

The script intentionally builds only the native architecture. Build the arm64
package on an Apple silicon Mac and the x86-64 package on an Intel Mac. This
keeps every host shader tool and embedded library on the same architecture.

## Deployment target and package metadata

The defaults can be overridden through environment variables:

```sh
XWA_MACOS_DEPLOYMENT_TARGET=13.0 \
XWA_VERSION=1.2.3 \
XWA_MACOS_BUILD_VERSION=1 \
XWA_MACOS_BUNDLE_IDENTIFIER=org.openxwa.openxwa \
./packaging/macos/build-package.sh
```

`XWA_VERSION` defaults to `0.0.0-dev` and appears in both the package name and
the application bundle version. CI passes the short commit hash for
development builds and the tag version for releases.

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

For non-interactive environments such as CI, the credentials can be passed
directly instead of through a stored keychain profile:

```sh
XWA_MACOS_SIGN_IDENTITY="Developer ID Application: Name (TEAMID)" \
XWA_MACOS_NOTARY_APPLE_ID="developer@example.com" \
XWA_MACOS_NOTARY_TEAM_ID="TEAMID" \
XWA_MACOS_NOTARY_PASSWORD="app-specific-password" \
./packaging/macos/build-package.sh
```

The script signs embedded Mach-O libraries before the application and enables
the hardened runtime for Developer ID builds. Notarization then runs in two
passes: the application is submitted first (as a temporary ZIP) and stapled,
so a copy dragged out of the disk image validates offline; the DMG built from
the stapled application is then signed, submitted, and stapled itself. The
second submission is quick because its contents are already known to Apple.

## Signing a CI-built package

No signing material is stored on GitHub. The release workflow attaches an
ad-hoc-signed DMG to a draft GitHub release; a maintainer then re-signs and
notarizes that exact artifact locally and replaces the release asset:

```sh
gh release download v1.2.3 --pattern 'openxwa-*-macos-arm64-*.dmg' \
  --dir /tmp/openxwa
XWA_MACOS_SIGN_IDENTITY="Developer ID Application: Name (TEAMID)" \
XWA_MACOS_NOTARY_PROFILE=XWA_NOTARY \
XWA_MACOS_RELEASE_TAG=v1.2.3 \
./packaging/macos/sign-package.sh /tmp/openxwa/openxwa-1.2.3-macos-arm64-release.dmg
```

Run the same command per DMG when signing both variants; a debug-symbol
bundle packaged in a Debug DMG is preserved.

`sign-package.sh` mounts the disk image, extracts `OpenXWA.app`, re-signs it
with the hardened runtime, runs the same two-pass notarization and stapling as
a local Developer ID build, and rebuilds the DMG in place. When
`XWA_MACOS_RELEASE_TAG` is set it replaces the release asset with
`gh release upload --clobber`; publish the draft release afterwards. The
direct-credential notary variables work here as well. Signing does not rebuild
the application, so the released binary is the one CI produced.

## Icons

The bundle embeds `cmake/macos/OpenXWA.icns`, referenced by
`CFBundleIconFile` in `cmake/macos/Info.plist.in`. See
`packaging/icons/README.md` for regenerating icon assets from the master
image.

## DMG Finder layout (optional)

By default the DMG opens as a plain Finder window. To ship a styled window
(background image, positioned icons), add a pre-baked Finder metadata file:

```text
packaging/macos/dmg/DS_Store            copied to /.DS_Store in the volume
packaging/macos/dmg/background/         copied to /.background/ in the volume
```

Both are optional and applied only when present. To author them once:

1. Build a DMG, convert it to a writable image
   (`hdiutil convert openxwa-macos-arm64.dmg -format UDRW -o rw.dmg`), and
   mount it.
2. Style the window in Finder: set the background image (pointing at
   `/Volumes/OpenXWA/.background/<image>`), icon size, and icon positions.
3. Close the window, then copy `/Volumes/OpenXWA/.DS_Store` to
   `packaging/macos/dmg/DS_Store` and the background directory to
   `packaging/macos/dmg/background/`.

The stored layout references the volume by name, so it stays valid only while
the volume name remains `OpenXWA`.

## Continuous integration

`.github/workflows/ci.yml` builds this package on GitHub-hosted Apple silicon
runners with ad-hoc signing, caching the pinned dependency sources and build
trees keyed on the build script and the shadercross patch. On version tags,
`.github/workflows/release.yml` attaches the ad-hoc DMG to a draft GitHub
release. Releases are then finalized locally with `sign-package.sh` as
described above.
