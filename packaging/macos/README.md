# macOS package build

This native build produces a relocatable `OpenXWA.app`, the
`OpenXWA Mod Importer.app` that goes with it, and a DMG disk image for the
current Mac architecture. It downloads and builds pinned SDL3, zstd,
FFmpeg, and SDL_shadercross sources into a private build directory. Homebrew
libraries are not embedded in the application.

The default package targets macOS 13 or later. The generated application
contains only the minimal FFmpeg decoders and demuxer used by OpenXWA.

## Requirements

- macOS with Xcode or the Xcode command-line tools
- CMake 3.20 or later
- Ninja
- pkg-config
- ISPC 1.30 or later, which compiles the mod importer's BC6H encoder
- Git and Make

The non-Apple build tools can be installed with Homebrew:

```sh
brew install cmake ninja pkg-config ispc
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
build/cache/macos-arm64/stage-release/OpenXWA Mod Importer.app
```

`build/artifacts` holds final packages; `build/cache/macos-<architecture>`
holds the dependency sources, install prefixes, and build trees. The cache is
safe to delete but expensive to rebuild — it includes the vendored DXC
compiler.

The DMG volume contains `OpenXWA.app`, `OpenXWA Mod Importer.app` and an
`/Applications` symlink for drag-and-drop installation. It is an APFS image with LZMA compression, which
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

Instead of a keychain profile, you can pass the credentials in the
environment. Use either an App Store Connect API key:

```sh
XWA_MACOS_SIGN_IDENTITY="Developer ID Application: Name (TEAMID)" \
XWA_MACOS_NOTARY_KEY=/path/to/AuthKey_KEYID.p8 \
XWA_MACOS_NOTARY_KEY_ID="KEYID" \
XWA_MACOS_NOTARY_ISSUER="issuer-uuid" \
./packaging/macos/build-package.sh
```

or an Apple ID with an app-specific password:

```sh
XWA_MACOS_SIGN_IDENTITY="Developer ID Application: Name (TEAMID)" \
XWA_MACOS_NOTARY_APPLE_ID="developer@example.com" \
XWA_MACOS_NOTARY_TEAM_ID="TEAMID" \
XWA_MACOS_NOTARY_PASSWORD="app-specific-password" \
./packaging/macos/build-package.sh
```

The script signs embedded Mach-O libraries before each application and enables
the hardened runtime for Developer ID builds. Notarization then runs in two
passes: each application is submitted first (as a temporary ZIP) and stapled,
so a copy dragged out of the disk image validates offline; the DMG built from
the stapled applications is then signed, submitted, and stapled itself. The
second submission is quick because its contents are already known to Apple.

## Signing in the release workflow

`.github/workflows/release.yml` builds the DMGs with an ad-hoc signature,
then its `sign-macos` job runs `sign-package.sh` on them. Only that job can
read the signing secrets, and it runs only after you approve it. It doesn't
install Homebrew packages or restore caches, and its actions are pinned to
commits, so no third-party code runs while the private key is on the runner.
The DMGs are signed as they are: the application isn't rebuilt.

### One-time setup

1. Create a Developer ID Application certificate for CI on the Apple
   Developer site, separate from the one you use locally, so that you can
   revoke it on its own. Revoking a Developer ID certificate can stop
   already-released builds signed with it from opening. Export it from
   Keychain Access as a `.p12` file with its private key and a password.
2. In App Store Connect, under Users and Access > Integrations > App Store
   Connect API, create a team key with the Developer role. Note its key ID
   and the issuer ID, and download the `.p8` file.
3. In the repository settings, under Environments, create `macos-signing`:
   - Required reviewers: yourself.
   - Deployment branches and tags: selected branches and tags, with the tag
     rule `v*`.
   - Environment secrets:

     | Secret | Value |
     | --- | --- |
     | `MACOS_CERTIFICATE_P12` | `base64 -i certificate.p12` |
     | `MACOS_CERTIFICATE_PASSWORD` | the `.p12` password |
     | `MACOS_NOTARY_KEY_P8` | the contents of the `.p8` file |
     | `MACOS_NOTARY_KEY_ID` | the API key ID |
     | `MACOS_NOTARY_ISSUER` | the issuer ID |

   Don't add these as repository secrets: every job in every workflow can
   read repository secrets.
4. Under Rules > Rulesets, add a tag ruleset for `v*` that only you can
   bypass, so nobody else can push a tag that can use the environment.
5. Delete the local `.p12` and `.p8` files.

When you push a `v*` tag, the run waits at "Sign macOS packages" until you
approve it. The draft release then gets the notarized DMGs.

### Signing a DMG by hand

You can also sign a DMG on your Mac with the same script, for example a CI
build from another workflow:

```sh
XWA_MACOS_SIGN_IDENTITY="Developer ID Application: Name (TEAMID)" \
XWA_MACOS_NOTARY_PROFILE=XWA_NOTARY \
./packaging/macos/sign-package.sh openxwa-1.2.3-macos-arm64-release.dmg
```

The script mounts the DMG, copies out `OpenXWA.app` and
`OpenXWA Mod Importer.app`, signs them with the hardened runtime, notarizes
and staples them the same way as a local Developer ID build, and rebuilds the
DMG in place. A debug-symbol bundle in a Debug DMG is kept. With
`XWA_MACOS_RELEASE_TAG=v1.2.3`, it also replaces the DMG on that GitHub
release with `gh release upload --clobber`.

## Icons

Both bundles embed `cmake/macos/OpenXWA.icns`, referenced by
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
trees keyed on the build script and aeron's dependency versions and patches.
On version tags, `.github/workflows/release.yml` signs and notarizes the DMGs
and attaches them to a draft GitHub release, as described above.
