# Making mods for OpenXWA

A mod is a folder of files that changes *X-Wing Alliance* when OpenXWA plays
it. A mod can replace the 3D models, the skies, the menu art, the movies,
the sounds, the text and the missions. It can also change game rules, such
as weapon damage or the list of craft. The original game files are never
changed.

OpenXWA uses one mod at a time. Without a mod, it plays the original game.

These pages describe the files a mod can contain and how to make them. They
are for mod authors. Players only need the **Mods** screen in the game
options, or the `--mod` option.

## Quick start

This mod changes one line of text. It shows the steps that every mod goes
through.

1. Find the mods folder:

   | System | Mods folder |
   |-|-|
   | Windows | `%LOCALAPPDATA%\TotallyOpen\OpenXWA\mods` |
   | macOS | `~/Library/Application Support/TotallyOpen/OpenXWA/mods` |
   | Linux | `~/.local/share/TotallyOpen/OpenXWA/mods` |

   Create it if it doesn't exist.

2. In the mods folder, create a folder named `my-first-mod`.

3. In `my-first-mod`, create `mod.yaml`:

   ```yaml
   version: 7
   name: "My first mod"
   ```

   `version` is the mod format version. This OpenXWA reads version 7.

4. In `my-first-mod`, create a folder named `text`, and in it a file named
   `strings.yaml`:

   ```yaml
   strings:
     STR_STATISTICS: 'Pilot Records'
   ```

   The game now shows "Pilot Records" in place of "Pilot Statistics".

5. Start OpenXWA. In the options, open **Mods**, choose "My first mod" and
   restart OpenXWA.

To try a mod without changing the options, start OpenXWA with the mod's
folder:

```sh
OpenXWA --mod "/path/to/my-first-mod"
```

## What a mod can change

| Folder | What it changes |
|-|-|
| `flight/` | 3D models and their settings, object and craft types, flight rules, HUD, sound banks, skies, hangars, missions and fonts in flight |
| `frontend/` | menu and concourse art, screen layouts, room animations, medals, movies and menu fonts |
| `resdata/` | the images of the game's image groups, used in menus and in flight |
| `text/` | the game's text |
| `game/` | copies of original game files: missions, mission lists, sounds and a few text lists |

[The mod folder](mod_folder.md) describes each folder in more detail.

## Pages

- [The mod folder](mod_folder.md): the folder layout, `mod.yaml`, how the
  game finds mods and files, the units the files use, and how to test a
  mod.
- [Models](models.md): glTF models, with their parts, hardpoints, engine
  glows, lights, moving parts, cockpits and collision.
- [Materials](materials.md): materials, skins and markings, damage,
  cockpit screens, animated materials and effects.
- [Craft](craft.md): object types, craft types and the settings of each
  model.
- [Flight](flight.md): `flight.yaml`, `hud.yaml` and `sounds.yaml`.
- [Environments](environments.md): skies, lighting, suns and lens flares,
  backdrops.
- [Hangars](hangars.md): `hangars.yaml` and the hangar section of a
  mission.
- [Missions](missions.md): mission files, mission settings, TIE missions,
  mission voices and sound changes.
- [Images](images.md): the atlas format of 2D images, image groups, mission
  art and fonts.
- [Frontend](frontend.md): menus, the concourse, briefings, medals, movies
  and room animations.
- [Text](text.md): replacing the game's text.
- [Tools](tools.md): the Blender add-on, the model cooker, the other tools,
  and the steps to make common changes.
- [Importer](importer.md): turning a mod made for the original game into
  an OpenXWA mod.

## Reference

- [Object types](reference/object_types.md): the name and number of each
  object type, with its model, genus, family and flags.
- [Craft types](reference/craft_types.md): the number of each craft type,
  with its ship list name, object type and category.
- [Weapons](reference/weapons.md): weapon types, warheads, laser groups,
  weapon sound groups, ranks and difficulties.
- [Model values](reference/model_values.md): hardpoint types, mesh types,
  explosion flags, cockpit screens, HUD areas and reticle parts.
- [States and tags](reference/states_and_tags.md): flipbook states, and the
  tags of parts, cockpit controls, materials and lights.
- [Sound banks](reference/sound_banks.md): the banks of `sounds.yaml` and
  when each one plays.
- [Image groups](reference/image_groups.md): what each image group holds.
- [Frontend screens](reference/frontend_screens.md): the screens that can
  have a layout.
