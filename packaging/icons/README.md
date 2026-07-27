# Application icons

All platform icon assets derive from one committed master image. The artwork
must be original — extracting the icon resource from the original game
executable would redistribute copyrighted assets, which this project does
not do.

## Files

| File | Role |
|-|-|
| `packaging/icons/openxwa-icon.png` | Master image (input) |
| `packaging/icons/icon_tools.py` | Regenerates every derived asset |
| `cmake/windows/openxwa.ico` | Windows executable icon (generated, committed) |
| `cmake/macos/OpenXWA.icns` | macOS bundle icon (generated, committed) |
| `src/xwa_app/window_icon.h` | Embedded window/taskbar icon (generated, committed) |

The derived assets are committed so that builds need no image tooling; the
generated files live under `cmake/` and `src/` because those directories are
part of the Docker build context (`packaging/` is not). All icon assets stay
in this repository — Aeron only exposes the generic
`AeronConfig.window_icon_bmp` mechanism and carries no application branding.

## Regenerating

```sh
python3 packaging/icons/icon_tools.py
```

Pure Python standard library; runs on any platform. Commit the three
regenerated outputs together with the new master.

## How each platform consumes the icons

- **Windows**: `cmake/windows/xwa.rc.in` embeds `openxwa.ico` and a
  `VERSIONINFO` block (wired to `XWA_VERSION`) into `OpenXWA.exe` via
  `windres`. Shown by Explorer, the taskbar, and file Properties.
- **macOS**: `OpenXWA.icns` is added to the bundle's `Resources` and named by
  `CFBundleIconFile` in `cmake/macos/Info.plist.in`. Shown by Finder, the
  Dock, and Spotlight.
- **Window/taskbar icon at runtime** (Windows and Linux): `src/xwa_app/main.c`
  passes the BMP embedded in `src/xwa_app/window_icon.h` to Aeron through
  `AeronConfig.window_icon_bmp`; Aeron decodes it and calls
  `SDL_SetWindowIcon`. Aeron deliberately skips this on macOS: SDL implements
  it by replacing the running application's Dock icon, which would override
  the high-resolution bundle icon with this small image.
- **Linux desktop**: ELF binaries carry no icon. The runtime window icon above
  covers the taskbar; a `.desktop` file with an icon path is only meaningful
  for system-installed copies and is intentionally not shipped in the
  portable tarball.

## Providing artwork for the best quality

- **Master**: 1024x1024 PNG, 8-bit RGB or RGBA, non-interlaced, sRGB.
  Transparent background outside the icon shape.
- **Design for small sizes**: the same image is box-downscaled to every size
  down to 16x16. Favor a bold silhouette and high contrast; fine detail and
  thin strokes disappear below 32px. Check the generated `.ico` at 16px and
  32px before committing.
- **macOS conventions**: current macOS renders app icons inside a rounded
  square; supply artwork already composed in that shape (with margins around
  the canvas edge, roughly 5-10%) so it sits consistently in the Dock next to
  other applications. Test against both light and dark backgrounds.
- **Pixel-hinted small sizes**: the automatic downscale is a plain box
  filter. If the artwork warrants hand-tuned 16/32px versions, generate them
  externally and extend `icon_tools.py` to substitute the provided files for
  those sizes rather than scaling.
- **DMG appearance**: the disk-image window layout (background image, icon
  positions) is a separate, optional asset set — see "DMG Finder layout" in
  `packaging/macos/README.md`. A volume icon (`.VolumeIcon.icns`) can be
  added to the DMG staging in `packaging/macos/common.sh` if desired; it
  requires authoring on macOS because the custom-icon Finder attribute must
  be set on the staged volume.
