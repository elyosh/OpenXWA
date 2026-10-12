#include "xwa_runtime/models/hangar_scene.h"

#include "xwa/assets/model_def.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/object_type.h"
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

/* Modern port module; no original counterpart. It builds the hangar scene as
   XWAU's hook_hangars does (hangar.cpp: HangarMapHook, FamHangarMapHook,
   HangarReloadHook), with the same spawns as Hangar_SetupReadyScene, and
   places the objects and the player as its floor hooks do. */

/* The floor side of the map line spawning (-1: the hangar floor's). */
static int s_spawnFloorInverted = -1;
/* The player X/Y offset the launch reference holds. */
static int s_launchRefOffset[2];
/* The re-entry start Y. */
static int s_reentryStartY;
/* Whether the scene has a shuttle (LoadShuttle, or the player needed one). */
static int s_shuttleLoaded;

/* HangarMapHook / FamHangarMapHook: the layout. Each line places an object
   with its markings and object profile; the first four are the objects the
   droid and crane traffic refer to. A line the hook skips keeps its place. */
static void HangarScene_SpawnMap(int family) {
	const XwaHangarMapEntry* map;
	int count;
	int line;

	map = XwaHangars_Map(family, &count);
	for (line = 0; map != NULL && line < count; ++line) {
		const XwaHangarMapEntry* entry = &map[line];
		uint16_t objIdx;

		if (entry->objectType == 0) {
			continue;
		}
		s_spawnFloorInverted = entry->isFloorInverted;
		objIdx = (uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(entry->objectType, entry->position[0],
																 entry->position[1], entry->position[2],
																 entry->headingXY, entry->headingZ);
		s_spawnFloorInverted = -1;
		if (objIdx != 0xffffu && objIdx != 0) {
			g_objectTable[objIdx].mobj->nodeSwitchIndex = (uint8_t)entry->markings;
			XwaSkins_UseMarkings(entry->objectType, g_objectTable[objIdx].mobj->nodeSwitchIndex);
			XwaObjectProfiles_ApplyNamed(entry->objectType, entry->objectProfile,
										 g_objectTable[objIdx].mobj->pCraft);
		}
		switch (line) {
			case 0:
				if (family) {
					g_unusedHangarWorkStandObjIdx = objIdx;
					g_unusedHangarWorkStandObjIdxMirror = g_unusedHangarWorkStandObjIdx;
				} else {
					g_hangarDroidTargetObjIdx = objIdx;
				}
				break;
			case 1:
				if (family) {
					g_hangarDroidTargetObjIdx = objIdx;
				} else {
					g_hangarCraneObjIdx = objIdx;
				}
				break;
			case 2:
				if (family) {
					g_hangarCraneObjIdx = objIdx;
				} else {
					g_hangarDroidRouteTargetObjIdx = objIdx;
				}
				break;
			case 3:
				if (family) {
					g_hangarDroidRouteTargetObjIdx = objIdx;
				} else {
					g_unusedHangarWorkStandObjIdx = objIdx;
					g_unusedHangarWorkStandObjIdxMirror = g_unusedHangarWorkStandObjIdx;
				}
				break;
		}
	}
}

/* Spawns `objectType` on the floor (`inverted`: below the inverted floor),
   `heightZ` higher. */
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
static void HangarScene_Dress(uint16_t objIdx, uint16_t objectType, int markings, const char* profile) {
	if (objIdx != 0xffffu && objIdx != 0) {
		g_objectTable[objIdx].mobj->nodeSwitchIndex = (uint8_t)markings;
		XwaSkins_UseMarkings(objectType, g_objectTable[objIdx].mobj->nodeSwitchIndex);
		XwaObjectProfiles_ApplyNamed(objectType, profile, g_objectTable[objIdx].mobj->pCraft);
	}
}

/* The shuttle, at its place on the floor. The hook applies its object
   profile only with a ShuttleModelIndex; it applies here in all cases. */
static void HangarScene_SpawnShuttle(void) {
	const XwaHangarObjects* objects = XwaHangars_Objects();
	const uint16_t objectType = objects->shuttleModelType != 0 ? objects->shuttleModelType : OBJ_Shuttle;

	g_hangarShuttleState.objectIdx = HangarScene_SpawnOnFloor(
		objectType, objects->shuttlePosition[0], objects->shuttlePosition[1], objects->shuttlePosition[2],
		objects->shuttleOrientation, objects->isShuttleFloorInverted);
	FlightObject_InitMeshAnimationDefaults(g_hangarShuttleState.objectIdx);
	HangarScene_Dress((uint16_t)g_hangarShuttleState.objectIdx, objectType, objects->shuttleMarkings,
					  objects->shuttleObjectProfile);
	s_shuttleLoaded = 1;
}

/* The scene objects: the layout, then the hangar's droids, shuttle and roof
   crane, or the family base's work droid, as Hangar_SetupReadyScene spawns
   them (same random draws, in the same order). */
void XwaHangarScene_Spawn(int family) {
	const XwaHangarObjects* objects = XwaHangars_Objects();
	int sceneSlot;
	int droid;

	HangarScene_SpawnMap(family);
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

	/* HangarLoadDroidsHook: without LoadDroids no droid slot; a droid not
	   loaded keeps its slot, with object 0 and no update. Droid 2 keeps its
	   stock place: the hook spawns it at droid 1's. */
	if (objects->droids[0].load || objects->droids[1].load) {
		static const int places[2][3] = { { 227, 351, 58736 }, { -70, 400, 0 } };

		for (droid = 0; droid < 2; ++droid) {
			const XwaHangarDroid* settings = &objects->droids[droid];
			uint16_t objIdx = 0;

			if (settings->load) {
				objIdx = HangarScene_SpawnOnFloor(settings->modelType, places[droid][0], places[droid][1],
												  settings->positionZ, (uint16_t)places[droid][2],
												  settings->isFloorInverted);
				HangarScene_Dress(objIdx, settings->modelType, settings->markings, settings->objectProfile);
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

	/* HangarLoadShuttleHook: without LoadShuttle no shuttle (object 0), but
	   the same random draws. */
	g_hangarShuttleState.objectIdx = 0;
	s_shuttleLoaded = 0;
	if (objects->loadShuttle) {
		HangarScene_SpawnShuttle();
	}
	g_hangarShuttleState.moveState = 0;
	g_hangarShuttleState.nextDecisionTime = ((uint16_t)GameRand() >> 1) + 10000;
	g_hangarShuttleState.moveSpeed = 0.0f;
	g_hangarShuttleState.targetObjIdx = 0;

	/* HangarLoadHangarRoofCraneHook. */
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

/* The hangar shows the HangarIff markings, by default the command ship's. */
void XwaHangarScene_SetHangarMarkings(void) {
	const uint8_t hangarIff = XwaHangars_Objects()->hangarIff;

	g_objectTable[g_launchRefObjIdx].mobj->nodeSwitchIndex =
		hangarIff != 0xFF ? hangarIff : XwaHangars_CommandShipMarkings();
	XwaSkins_UseMarkings(g_objectTable[g_launchRefObjIdx].objectType,
						 g_objectTable[g_launchRefObjIdx].mobj->nodeSwitchIndex);
}

/* HangarReloadHook: the hangar of another command ship. The hangar model
   reloads in place, its floor at the new model's height, and moves for the
   new player offset; the objects the last scene spawned are released and the
   scene spawns again. */
void XwaHangarScene_Rebuild(void) {
	ObjectRecord* launchRef = &g_objectTable[g_launchRefObjIdx];
	const uint8_t hangarFlightGroup = launchRef->flightGroupIdx;
	uint32_t objIdx;

	g_hangarBackdropModelType = g_pilotData.hangarType ? 179 : OBJ_Hangar;
	launchRef->objectType = (ObjectTypeId)g_hangarBackdropModelType;
	XwaHangars_ReleaseStaleModel((uint16_t)g_hangarBackdropModelType);
	if (!g_loadedModels.byObjectType[(uint16_t)g_hangarBackdropModelType]) {
		Hangar_LoadCraftModelByType((ObjectTypeId)g_hangarBackdropModelType);
		Math_SetFpuSinglePrecisionMode();
	}
	g_launchBaseZ = launchRef->world_z +
					g_modelDefs[(uint16_t)GetModelIndexFromType((ObjectTypeId)g_hangarBackdropModelType)]
						.meshAttachData[6];
	/* The hook keeps the setup offset; the entering craft's applies here. */
	launchRef->world_x += s_launchRefOffset[0];
	launchRef->world_y += s_launchRefOffset[1];
	XwaHangarScene_OffsetLaunchRef(g_launchRefObjIdx, g_players[g_localPlayer].objectIndex);
	launchRef->mobj->prevWorldX = launchRef->world_x;
	launchRef->mobj->prevWorldY = launchRef->world_y;
	XwaHangarScene_SetHangarMarkings();
	if (g_hangarBackdropModelType == OBJ_Hangar) {
		int16_t hangarModelIdx = GetModelIndexFromType(OBJ_Hangar);
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
	XwaHangarScene_Spawn(g_hangarBackdropModelType != OBJ_Hangar);
}

/* ---- placement (hook_hangars GetCraftElevation, GetPlayerCraft and its floor
   and animation hooks) ------------------------------------------------- */

static int HangarScene_PlayerInverted(int objIdx) {
	return XwaHangars_PlayerFloorInverted(g_objectTable[objIdx].objectType);
}

/* The height above the floor of the player standing `elevation` off its
   floor side, with its offset. */
static int HangarScene_PlayerHeight(int objIdx, int elevation) {
	if (HangarScene_PlayerInverted(objIdx)) {
		return XwaHangars_Objects()->hangarFloorInvertedHeight + XwaHangarScene_PlayerOffset(objIdx, 2) -
			   elevation;
	}
	return XwaHangarScene_PlayerOffset(objIdx, 2) + elevation;
}

/* HangarFloorElevationHook sets the floor as here, then moves it to the
   player's height plus 0x223 plus its elevation over the hangar's
   InsideHangar point, which leaves the player off the floor at mission start
   (both the stock and XWAU hangar models have their floor at that point).
   The player rests on the floor instead, as in the stock game. */
int XwaHangarScene_FloorZ(int playerObjIdx) {
	return g_objectTable[playerObjIdx].world_z -
		   HangarScene_PlayerHeight(playerObjIdx, XwaHangarScene_PlayerRestElevation(playerObjIdx));
}

/* HangarPositionXYHook. */
void XwaHangarScene_OffsetLaunchRef(int launchRefIdx, int playerObjIdx) {
	s_launchRefOffset[0] = XwaHangarScene_PlayerOffset(playerObjIdx, 0);
	s_launchRefOffset[1] = XwaHangarScene_PlayerOffset(playerObjIdx, 1);
	g_objectTable[launchRefIdx].world_x -= s_launchRefOffset[0];
	g_objectTable[launchRefIdx].world_y -= s_launchRefOffset[1];
}

/* HangarObjectsElevationHook. */
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
	return XwaHangars_CraftElevation(g_objectTable[objIdx].objectType, HangarScene_PlayerInverted(objIdx));
}

int XwaHangarScene_PlayerAnimationElevation(int objIdx, int elevation) {
	const XwaHangarObjects* objects = XwaHangars_Objects();

	return elevation + (HangarScene_PlayerInverted(objIdx) ? objects->playerAnimationInvertedElevation
														   : objects->playerAnimationElevation);
}

int XwaHangarScene_PlayerAbove(int objIdx, int elevation) {
	const int z =
		g_objectTable[objIdx].world_z - (g_launchBaseZ + HangarScene_PlayerHeight(objIdx, elevation));

	return HangarScene_PlayerInverted(objIdx) ? z < 0 : z > 0;
}

void XwaHangarScene_PlayerRise(int objIdx, int distance) {
	g_objectTable[objIdx].world_z += HangarScene_PlayerInverted(objIdx) ? -distance : distance;
}

/* HangarPlayerCraftElevationHook. */
void XwaHangarScene_PlacePlayerAtRest(int objIdx) {
	g_objectTable[objIdx].world_z =
		g_launchBaseZ + HangarScene_PlayerHeight(objIdx, XwaHangarScene_PlayerRestElevation(objIdx));
}

/* HangarReenterInitPositionZHook, after the stock X/Y. */
void XwaHangarScene_PlaceReentryStart(int objIdx) {
	ObjectRecord* object = &g_objectTable[objIdx];

	object->world_x += XwaHangarScene_PlayerOffset(objIdx, 0);
	object->world_y +=
		XwaHangarScene_PlayerOffset(objIdx, 1) + XwaHangars_Objects()->playerAnimationStraightLine;
	s_reentryStartY = object->world_y;
	object->world_z = g_launchBaseZ + HangarScene_PlayerHeight(
										  objIdx, XwaHangarScene_PlayerAnimationElevation(objIdx, 0x223));
}

/* HangarReenterAnimation51Hook: no descent before the stock start. */
int XwaHangarScene_ReentryStraight(int objIdx) {
	return g_objectTable[objIdx].world_y > s_reentryStartY - XwaHangarScene_PlayerOffset(objIdx, 1) -
											   XwaHangars_Objects()->playerAnimationStraightLine;
}

/* HangarReenterAnimation73Hook, after the stock placement. */
void XwaHangarScene_PlaceOnLaunchRail(int objIdx) {
	ObjectRecord* object = &g_objectTable[objIdx];

	object->world_x += XwaHangarScene_PlayerOffset(objIdx, 0);
	object->world_y = g_objectTable[g_launchRefObjIdx].world_y +
					  (g_hangarBackdropModelType == OBJ_Hangar ? -800 : -6800) +
					  XwaHangarScene_PlayerOffset(objIdx, 1);
	XwaHangarScene_PlacePlayerAtRest(objIdx);
}

/* ---- shuttle, droids and roof crane (hook_hangars HangarShuttle*,
   HangarDroidsUpdateHook, HangarRoofCrane*Hook) ------------------------ */

int XwaHangarScene_ShuttleLoaded(void) { return s_shuttleLoaded; }

void XwaHangarScene_EnsureShuttle(void) {
	if (!s_shuttleLoaded && g_hangarBackdropModelType == OBJ_Hangar) {
		HangarScene_SpawnShuttle();
	}
}

/* The height above the floor of the shuttle `elevation` off its floor side,
   plus ShuttlePositionZ. The hook leaves HangarFloorInvertedHeight out of
   the inverted heights of the shuttle's flight but not of its spawn; it is
   in both here. */
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

/* HangarShuttleLaunchReenterAnimation2Update/CheckHook. */
int XwaHangarScene_ShuttleTurnOut(int dtTicks) {
	ObjectRecord* shuttle = &g_objectTable[g_hangarShuttleState.objectIdx];
	int heading = shuttle->yaw;

	heading += heading >= 0x8000 ? 50 * dtTicks : -50 * dtTicks;
	shuttle->yaw = (uint16_t)heading;
	return heading <= 0 || heading >= 0x10000;
}

/* HangarShuttleLaunchReenterAnimation8Update/CheckHook. */
int XwaHangarScene_ShuttleTurnIn(int dtTicks) {
	ObjectRecord* shuttle = &g_objectTable[g_hangarShuttleState.objectIdx];
	const int orientation =
		XwaHangars_Objects()->shuttleOrientation != 0 ? XwaHangars_Objects()->shuttleOrientation : 0x10000;
	int heading = shuttle->yaw;

	heading += orientation >= 0x8000 ? 50 * dtTicks : -50 * dtTicks;
	shuttle->yaw = (uint16_t)heading;
	return orientation >= 0x8000 ? heading >= orientation : heading <= orientation;
}

/* HangarShuttleLaunchReenterAnimations4And6Hook: the leg off the landing
   place goes sideways (Right, banking) or up or down (Top, Bottom: no bank). */
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

/* HangarShuttleReenterPositionHook, after the stock placement. */
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

/* The roof crane's axis (HangarRoofCraneAxis; others are X). */
static int HangarScene_CraneAxis(void) {
	const int axis = XwaHangars_Objects()->roofCraneAxis;

	return axis == 1 || axis == 2 ? axis : 0;
}

static int* HangarScene_Coordinate(ObjectRecord* object, int axis) {
	return axis == 1 ? &object->world_y : axis == 2 ? &object->world_z : &object->world_x;
}

/* HangarRoofCraneUpdateHook. */
int* XwaHangarScene_CraneCoordinate(void) {
	return HangarScene_Coordinate(&g_objectTable[g_hangarRoofCraneState.objectIdx], HangarScene_CraneAxis());
}

/* HangarRoofCraneInitHook: `offset` along the axis, turned back when it
   leaves [low, high] off the crane's place and kept within it. */
int XwaHangarScene_CraneTarget(int offset) {
	const XwaHangarObjects* objects = XwaHangars_Objects();
	const int axis = HangarScene_CraneAxis();
	const int current = *XwaHangarScene_CraneCoordinate();
	const int place =
		*HangarScene_Coordinate(&g_objectTable[g_launchRefObjIdx], axis) + objects->roofCranePosition[axis];
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

/* ---- cameras (hook_hangars HangarCameraPositionHook,
   HangarRoofCraneCameraHook) ------------------------------------------- */

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

/* A fixed shot: the camera section's key (a missing key is 0), else stock. */
static void HangarScene_FixedShot(int family, int shot, int* x, int* y, int* z) {
	const XwaHangarCamera* camera = XwaHangars_Camera(family);
	const int* offset = camera->present ? camera->offsets[shot] : s_stockShots[family][shot];
	const ObjectRecord* launchRef = &g_objectTable[g_launchRefObjIdx];

	*x = launchRef->world_x + offset[0];
	*y = launchRef->world_y + offset[1];
	*z = launchRef->world_z + offset[2];
}

/* The player craft's own camera; 0 without one (the stock per-type shot). */
static int HangarScene_PlayerShot(int family, int* x, int* y, int* z) {
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
	const int family = g_hangarBackdropModelType != OBJ_Hangar;
	const int shot = (uint16_t)shotIndex;

	if (!family) {
		switch (shot) {
			case 1:
			case 2:
			case 3:
			case 6:
			case 9:
				HangarScene_FixedShot(0, shot, x, y, z);
				return 1;
			case 4:
			case 5:
				/* A droid not loaded gives key 3. */
				if (!XwaHangars_Objects()->droids[shot - 4].load) {
					HangarScene_FixedShot(0, 3, x, y, z);
					return 1;
				}
				g_hangarCamFocusObj = g_hangarSceneObjects[shot - 4].objectIdx;
				*x = g_objectTable[g_hangarCamFocusObj].world_x;
				*y = g_objectTable[g_hangarCamFocusObj].world_y;
				*z = g_objectTable[g_hangarCamFocusObj].world_z + 30;
				return 1;
			case 7:
				if (g_hangarPlayerObjIdx == 0xffff || g_launchSeqPhase) {
					HangarScene_FixedShot(0, 3, x, y, z);
					return 1;
				}
				return HangarScene_PlayerShot(0, x, y, z);
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
			HangarScene_FixedShot(1, shot, x, y, z);
			return 1;
		case 5:
			if (g_hangarPlayerObjIdx == 0xffff) {
				HangarScene_FixedShot(1, 3, x, y, z);
				return 1;
			}
			return HangarScene_PlayerShot(1, x, y, z);
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
