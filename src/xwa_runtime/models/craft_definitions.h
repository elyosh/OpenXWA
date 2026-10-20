#ifndef XWA_RUNTIME_MODELS_CRAFT_DEFINITIONS_H
#define XWA_RUNTIME_MODELS_CRAFT_DEFINITIONS_H

/* Reads the object files of the mod folder, flight/objects/<name>.yaml. Each
 * file changes one object type of the game, or fills a reserved one:
 *
 *   version: 1
 *   object_type: 138
 *   model: victorystardestroyer   # loads flight/models/victorystardestroyer.glb
 *   list: spacecraft              # or equipment
 *   model_type: {genus: 4, family: 0, flags: 0x43, craft_record_index: 140}
 *   craft_record: {hull: 16590, shields: 5000, speed: 50, ...}
 *   sounds_like: 5                # uses the engine and launch sounds of type 5
 *   beam_allowed: true            # keeps the beam that its flight group gives
 *   names: {short: 'QF/Cr', long: 'Quasar Fire Carrier', plural: '...', gender: m}
 *   craft_types:
 *     214:
 *       ship_list: {name: '...', category: Starship, flyable: false, known: true,
 *                   skirmish: true, hidden: false, icon: [8, 444, 28, 472]}
 *       tech_room: {name: '...', manufacturer: '...', side: '...',
 *                   description: '...', crew: '...'}
 *
 * Every key but `version` and `object_type` is optional, and a key left out
 * keeps the game's value. With `model`, the object type gets a list row of
 * its own that names the model, which only exists in memory. The other
 * object types that share its stock row keep that row. `craft_types` maps
 * each listed craft type to this object type and sets its ship list and tech
 * room texts, as text overrides (text_overrides.h). The names go to the craft
 * record of the object type.
 *
 * The files only apply with model replacements on, because they're part of
 * the mod's model set. The game reads them once at startup. Without them,
 * the game's craft are stock. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reads the object files and writes the game's tables: the object types, the
   craft records, the craft type table and the added list rows. Runs before
   XwaModelSource_Init, which reads the craft record names and the rows. */
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

/* Gives the list readers the rows that the object files add to list `list`
   (0 for Spacecraft0.LST, 1 for Equipment0.LST), after the last line of the
   file. `*rowIndex` is the number of rows read so far. The function writes
   the line of the next added row to `line`, sets `*rowIndex` to the number
   of rows before it, and returns 1. It returns 0 when there are no more. */
int XwaCraftDefinitions_NextAddedRow(int list, int* rowIndex, char* line, size_t lineSize);

#ifdef __cplusplus
}
#endif

#endif /* XWA_RUNTIME_MODELS_CRAFT_DEFINITIONS_H */
