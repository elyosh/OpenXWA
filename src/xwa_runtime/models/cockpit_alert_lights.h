#ifndef XWA_RUNTIME_MODELS_COCKPIT_ALERT_LIGHTS_H
#define XWA_RUNTIME_MODELS_COCKPIT_ALERT_LIGHTS_H

/* Cockpit alert lights: the game blinks six coloured lights at the eye of
 * the player's seat (FlightLight_InitLocalPlayerPulses). The
 * `cockpit_alert_lights` section of flight/flight.yaml replaces them in the
 * HD renderer, and is read once at startup:
 *
 *   cockpit_alert_lights:
 *     hull_critical:            # shields down and under a third of the hull left
 *       position_m: [0, 0.3, 0.6]   # from the eye: X right, Y up, Z forward
 *       color: [1, 0, 0]            # linear
 *       power_w: 20
 *     warhead_lock: ...         # an enemy locks a warhead on the craft
 *     beam_warning: ...         # a tractor or jamming beam targets the craft
 *     hyperspace_jump: ...      # the jump to hyperspace starts
 *     hyperspace_transfer: ...  # the jump between regions
 *     hyperspace_arrival: ...   # the craft leaves hyperspace
 *
 * Each light is a Blender point light: P watts give P * 683 / (4 pi)
 * candela all around. The game still decides when a light is on and how it
 * blinks. `position_m` is at the eye without it, and `color` is the game's
 * colour. `power_w` is required. A light that isn't in the section keeps the
 * game's light. The game's own code and the classic renderers keep the
 * game's lights. */

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
