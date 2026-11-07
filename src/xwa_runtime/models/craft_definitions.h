#ifndef XWA_RUNTIME_MODELS_CRAFT_DEFINITIONS_H
#define XWA_RUNTIME_MODELS_CRAFT_DEFINITIONS_H

/* Reads the object type files and the craft type files of a mod. They change
 * the game's object types, craft records, model list rows, ship list lines
 * and tech room entries, or fill free slots with new craft. The mod
 * documentation describes the keys.
 *
 * The game reads the files once at startup, before the other mod files,
 * because those files use the new object type names. The files only apply
 * when model replacements are on, because they belong to the mod's models.
 * Without them, all craft are stock.
 *
 * Several object types can share one craft record. In the stock game,
 * mod_strike_cruiser and golan_two share one. When a file changes the craft
 * record or the names of such an object type, the game gives that object type
 * an unused craft record, starting as a copy of the shared one.
 *
 * With a model, an object type gets its own list row, in memory only, that
 * loads the model. Other object types that used the same stock row keep it.
 * Each cockpit model that a gunner seat uses gets a hidden object type, in a
 * free slot with a row in the equipment list. */

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
