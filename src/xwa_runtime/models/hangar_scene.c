#include "xwa_runtime/models/hangar_scene.h"

#include "xwa/assets/model_def.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/object_type.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/hangar.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/math/scalar.h"
#include "xwa/render/renderer.h"
#include "xwa/util/random.h"
#include "xwa_runtime/models/hangars.h"
#include "xwa_runtime/models/object_profiles.h"

/* Modern port module; no original counterpart. It builds the hangar scene as
   XWAU's hook_hangars does (hangar.cpp: HangarMapHook, FamHangarMapHook,
   HangarReloadHook), with the same spawns as Hangar_SetupReadyScene. */

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
		objIdx = (uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(entry->objectType, entry->position[0],
																   entry->position[1], entry->position[2],
																   entry->headingXY, entry->headingZ);
		if (objIdx != 0xffffu && objIdx != 0) {
			g_objectTable[objIdx].mobj->nodeSwitchIndex = (uint8_t)entry->markings;
			XwaObjectProfiles_ApplyNamed(entry->objectType, entry->objectProfile, g_objectTable[objIdx].mobj->pCraft);
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

/* The scene objects: the layout, then the hangar's droids, shuttle and roof
   crane, or the family base's work droid, as Hangar_SetupReadyScene spawns
   them (same random draws, in the same order). */
void XwaHangarScene_Spawn(int family) {
	int sceneSlot;

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

	sceneSlot = g_hangarSceneObjectCount;
	g_hangarSceneObjects[sceneSlot].objectIdx =
		(uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(0x137, 227, 351, 0x7fffffff, 58736, 0);
	g_hangarSceneObjects[sceneSlot].moveState = 0;
	g_hangarSceneObjects[sceneSlot].nextDecisionTime = 0;
	g_hangarSceneObjects[sceneSlot].moveSpeed = 0.0f;
	g_hangarSceneObjects[sceneSlot].targetObjIdx = 0;
	++g_hangarSceneObjectCount;

	sceneSlot = g_hangarSceneObjectCount;
	g_hangarSceneObjects[sceneSlot].objectIdx =
		(uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(0x138, -70, 400, 0x7fffffff, 0, 0);
	g_hangarSceneObjects[sceneSlot].moveState = 0;
	g_hangarSceneObjects[sceneSlot].nextDecisionTime = 0;
	g_hangarSceneObjects[sceneSlot].moveSpeed = 0.0f;
	g_hangarSceneObjects[sceneSlot].targetObjIdx = 0;
	++g_hangarSceneObjectCount;

	g_hangarShuttleState.objectIdx =
		(uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(0x32, 1127, 959, -741, 43136, 0);
	FlightObject_InitMeshAnimationDefaults(g_hangarShuttleState.objectIdx);
	g_hangarShuttleState.moveState = 0;
	g_hangarShuttleState.nextDecisionTime = ((uint16_t)GameRand() >> 1) + 10000;
	g_hangarShuttleState.moveSpeed = 0.0f;
	g_hangarShuttleState.targetObjIdx = 0;

	g_hangarRoofCraneState.objectIdx =
		(uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(0x13c, -1400, 786, -282, 0, 0);
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
}

/* HangarReloadHook: the hangar of another command ship. The hangar model
   reloads in place, its floor at the new model's height; the objects the
   last scene spawned are released and the scene spawns again. */
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
	g_launchBaseZ =
		launchRef->world_z +
		g_modelDefs[(uint16_t)GetModelIndexFromType((ObjectTypeId)g_hangarBackdropModelType)].meshAttachData[6];
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
