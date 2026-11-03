#ifndef XWA_RUNTIME_MODELS_HANGAR_SCENE_H
#define XWA_RUNTIME_MODELS_HANGAR_SCENE_H

/* Modern port: builds the hangar scene from the hangar settings. While hangar
   settings are active, the hangar code calls these functions instead of its
   stock spawn code. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Spawns the layout, then the hangar's droids, shuttle and roof crane. In
   the family base (`family`), it spawns the work droid instead. */
void XwaHangarScene_Spawn(int family);
/* Shows the markings of `hangar.markings` on the hangar. By default, these
   are the command ship's markings. */
void XwaHangarScene_SetHangarMarkings(void);
/* Switches to the hangar of another command ship. The hangar model reloads
   in place and the scene spawns again. */
void XwaHangarScene_Rebuild(void);

/* Placement. The floor is at g_launchBaseZ. An object stands its craft
   elevation (XwaHangars_CraftElevation) above the floor. On the inverted
   floor, which is `hangar.inverted_floor_height_m` above the floor, it hangs
   that far below. */

/* Returns the floor height at setup, from where the hangar player
   `playerObjIdx` stands. */
int XwaHangarScene_FloorZ(int playerObjIdx);
/* Moves the launch reference back by the player's X/Y offset, so the player
   stands that far from its stock place. */
void XwaHangarScene_OffsetLaunchRef(int launchRefIdx, int playerObjIdx);
/* Returns the Z of an object of `objectType` that stands on its floor. The
   floor side comes from the layout entry being placed, else the hangar's. */
int XwaHangarScene_ObjectFloorZ(uint16_t objectType);

/* The hangar player `objIdx`. It uses the inverted floor when its craft is
   in `player.inverted_floor_crafts`, else the normal floor. */

/* Returns the player's offset on `axis`. */
int XwaHangarScene_PlayerOffset(int objIdx, int axis);
/* Returns the player's height above its floor at rest. */
int XwaHangarScene_PlayerRestElevation(int objIdx);
/* Returns the stock launch or re-entry height `elevation` plus `player.rise_m`
   or `player.inverted_rise_m`. */
int XwaHangarScene_PlayerAnimationElevation(int objIdx, int elevation);
/* Returns whether the player is more than `elevation` away from its floor. */
int XwaHangarScene_PlayerAbove(int objIdx, int elevation);
/* Moves the player `distance` away from its floor. */
void XwaHangarScene_PlayerRise(int objIdx, int distance);
/* Puts the player at rest on its floor, for a new craft. */
void XwaHangarScene_PlacePlayerAtRest(int objIdx);
/* For the re-entry: moves the player to its start, `player.straight_m`
   farther out; returns whether it is still on that straight line; moves it
   to its parking place on the launch rail. */
void XwaHangarScene_PlaceReentryStart(int objIdx);
int XwaHangarScene_ReentryStraight(int objIdx);
void XwaHangarScene_PlaceOnLaunchRail(int objIdx);

/* The shuttle (g_hangarShuttleState). */

/* Returns whether the scene has a shuttle. It has one when
   `shuttle.enabled` is set, or after EnsureShuttle spawned one. */
int XwaHangarScene_ShuttleLoaded(void);
/* Spawns the shuttle in the hangar when there is none, for a player
   without a craft, who then flies in it. */
void XwaHangarScene_EnsureShuttle(void);
/* The shuttle flies relative to its own floor, `shuttle.height_m` higher.
   These return its height at rest, a stock climb height plus
   `shuttle.path.rise_m`, and whether it is more than `elevation` away from
   its floor. ShuttleRise moves it away from its floor. */
int XwaHangarScene_ShuttleRestElevation(void);
int XwaHangarScene_ShuttleAnimationElevation(int elevation);
int XwaHangarScene_ShuttleAbove(int elevation);
void XwaHangarScene_ShuttleRise(int distance);
/* Turn the shuttle to heading 0, or back to `shuttle.heading_degrees`.
   They return 1 when the turn is done. */
int XwaHangarScene_ShuttleTurnOut(int dtTicks);
int XwaHangarScene_ShuttleTurnIn(int dtTicks);
/* Moves the shuttle `step` toward `shuttle.path.exit_side`. */
void XwaHangarScene_ShuttleSideStep(int step, int dtTicks);
/* Moves the player who flies in the shuttle to its re-entry start. */
void XwaHangarScene_PlaceShuttleReentry(int objIdx);

/* Returns whether the droid in scene slot `sceneIdx` moves (its `moves`). */
int XwaHangarScene_DroidUpdates(int sceneIdx);
/* Return the roof crane's coordinate on `roof_crane.axis`, and its next
   target `offset` away on that axis, kept within `roof_crane.travel_m`. */
int* XwaHangarScene_CraneCoordinate(void);
int XwaHangarScene_CraneTarget(int offset);

/* Gets the camera position of shot `shotIndex` for Hangar_SetCameraShot. It
   uses the settings (`cameras`, `family_base.cameras` and the player craft's
   `player_camera_m`) and the places of the droids and the crane. Returns 0
   when the stock code places the shot: the cockpit, the family base's work
   droid, and a player craft without its own camera. */
int XwaHangarScene_CameraShot(int shotIndex, int* x, int* y, int* z);
/* Gets the camera of the crane shot, above the crane on any axis. */
void XwaHangarScene_FollowCrane(int* x, int* y, int* z);

/* The scene state that changes during a flight, for films. */
typedef struct XwaHangarSceneFlightState {
	int32_t launchRefOffset[2];
	int32_t reentryStartY;
	uint8_t shuttleLoaded;
} XwaHangarSceneFlightState;
void XwaHangarScene_SaveFlightState(XwaHangarSceneFlightState* state);
void XwaHangarScene_RestoreFlightState(const XwaHangarSceneFlightState* state);

#ifdef __cplusplus
}
#endif

#endif
