#ifndef XWA_RUNTIME_MODELS_SURFACES_H
#define XWA_RUNTIME_MODELS_SURFACES_H

/* Surfaces that show by the craft's state, from the `surfaces` group of a
 * model settings file:
 *
 *   surfaces:
 *     engine_glow:       {hidden_when_disabled: true}
 *     engine_glow_small: {hidden_when_disabled: true, min_throttle_percent: 99, min_ordered_speed_mglt: 11}
 *     forcefield:        {hidden_when_disabled: true}
 *
 * Keys are tags of the model's materials and lights. A tagged surface or
 * light is hidden when any of its rules hides it:
 *   - `hidden_when_disabled`: while no subsystem works
 *     (CraftData.workingSubsystems == 0, the game's "disabled" test);
 *   - `min_speed_percent`: while the speed is below this percent of the top
 *     speed (as in `engine_glows`);
 *   - `min_speed_mglt`: while the speed is below this many MGLT;
 *   - `min_throttle_percent`: while the throttle is below this percent;
 *   - `min_ordered_speed_mglt`: while the speed that the craft's orders set
 *     (CraftData.commandedSpeed) is below this many MGLT.
 * All the keys are optional. A model has at most XWA_SURFACES_MAX_RULES
 * rules. Objects without a craft record show every surface.
 *
 * The setting only changes drawing, through the snapshot. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;
struct CraftData;

enum { XWA_SURFACES_MAX_RULES = 32 };

/* Drops the previous mission's settings. Called by the model settings
   reader. */
void XwaSurfaces_ClearModelSettings(void);
/* Reads the `surfaces` group of `objectType`'s model settings file, or does
   nothing when `group` is NULL. `path` is the file, for the log. Called by
   the model settings reader. */
void XwaSurfaces_ReadModelSettings(unsigned int objectType, const char* path,
								   const struct AeronConfigNode* group);

/* The rules of `objectType` that hide their tag on `craft`, moving at `speed`
   (MobileObject.speed): bit r for rule r. */
uint32_t XwaSurfaces_HiddenMask(unsigned int objectType, const struct CraftData* craft, uint16_t speed);
/* The tag of rule `rule` of `objectType`, or NULL. */
const char* XwaSurfaces_RuleTag(unsigned int objectType, unsigned int rule);

#ifdef __cplusplus
}
#endif

#endif
