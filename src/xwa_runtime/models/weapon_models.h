#ifndef XWA_RUNTIME_MODELS_WEAPON_MODELS_H
#define XWA_RUNTIME_MODELS_WEAPON_MODELS_H

/* The models of a craft's shots. A mod sets them in the `models` group of the
 * `weapons` group of a model settings file:
 *
 *   weapons:
 *     models: {torpedo: capitaltorpedo, heavy_rocket: capitalrocket}
 *
 * Each key is a weapon type, and each value is the key of a model in the
 * mod. When the craft fires that weapon type, its shots show that model
 * instead of the weapon's own model. For example, a capital ship can fire
 * bigger torpedoes than a fighter.
 *
 * When a mission loads, the game loads these models for the object types
 * that the mission's flight groups use. It frees them when it frees the
 * mission's other models. Shots from other object types keep the weapon's
 * own model. XWAU's hook_mission_tie does the same with [OptReplace].
 *
 * Only drawing changes. Collisions use the weapon's own model. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

/* Drops the previous mission's settings. Called by the model settings
   reader. */
void XwaWeaponModels_ClearModelSettings(void);
/* Reads `models` from the `weapons` group of `objectType`'s model settings
   file. Does nothing when `group` is NULL. `path` is the file name, used in
   log messages. Called by the model settings reader. */
void XwaWeaponModels_ReadModelSettings(unsigned int objectType, const char* path,
									   const struct AeronConfigNode* group);
/* Frees the previous mission's models, then loads the models named in the
   settings read so far. A model that can't be loaded is a fatal error, and
   the function returns 0 after it. Mission_Init calls it after it reads the
   model settings of the flight groups. */
int XwaWeaponModels_LoadMission(void);
/* Frees the models that XwaWeaponModels_LoadMission loaded. Called when the
   game frees the mission's other models. */
void XwaWeaponModels_FreeMission(void);
/* Returns the handle of the model to draw for a `weaponType` shot fired by
   a craft of type `sourceObjectType`. Returns 0 when the shot uses the
   weapon's own model. */
uint16_t XwaWeaponModels_Handle(unsigned int sourceObjectType, unsigned int weaponType);
/* Returns whether `handle` is one of the models that
   XwaWeaponModels_LoadMission loaded. */
int XwaWeaponModels_IsModel(uint16_t handle);

#ifdef __cplusplus
}
#endif

#endif
