#ifndef XWA_RUNTIME_MODELS_MODEL_SETTINGS_H
#define XWA_RUNTIME_MODELS_MODEL_SETTINGS_H

/* Reads the model settings files of the mod folder,
 * flight/models/<model key>.yaml, next to each model's GLB:
 *
 *   version: 1
 *   hud: {...}            # craft_hud.h
 *   engine_glows: {...}   # engine_glows.h
 *
 * Each top-level key is a group that one module owns. This module reads the
 * files and hands each group to its module, which keeps the values it needs
 * in a table by object type.
 *
 * The files are per model, but the game code only knows object types. Which
 * model an object type loads depends on the mission: its list row's model,
 * or the target of a mission swap (model_swaps.h). So the settings are read
 * again for each mission. At mission load, the module reads the settings of
 * the object types that the mission's flight groups use. Any other object
 * type, such as a craft that the player switches to in the hangar, gets its
 * settings the first time a module asks for them. Each model file is parsed
 * once per mission, even when several object types load the same model.
 *
 * The files only apply with model replacements on, because the model keys
 * come from the replacement list. Without them, every module keeps the
 * game's own values. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct XwaFlightGroup;

/* Drops the previous mission's settings and reads the settings of the
   object types that the mission's flight groups use. `flightGroups` points
   to the first flight group, `stride` is the distance between two flight
   groups, and `count` is their number. Call it after the flight groups'
   craft types are final and the mission's model swaps are loaded. */
void XwaModelSettings_LoadMission(const struct XwaFlightGroup* flightGroups, size_t stride, int count);

/* Reads the settings of `objectType` if this mission hasn't read them yet.
   Modules call it before they look up a type's values. */
void XwaModelSettings_Ensure(unsigned int objectType);

#ifdef __cplusplus
}
#endif

#endif
