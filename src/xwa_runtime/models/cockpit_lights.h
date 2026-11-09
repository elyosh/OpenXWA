#ifndef XWA_RUNTIME_MODELS_COCKPIT_LIGHTS_H
#define XWA_RUNTIME_MODELS_COCKPIT_LIGHTS_H

/* Cockpit lights: the game blinks six coloured lights at the eye of
 * the player's seat (FlightLight_InitCockpitLights). The HD renderer
 * shows each one as a Blender point light: P watts give P * 683 / (4 pi)
 * candela in every direction. The default values are in
 * `point_lights.cockpit_lights` of resources/remaster/config.yaml.
 * A mod can change them in the `cockpit_lights` section of its flight
 * settings. Each key that the mod sets replaces the default. The section is
 * read once at startup. The game still decides when a light is on and how
 * it blinks. The game's own code and the classic renderers still use the
 * game's lights, so these settings only change what the HD renderer
 * shows. */

#ifdef __cplusplus
extern "C" {
#endif

struct XwaSnapshot;

/* Returns the slot of the light called `name`, or -1 when there is no such
   light. */
int XwaCockpitLights_Find(const char* name);

/* Returns the name of the light in `slot`, from 0 to
   XWA_SNAP_COCKPIT_LIGHT_COUNT - 1, or NULL for any other slot. */
const char* XwaCockpitLights_Name(int slot);

/* Reads the `cockpit_lights` section of flight/flight.yaml. */
void XwaCockpitLights_Init(void);

/* Writes the values that the mod sets into the snapshot's cockpit lights. */
void XwaCockpitLights_Capture(struct XwaSnapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif
