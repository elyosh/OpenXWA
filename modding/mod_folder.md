# The mod folder

A mod is a folder with a `mod.yaml` file at its root. This page describes
what goes in the folder, how OpenXWA finds mods and files, the units the
files use, and how to test a mod.

## Layout

```text
my-mod/
  mod.yaml              the mod's name and format version
  flight/
    flight.yaml         flight rules and settings
    hud.yaml            HUD icons and the target display
    sounds.yaml         sound banks
    hangars.yaml        hangars
    object_types/       one file per object type
    craft_types/        one file per craft type
    models/             3D models (.glb) and their settings (.yaml)
    effects/            effect libraries for animated materials
    environments/       environments.yaml and the sky images
    missions/           one file per mission
    fonts/              fonts in flight
  frontend/
    frontend.yaml       design size, paint settings and screen layouts
    animations.yaml     room animations
    animations/         the videos and sounds of room animations
    awards.yaml         medals and other awards
    concourse/          the concourse layout and starfield mask
    frontres/           menu images
    movies/             movies
    fonts/              menu fonts
  resdata/
    atlas/              the images of the game's image groups
    mission_art/        image groups for one mission
  text/
    strings.yaml        text replacements
  game/                 copies of original game files
    files.yaml          original game files that the mod removes
```

Every file and folder is optional. A mod only needs the files it changes.

## mod.yaml

```yaml
version: 8
name: "Rebel Fleet Refit"
complete: true
```

| Key | Meaning |
|-|-|
| `version` | Required. The mod format version that the mod was made for. This OpenXWA reads version 8. |
| `name` | The name that the **Mods** screen shows. The default is the folder name. |
| `complete` | `false` marks a mod that isn't ready to play. The default is `true`. |

The importer also writes `source` and `imported`: where the mod came from
and when it was imported. The game only uses `imported`, to tell the player
to import the mod again when the mod can't be used.

### The mod format version

The mod format version covers every file of a mod. Files have no version of
their own. Each change to a file format raises the version by one.

OpenXWA only uses a mod whose `version` is the version it reads. The
**Mods** screen says when a mod was made for another version.
When such a mod is chosen, OpenXWA says why it can't use it and starts with
the original game. Update the mod's files, then change `version`.

### Unfinished mods

OpenXWA doesn't use a mod with `complete: false` when it is chosen in the
options. The `--mod` option still uses it, so you can test a mod that you
are still making.

## How OpenXWA finds mods

OpenXWA lists:

- the original game, with no mod;
- each folder of the mods folder that has a `mod.yaml`, by folder name;
- the mod chosen in the options, when it is in another folder.

A folder without a `mod.yaml` isn't a mod. Only one mod is used at a time.

The mods folder is:

| System | Mods folder |
|-|-|
| Windows | `%LOCALAPPDATA%\TotallyOpen\OpenXWA\mods` |
| macOS | `~/Library/Application Support/TotallyOpen/OpenXWA/mods` |
| Linux | `~/.local/share/TotallyOpen/OpenXWA/mods` |

Players choose a mod in the options, under **Mods**. The choice takes effect
the next time OpenXWA starts.

The `--mod` option uses a mod for one launch, wherever its folder is. It
doesn't change the options:

```sh
OpenXWA --mod "/path/to/my-mod"
```

When the mod given to `--mod` can't be used, OpenXWA shows why and doesn't
start.

OpenXWA saves the choice in `config.yaml`, in its user folder:

| Key | Meaning |
|-|-|
| `paths.mods` | The mods folder, when it isn't the default one. |
| `paths.mod` | The folder of the chosen mod. |
| `paths.use_mod` | `false` plays the original game, even when `paths.mod` is set. |

## How files are found

When the game needs a file, it looks in the mod first. When the mod doesn't
have the file, the game uses its original data. So a mod that only has
`text/strings.yaml` changes the text and nothing else.

A file in the mod replaces the whole original file. The game doesn't merge
the two. Inside a mod file, most keys are optional. A missing key keeps the
game's value, unless the key's page says otherwise.

File and folder names can be written in any case. `Flight/Models/XWing.glb`
and `flight/models/xwing.glb` are the same file.

### Replacing original game files

Some files of the game folder can be replaced by putting a copy in `game/`,
at the same path as in the game folder. The copy replaces the original file.
These files can be replaced:

| Path | What it holds |
|-|-|
| `MISSIONS/*.TIE`, `MELEE/*.TIE`, `COMBAT/*.TIE` | missions |
| `MISSIONS/MISSION.LST`, `MELEE/MISSION.LST`, `COMBAT/MISSION.LST` | mission lists |
| `MISSIONS/EMAIL.TXT` | the e-mails in the concourse |
| `WAVE/...` | any sound: voices, briefing and concourse sounds, sound effects, and new sounds that other mod files name, such as `Wave/Custom/Alarm.wav` |
| `MUSIC/*.IMC` | the music |
| `FRONTRES/CUTSCENE/CUTSCENE.TXT` | the cutscenes that follow missions |
| `FRONTRES/MEDALS/MEDALS.TXT` | the missions that give the battle medals |
| `FRONTRES/MEDALS/MEDALS.LST` | the medal images |
| `FRONTRES/MEDALS/FAMILYAWARDS.TXT` | the memento texts |

Other files in `game/` are ignored. The other original files are changed
through the mod's own formats, described in the other pages.

### Removing original game files

To remove original game files, list them in `game/files.yaml`. The game
then works as if these files didn't exist. For example, if you remove a
mission's briefing narration, the briefing officer says one of their usual
lines instead:

```yaml
removed:
  - Wave/FRONTEND/B0M3/N000301.WAV
  - Wave/FRONTEND/N01ZL01.WAV
```

| Key | Meaning |
|-|-|
| `removed` | The files to remove. Write each path as you would in `game/`. You can only remove the files that `game/` can replace. If `game/` also has a copy of a file in this list, the copy isn't used either. |

The importer writes this file for an installation that is missing some of
the original sounds. TFTC, for example, removes some of the original
briefing voices.

### Replacing models

A model in the mod replaces the original model with the same name. The
name is the original file's name, without its folder and extension, in
lower case. For example, `flight/models/xwing.glb` replaces
`FLIGHTMODELS/XWING.OPT`. The settings of that model go in
`flight/models/xwing.yaml`.

Models are cooked `.glb` files, made from glTF files with the model cooker.
The models page describes how to make them.

The game uses a mod's models only when model replacements are on, which is
the default. While they are on, the game draws only in modern graphics, and
the keys that switch to classic graphics do nothing.

## Units and conventions

- **Lengths** are in metres, **times** in milliseconds, **speeds** in MGLT
  and **angles** in degrees. Keys with a unit say it in their name, such as
  `position_m`, `cooldown_ms`, `speed_mglt` or `heading_degrees`. Hull,
  shields and damage keep the game's own points. See
  [Craft](craft.md#units). Two values of a model still use the game's
  units: `hangar_doors.distance`, in game units of 1/40.96 metre, and
  `step_ticks`, in ticks of 4 ms.
- **Models** use the glTF axes: +Y is up and the nose points along +Z.
  Lengths are in metres.
- **Screen positions and sizes** in the `frontend/` and `resdata/` files are
  in design pixels: pixels of the screen size that `design` sets in
  `frontend/frontend.yaml`. The default is 640 × 480, the size of the
  original screens. An atlas file can use the game's own units instead.
- **Names** are used for game values, such as object types, weapons and
  hardpoint types: `rebel_laser`, not a number. The
  [reference pages](README.md#reference) list the accepted names, with the
  numbers that the community tools show.
- **Colours** are written in one of two ways. A list `[r, g, b]`, where 1 is
  full brightness, is linear unless the key's description says it is sRGB.
  A hex number such as `0xFF0000` is used where the game itself stores the
  colour that way.
- **Image numbers** in a group and **mission numbers** start at 1.
- **YAML files** are UTF-8. Copies of the game's text files in `game/` keep
  the game's encoding, Windows-1252.

## Testing a mod

1. Start OpenXWA with `--mod` and the mod's folder.
2. Play the part of the game that the mod changes.
3. Read the log. The game writes a warning or an error there for each
   problem it finds in a mod file, such as an unknown name or a value out of
   range.
4. Restart OpenXWA after you change a file, so that the game reads it again.

The log is in the `diagnostics` folder of the user folder. Each launch writes
a new file, named after the date and time.

| System | User folder |
|-|-|
| Windows | `%APPDATA%\TotallyOpen\OpenXWA` |
| macOS | `~/Library/Application Support/TotallyOpen/OpenXWA` |
| Linux | `~/.local/share/TotallyOpen/OpenXWA` |

The debug packages of OpenXWA also have a debug overlay. The key to the
left of 1 opens it.
