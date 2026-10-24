#ifndef XWA_RUNTIME_MODELS_CRAFT_NAMES_H
#define XWA_RUNTIME_MODELS_CRAFT_NAMES_H

/* A mission's craft names: a mission can rename the craft of a model for its
 * length, in the `craft_names` section of its mission file, keyed by model key.
 * Every key is optional:
 *
 *   craft_names:
 *     containerbrick: { ship_list: "Imperial II Star Destroyer 70", name: "Spec",
 *                       short_name: "ISD", plural: "Imperial II Star Destroyers",
 *                       map_icon: [0, 0, 15, 15] }
 *
 * `name`, `short_name` and `plural` are the names of the model's craft
 * record (ModelDef.longName and shortName, g_strSpeciesNamesPlural), which
 * the HUD and the messages show. `ship_list` and `map_icon` change the ship
 * list entry of the craft type of the first object type that uses the
 * record. The model is the one of the type's list row, so the names need
 * the model keys of the model source.
 *
 * The names are set at mission load and put back after the debriefing. The
 * briefing doesn't get them, except for `ship_list` in the order of
 * battle. */

#ifdef __cplusplus
extern "C" {
#endif

/* Reads the names of mission `missionPath` (for example
   "missions\1b7m2w.tie"), replacing the previous mission's. It doesn't set
   them: Mission_Init calls XwaCraftNames_Apply. */
void XwaCraftNames_LoadMission(const char* missionPath);

/* Puts back the names of the previous mission, then sets this mission's. */
void XwaCraftNames_Apply(void);

/* Puts back the stock names. Called after the debriefing. */
void XwaCraftNames_Restore(void);

/* The ship list name that the mission gives craft type `craftType`, or
   NULL. The order of battle shows it. */
const char* XwaCraftNames_ShipListName(int craftType);

#ifdef __cplusplus
}
#endif

#endif
