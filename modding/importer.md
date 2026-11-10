# Importer

The OpenXWA Mod Importer turns an X-Wing Alliance Upgrade installation into
an OpenXWA mod. It reads the installation's models, textures, settings,
missions, skies, menu art and sounds, and writes them in the formats that
the other pages describe. It works with XWAU and with the TIE Fighter Total
Conversion (TFTC).

The importer comes with the game, next to OpenXWA. It only reads the
installation, and never changes it.

## Importing an installation

The importer needs two folders:

- **XWAU or TFTC.** The installation to import. It must have
  `XWINGALLIANCE.EXE` at its top, a `FlightModels` folder with OPT models,
  and `Hooks.ini` or `XWAUVersion.txt`.
- **Original game.** An installation of the original X-Wing Alliance, the
  one that OpenXWA uses. It must be another folder than the one to import.

Then:

1. Start the OpenXWA Mod Importer.
2. Choose the two folders. The importer shows the name of the mod that it
   found, and how many models it has.
3. Check **Mod name** and **Mods folder**. The importer saves the mod in a
   folder of the mods folder, named after the mod.
4. Click **Import**. An import takes a few minutes.
5. When the import succeeds, the importer makes OpenXWA use the new mod. Start
   OpenXWA to play it.

The **Mods** tab of the importer lists the mods in the mods folder, and
chooses the one that OpenXWA uses, like the **Mods** screen of the game. The
change takes effect the next time OpenXWA starts. In this tab you can also:

- choose another mods folder. OpenXWA then lists the mods in that folder.
- see the folder of the selected mod, and why it can't be used if it has a
  problem.
- click **Show Folder** to open the mod's folder in Explorer or Finder.
- click **Delete...** to delete a mod that the importer made, with its
  folder. You can import it again from its XWAU or TFTC installation. If
  OpenXWA used the mod, it starts with the original game. Mods that you made
  yourself can't be deleted here: use **Show Folder** and delete the folder.

## What the importer makes

The import is a complete mod, with `mod.yaml` and the folders that
[The mod folder](mod_folder.md) describes:

| Part | Files |
|-|-|
| Models | `flight/models/<key>.glb`, for each model that the installation uses, and the settings of each model in `flight/models/<key>.yaml` |
| Effects | the effect libraries of the models' animated materials, and `effects.yaml`, in `flight/effects/` |
| Craft | `flight/object_types/`, `flight/craft_types/`, `flight/hud.yaml` and `flight/sounds.yaml` |
| Missions | the mission files in `flight/missions/`, the `.TIE` files, mission lists, sounds and music in `game/`, the list of removed original sounds in `game/files.yaml`, and `flight/flight.yaml` |
| Skies | the cube images and `flight/environments/environments.yaml` |
| Hangars | `flight/hangars.yaml` |
| Menus | the menu art, room animations, movies, fonts and awards in `frontend/` |
| Flight art | the image groups in `resdata/`, and the flight fonts |
| Text | `text/strings.yaml` |

## When an import stops

An import stops at the first error, such as a model that can't be
converted or a setting that the mod formats can't hold. The importer then
lists what went wrong. The first message usually explains the problem.

`mod.yaml` says `complete: false` until the import finishes, so OpenXWA
doesn't use a mod whose import stopped or was cancelled. Fix the problem,
then import again into the same folder.

The full log is in `import.log`, in the mod folder. It ends with a list of
the installation's features that the import doesn't convert, with the
number of times that it found each one, such as:

```text
xwau: the import doesn't convert OPT LODs after LOD 0 (941 times)
xwau: the import doesn't convert the .mat key NoBloom (706 times)
Import finished in 4m 02s: 0 failures, 31 warnings
```

These are warnings: the mod works without them.

[XWAU and TFTC support](xwau_support.md) lists the features that OpenXWA
supports.

## Keep authoring files

**Keep authoring files** also saves the sources of the models and of the
effects, so that you can edit them in Blender. The importer saves them in
the **Authoring folder**, `<mod folder>-authoring` by default. It needs
about as much disk space as the mod.

The authoring folder is a project of the Blender add-on, with the layout
that [Tools](tools.md#projects) describes:

```text
my-mod-authoring/
  models/<key>/<key>.gltf    the models, with their textures
  effects/<library>/         the effect libraries
  effects/effects.yaml
  import.json                what the importer wrote
```

The importer cooks the mod's models and effects from this folder. You can
also build the mod from it yourself, with `aeron_gltf_cook --build-project`.
See [Tools](tools.md#aeron_gltf_cook).

## Importing again

Import again into the same folder when a new version of the installation
comes out, or to finish a stopped import. The importer asks before it
replaces a mod that it made. It never replaces a folder that holds
something else.

The new import deletes the old mod and makes a new one. Changes that you
made in the mod folder, such as in its YAML files, are lost. Keep a copy of
the files that you changed, and copy your changes back after the import.

The authoring folder keeps your work. `import.json` lists each model and
effect library that the importer wrote, with the size and time of each of
its files. When you import again:

| The entry is | The importer |
|-|-|
| as the importer wrote it | replaces it with the new version |
| changed since the importer wrote it | keeps it, and cooks your version |
| missing | imports it |
| one that the importer didn't write, such as a model of your own | keeps it, and cooks it into the mod |

So your edited models and new models stay in the mod after an update. To
get the new version of a model that you changed, delete its folder in the
authoring folder before you import.

## The command line

`xwa_mod_import` does the same import from the command line. It is built
from the source code, with the other tools.

```sh
xwa_mod_import --source /path/to/XWAU --mod /path/to/mods/my-mod \
    --authoring /path/to/my-mod-authoring
```

| Option | Meaning |
|-|-|
| `--source <folder>` | Required. The installation to import. |
| `--mod <folder>` | Required. The mod folder. When it isn't empty, it must hold a mod that the importer made, and you must give `--replace`. |
| `--game <folder>` | The original game. The default is the game folder of OpenXWA's settings. |
| `--name <text>` | The mod's name. The default is the name that the installation gives itself. |
| `--authoring <folder>` | Keeps the authoring files in this folder, as **Keep authoring files** does. |
| `--replace` | Replaces the mod in the mod folder. |
| `--details` | Writes more detail in the log, such as a line for each model. |
| `--jobs <N>` | The number of models converted at once. The default is half the processor cores. |
| `--zstd-level <N>` | The compression level of the models' textures, 1 to 22. The default is 9. |
| `--only <key>` | Imports only this model, given by its OPT name in lower case. You can give it several times. |

The log goes to the console and to `import.log`. Ctrl+C cancels the
import. The tool ends with 0 when the import finished, 1 when it stopped or
was cancelled, and 2 when the command line is wrong.
