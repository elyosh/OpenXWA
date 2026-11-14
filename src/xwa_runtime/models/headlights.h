#ifndef XWA_RUNTIME_MODELS_HEADLIGHTS_H
#define XWA_RUNTIME_MODELS_HEADLIGHTS_H

/* Headlights: a light along the player craft's nose, for dark places such
 * as the Death Star. The default light is in `point_lights.headlights` of
 * resources/remaster/config.yaml. A mod can change it in the `headlights`
 * section of its flight settings, which is read once at startup. Each key
 * that the mod sets replaces the default. A mission file can turn them
 * on. Otherwise `auto_turn_on` turns them on in the Death Star mission and
 * while there is no directional light. They are never on in the hangar.
 *
 * The colour and the power in watts are those of a Blender area light, as
 * for the lights of the GLB models: P watts give P * 683 / pi candela
 * straight ahead. With an angle of 0, the light is one-sided, with a
 * cosine falloff. Above 0, the light is a spot light that fades from the
 * nose out to that angle. */

#ifdef __cplusplus
extern "C" {
#endif

struct XwaSnapshot;

/* Reads the `headlights` section of flight/flight.yaml. */
void XwaHeadlights_Init(void);

/* Reads `headlights` from the file of mission `missionPath` (for example
   "missions\1b7m4w.tie"). */
void XwaHeadlights_LoadMission(const char* missionPath);

/* Writes whether the headlights are on now, and the values that the mod
   sets, into the snapshot. */
void XwaHeadlights_Capture(struct XwaSnapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif
