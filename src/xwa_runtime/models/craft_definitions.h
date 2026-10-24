#ifndef XWA_RUNTIME_MODELS_CRAFT_DEFINITIONS_H
#define XWA_RUNTIME_MODELS_CRAFT_DEFINITIONS_H

/* Reads the object type and craft type files of the mod folder.
 *
 * flight/object_types/<name>.yaml changes one object type of the game, or
 * fills a reserved one:
 *
 *   version: 1
 *   object_type: 138
 *   model: victorystardestroyer   # loads flight/models/victorystardestroyer.glb
 *   list: spacecraft              # or equipment
 *   genus: 4
 *   family: 0
 *   flags: 0x43
 *   craft_record_index: 140       # uses craft record 140 instead of its own
 *   craft_record: {hull: 16590, shields: 5000, speed: 50, ...}
 *   sounds_like: 5                # uses the engine and launch sounds of type 5
 *   beam_allowed: true            # keeps the beam that its flight group gives
 *   names: {short: 'QF/Cr', long: 'Quasar Fire Carrier', plural: '...', gender: m}
 *   gunner_cockpit: true          # the cockpit of a gunner seat
 *
 * `gunner_cockpit` makes the object type a gunner seat's cockpit, like the
 * stock type 317, and gives it type 317's asset flags. Its model then loads
 * in every mission that has a craft with that seat, Death Star missions
 * included. The game doesn't pair the hardpoints of its turret meshes, so
 * its two gun hardpoints become weapon slots 0 and 1 of its craft record.
 * The type needs a craft record of its own.
 *
 * Every key but `version` and `object_type` is optional, and a key left out
 * keeps the game's value. With `model`, the object type gets a list row of
 * its own that names the model, which only exists in memory. The other
 * object types that share its stock row keep that row. The names go to the
 * craft record of the object type.
 *
 * flight/craft_types/<name>.yaml changes one craft type:
 *
 *   version: 1
 *   craft_type: 214
 *   object_type: 123              # optional: the craft type keeps its own
 *   ship_list: {name: '...', category: Starship, flyable: false, known: true,
 *               skirmish: true, hidden: false, icon: [8, 444, 28, 472]}
 *   tech_room: {name: '...', manufacturer: '...', side: '...',
 *               description: '...', crew: '...'}
 *
 * `object_type` links the craft type to that object type, which needs a list
 * row once the object type files apply. The ship list and tech room keys
 * become text overrides.
 *
 * The game uses the numbers in the files; the file names only appear in the
 * log. Two files that set the same object type, craft record or craft type
 * are both ignored. The files only apply with model replacements on, because
 * they're part of the mod's model set. The game reads them once at startup.
 * Without them, the game's craft are stock. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reads the object type and craft type files and writes the game's tables:
   the object types, the craft records, the craft type table and the added
   list rows. Runs before XwaModelSource_Init, which reads the craft record
   names and the rows. */
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
/* Whether `objectType` is a gunner seat's cockpit added by an object type
   file (`gunner_cockpit: true`). */
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
