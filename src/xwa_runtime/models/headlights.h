#ifndef XWA_RUNTIME_MODELS_HEADLIGHTS_H
#define XWA_RUNTIME_MODELS_HEADLIGHTS_H

/* Headlights: a light along the player craft's nose, for dark places such
 * as the Death Star. The `headlights` section of flight/flight.yaml gives
 * the light, and is read once at startup:
 *
 *   headlights:
 *     position_m: [0, 0, 20]      # from the craft's eye: X right, Y up, Z forward
 *     color: [0.944, 0.944, 1]    # linear
 *     power_w: 17671
 *     angle_degrees: 25
 *     auto_turn_on: true
 *
 * The light is one-sided and points along the nose. Its colour and its
 * power in watts are those of a Blender area light, as for the lights of
 * the GLB models. `angle_degrees` is the width of a cone, which isn't used
 * yet: the renderer has no cone lights. Without the section there are no
 * headlights. Every key but `power_w` is optional.
 *
 * A mission turns them on with `headlights: true` in its mission file
 * (mission_file.h): they are then on while the player isn't in the
 * hangar. In the other missions, with `auto_turn_on`, they are on while
 * there is no directional light. */

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
