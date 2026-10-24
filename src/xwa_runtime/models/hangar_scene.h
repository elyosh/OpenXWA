#ifndef XWA_RUNTIME_MODELS_HANGAR_SCENE_H
#define XWA_RUNTIME_MODELS_HANGAR_SCENE_H

/* Modern port: the hangar scene from the hangar settings. The hangar code calls
 * these instead of its stock spawns while settings are active. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The scene objects: the map layout, then the hangar's droids, shuttle and
   roof crane, or the family base's work droid (`family`). */
void XwaHangarScene_Spawn(int family);
/* The hangar shows the HangarIff markings, by default the command ship's. */
void XwaHangarScene_SetHangarMarkings(void);
/* The hangar of another command ship: the hangar model reloads in place and
   the scene spawns again. */
void XwaHangarScene_Rebuild(void);

/* Placement. The floor is g_launchBaseZ; an object stands its craft
   elevation (XwaHangars_CraftElevation) above it, or hangs that far below
   the inverted floor HangarFloorInvertedHeight above it. */

/* The floor at setup, from where the hangar player `playerObjIdx` stands. */
int XwaHangarScene_FloorZ(int playerObjIdx);
/* Moves the launch reference by minus the player's X/Y offset, so the player
   stands that far from its stock place. */
void XwaHangarScene_OffsetLaunchRef(int launchRefIdx, int playerObjIdx);
/* Where an object spawned on the floor stands (its map line's floor side). */
int XwaHangarScene_ObjectFloorZ(uint16_t objectType);

/* The hangar player `objIdx`, on its own floor side (the inverted floor for
   the types PlayerFloorInvertedModelIndices lists) with its offset: */
int XwaHangarScene_PlayerOffset(int objIdx, int axis);
int XwaHangarScene_PlayerRestElevation(int objIdx);
/* the stock launch or re-entry height `elevation` plus PlayerAnimation(Inverted)Elevation; */
int XwaHangarScene_PlayerAnimationElevation(int objIdx, int elevation);
/* whether it stands more than `elevation` off its floor; */
int XwaHangarScene_PlayerAbove(int objIdx, int elevation);
/* moves it `distance` away from its floor; */
void XwaHangarScene_PlayerRise(int objIdx, int distance);
/* puts it at rest on its floor (a new craft); */
void XwaHangarScene_PlacePlayerAtRest(int objIdx);
/* the re-entry: its start, PlayerAnimationStraightLine farther out; whether
   it is still on that straight line; its parking place on the launch rail. */
void XwaHangarScene_PlaceReentryStart(int objIdx);
int XwaHangarScene_ReentryStraight(int objIdx);
void XwaHangarScene_PlaceOnLaunchRail(int objIdx);

/* The shuttle (g_hangarShuttleState): whether the scene has one (LoadShuttle);
   EnsureShuttle spawns it for a player with no craft, who flies in it. */
int XwaHangarScene_ShuttleLoaded(void);
void XwaHangarScene_EnsureShuttle(void);
/* Its traffic, on its floor side, ShuttlePositionZ higher: its height at
   rest, a stock climb height plus ShuttleAnimationElevation, whether it is
   more than `elevation` off its floor, a move away from it; */
int XwaHangarScene_ShuttleRestElevation(void);
int XwaHangarScene_ShuttleAnimationElevation(int elevation);
int XwaHangarScene_ShuttleAbove(int elevation);
void XwaHangarScene_ShuttleRise(int distance);
/* its turns out to heading 0 and back in to ShuttleOrientation (1 once
   done), its sideways step along ShuttleAnimation; */
int XwaHangarScene_ShuttleTurnOut(int dtTicks);
int XwaHangarScene_ShuttleTurnIn(int dtTicks);
void XwaHangarScene_ShuttleSideStep(int step, int dtTicks);
/* the re-entry start of the player flying in it. */
void XwaHangarScene_PlaceShuttleReentry(int objIdx);

/* Whether the droid of scene slot `sceneIdx` moves (Droid1/2Update). */
int XwaHangarScene_DroidUpdates(int sceneIdx);
/* The roof crane's coordinate on HangarRoofCraneAxis, and its next target
   there `offset` away, within HangarRoofCraneLow/HighOffset. */
int* XwaHangarScene_CraneCoordinate(void);
int XwaHangarScene_CraneTarget(int offset);

/* Hangar_SetCameraShot's position for shot `shotIndex` from the camera
   sections ([HangarCamera], [FamHangarCamera], the player craft's [Camera])
   and the droids' and crane's places; 0 for a shot the stock code places
   (the cockpit, the family work droid, a player craft without [Camera]). */
int XwaHangarScene_CameraShot(int shotIndex, int* x, int* y, int* z);
/* The crane shot's camera, above the crane on any axis. */
void XwaHangarScene_FollowCrane(int* x, int* y, int* z);

#ifdef __cplusplus
}
#endif

#endif
