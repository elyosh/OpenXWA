#ifndef XWA_RUNTIME_MODELS_COCKPIT_ALERT_LIGHTS_H
#define XWA_RUNTIME_MODELS_COCKPIT_ALERT_LIGHTS_H

/* Cockpit alert lights: the game blinks six coloured lights at the eye of
 * the player's seat (FlightLight_InitLocalPlayerPulses). The
 * `cockpit_alert_lights` section of the mod's flight settings replaces them
 * in the HD renderer, and is read once at startup. The game still decides
 * when a light is on and how it blinks.
 *
 * Each light is a Blender point light: P watts give P * 683 / (4 pi)
 * candela all around. A light that isn't in the section keeps the game's
 * light. The game's own code and the classic renderers keep the game's
 * lights, so the section only changes the display. */

#ifdef __cplusplus
extern "C" {
#endif

struct XwaSnapshot;

/* Reads the `cockpit_alert_lights` section of flight/flight.yaml. */
void XwaCockpitAlertLights_Init(void);

/* Writes the lights into the snapshot's light pulses. */
void XwaCockpitAlertLights_Capture(struct XwaSnapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif
