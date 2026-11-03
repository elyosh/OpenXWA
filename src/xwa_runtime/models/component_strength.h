#ifndef XWA_RUNTIME_MODELS_COMPONENT_STRENGTH_H
#define XWA_RUNTIME_MODELS_COMPONENT_STRENGTH_H

/* The strength of a model's components, from the `component_strength` group
 * of its model settings file:
 *
 *   component_strength: {gun_turret: 18, small_gun: 18, shield_generator: 36}
 *
 * The keys are the mesh type names of the AERON_flight_model extension, such
 * as main_hull or rotary_communication_system. The values go from 0 to 255.
 * A mesh type without a value keeps the game's strength for that type.
 *
 * A new craft's components start with this strength, and the target display
 * shows a component's health as a percent of it. The Super Star Destroyer's
 * shield generators keep their own strength. The values change the
 * simulation. */

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

/* Drops the previous mission's values. Called by the model settings reader. */
void XwaComponentStrength_ClearModelSettings(void);
/* Reads the `component_strength` group of `objectType`'s model settings
   file, or does nothing when `group` is NULL. `path` is the file, for the
   log. Called by the model settings reader. */
void XwaComponentStrength_ReadModelSettings(unsigned int objectType, const char* path,
											const struct AeronConfigNode* group);

/* Returns the strength of the components of mesh type `meshType` of object
   type `objectType`, or `stock` when its model doesn't set it. */
int XwaComponentStrength_Value(unsigned int objectType, int meshType, int stock);

#ifdef __cplusplus
}
#endif

#endif
