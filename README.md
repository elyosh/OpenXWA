# OpenXWA
[![GitHub Release](https://img.shields.io/github/v/release/elyosh/OpenXWA)](https://github.com/elyosh/OpenXWA/releases/latest)

![Screenshot of the Azzameen hangar in OpenXWA](assets/openxwa_hangar.jpg)

OpenXWA is an in-progress open-source faithful re-implementation of the 1999 game *Star Wars:
X-Wing Alliance* with optional enhancements.  
It
runs the original game data on Windows, macOS, and Linux.

> [!IMPORTANT]
> OpenXWA does not include any content from the original game. A complete copy of *X-Wing Alliance* is required.
> The game can be bought on [GOG](https://www.gog.com/en/game/star_wars_xwing_alliance) or [Steam](https://store.steampowered.com/app/361670/STAR_WARS__XWing_Alliance/)

## Supported platforms

| Platform | Target | Graphics backend |
|---|---|---|
| Windows | x86-64 | Direct3D 12 or Vulkan |
| macOS | macOS 13 or later; arm64 or x86-64 | Metal |
| Linux | x86-64; glibc 2.36 or later | Vulkan |

## Current state

Every original function not dedicated to multiplayer has been reimplemented.

The digital iMUSE audio engine has been fully reimplemented.

OpenXWA remains under active development. Bugs and differences from the
original game are still likely despite this level of coverage. Multiplayer is
not implemented.

## Rendering

OpenXWA has two rendering modes. The classic renderer reproduces the original
presentation through a portable SDL3 GPU backend, without relying on
DirectDraw or the original Direct3D interfaces.

The modern renderer provides:

- native widescreen rendering at the display resolution
- cascaded directional shadows, SSAO, bloom, and motion blur
- anisotropic texture filtering
- 2x, 4x, or 8x MSAA
- AMD FidelityFX FSR 3.1 temporal anti-aliasing and upscaling
- HDR output

OpenXWA's settings are available from **OpenXWA Video Options** in the game's
Video Options menu.

Original OPT models work in both modes. The modern renderer can also load
optional replacement models, textures, interface art, and videos without
modifying the original game data.

Press `F5` while the game is running to switch between the modern and classic
views. Press `F2` to switch between a split comparison and the selected view.

## Other changes

Single-player flight can run at a higher simulation rate for smoother input and
motion on modern displays. A classic timing mode is available when the original
timing is preferred.

Flight mouse controls (absolute and relative modes) have been added.
Gamepads are supported, with force-feedback effects translated to rumble.

## Original game data

On first launch, OpenXWA asks for the directory containing the original game data and remembers the selection.
This directory must use the installed game layout (as in a GOG or Steam install).
For a raw CD copy, first merge the contents of the ALLIANCE directory into the root.

For unattended or development launches, pass the directory on the command
line:

```sh
OpenXWA --game-data /path/to/xwa-data
```

## System requirements

- a 64-bit system with a modern GPU
- the original *X-Wing Alliance* game data
- a joystick or gamepad is required for flight

Release packages include the required SDL3, FFmpeg, and compression libraries.
Keep the executable, libraries, resources, and shader directories together
when moving an installation.

## Building from source

The build requires CMake 3.20 or later, a C/C++ toolchain, SDL3 3.4, zstd,
FFmpeg, and SDL_shadercross. The packaging scripts and Dockerfiles build pinned
dependencies and are the reference for reproducible builds.

Platform-specific instructions are available for
[Windows](packaging/windows/README.md),
[macOS](packaging/macos/README.md), and
[Linux](packaging/linux/README.md).
