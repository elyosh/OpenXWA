#ifndef XWA_RUNTIME_MODELS_SURFACES_H
#define XWA_RUNTIME_MODELS_SURFACES_H

/* Surfaces that show by the craft's state, from the `surfaces` group of a
 * model settings file. The keys are tags of the model's materials and
 * lights. A tagged surface or light is hidden when any of its rules hides
 * it. The mod documentation describes the rules.
 *
 * `hidden_when_disabled` uses the game's "disabled" test,
 * CraftData.workingSubsystems == 0. `min_speed_percent` works as in
 * `engine_glows`. `min_ordered_speed_mglt` tests CraftData.commandedSpeed,
 * the speed that the craft's orders set. A model has at most
 * XWA_SURFACES_MAX_RULES rules. Objects without a craft record show every
 * surface.
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
