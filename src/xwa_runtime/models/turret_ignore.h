#ifndef XWA_RUNTIME_MODELS_TURRET_IGNORE_H
#define XWA_RUNTIME_MODELS_TURRET_IGNORE_H

/* Kinds of objects that the AI turrets of a craft ignore, for each weapon
 * type. A mod sets them in the `turret_ignore` group of the `weapons`
 * section of flight/flight.yaml and of a model settings file:
 *
 *   turret_ignore:
 *     rebel_laser_turbo: [freighter, starship, platform]
 *     turbolaser_red_heavy: [fighter, warhead]
 *
 * Each list has genus names and `warhead`, which means a projectile that
 * the AI can target, such as a missile. For each weapon type, the list of
 * the craft's model applies first, then the list of flight/flight.yaml. An
 * empty list ignores nothing and stops the list of flight/flight.yaml from
 * applying.
 *
 * The AI gunner orders select the list of a rack before they look for its
 * target (XwaTurretIgnore_Select). The target searches then skip the
 * objects that the list ignores (XwaTurretIgnore_Skips). This includes the
 * friendly objects that could block the shot. The order switch and the
 * target scan clear the selection (XwaTurretIgnore_Clear).
 *
 * This changes the simulation. */

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

/* Reads the lists of the `weapons` section of flight/flight.yaml. Called
   once at startup. */
void XwaTurretIgnore_Init(void);
/* Drops the lists of the previous mission's models and clears the
   selection. Called by the model settings reader. */
void XwaTurretIgnore_ClearModelSettings(void);
/* Reads `turret_ignore` from the `weapons` group of `objectType`'s model
   settings file, or does nothing when `group` is NULL. `path` is the file,
   for the log. Called by the model settings reader. */
void XwaTurretIgnore_ReadModelSettings(unsigned int objectType, const char* path,
									   const struct AeronConfigNode* group);

/* Selects the list of rack `rackIdx` of object `objIdx`, from the object's
   type and the rack's weapon type. g_curCraft must be the object's craft. */
void XwaTurretIgnore_Select(unsigned int objIdx, unsigned int rackIdx);
/* Clears the selection. */
void XwaTurretIgnore_Clear(void);
/* Returns 1 when the selected list ignores object `objIdx`. Returns 0 when
   nothing is selected. */
int XwaTurretIgnore_Skips(unsigned int objIdx);
/* XwaTurretIgnore_Skips, but only when the selection was made for object
   `craftObjIdx`. Returns 0 otherwise. */
int XwaTurretIgnore_SkipsForCraft(unsigned int craftObjIdx, unsigned int objIdx);

#ifdef __cplusplus
}
#endif

#endif
