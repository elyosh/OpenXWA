#include "xwa_runtime/models/hangar_scene.h"

#include "aeron/log.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/object_type_info.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/hangar.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/math/scalar.h"
#include "xwa/render/renderer.h"
#include "xwa/util/random.h"
#include "xwa_runtime/models/hangars.h"
#include "xwa_runtime/models/object_profiles.h"
#include "xwa_runtime/models/skins.h"

#include <string.h>

/* Modern port module; no original counterpart. It builds the hangar scene
   from the hangar settings, with the same spawns as Hangar_SetupReadyScene,
   and places the objects and the player on the settings' floor. */

/* Whether the layout object being placed is on the inverted floor, or -1 to
   use the hangar floor's side. */
static int s_spawnFloorInverted = -1;
/* The player's X/Y offset that is applied to the launch reference. */
static int s_launchRefOffset[2];
/* The re-entry start Y. */
static int s_reentryStartY;
/* Whether the scene has a shuttle (`shuttle.enabled`, or the player needed one). */
static int s_shuttleLoaded;

/* The number of free slots in the craft range of the active region. */
static int HangarScene_FreeCraftSlots(void) {
	const ObjectSlotRange* craft = &g_objectSlotRangeByGenus[GENUS_Fighter];
	uint32_t objIdx;
	int count = 0;

	for (objIdx = craft->next; objIdx < craft->end; ++objIdx) {
		if (g_objectTable[objIdx].objectType == 0) {
			++count;
		}
	}
	return count;
}

/* Whether objects of `objectType` take their slot from the craft range. */
static int HangarScene_UsesCraftSlots(uint16_t objectType) {
	const ObjectSlotRange* craft = &g_objectSlotRangeByGenus[GENUS_Fighter];
	const ObjectSlotRange* range = &g_objectSlotRangeByGenus[g_objectTypes[objectType].genusId];

	return range->next == craft->next && range->end == craft->end;
}

/* Counts the craft slots that the scene still needs after the layout. In the
   family base, these are for the work droid. In the hangar, they are for the
   droids, the shuttle and the roof crane. Both also count the player's
   craft. */
static int HangarScene_CraftSlotsNeededAfterLayout(int family) {
	const XwaHangarObjects* objects = XwaHangars_Objects();

	if (family) {
		return 2;
	}
	return (objects->droids[0].load != 0) + (objects->droids[1].load != 0) + 3;
}

/* Places a layout object with its markings and object profile, when the
   craft slots allow it. Returns its object, or -1 when it isn't placed. */
static int HangarScene_SpawnLayoutEntry(const XwaHangarMapEntry* entry, int* freeSlots, int* skipped) {
	const int usesCraftSlots = HangarScene_UsesCraftSlots(entry->objectType);
	uint16_t objIdx;

	if (usesCraftSlots && *freeSlots <= 0) {
		++*skipped;
		return -1;
	}
	s_spawnFloorInverted = entry->isFloorInverted;
	objIdx = (uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(entry->objectType, entry->position[0],
															 entry->position[1], entry->position[2],
															 entry->headingXY, entry->headingZ);
	s_spawnFloorInverted = -1;
	if (objIdx == 0xffffu || objIdx == 0) {
		return -1;
	}
	if (usesCraftSlots) {
		--*freeSlots;
	}
	g_objectTable[objIdx].mobj->nodeSwitchIndex = (uint8_t)entry->markings;
	XwaSkins_UseMarkings(entry->objectType, g_objectTable[objIdx].mobj->nodeSwitchIndex);
	XwaObjectProfiles_ApplyNamed(entry->objectType, entry->objectProfile, g_objectTable[objIdx].mobj->pCraft);
	return objIdx;
}

/* Spawns the layout: the crane arm first, then the parked objects. The first
   parked hangar monitor and hangar generator become the droids' targets, like
   these objects in the stock layout.
   The layout shares the craft slots of the hangar's region with the rest of
   the scene and the player's craft. When it needs more slots than the region
   has free, its last objects aren't placed, so that the player's craft
   always gets a slot. */
static void HangarScene_SpawnLayout(int family) {
	const XwaHangarMapEntry* craneArm = XwaHangars_CraneArm(family);
	const XwaHangarMapEntry* map;
	int count;
	int index;
	int freeSlots;
	int skipped = 0;

	map = XwaHangars_Map(family, &count);
	freeSlots = HangarScene_FreeCraftSlots() - HangarScene_CraftSlotsNeededAfterLayout(family);
	/* Without a crane arm, the hangar code skips the crane. */
	g_hangarCraneObjIdx =
		craneArm != NULL ? HangarScene_SpawnLayoutEntry(craneArm, &freeSlots, &skipped) : -1;
	g_hangarDroidTargetObjIdx = -1;
	g_hangarDroidRouteTargetObjIdx = -1;
	g_unusedHangarWorkStandObjIdx = -1;
	for (index = 0; map != NULL && index < count; ++index) {
		const int objIdx = HangarScene_SpawnLayoutEntry(&map[index], &freeSlots, &skipped);

		if (objIdx < 0) {
			continue;
		}
		if (map[index].objectType == OBJ_HangarMonitor && g_hangarDroidTargetObjIdx < 0) {
			g_hangarDroidTargetObjIdx = objIdx;
		} else if (map[index].objectType == OBJ_HangarGenerator && g_hangarDroidRouteTargetObjIdx < 0) {
			g_hangarDroidRouteTargetObjIdx = objIdx;
		} else if (map[index].objectType == OBJ_HangarWorkStand && g_unusedHangarWorkStandObjIdx < 0) {
			g_unusedHangarWorkStandObjIdx = objIdx;
		}
	}
	g_unusedHangarWorkStandObjIdxMirror = g_unusedHangarWorkStandObjIdx;
	if (skipped != 0) {
		Aeron_LogWarn(
			"xwa.models",
			"hangar map: %d object(s) not placed, because the hangar's region has no free craft slots "
			"left for them",
			skipped);
	}
}

/* Spawns `objectType` on the floor, or below the inverted floor when
   `inverted` is set, then raises it by `heightZ`. */
static uint16_t HangarScene_SpawnOnFloor(uint16_t objectType, int x, int y, int heightZ, uint16_t headingXY,
										 int inverted) {
	uint16_t objIdx;

	s_spawnFloorInverted = inverted;
	objIdx = (uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(objectType, x, y, 0x7fffffff, headingXY, 0);
	s_spawnFloorInverted = -1;
	if (objIdx != 0xffffu && objIdx != 0) {
		g_objectTable[objIdx].world_z += heightZ;
		g_objectTable[objIdx].mobj->prevWorldZ = g_objectTable[objIdx].world_z;
	}
	return objIdx;
}

/* Gives a spawned object its markings and object profile. */
static void HangarScene_ApplyMarkingsAndProfile(uint16_t objIdx, uint16_t objectType, int markings,
												const char* profile) {
	if (objIdx != 0xffffu && objIdx != 0) {
		g_objectTable[objIdx].mobj->nodeSwitchIndex = (uint8_t)markings;
		XwaSkins_UseMarkings(objectType, g_objectTable[objIdx].mobj->nodeSwitchIndex);
		XwaObjectProfiles_ApplyNamed(objectType, profile, g_objectTable[objIdx].mobj->pCraft);
	}
}

/* Spawns the shuttle at its place on the floor, with its markings and object
   profile. */
static void HangarScene_SpawnShuttle(void) {
	const XwaHangarObjects* objects = XwaHangars_Objects();
	const uint16_t objectType = objects->shuttleObjectType != 0 ? objects->shuttleObjectType : OBJ_Shuttle;

	g_hangarShuttleState.objectIdx = HangarScene_SpawnOnFloor(
		objectType, objects->shuttlePosition[0], objects->shuttlePosition[1], objects->shuttlePosition[2],
		objects->shuttleOrientation, objects->isShuttleFloorInverted);
	FlightObject_InitMeshAnimationDefaults(g_hangarShuttleState.objectIdx);
	HangarScene_ApplyMarkingsAndProfile((uint16_t)g_hangarShuttleState.objectIdx, objectType,
										objects->shuttleMarkings, objects->shuttleObjectProfile);
	s_shuttleLoaded = 1;
}

/* Spawns the layout, then the hangar's droids, shuttle and roof crane, or the
   family base's work droid. It draws the same random numbers in the same
   order as Hangar_SetupReadyScene. */
void XwaHangarScene_Spawn(int family) {
	const XwaHangarObjects* objects = XwaHangars_Objects();
	int sceneSlot;
	int droid;

	HangarScene_SpawnLayout(family);
	if (family) {
		sceneSlot = g_hangarSceneObjectCount;
		g_hangarSceneObjects[sceneSlot].objectIdx =
			(uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(0x1e9, 1208, -6983, -4831, 0, 0);
		g_hangarSceneObjects[sceneSlot].moveState = 0;
		g_hangarSceneObjects[sceneSlot].nextDecisionTime = 0;
		g_hangarSceneObjects[sceneSlot].moveSpeed = 0.0f;
		g_hangarSceneObjects[sceneSlot].targetObjIdx = 0;
		g_hangarSceneObjects[sceneSlot].routeNodeIdx = 0;
		g_hangarSceneObjects[sceneSlot].prevRouteNodeIdx = 0;
		g_hangarSceneObjects[sceneSlot].meshRotationDir = 0;
		++g_hangarSceneObjectCount;
		return;
	}

	/* The droids: when neither droid is enabled, there are no droid slots. A
	   droid that isn't enabled keeps its slot, with object 0 and no update.
	   Both droids keep their stock places. */
	if (objects->droids[0].load || objects->droids[1].load) {
		static const int places[2][3] = { { 227, 351, 58736 }, { -70, 400, 0 } };

		for (droid = 0; droid < 2; ++droid) {
			const XwaHangarDroid* settings = &objects->droids[droid];
			uint16_t objIdx = 0;

			if (settings->load) {
				objIdx = HangarScene_SpawnOnFloor(settings->objectType, places[droid][0], places[droid][1],
												  settings->positionZ, (uint16_t)places[droid][2],
												  settings->isFloorInverted);
				HangarScene_ApplyMarkingsAndProfile(objIdx, settings->objectType, settings->markings,
													settings->objectProfile);
			}
			sceneSlot = g_hangarSceneObjectCount;
			g_hangarSceneObjects[sceneSlot].objectIdx = objIdx;
			g_hangarSceneObjects[sceneSlot].moveState = 0;
			g_hangarSceneObjects[sceneSlot].nextDecisionTime = 0;
			g_hangarSceneObjects[sceneSlot].moveSpeed = 0.0f;
			g_hangarSceneObjects[sceneSlot].targetObjIdx = 0;
			++g_hangarSceneObjectCount;
		}
	}

	/* The shuttle: when `shuttle.enabled` is false, there is no shuttle
	   (object 0), but the same random numbers are drawn. */
	g_hangarShuttleState.objectIdx = 0;
	s_shuttleLoaded = 0;
	if (objects->loadShuttle) {
		HangarScene_SpawnShuttle();
	}
	g_hangarShuttleState.moveState = 0;
	g_hangarShuttleState.nextDecisionTime = ((uint16_t)GameRand() >> 1) + 10000;
	g_hangarShuttleState.moveSpeed = 0.0f;
	g_hangarShuttleState.targetObjIdx = 0;

	/* The roof crane, at the place that the settings give. */
	g_hangarRoofCraneState.objectIdx = (uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(
		0x13c, objects->roofCranePosition[0], objects->roofCranePosition[1], objects->roofCranePosition[2], 0,
		0);
	g_hangarRoofCraneState.moveState = 0;
	{
		uint16_t decisionSeed = (uint16_t)GameRand();
		g_hangarRoofCraneState.moveSpeed = 0.0f;
		g_hangarRoofCraneState.nextDecisionTime = decisionSeed >> 1;
	}
	g_hangarRoofCraneState.targetObjIdx = 0;
}

/* Shows the markings of `hangar.markings` on the hangar. By default, these
   are the command ship's markings. */
void XwaHangarScene_SetHangarMarkings(void) {
	const uint8_t hangarIff = XwaHangars_Objects()->hangarIff;

	g_objectTable[g_launchRefObjIdx].mobj->nodeSwitchIndex =
		hangarIff != 0xFF ? hangarIff : XwaHangars_CommandShipMarkings();
	XwaSkins_UseMarkings(g_objectTable[g_launchRefObjIdx].objectType,
						 g_objectTable[g_launchRefObjIdx].mobj->nodeSwitchIndex);
}

/* Switches to the hangar of another command ship. The hangar model reloads in
   place, the floor moves to the new model's height, and the hangar moves for
   the new player offset. The objects of the last scene are released and the
   scene spawns again. */
void XwaHangarScene_Rebuild(void) {
	ObjectRecord* launchRef = &g_objectTable[g_launchRefObjIdx];
	const uint8_t hangarFlightGroup = launchRef->flightGroupIdx;
	uint32_t objIdx;

	g_hangarBackdropObjectType = g_pilotData.hangarType ? 179 : OBJ_Hangar;
	launchRef->objectType = (ObjectTypeId)g_hangarBackdropObjectType;
	XwaHangars_ReleaseStaleModel((uint16_t)g_hangarBackdropObjectType);
	if (!g_loadedModels.byObjectType[(uint16_t)g_hangarBackdropObjectType]) {
		Hangar_LoadCraftModelByType((ObjectTypeId)g_hangarBackdropObjectType);
		Math_SetFpuSinglePrecisionMode();
	}
	g_launchBaseZ = launchRef->world_z +
					g_modelDefs[(uint16_t)GetModelDefIndexFromType((ObjectTypeId)g_hangarBackdropObjectType)]
						.meshAttachData[6];
	/* Undo the setup offset, then apply the entering craft's. */
	launchRef->world_x += s_launchRefOffset[0];
	launchRef->world_y += s_launchRefOffset[1];
	XwaHangarScene_OffsetLaunchRef(g_launchRefObjIdx, g_players[g_localPlayer].objectIndex);
	launchRef->mobj->prevWorldX = launchRef->world_x;
	launchRef->mobj->prevWorldY = launchRef->world_y;
	XwaHangarScene_SetHangarMarkings();
	if (g_hangarBackdropObjectType == OBJ_Hangar) {
		int16_t hangarModelIdx = GetModelDefIndexFromType(OBJ_Hangar);
		double worldX = (double)launchRef->world_x;
		float worldY = (float)launchRef->world_y;
		RenderScene_EnableProjectionYClamp(
			(float)((double)g_modelDefs[(uint16_t)hangarModelIdx].childMountPoints[5] + worldY),
			(float)((double)g_modelDefs[(uint16_t)hangarModelIdx].childMountPoints[2] + worldY),
			(float)((double)g_modelDefs[(uint16_t)hangarModelIdx].childMountPoints[3] + worldX),
			(float)((double)g_modelDefs[(uint16_t)hangarModelIdx].childMountPoints[0] + worldX));
	} else {
		RenderScene_DisableProjectionYClamp();
	}

	for (objIdx = 0; objIdx < g_objectTableSlotCount; ++objIdx) {
		ObjectRecord* object = &g_objectTable[objIdx];

		if (object->objectType == 0 || object->flightGroupIdx != hangarFlightGroup ||
			object->objectType == OBJ_Hangar || object->objectType == 179 || object->objectSignature == 0) {
			continue;
		}
		Object_ClearSlotState(objIdx);
		object->objectSignature = 0;
	}
	Mission_SetActiveRegionObjectRanges(launchRef->regionIdx);
	g_hangarSceneObjectCount = 0;
	XwaHangarScene_Spawn(g_hangarBackdropObjectType != OBJ_Hangar);
}

/* ---- placement: elevations, the player's offset and its animations ------ */

static int HangarScene_PlayerOnInvertedFloor(int objIdx) {
	return XwaHangars_PlayerFloorInverted(g_objectTable[objIdx].objectType);
}

/* Returns the player's height above the floor when it is `elevation` away
   from its own floor, including its offset. */
static int HangarScene_PlayerHeight(int objIdx, int elevation) {
	if (HangarScene_PlayerOnInvertedFloor(objIdx)) {
		return XwaHangars_Objects()->hangarFloorInvertedHeight + XwaHangarScene_PlayerOffset(objIdx, 2) -
			   elevation;
	}
	return XwaHangarScene_PlayerOffset(objIdx, 2) + elevation;
}

/* Puts the floor below the player by the player's height at rest. The player
   then rests on the floor at mission start, as in the stock game. */
int XwaHangarScene_FloorZ(int playerObjIdx) {
	return g_objectTable[playerObjIdx].world_z -
		   HangarScene_PlayerHeight(playerObjIdx, XwaHangarScene_PlayerRestElevation(playerObjIdx));
}

/* Moves the launch reference by the player's X/Y offset, and keeps the
   offset for a rebuild. */
void XwaHangarScene_OffsetLaunchRef(int launchRefIdx, int playerObjIdx) {
	s_launchRefOffset[0] = XwaHangarScene_PlayerOffset(playerObjIdx, 0);
	s_launchRefOffset[1] = XwaHangarScene_PlayerOffset(playerObjIdx, 1);
	g_objectTable[launchRefIdx].world_x -= s_launchRefOffset[0];
	g_objectTable[launchRefIdx].world_y -= s_launchRefOffset[1];
}

/* Returns the Z of an object of `objectType` that stands on its floor. */
int XwaHangarScene_ObjectFloorZ(uint16_t objectType) {
	const XwaHangarObjects* objects = XwaHangars_Objects();
	const int inverted = s_spawnFloorInverted >= 0 ? s_spawnFloorInverted : objects->isHangarFloorInverted;

	if (inverted) {
		return g_launchBaseZ + objects->hangarFloorInvertedHeight - XwaHangars_CraftElevation(objectType, 1);
	}
	return g_launchBaseZ + XwaHangars_CraftElevation(objectType, 0);
}

int XwaHangarScene_PlayerOffset(int objIdx, int axis) {
	int offset[3];

	XwaHangars_PlayerOffset(g_objectTable[objIdx].objectType, offset);
	return offset[axis];
}

int XwaHangarScene_PlayerRestElevation(int objIdx) {
	return XwaHangars_CraftElevation(g_objectTable[objIdx].objectType,
									 HangarScene_PlayerOnInvertedFloor(objIdx));
}

int XwaHangarScene_PlayerAnimationElevation(int objIdx, int elevation) {
	const XwaHangarObjects* objects = XwaHangars_Objects();

	return elevation + (HangarScene_PlayerOnInvertedFloor(objIdx) ? objects->playerAnimationInvertedElevation
																  : objects->playerAnimationElevation);
}

int XwaHangarScene_PlayerAbove(int objIdx, int elevation) {
	const int z =
		g_objectTable[objIdx].world_z - (g_launchBaseZ + HangarScene_PlayerHeight(objIdx, elevation));

	return HangarScene_PlayerOnInvertedFloor(objIdx) ? z < 0 : z > 0;
}

void XwaHangarScene_PlayerRise(int objIdx, int distance) {
	g_objectTable[objIdx].world_z += HangarScene_PlayerOnInvertedFloor(objIdx) ? -distance : distance;
}

/* Puts the player at its rest elevation over the floor. */
void XwaHangarScene_PlacePlayerAtRest(int objIdx) {
	g_objectTable[objIdx].world_z =
		g_launchBaseZ + HangarScene_PlayerHeight(objIdx, XwaHangarScene_PlayerRestElevation(objIdx));
}

/* Moves the player to its re-entry start, after the stock code sets its X/Y.
   It adds the player's offset and `player.straight_m`, and sets the height
   from `player.rise_m` or `player.inverted_rise_m`. */
void XwaHangarScene_PlaceReentryStart(int objIdx) {
	ObjectRecord* object = &g_objectTable[objIdx];

	object->world_x += XwaHangarScene_PlayerOffset(objIdx, 0);
	object->world_y +=
		XwaHangarScene_PlayerOffset(objIdx, 1) + XwaHangars_Objects()->playerAnimationStraightLine;
	s_reentryStartY = object->world_y;
	object->world_z = g_launchBaseZ + HangarScene_PlayerHeight(
										  objIdx, XwaHangarScene_PlayerAnimationElevation(objIdx, 0x223));
}

/* Whether the re-entering player is still on its straight line. It doesn't
   descend before it reaches the stock start. */
int XwaHangarScene_ReentryStraight(int objIdx) {
	return g_objectTable[objIdx].world_y > s_reentryStartY - XwaHangarScene_PlayerOffset(objIdx, 1) -
											   XwaHangars_Objects()->playerAnimationStraightLine;
}

/* Puts the re-entered player on the launch rail, after the stock placement. */
void XwaHangarScene_PlaceOnLaunchRail(int objIdx) {
	ObjectRecord* object = &g_objectTable[objIdx];

	object->world_x += XwaHangarScene_PlayerOffset(objIdx, 0);
	object->world_y = g_objectTable[g_launchRefObjIdx].world_y +
					  (g_hangarBackdropObjectType == OBJ_Hangar ? -800 : -6800) +
					  XwaHangarScene_PlayerOffset(objIdx, 1);
	XwaHangarScene_PlacePlayerAtRest(objIdx);
}

/* ---- shuttle, droids and roof crane ------------------------------------- */

int XwaHangarScene_ShuttleLoaded(void) { return s_shuttleLoaded; }

void XwaHangarScene_EnsureShuttle(void) {
	if (!s_shuttleLoaded && g_hangarBackdropObjectType == OBJ_Hangar) {
		HangarScene_SpawnShuttle();
	}
}

/* Returns the shuttle's height above the floor when it is `elevation` away
   from its own floor, plus `shuttle.height_m`. On the inverted floor,
   `inverted_floor_height_m` applies both when it spawns and when it flies. */
static int HangarScene_ShuttleHeight(int elevation) {
	const XwaHangarObjects* objects = XwaHangars_Objects();

	return (objects->isShuttleFloorInverted ? objects->hangarFloorInvertedHeight - elevation : elevation) +
		   objects->shuttlePosition[2];
}

int XwaHangarScene_ShuttleRestElevation(void) {
	return XwaHangars_CraftElevation(g_objectTable[g_hangarShuttleState.objectIdx].objectType,
									 XwaHangars_Objects()->isShuttleFloorInverted);
}

int XwaHangarScene_ShuttleAnimationElevation(int elevation) {
	return elevation + XwaHangars_Objects()->shuttleAnimationElevation;
}

int XwaHangarScene_ShuttleAbove(int elevation) {
	const int z = g_objectTable[g_hangarShuttleState.objectIdx].world_z -
				  (g_launchBaseZ + HangarScene_ShuttleHeight(elevation));

	return XwaHangars_Objects()->isShuttleFloorInverted ? z < 0 : z > 0;
}

void XwaHangarScene_ShuttleRise(int distance) {
	g_objectTable[g_hangarShuttleState.objectIdx].world_z +=
		XwaHangars_Objects()->isShuttleFloorInverted ? -distance : distance;
}

/* Turns the shuttle toward heading 0 the short way. Returns 1 when done. */
int XwaHangarScene_ShuttleTurnOut(int dtTicks) {
	ObjectRecord* shuttle = &g_objectTable[g_hangarShuttleState.objectIdx];
	int heading = shuttle->yaw;

	heading += heading >= 0x8000 ? 50 * dtTicks : -50 * dtTicks;
	shuttle->yaw = (uint16_t)heading;
	return heading <= 0 || heading >= 0x10000;
}

/* Turns the shuttle toward `shuttle.heading_degrees`. Returns 1 when done. */
int XwaHangarScene_ShuttleTurnIn(int dtTicks) {
	ObjectRecord* shuttle = &g_objectTable[g_hangarShuttleState.objectIdx];
	const int orientation =
		XwaHangars_Objects()->shuttleOrientation != 0 ? XwaHangars_Objects()->shuttleOrientation : 0x10000;
	int heading = shuttle->yaw;

	heading += orientation >= 0x8000 ? 50 * dtTicks : -50 * dtTicks;
	shuttle->yaw = (uint16_t)heading;
	return orientation >= 0x8000 ? heading >= orientation : heading <= orientation;
}

/* Moves the shuttle `step` away from its landing place: sideways for
   `right`, up for `top` and down for `bottom`. For `top` and `bottom`, it
   also rolls the shuttle. */
void XwaHangarScene_ShuttleSideStep(int step, int dtTicks) {
	ObjectRecord* shuttle = &g_objectTable[g_hangarShuttleState.objectIdx];

	switch (XwaHangars_Objects()->shuttleAnimation) {
		case XWA_HANGAR_SHUTTLE_TOP:
			shuttle->roll = (uint16_t)(shuttle->roll + 20 * dtTicks);
			shuttle->world_z += step;
			break;
		case XWA_HANGAR_SHUTTLE_BOTTOM:
			shuttle->roll = (uint16_t)(shuttle->roll + 20 * dtTicks);
			shuttle->world_z -= step;
			break;
		default:
			shuttle->world_x += step;
			break;
	}
}

/* Moves the shuttle to its re-entry start, after the stock placement. */
void XwaHangarScene_PlaceShuttleReentry(int objIdx) {
	const XwaHangarObjects* objects = XwaHangars_Objects();
	ObjectRecord* object = &g_objectTable[objIdx];

	object->world_x = g_objectTable[g_launchRefObjIdx].world_x + objects->shuttlePosition[0];
	object->world_y = g_objectTable[g_launchRefObjIdx].world_y + 9000 + objects->shuttleAnimationStraightLine;
	object->world_z =
		g_launchBaseZ + HangarScene_ShuttleHeight(XwaHangarScene_ShuttleAnimationElevation(0x161));
}

int XwaHangarScene_DroidUpdates(int sceneIdx) {
	return sceneIdx < 2 && XwaHangars_Objects()->droids[sceneIdx].update;
}

/* Returns the roof crane's axis (`roof_crane.axis`). */
static int HangarScene_CraneAxis(void) {
	const int axis = XwaHangars_Objects()->roofCraneAxis;

	return axis == 1 || axis == 2 ? axis : 0;
}

static int* HangarScene_AxisCoordinate(ObjectRecord* object, int axis) {
	return axis == 1 ? &object->world_y : axis == 2 ? &object->world_z : &object->world_x;
}

/* Returns the roof crane's coordinate along its axis. */
int* XwaHangarScene_CraneCoordinate(void) {
	return HangarScene_AxisCoordinate(&g_objectTable[g_hangarRoofCraneState.objectIdx],
									  HangarScene_CraneAxis());
}

/* Returns the roof crane's next target, `offset` away along its axis. When
   the target is outside the crane's travel, the step goes the other way, and
   it is clamped to the travel. */
int XwaHangarScene_CraneTarget(int offset) {
	const XwaHangarObjects* objects = XwaHangars_Objects();
	const int axis = HangarScene_CraneAxis();
	const int current = *XwaHangarScene_CraneCoordinate();
	const int place = *HangarScene_AxisCoordinate(&g_objectTable[g_launchRefObjIdx], axis) +
					  objects->roofCranePosition[axis];
	const int low = place + objects->roofCraneLowOffset;
	const int high = place + objects->roofCraneHighOffset;
	int step = current + offset > high || current + offset < low ? -offset : offset;

	if (step > high - current) {
		step = high - current;
	}
	if (step < low - current) {
		step = low - current;
	}
	return current + step;
}

/* ---- cameras ------------------------------------------------------------ */

/* The stock offsets from the launch reference of the fixed shots. */
static const int s_stockShots[2][10][3] = {
	{ { 0 },
	  { 1130, -2320, -300 },
	  { 1240, -330, -700 },
	  { -1120, 1360, -790 },
	  { 0 },
	  { 0 },
	  { -1200, -1530, -850 },
	  { 0 },
	  { 0 },
	  { 1070, 4640, -130 } },
	{ { 0 },
	  { 780, -6471, -4977 },
	  { -1970, -8810, -4707 },
	  { 2510, -5391, -5067 },
	  { 0 },
	  { 0 },
	  { 1740, -8461, -5047 },
	  { 3180, 2629, -3777 },
	  { 8242, 6600, 10 },
	  { -13360, 35019, -6537 } },
};

/* Places a fixed shot from the camera key of the settings, else from the
   stock shot. */
static void HangarScene_PlaceFixedShot(int family, int shot, int* x, int* y, int* z) {
	const XwaHangarCamera* camera = XwaHangars_Camera(family);
	const int* offset = camera->set[shot] ? camera->offsets[shot] : s_stockShots[family][shot];
	const ObjectRecord* launchRef = &g_objectTable[g_launchRefObjIdx];

	*x = launchRef->world_x + offset[0];
	*y = launchRef->world_y + offset[1];
	*z = launchRef->world_z + offset[2];
}

/* Places the player craft's own camera. Returns 0 when the craft has none,
   and the caller then uses the stock shot for its type. */
static int HangarScene_PlacePlayerCraftShot(int family, int* x, int* y, int* z) {
	const ObjectRecord* player = &g_objectTable[g_hangarPlayerObjIdx];
	int offset[3];

	if (!XwaHangars_CraftCamera(player->objectType, family,
								XwaHangars_PlayerFloorInverted(player->objectType), offset)) {
		return 0;
	}
	*x = player->world_x + offset[0];
	*y = player->world_y + offset[1];
	*z = player->world_z + offset[2];
	return 1;
}

int XwaHangarScene_CameraShot(int shotIndex, int* x, int* y, int* z) {
	const int family = g_hangarBackdropObjectType != OBJ_Hangar;
	const int shot = (uint16_t)shotIndex;

	if (!family) {
		switch (shot) {
			case 1:
			case 2:
			case 3:
			case 6:
			case 9:
				HangarScene_PlaceFixedShot(0, shot, x, y, z);
				return 1;
			case 4:
			case 5:
				/* When the droid isn't loaded, the shot of key 3 is used. */

				if (!XwaHangars_Objects()->droids[shot - 4].load) {
					HangarScene_PlaceFixedShot(0, 3, x, y, z);
					return 1;
				}
				g_hangarCamFocusObj = g_hangarSceneObjects[shot - 4].objectIdx;
				*x = g_objectTable[g_hangarCamFocusObj].world_x;
				*y = g_objectTable[g_hangarCamFocusObj].world_y;
				*z = g_objectTable[g_hangarCamFocusObj].world_z + 30;
				return 1;
			case 7:
				if (g_hangarPlayerObjIdx == 0xffff || g_launchSeqPhase) {
					HangarScene_PlaceFixedShot(0, 3, x, y, z);
					return 1;
				}
				return HangarScene_PlacePlayerCraftShot(0, x, y, z);
			case 8:
				/* Above the crane, wherever it travels. */
				g_hangarCamFocusObj = g_hangarRoofCraneState.objectIdx;
				*x = g_objectTable[g_hangarCamFocusObj].world_x;
				*y = g_objectTable[g_hangarCamFocusObj].world_y + 654;
				*z = g_objectTable[g_hangarCamFocusObj].world_z - 8;
				return 1;
			default:
				return 0;
		}
	}
	switch (shot) {
		case 1:
		case 2:
		case 3:
		case 6:
		case 7:
		case 8:
		case 9:
			HangarScene_PlaceFixedShot(1, shot, x, y, z);
			return 1;
		case 5:
			if (g_hangarPlayerObjIdx == 0xffff) {
				HangarScene_PlaceFixedShot(1, 3, x, y, z);
				return 1;
			}
			return HangarScene_PlacePlayerCraftShot(1, x, y, z);
		default:
			return 0;
	}
}

void XwaHangarScene_FollowCrane(int* x, int* y, int* z) {
	const ObjectRecord* crane = &g_objectTable[g_hangarRoofCraneState.objectIdx];

	*x = crane->world_x;
	*y = crane->world_y + 654;
	*z = crane->world_z - 8;
}

void XwaHangarScene_SaveFlightState(XwaHangarSceneFlightState* state) {
	memset(state, 0, sizeof *state);
	state->launchRefOffset[0] = s_launchRefOffset[0];
	state->launchRefOffset[1] = s_launchRefOffset[1];
	state->reentryStartY = s_reentryStartY;
	state->shuttleLoaded = (uint8_t)(s_shuttleLoaded != 0);
}

void XwaHangarScene_RestoreFlightState(const XwaHangarSceneFlightState* state) {
	s_launchRefOffset[0] = state->launchRefOffset[0];
	s_launchRefOffset[1] = state->launchRefOffset[1];
	s_reentryStartY = state->reentryStartY;
	s_shuttleLoaded = state->shuttleLoaded;
}
