# OpenXWA

[![GitHub Release](https://img.shields.io/github/v/release/elyosh/OpenXWA)](https://github.com/elyosh/OpenXWA/releases/latest)
[![Visit our Discord server](https://img.shields.io/discord/1533001488391995442)](https://discord.gg/WBvYzczWfG)

![Screenshot of the Azzameen hangar in OpenXWA](assets/openxwa_hangar.jpg)

OpenXWA is an open-source reimplementation of the 1999 game *Star Wars:
X-Wing Alliance* for Windows, macOS, and Linux. It runs the original game data
natively on current systems while preserving the original experience and
offering optional modern enhancements.

> [!IMPORTANT]
> OpenXWA does not include any content from the original game. A complete
> installation of *X-Wing Alliance* is required.
>
> The game is available from
> [GOG](https://www.gog.com/en/game/star_wars_xwing_alliance) and
> [Steam](https://store.steampowered.com/app/361670/STAR_WARS__XWing_Alliance/).

## Graphics

OpenXWA offers classic and modern graphics modes. Classic mode preserves the
original game's appearance, while modern mode adds high-resolution rendering
and advanced lighting.

Modern graphics include:

- native widescreen rendering at the display resolution
- cascaded directional shadows, ambient occlusion, bloom, and motion blur
- anisotropic texture filtering
- 2x, 4x, or 8x MSAA
- AMD FidelityFX FSR 3.1.4 temporal anti-aliasing and upscaling
- HDR output

Original OPT models work in both modes. Modern mode can also load optional
replacement models, textures, interface art, and videos without modifying the
original game data.

## Smoother flight and modern controls

Single-player flight can run at a higher simulation rate for smoother motion
and more responsive input on modern displays. A classic timing mode remains
available when the original timing is preferred.

OpenXWA adds mouse flight control in both Virtual Stick and Direct modes, with
adjustable sensitivity and Y-axis inversion. Flight can be played with a mouse
and keyboard, so a joystick or gamepad is no longer required. Modern gamepads
and joysticks are also supported, with force-feedback effects translated to
gamepad rumble.

## Getting started

1. Download the latest package for your platform from
   [GitHub Releases](https://github.com/elyosh/OpenXWA/releases/latest).
2. Extract or install the package and launch OpenXWA.
3. Select the folder containing a complete *X-Wing Alliance* installation when
   prompted.
4. To fly with a mouse, open **Game Controller Options** and enable
   **Mouse Flight Control**.

OpenXWA validates and remembers the selected installation. The folder must use
the installed game layout found in a GOG or Steam installation. For a raw CD
copy, first merge the contents of the `ALLIANCE` directory into the root.

Open **Video Options**, then **OpenXWA Video Options**, to configure the
renderer and display settings. Mouse and controller settings are available
under **Game Controller Options**.

Useful shortcuts:

| Key | Action |
|---|---|
| `F5` | Switch between modern and classic graphics |
| `F2` | Switch between split comparison and the selected view |
| `Ctrl`+`Alt`+`M` | Release or recapture the mouse during flight |

For unattended or development launches, pass the game-data folder on the
command line:

```sh
OpenXWA --game-data /path/to/xwa-data
```

## Supported platforms

| Platform | Target | Graphics backend |
|---|---|---|
| Windows | x86-64 | Direct3D 12 or Vulkan |
| macOS | macOS 13 or later; arm64 or x86-64 | Metal |
| Linux | x86-64; glibc 2.35 or later | Vulkan |

## Current state

Every original function not dedicated to multiplayer has been reimplemented,
including the digital iMUSE audio engine.

OpenXWA remains under active development. Bugs and differences from the
original game are still possible despite this level of coverage. Multiplayer
is not implemented.

## OpenTIE

Fans of Totally Games' space simulators may also be interested in
[OpenTIE](https://github.com/elyosh/OpenTIE), an open-source reimplementation
of *Star Wars: TIE Fighter* for Windows, macOS, and Linux.

## Community

Join the [TotallyOpen Discord server](https://discord.gg/WBvYzczWfG) to discuss
OpenXWA, OpenTIE, development, and the Totally Games flight simulators.

## System requirements

- a 64-bit system with a modern GPU
- a complete installation of *X-Wing Alliance*
- a mouse and keyboard, gamepad, or joystick for flight

Release packages include the required runtime libraries. Keep the executable,
libraries, resources, and shader directories together when moving an
installation.

## Building from source

The build requires CMake 3.20 or later, a C/C++ toolchain, SDL3 3.4, zstd,
FFmpeg, and SDL_shadercross. Release packaging pins its dependencies and
provides the reference for reproducible builds.

Platform-specific instructions are available for
[Windows](packaging/windows/README.md),
[macOS](packaging/macos/README.md), and
[Linux](packaging/linux/README.md).
