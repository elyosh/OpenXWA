#ifndef XWA_RUNTIME_MODELS_CRAFT_DEFINITIONS_H
#define XWA_RUNTIME_MODELS_CRAFT_DEFINITIONS_H

/* Reads the object type and craft type files of the mod folder.
 *
 * flight/object_types/<name>.yaml changes one object type of the game, or
 * fills a free slot:
 *
 *   object_type: victory_star_destroyer2
 *   name: victory_star_destroyer       # optional: a new name for the slot
 *   model: victorystardestroyer        # loads flight/models/victorystardestroyer.glb
 *   list: spacecraft                   # or equipment
 *   genus: starship
 *   family: craft
 *   flags: [player_targetable, ai_targetable, float_hardpoints]
 *   craft_record: {hull: 16590, shields: 5000, speed: 50, ...}
 *   sounds_like: tie_fighter           # uses the engine and launch sounds of this type
 *   beam_allowed: true                 # keeps the beam that its flight group gives
 *   names: {short: 'QF/Cr', long: 'Quasar Fire Carrier', plural: '...', gender: m}
 *
 * Each object type has a name, such as `xwing`, or `slot_33` for a free
 * slot. `object_type` is always the stock name. `name` renames the slot, for
 * example when the mod puts another craft in it. The rest of the mod then
 * uses the new name, for example in `sounds_like` and in mission files, and
 * the stock name no longer works there. Names must be unique: if another
 * object type already has the name, the rename is ignored.
 *
 * `genus` and `family` take names. `flags` lists the flags that are on. The
 * flags that it leaves out are off. Without `flags`, the object type keeps
 * its stock flags. A file can only set player_targetable,
 * ai_targetable, nav_buoy, static_object, float_hardpoints, spins and
 * collision_face_groups. The other flags come from the original image files.
 *
 * Besides single values, `craft_record` has these lists:
 *   - lasers: up to 3 groups of {type, count, kind, range, cooldown}. `type`
 *     is a weapon type name, or none for an empty group. `kind` is
 *     laser_cannon, ion_cannon, turret or long_range_turret.
 *   - warheads: up to 2 groups of {type, count, capacity}.
 *   - turrets: up to 2 seats. They use the gunner seat keys, plus
 *     `object_type`, the object type of the seat's cockpit.
 *   - cockpit_eye_m: the pilot's eye in metres, as [right, forward, up].
 * If a list leaves out a group or a seat, that group or seat is emptied.
 *
 * Several object types can share one craft record. In the stock game,
 * mod_strike_cruiser and golan_two share one. When a file changes the craft
 * record or the names of such an object type, the game gives that object type
 * an unused craft record, starting as a copy of the shared one.
 *
 * All keys except `object_type` are optional. A missing key
 * keeps the game's value. With `model`, the object type gets its own list
 * row, in memory only, that loads the model. Other object types that used the
 * same stock row keep it. `names` changes the names in the object type's
 * craft record.
 *
 * Each cockpit model that a gunner seat uses gets a hidden object type, in a
 * free slot with a row in the equipment list.
 *
 * flight/craft_types/<name>.yaml changes one craft type:
 *
 *   craft_type: 214
 *   object_type: victory_star_destroyer  # optional: the craft type keeps its own
 *   ship_list: {name: '...', category: starship, flyable: false, known: true,
 *               skirmish: true, hidden: false, icon: [8, 444, 28, 472]}
 *   tech_room: {name: '...', manufacturer: '...', side: '...',
 *               description: '...', crew: '...'}
 *
 * Craft types are numbers. Their only name is the ship list text, and the
 * mod can change it. `object_type` points the craft type to another object
 * type. That object type must have a list row after the object type files
 * are applied. The ship list category is one of fighter, light_transport,
 * utility, container, heavy_transport, starship, station,
 * weapon_emplacement, mine, satellite_buoy and droid. The ship list and tech
 * room keys replace the game's texts.
 *
 * File names only appear in the log. If two files change the same object
 * type or craft type, both files are ignored. The files only apply when model
 * replacements are on, because they belong to the mod's models. The game
 * reads them once at startup, before the other mod files, because those
 * files use the new names. Without these files, all craft are stock. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reads the object type and craft type files and updates the game's tables:
   object types and their names, craft records, the craft type table and the
   new list rows. Call it before the other mod readers, because they use the
   object type names, and before XwaModelSource_Init, because it reads the
   craft record names and the list rows. */
void XwaCraftDefinitions_Apply(void);
/* Writes the names of the craft records. Runs after
   StringTable_LoadGameStrings, which sets the stock names. */
void XwaCraftDefinitions_ApplyNames(void);

/* The object type whose engine and launch sounds `objectType` uses. */
unsigned int XwaCraftDefinitions_SoundObjectType(unsigned int objectType);
/* Whether a craft of `objectType` keeps the beam that its flight group gives
   it. In the stock game, the X-wing, Y-wing, A-wing, B-wing, Z-95 and TIE
   Fighter don't. */
int XwaCraftDefinitions_BeamAllowed(unsigned int objectType);
/* Returns whether `objectType` is a hidden object type that the game added
   for a gunner cockpit. */
int XwaCraftDefinitions_GunnerCockpit(unsigned int objectType);

/* Gives the list readers the rows that the object type files add to list `list`
   (0 for Spacecraft0.LST, 1 for Equipment0.LST), after the last line of the
   file. `*rowIndex` is the number of rows read so far. The function writes
   the line of the next added row to `line`, sets `*rowIndex` to the number
   of rows before it, and returns 1. It returns 0 when there are no more. */
int XwaCraftDefinitions_NextAddedRow(int list, int* rowIndex, char* line, size_t lineSize);

#ifdef __cplusplus
}
#endif

#endif /* XWA_RUNTIME_MODELS_CRAFT_DEFINITIONS_H */
