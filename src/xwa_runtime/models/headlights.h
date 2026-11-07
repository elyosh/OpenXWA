#ifndef XWA_RUNTIME_MODELS_HEADLIGHTS_H
#define XWA_RUNTIME_MODELS_HEADLIGHTS_H

/* Headlights: a light along the player craft's nose, for dark places such
 * as the Death Star. The `headlights` section of the mod's flight settings
 * gives the light, and is read once at startup. A mission turns them on in
 * its mission file. They are then on while the player isn't in the hangar.
 * In the other missions, `auto_turn_on` turns them on while there is no
 * directional light.
 *
 * The colour and the power in watts are those of a Blender area light, as
 * for the lights of the GLB models: P watts give P * 683 / pi candela
 * straight ahead. Without `angle_degrees`, the light is one-sided, with a
 * cosine falloff. With it, the light is a spot light that fades from the
 * nose out. */

#ifdef __cplusplus
extern "C" {
#endif

struct XwaSnapshot;

/* Reads the `headlights` section of flight/flight.yaml. */
void XwaHeadlights_Init(void);

/* Reads `headlights` from the file of mission `missionPath` (for example
   "missions\1b7m4w.tie"). */
void XwaHeadlights_LoadMission(const char* missionPath);

/* Writes whether the headlights are on now, and the light, into the
   snapshot. */
void XwaHeadlights_Capture(struct XwaSnapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif
