#ifndef XWA_RUNTIME_MODELS_HANGAR_CRAFTS_H
#define XWA_RUNTIME_MODELS_HANGAR_CRAFTS_H

/* The craft that the hangar's Switch Craft menu offers, from the
 * `hangar_crafts` section of flight/flight.yaml.
 *
 * The original game has three lists in Hangar_SetupReadyScene:
 *   - fighters: X-wing, Y-wing, A-wing, B-wing, Z-95;
 *   - transports: Corellian transport, family transport, Millennium Falcon;
 *   - all: the fighters, then the transports.
 * In a Simulator mission (not the campaign, not the Proving Ground), the
 * hangar offers the transports when the player's craft is one of them, and
 * the fighters otherwise. In the campaign and the Proving Ground, the
 * flight group's optional craft category picks the list: 1 all, 2 fighters,
 * 3 transports. Categories 0 and 4 don't use these lists.
 *
 * The section can replace each list with object type names. A list that it
 * leaves out keeps the original game's list. The player's current craft is
 * always first in the menu, so the lists skip it. Without the section,
 * Hangar_SetupReadyScene runs the original code. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum XwaHangarCraftList {
	XWA_HANGAR_CRAFTS_FIGHTERS,
	XWA_HANGAR_CRAFTS_TRANSPORTS,
	XWA_HANGAR_CRAFTS_ALL,
	XWA_HANGAR_CRAFT_LISTS
} XwaHangarCraftList;

/* A list holds at most 49 craft. The menu keeps a disabled flag for only 50
   rows (g_hangarMenuItemDisabled), and reads past it with a longer list.
   The player's craft takes one row. */
#define XWA_HANGAR_CRAFTS_MAX 49

/* Reads the `hangar_crafts` section of flight/flight.yaml. Called once at
   startup, after the object type files, so that renamed types are found. */
void XwaHangarCrafts_Init(void);

/* Returns whether the mod sets at least one list. */
int XwaHangarCrafts_IsSet(void);

/* Returns whether `objectType` is in the transports list. */
int XwaHangarCrafts_IsTransport(uint16_t objectType);

/* Appends the craft of `list` to `out` from index `count`, skipping
   `playerType`. Returns the new count. */
int XwaHangarCrafts_Fill(XwaHangarCraftList list, uint16_t playerType, uint16_t* out, int count);

#ifdef __cplusplus
}
#endif

#endif
