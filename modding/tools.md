# Tools

Most mod files are YAML files that you write in a text editor. The tools
build the files that the game can't read as you write them, such as 3D
models, effect libraries, skies and 2D images.

| Tool | What it does |
|-|-|
| Blender add-on | Imports, edits and exports models |
| `aeron_gltf_cook` | Cooks glTF models into `.glb` files, and builds effect libraries |
| `opt2gltf` | Converts an OPT model of the original game into a glTF model |
| `environment_bake` | Bakes the images of a sky |
| `xwa_image_cook` | Builds an atlas of 2D images from PNG files |

## Getting the tools

Each OpenXWA release on
[GitHub Releases](https://github.com/elyosh/OpenXWA/releases/latest) has a
Blender add-on package for each system:

| System | Package |
|-|-|
| Windows | `aeron_flight_model-<version>-windows-x64.zip` |
| macOS | `aeron_flight_model-<version>-macos.zip` |
| Linux | `aeron_flight_model-<version>-linux-x64.zip` |

The package holds `aeron_gltf_cook` and `opt2gltf` in its `bin` folder. The
add-on runs them for you, and you can also run them from the command line.
Use the package from the same release as your game, because the `.glb`
files that the cooker makes must match the game.

The game packages hold OpenXWA and the OpenXWA Mod Importer, but no other
tools. `environment_bake` and `xwa_image_cook` are built from source; see
the build instructions for
[Windows](../packaging/windows/README.md),
[macOS](../packaging/macos/README.md) and
[Linux](../packaging/linux/README.md).

## Projects

A project is a folder that holds the sources of your models and effect
libraries, such as glTF files, PNG textures and library files. The Blender
add-on, `opt2gltf` and `aeron_gltf_cook --build-project` all use this
layout:

```text
my-project/
  models/
    xwing/
      xwing.gltf        the model
      xwing.bin         its meshes
      textures/         its PNG textures
        skins/<skin>/   the images of its skins
      bake/probes/      its probes, after Bake Probes
  effects/
    effects.yaml        the blast marks and energy effects of the game
    mycockpit/
      mycockpit.yaml    an effect library
      frames/           the PNG images of the library
```

Each model has its own folder, `models/<key>/`, and its file is
`<key>.gltf`. The key is the name of the model in the game, in lower case.
To replace an original model, use the name of its OPT file:
`models/xwing/` replaces `FLIGHTMODELS/XWING.OPT`.

Effect libraries hold the frames that animated materials play. All the
models of a project share them. [Materials](materials.md) describes them,
and `effects.yaml`.

Keep the project in its own folder, apart from the mod. The tools build
the project into the mod folder.

## The Blender add-on

The add-on imports, edits and exports models. It needs Blender 5.0 or
later.

### Install

1. In Blender, open Edit > Preferences > Get Extensions, then Install from
   Disk, and choose the add-on package.
2. In the add-on preferences, set **Mod Folder** to the folder of your mod.

The add-on finds its tools in its package. To use other copies, set their
paths in the preferences. The preferences also set how models are cooked:

| Preference | Cooker option | Default |
|-|-|-|
| Atlas Page | `--atlas-page` | 2048 |
| Max Texture | `--max-texture` | 4096 |
| Atlas Padding | `--mip-min-subrect` | 16 |
| BC7 Quality | `--quality` | Medium |
| Zstd Supercompression | `--no-zstd` when off | on |

The add-on package doesn't hold `environment_bake`. Only **Bake Probes**
needs it. To bake probes, build `environment_bake` and set its path in the
preferences.

### Import a model

To open a model of your project, use File > Import > Aeron Flight Model
(.gltf/.glb), or **Import** in the **Aeron** tab of the 3D view sidebar
(press N to show the sidebar). Choose the `.gltf` file. Don't import a
`.glb` that was cooked for the game, because Blender can't read its
textures. Then save your work as a `.blend` file.

If the `.gltf` changes outside Blender, import it again. Don't save over it
from an older `.blend`.

### Import an OPT

To start from a model of the original game, use File > Import > XWA OPT
(.opt), the **Import OPT** button in the Aeron tab, or drop an OPT file on
the 3D view. The add-on converts the OPT with `opt2gltf` into
`models/<key>/` of a project, then imports it. The key is the OPT name in
lower case.

The import uses the **OPT Project** that the Aeron tab shows. Click it to
choose another project. A dialog asks for the project when there is none
yet, and before it replaces a model that the project already has. If you
replace a model, you lose the changes you made to it. A new folder starts a
new project.

Set **Source** to **Original Game** to convert the OPT the way the original
game draws it. The import adds the game's data for the model, such as its
pilot eye, crew and moving parts.

### Start from your own meshes

Select the meshes and click **Make Flight Model** in the Aeron tab. They
become the components of a new model. Then use **Save to Project** and save
the model as `models/<key>/<key>.gltf` in your project.

### Edit

The Aeron tab adds parts to the model and checks it. The flight data of
each object is in Object properties > Aeron Flight Model. The animations of
a material are in Material properties > Aeron Animation, and its skins in
Aeron Skins. You can paint the textures with any program, in the model's
folder.

[Models](models.md) describes the parts of a model, and
[Materials](materials.md) describes materials, skins and animations.

### Save to Project

**Save to Project**, in the Aeron tab, saves the model's `.gltf` and
`.bin`. It offers the file that you imported. It is also in
File > Export > Aeron Flight Model (.gltf).

Textures stay where they are. Images from other folders, and images that
exist only in Blender, are copied into `textures/` as PNG files. The other
files in the folder aren't changed or removed.

Save to Project stops when:

- an image has unsaved changes. Save the image first, with Image > Save;
- a copied image would replace another file with the same name;
- the model would lose an extension or extras that the imported file had.
  Turn on **Allow Losing Data** to save anyway.

### Export for Game

**Export for Game**, in the Aeron tab, cooks the model into a `.glb` file
for the game. It is also in File > Export > Aeron Flight Model for Game
(.glb). Save the file in `flight/models/` of your mod, as `<key>.glb`.

When **Mod Folder** is set, it first rebuilds the project's effect
libraries that have changed, into `flight/effects/` of the mod. It lists
them, and checks every frame that the materials play against them. If a
rebuild removes an image that other models use, or changes its kind, it
names those models. Their next build fails until you fix them.

Export for Game doesn't copy `effects/effects.yaml` to the mod. Use
`aeron_gltf_cook --build-project` for that.

Both exports check the model first. Aeron > Problems lists each problem,
with a button that selects the object at fault.

## aeron_gltf_cook

`aeron_gltf_cook` turns glTF models into the `.glb` files that the game
loads, and builds effect libraries. The add-on runs it for you. Use it
from the command line to build a whole project, or in your own scripts.

### Build a project

```sh
aeron_gltf_cook --build-project <project> --out <mod folder>
```

This builds the whole project into the mod folder:

1. It builds each `effects/<library>/<library>.yaml` into
   `flight/effects/`. A library whose files haven't changed since the last
   build is left as it is.
2. It copies `effects/effects.yaml` to `flight/effects/`.
3. It checks the materials of each `models/<key>/<key>.gltf` against the
   libraries, then cooks the model into `flight/models/<key>.glb`. When the
   model has a probes bake in `models/<key>/bake/probes/`, the `.glb`
   includes it.

For the example project above, the mod gets:

```text
my-mod/
  flight/
    models/
      xwing.glb
    effects/
      effects.yaml
      mycockpit.yaml
      mycockpit.*.ktx2
```

It never deletes files from the mod. When you remove a model from the
project, delete its `.glb` from the mod yourself.

| Option | Meaning |
|-|-|
| `--out <mod folder>` | Required. The mod folder to build into. |
| `--jobs <N>` | Cooks N models at once. The default is 1. |
| `--only <key>` | Cooks only the model with this key. The libraries are still built, because the model is checked against them. |
| `--zstd-level <N>` | The zstd compression level, 1 to 22. The default is 9. |
| `--verbose` | Prints more detail about each cook. |

The tool cooks every model each time. Use `--only` to cook just the model
you are working on.

### Cook one model

```sh
aeron_gltf_cook <model>.gltf --out <mod folder>/flight/models/<key>.glb
```

To check the frames of the model's animations against its libraries, add
`--effects-dir <mod folder>/flight/effects`. The folder must exist. To also
build the libraries that the model uses from a project first, add
`--project <project>`.

| Option | Meaning |
|-|-|
| `--out <path>` | Required. The `.glb` file to write. |
| `--atlas-page <N>` | The width and height of an atlas page, in pixels. A power of 2, at least 128. The default is 2048. The page grows to fit the largest texture. A texture larger than half a page gets its own page. |
| `--max-texture <N>` | Larger textures are scaled down to N pixels. The default is 4096. |
| `--mip-min-subrect <N>` | The padding around each texture in the atlas, in texels: 4, 8, 16 or 32. It also limits the number of mip levels. The default is 16. The atlas page must be at least 8 times this size. |
| `--quality fast\|med\|uber` | The texture compression quality. `uber` is the slowest. The default is `med`. |
| `--threads <N>` | Compresses textures on at most N threads. The default is one per processor thread. Set it when you run several cooks at once. |
| `--no-zstd` | Doesn't compress the textures with zstd. |
| `--zstd-level <N>` | The zstd compression level, 1 to 22. The default is 9. Level 19 is about 14 times slower and makes the textures about 6% smaller. |
| `--effects-dir <dir>` | Checks every frame and damage stage that the materials name against the libraries in this folder. |
| `--project <dir>` | With `--effects-dir`, first builds each library that the materials name and that the project has into the `--effects-dir` folder. |
| `--probes <dir>` | Adds the probes bake in this folder to the model. |
| `--verbose` | Prints the progress of each texture channel, and more detail. |

After cooking, the tool loads the `.glb` the way the game does. It fails
when the file doesn't load.

### Build an effect library

```sh
aeron_gltf_cook --library <project>/effects/mycockpit/mycockpit.yaml \
    --out <mod folder>/flight/effects
```

This writes the library's index, `mycockpit.yaml`, and its image files,
into the folder. When the library file and its images haven't changed
since the last build, the library is left as it is. Add `--verbose` for
more detail.

Build the libraries before you cook models with `--effects-dir`.
[Materials](materials.md) describes the library file.

## opt2gltf

`opt2gltf` converts an OPT model into a glTF model that you can edit. The
add-on's Import OPT runs it for you.

To convert an OPT into a project:

```sh
opt2gltf --stock --project <project> FLIGHTMODELS/XWING.OPT
```

This writes `models/xwing/xwing.gltf`, its `.bin` and a `textures/` folder
into the project. If the project already has the model, the tool stops.
Add `--replace` before the OPT file to replace the model. You then lose
the changes you made to it.

To convert an OPT into any folder:

```sh
opt2gltf --stock FLIGHTMODELS/XWING.OPT <output folder>
```

The options go before the OPT file.

| Option | Meaning |
|-|-|
| `--stock` | Required. Converts the OPT the way the original game draws it. |
| `--smooth-angle <degrees>` | The smoothing angle of the normals, 0 to 180. The default is 90. A smaller angle, such as 45, keeps the edges of flat panels sharp. |
| `--project <dir>` | Writes the model into this project. |
| `--replace` | With `--project`, replaces a model that the project already has. |

The conversion:

- keeps only the most detailed version of the model;
- makes new normals, smoothed at the smoothing angle;
- adds an emissive texture, `*_emissive.png`, for each texture that has
  lit pixels, such as windows and cockpit lights;
- adds the game's data for the model, such as its pilot eye and moving
  parts.

## environment_bake

`environment_bake` turns a sky into the images that the game uses to draw
it and to light the scene. The sky can be one panorama or six cube faces.
The tool also bakes sky overlays.
[Environments](environments.md) describes its commands and options.

The add-on also uses `environment_bake` for **Bake Probes**.

## xwa_image_cook

`xwa_image_cook` builds an atlas of 2D images, such as menu art, from PNG
files:

```sh
xwa_image_cook <folder> --mod <mod folder>
```

The folder holds the PNG files and an `image.yaml` file that says which
image the atlas replaces. The tool writes the atlas into the right folder
of the mod. `--quality fast|med|uber` sets the compression quality, as for
`aeron_gltf_cook`. The default is `med`. [Images](images.md) describes
`image.yaml` and the atlas format.

## Common changes

### A new ship

1. Make the model in Blender, or start from an OPT with Import OPT.
2. Save it to your project with Save to Project, as
   `models/<key>/<key>.gltf`. Use a new key, not the name of an original
   model.
3. Add its components, hardpoints, engine glows and collision mesh. See
   [Models](models.md).
4. Cook it into `flight/models/<key>.glb` with Export for Game, or build
   the project with `aeron_gltf_cook --build-project`.
5. Add its object type, its craft type and its model settings. See
   [Craft](craft.md).
6. Put it in a mission. See [Missions](missions.md).
7. Test it.

### A reskin

To change how an original model looks for every craft that uses it:

1. Import its OPT with Import OPT.
2. Paint the textures in `models/<key>/textures/`, or paint skins over
   them in Blender.
3. Use Export for Game to write `flight/models/<key>.glb`.
4. Test it.

To add a look next to the original one, paint a skin and add a variant
that uses it. In the model's markings list, give the variant a markings
value. Then give that markings value to a flight group in its mission file.
See [Materials](materials.md) and [Missions](missions.md).

### A new sky

1. Make the sky as one panorama or as six cube faces.
2. Bake it with `environment_bake`.
3. Add it to `flight/environments/environments.yaml`.
4. Test it in a mission that uses it.

[Environments](environments.md) describes each step.

### A new mission

1. Make the mission file (`.TIE`) with a mission editor for the original
   game.
2. Copy it to `game/MISSIONS/` in the mod, and add it to a copy of
   `MISSIONS/MISSION.LST` in `game/MISSIONS/`.
3. If the mission needs OpenXWA settings, such as the markings of a
   flight group or a hangar, write its file in `flight/missions/`. For
   `MISSIONS/MYMISSION.TIE`, that file is
   `flight/missions/missions/mymission.yaml`.
4. Test it.

[Missions](missions.md) describes the mission files.

### New menu art

1. Paint the images as PNG files, in a folder of their own.
2. Write `image.yaml` in that folder. It names the image that the atlas
   replaces.
3. Run `xwa_image_cook` on the folder.
4. Test it.

[Images](images.md) describes `image.yaml`, and [Frontend](frontend.md)
describes the screens.

### The test loop

1. Change the source files, such as the model in Blender, a library, a PNG
   file or a YAML file.
2. Build what changed. In Blender, use Export for Game. From the command
   line, run `aeron_gltf_cook --build-project`, `environment_bake` or
   `xwa_image_cook`. YAML files need no build.
3. Start OpenXWA with `--mod` and your mod folder.
4. Read the log, and fix each warning and error about your files.

[Testing a mod](mod_folder.md#testing-a-mod) describes where the log is.
