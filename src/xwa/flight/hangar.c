#include "xwa/flight/hangar.h"
#include "xwa/flight/fediskio.h"

#include "aeron/log.h"
#include "xwa/flight/ai/pai.h"
#include "xwa/flight/ai/pai_plan.h"
#include "xwa/flight/ai/paifight.h"
#include "xwa/flight/ai/paiman.h"
#include "xwa/flight/ai/paiorder.h"
#include "xwa/assets/flight_model.h"
#include "xwa/assets/model_bounds.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_texture.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/opt_model.h"
#include "xwa/assets/string_table.h"
#include "xwa/audio/music.h"
#include "xwa/audio/sound.h"
#include "xwa/config/game_config.h"
#include "xwa/console/console.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/object/damage.h"
#include "xwa/flight/death_star.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/flight_light.h"
#include "xwa/flight/flight_map.h"
#include "xwa/audio/fsfx.h"
#include "xwa/flight/starfield.h"
#include "xwa/frontend/film_room.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_input.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/input/dinput.h"
#include "xwa/input/forcefeedback.h"
#include "xwa/math/fixed.h"
#include "xwa/math/scalar.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/yard.h"
#include "xwa/flight/film.h"
#include "xwa/flight/object/laser.h"
#include "xwa/flight/player/player.h"
#ifdef XWA_MODERN
#include "xwa_runtime/timing/modern_flight_timing.h"
#include "xwa_runtime/snapshot/snapshot_hud.h"
#endif
#include "xwa/render/effects.h"
#include "xwa/render/renderer.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"
#include "xwa/util/random.h"
#include "xwa/util/time.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XWA_MODERN
#define HANGAR_HUD_PANE_PUSH(id_, x_, y_, w_, h_) XwaSnapshotHud_PushPane((id_), (x_), (y_), (w_), (h_))
#define HANGAR_HUD_PANE_POP() XwaSnapshotHud_PopPane()
#else
#define HANGAR_HUD_PANE_PUSH(id_, x_, y_, w_, h_) ((void)0)
#define HANGAR_HUD_PANE_POP() ((void)0)
#endif

#ifdef XWA_MODERN
static int g_hangarReadyLoopRequested;
#endif

#ifndef XWA_MODERN
extern void(__stdcall* g_OutputDebugStringA)(const char* outputString);
#define HANGAR_OUTPUT_DEBUG_STRING g_OutputDebugStringA
#else
static void Hangar_OutputDebugString(const char* outputString) { DebugPrintf("%s", outputString); }
#define HANGAR_OUTPUT_DEBUG_STRING Hangar_OutputDebugString
#endif

typedef struct HangarDroidRouteNode {
	int targetOffsetX;
	int targetOffsetY;
	int targetOffsetZ;
	Q16Angle yaw;
	int anchorMode;
	int nextNode[6];
} HangarDroidRouteNode;

static __inline int Hangar_Abs32(int value) {
#ifdef XWA_MODERN
	return Xwa_Abs32(value);
#else
	return abs(value);
#endif
}

// GLOBAL: XWA 0x5A957C
float g_hangarAngleToRadians = 0.000095873722f;
// GLOBAL: XWA 0x5A95AC
float g_hangarSceneMinMoveSpeed = 2.0f;
// GLOBAL: XWA 0x5A95D0
float g_hangarSceneDecelPerTick = 0.1f;
// GLOBAL: XWA 0x5A95D4
float g_hangarSceneEscapeMoveSpeed = 10.0f;
// GLOBAL: XWA 0x5A95D8
float g_hangarSceneEscapeAccelPerTickNeg = -0.15000001f;
// GLOBAL: XWA 0x5A95DC
float g_hangarSceneCruiseMoveSpeed = 4.0f;
// GLOBAL: XWA 0x5A95E0
float g_hangarSceneAccelPerTickNeg = -0.050000001f;
// GLOBAL: XWA 0x5A95E8
double g_hangarDroidSeparationScale = -0.02;

// GLOBAL: XWA 0x5B4B68
const HangarDroidRouteNode g_hangarDroidRouteNodes[10] = {
	{ -160, 351, -802, 0x0000u, 1, { 1, 2, 3, 4, 8, -1 } },
	{ -740, 310, -827, 0xd000u, 2, { 0, 0, 0, 0, 0, 0 } },
	{ 1980, 420, -827, 0x5000u, 2, { 0, 0, 0, 0, 0, 0 } },
	{ 376, 2289, -827, 0x0000u, 2, { 0, -1, 4, 8, -1, -1 } },
	{ 536, -891, -827, 0x0000u, 1, { 0, -1, 3, 5, 6, -1 } },
	{ 680, -830, -827, 0x3000u, 2, { 4, 4, 4, 4, 4, 4 } },
	{ 670, -1729, -866, 0x0000u, 2, { 4, 4, 7, 7, -1, -1 } },
	{ -450, -1669, -785, 0x9000u, 2, { 6, 6, 6, 8, 8, 8 } },
	{ -650, -1229, -785, 0x0000u, 1, { 0, 3, 7, 9, 0, -1 } },
	{ -1470, -1719, -785, 0xb000u, 2, { 8, 8, 8, -1, -1, -1 } },
};
// GLOBAL: XWA 0x5B4D10
static const HangarDroidRouteNode g_familyBaseDroidRouteNodes[10] = {
	{ 1388, -8391, -5096, 0x0000u, 2, { 1, 1, 8, 8, 1, -1 } },
	{ 1190, -8560, -4930, 0x7000u, 1, { 0, 0, 0, 8, 6, 5 } },
	{ -1630, -6730, -4790, 0xc000u, 1, { 3, 3, 3, 1, 6, 5 } },
	{ -582, -3921, -5037, 0xe000u, 2, { 6, 5, 4, 8, 2, -1 } },
	{ 142, -6014, -4869, 0x8000u, 2, { 5, 5, 5, 6, -1, -1 } },
	{ 328, -6551, -4648, 0x0000u, 1, { 4, 4, 2, 6, 1, 8 } },
	{ 1608, -6652, -4825, 0xc000u, 1, { 1, 7, 7, 7, 5, 4 } },
	{ 1828, -6671, -5016, 0xc000u, 2, { 6, 6, 6, 6, -1, -1 } },
	{ -1330, -8746, -4590, 0x4000u, 1, { 9, 9, 5, 0, 6, 1 } },
	{ -1712, -8590, -5095, 0x4000u, 2, { 8, 8, 8, 8, 8, 8 } },
};
// GLOBAL: XWA 0x686B90
float g_hangarLaunchYawRate;
// GLOBAL: XWA 0x686B94
int g_launchSeqPhase;
// GLOBAL: XWA 0x686B98
int g_hangarSavedPlayerWorldX;
// GLOBAL: XWA 0x686B9C
int g_hangarSavedPlayerWorldY;
// GLOBAL: XWA 0x686BA0
int g_hangarSavedPlayerWorldZ;
// GLOBAL: XWA 0x686BA8
int g_hangarMenuItemDisabled[HANGAR_MENU_LEVEL_COUNT][HANGAR_MENU_SELECTABLE_ROW_COUNT];
// GLOBAL: XWA 0x686D38
HangarSceneObjectState g_hangarRoofCraneState;
// GLOBAL: XWA 0x686D68
int g_hangarMenuItemCount[HANGAR_MENU_LEVEL_COUNT];
// GLOBAL: XWA 0x686D70
int g_hangarSavedPlayerPitch;
// GLOBAL: XWA 0x686D74
int g_hangarLastRandomCameraTime4x;
// GLOBAL: XWA 0x686D78
char g_hangarMenuItemLabels[HANGAR_MENU_LEVEL_COUNT][HANGAR_MENU_ROW_COUNT][HANGAR_MENU_TEXT_LENGTH];
// GLOBAL: XWA 0x68BB98
int srcObjIdx;
// GLOBAL: XWA 0x68BB9C
int g_hangarServiceCooldown;
// GLOBAL: XWA 0x68BBA0
int g_hangarTipStep;
// GLOBAL: XWA 0x68BBA4
int g_hangarSavedPlayerAngleD;
// GLOBAL: XWA 0x68BBA8
int g_hangarTipDelayMs;
// GLOBAL: XWA 0x68BBAC
int g_hangarNextAmbientSoundTime;
// GLOBAL: XWA 0x68BBB0
int g_unusedHangarPrimaryDroidObjIdxMirror;
// GLOBAL: XWA 0x68BBB4
int g_unusedHangarPrimaryDroidObjIdx;
// GLOBAL: XWA 0x68BBB8
int g_launchAnimDone;
// GLOBAL: XWA 0x68BBBC
int g_hangarDroidRouteTargetObjIdx;
// GLOBAL: XWA 0x68BBC0
int g_hangarMenuLevel;
// GLOBAL: XWA 0x68BBC8
HangarSceneObjectState g_hangarShuttleState;
// GLOBAL: XWA 0x68BBF8
int g_hangarSavedThrottleSpeed;
// GLOBAL: XWA 0x68BBFC
int g_hangarSavedPlayerRoll;
// GLOBAL: XWA 0x68BC00
int g_hangarSavedBeamLevel;
// GLOBAL: XWA 0x68BC04
int g_hangarSavedLaserRedirect;
// GLOBAL: XWA 0x68BC08
int g_hangarPlayerObjIdx;
// GLOBAL: XWA 0x68BC10
int g_hangarSceneObjectCount;
// GLOBAL: XWA 0x68BC14
int g_hangarEntryTime4x;
// GLOBAL: XWA 0x68BC18
int g_unusedHangarEnterCraftReadyLatch;
// GLOBAL: XWA 0x68BC1C
float g_hangarLaunchRollRate;
// GLOBAL: XWA 0x68BC20
int g_hangarSavedShieldRedirect;
// GLOBAL: XWA 0x68BC28
int g_hangarMenuCursor[HANGAR_MENU_LEVEL_COUNT];
// GLOBAL: XWA 0x68BC30
int g_hangarSavedPlayerYaw;
// GLOBAL: XWA 0x68BC34
int g_hangarDroidTargetObjIdx;
// GLOBAL: XWA 0x68BC38
int g_launchBaseZ;
// GLOBAL: XWA 0x68BC40
char g_hangarMenuColTitle[2][HANGAR_MENU_TEXT_LENGTH];
// GLOBAL: XWA 0x68BCA4
float g_hangarLaunchMoveSpeed;
// GLOBAL: XWA 0x68BCA8
int g_hangarAnimatedDroidObjIdx;
// GLOBAL: XWA 0x68BCAC
int g_hangarReadyElapsedMs;
// GLOBAL: XWA 0x68BCB0
float g_hangarLaunchPitchRate;
// GLOBAL: XWA 0x68BCB8
int g_hangarMenuScroll[HANGAR_MENU_LEVEL_COUNT];
// GLOBAL: XWA 0x68BCC0
int g_hangarCamFocusObj;
// GLOBAL: XWA 0x68BCC4
int g_launchRefObjIdx;
// GLOBAL: XWA 0x68BCC8
int16_t dstX;
// GLOBAL: XWA 0x68BCCC
int16_t dstY;
// GLOBAL: XWA 0x68BCD0
YardHighScoreTable* g_yardHighScoreTable;
// GLOBAL: XWA 0x68BCD4
YardCraftScoreTable* g_yardCurrentCraftScoreTable;
// GLOBAL: XWA 0x68BCD8
uint8_t g_yardSelectedCraftType;
// GLOBAL: XWA 0x68BCDC
int g_hangarLastSoundUpdateTimestamp;
// GLOBAL: XWA 0x80DB60
int g_hangarSceneRegionIdx;
// GLOBAL: XWA 0x9C674C
int g_unusedHangarShuttleMeshFlagLatch;
// GLOBAL: XWA 0x9C6750
int g_hangarReturnToFlightAvailable;
// GLOBAL: XWA 0x9C6754
int16_t g_hangarBackdropModelType;
// GLOBAL: XWA 0x9C6760
uint16_t g_hangarWarheadList[9];
// GLOBAL: XWA 0x9C6780
HangarSceneObjectState g_hangarSceneObjects[10];
// GLOBAL: XWA 0x9C694C
int g_hangarEvacuateWarningPlayed;
// GLOBAL: XWA 0x9C6950
int g_hangarDroid0RouteActive;
// GLOBAL: XWA 0x9C6958
int g_hangarDroid1RouteActive;
// GLOBAL: XWA 0x9C6960
uint16_t g_hangarCraftList[HANGAR_CRAFT_LIST_STORAGE];
// GLOBAL: XWA 0x9C6E20
uint16_t g_hangarBeamList[5];
// GLOBAL: XWA 0x9C6E30
int g_hangarSavedMissionRegionIdx;
// GLOBAL: XWA 0x9C6E34
int g_hangarMissionResolved;
// GLOBAL: XWA 0x9C6E38
int g_hangarSourceObjIdx;
// GLOBAL: XWA 0x9C6E3C
int g_hangarAutoCam;
// GLOBAL: XWA 0x9C6E40
int g_inHangarReady;
// GLOBAL: XWA 0x9C6F60
int g_hangarLaunchMirrorAttachOffsets;
// GLOBAL: XWA 0x9C6F64
int g_hangarReservedFilmState0;
// GLOBAL: XWA 0x9C6F68
int16_t g_hangarCountermeasureTypeList[HANGAR_COUNTERMEASURE_LIST_COUNT];
// GLOBAL: XWA 0x9C6F74
int g_hangarInitialReadyEntryPending;
// GLOBAL: XWA 0x9C6FB4
int g_hangarHullDamageWarningPlayed;
#ifdef XWA_MODERN
static GameConfig g_hangarOptionsModalOldConfig;
static int g_hangarOptionsModalPending;
#endif
#ifdef XWA_MODERN
typedef struct ModernHangarPackedRotationState {
	int decreasing;
	int scaledStateActive;
} ModernHangarPackedRotationState;

static int g_modernHangarLegacyCadenceTicks;
static int g_modernHangarLegacyCadenceDue;
static int g_modernHangarCraneRotationActive;
static int g_modernHangarCraneScaledStateActive;
static ModernHangarPackedRotationState g_modernHangarPrimaryDroidRotation;
static ModernHangarPackedRotationState g_modernFamilyDroidRotations[10][2];

static void Hangar_ModernBeginLegacyCadence(int dtTicks);
#endif

// FUNCTION: XWA 0x462BD0
void Hangar_ClearSceneObjectCount(void) { g_hangarSceneObjectCount = 0; }

// FUNCTION: XWA 0x462A90
int Hangar_FilmWriteSceneObjectState(void) {
	int objectIdx;

	for (objectIdx = 0; objectIdx < g_hangarSceneObjectCount; ++objectIdx) {
		Film_WriteBytesBuffered(&g_hangarSceneObjects[objectIdx], sizeof(g_hangarSceneObjects[objectIdx]));
	}

	Film_WriteBytesBuffered(&g_hangarShuttleState, sizeof(g_hangarShuttleState));
	Film_WriteBytesBuffered(&g_hangarRoofCraneState, sizeof(g_hangarRoofCraneState));
	Film_WriteBytesBuffered(&g_hangarReservedFilmState0, sizeof(g_hangarReservedFilmState0));
	Film_WriteBytesBuffered(&g_hangarHullDamageWarningPlayed, sizeof(g_hangarHullDamageWarningPlayed));
	Film_WriteBytesBuffered(&g_hangarEvacuateWarningPlayed, sizeof(g_hangarEvacuateWarningPlayed));
	Film_WriteBytesBuffered(&g_hangarLaunchMirrorAttachOffsets, sizeof(g_hangarLaunchMirrorAttachOffsets));
	Film_WriteBytesBuffered(&g_hangarReturnToFlightAvailable, sizeof(g_hangarReturnToFlightAvailable));
	return 1;
}

// FUNCTION: XWA 0x462B30
int Hangar_FilmReadSceneObjectState(void) {
	int objectIdx;

	for (objectIdx = 0; objectIdx < g_hangarSceneObjectCount; ++objectIdx) {
		Film_ReadBytes(&g_hangarSceneObjects[objectIdx], sizeof(g_hangarSceneObjects[objectIdx]));
	}

	Film_ReadBytes(&g_hangarShuttleState, sizeof(g_hangarShuttleState));
	Film_ReadBytes(&g_hangarRoofCraneState, sizeof(g_hangarRoofCraneState));
	Film_ReadBytes(&g_hangarReservedFilmState0, sizeof(g_hangarReservedFilmState0));
	Film_ReadBytes(&g_hangarHullDamageWarningPlayed, sizeof(g_hangarHullDamageWarningPlayed));
	Film_ReadBytes(&g_hangarEvacuateWarningPlayed, sizeof(g_hangarEvacuateWarningPlayed));
	Film_ReadBytes(&g_hangarLaunchMirrorAttachOffsets, sizeof(g_hangarLaunchMirrorAttachOffsets));
	Film_ReadBytes(&g_hangarReturnToFlightAvailable, sizeof(g_hangarReturnToFlightAvailable));
	return 1;
}

static const char* Hangar_GetReadyWarheadName(uint16_t warheadType) {
	const char* label;

	label = NULL;
	if (warheadType >= OBJ_WarheadTorpedo && warheadType < OBJ_WarheadTorpedo + 16) {
		label = g_strWarheadNames[warheadType - OBJ_WarheadTorpedo];
	}
	return label != NULL ? label : g_strHangarMiscStrings[HANGAR_MISC_WARHEAD_NONE];
}

static uint16_t Hangar_CountReadyWarheads(const CraftData* craft, ModelIndex modelIndex,
										  unsigned int launcherIdx) {
	uint8_t firstSlot;
	uint8_t lastSlot;
	uint16_t total;

	if (modelIndex == 0xffffu) {
		return 0;
	}

	firstSlot = g_modelDefs[modelIndex].warheadLauncherFirstSlot[launcherIdx];
	lastSlot = g_modelDefs[modelIndex].warheadLauncherLastSlot[launcherIdx];
	total = 0;
	if (firstSlot <= lastSlot) {
		uint8_t slotIdx;

		for (slotIdx = firstSlot; slotIdx <= lastSlot; ++slotIdx) {
			total = (uint16_t)(total + craft->warheadData[slotIdx].count);
		}
	}
	return total;
}

static void Hangar_DrawReadyWarheadLine(const CraftData* craft, ModelIndex modelIndex,
										unsigned int launcherIdx, int16_t y, char* buffer) {
	uint16_t warheadType;
	uint16_t count;

	warheadType = craft->warheadSlotTypeIds[launcherIdx];
	if (warheadType == 0) {
		return;
	}

	count = Hangar_CountReadyWarheads(craft, modelIndex, launcherIdx);
	strcpy(buffer, Hangar_GetReadyWarheadName(warheadType));
	strcat(buffer, ": ");
	FlightText_SetCursor(15, y);
	FlightText_SetColor(0x36u);
	FlightText_DrawString(buffer);
	FlightText_SetColor(0x3au);
	FlightText_DrawDecimalNumber(count, 3u, 1u);
}

static void Hangar_FormatReadySourceStatus(char* buffer) {
	ObjectRecord* sourceObj;
	CraftData* craft;
	const char* fgName;

	sourceObj = &g_objectTable[g_hangarSourceObjIdx];
	craft = sourceObj->mobj->pCraft;
	fgName = g_missionFlightGroups[sourceObj->flightGroupIdx].fg.name;
	(void)GetModelIndexFromType(sourceObj->objectType);

	buffer[0] = '\0';
	if (craft->objectKind == 5) {
		sprintf(buffer, "%s %s", fgName, g_strHangarMiscStrings[HANGAR_MISC_ENTERING_HYPERSPACE]);
	} else if (craft->objectKind == 6) {
		sprintf(buffer, "%s %s", fgName, g_strHangarMiscStrings[HANGAR_MISC_LEAVING_HYPERSPACE]);
	} else if (craft->objectKind == 7) {
		sprintf(buffer, "%s %s", fgName, g_strHangarMiscStrings[HANGAR_MISC_IN_HYPERSPACE]);
	} else {
		AiController* ai;

		ai = pai_GetEffectiveAIController(craft);
		if (strcmp(g_planTable[ai->pendingPlanId].name, "hyperspacepln") == 0) {
			if (ai->maneuverPhase) {
				sprintf(buffer, "%s %s %d %s", fgName,
						g_strHangarMiscStrings[HANGAR_MISC_HYPERSPACE_IN_SECONDS1], ai->aiPlanState,
						g_strHangarMiscStrings[HANGAR_MISC_SECONDS]);
			} else {
				sprintf(buffer, "%s %d", fgName, ai->aiPlanState);
			}
		}
	}

	if ((uint32_t)craft->hullDamage < MATH2_longfraction(craft->hullMax, 0x8000u)) {
		if (craft->shieldFront == 0 && craft->shieldRear == 0) {
			sprintf(buffer, "%s %s %s", fgName, g_strHangarMiscStrings[HANGAR_MISC_WARNING],
					g_strHangarMiscStrings[HANGAR_MISC_HAS_HULL_DAMAGE]);
			if (!g_hangarHullDamageWarningPlayed) {
				fsfx_PlaySound(2639, 0xffffu, g_localPlayer);
				g_hangarHullDamageWarningPlayed = 1;
			}
		}
	} else {
		strcpy(buffer, g_strHangarMiscStrings[HANGAR_MISC_EVACUATE]);
		if (!g_hangarEvacuateWarningPlayed) {
			fsfx_PlaySound(2640, 0xffffu, g_localPlayer);
			g_hangarEvacuateWarningPlayed = 1;
		}
	}
}

static void Hangar_DrawReadyCraftPanel(void) {
	char buffer[52];
	char systemName[52];
	int16_t lineStep;
	ModelIndex modelIndex;
	CraftData* craft;
	HANGAR_HUD_PANE_PUSH(XWA_HUD_PANE_CMD, dstX, dstY, g_hudCmdPanelWidth, g_hudCmdPanelHeight);

	if (g_useHardware3D) {
		FlightText_SetRenderOffset(dstX, dstY);
	} else {
		FlightSw_SetRenderTarget(g_hudCmdTexPixels, g_hudCmdPanelWidth, (unsigned int)g_hudCmdPanelHeight,
								 g_hudCmdPanelWidth * g_flight16bppBytesPerPixel);
	}

	FlightText_SetClipRect(0, 0, (uint16_t)g_hudCmdPanelWidth, (uint16_t)g_hudCmdPanelHeight);
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetShadowEnabled(0);
	lineStep = (int16_t)((g_useHardware3D ? g_flightFontScale : g_flightFontLineHeight) + 1);
	g_curCraft = g_objectTable[g_hangarPlayerObjIdx].mobj->pCraft;
	craft = g_curCraft;

	if (!g_useHardware3D) {
		g_flightFillClipRectFn();
	}

	buffer[0] = '\0';
	modelIndex = GetModelIndexFromType(g_objectTable[g_players[g_localPlayer].objectIndex].objectType);
	if (modelIndex != 0xffffu) {
		strcpy(buffer, g_strHangarMiscStrings[HANGAR_MISC_YOUR_CRAFT]);
		strcat(buffer, g_modelDefs[modelIndex].nameAlt);
	}
	FlightText_SetCursor(0, 16);
	FlightText_SetColor(0x3au);
	FlightText_DrawStringCentered(buffer);

	Hangar_DrawReadyWarheadLine(craft, modelIndex, 0, (int16_t)(g_hudCmdPanelHeight - lineStep), buffer);
	Hangar_DrawReadyWarheadLine(craft, modelIndex, 1, (int16_t)(g_hudCmdPanelHeight - 2 * lineStep), buffer);

	Hangar_FormatBeamSystemName((int16_t)craft->beamTypeId, systemName);
	if (craft->beamTypeId != 0) {
		FlightText_SetColor(0x36u);
		strcpy(buffer, g_strHangarMiscStrings[HANGAR_MISC_BEAM_SYSTEM]);
		FlightText_SetCursor((int16_t)(g_hudCmdPanelWidth / 2 - 16),
							 (int16_t)(g_hudCmdPanelHeight - 2 * lineStep));
		FlightText_DrawString(buffer);
		FlightText_SetColor(0x3au);
		strcpy(buffer, systemName);
		FlightText_DrawString(buffer);
	}

	Hangar_FormatCountermeasureName((int16_t)craft->cmTypeId, systemName);
	if (craft->cmTypeId != 0) {
		FlightText_SetColor(0x36u);
		strcpy(buffer, g_strHangarMiscStrings[HANGAR_MISC_DEFENSE_SYSTEM]);
		FlightText_SetCursor((int16_t)(g_hudCmdPanelWidth / 2 - 16),
							 (int16_t)(g_hudCmdPanelHeight - lineStep));
		FlightText_DrawString(buffer);
		FlightText_SetColor(0x3au);
		strcpy(buffer, systemName);
		FlightText_DrawString(buffer);
	}

	if (g_hangarSourceObjIdx != 0xffff) {
		Hangar_FormatReadySourceStatus(buffer);
		FlightText_SetCursor(0, 0);
		FlightText_SetColor(0x4au);
		FlightText_DrawStringCentered(buffer);
	}

	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		Blit16ToFlightSurface(g_hudCmdTexPixels, g_flightColorEscapeBypassChar, 0, 0, (uint16_t)dstX,
							  (uint16_t)dstY, (uint16_t)g_hudCmdPanelWidth, (uint16_t)g_hudCmdPanelHeight,
							  (uint16_t)(g_flight16bppBytesPerPixel * g_hudCmdPanelWidth));
	}
	HANGAR_HUD_PANE_POP();
}

static void Hangar_FinishReadyScreenFrame(void) {
	if (g_useHardware3D) {
		RenderScene_Initialize(1);
		FlightText_FlushQueue();
		RenderScene_DrawVisibleFaces();
		RenderScene_End3D();
	}

	if (g_flightConfPowerVr && g_useHardware3D) {
		Hud_DrawHudTargetInsetIfEnabled(g_localPlayer);
		RenderScene_DrawVisibleFaces();
		RenderScene_End3D();
	}

	FlightDisplay_Flip();
	if (g_useHardware3D) {
		RenderScene_ClearFrameBuffers();
		Math_SetFpuSinglePrecisionMode();
		return;
	}

	FlightDisplay_BlitRenderSurface();
	Math_SetFpuSinglePrecisionMode();
}

static void Hangar_UpdateReadyAutoCamera(PlayerData* player) {
	if (g_hangarCamFocusObj != 0xffff) {
		ObjectRecord* playerObj;
		ObjectRecord* focusObj;

		playerObj = &g_objectTable[g_hangarPlayerObjIdx];
		focusObj = &g_objectTable[g_hangarCamFocusObj];
		trig2_ctop(playerObj->world_x - focusObj->world_x, playerObj->world_y - focusObj->world_y,
				   playerObj->world_z - focusObj->world_z);
		if (g_hangarCamFocusObj == g_hangarRoofCraneState.objectIdx) {
			player->viewState.savedTargetX = focusObj->world_x;
		} else {
			double angleRadians;

			angleRadians = (double)trig2_xyangle * 0.000095873722;
			player->viewState.savedTargetX = focusObj->world_x - (int)(sin(angleRadians) * 150.0);
			player->viewState.savedTargetY = focusObj->world_y - (int)(cos(angleRadians) * 150.0);
			if (focusObj->objectType == OBJ_WorkDroid1) {
				player->viewState.savedTargetZ = focusObj->world_z - 10;
			}
		}
	}

	trig2_ctop(g_objectTable[g_hangarPlayerObjIdx].world_x - player->viewState.savedTargetX,
			   g_objectTable[g_hangarPlayerObjIdx].world_y - player->viewState.savedTargetY,
			   g_objectTable[g_hangarPlayerObjIdx].world_z - player->viewState.savedTargetZ);
	player->viewState.viewRoll = 0;
	player->viewState.viewPitch = targetPitch;
	player->viewState.viewYaw = trig2_xyangle;
	FVIEW_BuildCameraOrient(player->viewState.viewRoll, (int16_t)player->viewState.viewPitch,
							(int16_t)player->viewState.viewYaw, 0, (int16_t)player->viewState.hudAimX,
							(int16_t)player->viewState.hudAimY, NULL, -1);
}

static int Hangar_EnterCraftInternal(uint16_t fromObjIdx, int runReadyLoop) {
	int result = 0;
	int missionWasEnding = 0; // v56
	int i;

	g_localPlayerLightPulses[0].enabled = 0;
	g_localPlayerLightPulses[1].enabled = 0;
	g_localPlayerLightPulses[2].enabled = 0;
	g_localPlayerLightPulses[3].enabled = 0;
	g_localPlayerLightPulses[4].enabled = 0;
	g_localPlayerLightPulses[5].enabled = 0;
	if (g_useHardware3D) {
		Particle_FreeAllEffects();
	}
	g_hangarTipStep = 0;
	if (g_players[g_localPlayer].currentSeatIdx) {
		Player_CycleGunnerSeat(g_localPlayer, (void*)1);
	}
	g_players[g_localPlayer].selectedTargetComponent = 0;
	g_players[g_localPlayer].pendingActionTimer = 0;
	g_players[g_localPlayer].pendingActionId = 0;
	g_players[g_localPlayer].currentTargetObjectIdx = 0xFFFF;
	if (fromObjIdx != 0xFFFF) {
		g_hangarSourceObjIdx = fromObjIdx;
	}
	RenderScene_SetDepthProjectionScale(512.0);
	Math_SetFpuSinglePrecisionMode();
	g_currentMusicState = MUSIC_STATE_HANGAR_READY;
	g_selectedMusicState = MUSIC_STATE_HANGAR_READY;
	if (g_gameConfig.musicEnabled) {
		Music_SetState(MUSIC_STATE_HANGAR_READY);
	}

	if (g_players[g_localPlayer].mapCameraState) {
		if (g_players[g_localPlayer].connectedFlag != 2) {
			int mapCameraState;

			Mission_ProcessFlightGroupWaveCompletion(g_players[g_localPlayer].boundFlightGroupIdx);
			mapCameraState = g_players[g_localPlayer].mapCameraState;
			g_players[g_localPlayer].mapCameraState = 0;
			if (!Player_BindToAvailableCraft(g_localPlayer, 0xFFFFu,
											 g_players[g_localPlayer].boundObjectSignature, 0)) {
				Hud_RestorePlayerHudState(g_localPlayer);
				g_players[g_localPlayer].mapCameraState = 0;
			} else {
				g_players[g_localPlayer].mapCameraState = mapCameraState;
			}
		}
		g_players[g_localPlayer].mapCameraState = 0;
	} else {
		if (g_filmPlaybackMode && g_filmOverlayActive == 1) {
			Hud_SetHudViewState(g_players[g_localPlayer].viewState.hudStateLive, g_localPlayer);
			g_filmOverlayActive = 0;
			Hud_SyncLocalSoftwareHudMasks(1);
		}
		if (g_players[g_localPlayer].viewState.externalCameraActive) {
			Player_StepExtView(g_localPlayer);
		}
	}

	{
		int objectIndex = g_players[g_localPlayer].objectIndex;
		g_players[g_localPlayer].engineWashSourceObjIdx = -1;
		srcObjIdx = objectIndex;
		if (g_flightMissionEndPending) {
			g_flightMissionEndPending = 0;
			missionWasEnding = 1;
		}

		if (objectIndex == 0xFFFF || g_objectTable[objectIndex].objectType == OBJ_None ||
			!g_objectTable[objectIndex].mobj->pCraft) {
			// No valid player craft: stage the hangar shuttle object (launch phase 9).
			int shuttleIdx;
			ObjectRecord* pl;
			CraftData* plc;
			int meshCount;
			int v39;

			if (g_hangarSourceObjIdx != 0xFFFF) {
				CraftData* srcCraft = g_objectTable[g_hangarSourceObjIdx].mobj->pCraft;
				if (srcCraft && (unsigned)srcCraft->hullDamage >= (unsigned)srcCraft->hullMax &&
					missionWasEnding) {
					g_flightMissionEndPending = 1;
					return g_flightExitRequest;
				}
			}
			Hud_ResetFlightMessagePanes(1);
			shuttleIdx = g_hangarShuttleState.objectIdx;
			g_launchSeqPhase = 9;
			g_players[g_localPlayer].regionSessionId = 0;
			g_players[g_localPlayer].objectIndex = shuttleIdx;
			g_players[g_localPlayer].viewState.cameraFocusObjIdx = 0xFFFF;
			g_players[g_localPlayer].viewState.cameraDistance = 1024;
			g_inHangarReady = 1;
			g_launchAnimDone = 0;
			g_flightExitRequest = 0;
			g_launchTriggered = 0;
			g_hangarMissionResolved = 0;
			g_unusedHangarEnterCraftReadyLatch = 1;
			g_hangarAutoCam = 0;
			g_hangarPlayerObjIdx = shuttleIdx;
			g_players[g_localPlayer].currentSeatIdx = 0;
			g_loadingModel = 1;
			FeDiskIo_LoadCockpitModel();
			FeDiskIo_LoadExteriorModel();
			g_loadingModel = 0;
			Math_SetFpuSinglePrecisionMode();
			g_players[g_localPlayer].regionIndex = (uint8_t)g_hangarSceneRegionIdx;
			Mission_SetActiveRegionObjectRanges((uint8_t)g_hangarSceneRegionIdx);
			g_players[g_localPlayer].cockpitVisible = 1;
			g_players[g_localPlayer].lookYawOffset = 0;
			g_players[g_localPlayer].lookPitchOffset = 0;
			Player_SetTarget(g_hangarPlayerObjIdx, g_localPlayer);
			if (fromObjIdx != 0xFFFF || g_hangarMissionResolved || missionWasEnding) {
				g_hangarLastRandomCameraTime4x = 0;
				g_hangarEntryTime4x = 0;
			} else {
				g_hangarLastRandomCameraTime4x = g_gameTime;
				g_hangarEntryTime4x = g_gameTime;
			}
			FlightLight_ClearDirectionalLights();
			FlightLight_AddDirectionalLight(0, 0, 0xFFFF, 0.75, 1.0, 1.0, 1.0);
			g_objectTable[g_hangarPlayerObjIdx].world_x = g_objectTable[g_launchRefObjIdx].world_x + 1127;
			g_objectTable[g_hangarPlayerObjIdx].world_y = g_objectTable[g_launchRefObjIdx].world_y + 9000;
			g_objectTable[g_hangarPlayerObjIdx].world_z = g_launchBaseZ + 353;
			g_objectTable[g_hangarPlayerObjIdx].yaw = 0x8000;
			g_objectTable[g_hangarPlayerObjIdx].pitch = 0x4000;
			g_objectTable[g_hangarPlayerObjIdx].roll = 0;
			g_hangarShuttleState.moveSpeed = 40.0f;
			g_hangarShuttleState.moveState = 10;
			pl = &g_objectTable[g_hangarPlayerObjIdx];
			plc = pl->mobj->pCraft;
			meshCount = ModelMesh_GetObjectTypeMeshCount(pl->objectType);
			for (i = 0; i < meshCount; ++i) {
				if (ModelMesh_GetObjectTypeMeshType((uint16_t)g_objectTable[g_hangarPlayerObjIdx].objectType,
													i) == MESH_RotaryWing) {
					plc->meshRotation[i] = 0;
				}
			}
			FVIEW_calcrotatemove(g_objectTable[g_hangarPlayerObjIdx].pitch,
								 g_objectTable[g_hangarPlayerObjIdx].yaw,
								 &g_objectTable[g_hangarPlayerObjIdx]);
			FVIEW_calcrotateorient(g_objectTable[g_hangarPlayerObjIdx].roll,
								   g_objectTable[g_hangarPlayerObjIdx].angleD,
								   &g_objectTable[g_hangarPlayerObjIdx]);
			Hangar_BuildMenu(1);
			fsfx_UpdateMissileThreatWarning();
			for (i = 4; i < 2871; ++i) {
				if (Sound_CountPlayingInstances(g_sfxIds[i])) {
					Sound_StopOldestInstance(g_sfxIds[i]);
				}
			}
			v39 = (uint16_t)GameRand() % 10;
			++v39;
			if (v39 == 9 || v39 == 0) {
				v39 = 10;
			}
			Hangar_SetCameraShot(v39);
		} else {
			// Save the current player craft and build the hangar player object from it.
			ObjectRecord* cur = &g_objectTable[objectIndex];
			CraftData* curc;
			CraftData* newc;

			if (!g_hangarMissionResolved) {
				g_hangarSavedMissionRegionIdx = g_players[g_localPlayer].regionIndex;
			}
			g_hangarSavedPlayerWorldX = cur->world_x;
			g_hangarSavedPlayerWorldY = cur->world_y;
			g_hangarSavedPlayerWorldZ = cur->world_z;
			g_hangarSavedPlayerPitch = cur->pitch;
			g_hangarSavedPlayerRoll = cur->roll;
			g_hangarSavedPlayerAngleD = cur->angleD;
			curc = cur->mobj->pCraft;
			g_hangarSavedThrottleSpeed = curc->throttleSpeed;
			g_hangarSavedBeamLevel = curc->beamLevel;
			g_hangarSavedShieldRedirect = curc->shieldRedirect;
			g_hangarSavedLaserRedirect = curc->laserRedirect;
			{
				int savedYaw;

				if (fromObjIdx != 0xFFFF) {
					savedYaw = cur->yaw + 0x8000;
				} else {
					savedYaw = cur->yaw;
				}
				g_hangarSavedPlayerYaw = savedYaw;
			}
			g_inHangarReady = 1;
			g_hangarAutoCam = 0;
			g_launchAnimDone = 0;
			g_flightExitRequest = 0;
			g_launchTriggered = 0;
			g_unusedHangarEnterCraftReadyLatch = 1;
			if (fromObjIdx != 0xFFFF) {
				ObjectTypeId mt = g_objectTable[fromObjIdx].objectType;
				if (mt == OBJ_CalamariCruiserNew) {
					g_hangarBackdropModelType = 308;
				} else {
					g_hangarBackdropModelType = 179;
					if (mt == OBJ_CalamariWinged) {
						g_hangarBackdropModelType = 308;
					}
				}
			}
			if (g_hangarInitialReadyEntryPending) {
				g_yardFinishPlacementMessagePending = 0;
				g_yardFinishPlacementResultCode = 0;
				g_hangarInitialReadyEntryPending = 0;
				g_hangarTipDelayMs = 0;
				g_hangarTipStep = 0;
				if (g_provingGroundsModeActive) {
					g_yardSelectedCraftType = 0;
					++g_pilotData.combatChamberMissions[66].numberTimesFlown;
				}
				g_hangarReturnToFlightAvailable = 1;
			} else {
				g_hangarTipStep = 6;
				g_hangarReturnToFlightAvailable = 0;
			}
			g_players[g_localPlayer].cockpitVisible = 1;
			g_players[g_localPlayer].hudEnabled = 1;
			g_players[g_localPlayer].lookYawOffset = 0;
			g_hangarEvacuateWarningPlayed = 0;
			g_hangarHullDamageWarningPlayed = 0;
			g_hangarReservedFilmState0 = 0;
			g_players[g_localPlayer].lookPitchOffset = 0;
			FlightView_UpdatePlayerCamera(g_localPlayer);
			if (fromObjIdx != 0xFFFF || g_hangarMissionResolved || missionWasEnding) {
				g_hangarLastRandomCameraTime4x = 0;
				g_hangarEntryTime4x = 0;
			} else {
				g_hangarLastRandomCameraTime4x = g_gameTime;
				g_hangarEntryTime4x = g_gameTime;
			}
			g_players[g_localPlayer].regionIndex = (uint8_t)g_hangarSceneRegionIdx;
			Mission_SetActiveRegionObjectRanges((uint8_t)g_hangarSceneRegionIdx);
			{
				int genus = g_objectTable[srcObjIdx].genusId;
				g_hangarPlayerObjIdx = Object_AllocSlotForGenus(genus);
			}
			Object_CopyStatePreservingStorage(g_hangarPlayerObjIdx, srcObjIdx);
			g_players[g_localPlayer].objectIndex = g_hangarPlayerObjIdx;
			g_players[g_localPlayer].viewState.cameraFocusObjIdx = g_hangarPlayerObjIdx;
			newc = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
			g_objectTable[g_hangarPlayerObjIdx].regionIdx = (uint8_t)g_players[g_localPlayer].regionIndex;
			g_objectTable[srcObjIdx].objectType = OBJ_None;
			Player_SetTarget((uint16_t)srcObjIdx, g_localPlayer);
			g_hangarReadyElapsedMs = 0;
			g_hangarMenuLevel = 0;
			Hud_ForceHudRefresh(g_localPlayer, 1);
			g_hangarMenuCursor[0] = 0;
			g_hangarMenuCursor[1] = 0;
			Hangar_BuildMenu(1);
			if (fromObjIdx != 0xFFFF || g_hangarMissionResolved || missionWasEnding) {
				if (g_hangarBackdropModelType == 308) {
					g_objectTable[g_hangarPlayerObjIdx].world_x = g_objectTable[g_launchRefObjIdx].world_x;
					g_objectTable[g_hangarPlayerObjIdx].world_y =
						g_objectTable[g_launchRefObjIdx].world_y + 3000;
				} else {
					g_objectTable[g_hangarPlayerObjIdx].world_x =
						g_objectTable[g_launchRefObjIdx].world_x + 142;
					g_objectTable[g_hangarPlayerObjIdx].world_y =
						g_objectTable[g_launchRefObjIdx].world_y - 400;
				}
				g_objectTable[g_hangarPlayerObjIdx].world_z = g_launchBaseZ + 547;
				g_objectTable[g_hangarPlayerObjIdx].yaw = 0x8000;
				g_objectTable[g_hangarPlayerObjIdx].pitch = 14336;
				g_objectTable[g_hangarPlayerObjIdx].roll = 0;
				g_objectTable[g_hangarPlayerObjIdx].angleD = 0;
				g_hangarLaunchMoveSpeed = 32.0f - (float)((uint16_t)GameRand() % 20) * -0.1f;
				newc->throttleSpeed = 0;
				newc->beamLevel = 0;
				newc->shieldRedirect = 0;
				newc->laserRedirect = 0;
				if (g_useHardware3D && g_hangarBackdropModelType != 179) {
					Renderer_FlushTextureCacheAndReturnTrue();
					Hangar_RenderFourStepCameraTransition();
					Math_SetFpuSinglePrecisionMode();
				}
				Time_GetFrameDelta();
				switch (g_objectTable[g_hangarPlayerObjIdx].objectType) {
					case OBJ_MilleniumFalcon2:
						g_hangarLaunchPitchRate = -18.0f;
						break;
					case OBJ_CorellianTransport2:
					case OBJ_FamilyTransport:
						g_hangarLaunchPitchRate = -10.0f;
						break;
					case OBJ_AWing:
						g_hangarLaunchPitchRate = -30.0f;
						break;
					case OBJ_XWing:
					case OBJ_YWing:
					case OBJ_BWing:
						g_hangarLaunchPitchRate = -20.0f;
						break;
					default:
						g_hangarLaunchPitchRate = -25.0f;
						break;
				}
				g_launchSeqPhase = 5;
				Hangar_SetCameraShot((uint16_t)GameRand() % 10);
			} else {
				g_objectTable[g_hangarPlayerObjIdx].yaw = 0;
				g_objectTable[g_hangarPlayerObjIdx].pitch = 0x4000;
				g_objectTable[g_hangarPlayerObjIdx].roll = 0;
				g_objectTable[g_hangarPlayerObjIdx].angleD = 0;
				Hangar_SetCameraShot(0);
				g_launchSeqPhase = 0;
				g_hangarLaunchMoveSpeed = 0.0f;
				newc->throttleSpeed = 0;
				newc->beamLevel = 0;
				newc->shieldRedirect = 0;
				newc->laserRedirect = 0;
				Time_GetFrameDelta();
				g_inputTimestamp = 0;
				g_lastLocalReplayInputTimestamp = 0;
				g_flightSfxSideEffectGate = 0;
				g_gameTime = 0;
			}
			if (g_useHardware3D && g_hangarBackdropModelType != 179) {
				Renderer_FlushTextureCacheAndReturnTrue();
				Hangar_RenderFourStepCameraTransition();
				Math_SetFpuSinglePrecisionMode();
			}
			Time_GetFrameDelta();
			FVIEW_calcrotatemove(g_objectTable[g_hangarPlayerObjIdx].pitch,
								 g_objectTable[g_hangarPlayerObjIdx].yaw,
								 &g_objectTable[g_hangarPlayerObjIdx]);
			FVIEW_calcrotateorient(g_objectTable[g_hangarPlayerObjIdx].roll,
								   g_objectTable[g_hangarPlayerObjIdx].angleD,
								   &g_objectTable[g_hangarPlayerObjIdx]);
			FlightObject_InitMeshAnimationDefaults(g_hangarPlayerObjIdx);
			FlightLight_ClearDirectionalLights();
			FlightLight_AddDirectionalLight(0, 0, 0xFFFF, 0.75, 1.0, 1.0, 1.0);
			if (!g_useHardware3D) {
				Hud_SetupCraftEntryHudMasks();
				Math_SetFpuSinglePrecisionMode();
			}
		}
	}

	DInput_DrainKeyboardEvents();
	DInput_PollMouseState();

	if (g_hangarSourceObjIdx == 0xFFFF || g_hangarShuttleState.objectIdx == g_hangarPlayerObjIdx ||
		missionWasEnding) {
		while (!fsfx_IsVoiceQueueEmpty()) {
			fsfx_RemoveVoiceQueueEntryChain(0);
		}
		if (!runReadyLoop) {
#ifdef XWA_MODERN
			g_hangarReadyLoopRequested = 1;
#endif
			return result;
		}
		while (1) {
			int startGameTime;
			int elapsed;
			int off, p;

			g_inputTimestamp += Time_GetFrameDelta();
			for (startGameTime = g_gameTime; g_inputTimestamp - g_gameTime < 8; startGameTime = g_gameTime) {
				g_inputTimestamp += Time_GetFrameDelta();
			}
			elapsed = g_inputTimestamp - startGameTime;
			g_elapsedTicks = (uint16_t)elapsed;

			for (off = 0; off < (int)sizeof(PlayerFlightTransientTimers); off += 2) {
				for (p = 0; p < XWA_PLAYER_COUNT; ++p) {
					if (g_players[p].connectedFlag) {
						uint16_t* t = (uint16_t*)((char*)&g_playerFlightTransientTimers[p] + off);
						if (*t) {
							int16_t nv = (int16_t)(*t - elapsed);
							*t = (uint16_t)nv;
							if (nv < 0) {
								*t = 0;
							}
						}
					}
				}
			}

			g_missionElapsedClock.subsecondTicks = (int16_t)(g_missionElapsedClock.subsecondTicks - elapsed);
			if (g_missionElapsedClock.subsecondTicks <= 0) {
				g_missionElapsedClock.subsecondTicks += 236;
				Hud_AdvanceFlightMessagePaneTimers();
			}

			{
				int16_t crewTimer;

				crewTimer = (int16_t)g_flightGlobalCountdownTimers[2];
				if (crewTimer) {
					crewTimer = (int16_t)(crewTimer - elapsed);
					g_flightGlobalCountdownTimers[2] = (uint16_t)crewTimer;
					if (crewTimer < 0) {
						crewTimer = 0;
						g_flightGlobalCountdownTimers[2] = 0;
					}
				}
				if (crewTimer == 0) {
					g_flightGlobalCountdownTimers[2] = 15;
					if (g_flightPlayerCount == 1 && g_players[g_localPlayer].viewState.externalCameraActive) {
						FlightObject_AnimateCrewMeshRotations((uint16_t)g_players[g_localPlayer].objectIndex,
															  0);
					}
				}
			}

			Hud_UpdateFlightMessagePanes();
			Flight_UpdateDynamicMusicState();
			Hangar_UpdateLaunch(elapsed);
			g_gameTime = g_inputTimestamp;
			Sound_FlushQueuedEffects();
			result = g_flightExitRequest;
			if (g_launchAnimDone || g_flightExitRequest) {
				break;
			}
			Hangar_RenderReadyScreen();
			g_gameTime = g_inputTimestamp;
		}
	} else {
		uint8_t primary;

		fsfx_UpdateMissileThreatWarning();
		if (g_provingGroundsModeActive) {
			return 0;
		}
		primary = g_missionFlightRuntimeState
					  .teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff][TEAM_GOAL_PRIMARY];
		if (primary == 1) {
			if (fsfx_IsVoiceQueueEmpty()) {
				switch ((uint16_t)GameRand2() % 4) {
					case 0:
						fsfx_PlaySound(2623, 0xFFFFu, g_localPlayer);
						break;
					case 2:
						fsfx_PlaySound(2625, 0xFFFFu, g_localPlayer);
						break;
					case 3:
						fsfx_PlaySound(2626, 0xFFFFu, g_localPlayer);
						break;
					default:
						break;
				}
			}
			return 0;
		}
		if (primary) {
			return 0;
		}
		{
			ObjectRecord* pl = &g_objectTable[g_hangarPlayerObjIdx];
			ObjectTypeId ot = pl->objectType;
			CraftData* plc;

			if (ot == OBJ_None) {
				return 0;
			}
			plc = pl->mobj->pCraft;
			if ((unsigned)(4 * plc->hullDamage) > (unsigned)plc->hullMax || plc->subsystemDamage) {
				if (fsfx_IsVoiceQueueEmpty()) {
					if ((uint16_t)GameRand2() % 2) {
						fsfx_PlaySound(2620, 0xFFFFu, g_localPlayer);
					} else {
						fsfx_PlaySound(2619, 0xFFFFu, g_localPlayer);
					}
				}
				return 0;
			}
			if (ot != OBJ_MilleniumFalcon2 && ot != OBJ_FamilyTransport && ot != OBJ_CorellianTransport2) {
				if (fsfx_IsVoiceQueueEmpty()) {
					switch ((uint16_t)GameRand2() % 3) {
						case 0:
							fsfx_PlaySound(2618, 0xFFFFu, g_localPlayer);
							break;
						case 1:
							fsfx_PlaySound(2621, 0xFFFFu, g_localPlayer);
							break;
						case 2:
							fsfx_PlaySound(2622, 0xFFFFu, g_localPlayer);
							break;
						default:
							break;
					}
				}
				return 0;
			}
			if (!fsfx_IsVoiceQueueEmpty()) {
				return 0;
			}
			switch ((uint16_t)GameRand2() % 4) {
				case 0:
					fsfx_PlaySound(2618, 0xFFFFu, g_localPlayer);
					return 0;
				case 1:
					fsfx_PlaySound(2621, 0xFFFFu, g_localPlayer);
					return 0;
				case 2:
					fsfx_PlaySound(2622, 0xFFFFu, g_localPlayer);
					return 0;
				case 3:
					fsfx_PlaySound(2644, 0xFFFFu, g_localPlayer);
					result = 0;
					break;
				default:
					return 0;
			}
		}
	}
	return result;
	return result;
}

int Hangar_BeginEnterCraft(uint16_t fromObjIdx) { return Hangar_EnterCraftInternal(fromObjIdx, 0); }

#ifdef XWA_MODERN
int Hangar_TakeReadyLoopRequest(void) {
	int requested;

	requested = g_hangarReadyLoopRequested;
	g_hangarReadyLoopRequested = 0;
	return requested;
}
#endif

// FUNCTION: XWA 0x457C20
// Enter the pre-launch hangar / ready scene for the local player. fromObjIdx is the
// optional source/mothership object (0xFFFF if none). Resets light pulses and player
// targeting, saves the current craft state, creates and positions the hangar player
// object, builds the menu, sets the launch-sequence phase, and then either runs the ready
// loop (Hangar_UpdateLaunch + Hangar_RenderReadyScreen) until launch/exit, or plays a
// single mission-status voice line. Returns g_flightExitRequest / a result code.
int Hangar_EnterCraft(uint16_t fromObjIdx) {
	int result = 0;
	int missionWasEnding = 0; // v56
	int i;

	g_localPlayerLightPulses[0].enabled = 0;
	g_localPlayerLightPulses[1].enabled = 0;
	g_localPlayerLightPulses[2].enabled = 0;
	g_localPlayerLightPulses[3].enabled = 0;
	g_localPlayerLightPulses[4].enabled = 0;
	g_localPlayerLightPulses[5].enabled = 0;
	if (g_useHardware3D) {
		Particle_FreeAllEffects();
	}
	g_hangarTipStep = 0;
	if (g_players[g_localPlayer].currentSeatIdx) {
		Player_CycleGunnerSeat(g_localPlayer, (void*)1);
	}
	g_players[g_localPlayer].selectedTargetComponent = 0;
	g_players[g_localPlayer].pendingActionTimer = 0;
	g_players[g_localPlayer].pendingActionId = 0;
	g_players[g_localPlayer].currentTargetObjectIdx = 0xFFFF;
	if (fromObjIdx != 0xFFFF) {
		g_hangarSourceObjIdx = fromObjIdx;
	}
	RenderScene_SetDepthProjectionScale(512.0);
	Math_SetFpuSinglePrecisionMode();
	g_currentMusicState = MUSIC_STATE_HANGAR_READY;
	g_selectedMusicState = MUSIC_STATE_HANGAR_READY;
	if (g_gameConfig.musicEnabled) {
		Music_SetState(MUSIC_STATE_HANGAR_READY);
	}

	if (g_players[g_localPlayer].mapCameraState) {
		if (g_players[g_localPlayer].connectedFlag != 2) {
			int mapCameraState;

			Mission_ProcessFlightGroupWaveCompletion(g_players[g_localPlayer].boundFlightGroupIdx);
			mapCameraState = g_players[g_localPlayer].mapCameraState;
			g_players[g_localPlayer].mapCameraState = 0;
			if (!Player_BindToAvailableCraft(g_localPlayer, 0xFFFFu,
											 g_players[g_localPlayer].boundObjectSignature, 0)) {
				Hud_RestorePlayerHudState(g_localPlayer);
				g_players[g_localPlayer].mapCameraState = 0;
			} else {
				g_players[g_localPlayer].mapCameraState = mapCameraState;
			}
		}
		g_players[g_localPlayer].mapCameraState = 0;
	} else {
		if (g_filmPlaybackMode && g_filmOverlayActive == 1) {
			Hud_SetHudViewState(g_players[g_localPlayer].viewState.hudStateLive, g_localPlayer);
			g_filmOverlayActive = 0;
			Hud_SyncLocalSoftwareHudMasks(1);
		}
		if (g_players[g_localPlayer].viewState.externalCameraActive) {
			Player_StepExtView(g_localPlayer);
		}
	}

	{
		int objectIndex = g_players[g_localPlayer].objectIndex;
		g_players[g_localPlayer].engineWashSourceObjIdx = -1;
		srcObjIdx = objectIndex;
		if (g_flightMissionEndPending) {
			g_flightMissionEndPending = 0;
			missionWasEnding = 1;
		}

		if (objectIndex == 0xFFFF || g_objectTable[objectIndex].objectType == OBJ_None ||
			!g_objectTable[objectIndex].mobj->pCraft) {
			// No valid player craft: stage the hangar shuttle object (launch phase 9).
			int shuttleIdx;
			ObjectRecord* pl;
			CraftData* plc;
			int meshCount;
			int v39;

			if (g_hangarSourceObjIdx != 0xFFFF) {
				CraftData* srcCraft = g_objectTable[g_hangarSourceObjIdx].mobj->pCraft;
				if (srcCraft && (unsigned)srcCraft->hullDamage >= (unsigned)srcCraft->hullMax &&
					missionWasEnding) {
					g_flightMissionEndPending = 1;
					return g_flightExitRequest;
				}
			}
			Hud_ResetFlightMessagePanes(1);
			shuttleIdx = g_hangarShuttleState.objectIdx;
			g_launchSeqPhase = 9;
			g_players[g_localPlayer].regionSessionId = 0;
			g_players[g_localPlayer].objectIndex = shuttleIdx;
			g_players[g_localPlayer].viewState.cameraFocusObjIdx = 0xFFFF;
			g_players[g_localPlayer].viewState.cameraDistance = 1024;
			g_inHangarReady = 1;
			g_launchAnimDone = 0;
			g_flightExitRequest = 0;
			g_launchTriggered = 0;
			g_hangarMissionResolved = 0;
			g_unusedHangarEnterCraftReadyLatch = 1;
			g_hangarAutoCam = 0;
			g_hangarPlayerObjIdx = shuttleIdx;
			g_players[g_localPlayer].currentSeatIdx = 0;
			g_loadingModel = 1;
			FeDiskIo_LoadCockpitModel();
			FeDiskIo_LoadExteriorModel();
			g_loadingModel = 0;
			Math_SetFpuSinglePrecisionMode();
			g_players[g_localPlayer].regionIndex = (uint8_t)g_hangarSceneRegionIdx;
			Mission_SetActiveRegionObjectRanges((uint8_t)g_hangarSceneRegionIdx);
			g_players[g_localPlayer].cockpitVisible = 1;
			g_players[g_localPlayer].lookYawOffset = 0;
			g_players[g_localPlayer].lookPitchOffset = 0;
			Player_SetTarget(g_hangarPlayerObjIdx, g_localPlayer);
			if (fromObjIdx != 0xFFFF || g_hangarMissionResolved || missionWasEnding) {
				g_hangarLastRandomCameraTime4x = 0;
				g_hangarEntryTime4x = 0;
			} else {
				g_hangarLastRandomCameraTime4x = g_gameTime;
				g_hangarEntryTime4x = g_gameTime;
			}
			FlightLight_ClearDirectionalLights();
			FlightLight_AddDirectionalLight(0, 0, 0xFFFF, 0.75, 1.0, 1.0, 1.0);
			g_objectTable[g_hangarPlayerObjIdx].world_x = g_objectTable[g_launchRefObjIdx].world_x + 1127;
			g_objectTable[g_hangarPlayerObjIdx].world_y = g_objectTable[g_launchRefObjIdx].world_y + 9000;
			g_objectTable[g_hangarPlayerObjIdx].world_z = g_launchBaseZ + 353;
			g_objectTable[g_hangarPlayerObjIdx].yaw = 0x8000;
			g_objectTable[g_hangarPlayerObjIdx].pitch = 0x4000;
			g_objectTable[g_hangarPlayerObjIdx].roll = 0;
			g_hangarShuttleState.moveSpeed = 40.0f;
			g_hangarShuttleState.moveState = 10;
			pl = &g_objectTable[g_hangarPlayerObjIdx];
			plc = pl->mobj->pCraft;
			meshCount = ModelMesh_GetObjectTypeMeshCount(pl->objectType);
			for (i = 0; i < meshCount; ++i) {
				if (ModelMesh_GetObjectTypeMeshType((uint16_t)g_objectTable[g_hangarPlayerObjIdx].objectType,
													i) == MESH_RotaryWing) {
					plc->meshRotation[i] = 0;
				}
			}
			FVIEW_calcrotatemove(g_objectTable[g_hangarPlayerObjIdx].pitch,
								 g_objectTable[g_hangarPlayerObjIdx].yaw,
								 &g_objectTable[g_hangarPlayerObjIdx]);
			FVIEW_calcrotateorient(g_objectTable[g_hangarPlayerObjIdx].roll,
								   g_objectTable[g_hangarPlayerObjIdx].angleD,
								   &g_objectTable[g_hangarPlayerObjIdx]);
			Hangar_BuildMenu(1);
			fsfx_UpdateMissileThreatWarning();
			for (i = 4; i < 2871; ++i) {
				if (Sound_CountPlayingInstances(g_sfxIds[i])) {
					Sound_StopOldestInstance(g_sfxIds[i]);
				}
			}
			v39 = (uint16_t)GameRand() % 10;
			++v39;
			if (v39 == 9 || v39 == 0) {
				v39 = 10;
			}
			Hangar_SetCameraShot(v39);
		} else {
			// Save the current player craft and build the hangar player object from it.
			ObjectRecord* cur = &g_objectTable[objectIndex];
			CraftData* curc;
			CraftData* newc;

			if (!g_hangarMissionResolved) {
				g_hangarSavedMissionRegionIdx = g_players[g_localPlayer].regionIndex;
			}
			g_hangarSavedPlayerWorldX = cur->world_x;
			g_hangarSavedPlayerWorldY = cur->world_y;
			g_hangarSavedPlayerWorldZ = cur->world_z;
			g_hangarSavedPlayerPitch = cur->pitch;
			g_hangarSavedPlayerRoll = cur->roll;
			g_hangarSavedPlayerAngleD = cur->angleD;
			curc = cur->mobj->pCraft;
			g_hangarSavedThrottleSpeed = curc->throttleSpeed;
			g_hangarSavedBeamLevel = curc->beamLevel;
			g_hangarSavedShieldRedirect = curc->shieldRedirect;
			g_hangarSavedLaserRedirect = curc->laserRedirect;
			{
				int savedYaw;

				if (fromObjIdx != 0xFFFF) {
					savedYaw = cur->yaw + 0x8000;
				} else {
					savedYaw = cur->yaw;
				}
				g_hangarSavedPlayerYaw = savedYaw;
			}
			g_inHangarReady = 1;
			g_hangarAutoCam = 0;
			g_launchAnimDone = 0;
			g_flightExitRequest = 0;
			g_launchTriggered = 0;
			g_unusedHangarEnterCraftReadyLatch = 1;
			if (fromObjIdx != 0xFFFF) {
				ObjectTypeId mt = g_objectTable[fromObjIdx].objectType;
				if (mt == OBJ_CalamariCruiserNew) {
					g_hangarBackdropModelType = 308;
				} else {
					g_hangarBackdropModelType = 179;
					if (mt == OBJ_CalamariWinged) {
						g_hangarBackdropModelType = 308;
					}
				}
			}
			if (g_hangarInitialReadyEntryPending) {
				g_yardFinishPlacementMessagePending = 0;
				g_yardFinishPlacementResultCode = 0;
				g_hangarInitialReadyEntryPending = 0;
				g_hangarTipDelayMs = 0;
				g_hangarTipStep = 0;
				if (g_provingGroundsModeActive) {
					g_yardSelectedCraftType = 0;
					++g_pilotData.combatChamberMissions[66].numberTimesFlown;
				}
				g_hangarReturnToFlightAvailable = 1;
			} else {
				g_hangarTipStep = 6;
				g_hangarReturnToFlightAvailable = 0;
			}
			g_players[g_localPlayer].cockpitVisible = 1;
			g_players[g_localPlayer].hudEnabled = 1;
			g_players[g_localPlayer].lookYawOffset = 0;
			g_hangarEvacuateWarningPlayed = 0;
			g_hangarHullDamageWarningPlayed = 0;
			g_hangarReservedFilmState0 = 0;
			g_players[g_localPlayer].lookPitchOffset = 0;
			FlightView_UpdatePlayerCamera(g_localPlayer);
			if (fromObjIdx != 0xFFFF || g_hangarMissionResolved || missionWasEnding) {
				g_hangarLastRandomCameraTime4x = 0;
				g_hangarEntryTime4x = 0;
			} else {
				g_hangarLastRandomCameraTime4x = g_gameTime;
				g_hangarEntryTime4x = g_gameTime;
			}
			g_players[g_localPlayer].regionIndex = (uint8_t)g_hangarSceneRegionIdx;
			Mission_SetActiveRegionObjectRanges((uint8_t)g_hangarSceneRegionIdx);
			{
				int genus = g_objectTable[srcObjIdx].genusId;
				g_hangarPlayerObjIdx = Object_AllocSlotForGenus(genus);
			}
			Object_CopyStatePreservingStorage(g_hangarPlayerObjIdx, srcObjIdx);
			g_players[g_localPlayer].objectIndex = g_hangarPlayerObjIdx;
			g_players[g_localPlayer].viewState.cameraFocusObjIdx = g_hangarPlayerObjIdx;
			newc = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
			g_objectTable[g_hangarPlayerObjIdx].regionIdx = (uint8_t)g_players[g_localPlayer].regionIndex;
			g_objectTable[srcObjIdx].objectType = OBJ_None;
			Player_SetTarget((uint16_t)srcObjIdx, g_localPlayer);
			g_hangarReadyElapsedMs = 0;
			g_hangarMenuLevel = 0;
			Hud_ForceHudRefresh(g_localPlayer, 1);
			g_hangarMenuCursor[0] = 0;
			g_hangarMenuCursor[1] = 0;
			Hangar_BuildMenu(1);
			if (fromObjIdx != 0xFFFF || g_hangarMissionResolved || missionWasEnding) {
				if (g_hangarBackdropModelType == 308) {
					g_objectTable[g_hangarPlayerObjIdx].world_x = g_objectTable[g_launchRefObjIdx].world_x;
					g_objectTable[g_hangarPlayerObjIdx].world_y =
						g_objectTable[g_launchRefObjIdx].world_y + 3000;
				} else {
					g_objectTable[g_hangarPlayerObjIdx].world_x =
						g_objectTable[g_launchRefObjIdx].world_x + 142;
					g_objectTable[g_hangarPlayerObjIdx].world_y =
						g_objectTable[g_launchRefObjIdx].world_y - 400;
				}
				g_objectTable[g_hangarPlayerObjIdx].world_z = g_launchBaseZ + 547;
				g_objectTable[g_hangarPlayerObjIdx].yaw = 0x8000;
				g_objectTable[g_hangarPlayerObjIdx].pitch = 14336;
				g_objectTable[g_hangarPlayerObjIdx].roll = 0;
				g_objectTable[g_hangarPlayerObjIdx].angleD = 0;
				g_hangarLaunchMoveSpeed = 32.0f - (float)((uint16_t)GameRand() % 20) * -0.1f;
				newc->throttleSpeed = 0;
				newc->beamLevel = 0;
				newc->shieldRedirect = 0;
				newc->laserRedirect = 0;
				if (g_useHardware3D && g_hangarBackdropModelType != 179) {
					Renderer_FlushTextureCacheAndReturnTrue();
					Hangar_RenderFourStepCameraTransition();
					Math_SetFpuSinglePrecisionMode();
				}
				Time_GetFrameDelta();
				switch (g_objectTable[g_hangarPlayerObjIdx].objectType) {
					case OBJ_MilleniumFalcon2:
						g_hangarLaunchPitchRate = -18.0f;
						break;
					case OBJ_CorellianTransport2:
					case OBJ_FamilyTransport:
						g_hangarLaunchPitchRate = -10.0f;
						break;
					case OBJ_AWing:
						g_hangarLaunchPitchRate = -30.0f;
						break;
					case OBJ_XWing:
					case OBJ_YWing:
					case OBJ_BWing:
						g_hangarLaunchPitchRate = -20.0f;
						break;
					default:
						g_hangarLaunchPitchRate = -25.0f;
						break;
				}
				g_launchSeqPhase = 5;
				Hangar_SetCameraShot((uint16_t)GameRand() % 10);
			} else {
				g_objectTable[g_hangarPlayerObjIdx].yaw = 0;
				g_objectTable[g_hangarPlayerObjIdx].pitch = 0x4000;
				g_objectTable[g_hangarPlayerObjIdx].roll = 0;
				g_objectTable[g_hangarPlayerObjIdx].angleD = 0;
				Hangar_SetCameraShot(0);
				g_launchSeqPhase = 0;
				g_hangarLaunchMoveSpeed = 0.0f;
				newc->throttleSpeed = 0;
				newc->beamLevel = 0;
				newc->shieldRedirect = 0;
				newc->laserRedirect = 0;
				Time_GetFrameDelta();
				g_inputTimestamp = 0;
				g_lastLocalReplayInputTimestamp = 0;
				g_flightSfxSideEffectGate = 0;
				g_gameTime = 0;
			}
			if (g_useHardware3D && g_hangarBackdropModelType != 179) {
				Renderer_FlushTextureCacheAndReturnTrue();
				Hangar_RenderFourStepCameraTransition();
				Math_SetFpuSinglePrecisionMode();
			}
			Time_GetFrameDelta();
			FVIEW_calcrotatemove(g_objectTable[g_hangarPlayerObjIdx].pitch,
								 g_objectTable[g_hangarPlayerObjIdx].yaw,
								 &g_objectTable[g_hangarPlayerObjIdx]);
			FVIEW_calcrotateorient(g_objectTable[g_hangarPlayerObjIdx].roll,
								   g_objectTable[g_hangarPlayerObjIdx].angleD,
								   &g_objectTable[g_hangarPlayerObjIdx]);
			FlightObject_InitMeshAnimationDefaults(g_hangarPlayerObjIdx);
			FlightLight_ClearDirectionalLights();
			FlightLight_AddDirectionalLight(0, 0, 0xFFFF, 0.75, 1.0, 1.0, 1.0);
			if (!g_useHardware3D) {
				Hud_SetupCraftEntryHudMasks();
				Math_SetFpuSinglePrecisionMode();
			}
		}
	}

	DInput_DrainKeyboardEvents();
	DInput_PollMouseState();

	if (g_hangarSourceObjIdx == 0xFFFF || g_hangarShuttleState.objectIdx == g_hangarPlayerObjIdx ||
		missionWasEnding) {
		while (!fsfx_IsVoiceQueueEmpty()) {
			fsfx_RemoveVoiceQueueEntryChain(0);
		}
		while (1) {
			int startGameTime;
			int elapsed;
			int off, p;

			g_inputTimestamp += Time_GetFrameDelta();
			for (startGameTime = g_gameTime; g_inputTimestamp - g_gameTime < 8; startGameTime = g_gameTime) {
				g_inputTimestamp += Time_GetFrameDelta();
			}
			elapsed = g_inputTimestamp - startGameTime;
			g_elapsedTicks = (uint16_t)elapsed;

			for (off = 0; off < (int)sizeof(PlayerFlightTransientTimers); off += 2) {
				for (p = 0; p < XWA_PLAYER_COUNT; ++p) {
					if (g_players[p].connectedFlag) {
						uint16_t* t = (uint16_t*)((char*)&g_playerFlightTransientTimers[p] + off);
						if (*t) {
							int16_t nv = (int16_t)(*t - elapsed);
							*t = (uint16_t)nv;
							if (nv < 0) {
								*t = 0;
							}
						}
					}
				}
			}

			g_missionElapsedClock.subsecondTicks = (int16_t)(g_missionElapsedClock.subsecondTicks - elapsed);
			if (g_missionElapsedClock.subsecondTicks <= 0) {
				g_missionElapsedClock.subsecondTicks += 236;
				Hud_AdvanceFlightMessagePaneTimers();
			}

			{
				int16_t crewTimer;

				crewTimer = (int16_t)g_flightGlobalCountdownTimers[2];
				if (crewTimer) {
					crewTimer = (int16_t)(crewTimer - elapsed);
					g_flightGlobalCountdownTimers[2] = (uint16_t)crewTimer;
					if (crewTimer < 0) {
						crewTimer = 0;
						g_flightGlobalCountdownTimers[2] = 0;
					}
				}
				if (crewTimer == 0) {
					g_flightGlobalCountdownTimers[2] = 15;
					if (g_flightPlayerCount == 1 && g_players[g_localPlayer].viewState.externalCameraActive) {
						FlightObject_AnimateCrewMeshRotations((uint16_t)g_players[g_localPlayer].objectIndex,
															  0);
					}
				}
			}

			Hud_UpdateFlightMessagePanes();
			Flight_UpdateDynamicMusicState();
			Hangar_UpdateLaunch(elapsed);
			g_gameTime = g_inputTimestamp;
			Sound_FlushQueuedEffects();
			result = g_flightExitRequest;
			if (g_launchAnimDone || g_flightExitRequest) {
				break;
			}
			Hangar_RenderReadyScreen();
			g_gameTime = g_inputTimestamp;
		}
	} else {
		uint8_t primary;

		fsfx_UpdateMissileThreatWarning();
		if (g_provingGroundsModeActive) {
			return 0;
		}
		primary = g_missionFlightRuntimeState
					  .teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff][TEAM_GOAL_PRIMARY];
		if (primary == 1) {
			if (fsfx_IsVoiceQueueEmpty()) {
				switch ((uint16_t)GameRand2() % 4) {
					case 0:
						fsfx_PlaySound(2623, 0xFFFFu, g_localPlayer);
						break;
					case 2:
						fsfx_PlaySound(2625, 0xFFFFu, g_localPlayer);
						break;
					case 3:
						fsfx_PlaySound(2626, 0xFFFFu, g_localPlayer);
						break;
					default:
						break;
				}
			}
			return 0;
		}
		if (primary) {
			return 0;
		}
		{
			ObjectRecord* pl = &g_objectTable[g_hangarPlayerObjIdx];
			ObjectTypeId ot = pl->objectType;
			CraftData* plc;

			if (ot == OBJ_None) {
				return 0;
			}
			plc = pl->mobj->pCraft;
			if ((unsigned)(4 * plc->hullDamage) > (unsigned)plc->hullMax || plc->subsystemDamage) {
				if (fsfx_IsVoiceQueueEmpty()) {
					if ((uint16_t)GameRand2() % 2) {
						fsfx_PlaySound(2620, 0xFFFFu, g_localPlayer);
					} else {
						fsfx_PlaySound(2619, 0xFFFFu, g_localPlayer);
					}
				}
				return 0;
			}
			if (ot != OBJ_MilleniumFalcon2 && ot != OBJ_FamilyTransport && ot != OBJ_CorellianTransport2) {
				if (fsfx_IsVoiceQueueEmpty()) {
					switch ((uint16_t)GameRand2() % 3) {
						case 0:
							fsfx_PlaySound(2618, 0xFFFFu, g_localPlayer);
							break;
						case 1:
							fsfx_PlaySound(2621, 0xFFFFu, g_localPlayer);
							break;
						case 2:
							fsfx_PlaySound(2622, 0xFFFFu, g_localPlayer);
							break;
						default:
							break;
					}
				}
				return 0;
			}
			if (!fsfx_IsVoiceQueueEmpty()) {
				return 0;
			}
			switch ((uint16_t)GameRand2() % 4) {
				case 0:
					fsfx_PlaySound(2618, 0xFFFFu, g_localPlayer);
					return 0;
				case 1:
					fsfx_PlaySound(2621, 0xFFFFu, g_localPlayer);
					return 0;
				case 2:
					fsfx_PlaySound(2622, 0xFFFFu, g_localPlayer);
					return 0;
				case 3:
					fsfx_PlaySound(2644, 0xFFFFu, g_localPlayer);
					result = 0;
					break;
				default:
					return 0;
			}
		}
	}
	return result;
}

// FUNCTION: XWA 0x45B0D0
// Per-tick hangar / launch-bay update. Shows hangar control tips and proving-ground
// placement messages, advances the launch-animation phases (lift-off + roll level,
// climb-out, carrier-bay descend/yaw/lower onto the rail), drives random camera shots,
// backdrop traffic, input, ambient/engine SFX, mothership-status cockpit lighting, and
// triggers the flight handoff / hangar exit. Returns g_flightExitRequest (or 0).
//
// The original's Sound_Update3DListenerAndSources() call is a DirectSound3D listener/source
// update superseded by the Aeron audio path, so it is omitted at the port boundary.
int Hangar_UpdateLaunch(int dtMs) {
	int clampedDtMs = dtMs;
	int dtMsa;
	int playerObjIdx;
	int refObjIdx;

	if (clampedDtMs > 200) {
		clampedDtMs = 200;
	}

	g_hangarReadyElapsedMs += clampedDtMs;
	g_flightSideEffectsEnabled = 1;
	g_simStepScale = 236 / clampedDtMs;
	if ((uint16_t)(236 / clampedDtMs) == 0) {
		g_simStepScale = 1;
	}

	if (g_provingGroundsModeActive) {
		if (g_launchSeqPhase < 5) {
			// Proving-ground placement result + combat-chamber instructional voice lines.
			if (g_yardFinishPlacementMessagePending && g_yardFinishPlacementResultCode) {
				int code = g_yardFinishPlacementResultCode;
				if (code <= 1000) {
					if (code != 1000) {
						switch (code) {
							case 1:
								g_pendingHudMessageVoiceSfxId = 225;
								msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_FIRST_PLACE]);
								break;
							case 2:
								g_pendingHudMessageVoiceSfxId = 224;
								msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_SECOND_PLACE]);
								break;
							case 3:
								g_pendingHudMessageVoiceSfxId = 223;
								msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_THIRD_PLACE]);
								break;
							case 4:
								g_pendingHudMessageVoiceSfxId = 222;
								msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_FOURTH_PLACE]);
								break;
							case 5:
								g_pendingHudMessageVoiceSfxId = 221;
								msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_FIFTH_PLACE]);
								break;
							case 6:
							case 7:
							case 8:
							case 9:
							case 10:
								g_pendingHudMessageVoiceSfxId = 220;
								msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_YOU_PLACED]);
								break;
							default:
								break;
						}
					} else {
						g_pendingHudMessageVoiceSfxId = 219;
						msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_ALMOST_TENTH_PLACE]);
					}
				} else if (code == 1001) {
					switch ((uint16_t)GameRand2() % 2) {
						case 0:
							g_pendingHudMessageVoiceSfxId = 217;
							msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_AT_LEAST_FINISHED]);
							break;
						case 1:
							g_pendingHudMessageVoiceSfxId = 218;
							msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_NOT_BAD]);
							break;
						default:
							break;
					}
				}
				msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
				g_pendingHudMessageVoiceSfxId = 0;
				g_yardFinishPlacementMessagePending = 0;
			} else if (g_hangarTipStep != 6) {
				int flown = g_pilotData.combatChamberMissions[66].numberTimesFlown;
				if (flown == 1 || flown == 2) {
					if (g_hangarTipDelayMs) {
						g_hangarTipDelayMs -= clampedDtMs;
						if (g_hangarTipDelayMs < 0) {
							g_hangarTipDelayMs = 0;
						}
					}
					if (!g_hangarTipDelayMs && !g_launchTriggered) {
						switch (g_hangarTipStep) {
							case 0:
								g_pendingHudMessageVoiceSfxId = 196;
								msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_GLAD_TO_SEE_COURAGE]);
								g_hangarTipDelayMs = 1652;
								++g_hangarTipStep;
								break;
							case 1:
								g_pendingHudMessageVoiceSfxId = 197;
								msg_addMessagePtr(0,
												  g_strHangarMiscStrings[HANGAR_MISC_YOU_MUST_PUSH_YOURSELF]);
								g_hangarTipDelayMs = 1652;
								++g_hangarTipStep;
								break;
							case 2:
								g_pendingHudMessageVoiceSfxId = 198;
								msg_addMessagePtr(0,
												  g_strHangarMiscStrings[HANGAR_MISC_BEFORE_GOING_TO_GROUND]);
								g_hangarTipDelayMs = 1652;
								++g_hangarTipStep;
								break;
							case 3:
								g_pendingHudMessageVoiceSfxId = 200;
								msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_HALL_OF_FAME_DESC]);
								g_hangarTipDelayMs = 1652;
								++g_hangarTipStep;
								break;
							case 4:
								g_pendingHudMessageVoiceSfxId = 199;
								msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_NOT_OFFICIAL_DESC]);
								g_hangarTipDelayMs = 1652;
								++g_hangarTipStep;
								break;
							case 5:
								switch ((uint16_t)GameRand2() % 2) {
									case 0:
										g_pendingHudMessageVoiceSfxId = 201;
										msg_addMessagePtr(
											0, g_strHangarMiscStrings[HANGAR_MISC_READ_INSTRUCTIONS]);
										break;
									case 1:
										g_pendingHudMessageVoiceSfxId = 202;
										msg_addMessagePtr(
											0, g_strHangarMiscStrings[HANGAR_MISC_PAY_CLOSE_ATTENTION]);
										break;
									default:
										break;
								}
								++g_hangarTipStep;
								break;
							default:
								break;
						}
						msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
						g_pendingHudMessageVoiceSfxId = 0;
					}
				} else if ((unsigned)flown < 6u) {
					int delay = g_hangarTipDelayMs;
					if (delay) {
						delay -= clampedDtMs;
						g_hangarTipDelayMs = delay;
						if (delay < 0) {
							delay = 0;
							g_hangarTipDelayMs = 0;
						}
					}
					if (!g_hangarTipStep && !g_launchTriggered) {
						switch ((uint16_t)GameRand2() % 2) {
							case 0:
								g_pendingHudMessageVoiceSfxId = 196;
								msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_GLAD_TO_SEE_COURAGE]);
								break;
							case 1:
								g_pendingHudMessageVoiceSfxId = 197;
								msg_addMessagePtr(0,
												  g_strHangarMiscStrings[HANGAR_MISC_YOU_MUST_PUSH_YOURSELF]);
								break;
							default:
								break;
						}
						msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
						g_pendingHudMessageVoiceSfxId = 0;
						g_hangarTipStep = 1;
						g_hangarTipDelayMs = 1652;
					} else if (g_hangarTipStep == 1 && !delay && !g_launchTriggered) {
						switch ((uint16_t)GameRand2() % 2) {
							case 0:
								g_pendingHudMessageVoiceSfxId = 198;
								msg_addMessagePtr(0,
												  g_strHangarMiscStrings[HANGAR_MISC_BEFORE_GOING_TO_GROUND]);
								break;
							case 1:
								g_pendingHudMessageVoiceSfxId = 199;
								msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_NOT_OFFICIAL_DESC]);
								break;
							default:
								break;
						}
						msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
						g_pendingHudMessageVoiceSfxId = 0;
						g_hangarTipStep = 2;
						g_hangarTipDelayMs = 1652;
					} else if (g_hangarTipStep == 2 && !delay && !g_launchTriggered) {
						switch ((uint16_t)GameRand2() % 2) {
							case 0:
								g_pendingHudMessageVoiceSfxId = 201;
								msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_READ_INSTRUCTIONS]);
								break;
							case 1:
								g_pendingHudMessageVoiceSfxId = 202;
								msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_PAY_CLOSE_ATTENTION]);
								break;
							default:
								break;
						}
						msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
						g_pendingHudMessageVoiceSfxId = 0;
						g_hangarTipStep = 6;
					}
				} else if ((unsigned)flown < 11u) {
					if (!g_launchTriggered) {
						switch ((uint16_t)GameRand2() % 2) {
							case 0:
								g_pendingHudMessageVoiceSfxId = 201;
								msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_READ_INSTRUCTIONS]);
								break;
							case 1:
								g_pendingHudMessageVoiceSfxId = 202;
								msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_PAY_CLOSE_ATTENTION]);
								break;
							default:
								break;
						}
						msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
						g_pendingHudMessageVoiceSfxId = 0;
						g_hangarTipStep = 6;
					}
				} else {
					g_hangarTipStep = 6;
				}
			}
		}
	} else {
		// Training-hangar control tips.
		if ((unsigned)g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] < 4u) {
			if (g_hangarTipDelayMs) {
				g_hangarTipDelayMs -= clampedDtMs;
				if (g_hangarTipDelayMs < 0) {
					g_hangarTipDelayMs = 0;
				}
			} else if (g_hangarTipStep < 4) {
				g_pendingHudMessageVoiceSfxId = 0;
				switch (g_hangarTipStep) {
					case 0:
						msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_ARROWS_TO_MOVE]);
						g_hangarTipDelayMs = 1800;
						break;
					case 1:
						msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_NUMBER_KEYS_FOR_CAMERA]);
						g_hangarTipDelayMs = 1800;
						break;
					case 2:
						msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_ZERO_FOR_COCKPIT]);
						g_hangarTipDelayMs = 1800;
						break;
					case 3:
						msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_SPACE_TO_LAUNCH]);
						break;
					default:
						break;
				}
				msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
				++g_hangarTipStep;
			}
		}
	}

	// ---- launch-animation phase machine ----
	playerObjIdx = g_hangarPlayerObjIdx;
	dtMsa = clampedDtMs >> 2;
	if (dtMsa > 236) {
		dtMsa = 236;
	}
#ifdef XWA_MODERN
	Hangar_ModernBeginLegacyCadence(dtMsa);
#endif

	switch (g_launchSeqPhase) {
		case 8:
			Hangar_ServicePlayerCraft(dtMsa);
			playerObjIdx = g_hangarPlayerObjIdx;
			/* fall through to launch-trigger handling */
		case 0:
			if (g_launchTriggered) {
				g_launchSeqPhase = 1;
				switch (g_objectTable[playerObjIdx].objectType) {
					case OBJ_CorellianTransport2:
						g_hangarLaunchRollRate = (float)((uint16_t)GameRand() % 40);
						fsfx_PlaySound(2643, 0xFFFFu, g_localPlayer);
						break;
					case OBJ_MilleniumFalcon2:
						g_hangarLaunchRollRate = (float)((uint16_t)GameRand() % 60);
						fsfx_PlaySound(2643, 0xFFFFu, g_localPlayer);
						break;
					case OBJ_FamilyTransport:
						g_hangarLaunchRollRate = (float)((uint16_t)GameRand() % 20);
						fsfx_PlaySound(2643, 0xFFFFu, g_localPlayer);
						break;
					case OBJ_BWing:
						g_hangarLaunchRollRate = (float)((uint16_t)GameRand() % 50);
						break;
					case OBJ_YWing:
						g_hangarLaunchRollRate = (float)((uint16_t)GameRand() % 60);
						break;
					default:
						g_hangarLaunchRollRate = (float)((uint16_t)GameRand() % 100);
						break;
				}
				g_objectTable[g_hangarPlayerObjIdx].mobj->pCraft->throttleSpeed = 4096;
				fsfx_UpdatePlayerEngineLoop();
				if (g_objectTable[g_hangarPlayerObjIdx].objectType == OBJ_CorellianTransport2 ||
					g_objectTable[g_hangarPlayerObjIdx].objectType == OBJ_FamilyTransport) {
					fsfx_PlaySound(139, g_hangarPlayerObjIdx, g_localPlayer);
				} else {
					fsfx_PlaySound(132, g_hangarPlayerObjIdx, g_localPlayer);
				}
			}
			break;
		case 1: {
			// Lift-off: rise and damp the launch roll back to level.
			int objIdx = g_hangarPlayerObjIdx;
			double fdt = (double)dtMsa;
			int steps = dtMsa;
			double rate;

			g_objectTable[g_hangarPlayerObjIdx].world_z += dtMsa;
			g_objectTable[objIdx].roll += (Q16Angle)((int)g_hangarLaunchRollRate * dtMsa);
			rate = (0x8000 - g_objectTable[objIdx].roll <= 0) ? (g_hangarLaunchRollRate + fdt)
															  : (g_hangarLaunchRollRate - fdt);
			g_hangarLaunchRollRate = (float)rate;
			if (dtMsa > 0) {
				do {
					rate = rate * 0.95999998;
					--steps;
				} while (steps);
				g_hangarLaunchRollRate = (float)rate;
			}
			if ((int)g_objectTable[objIdx].roll < 10 && abs((int)g_hangarLaunchRollRate) < 6) {
				g_objectTable[objIdx].roll = 0;
				g_hangarLaunchRollRate = 0.0f;
			}
			g_objectTable[objIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[objIdx].mobj->moveVectorDirty = 1;
			if (g_objectTable[objIdx].world_z > g_launchBaseZ + 247) {
				g_launchSeqPhase = 2;
				g_objectTable[objIdx].roll = 0;
				g_hangarLaunchRollRate = 0.0f;
				g_hangarLaunchPitchRate = 30.0f;
				g_objectTable[objIdx].pitch += (Q16Angle)(int64_t)(fdt * 30.0);
				if (!g_hangarAutoCam) {
					Hangar_SetCameraShot((uint16_t)GameRand() % 10);
				}
			}
			break;
		}
		case 2: {
			// Climb-out: move up, ramp throttle and pitch over.
			int objIdx = g_hangarPlayerObjIdx;
			float fdt = (float)dtMsa;
			uint16_t throttle;

			g_objectTable[g_hangarPlayerObjIdx].world_y += (int64_t)g_hangarLaunchMoveSpeed * dtMsa;
			g_hangarLaunchMoveSpeed = g_hangarLaunchMoveSpeed - fdt * -0.2f;
			throttle = g_objectTable[g_hangarPlayerObjIdx].mobj->pCraft->throttleSpeed;
			if (throttle != 0xFFFF) {
				throttle = (uint16_t)(throttle + 3840);
				g_objectTable[g_hangarPlayerObjIdx].mobj->pCraft->throttleSpeed = throttle;
				if (throttle < 0x1000u) {
					g_objectTable[g_hangarPlayerObjIdx].mobj->pCraft->throttleSpeed =
						(uint16_t)(throttle - 1);
				}
			}
			if (g_objectTable[objIdx].pitch > g_hangarSavedPlayerPitch) {
				g_objectTable[objIdx].pitch += (Q16Angle)(int64_t)(g_hangarLaunchPitchRate * fdt);
				if (g_hangarLaunchPitchRate > -20.0f) {
					g_hangarLaunchPitchRate = g_hangarLaunchPitchRate - (float)(fdt * 0.40000001);
				}
			}
			g_objectTable[objIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[objIdx].mobj->moveVectorDirty = 1;
			if (g_objectTable[objIdx].world_y >
				(g_hangarBackdropModelType != 308 ? -900 : 2500) + g_objectTable[g_launchRefObjIdx].world_y) {
				g_launchAnimDone = 1;
				g_launchSeqPhase = 4;
			}
			break;
		}
		case 5: {
			// Carrier-bay exit: descend toward the launch rail and pitch up.
			int objIdx = g_hangarPlayerObjIdx;
			CraftData* craft;
			float decel;
			int railZ;

			g_objectTable[g_hangarPlayerObjIdx].world_y -= (int64_t)g_hangarLaunchMoveSpeed * dtMsa;
			decel = (g_hangarBackdropModelType == 308) ? 0.16f : 0.085000001f;
			if (g_hangarLaunchMoveSpeed > 3.0f) {
				g_hangarLaunchMoveSpeed = g_hangarLaunchMoveSpeed - (float)((double)dtMsa * decel);
			}
			if (g_hangarLaunchMoveSpeed < 3.0f) {
				g_hangarLaunchMoveSpeed = 3.0f;
			}
			if (g_objectTable[objIdx].world_z > g_launchBaseZ + 147) {
				int objectType = g_objectTable[g_hangarPlayerObjIdx].objectType;
				if (objectType != OBJ_BWing) {
					railZ = ModelBounds_GetSizeZ((uint16_t)objectType) / 2;
				} else {
					railZ = 50;
				}
				if (g_objectTable[objIdx].world_z > g_launchBaseZ + railZ) {
					int step;
					if (g_hangarLaunchMoveSpeed > 2.0f) {
						step = 2 * dtMsa;
					} else {
						step = (int)((double)dtMsa * g_hangarLaunchMoveSpeed);
					}
					g_objectTable[objIdx].world_z -= step;
				}
			}
			craft = g_objectTable[objIdx].mobj->pCraft;
			craft->throttleSpeed = (uint16_t)(int64_t)(g_hangarLaunchMoveSpeed * 1024.0);
			if (g_objectTable[objIdx].pitch < 0x4000u) {
				double fdt = (double)dtMsa;
				g_objectTable[objIdx].pitch += (Q16Angle)(int64_t)(g_hangarLaunchPitchRate * fdt);
				g_hangarLaunchPitchRate = g_hangarLaunchPitchRate - (float)(fdt * -0.5);
				if (g_objectTable[objIdx].pitch > 0x4000u) {
					g_objectTable[objIdx].pitch = 0x4000;
				}
			}
			g_objectTable[objIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[objIdx].mobj->moveVectorDirty = 1;
			if (g_objectTable[objIdx].world_y <= (g_hangarBackdropModelType != 308 ? -6800 : -800) +
													 g_objectTable[g_launchRefObjIdx].world_y) {
				g_launchSeqPhase = 6;
				g_hangarLaunchYawRate = 0.0f;
				craft->throttleSpeed = 12288;
				if (g_hangarMissionResolved) {
					if (!g_provingGroundsModeActive &&
						g_missionFlightRuntimeState.teamGoalStatus[(uint16_t)g_players[g_localPlayer]
																	   .playerIff][TEAM_GOAL_PRIMARY] == 1) {
						g_flightExitRequest = 1;
					} else {
						Hangar_SetCameraShot(0);
					}
				}
			}
			break;
		}
		case 6: {
			// Carrier-bay exit: yaw the craft around toward the launch heading.
			int objIdx = g_hangarPlayerObjIdx;
			float fdt = (float)dtMsa;
			Q16Angle yaw = g_objectTable[g_hangarPlayerObjIdx].yaw;
			int railZ;

			if (yaw <= 0x7000u || g_hangarLaunchYawRate >= 200.0f) {
				if (yaw > 0xC000u) {
					if (g_hangarLaunchYawRate > 2.0f) {
						g_hangarLaunchYawRate = g_hangarLaunchYawRate - (float)(3 * dtMsa);
					} else {
						g_hangarLaunchYawRate = 2.0f;
					}
				}
			} else {
				g_hangarLaunchYawRate = (float)(2 * dtMsa) + g_hangarLaunchYawRate;
			}
			g_objectTable[objIdx].yaw += (Q16Angle)(int64_t)(g_hangarLaunchYawRate * fdt);
			{
				int objectType = g_objectTable[g_hangarPlayerObjIdx].objectType;
				if (objectType != OBJ_BWing) {
					railZ = ModelBounds_GetSizeZ((uint16_t)objectType) / 2;
				} else {
					railZ = 50;
				}
			}
			if (g_objectTable[objIdx].world_z > g_launchBaseZ + railZ) {
				g_objectTable[objIdx].world_z -= dtMsa;
			}
			if (g_objectTable[objIdx].pitch < 0x4000u) {
				g_objectTable[objIdx].pitch += (Q16Angle)(int64_t)(g_hangarLaunchPitchRate * fdt);
				g_hangarLaunchPitchRate = g_hangarLaunchPitchRate - (float)(fdt * -0.5f);
				if (g_objectTable[objIdx].pitch > 0x4000u) {
					g_objectTable[objIdx].pitch = 0x4000;
				}
			}
			g_objectTable[objIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[objIdx].mobj->moveVectorDirty = 1;
			if (g_objectTable[objIdx].yaw < 0x6000u) {
				g_objectTable[objIdx].yaw = 0;
				g_launchSeqPhase = 7;
				fsfx_PlaySound(131, playerObjIdx, g_localPlayer);
			}
			break;
		}
		case 7: {
			// Carrier-bay exit: lower the craft onto the launch rail and park it.
			int objIdx = g_hangarPlayerObjIdx;
			int railZ;

			g_objectTable[g_hangarPlayerObjIdx].world_z -= dtMsa;
			g_objectTable[playerObjIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[playerObjIdx].mobj->moveVectorDirty = 1;
			{
				int objectType = g_objectTable[g_hangarPlayerObjIdx].objectType;
				if (objectType != OBJ_BWing) {
					railZ = ModelBounds_GetSizeZ((uint16_t)objectType) / 2;
				} else {
					railZ = 50;
				}
			}
			if (g_objectTable[objIdx].world_z <= g_launchBaseZ + railZ) {
				if (g_hangarBackdropModelType == 308) {
					g_objectTable[objIdx].world_x = g_objectTable[g_launchRefObjIdx].world_x;
					g_objectTable[objIdx].world_y = g_objectTable[g_launchRefObjIdx].world_y - 800;
				} else {
					g_objectTable[objIdx].world_x = g_objectTable[g_launchRefObjIdx].world_x + 142;
					g_objectTable[objIdx].world_y = g_objectTable[g_launchRefObjIdx].world_y - 6800;
				}
				g_objectTable[objIdx].world_z =
					g_launchBaseZ +
					Hangar_GetLaunchModelZOffset(g_objectTable[g_hangarPlayerObjIdx].objectType);
				g_objectTable[objIdx].yaw = 0;
				g_objectTable[objIdx].pitch = 0x4000;
				g_launchSeqPhase = 8;
				g_objectTable[g_hangarPlayerObjIdx].mobj->pCraft->throttleSpeed = 0;
				g_hangarServiceCooldown = 236;
				fsfx_PlaySound(131, playerObjIdx, g_localPlayer);
				g_objectTable[g_hangarPlayerObjIdx].mobj->pCraft->throttleSpeed = 0;
				Hangar_SetCameraShot(0);
				if (g_provingGroundsModeActive) {
					g_flightExitRequest = 2;
				}
			}
			break;
		}
		default:
			break;
	}

	FVIEW_calcrotatemove(g_objectTable[playerObjIdx].pitch, g_objectTable[playerObjIdx].yaw,
						 &g_objectTable[playerObjIdx]);
	FVIEW_calcrotateorient(g_objectTable[playerObjIdx].roll, g_objectTable[playerObjIdx].angleD,
						   &g_objectTable[playerObjIdx]);

	// Occasional random camera shot.
#ifdef XWA_MODERN
	if ((!XwaModernFlightTiming_IsHighRate() || g_modernHangarLegacyCadenceDue) &&
#else
	if (
#endif
		(unsigned)(4 * g_gameTime - g_hangarEntryTime4x) > 0x7530 &&
		(unsigned)(4 * g_gameTime - g_hangarLastRandomCameraTime4x) > 0x2710 &&
		(unsigned)(GameRand() & 0x1FF) < 0xAu) {
		g_hangarLastRandomCameraTime4x = 4 * g_gameTime;
		Hangar_SetCameraShot((uint16_t)GameRand() % 10);
	}

	if (g_hangarBackdropModelType == 308) {
		Hangar_UpdateHangarDroidTraffic(dtMsa);
		Hangar_UpdateShuttleTrafficCycle(dtMsa);
		Hangar_UpdateRoofCraneMotion(dtMsa);
	} else if (g_hangarBackdropModelType == 179) {
		Hangar_UpdateFamilyBaseDroidTraffic(dtMsa);
	}
	Hangar_HandleInput();

	{
		++g_fpsSampleRingIndex;
		g_fpsSampleHistory[g_fpsSampleRingIndex] = (float)g_simStepScale;
		if (g_fpsSampleRingIndex == 4) {
			g_fpsSampleRingIndex = 0;
		}
	}

	if (g_inputTimestamp - (unsigned)g_hangarLastSoundUpdateTimestamp > 0x1Du) {
		g_hangarLastSoundUpdateTimestamp = (int)g_inputTimestamp;
		// Sound_Update3DListenerAndSources() omitted: DirectSound3D listener path -> Aeron audio.
		if (((uint8_t)g_inputTimestamp & 0x40) != 0 && (GameRand2() & 0x3F) == 1) {
			fsfx_PlaySound(69, (uint16_t)g_hangarPlayerObjIdx, g_localPlayer);
		}
	}

	refObjIdx = g_hangarSourceObjIdx;
	if (g_hangarSourceObjIdx == 0xFFFF) {
		int departureMothership =
			g_missionFlightGroups[g_objectTable[g_hangarPlayerObjIdx].flightGroupIdx].fg.departureMothership;
		int start, end, i;

		Mission_SetActiveRegionObjectRanges(g_hangarSavedMissionRegionIdx);
		start = g_activeRegionObjectSlotStart;
		end = g_activeRegionCraftObjectSlotEnd;
		Mission_SetActiveRegionObjectRanges(g_hangarSceneRegionIdx);
		for (i = start; i < end; ++i) {
			if (g_objectTable[i].flightGroupIdx == departureMothership) {
				ObjectTypeId t = g_objectTable[i].objectType;
				if (t == OBJ_CalamariCruiserNew || t == OBJ_CalamariWinged || t == OBJ_FamilyBase) {
					refObjIdx = i;
					break;
				}
			}
		}
	}

	if (refObjIdx != 0xFFFF) {
		CraftData* mship = g_objectTable[refObjIdx].mobj->pCraft;
		int descId = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];

		if ((uint32_t)mship->hullDamage >= MATH2_longfraction((uint32_t)mship->hullMax, 0x8000u) ||
			(descId == 20 &&
			 g_missionFlightRuntimeState
					 .teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff][TEAM_GOAL_PRIMARY] != 1)) {
			float pulse = (float)(4 * g_gameTime % 1000) * 0.001f;
			FlightLight_ClearDirectionalLights();
			FlightLight_AddDirectionalLight(0, 0, 0xFFFF, pulse, pulse, 0.0f, 0.0f);
			FlightLight_AddDirectionalLight(0x4000, 53248, 0x4000, pulse, pulse, 0.0f, 0.0f);
			if (pulse > 0.9f) {
				fsfx_PlaySound(58, 0xFFFFu, g_localPlayer);
			}
		} else if (mship->shieldFront <= 200 && mship->shieldRear <= 200) {
			float pulse = (float)(4 * g_gameTime % 1000) * 0.00062499999f;
			FlightLight_ClearDirectionalLights();
			FlightLight_AddDirectionalLight(0, 0, 0xFFFF, pulse, 1.0f, 0.0f, 0.0f);
			FlightLight_AddDirectionalLight(0x4000, 0x4000, 0x4000, pulse, 1.0f, 0.0f, 0.0f);
		}
		if ((unsigned)mship->hullDamage >= (unsigned)mship->hullMax) {
			collide_damagecraft(g_hangarPlayerObjIdx, 0xFFFFu, 0xFFFFu, 0, 0);
			g_launchAnimDone = 1;
		}
	}

	fsfx_UpdatePlayerEngineLoop();
	fsfx_UpdateVoiceQueue();
	if (g_launchAnimDone) {
		Hangar_LaunchPlayerCraft();
		return g_flightExitRequest;
	}
	if (g_flightExitRequest) {
		Hangar_LaunchPlayerCraft();
		return g_flightExitRequest;
	}

	if (g_gameTime >= g_hangarNextAmbientSoundTime) {
		fsfx_PlaySound((uint16_t)GameRand2() % 10 + 151, 0xFFFFu, g_localPlayer);
		g_hangarNextAmbientSoundTime = g_gameTime + 236 * ((GameRand2() & 7) + 9);
	}
	Mission_SetActiveRegionObjectRanges(g_players[g_localPlayer].regionIndex);
	g_flightSideEffectsEnabled = 0;
	return 0;
}

// FUNCTION: XWA 0x458DC0
void Hangar_LaunchPlayerCraft(void) {
	unsigned int newObjIdx;
	unsigned int sourceObjIdx;
	CraftData* craft;
	MobileObject* playerMobile;
	ObjectRecord* exitSourceObj;
	int* sfxId;

	RenderScene_ResetDepthProjectionScale();
	Math_SetFpuSinglePrecisionMode();
	if (g_provingGroundsModeActive) {
		Yard_FreeHighScoreTable(g_yardHighScoreTable);
		g_yardHighScoreTable = NULL;
		g_yardCurrentCraftScoreTable = NULL;
	}

	g_hangarTipStep = 4;
	if (g_flightExitRequest) {
		g_inHangarReady = 0;
		return;
	}

	ForceFeedback_StopAllEffects();
	ForceFeedback_EnableEffects();
	for (sfxId = &g_sfxIds[4];
#ifdef XWA_MODERN
		 sfxId < &g_sfxIds[196];
#else
		 (intptr_t)sfxId < (intptr_t)&g_sfxIds[196];
#endif
		 ++sfxId) {
		if (Sound_CountPlayingInstances(*sfxId)) {
			Sound_StopOldestInstance(*sfxId);
		}
	}

	{
		int launchSfxSlot;

		launchSfxSlot = -1;
		switch (g_objectTable[g_hangarPlayerObjIdx].objectType) {
			case OBJ_TIEFighter:
			case OBJ_TIEInterceptor:
			case OBJ_TIEBomber:
			case OBJ_TIEAdvanced:
			case OBJ_TIEDefender:
				launchSfxSlot = (g_sound3DEnabled && g_sfxIds[102] != -1) ? 102 : 101;
				break;

			case OBJ_MissileBoat:
			case OBJ_AssaultGunboat:
			case OBJ_RazorFighter:
			case OBJ_PlanetaryFighter:
			case OBJ_PreybirdFighter:
			case OBJ_Tug:
			case OBJ_CombatUtilityVehicle:
			case OBJ_HeavyLifter:
			case OBJ_Shuttle:
			case OBJ_EscortShuttle:
			case OBJ_StormtrooperTransport:
			case OBJ_AssaultTransport:
			case OBJ_EscortTransport:
			case OBJ_SystemPatrolCraft:
			case OBJ_ScoutCraft:
				launchSfxSlot = (g_sound3DEnabled && g_sfxIds[104] != -1) ? 104 : 103;
				break;

			case OBJ_XWing:
			case OBJ_BWing:
			case OBJ_Z95:
			case OBJ_R41:
			case OBJ_SlaveOne:
				launchSfxSlot = (g_sound3DEnabled && g_sfxIds[106] != -1) ? 106 : 105;
				break;

			case OBJ_YWing:
			case OBJ_ToscanFighter:
			case OBJ_CloakshapeFighter:
				launchSfxSlot = (g_sound3DEnabled && g_sfxIds[108] != -1) ? 108 : 107;
				break;

			case OBJ_AWing:
			case OBJ_IrdFighter:
			case OBJ_Twing:
			case OBJ_Piggyback:
				launchSfxSlot = (g_sound3DEnabled && g_sfxIds[110] != -1) ? 110 : 109;
				break;

			case OBJ_SkiprayBlastBoat:
			case OBJ_SupaFighter:
			case OBJ_SlaveTwo:
			case OBJ_CorellianTransport2:
			case OBJ_MilleniumFalcon2:
			case OBJ_FamilyTransport:
				launchSfxSlot = (g_sound3DEnabled && g_sfxIds[112] != -1) ? 112 : 111;
				break;

			default:
				break;
		}

		if (launchSfxSlot != -1) {
			fsfx_PlaySound(launchSfxSlot, g_hangarPlayerObjIdx, (unsigned int)g_localPlayer);
		}
	}

	if (!g_useHardware3D) {
		FlightText_SetClipRect(0, 0, g_hudMfdPaneWidth, g_hudMfdPaneHeight);
		FlightSw_SetRenderTarget(g_hudMfdLeftTexPixels, (uint16_t)g_hudMfdPaneWidth,
								 (uint16_t)g_hudMfdPaneHeight,
								 (uint16_t)g_hudMfdPaneWidth * g_flight16bppBytesPerPixel);
		g_flightFillClipRectFn();
		FlightSw_SetRenderTarget(g_hudMfdRightTexPixels, (uint16_t)g_hudMfdPaneWidth,
								 (uint16_t)g_hudMfdPaneHeight,
								 (uint16_t)g_hudMfdPaneWidth * g_flight16bppBytesPerPixel);
		g_flightFillClipRectFn();
	}

	Hangar_SetCameraShot(0);
	g_players[g_localPlayer].lookYawOffset = 0;
	g_players[g_localPlayer].lookPitchOffset = 0;
	FlightView_UpdatePlayerCamera(g_localPlayer);
	g_players[g_localPlayer].regionIndex = (uint8_t)g_hangarSavedMissionRegionIdx;
	Mission_SetActiveRegionObjectRanges(g_players[g_localPlayer].regionIndex);
	newObjIdx = Object_AllocSlotForGenus(g_objectTable[g_hangarPlayerObjIdx].genusId);
	Object_CopyStatePreservingStorage(newObjIdx, (unsigned int)g_hangarPlayerObjIdx);
	g_players[g_localPlayer].objectIndex = newObjIdx;
	g_players[g_localPlayer].viewState.cameraFocusObjIdx = newObjIdx;
	g_objectTable[newObjIdx].regionIdx = (uint8_t)g_hangarSavedMissionRegionIdx;
	g_objectTable[g_hangarPlayerObjIdx].objectType = OBJ_None;
	playerMobile = g_objectTable[newObjIdx].mobj;
	sourceObjIdx = g_hangarSourceObjIdx;
	craft = playerMobile->pCraft;

	if (sourceObjIdx == 0xffff || g_flightExitRequest) {
		g_objectTable[newObjIdx].world_x = g_hangarSavedPlayerWorldX;
		g_objectTable[newObjIdx].world_y = g_hangarSavedPlayerWorldY;
		g_objectTable[newObjIdx].world_z = g_hangarSavedPlayerWorldZ;
		g_objectTable[newObjIdx].pitch = (Q16Angle)g_hangarSavedPlayerPitch;
		g_objectTable[newObjIdx].yaw = (Q16Angle)g_hangarSavedPlayerYaw;
		g_objectTable[newObjIdx].roll = (Q16Angle)g_hangarSavedPlayerRoll;
		g_objectTable[newObjIdx].angleD = (Q16Angle)g_hangarSavedPlayerAngleD;
		craft->throttleSpeed = (uint16_t)g_hangarSavedThrottleSpeed;
		craft->throttleSpeed = (uint16_t)g_hangarSavedThrottleSpeed;
		craft->beamLevel = (uint8_t)g_hangarSavedBeamLevel;
		craft->shieldRedirect = (uint8_t)g_hangarSavedShieldRedirect;
		craft->laserRedirect = (uint8_t)g_hangarSavedLaserRedirect;
		g_missionElapsedClock.hours = 0;
		g_missionElapsedClock.minutes = 0;
		g_missionElapsedClock.seconds = 0;
		g_missionElapsedClock.subsecondTicks = 0;
	} else {
		ObjectRecord* sourceObj;
		int targetWorldX;
		int targetWorldY;
		int targetWorldZ;
		int exitWorldX;
		int exitWorldY;
		int exitWorldZ;
		ModelIndex sourceModelIndex;
		CraftData* sourceCraft;

		sourceObj = &g_objectTable[sourceObjIdx];
		targetWorldX = sourceObj->world_x;
		targetWorldY = sourceObj->world_y;
		sourceCraft = sourceObj->mobj->pCraft;
		targetWorldZ = sourceObj->world_z;
		sourceModelIndex = GetModelIndexFromType(sourceObj->objectType);
		if (sourceModelIndex != 0xffffu) {
			if ((uint32_t)sourceCraft->hullDamage < (uint32_t)sourceCraft->hullMax) {
				int localSide;

				if (g_hangarLaunchMirrorAttachOffsets) {
					localSide = -g_modelDefs[sourceModelIndex].meshAttachData[8];
				} else {
					localSide = g_modelDefs[sourceModelIndex].meshAttachData[8];
				}
				pai_RotateLocalVectorToWorldScratch(&g_objectTable[g_hangarSourceObjIdx], localSide,
													g_modelDefs[sourceModelIndex].meshAttachData[9],
													g_modelDefs[sourceModelIndex].meshAttachData[10]);
				targetWorldX += g_rotatedX;
				targetWorldY += g_rotatedY;
				targetWorldZ += g_rotatedZ;
			}

			exitSourceObj = &g_objectTable[g_hangarSourceObjIdx];
			exitWorldX = exitSourceObj->world_x;
			exitWorldY = exitSourceObj->world_y;
			exitWorldZ = exitSourceObj->world_z;
			pai_RotateLocalVectorToWorldScratch(exitSourceObj,
												g_hangarLaunchMirrorAttachOffsets
													? -g_modelDefs[sourceModelIndex].meshAttachData[5]
													: g_modelDefs[sourceModelIndex].meshAttachData[5],
												g_modelDefs[sourceModelIndex].meshAttachData[6],
												g_modelDefs[sourceModelIndex].meshAttachData[7]);
			exitWorldX += g_rotatedX;
			exitWorldY += g_rotatedY;
			exitWorldZ += g_rotatedZ;
		}
#ifdef XWA_MODERN
		else {
			exitWorldX = targetWorldX;
			exitWorldY = targetWorldY;
			exitWorldZ = targetWorldZ;
		}
#endif

		if ((uint32_t)sourceCraft->hullDamage >= (uint32_t)sourceCraft->hullMax) {
			trig2_ctop(exitWorldX - targetWorldX, exitWorldY - targetWorldY, exitWorldZ - targetWorldZ);
		} else {
			trig2_ctop(targetWorldX - exitWorldX, targetWorldY - exitWorldY, targetWorldZ - exitWorldZ);
		}
		g_objectTable[newObjIdx].pitch = (Q16Angle)targetPitch;
		g_objectTable[newObjIdx].yaw = (Q16Angle)trig2_xyangle;
		g_objectTable[newObjIdx].roll = 0;
		g_objectTable[newObjIdx].angleD = 0;
		g_objectTable[newObjIdx].world_x = exitWorldX + (targetWorldX - exitWorldX) / 6;
		g_objectTable[newObjIdx].world_y = exitWorldY + (targetWorldY - exitWorldY) / 6;
		g_objectTable[newObjIdx].world_z = exitWorldZ + (targetWorldZ - exitWorldZ) / 6;
		{
			uint16_t maxSpeedCache;

			maxSpeedCache = (uint16_t)craft->aiFlight.maxSpeedCache;
			craft->throttleSpeed = 0xaaaau;
			g_objectTable[newObjIdx].mobj->speed = (uint16_t)(maxSpeedCache >> 3);
			g_objectTable[newObjIdx].mobj->simStateTimestamp = g_gameTime;
			g_players[g_localPlayer].lockstepTimestamp = g_gameTime;
			g_objectTable[newObjIdx].objectSignature = g_nextObjectSignature;
			++g_nextObjectSignature;
			g_players[g_localPlayer].boundObjectSignature = g_objectTable[newObjIdx].objectSignature;
			craft->throttleSpeed = (uint16_t)g_hangarSavedThrottleSpeed;
			craft->throttleSpeed = (uint16_t)g_hangarSavedThrottleSpeed;
			craft->beamLevel = (uint8_t)g_hangarSavedBeamLevel;
			craft->shieldRedirect = (uint8_t)g_hangarSavedShieldRedirect;
			craft->laserRedirect = (uint8_t)g_hangarSavedLaserRedirect;
		}
	}

	g_objectTable[newObjIdx].mobj->framesAlive = 0;
	if (g_objectTable[newObjIdx].objectType == OBJ_XWing ||
		g_objectTable[newObjIdx].objectType == OBJ_BWing) {
		craft->sFoilState = (uint8_t)((craft->sFoilState ^ 2u) | 1u);
		fsfx_PlaySound(120, 0xffffu, (unsigned int)g_localPlayer);
		msg_emitInFlightMessage(MSG_SFOILS_OPENING, g_localPlayer);
	}

	g_players[g_localPlayer].currentTargetObjectIdx = 0xffffu;
	g_players[g_localPlayer].selectedTargetComponent = 0;
	FVIEW_calcrotatemove(g_objectTable[newObjIdx].pitch, g_objectTable[newObjIdx].yaw,
						 &g_objectTable[newObjIdx]);
	FVIEW_calcrotateorient(g_objectTable[newObjIdx].roll, g_objectTable[newObjIdx].angleD,
						   &g_objectTable[newObjIdx]);
	Hud_RestorePlayerHudState(g_localPlayer);
	g_inHangarReady = 0;
	if (g_useHardware3D || (Hud_RedrawSoftwareHudFrame(), g_useHardware3D)) {
		if (g_hangarBackdropModelType != 179) {
			Renderer_FlushTextureCacheAndReturnTrue();
			Hangar_RenderFourStepCameraTransition();
			Math_SetFpuSinglePrecisionMode();
		}
	}

	Time_GetFrameDelta();
	FlightLight_ClearDirectionalLights();
	FlightLight_AddCurrentRegionBackdropLights();
	if (g_provingGroundsModeActive) {
		Yard_InitChallengeScene();
	}
}

// FUNCTION: XWA 0x4598E0
void Hangar_RenderReadyScreen(void) {
	PlayerData* player;
	int shouldRender;

	Math_SetFpuSinglePrecisionMode();
	player = &g_players[g_localPlayer];
	Mission_SetActiveRegionObjectRanges(player->regionIndex);

	if (g_hangarAutoCam) {
		Hangar_UpdateReadyAutoCamera(player);
	} else {
		FlightView_UpdatePlayerCamera(g_localPlayer);
	}

	shouldRender = 0;
	if (g_filmPlaybackMode && (uint8_t)g_pauseState > 2u) {
		g_pauseState++;
		if ((uint8_t)g_pauseState > 9u) {
			shouldRender = 1;
			g_pauseState = 3;
		}
	} else {
		shouldRender = 1;
	}

	if (!shouldRender) {
		Math_SetFpuSinglePrecisionMode();
		return;
	}

#ifdef XWA_MODERN
	XwaSnapshotHud_BeginClassicFrame();
#endif
	Hangar_RenderScene();
	if (!g_useHardware3D) {
		FlightSurface_Lock();
	}
	Hud_DrawFilmRecordingIndicator();
	if (!g_useHardware3D) {
		FlightSurface_Unlock();
	}

	if (!player->hudEnabled) {
		if (g_filmPlaybackMode) {
			if (!g_useHardware3D) {
				FlightSurface_Lock();
			}
			if (g_filmOverlayMfdVisible) {
				Hud_DrawHangarFilmMfdOverlay();
			}
			if (!g_useHardware3D) {
				FlightSurface_Unlock();
			}
		}
#ifdef XWA_MODERN
		XwaSnapshotHud_EndClassicFrame();
#endif
		Hangar_FinishReadyScreenFrame();
		return;
	}

	if (!player->regionSessionId && !g_flightMissionEndPending && !player->viewState.externalCameraActive) {
		if (!g_useHardware3D) {
			FlightSurface_Lock();
		}
		if (g_provingGroundsModeActive) {
			Hangar_DrawProvingGroundStatusPanel();
		} else {
			Hangar_DrawReadyCraftPanel();
		}
		Hangar_DrawMenu();
		if (!g_useHardware3D) {
			FlightSurface_Unlock();
		}
	}

#ifdef XWA_MODERN
	XwaSnapshotHud_EndClassicFrame();
#endif
	Hangar_FinishReadyScreenFrame();
}

// FUNCTION: XWA 0x4596C0
void Hangar_RenderFourStepCameraTransition(void) {
	int objectIdx;
	int step;
	Q16Angle savedYaw;
	int savedX;
	int savedY;
	int savedZ;
	int stepX;
	int stepY;
	int stepZ;

	ModelTexture_CacheHyperspaceTunnelFrames();
	step = 0;
	objectIdx = g_players[g_localPlayer].objectIndex;
	savedYaw = g_objectTable[objectIdx].yaw;
	savedX = g_objectTable[objectIdx].world_x;
	savedY = g_objectTable[objectIdx].world_y;
	savedZ = g_objectTable[objectIdx].world_z;

	for (; step < 4; ++step) {
		Mission_SetActiveRegionObjectRanges(g_players[g_localPlayer].regionIndex);
		RenderList_Reset();

		switch (step) {
			case 0:
				g_objectTable[objectIdx].yaw = (Q16Angle)(0x8000 - savedYaw);
				break;
			case 1:
				g_objectTable[objectIdx].yaw = savedYaw;
				g_objectTable[objectIdx].world_x = savedX + stepX;
				g_objectTable[objectIdx].world_y = savedY + stepY;
				g_objectTable[objectIdx].world_z = savedZ + stepZ;
				break;
			case 2:
				g_objectTable[objectIdx].yaw = savedYaw;
				g_objectTable[objectIdx].world_x = savedX + 2 * stepX;
				g_objectTable[objectIdx].world_y = savedY + 2 * stepY;
				g_objectTable[objectIdx].world_z = savedZ + 2 * stepZ;
				break;
			case 3:
				g_objectTable[objectIdx].yaw = savedYaw;
				g_objectTable[objectIdx].world_x = savedX;
				g_objectTable[objectIdx].world_y = savedY;
				g_objectTable[objectIdx].world_z = savedZ;
				break;
			default:
				break;
		}

		FlightView_UpdatePlayerCamera(g_localPlayer);
		if (step == 0) {
			stepX = Xwa_Q15MulReuseFirstSlot(g_curMatR2_X, 0x4000);
			stepY = Xwa_Q15MulReuseFirstSlot(g_curMatR2_Y, 0x4000);
			stepZ = Xwa_Q15MulReuseFirstSlot(g_curMatR2_Z, 0x4000);
		}

#ifdef XWA_MODERN
		XwaSnapshotHud_BeginClassicFrame();
#endif
		if (g_inHangarReady) {
			Hangar_RenderScene();
		} else {
			FlightView_Render();
		}
#ifdef XWA_MODERN
		XwaSnapshotHud_EndClassicFrame();
#endif
	}

	RenderScene_End3D();
	RenderScene_ClearFrameBuffers();
	if (g_flightInitialTextureCacheFlushPending) {
		g_flightInitialTextureCacheFlushPending = 0;
	}
}

// FUNCTION: XWA 0x45A520
void Hangar_RenderScene(void) {
	int scanObjIdx;
	int objectIdx;
	RenderObjectListEntry* entry;
	int projectedRadius;
	uint16_t queuedObjectIdx;

	g_scenePointLightCount = 0;
	for (scanObjIdx = g_activeRegionObjectSlotStart; (uint16_t)scanObjIdx < g_explosionObjectSlotEnd;
		 ++scanObjIdx) {
		objectIdx = (uint16_t)scanObjIdx;
		if (g_objectTable[objectIdx].objectType != OBJ_None) {
			FlightLight_AppendScenePointLightForObject(&g_objectTable[objectIdx]);
		}
	}

	g_flightSurfaceAlreadyLocked = 0;
	FlightSurface_Lock();
	FlightStarfield_Render();
	FlightSurface_Unlock();
	RenderScene_Initialize(1);
	g_sceneBillboardQueueCount = 0;

	g_players[g_localPlayer].regionIndex = (uint8_t)g_hangarSavedMissionRegionIdx;
	Backdrop_RenderCurrentRegion();
	g_players[g_localPlayer].regionIndex = (uint8_t)g_hangarSceneRegionIdx;

	RenderList_Reset();
	for (scanObjIdx = g_regionMainObjectSlotStart; (uint16_t)scanObjIdx < g_regionMainObjectSlotEnd;
		 ++scanObjIdx) {
		objectIdx = (uint16_t)scanObjIdx;
		if (objectIdx != g_players[g_localPlayer].objectIndex ||
			g_players[g_localPlayer].viewState.externalCameraActive || g_replayViewMode) {
			ObjectRecord* obj;
			uint16_t objectType;

			objectType = (uint16_t)g_objectTable[objectIdx].objectType;
			obj = &g_objectTable[objectIdx];
			if (objectType == OBJ_None) {
				continue;
			}

			if (objectType == OBJ_NoAsset_222 && g_objectTable[OBJ_NoAsset_222].mobj != NULL) {
				g_curModelMaxExtent = g_modelTypeTable[obj->mobj->sourceObjectType].maxBoundsExtent;
			} else {
				g_curModelMaxExtent = g_modelTypeTable[objectType].maxBoundsExtent;
			}

			switch (obj->genusId) {
				case GENUS_Fighter:
				case GENUS_Transport:
				case GENUS_Utility:
				case GENUS_Freighter:
				case GENUS_Starship:
				case GENUS_Platform:
				case GENUS_SatelliteBuoy:
				case GENUS_LargeScenery:
				case GENUS_Container:
				case GENUS_PilotDroid:
				case GENUS_WeaponEmplacement:
					g_curCraft = obj->mobj->pCraft;
					if (g_curCraft->objectKind == 7) {
						break;
					}
					g_renderFlags = FlightView_CullObjectSphereToViewport(objectIdx, g_curModelMaxExtent,
																		  &projectedRadius);
					if (g_renderFlags != VIEWPORT_CULL_NONE) {
						RenderList_QueueObject(objectIdx, viewZ, viewX, viewY, viewZ, g_renderFlags,
											   projectedRadius);
					}
					break;
				case GENUS_PlayerProjectile:
				case GENUS_NpcProjectile:
				case GENUS_Debris:
					g_renderFlags = FlightView_CullObjectSphereToViewport(objectIdx, g_curModelMaxExtent,
																		  &projectedRadius);
					if (g_renderFlags != VIEWPORT_CULL_NONE) {
						RenderList_QueueObject(objectIdx, viewZ, viewX, viewY, viewZ, g_renderFlags,
											   projectedRadius);
					}
					break;
				case GENUS_Explosion:
					g_renderFlags = FlightView_CullObjectSphereToViewport(objectIdx, g_curModelMaxExtent,
																		  &projectedRadius);
					if (g_renderFlags != VIEWPORT_CULL_NONE && viewZ > 0) {
						RenderList_QueueObject(objectIdx, viewZ, viewX, viewY, viewZ, g_renderFlags,
											   projectedRadius);
					}
					break;
				default:
					break;
			}
		} else {
			if (g_objectTable[objectIdx].objectType != OBJ_None && g_players[g_localPlayer].cockpitVisible) {
				if (g_players[g_localPlayer].currentSeatIdx == 0) {
					if (g_players[g_localPlayer].cockpitLookAvailable) {
						RenderList_QueueObject(objectIdx, viewZ, viewX, viewY, viewZ,
											   VIEWPORT_CULL_RIGHT | VIEWPORT_CULL_LEFT | VIEWPORT_CULL_TOP |
												   VIEWPORT_CULL_BOTTOM | VIEWPORT_CULL_NEAR,
											   g_screenWidth);
					}
				}
				if (g_players[g_localPlayer].currentSeatIdx > 0) {
					if (g_players[g_localPlayer].cockpitToggleAvailable) {
						RenderList_QueueObject(objectIdx, viewZ, viewX, viewY, viewZ,
											   VIEWPORT_CULL_RIGHT | VIEWPORT_CULL_LEFT | VIEWPORT_CULL_TOP |
												   VIEWPORT_CULL_BOTTOM | VIEWPORT_CULL_NEAR,
											   g_screenWidth);
					}
				}
			}
		}
	}

	{
		int regionIndex;
		uint32_t backdropIdx;
		WorldRectRecord* backdrop;

		regionIndex = g_players[g_localPlayer].regionIndex;
		backdropIdx = 0;
		if (backdropIdx < (uint32_t)g_backdropCountByRegion[regionIndex]) {
			backdrop = g_backdropRecordsByRegion[regionIndex];
			do {
				uint16_t modelType;

				modelType = (uint16_t)backdrop->modelType;
				if (modelType >= OBJ_BackdropTextureGroup9001_Sprite1100_521) {
					if (modelType >= OBJ_BackdropTextureGroup9005_Sprite1500 && backdrop->viewDirQ20.z > 0 &&
						FlightView_IsLensFlareSourceVisible(
							backdrop->worldDirQ20.x + g_players[g_localPlayer].viewState.savedTargetX,
							backdrop->worldDirQ20.y + g_players[g_localPlayer].viewState.savedTargetY,
							backdrop->worldDirQ20.z + g_players[g_localPlayer].viewState.savedTargetZ)) {
						int backdropViewX;
						int backdropViewY;
						int backdropViewZ;

						backdropViewX = backdrop->viewDirQ20.x;
						backdropViewZ = backdrop->viewDirQ20.z;
						backdropViewY = backdrop->viewDirQ20.y;
						viewX = backdropViewX;
						viewY = backdropViewY;
						viewZ = backdropViewZ;
						LensFlare_QueueSource(-1);
					}
				}
				++backdropIdx;
				++backdrop;
			} while (backdropIdx < (uint32_t)g_backdropCountByRegion[regionIndex]);
		}
	}

	for (scanObjIdx = g_objScanStart; (uint16_t)scanObjIdx < g_regionStaticObjectSlotEnd; ++scanObjIdx) {
		uint16_t objectType;

		objectIdx = (uint16_t)scanObjIdx;
		objectType = (uint16_t)g_objectTable[objectIdx].objectType;
		if (objectType == OBJ_None) {
			continue;
		}
		if (g_useHardware3D) {
			g_objRenderState[objectIdx].drawnThisFrame = 0;
		}
		g_curModelMaxExtent = g_modelTypeTable[objectType].maxBoundsExtent;
		switch (g_objectTable[objectIdx].genusId) {
			case GENUS_Mine:
			case GENUS_Asteroid:
			case GENUS_Debris:
			case GENUS_DeathStarTunnelSegment:
				g_renderFlags = FlightView_CullWorldSphereToViewport(
					g_objectTable[objectIdx].world_x, g_objectTable[objectIdx].world_y,
					g_objectTable[objectIdx].world_z, g_curModelMaxExtent, &projectedRadius);
				if (g_renderFlags != VIEWPORT_CULL_NONE) {
					RenderList_QueueObject(objectIdx, viewZ, viewX, viewY, viewZ, g_renderFlags,
										   projectedRadius);
				}
				break;
			default:
				break;
		}
	}

	RenderList_SortDepthAscending();
	g_glowMarkRequestCount = 0;
	for (entry = g_renderListHead; entry != NULL; entry = entry->next) {
		uint16_t genusId;

		queuedObjectIdx = (uint16_t)entry->objectIdx;
		objectIdx = queuedObjectIdx;
		genusId = g_objectTable[objectIdx].genusId;
		if (objectIdx < g_regionMainObjectSlotStart || objectIdx >= g_regionMainObjectSlotEnd) {
			switch (genusId) {
				case GENUS_Mine:
				case GENUS_Asteroid:
				case GENUS_Debris:
				case GENUS_DeathStarTunnelSegment:
					viewX = entry->viewX;
					viewY = entry->viewY;
					viewZ = entry->viewZ;
					g_renderFlags = entry->cullFlags;
					FVIEW_SetObjectTransform(g_objectTable[objectIdx].roll, g_objectTable[objectIdx].pitch,
											 g_objectTable[objectIdx].yaw, g_objectTable[objectIdx].angleD,
											 NULL);
					FlightLight_SetupObjectLighting(&g_objectTable[objectIdx]);
					RenderNonCraftSceneObject(queuedObjectIdx);
					g_objectPointLightCount = 0;
					break;
				default:
					break;
			}
			continue;
		}

		if (g_useHardware3D && g_objRenderState[objectIdx].pendingGlowMarks != NULL) {
			GlowMark_ProcessPendingRequests(queuedObjectIdx);
		}

		switch (genusId) {
			case GENUS_Fighter:
			case GENUS_Transport:
			case GENUS_Utility:
			case GENUS_Freighter:
			case GENUS_Starship:
			case GENUS_Platform:
			case GENUS_SatelliteBuoy:
			case GENUS_LargeScenery:
			case GENUS_Container:
			case GENUS_PilotDroid:
			case GENUS_WeaponEmplacement: {
				uint16_t savedModel;

				savedModel = 0xffffu;
				g_curCraft = g_objectTable[objectIdx].mobj->pCraft;
				FVIEW_SetObjectTransform(g_objectTable[objectIdx].roll, g_objectTable[objectIdx].pitch,
										 g_objectTable[objectIdx].yaw, g_objectTable[objectIdx].angleD,
										 &g_objectTable[objectIdx]);

				if (objectIdx == g_players[g_localPlayer].objectIndex &&
					!g_players[g_localPlayer].viewState.externalCameraActive && !g_replayViewMode) {
					uint16_t objectType;

					objectType = (uint16_t)g_objectTable[objectIdx].objectType;
					if (objectType == OBJ_None) {
						continue;
					}
					savedModel = g_loadedModels.byObjectType[objectType];
					g_curModelMaxExtent = g_modelTypeTable[objectType].maxBoundsExtent;
					g_camRelWorldX = g_objectTable[objectIdx].world_x;
					g_camRelWorldY = g_objectTable[objectIdx].world_y;
					g_camRelWorldZ = g_objectTable[objectIdx].world_z;
					g_cockpitViewActive = 1;
					if (g_players[g_localPlayer].currentSeatIdx != 0) {
						ModelIndex modelIndex;
						uint16_t turretModelType;

						modelIndex = (ModelIndex)GetModelIndexFromType((ObjectTypeId)objectType);
						turretModelType = g_modelDefs[(uint16_t)modelIndex]
											  .turretModelIndex[g_players[g_localPlayer].currentSeatIdx - 1];
						if (turretModelType != 0) {
							g_loadedModels.byObjectType[objectType] =
								g_loadedModels.byObjectType[turretModelType];
						} else {
							HANGAR_OUTPUT_DEBUG_STRING("Can't find cockpit model\n");
						}
					} else {
						g_loadedModels.byObjectType[objectType] = g_cockpitModel;
					}
				} else if (objectIdx == g_players[g_localPlayer].objectIndex &&
						   g_players[g_localPlayer].viewState.externalCameraActive) {
					savedModel = g_loadedModels.byObjectType[(uint16_t)g_objectTable[objectIdx].objectType];
					g_drawingOwnCraft = 1;
					if (g_exteriorModelLoaded) {
						g_loadedModels.byObjectType[(uint16_t)g_objectTable[objectIdx].objectType] =
							g_exteriorModel;
					}
					viewX = entry->viewX;
					viewY = entry->viewY;
					viewZ = entry->viewZ;
				} else {
					g_cockpitViewActive = 0;
					g_drawingOwnCraft = 0;
					viewX = entry->viewX;
					viewY = entry->viewY;
					viewZ = entry->viewZ;
				}

				g_renderFlags = entry->cullFlags;
				FlightLight_SetupObjectLighting(&g_objectTable[objectIdx]);
				Damage_QueueCraftBillboards(queuedObjectIdx);
				RenderScene_DrawObjectModel(&g_objectTable[objectIdx]);
				g_objectPointLightCount = 0;
				if (savedModel != 0xffffu) {
					g_loadedModels.byObjectType[(uint16_t)g_objectTable[objectIdx].objectType] = savedModel;
				}
				g_cockpitViewActive = 0;
				g_drawingOwnCraft = 0;
				break;
			}
			case GENUS_PlayerProjectile:
			case GENUS_NpcProjectile:
				viewX = entry->viewX;
				viewY = entry->viewY;
				viewZ = entry->viewZ;
				g_renderFlags = entry->cullFlags;
				FVIEW_SetObjectTransform(g_objectTable[objectIdx].roll, g_objectTable[objectIdx].pitch,
										 g_objectTable[objectIdx].yaw, g_objectTable[objectIdx].angleD,
										 &g_objectTable[objectIdx]);
				RenderBillboard_DrawRollAlignedObjectModel(queuedObjectIdx);
				break;
			case GENUS_Debris:
			case GENUS_Explosion:
				viewX = entry->viewX;
				viewY = entry->viewY;
				viewZ = entry->viewZ;
				g_renderFlags = entry->cullFlags;
				FVIEW_SetObjectTransform(g_objectTable[objectIdx].roll, g_objectTable[objectIdx].pitch,
										 g_objectTable[objectIdx].yaw, g_objectTable[objectIdx].angleD,
										 &g_objectTable[objectIdx]);
				SceneBillboard_QueueObjectTextured(objectIdx);
				break;
			default:
				break;
		}
	}

	if (g_useHardware3D) {
		ObjectRecord* objectTable;

		RenderScene_SetProjectionDepthOverride((float)g_launchBaseZ);
		entry = g_renderListHead;
		if (entry != NULL) {
			objectTable = g_objectTable;
			do {
				ObjectRecord* object;
				uint16_t genusId;

				objectIdx = entry->objectIdx & UINT16_MAX;
				object = &objectTable[objectIdx];
				if (object->objectType != OBJ_HangarRoofCrane) {
					uint32_t regionStart;

					regionStart = g_regionMainObjectSlotStart;
					genusId = object->genusId;
					if (objectIdx >= regionStart && objectIdx < g_regionMainObjectSlotEnd &&
						objectIdx != g_launchRefObjIdx) {
						switch (genusId) {
							case GENUS_Fighter:
							case GENUS_Transport:
							case GENUS_Utility:
							case GENUS_Freighter:
							case GENUS_Starship:
							case GENUS_Platform:
							case GENUS_SatelliteBuoy:
							case GENUS_LargeScenery:
							case GENUS_Container:
							case GENUS_PilotDroid:
							case GENUS_WeaponEmplacement: {
								uint16_t savedModel;

								savedModel = 0xffffu;
								g_curCraft = object->mobj->pCraft;
								FVIEW_SetObjectTransform(object->roll, object->pitch, object->yaw,
														 object->angleD, object);
								objectTable = g_objectTable;
								g_cockpitViewActive = 0;
								g_drawingOwnCraft = 0;
								if (objectIdx == g_players[g_localPlayer].objectIndex &&
									!g_players[g_localPlayer].viewState.externalCameraActive &&
									!g_replayViewMode) {
									uint16_t objectType;

									objectType = (uint16_t)objectTable[objectIdx].objectType;
									if (objectType == OBJ_None) {
										break;
									}
									savedModel = g_loadedModels.byObjectType[objectType];
									g_cockpitViewActive = 1;
									g_loadedModels.byObjectType[objectType] = g_cockpitModel;
								}
								viewX = entry->viewX;
								viewY = entry->viewY;
								viewZ = entry->viewZ;
								g_renderFlags = entry->cullFlags;
								RenderScene_DrawObjectModelHardware(&objectTable[objectIdx]);
								objectTable = g_objectTable;
								if (savedModel != 0xffffu) {
									g_loadedModels.byObjectType[(uint16_t)objectTable[objectIdx].objectType] =
										savedModel;
								}
								g_cockpitViewActive = 0;
								g_drawingOwnCraft = 0;
								break;
							}
							default:
								break;
						}
					}
				}
				entry = entry->next;
			} while (entry != NULL);
		}
	}

	g_renderFlags = VIEWPORT_CULL_RIGHT | VIEWPORT_CULL_LEFT | VIEWPORT_CULL_TOP | VIEWPORT_CULL_BOTTOM |
					VIEWPORT_CULL_NEAR;
	GlowMark_ClearPendingRequests();
	if (g_useHardware3D) {
		RenderScene_FlushGeometry();
		EngineGlow_RenderSceneGlows();
	}
	g_drawSceneEffects = 1;
	RenderScene_DrawVisibleFaces();
	g_drawSceneEffects = 0;
	if (!g_useHardware3D) {
		SceneBillboard_RenderQueuedTextured(1);
	}
	if (g_players[g_localPlayer].hudEnabled && !g_hangarAutoCam) {
		Hud_DrawHudTargetInsetIfEnabled(g_localPlayer);
	}
	g_unusedFlightRenderColorByte = g_flightColorEscapeBypassChar;
	Hud_BlitSoftwareHudTextPanes();
	if (g_flightFpsOverlayMode) {
		Hud_DrawFpsOverlay();
	}
}

// FUNCTION: XWA 0x45EC70
void Hangar_UpdateShuttleTrafficCycle(int dtTicks) {
	unsigned int decisionTime;
	int objectIdx;
	int sourceObjIdx;
	CraftData* craft;
	int volume;
	unsigned int outPriority;
	uint16_t meshCount;
	uint16_t meshIdx;

	objectIdx = g_hangarShuttleState.objectIdx;
	sourceObjIdx = objectIdx;
	decisionTime = 4u * (unsigned int)g_gameTime;
	craft = g_objectTable[objectIdx].mobj->pCraft;

	switch (g_hangarShuttleState.moveState) {
		case 0:
			if (decisionTime >= (unsigned int)g_hangarShuttleState.nextDecisionTime &&
				g_hangarPlayerObjIdx != g_hangarShuttleState.objectIdx) {
				g_hangarShuttleState.moveState = 1;
				fsfx_PlaySound(132, sourceObjIdx, (unsigned int)g_localPlayer);
				fsfx_PlaySound(149, sourceObjIdx, (unsigned int)g_localPlayer);
				if (g_launchSeqPhase != 5) {
					fsfx_PlaySound(2641, 0xffffu, (unsigned int)g_localPlayer);
				}
			}
			break;

		case 1: {
			int baseVolume;
			int pan;
			int distance;

			pan = 64;
			g_objectTable[objectIdx].world_z += 2 * dtTicks;
			g_objectTable[objectIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[objectIdx].mobj->moveVectorDirty = 1;
			baseVolume = (uint16_t)fsfx_ComputeSourceVolume(sourceObjIdx, 150u, &outPriority);
			volume = baseVolume;
			if (baseVolume != 0) {
				pan = (uint16_t)fsfx_ComputeSourcePan(sourceObjIdx, (unsigned int*)&volume);
			}
			distance = g_objectTable[objectIdx].world_z - g_launchBaseZ - 64;
			if (distance < 0) {
				distance = 0;
			} else if (distance > 215) {
				distance = 215;
			}
			volume = (uint16_t)((volume * distance) / 215);
			if (volume == 0) {
				volume = 1;
			}
			if ((unsigned int)volume > 127u) {
				volume = 127;
			}
			if (Sound_CountPlayingInstances(g_sfxIds[150]) == 0) {
				Sound_QueueEffect(g_sfxIds[150], 1, 1, 125, volume, pan, -1, sourceObjIdx);
			} else {
				Sound_SetLatestInstancePan(g_sfxIds[150], pan);
				Sound_SetLatestInstanceVolume(g_sfxIds[150], volume);
			}
			if (g_objectTable[objectIdx].world_z > g_launchBaseZ + 353) {
				g_hangarShuttleState.moveState = 2;
				Sound_SetLatestInstanceVolume(g_sfxIds[150], baseVolume);
			}
			break;
		}

		case 2: {
			g_objectTable[objectIdx].yaw = (uint16_t)(g_objectTable[objectIdx].yaw + 50 * dtTicks);
			g_objectTable[objectIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[objectIdx].mobj->moveVectorDirty = 1;
			meshCount = ModelMesh_GetObjectTypeMeshCount(g_objectTable[objectIdx].objectType);
			for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
				MeshType meshType;
				uint8_t meshRotation;

				meshType =
					ModelMesh_GetObjectTypeMeshType((uint16_t)g_objectTable[objectIdx].objectType, meshIdx);
				if (meshType == MESH_Hatch || meshType == MESH_Custom) {
					meshRotation = craft->meshRotation[meshIdx];
					if (meshRotation > 0) {
						meshRotation = (uint8_t)(meshRotation - dtTicks);
						craft->meshRotation[meshIdx] = meshRotation;
						if (meshRotation > 0x60u) {
							craft->meshRotation[meshIdx] = 0;
						}
					}
				}
			}
			if (g_objectTable[objectIdx].yaw < 0x4e20u) {
				g_objectTable[objectIdx].yaw = 0;
				g_hangarShuttleState.moveState = 3;
				fsfx_PlaySound(146, sourceObjIdx, (unsigned int)g_localPlayer);
			}
			break;
		}

		case 3: {
			int pan;
			int distance;
			int targetY;

			g_objectTable[objectIdx].world_y += (int)g_hangarShuttleState.moveSpeed * dtTicks;
			g_hangarShuttleState.moveSpeed = g_hangarShuttleState.moveSpeed - (float)dtTicks * -0.1f;
			craft->throttleSpeed = (uint16_t)(4096 - (int)(g_hangarShuttleState.moveSpeed * -256.0f));
			g_objectTable[objectIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[objectIdx].mobj->moveVectorDirty = 1;
			if (Sound_CountPlayingInstances(g_sfxIds[150]) != 0) {
				pan = 64;
				volume = (uint16_t)fsfx_ComputeSourceVolume(sourceObjIdx, 150u, &outPriority);
				if (volume != 0) {
					pan = (uint16_t)fsfx_ComputeSourcePan(sourceObjIdx, (unsigned int*)&volume);
				}
				distance = g_objectTable[g_launchRefObjIdx].world_y - g_objectTable[objectIdx].world_y + 2400;
				if (distance < 0) {
					distance = 0;
				} else if (distance > 1441) {
					distance = 1441;
				}
				volume = (uint16_t)((volume * distance) / 1441);
				if ((unsigned int)volume > 127u) {
					volume = 127;
				}
				if (volume == 0) {
					Sound_StopOldestInstance(g_sfxIds[150]);
				} else {
					Sound_SetLatestInstanceVolume(g_sfxIds[150], volume);
					Sound_SetLatestInstancePan(g_sfxIds[150], pan);
				}
			}
			targetY = g_objectTable[g_launchRefObjIdx].world_y + 2400;
			if (g_objectTable[objectIdx].world_y > targetY) {
				g_objectTable[objectIdx].world_y = targetY;
				g_hangarShuttleState.moveState = 4;
				fsfx_PlaySound(147, sourceObjIdx, (unsigned int)g_localPlayer);
			}
			break;
		}

		case 4: {
			float angleRadians;

			g_objectTable[objectIdx].yaw = (uint16_t)(g_objectTable[objectIdx].yaw + 50 * dtTicks);
			g_objectTable[objectIdx].roll = (uint16_t)(g_objectTable[objectIdx].roll - 20 * dtTicks);
			g_hangarShuttleState.moveSpeed = g_hangarShuttleState.moveSpeed - (float)dtTicks * -0.1f;
			angleRadians = (float)g_objectTable[objectIdx].yaw * 0.000095873722f;
			g_objectTable[objectIdx].world_y +=
				dtTicks * (int)(cos(angleRadians) * g_hangarShuttleState.moveSpeed);
			g_objectTable[objectIdx].world_x +=
				dtTicks * (int)(sin(angleRadians) * g_hangarShuttleState.moveSpeed);
			craft->throttleSpeed = (uint16_t)(4096 - (int)(g_hangarShuttleState.moveSpeed * -256.0f));
			meshCount = ModelMesh_GetObjectTypeMeshCount(g_objectTable[objectIdx].objectType);
			for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
				uint8_t meshRotation;

				if (ModelMesh_GetObjectTypeMeshType((uint16_t)g_objectTable[objectIdx].objectType, meshIdx) ==
					MESH_RotaryWing) {
					meshRotation = craft->meshRotation[meshIdx];
					if (meshRotation > 0) {
						meshRotation = (uint8_t)(meshRotation - dtTicks);
						craft->meshRotation[meshIdx] = meshRotation;
						if (meshRotation > 0x60u) {
							craft->meshRotation[meshIdx] = 0;
						}
					}
				}
			}
			g_objectTable[objectIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[objectIdx].mobj->moveVectorDirty = 1;
			if (g_objectTable[objectIdx].yaw > 0x8000u && g_objectTable[objectIdx].yaw < 0xf000u) {
				g_hangarShuttleState.moveState = 5;
				g_hangarShuttleState.nextDecisionTime = (int)(decisionTime + (uint16_t)GameRand() % 25000);
				g_objectTable[objectIdx].yaw = (uint16_t)(g_objectTable[objectIdx].yaw + 0x8000u);
				g_objectTable[objectIdx].roll = (uint16_t)(-1 - g_objectTable[objectIdx].roll);
			}
			break;
		}

		case 5:
			if (decisionTime >= (unsigned int)g_hangarShuttleState.nextDecisionTime) {
				g_hangarShuttleState.moveState = 6;
			}
			break;

		case 6: {
			float angleRadians;
			uint8_t dtLow;

			dtLow = (uint8_t)dtTicks;
			g_objectTable[objectIdx].yaw = (uint16_t)(g_objectTable[objectIdx].yaw - 50 * dtTicks);
			g_objectTable[objectIdx].roll = (uint16_t)(g_objectTable[objectIdx].roll - 20 * dtTicks);
			g_hangarShuttleState.moveSpeed = g_hangarShuttleState.moveSpeed - (float)dtTicks * 0.1f;
			angleRadians = (float)g_objectTable[objectIdx].yaw * 0.000095873722f;
			g_objectTable[objectIdx].world_y +=
				dtTicks * (int)(cos(angleRadians) * g_hangarShuttleState.moveSpeed);
			g_objectTable[objectIdx].world_x +=
				dtTicks * (int)(sin(angleRadians) * g_hangarShuttleState.moveSpeed);
			craft->throttleSpeed = (uint16_t)(4096 - (int)(g_hangarShuttleState.moveSpeed * -256.0f));
			if (g_objectTable[objectIdx].yaw < 0xa05au && g_objectTable[objectIdx].yaw > 0x1000u) {
				if (Sound_CountPlayingInstances(g_sfxIds[147]) == 0) {
					fsfx_PlaySound(147, sourceObjIdx, (unsigned int)g_localPlayer);
					if (g_launchSeqPhase != 5) {
						fsfx_PlaySound(2642, 0xffffu, (unsigned int)g_localPlayer);
					}
				}
				{
					meshCount = ModelMesh_GetObjectTypeMeshCount(g_objectTable[objectIdx].objectType);
					for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
						uint8_t meshRotation;

						if (ModelMesh_GetObjectTypeMeshType((uint16_t)g_objectTable[objectIdx].objectType,
															meshIdx) == MESH_RotaryWing) {
							meshRotation = craft->meshRotation[meshIdx];
							if (meshRotation < 0x60u) {
								meshRotation = (uint8_t)(meshRotation + dtLow);
								craft->meshRotation[meshIdx] = meshRotation;
								if (meshRotation > 0x60u) {
									craft->meshRotation[meshIdx] = 0x60u;
								}
							}
						}
					}
				}
			}
			g_objectTable[objectIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[objectIdx].mobj->moveVectorDirty = 1;
			if (g_objectTable[objectIdx].yaw <= 0x8000u && g_objectTable[objectIdx].yaw > 0x1000u) {
				g_objectTable[objectIdx].yaw = 0x8000u;
				g_hangarShuttleState.moveState = 7;
			}
			break;
		}

		case 7: {
			int pan;
			int distance;
			int targetX;

			g_objectTable[objectIdx].world_y -= (int)g_hangarShuttleState.moveSpeed * dtTicks;
			g_hangarShuttleState.moveSpeed = g_hangarShuttleState.moveSpeed - (float)dtTicks * 0.13f;
			targetX = g_objectTable[g_launchRefObjIdx].world_x + 1127;
#ifdef XWA_MODERN
			if (!XwaModernFlightTiming_IsHighRate() || g_modernHangarLegacyCadenceDue) {
#endif
				if (g_objectTable[objectIdx].world_x > targetX) {
					--g_objectTable[objectIdx].world_x;
				} else if (g_objectTable[objectIdx].world_x < targetX) {
					++g_objectTable[objectIdx].world_x;
				}
				if (g_objectTable[objectIdx].roll > 0) {
					--g_objectTable[objectIdx].roll;
				}
#ifdef XWA_MODERN
			}
#endif
			craft->throttleSpeed = (uint16_t)(4096 - (int)(g_hangarShuttleState.moveSpeed * -256.0f));
			pan = 64;
			g_objectTable[objectIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[objectIdx].mobj->moveVectorDirty = 1;
			volume = (uint16_t)fsfx_ComputeSourceVolume(sourceObjIdx, 150u, &outPriority);
			if (volume != 0) {
				pan = (uint16_t)fsfx_ComputeSourcePan(sourceObjIdx, (unsigned int*)&volume);
			}
			distance = g_objectTable[g_launchRefObjIdx].world_y - g_objectTable[objectIdx].world_y + 2400;
			if (distance < 0) {
				distance = 0;
			} else if (distance > 1441) {
				distance = 1441;
			}
			volume = (uint16_t)((volume * distance) / 1441);
			if ((unsigned int)volume > 127u) {
				volume = 127;
			}
			if (Sound_CountPlayingInstances(g_sfxIds[150]) == 0) {
				Sound_QueueEffect(g_sfxIds[150], 1, 1, 125, volume, pan, -1, sourceObjIdx);
			} else {
				Sound_SetLatestInstancePan(g_sfxIds[150], pan);
				Sound_SetLatestInstanceVolume(g_sfxIds[150], volume);
			}
			if (g_hangarShuttleState.moveSpeed <= 0.0f ||
				g_objectTable[objectIdx].world_y <= g_objectTable[g_launchRefObjIdx].world_y + 959) {
				g_objectTable[objectIdx].world_x = g_objectTable[g_launchRefObjIdx].world_x + 1127;
				g_objectTable[objectIdx].world_y = g_objectTable[g_launchRefObjIdx].world_y + 959;
				g_objectTable[objectIdx].roll = 0;
				g_hangarShuttleState.moveSpeed = 0.0f;
				g_hangarShuttleState.moveState = 8;
				fsfx_PlaySound(148, sourceObjIdx, (unsigned int)g_localPlayer);
				fsfx_PlaySound(131, sourceObjIdx, (unsigned int)g_localPlayer);
			}
			break;
		}

		case 8: {
			g_objectTable[objectIdx].yaw = (uint16_t)(g_objectTable[objectIdx].yaw + 50 * dtTicks);
			g_objectTable[objectIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[objectIdx].mobj->moveVectorDirty = 1;
			meshCount = ModelMesh_GetObjectTypeMeshCount(g_objectTable[objectIdx].objectType);
			for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
				MeshType meshType;
				uint8_t meshRotation;

				meshType =
					ModelMesh_GetObjectTypeMeshType((uint16_t)g_objectTable[objectIdx].objectType, meshIdx);
				if (meshType == MESH_Hatch || meshType == MESH_Custom) {
					meshRotation = craft->meshRotation[meshIdx];
					if (meshRotation < 0x60u) {
						meshRotation = (uint8_t)(meshRotation + dtTicks);
						craft->meshRotation[meshIdx] = meshRotation;
						if (meshRotation > 0x60u) {
							craft->meshRotation[meshIdx] = 0x60u;
						}
					}
				}
			}
			if (g_objectTable[objectIdx].yaw >= 0xa880u) {
				g_hangarShuttleState.moveState = 9;
			}
			break;
		}

		case 9: {
			int pan;
			int distance;

			g_objectTable[objectIdx].world_z -= 2 * dtTicks;
			g_objectTable[objectIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[objectIdx].mobj->moveVectorDirty = 1;
			if (Sound_CountPlayingInstances(g_sfxIds[150]) != 0) {
				pan = 64;
				volume = (uint16_t)fsfx_ComputeSourceVolume(sourceObjIdx, 150u, &outPriority);
				if (volume != 0) {
					pan = (uint16_t)fsfx_ComputeSourcePan(sourceObjIdx, (unsigned int*)&volume);
				}
				distance = g_objectTable[objectIdx].world_z - g_launchBaseZ - 32;
				if (distance < 0) {
					distance = 0;
				} else if (distance > 215) {
					distance = 215;
				}
				volume = (uint16_t)((volume * distance) / 215);
				if ((unsigned int)volume > 127u) {
					volume = 127;
				}
				if (volume == 0) {
					Sound_StopOldestInstance(g_sfxIds[150]);
				} else {
					Sound_SetLatestInstanceVolume(g_sfxIds[150], volume);
					Sound_SetLatestInstancePan(g_sfxIds[150], pan);
				}
			}
			if (g_objectTable[objectIdx].world_z <= g_launchBaseZ + 138) {
				g_hangarShuttleState.moveState = 0;
				g_hangarShuttleState.nextDecisionTime =
					(int)((uint16_t)GameRand() % 20000 + decisionTime + 25000);
				Sound_StopOldestInstance(g_sfxIds[150]);
				if (g_launchSeqPhase == 9) {
					if (g_missionFlightRuntimeState.teamGoalStatus[(uint16_t)g_players[g_localPlayer]
																	   .playerIff][TEAM_GOAL_PRIMARY] == 1) {
						g_flightExitRequest = 1;
					} else {
						Hangar_SetCameraShot(0);
					}
				}
			}
			break;
		}

		case 10: {
			int* pWorldY;
			int pan;
			int distance;

			pWorldY = &g_objectTable[objectIdx].world_y;
			outPriority = (unsigned int)(*pWorldY - g_objectTable[g_launchRefObjIdx].world_y);
			g_hangarShuttleState.moveSpeed = (float)(((double)(int)outPriority - -959.0) * 0.0049999999);
			*pWorldY -= (int)g_hangarShuttleState.moveSpeed * dtTicks;
			craft->throttleSpeed = (uint16_t)(4096 - (int)(g_hangarShuttleState.moveSpeed * -256.0f));
			if (Sound_CountPlayingInstances(g_sfxIds[147]) == 0) {
				fsfx_PlaySound(147, sourceObjIdx, (unsigned int)g_localPlayer);
			}
			meshCount = ModelMesh_GetObjectTypeMeshCount(g_objectTable[objectIdx].objectType);
			for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
				uint8_t meshRotation;

				if (ModelMesh_GetObjectTypeMeshType((uint16_t)g_objectTable[objectIdx].objectType, meshIdx) ==
					MESH_RotaryWing) {
					meshRotation = craft->meshRotation[meshIdx];
					if (meshRotation < 0x60u) {
						meshRotation = (uint8_t)(meshRotation + dtTicks);
						craft->meshRotation[meshIdx] = meshRotation;
						if (meshRotation > 0x60u) {
							craft->meshRotation[meshIdx] = 0x60u;
						}
					}
				}
			}
			pan = 64;
			g_objectTable[objectIdx].mobj->orientMatrixDirty = 1;
			g_objectTable[objectIdx].mobj->moveVectorDirty = 1;
			volume = (uint16_t)fsfx_ComputeSourceVolume(sourceObjIdx, 150u, &outPriority);
			if (volume != 0) {
				pan = (uint16_t)fsfx_ComputeSourcePan(sourceObjIdx, (unsigned int*)&volume);
			}
			distance = g_objectTable[g_launchRefObjIdx].world_y - g_objectTable[objectIdx].world_y + 2400;
			if (distance < 0) {
				distance = 0;
			} else if (distance > 1441) {
				distance = 1441;
			}
			volume = (uint16_t)((volume * distance) / 1441);
			if ((unsigned int)volume > 127u) {
				volume = 127;
			}
			if (Sound_CountPlayingInstances(g_sfxIds[150]) == 0) {
				Sound_QueueEffect(g_sfxIds[150], 1, 1, 125, volume, pan, -1, sourceObjIdx);
			} else {
				Sound_SetLatestInstancePan(g_sfxIds[150], pan);
				Sound_SetLatestInstanceVolume(g_sfxIds[150], volume);
			}
			if (g_hangarShuttleState.moveSpeed <= 0.0f ||
				g_objectTable[objectIdx].world_y <= g_objectTable[g_launchRefObjIdx].world_y + 959) {
				g_objectTable[objectIdx].world_x = g_objectTable[g_launchRefObjIdx].world_x + 1127;
				g_objectTable[objectIdx].world_y = g_objectTable[g_launchRefObjIdx].world_y + 959;
				g_objectTable[objectIdx].roll = 0;
				g_hangarShuttleState.moveSpeed = 0.0f;
				g_hangarShuttleState.moveState = 8;
				fsfx_PlaySound(148, sourceObjIdx, (unsigned int)g_localPlayer);
				fsfx_PlaySound(131, sourceObjIdx, (unsigned int)g_localPlayer);
			}
			break;
		}

		default:
			break;
	}

	if ((decisionTime & 0x400u) != 0) {
		g_unusedHangarShuttleMeshFlagLatch = 1;
	}
}

#ifdef XWA_MODERN
static int Hangar_RunOptionsModal(void) {
	int exitToFrontend;

	if (!g_hangarOptionsModalPending) {
		g_inputTimestamp += Time_GetFrameDelta();
		Sound_StopAllInstances();
		Music_PauseIfInitialized();
		g_hangarOptionsModalOldConfig = g_gameConfig;
		g_hangarOptionsModalPending = 1;
	}

	exitToFrontend = FlightDisplay_RunRestrictedOptionsModal();
	if (FlightDisplay_IsFrontendModalActive()) {
		return 1;
	}

	g_hangarOptionsModalPending = 0;
	Flight_ApplyConfigToRuntime(&g_hangarOptionsModalOldConfig, &g_gameConfig);
	Music_ResumeIfInitialized();
	Time_GetFrameDelta();
	sub_4D4640();
	if (exitToFrontend) {
		g_flightReturnToFrontendRequested = 1;
		g_flightExitRequest = 1;
		Player_EndFlightParticipation(g_localPlayer);
		if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART &&
			g_pilotData.numHumanPlayersLastMission == 1) {
			int playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
			if (g_missionFlightRuntimeState.teamGoalStatus[playerIff][TEAM_GOAL_PRIMARY] != 1) {
				g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][playerIff] -= 2000;
			}
		}
	}
	if (!g_useHardware3D) {
		Hud_RedrawSoftwareHudFrame();
	}
	if (g_filmRecording == 2) {
		Film_WriteBytesBuffered(&g_flightCollisionsEnabled, 4);
		if (g_pilotData.missionDirectoryId != 3) {
			Film_WriteBytesBuffered(
				&g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status1, 1);
			Film_WriteBytesBuffered(
				&g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status1, 1);
		}
		Film_WriteBytesBuffered(&g_debrisDensityLevel, 4);
	}
	return 0;
}

int Hangar_ContinueOptionsModal(void) {
	if (!g_hangarOptionsModalPending) {
		return 0;
	}

	(void)Hangar_RunOptionsModal();
	return 1;
}
#endif

// FUNCTION: XWA 0x45C680
// Per-frame hangar input handler. Reads/records one key, drives
// launch/exit/camera-shot shortcuts, navigates and commits the hangar loadout
// menus (craft / warhead / beam / countermeasure), applies the selected
// options, and updates the look/padlock input timestamps from the mouse.
void Hangar_HandleInput(void) {
	int key = 0;
	int level = 0;   // active menu level during navigation
	int lastRow = 0; // last selectable row in the active level
#ifndef XWA_MODERN
	GameConfig oldConfig;
#endif

#ifdef XWA_MODERN
	if (g_hangarOptionsModalPending || FlightDisplay_IsFrontendModalActive()) {
		if (Hangar_RunOptionsModal()) {
			return;
		}
		goto end;
	}
#endif

	// --- Read (and film-record/replay) one key ---
	if (!g_filmPlaybackMode) {
		if (FlightInput_HasKeyReady())
			key = FlightInput_GetNextKey();
		if (g_filmRecording == 2)
			Film_WriteBytesBuffered(&key, 2);
	} else if (g_filmPlaybackMode == 2) {
		Film_ReadBytes(&key, 2);
	}
after_key:
	if ((uint16_t)key)
		g_hangarEntryTime4x = 4 * g_gameTime;

	// Once launch is committed only a small set of keys stays active.
	if (g_launchTriggered) {
		switch ((int16_t)key) {
			case 27:
			case 32:
			case 47:
			case 48:
			case 49:
			case 50:
			case 51:
			case 52:
			case 53:
			case 54:
			case 55:
			case 56:
			case 57:
			case 128:
			case 152:
			case 232:
				break;
			default:
				key = 0;
				break;
		}
	}

	if ((int16_t)key <= 27) {
		if ((int16_t)key == 27) {
			goto handle_escape;
		}
		if ((int16_t)key != 13)
			goto end;

		// Enter: commit the current camera/menu/launch action.
		if (g_hangarAutoCam)
			goto label177;
		{
			int playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
			if (g_hangarMissionResolved || g_hangarPlayerObjIdx == g_hangarShuttleState.objectIdx ||
				g_missionFlightRuntimeState.teamGoalStatus[playerIff][TEAM_GOAL_PRIMARY] == 1 ||
				g_missionFlightRuntimeState.teamGoalStatus[playerIff][TEAM_GOAL_SECONDARY] == 1) {
				if (g_hangarMenuCursor[0]) {
					if (g_hangarMenuCursor[0] == 2)
						g_flightExitRequest = 2;
				} else {
					g_flightExitRequest = 1;
				}
				goto end;
			}
		}
		if (g_hangarMenuLevel) {
			// Apply the highlighted level-1 loadout option.
			int objectIndex = g_players[g_localPlayer].objectIndex;
			int placementObjIdx = objectIndex;
			int objType = g_objectTable[objectIndex].objectType;
			int modelIndex = (uint16_t)GetModelIndexFromType((ObjectTypeId)objType);
			uint16_t boundFG = g_players[g_localPlayer].boundFlightGroupIdx;
			int playerObjIdx = g_players[g_localPlayer].objectIndex;
			if (g_provingGroundsModeActive) {
				placementObjIdx = playerObjIdx;
				GetModelIndexFromType(g_objectTable[playerObjIdx].objectType);
				if (g_hangarMenuCursor[0] == 5) {
					Hangar_SwitchPlayerCraft(g_hangarCraftList[g_hangarMenuCursor[1]]);
					g_loadingModel = 1;
					FeDiskIo_LoadCockpitModel();
					FeDiskIo_LoadExteriorModel();
					g_loadingModel = 0;
					Math_SetFpuSinglePrecisionMode();
					FlightObject_InitMeshAnimationDefaults(g_hangarPlayerObjIdx);
					goto finalize_new_craft;
				}
			} else {
				CraftData* pCraft = g_objectTable[playerObjIdx].mobj->pCraft;
				g_curCraft = pCraft;
				switch (g_hangarMenuCursor[0]) {
					case 2:
					case 4: {
						int slot = (g_hangarMenuCursor[0] != 4);
						uint16_t oldType = pCraft->warheadSlotTypeIds[slot];
						pCraft->warheadLauncherCount = 0;
						g_curCraft->warheadSlotTypeIds[slot] =
							g_warheadTypeIds[g_hangarWarheadList[g_hangarMenuCursor[1]]];
						g_curCraft->warheadLauncherFlags[slot] = 1;
						g_curCraft->warheadLauncherCooldownTicks[slot] = 0;
						if (oldType != g_curCraft->warheadSlotTypeIds[slot]) {
							if (g_curCraft->warheadSlotTypeIds[slot])
								fsfx_PlaySound(2630, 0xFFFFu, g_localPlayer);
							else
								fsfx_PlaySound(2633, 0xFFFFu, g_localPlayer);
						}
						if (g_curCraft->warheadSlotTypeIds[slot]) {
							int first;
							int last;
							int wslot;

							++g_curCraft->warheadLauncherCount;
							first = g_modelDefs[modelIndex].warheadLauncherFirstSlot[slot];
							last = g_modelDefs[modelIndex].warheadLauncherLastSlot[slot];
							for (wslot = first; wslot <= last; ++wslot) {
								uint8_t value;
								uint16_t ammoIdx;
								uint8_t count;
								uint8_t status1;
								uint8_t status2;

								g_curCraft->warheadData[wslot].projectileTypeId =
									g_curCraft->warheadSlotTypeIds[slot];
								g_curCraft->warheadData[wslot].weaponType = 3;
								g_curCraft->warheadData[wslot].laserCharge = 127;
								g_curCraft->warheadData[wslot].turretTargetObjIdx = -1;
								value = g_modelDefs[modelIndex].warheadLauncherValue[slot];
								ammoIdx = g_hangarWarheadList[g_hangarMenuCursor[1]];
								if (slot && modelIndex == (uint16_t)GetModelIndexFromType(OBJ_MissileBoat))
									ammoIdx = 5;
								count = MATH2_fraction(value, g_warheadAmmoCounts[ammoIdx]);
								if (!count)
									count = 1;
								status1 = g_missionFlightGroups[boundFG].fg.status1;
								status2 = g_missionFlightGroups[boundFG].fg.status2;
								if (status1 == 1 || status2 == 1)
									count *= 2;
								else if (status1 == 2 || status2 == 2)
									count >>= 1;
								if (!count)
									count = 1;
								if (count > 9 && (uint16_t)objType != 12)
									count = 9;
								g_curCraft->warheadData[wslot].count = count;
							}
						}
						g_curCraft->warheadLockTicks = 0;
						if (g_curCraft->warheadLauncherCount) {
							if (!(g_curCraft->systemFlags & 8)) {
								g_curCraft->systemFlags |= 8;
								g_curCraft->workingSubsystems |= 8;
							}
						} else {
							g_curCraft->systemFlags ^= 8;
						}
						break;
					}
					case 3: {
						int16_t newCraft = g_hangarCraftList[g_hangarMenuCursor[1]];
						if (newCraft != g_objectTable[g_hangarPlayerObjIdx].objectType) {
							Hangar_SwitchPlayerCraft(newCraft);
							g_loadingModel = 1;
							g_curCraft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
							FeDiskIo_LoadCockpitModel();
							FeDiskIo_LoadExteriorModel();
							g_loadingModel = 0;
							Math_SetFpuSinglePrecisionMode();
							FlightObject_InitMeshAnimationDefaults(g_hangarPlayerObjIdx);
							goto finalize_new_craft;
						}
						break;
					}
					case 5: {
						uint8_t oldBeam = pCraft->beamTypeId;
						pCraft->beamTypeId = (uint8_t)g_hangarBeamList[g_hangarMenuCursor[1]];
						if (g_curCraft->beamTypeId != oldBeam) {
							if (g_curCraft->beamTypeId)
								fsfx_PlaySound(2636, 0xFFFFu, g_localPlayer);
							else
								fsfx_PlaySound(2637, 0xFFFFu, g_localPlayer);
						}
						g_curCraft->beamLevel = 2;
						g_curCraft->beamPresent = 9999;
						if (g_curCraft->beamTypeId) {
							g_curCraft->systemFlags |= 0x100;
							g_curCraft->installedHudFeatureMask |= 0x1000;
							g_curCraft->installedHudFeatureMask |= 0x10;
						} else {
							g_curCraft->beamPresent = 0;
							g_curCraft->systemFlags ^= 0x100;
							g_curCraft->installedHudFeatureMask ^= 0x1000;
							g_curCraft->installedHudFeatureMask ^= 0x10;
						}
						g_curCraft->workingSubsystems = g_curCraft->systemFlags;
						g_curCraft->activeHudFeatureMask = g_curCraft->installedHudFeatureMask;
						break;
					}
					case 6: {
						uint8_t oldCm = pCraft->cmTypeId;
						pCraft->cmTypeId = (uint8_t)g_hangarCountermeasureTypeList[g_hangarMenuCursor[1]];
						if (g_curCraft->cmTypeId != oldCm) {
							if (g_curCraft->cmTypeId)
								fsfx_PlaySound(2634, 0xFFFFu, g_localPlayer);
							else
								fsfx_PlaySound(2635, 0xFFFFu, g_localPlayer);
						}
						if (g_curCraft->cmTypeId) {
							g_curCraft->systemFlags |= 2;
							g_curCraft->cmAmmoCount = g_modelDefs[modelIndex].countermeasureCount;
							if (g_curCraft->cmTypeId == 2)
								g_curCraft->cmAmmoCount =
									(uint8_t)MATH2_fraction(g_curCraft->cmAmmoCount, 0xAAACu);
						} else {
							g_curCraft->cmAmmoCount = 0;
							g_curCraft->systemFlags ^= 2;
						}
						g_curCraft->chaffActiveTimer = 0;
						g_curCraft->cmFireCooldownTimer = 0;
						g_curCraft->workingSubsystems = g_curCraft->systemFlags;
						g_curCraft->activeHudFeatureMask = g_curCraft->installedHudFeatureMask;
						break;
					}
					default:
						break;
				}
			}
			goto label92;

		finalize_new_craft: {
			int sizeZ;

			if (g_objectTable[g_hangarPlayerObjIdx].objectType == OBJ_BWing)
				sizeZ = 50;
			else
				sizeZ = ModelBounds_GetSizeZ((uint16_t)g_objectTable[g_hangarPlayerObjIdx].objectType) / 2;
			g_objectTable[placementObjIdx].world_z = g_launchBaseZ + sizeZ;
			g_yardFinishPlacementResultCode = 0;
			if (GameRand2() & 1)
				fsfx_PlaySound(2627, 0xFFFFu, g_localPlayer);
			else
				fsfx_PlaySound(2628, 0xFFFFu, g_localPlayer);
			g_pendingHudMessageVoiceSfxId = 0;
			msg_addMessagePtr(0, g_strHangarMiscStrings[HANGAR_MISC_NEW_CRAFT_IS_READY]);
			msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
		}
			goto label92;
		}
		if (g_hangarMenuCursor[0]) {
			if (g_hangarMenuCursor[0] == 1) {
				g_launchAnimDone = 1;
				g_flightExitRequest = 1;
			} else if (!g_provingGroundsModeActive || g_hangarMenuCursor[0] == 3 ||
					   g_hangarMenuCursor[0] == 5) {
				goto label178;
			}
		} else {
			g_launchTriggered = 1;
		}
		goto end;

	handle_escape:
		// Escape: single-player opens the in-flight options modal.
		if (g_flightPlayerCount == 1) {
			if (g_filmPlaybackMode) {
				if (g_filmPlaybackMode == 2) {
					Film_ReadBytes(&g_flightCollisionsEnabled, 4);
					if (g_pilotData.missionDirectoryId != 3) {
						Film_ReadBytes(
							&g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status1,
							1);
						Film_ReadBytes(
							&g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status1,
							1);
					}
					Film_ReadBytes(&g_debrisDensityLevel, 4);
				}
			} else {
#ifdef XWA_MODERN
				if (Hangar_RunOptionsModal()) {
					return;
				}
#else
				int exitToFrontend;

				g_inputTimestamp += Time_GetFrameDelta();
				Sound_StopAllInstances();
				Music_PauseIfInitialized();
				oldConfig = g_gameConfig;
				exitToFrontend = FlightDisplay_RunRestrictedOptionsModal();
				Flight_ApplyConfigToRuntime(&oldConfig, &g_gameConfig);
				Music_ResumeIfInitialized();
				Time_GetFrameDelta();
				key = 0;
				sub_4D4640();
				if (exitToFrontend) {
					g_flightReturnToFrontendRequested = 1;
					g_flightExitRequest = 1;
					Player_EndFlightParticipation(g_localPlayer);
					if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART &&
						g_pilotData.numHumanPlayersLastMission == 1) {
						int playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
						if (g_missionFlightRuntimeState.teamGoalStatus[playerIff][TEAM_GOAL_PRIMARY] != 1) {
							g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][playerIff] -=
								2000;
						}
					}
				}
				if (!g_useHardware3D) {
					Hud_RedrawSoftwareHudFrame();
				}
				if (g_filmRecording == 2) {
					Film_WriteBytesBuffered(&g_flightCollisionsEnabled, 4);
					if (g_pilotData.missionDirectoryId != 3) {
						Film_WriteBytesBuffered(
							&g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status1,
							1);
						Film_WriteBytesBuffered(
							&g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status1,
							1);
					}
					Film_WriteBytesBuffered(&g_debrisDensityLevel, 4);
				}
#endif
			}
		}
		goto end;
	}

	if ((int16_t)key > 47) {
		switch ((int16_t)key) {
			case 48:
			case 49:
			case 50:
			case 51:
			case 52:
			case 53:
			case 54:
			case 55:
			case 56:
			case 57:
				g_hangarReadyElapsedMs = 0;
				Hangar_SetCameraShot(key - 48);
				goto end;
			case 81:
			case 113:
				g_flightExitRequest = 1;
				goto end;
			case 164:
				if (g_hangarAutoCam)
					goto label177;
				goto label92;
			case 165:
				if (g_hangarAutoCam)
					goto label177;
				goto label178;
			case 166:
				if (g_hangarAutoCam)
					goto label177;
				level = g_hangarMenuLevel;
				{
					int* cur = &g_hangarMenuCursor[level];
					int cursorVal = *cur;
					int nextRow;
					if (cursorVal <= 0)
						cursorVal = g_hangarMenuItemCount[level];
					*cur = cursorVal - 1;
					do {
						int rowIdx = *cur;
						if (g_hangarMenuItemLabels[level][rowIdx][0] &&
							!g_hangarMenuItemDisabled[level][rowIdx])
							break;
						nextRow = rowIdx - 1;
						*cur = nextRow;
					} while (nextRow > 0);
				}
				if (!level)
					goto label144;
				if (!g_provingGroundsModeActive || g_hangarMenuCursor[0] != 3)
					goto label169;
				g_yardChallengeMode = (uint8_t)g_hangarMenuCursor[level];
				if (level != 1)
					goto label168;
				{
					int i;

					strcpy(g_hangarMenuColTitle[0], g_strHangarMenuTitles[HANGAR_MENU_TITLE_INSTRUCTIONS]);
					for (i = 0; i < 8; ++i) {
						const char* desc = g_strProvingGroundDescs[8 * g_yardChallengeMode + i];
						memcpy(g_hangarMenuItemLabels[0][i], desc, strlen(desc) + 1);
					}
					g_hangarMenuItemCount[0] = 8;
				}
				g_yardFinishPlacementResultCode = 0;
				goto label169;
			case 167:
				if (g_hangarAutoCam)
					goto label177;
				level = g_hangarMenuLevel;
				lastRow = g_hangarMenuItemCount[g_hangarMenuLevel] - 1;
				{
					int* cur = &g_hangarMenuCursor[g_hangarMenuLevel];
					if (*cur >= lastRow)
						*cur = 0;
					else
						++*cur;
				}
				goto label_navdown;
			case 178:
			case 179:
			case 180:
			case 181:
			case 182:
			case 183:
			case 184:
			case 185:
			case 186:
			case 187:
				g_hangarReadyElapsedMs = 0;
				Hangar_SetCameraShot(key - 178);
				goto end;
			case 232:
				if (!g_filmPlaybackMode) {
					fsfx_PlaySound(68, 0xFFFFu, g_localPlayer);
					FlightScreenshot_Capture();
				}
				goto end;
			case 300:
				Hud_SetHudEnabled(g_localPlayer, g_players[g_localPlayer].hudEnabled == 0);
				goto end;
			default:
				goto end;
		}

	label_navdown:
		while (1) {
			int* cur = &g_hangarMenuCursor[level];
			int rowIdx = *cur;
			int nextRow;
			if (g_hangarMenuItemLabels[level][rowIdx][0] && !g_hangarMenuItemDisabled[level][rowIdx])
				break;
			nextRow = rowIdx + 1;
			*cur = nextRow;
			if (nextRow > lastRow) {
				*cur = 0;
				break;
			}
		}
		if (level) {
			if (g_provingGroundsModeActive && g_hangarMenuCursor[0] == 3) {
				g_yardChallengeMode = (uint8_t)g_hangarMenuCursor[level];
				if (level == 1) {
					int i;

					strcpy(g_hangarMenuColTitle[0], g_strHangarMenuTitles[HANGAR_MENU_TITLE_INSTRUCTIONS]);
					for (i = 0; i < 8; ++i) {
						const char* desc = g_strProvingGroundDescs[8 * g_yardChallengeMode + i];
						memcpy(g_hangarMenuItemLabels[0][i], desc, strlen(desc) + 1);
					}
					g_hangarMenuItemCount[0] = 8;
				}
				goto label168;
			}
			goto label169;
		}
	label144:
		Hangar_BuildMenu(0);
		level = g_hangarMenuLevel;
		goto label169;
	label168:
		g_yardFinishPlacementResultCode = 0;
	label169: {
		int cursorRow = g_hangarMenuCursor[level];
		if (cursorRow < g_hangarMenuScroll[level])
			g_hangarMenuScroll[level] = cursorRow;
		if (cursorRow > g_hangarMenuScroll[level] + 7)
			g_hangarMenuScroll[level] = cursorRow - 7;
		goto end;
	}
	} else {
		if ((int16_t)key == 47) {
			Hangar_SetCameraShot(GameRand() % 9 + 1);
			goto end;
		}
		if ((int16_t)key == 32) {
			if (g_launchTriggered) {
				g_launchAnimDone = 1;
				goto end;
			}
			if (g_hangarPlayerObjIdx != g_hangarShuttleState.objectIdx) {
				int playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
				if (g_hangarMissionResolved ||
					g_missionFlightRuntimeState.teamGoalStatus[playerIff][TEAM_GOAL_PRIMARY] == 1 ||
					g_missionFlightRuntimeState.teamGoalStatus[playerIff][TEAM_GOAL_SECONDARY] == 1) {
					g_flightExitRequest = 1;
				} else {
					g_launchTriggered = 1;
					g_flightReturnToMissionSetupRequested = 0;
				}
				goto end;
			}
			if (g_hangarShuttleState.moveState == 10)
				goto end;
			goto label177;
		}
		goto end;
	}

label92:
	g_hangarMenuLevel = 0;
	Hud_ForceHudRefresh(g_localPlayer, 1);
	Hangar_BuildMenu(0);
	goto end;
label177:
	Hangar_SetCameraShot(0);
	goto end;
label178:
	g_hangarMenuLevel = 1;
	Hud_ForceHudRefresh(g_localPlayer, 2);
	goto end;

end:
	// Mouse-driven cockpit look (disabled during film record/playback).
	if (!g_filmRecording && !g_filmPlaybackMode) {
		uint8_t rightButton;

		DInput_PollMouseState();
		if (g_dinputMouseState.lX || g_dinputMouseState.lY) {
			g_hangarEntryTime4x = 4 * g_gameTime;
			g_players[g_localPlayer].lookYawOffset += 15 * (int16_t)g_dinputMouseState.lX;
			g_players[g_localPlayer].lookPitchOffset += -15 * (int16_t)g_dinputMouseState.lY;
		}
		rightButton = g_dinputMouseState.rgbButtons[1];
		if (g_dinputMouseState.rgbButtons[0]) {
			g_hangarEntryTime4x = 4 * g_gameTime;
			g_players[g_localPlayer].lookYawOffset = 0;
			g_players[g_localPlayer].lookPitchOffset = 0;
		}
		if (rightButton) {
			if (g_hangarAutoCam)
				Hangar_SetCameraShot(0);
			g_hardpointOriginOffset[0] = 0;
			g_hardpointOriginOffset[1] = 0;
			g_hardpointOriginOffset[2] = 0;
		}
	}
}

// FUNCTION: XWA 0x45FC40
void Hangar_SetCameraShot(int shotIndex) {
	int worldX;
	int worldY;
	int worldZ;
	int noObject;
	ObjectRecord* playerObj;
	ObjectRecord* launchRef;

	noObject = 0xffff;
	g_hangarAutoCam = 1;
	g_hangarCamFocusObj = noObject;

	if (g_hangarBackdropModelType == 308) {
		switch ((uint16_t)shotIndex) {
			case 0:
				goto cockpit_shot;
			case 1:
				worldX = g_objectTable[g_launchRefObjIdx].world_x + 1130;
				worldY = g_objectTable[g_launchRefObjIdx].world_y - 2320;
				worldZ = g_objectTable[g_launchRefObjIdx].world_z - 300;
				break;
			case 2:
				worldX = g_objectTable[g_launchRefObjIdx].world_x + 1240;
				worldY = g_objectTable[g_launchRefObjIdx].world_y - 330;
				worldZ = g_objectTable[g_launchRefObjIdx].world_z - 700;
				break;
			case 4:
				g_hangarCamFocusObj = g_hangarSceneObjects[0].objectIdx;
				worldX = g_objectTable[g_hangarSceneObjects[0].objectIdx].world_x;
				worldY = g_objectTable[g_hangarSceneObjects[0].objectIdx].world_y;
				worldZ = g_objectTable[g_hangarSceneObjects[0].objectIdx].world_z + 30;
				break;
			case 5:
				g_hangarCamFocusObj = g_hangarSceneObjects[1].objectIdx;
				worldX = g_objectTable[g_hangarSceneObjects[1].objectIdx].world_x;
				worldY = g_objectTable[g_hangarSceneObjects[1].objectIdx].world_y;
				worldZ = g_objectTable[g_hangarSceneObjects[1].objectIdx].world_z + 30;
				break;
			case 6:
				worldX = g_objectTable[g_launchRefObjIdx].world_x - 1200;
				worldY = g_objectTable[g_launchRefObjIdx].world_y - 1530;
				worldZ = g_objectTable[g_launchRefObjIdx].world_z - 850;
				break;
			case 7: {
				ObjectRecord* objectTable;
				int playerObjectType;

				if (g_hangarPlayerObjIdx == noObject || g_launchSeqPhase) {
					goto hangar_main_wide_shot;
				}
				objectTable = g_objectTable;
				playerObj = &objectTable[g_hangarPlayerObjIdx];
				playerObjectType = playerObj->objectType;
				switch (playerObjectType) {
					case OBJ_CorellianTransport2:
					case OBJ_MilleniumFalcon2:
						worldX = g_objectTable[g_launchRefObjIdx].world_x + 470;
						worldY = g_objectTable[g_launchRefObjIdx].world_y - 500;
						worldZ = g_objectTable[g_launchRefObjIdx].world_z - 710;
						break;
					case OBJ_FamilyTransport:
						worldX = g_objectTable[g_launchRefObjIdx].world_x + 80;
						worldY = g_objectTable[g_launchRefObjIdx].world_y + 60;
						worldZ = g_objectTable[g_launchRefObjIdx].world_z - 590;
						break;
					case OBJ_BWing:
						worldX = g_objectTable[g_launchRefObjIdx].world_x + 470;
						worldY = g_objectTable[g_launchRefObjIdx].world_y - 650;
						worldZ = g_objectTable[g_launchRefObjIdx].world_z - 770;
						break;
					case OBJ_Z95:
						worldX = g_objectTable[g_launchRefObjIdx].world_x + 150;
						worldY = g_objectTable[g_launchRefObjIdx].world_y - 492;
						worldZ = g_objectTable[g_launchRefObjIdx].world_z - 817;
						break;
					case OBJ_YWing:
						worldX = g_objectTable[g_launchRefObjIdx].world_x - 280;
						worldY = g_objectTable[g_launchRefObjIdx].world_y - 135;
						worldZ = g_objectTable[g_launchRefObjIdx].world_z - 741;
						break;
					default:
						if (playerObjectType == OBJ_Shuttle) {
							worldX = g_objectTable[g_launchRefObjIdx].world_x - 280;
							worldY = g_objectTable[g_launchRefObjIdx].world_y - 135;
							worldZ = g_objectTable[g_launchRefObjIdx].world_z - 741;
						} else {
							launchRef = &g_objectTable[g_launchRefObjIdx];
							worldX = playerObj->world_x - 90;
							worldY = ModelBounds_GetSizeY(playerObjectType) / 2 + launchRef->world_y + 45;
							worldZ =
								ModelBounds_GetSizeZ(launchRef->objectType) / 2 + playerObj->world_z + 80;
						}
						break;
				}
			} break;
			case 3:
			hangar_main_wide_shot:
				worldX = g_objectTable[g_launchRefObjIdx].world_x - 1120;
				worldY = g_objectTable[g_launchRefObjIdx].world_y + 1360;
				worldZ = g_objectTable[g_launchRefObjIdx].world_z - 790;
				break;
			case 8:
				g_hangarCamFocusObj = g_hangarRoofCraneState.objectIdx;
				worldX = g_objectTable[g_hangarRoofCraneState.objectIdx].world_x;
				worldY = g_objectTable[g_launchRefObjIdx].world_y + 1440;
				worldZ = g_objectTable[g_launchRefObjIdx].world_z - 290;
				break;
			case 9:
				worldX = g_objectTable[g_launchRefObjIdx].world_x + 1070;
				worldY = g_objectTable[g_launchRefObjIdx].world_y + 4640;
				worldZ = g_objectTable[g_launchRefObjIdx].world_z - 130;
				break;
			case 10:
				worldX = g_objectTable[g_launchRefObjIdx].world_x + 481;
				worldY = g_objectTable[g_launchRefObjIdx].world_y + 668;
				worldZ = g_objectTable[g_launchRefObjIdx].world_z - 562;
				break;
			default:
				worldX = shotIndex;
				worldY = shotIndex;
				worldZ = shotIndex;
				break;
		}
	} else {
		switch ((uint16_t)shotIndex) {
			case 0:
			cockpit_shot:
				g_hangarCamFocusObj = noObject;
				g_hangarAutoCam = 0;
				worldX = g_objectTable[g_hangarPlayerObjIdx].world_x;
				worldY = g_objectTable[g_hangarPlayerObjIdx].world_y;
				worldZ = g_objectTable[g_hangarPlayerObjIdx].world_z;
				break;
			case 1:
				worldX = g_objectTable[g_launchRefObjIdx].world_x + 780;
				worldY = g_objectTable[g_launchRefObjIdx].world_y - 6471;
				worldZ = g_objectTable[g_launchRefObjIdx].world_z - 4977;
				break;
			case 2:
				worldX = g_objectTable[g_launchRefObjIdx].world_x - 1970;
				worldY = g_objectTable[g_launchRefObjIdx].world_y - 8810;
				worldZ = g_objectTable[g_launchRefObjIdx].world_z - 4707;
				break;
			case 3:
				worldX = g_objectTable[g_launchRefObjIdx].world_x + 2510;
				worldY = g_objectTable[g_launchRefObjIdx].world_y - 5391;
				worldZ = g_objectTable[g_launchRefObjIdx].world_z - 5067;
				break;
			case 4:
				g_hangarCamFocusObj = g_hangarSceneObjects[0].objectIdx;
				worldX = g_objectTable[g_hangarSceneObjects[0].objectIdx].world_x;
				worldY = g_objectTable[g_hangarSceneObjects[0].objectIdx].world_y;
				worldZ = g_objectTable[g_hangarSceneObjects[0].objectIdx].world_z - 10;
				break;
			case 5:
				playerObj = &g_objectTable[g_hangarPlayerObjIdx];
				{
					int objectType;

					objectType = playerObj->objectType;
					if ((uint16_t)objectType == OBJ_CorellianTransport2 ||
						(uint16_t)objectType == OBJ_MilleniumFalcon2) {
						worldX = g_objectTable[g_launchRefObjIdx].world_x + 610;
						worldY = g_objectTable[g_launchRefObjIdx].world_y - 6480;
						worldZ = g_objectTable[g_launchRefObjIdx].world_z - 5000;
					} else if ((uint16_t)objectType != OBJ_FamilyTransport) {
						worldX = playerObj->world_x - 90;
						worldY = ModelBounds_GetSizeY(objectType) / 2 + playerObj->world_y + 45;
						worldZ = ModelBounds_GetSizeZ(playerObj->objectType) / 2 +
								 g_objectTable[g_hangarPlayerObjIdx].world_z + 80;
					} else {
						worldX = g_objectTable[g_launchRefObjIdx].world_x + 400;
						worldY = g_objectTable[g_launchRefObjIdx].world_y - 5420;
						worldZ = g_objectTable[g_launchRefObjIdx].world_z - 4870;
					}
				}
				break;
			case 6:
				worldX = g_objectTable[g_launchRefObjIdx].world_x + 1740;
				worldY = g_objectTable[g_launchRefObjIdx].world_y - 8461;
				worldZ = g_objectTable[g_launchRefObjIdx].world_z - 5047;
				break;
			case 7:
				worldX = g_objectTable[g_launchRefObjIdx].world_x + 3180;
				worldY = g_objectTable[g_launchRefObjIdx].world_y + 2629;
				worldZ = g_objectTable[g_launchRefObjIdx].world_z - 3777;
				break;
			case 8:
				worldX = g_objectTable[g_launchRefObjIdx].world_x + 8242;
				worldY = g_objectTable[g_launchRefObjIdx].world_y + 6600;
				worldZ = g_objectTable[g_launchRefObjIdx].world_z + 10;
				break;
			case 9:
				worldX = g_objectTable[g_launchRefObjIdx].world_x - 13360;
				worldY = g_objectTable[g_launchRefObjIdx].world_y + 35019;
				worldZ = g_objectTable[g_launchRefObjIdx].world_z - 6537;
				break;
			default:
				worldX = shotIndex;
				worldY = shotIndex;
				worldZ = shotIndex;
				break;
		}
	}

	FlightView_UpdatePlayerCamera(g_localPlayer);
	g_players[g_localPlayer].viewState.savedTargetX = worldX;
	g_players[g_localPlayer].viewState.savedTargetY = worldY;
	g_players[g_localPlayer].viewState.savedTargetZ = worldZ;
	g_players[g_localPlayer].viewState.viewRoll = 0;
	g_players[g_localPlayer].viewState.viewPitch = 0x4000;
	g_players[g_localPlayer].viewState.viewYaw = 0;
	g_players[g_localPlayer].lookYawOffset = 0;
	g_players[g_localPlayer].lookPitchOffset = 0;
	g_players[g_localPlayer].viewState.hudAimX = 0;
	g_players[g_localPlayer].viewState.hudAimY = 0;
	g_players[g_localPlayer].viewState.transitionTimer = 0;
	g_players[g_localPlayer].viewState.playerInputBlocked = 0;
	g_players[g_localPlayer].padlockActive = 0;

	if (g_hangarAutoCam) {
		g_players[g_localPlayer].viewState.externalCameraActive = 1;
		g_players[g_localPlayer].viewState.playerInputBlocked = 0;
		g_drawingOwnCraft = 1;
		g_players[g_localPlayer].viewState.cameraFocusObjIdx = noObject;
		Hud_SetHudViewState(18, g_localPlayer);
	} else {
		FlightObject_AnimateCrewMeshRotations(g_players[g_localPlayer].objectIndex, 1);
		g_drawingOwnCraft = 0;
		g_players[g_localPlayer].viewState.playerInputBlocked = 0;
		g_players[g_localPlayer].viewState.externalCameraActive = 0;
		g_players[g_localPlayer].viewState.cameraFocusObjIdx = g_hangarPlayerObjIdx;
		Hud_SetHudViewState(19, g_localPlayer);
	}

	if ((uint16_t)shotIndex != 0) {
		Hud_SetHudEnabled(g_localPlayer, 0);
	} else if (g_inHangarReady) {
		Hud_SetHudEnabled(g_localPlayer, 1);
	}
}

#ifdef XWA_MODERN
static void Hangar_ModernResetCadenceSensitiveState(void) {
	CraftData* craft;
	int sceneIdx;

	g_modernHangarLegacyCadenceTicks = 0;
	g_modernHangarLegacyCadenceDue = 0;
	craft = g_objectTable[g_hangarAnimatedDroidObjIdx].mobj->pCraft;
	g_modernHangarCraneRotationActive = (craft->meshRotation[0] & 1u) != 0;
	g_modernHangarCraneScaledStateActive = 0;

	g_modernHangarPrimaryDroidRotation.decreasing = 0;
	g_modernHangarPrimaryDroidRotation.scaledStateActive = 0;
	memset(g_modernFamilyDroidRotations, 0, sizeof(g_modernFamilyDroidRotations));
	for (sceneIdx = 0; sceneIdx < g_hangarSceneObjectCount && sceneIdx < 10; ++sceneIdx) {
		craft = g_objectTable[g_hangarSceneObjects[sceneIdx].objectIdx].mobj->pCraft;
		if (sceneIdx == 0) {
			g_modernHangarPrimaryDroidRotation.decreasing = (craft->meshRotation[3] & 1u) != 0;
		}
		g_modernFamilyDroidRotations[sceneIdx][0].decreasing = (craft->meshRotation[4] & 1u) != 0;
		g_modernFamilyDroidRotations[sceneIdx][1].decreasing = (craft->meshRotation[6] & 1u) != 0;
	}
}

static void Hangar_ModernBeginLegacyCadence(int dtTicks) {
	if (!XwaModernFlightTiming_IsHighRate()) {
		g_modernHangarLegacyCadenceTicks = 0;
		g_modernHangarLegacyCadenceDue = 1;
		return;
	}

	g_modernHangarLegacyCadenceTicks += dtTicks;
	if (g_modernHangarLegacyCadenceTicks >= 2) {
		g_modernHangarLegacyCadenceTicks -= 2;
		g_modernHangarLegacyCadenceDue = 1;
	} else {
		g_modernHangarLegacyCadenceDue = 0;
	}
}

static void Hangar_ModernUpdateCraneRotation(int dtTicks, uint16_t stopThreshold) {
	CraftData* craft;
	uint8_t rotation;

	craft = g_objectTable[g_hangarAnimatedDroidObjIdx].mobj->pCraft;
	g_curCraft = craft;
	rotation = craft->meshRotation[0];
	if (g_modernHangarCraneRotationActive) {
		if ((g_hangarSceneObjects[0].routeNodeIdx & 1) != 0) {
			rotation = (uint8_t)(rotation - dtTicks);
		} else {
			rotation = (uint8_t)(rotation + dtTicks);
		}
	}

	if (g_modernHangarLegacyCadenceDue) {
		if (g_modernHangarCraneRotationActive) {
			if ((uint16_t)GameRand() < stopThreshold) {
				rotation ^= 1u;
				g_modernHangarCraneRotationActive = 0;
			}
		} else if ((uint16_t)GameRand() < 0x200u) {
			rotation ^= 1u;
			g_modernHangarCraneRotationActive = 1;
		}
	}
	craft->meshRotation[0] = rotation;
	g_modernHangarCraneScaledStateActive = 1;
}

static void Hangar_ModernPrepareOriginalCraneRotation(void) {
	CraftData* craft;
	uint8_t rotation;

	if (!g_modernHangarCraneScaledStateActive) {
		return;
	}
	craft = g_objectTable[g_hangarAnimatedDroidObjIdx].mobj->pCraft;
	rotation = craft->meshRotation[0];
	if (((rotation & 1u) != 0) != g_modernHangarCraneRotationActive) {
		craft->meshRotation[0] = rotation ^ 1u;
	}
	g_modernHangarCraneScaledStateActive = 0;
}

static void Hangar_ModernUpdatePackedRotation(CraftData* craft, int meshIdx, int dtTicks, int threshold,
											  ModernHangarPackedRotationState* state) {
	uint8_t rotation;

	rotation = craft->meshRotation[meshIdx];
	if (state->decreasing) {
		rotation = (uint8_t)(rotation - dtTicks);
		if (g_modernHangarLegacyCadenceDue && rotation == 0xf7u && (GameRand() & 0xffff) > threshold) {
			rotation = (uint8_t)(rotation + 2u);
		}
		if (rotation == 0xe1u) {
			rotation = (uint8_t)-32;
			state->decreasing = 0;
		}
	} else {
		rotation = (uint8_t)(rotation + dtTicks);
		if (g_modernHangarLegacyCadenceDue && rotation == 4u && (GameRand() & 0xffff) > threshold) {
			rotation = (uint8_t)(rotation - 2u);
		}
		if (rotation == 32u) {
			rotation = 33u;
			state->decreasing = 1;
		}
	}
	craft->meshRotation[meshIdx] = rotation;
	state->scaledStateActive = 1;
}

static void Hangar_ModernPrepareOriginalPackedRotation(CraftData* craft, int meshIdx,
													   ModernHangarPackedRotationState* state) {
	uint8_t rotation;

	if (!state->scaledStateActive) {
		return;
	}
	rotation = craft->meshRotation[meshIdx];
	if (((rotation & 1u) != 0) != state->decreasing) {
		craft->meshRotation[meshIdx] = rotation ^ 1u;
	}
	state->scaledStateActive = 0;
}

static void Hangar_ModernUpdateHangarDroidMesh(int sceneIdx, int objectIdx, int dtTicks) {
	if (sceneIdx == 1) {
		int meshDir;

		g_curCraft = g_objectTable[objectIdx].mobj->pCraft;
		if (g_modernHangarLegacyCadenceDue && (GameRand() & 0xffff) % 100 < 4) {
			meshDir = abs(abs(GameRand() & 0xffff) & 1) - 1;
			g_hangarSceneObjects[1].meshRotationDir = meshDir;
		} else {
			meshDir = g_hangarSceneObjects[1].meshRotationDir;
		}
		g_curCraft->meshRotation[1] = (uint8_t)(g_curCraft->meshRotation[1] + 2 * dtTicks * meshDir);
		if (g_hangarSceneObjects[1].moveState == 0) {
			if (g_modernHangarLegacyCadenceDue) {
				++g_curCraft->meshRotation[2];
			}
		} else {
			g_curCraft->meshRotation[2] = (uint8_t)(g_curCraft->meshRotation[2] + 2 * dtTicks);
		}
	} else if (sceneIdx == 0) {
		g_curCraft = g_objectTable[objectIdx].mobj->pCraft;
		Hangar_ModernUpdatePackedRotation(g_curCraft, 3, dtTicks, 0x200, &g_modernHangarPrimaryDroidRotation);
	}
}

static void Hangar_ModernUpdateFamilyDroidMesh(int sceneIdx, int objectIdx, int dtTicks) {
	int threshold;

	g_curCraft = g_objectTable[objectIdx].mobj->pCraft;
	if (g_modernHangarLegacyCadenceDue && (GameRand() & 0xffff) % 100 < 4) {
		g_hangarSceneObjects[sceneIdx].meshRotationDir = abs(abs(GameRand() & 0xffff) & 1) - 1;
	}
	g_curCraft->meshRotation[3] =
		(uint8_t)(g_curCraft->meshRotation[3] +
				  2 * dtTicks * (int8_t)g_hangarSceneObjects[sceneIdx].meshRotationDir);
	if (g_hangarSceneObjects[sceneIdx].moveState == 0) {
		if (g_modernHangarLegacyCadenceDue) {
			++g_curCraft->meshRotation[0];
		}
	} else {
		g_curCraft->meshRotation[0] = (uint8_t)(g_curCraft->meshRotation[0] + 2 * dtTicks);
	}

	threshold = g_hangarSceneObjects[sceneIdx].moveState != 0 ? 0x200 : 0xa000;
	Hangar_ModernUpdatePackedRotation(g_curCraft, 4, dtTicks, threshold,
									  &g_modernFamilyDroidRotations[sceneIdx][0]);
	Hangar_ModernUpdatePackedRotation(g_curCraft, 6, dtTicks, threshold,
									  &g_modernFamilyDroidRotations[sceneIdx][1]);
}
#endif

// FUNCTION: XWA 0x4554F0
// Initialize the pre-launch hangar/ready scene for the local player. Clears launch
// state, sets up the region/object table around the player's craft and launch
// reference, spawns hangar/display objects, initializes craft defaults, camera
// state, and the loadout menu before the ready scene begins rendering.
void Hangar_SetupReadyScene(void) {
	ObjectRecord* playerObj;
	MobileObject* mobj;
	CraftData* playerCraft;
	uint16_t objectType;
	uint8_t optionalCraftCategory;
	uint8_t warhead;
	uint8_t beamTypeId;
	int boundFlightGroupIdx;
	int launchRefIdx;
	int craftCount;
	int warheadCount;
	int beamCount;
	int cmCount;
	uint16_t* craftListOut;
	int i;
	int halfSizeZ;
	int sceneSlot;
	int containerFgIdx;
	uint32_t savedSlotStart;
	int savedSlotEnd;
	uint32_t searchCursor;
	int searchIdx;
	int newIdx;

	g_hangarSourceObjIdx = 0xffff;
	g_hangarSceneObjectCount = 0;
	g_hangarAutoCam = 0;
	g_launchAnimDone = 0;
	g_flightExitRequest = 0;
	g_launchTriggered = 0;
	g_hangarMissionResolved = 0;
	g_launchSeqPhase = 0;
	g_hangarLaunchMoveSpeed = 0.0f;
	g_hangarLaunchRollRate = 0.0f;
	g_hangarNextAmbientSoundTime = 236 * ((GameRand2() & 7) + 4);

	g_hangarSceneRegionIdx = g_missionRegionCount - 1;
	g_hangarPlayerObjIdx = g_players[g_localPlayer].objectIndex;
	Mission_SetActiveRegionObjectRanges(g_missionRegionCount - 1);

	dstY = (int16_t)((uint16_t)g_screenHeight - (uint16_t)g_hudCmdPanelHeight);
	dstX = (int16_t)((g_screenWidth - (uint16_t)g_hudCmdPanelWidth) >> 1);

	// Clear hangar loadout option-list storage before rebuilding the craft/warhead/
	// beam/countermeasure menus. XWA clears the whole 0x400-byte craft-list block
	// (g_hangarCraftListEnd at index 126 is only the scan limit; storage is 512).
	memset(g_hangarCraftList, 0, sizeof(g_hangarCraftList));
	boundFlightGroupIdx = g_players[g_localPlayer].boundFlightGroupIdx;
	memset(g_hangarWarheadList, 0, sizeof(g_hangarWarheadList));
	memset(g_hangarBeamList, 0, sizeof(g_hangarBeamList));
	memset(g_hangarCountermeasureTypeList, 0, sizeof(g_hangarCountermeasureTypeList));

	for (i = 0; i < 10; ++i) {
		g_hangarMenuItemDisabled[0][i] = 0;
		g_hangarMenuItemDisabled[1][i] = 0;
	}

	playerObj = &g_objectTable[g_players[g_localPlayer].objectIndex];
	objectType = playerObj->objectType;
	craftCount = 1;
	g_hangarCraftList[0] = objectType;

	if (!g_pilotData.campaignMode && !g_provingGroundsModeActive) {
		if (objectType != OBJ_CorellianTransport2) {
			if (objectType != OBJ_FamilyTransport && objectType != OBJ_MilleniumFalcon2) {
				if (objectType != OBJ_XWing) {
					g_hangarCraftList[1] = OBJ_XWing;
					craftCount = 2;
				}
				if (objectType != OBJ_YWing) {
					g_hangarCraftList[craftCount] = OBJ_YWing;
					goto lbl26;
				}
				goto lbl27;
			}
			g_hangarCraftList[1] = OBJ_CorellianTransport2;
			craftCount = 2;
			if (objectType != OBJ_FamilyTransport) {
				g_hangarCraftList[2] = OBJ_FamilyTransport;
				craftCount = 3;
				goto lbl58;
			}
			goto lbl38;
		}
		goto lbl36;
	}

	optionalCraftCategory = g_missionFlightGroups[boundFlightGroupIdx].fg.optionalCraftCategory;
	if (optionalCraftCategory == 4) {
		craftListOut = &g_hangarCraftList[1];
		for (i = 0; i < 10; ++i) {
			uint16_t t =
				(uint16_t)g_objectTypeTables
					.craftTypeToObjectType[g_missionFlightGroups[boundFlightGroupIdx].fg.optionalCraft[i]];
			if (t != OBJ_None && t != g_hangarCraftList[0]) {
				*craftListOut++ = t;
				++craftCount;
			}
		}
		goto lbl58;
	}
	if (optionalCraftCategory != 2) {
		if (optionalCraftCategory != 3) {
			if (optionalCraftCategory != 1) {
				goto lbl58;
			}
			if (objectType != OBJ_XWing) {
				g_hangarCraftList[1] = OBJ_XWing;
				craftCount = 2;
			}
			if (objectType != OBJ_YWing)
				g_hangarCraftList[craftCount++] = OBJ_YWing;
			if (objectType != OBJ_AWing)
				g_hangarCraftList[craftCount++] = OBJ_AWing;
			if (objectType != OBJ_BWing)
				g_hangarCraftList[craftCount++] = OBJ_BWing;
			if (objectType != OBJ_Z95)
				g_hangarCraftList[craftCount++] = OBJ_Z95;
			if (objectType != OBJ_CorellianTransport2)
				g_hangarCraftList[craftCount++] = OBJ_CorellianTransport2;
			if (objectType != OBJ_FamilyTransport)
				g_hangarCraftList[craftCount++] = OBJ_FamilyTransport;
			if (objectType == OBJ_MilleniumFalcon2)
				goto lbl58;
			goto lbl56;
		}
		if (objectType != OBJ_CorellianTransport2) {
			g_hangarCraftList[1] = OBJ_CorellianTransport2;
			craftCount = 2;
		}
		goto lbl36;
	}
	if (objectType != OBJ_XWing) {
		g_hangarCraftList[1] = OBJ_XWing;
		craftCount = 2;
	}
	if (objectType != OBJ_YWing) {
		g_hangarCraftList[craftCount] = OBJ_YWing;
		goto lbl26;
	}
	goto lbl27;
lbl26:
	++craftCount;
lbl27:
	if (objectType != OBJ_AWing)
		g_hangarCraftList[craftCount++] = OBJ_AWing;
	if (objectType != OBJ_BWing)
		g_hangarCraftList[craftCount++] = OBJ_BWing;
	if (objectType != OBJ_Z95) {
		g_hangarCraftList[craftCount] = OBJ_Z95;
		goto lbl57;
	}
	goto lbl58;
lbl36:
	if (objectType != OBJ_FamilyTransport)
		g_hangarCraftList[craftCount++] = OBJ_FamilyTransport;
lbl38:
	if (objectType == OBJ_MilleniumFalcon2)
		goto lbl58;
lbl56:
	g_hangarCraftList[craftCount] = OBJ_MilleniumFalcon2;
lbl57:
	++craftCount;
lbl58:
	if (!g_provingGroundsModeActive) {
		g_hangarMenuItemDisabled[0][3] = (craftCount <= 1);
	}

	// Warhead option list.
	g_hangarWarheadList[0] = 0;
	warheadCount = 1;
	warhead = g_missionFlightGroups[boundFlightGroupIdx].fg.warhead;
	{
		uint16_t warheadOption = warhead;
		if (warheadOption) {
			g_hangarWarheadList[1] = warheadOption;
			warheadCount = 2;
		}
	}
	if (g_pilotData.campaignMode || g_provingGroundsModeActive) {
		for (i = 0; i < 8; ++i) {
			uint16_t w = g_missionFlightGroups[boundFlightGroupIdx].fg.optionalWarheads[i];
			if (w)
				g_hangarWarheadList[warheadCount++] = w;
		}
	} else {
		for (i = 1; i < 9; ++i) {
			if (i != warhead)
				g_hangarWarheadList[warheadCount++] = (uint16_t)i;
		}
	}
	if (!g_provingGroundsModeActive) {
		g_hangarMenuItemDisabled[0][4] = (warheadCount <= 1);
	}

	// Beam option list.
	g_hangarBeamList[0] = 0;
	beamCount = 1;
	playerCraft = playerObj->mobj->pCraft;
	g_curCraft = playerCraft;
	beamTypeId = playerCraft->beamTypeId;
	if (beamTypeId) {
		beamCount = 2;
		g_hangarBeamList[1] = beamTypeId;
	}
	if (g_pilotData.campaignMode || g_provingGroundsModeActive) {
		for (i = 0; i < 6; ++i) {
			uint16_t b = g_missionFlightGroups[boundFlightGroupIdx].fg.optionalBeams[i];
			if (b)
				g_hangarBeamList[beamCount++] = b;
		}
	} else {
		for (i = 1; i < 5; ++i) {
			if (i != 4 && i != playerCraft->beamTypeId)
				g_hangarBeamList[beamCount++] = (uint16_t)i;
		}
	}
	if (!g_provingGroundsModeActive) {
		g_hangarMenuItemDisabled[0][5] = (beamCount <= 1);
	}

	// Countermeasure option list.
	g_hangarCountermeasureTypeList[0] = 0;
	cmCount = 1;
	playerCraft = playerObj->mobj->pCraft;
	g_curCraft = playerCraft;
	{
		uint16_t cmTypeId = playerCraft->cmTypeId;
		if (cmTypeId) {
			g_hangarCountermeasureTypeList[1] = (int16_t)cmTypeId;
			cmCount = 2;
		}
	}
	if (g_pilotData.campaignMode || g_provingGroundsModeActive) {
		for (i = 0; i < 4; ++i) {
			uint16_t c = g_missionFlightGroups[boundFlightGroupIdx].fg.optionalCountermeasures[i];
			if (c)
				g_hangarCountermeasureTypeList[cmCount++] = c;
		}
	} else {
		for (i = 1; i < 4; ++i) {
			if (i != playerCraft->cmTypeId && i != 3)
				g_hangarCountermeasureTypeList[cmCount++] = (int16_t)i;
		}
	}
	if (!g_provingGroundsModeActive) {
		g_hangarMenuItemDisabled[0][6] = (cmCount <= 1);
	}

	Hangar_BuildMenu(1);

	// Carrier hangar (308) for the default hangar type, family base model (179) otherwise.
	g_hangarBackdropModelType = g_pilotData.hangarType ? 179 : OBJ_Hangar;
	if (!g_loadedModels.byObjectType[(uint16_t)g_hangarBackdropModelType]) {
		Hangar_LoadCraftModelByType((ObjectTypeId)g_hangarBackdropModelType);
		Math_SetFpuSinglePrecisionMode();
	}

	// Allocate and configure the launch-reference / backdrop object.
	g_launchRefObjIdx = Object_AllocSlotForGenus(GENUS_Utility);
	launchRefIdx = (uint16_t)g_launchRefObjIdx;
	g_objectTable[launchRefIdx].regionIdx = (uint8_t)g_hangarSceneRegionIdx;
	g_objectTable[launchRefIdx].objectType = (ObjectTypeId)g_hangarBackdropModelType;
	g_objectTable[launchRefIdx].objectSignature = 1;
	g_objectTable[launchRefIdx].mobj->state = 6;
	g_objectTable[launchRefIdx].genusId = GENUS_Utility;
	g_objectTable[launchRefIdx].mobj->rollImpulseRate = 0;
	g_objectTable[launchRefIdx].mobj->speed = 0;
	g_objectTable[launchRefIdx].mobj->speedRemainder = 0;
	g_objectTable[launchRefIdx].mobj->damageAmount = 0x7fff;
	g_objectTable[launchRefIdx].mobj->lifetimeTimer = 0;
	g_objectTable[launchRefIdx].mobj->framesAlive = 0;
	g_objectTable[launchRefIdx].mobj->sourceObjIdx = 0;
	g_objectTable[launchRefIdx].mobj->sourceObjectType = 0;
	g_objectTable[launchRefIdx].mobj->iff = 0;
	g_objectTable[launchRefIdx].mobj->orientMatrixDirty = 1;
	g_objectTable[launchRefIdx].mobj->moveVectorDirty = 1;
	g_objectTable[launchRefIdx].flightGroupIdx = (uint8_t)((uint8_t)g_missionHeader.numFlightGroups + 1);
	mobj = g_objectTable[launchRefIdx].mobj;
	if (g_hangarBackdropModelType == 179) {
		mobj->iff = 5;
	} else {
		mobj->iff = 0;
	}
	g_objectTable[launchRefIdx].mobj->team = g_objectTable[g_hangarPlayerObjIdx].mobj->team;
	g_curCraft = g_objectTable[launchRefIdx].mobj->pCraft;

	for (i = 0; i < 2; ++i) {
		g_objectTable[launchRefIdx].typeSpecificByte[i] = 0;
	}
	for (i = 0; i < 50; ++i) {
		g_curCraft->componentState[i] = 0;
		g_curCraft->meshRotation[i] = 0;
		g_curCraft->componentHp[i] = 0xff;
	}
	g_curCraft->hullMax = 0x7fff;
	g_curCraft->systemDamageHullThreshold = 0x7fff;
	g_curCraft->hullDamage = 0;
	g_curCraft->unusedMissionFlag189 = 0;
	g_curCraft->attackedByTeam[0] = 0;
	g_curCraft->notDisabledAccountingSuppress = 0;
	g_curCraft->wasCaptured = 0;
	g_curCraft->sFoilState = 0;
	for (i = 0; i < 5; ++i) {
		g_curCraft->beamEffectAccum[i] = 0;
	}
	g_curCraft->systemFlags = 1;
	g_curCraft->workingSubsystems = g_curCraft->systemFlags;
	g_curCraft->shieldDistribMode = 0;
	g_curCraft->shieldFront = 0x7fff;
	g_curCraft->shieldRear = 0;

	g_objectTable[launchRefIdx].roll = 0;
	g_objectTable[launchRefIdx].angleD = 0;
	g_objectTable[launchRefIdx].yaw = 0;
	g_objectTable[launchRefIdx].pitch = 0x4000;

	if (g_objectTable[g_hangarPlayerObjIdx].objectType != OBJ_BWing) {
		halfSizeZ = ModelBounds_GetSizeZ((uint16_t)g_objectTable[g_hangarPlayerObjIdx].objectType) / 2;
	} else {
		halfSizeZ = 50;
	}
	g_launchBaseZ = g_objectTable[g_hangarPlayerObjIdx].world_z - halfSizeZ;

	if (g_hangarBackdropModelType == OBJ_Hangar) {
		g_objectTable[launchRefIdx].world_x = g_objectTable[g_hangarPlayerObjIdx].world_x;
		g_objectTable[launchRefIdx].world_y = g_objectTable[g_hangarPlayerObjIdx].world_y + 800;
	} else {
		g_objectTable[launchRefIdx].world_x = g_objectTable[g_hangarPlayerObjIdx].world_x - 142;
		g_objectTable[launchRefIdx].world_y = g_objectTable[g_hangarPlayerObjIdx].world_y + 6800;
	}
	g_objectTable[launchRefIdx].world_z =
		g_launchBaseZ - g_modelDefs[(uint16_t)GetModelIndexFromType((ObjectTypeId)g_hangarBackdropModelType)]
							.meshAttachData[6];

	g_objectTable[launchRefIdx].mobj->prevWorldX = g_objectTable[launchRefIdx].world_x;
	g_objectTable[launchRefIdx].mobj->prevWorldY = g_objectTable[launchRefIdx].world_y;
	g_objectTable[launchRefIdx].mobj->prevWorldZ = g_objectTable[launchRefIdx].world_z;
	FVIEW_calcrotatemove(g_objectTable[launchRefIdx].pitch, g_objectTable[launchRefIdx].yaw,
						 &g_objectTable[launchRefIdx]);
	FVIEW_calcrotateorient(g_objectTable[launchRefIdx].roll, g_objectTable[launchRefIdx].angleD,
						   &g_objectTable[launchRefIdx]);

	if (g_hangarBackdropModelType == OBJ_Hangar) {
		int16_t hangarModelIdx = GetModelIndexFromType(OBJ_Hangar);
		double worldX = (double)g_objectTable[launchRefIdx].world_x;
		float worldY = (float)g_objectTable[launchRefIdx].world_y;
		RenderScene_EnableProjectionYClamp(
			(float)((double)g_modelDefs[(uint16_t)hangarModelIdx].childMountPoints[5] + worldY),
			(float)((double)g_modelDefs[(uint16_t)hangarModelIdx].childMountPoints[2] + worldY),
			(float)((double)g_modelDefs[(uint16_t)hangarModelIdx].childMountPoints[3] + worldX),
			(float)((double)g_modelDefs[(uint16_t)hangarModelIdx].childMountPoints[0] + worldX));
	} else {
		RenderScene_DisableProjectionYClamp();
	}

	if (g_hangarBackdropModelType == OBJ_Hangar) {
		g_hangarDroidTargetObjIdx =
			(uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(0x13a, -1059, 1313, 0x7fffffff, 38600, 0);
		g_hangarAnimatedDroidObjIdx =
			(uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(0x135, 725, -416, 0x7fffffff, 23500, 0);
		g_hangarDroidRouteTargetObjIdx =
			(uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(0x139, -1352, 1440, 0x7fffffff, 536, 0);
		g_unusedHangarPrimaryDroidObjIdx =
			(uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(0x13b, -1065, -56, 0x7fffffff, 47736, 0);
		g_unusedHangarPrimaryDroidObjIdxMirror = g_unusedHangarPrimaryDroidObjIdx;
		if (g_gameConfig.lod[g_flightPlayerCount > 1] > 5u) {
			if (g_gameConfig.lod[g_flightPlayerCount > 1] > 6u) {
				Hangar_SpawnObjectRelativeToLaunchRef(0x13b, 2079, 733, 0x7fffffff, 6400, 0);
				Hangar_SpawnObjectRelativeToLaunchRef(0x13b, -2488, 1215, 0x7fffffff, 11864, 0);
				Hangar_SpawnObjectRelativeToLaunchRef(0x136, 794, -750, -848, 6800, 12500);
				Hangar_SpawnObjectRelativeToLaunchRef(0x136, -803, -1464, 0x7fffffff, 49200, 0);
				Hangar_SpawnObjectRelativeToLaunchRef(0x136, -1563, 944, 0x7fffffff, 51436, 0);
				Hangar_SpawnObjectRelativeToLaunchRef(0x53, -506, -2136, -722, 32836, 16480);
				Hangar_SpawnObjectRelativeToLaunchRef(0x53, -841, -1628, 0x7fffffff, 53100, 0);
				Hangar_SpawnObjectRelativeToLaunchRef(0x53, 1240, -2098, -722, 272, 16480);
				Hangar_SpawnObjectRelativeToLaunchRef(0x54, -834, -2034, 0x7fffffff, 32336, 0);
				Hangar_SpawnObjectRelativeToLaunchRef(0x50, -3474, -275, 0x7fffffff, 29436, 0);
				Hangar_SpawnObjectRelativeToLaunchRef(2, -1038, -798, 0x7fffffff, 7700, 0);
				Hangar_SpawnObjectRelativeToLaunchRef(0x32, 1146, -1288, -741, 50536, 0);
			}
			Hangar_SpawnObjectRelativeToLaunchRef(0x13b, 820, -665, 0x7fffffff, 49136, 0);
			Hangar_SpawnObjectRelativeToLaunchRef(0x136, -873, -1456, -849, 60472, 20400);
			Hangar_SpawnObjectRelativeToLaunchRef(0x136, -3095, -592, 0x7fffffff, 51436, 0);
			Hangar_SpawnObjectRelativeToLaunchRef(0x53, -1751, -1996, 0x7fffffff, 31172, 0);
			Hangar_SpawnObjectRelativeToLaunchRef(0x53, 979, -2103, 0x7fffffff, 37036, 0);
			Hangar_SpawnObjectRelativeToLaunchRef(0x54, -1374, -2068, 0x7fffffff, 40536, 0);
			Hangar_SpawnObjectRelativeToLaunchRef(0x50, 2806, 1357, 0x7fffffff, 20536, 0);
			Hangar_SpawnObjectRelativeToLaunchRef(0x50, -195, -1910, 0x7fffffff, 54372, 0);
			Hangar_SpawnObjectRelativeToLaunchRef(3, 2178, 506, -830, 40036, 1500);
			Hangar_SpawnObjectRelativeToLaunchRef(2, 1100, -503, 0x7fffffff, 49036, 0);
			Hangar_SpawnObjectRelativeToLaunchRef(1, -1003, 280, -825, 16600, 0);
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
	} else {
		g_unusedHangarPrimaryDroidObjIdx =
			(uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(0x13b, 821, -6618, 0x7fffffff, 30600, 0);
		g_unusedHangarPrimaryDroidObjIdxMirror = g_unusedHangarPrimaryDroidObjIdx;
		g_hangarDroidTargetObjIdx =
			(uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(0x13a, 2431, -5447, 0x7fffffff, 61536, 0);
		g_hangarAnimatedDroidObjIdx =
			(uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(0x135, 626, -6397, 0x7fffffff, 736, 0);
		g_hangarDroidRouteTargetObjIdx =
			(uint16_t)Hangar_SpawnObjectRelativeToLaunchRef(0x139, 1577, -6663, 0x7fffffff, 500, 0);
		if (g_gameConfig.lod[g_flightPlayerCount > 1] > 5u) {
			Hangar_SpawnObjectRelativeToLaunchRef(0x50, -1954, -7445, 0x7fffffff, 31236, 0);
			Hangar_SpawnObjectRelativeToLaunchRef(0x50, -1938, -7445, -4997, 31236, 0);
			Hangar_SpawnObjectRelativeToLaunchRef(0x53, -1231, -8544, -5003, 32736, 0x4000);
			Hangar_SpawnObjectRelativeToLaunchRef(0x53, -1481, -8444, -5003, 45736, 0x4000);
			Hangar_SpawnObjectRelativeToLaunchRef(0x53, -1821, -8464, -5003, 32736, 0x4000);
			Hangar_SpawnObjectRelativeToLaunchRef(0x54, -1834, -6154, 0x7fffffff, 42736, 0);
			Hangar_SpawnObjectRelativeToLaunchRef(0x54, 1939, -8013, 0x7fffffff, 31800, 0);
			if (g_gameConfig.lod[g_flightPlayerCount > 1] > 6u) {
				Hangar_SpawnObjectRelativeToLaunchRef(0x50, -1954, -7445, -4888, 31236, 0);
				Hangar_SpawnObjectRelativeToLaunchRef(0x50, -1978, -7386, -4781, 31736, 0);
				Hangar_SpawnObjectRelativeToLaunchRef(0x136, 1484, -8339, -5133, 29700, 20400);
				Hangar_SpawnObjectRelativeToLaunchRef(0x136, 1384, -8339, 0x7fffffff, 32036, 0);
				Hangar_SpawnObjectRelativeToLaunchRef(0x53, -2061, -8344, -5003, 32736, 0x4000);
				Hangar_SpawnObjectRelativeToLaunchRef(0x53, -1511, -6734, 0x7fffffff, 27736, 0x8000);
			}
		}

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

		// Find the active-region container-hangar marker (placed at 25600,25600,25600)
		// and clone the hangar-scene objects into the player's region around the
		// launch reference, inheriting the container's flight group.
		containerFgIdx = -1;
		Mission_SetActiveRegionObjectRanges(g_hangarSceneRegionIdx);
		savedSlotStart = g_activeRegionObjectSlotStart;
		savedSlotEnd = g_activeRegionCraftObjectSlotEnd;
		Mission_SetActiveRegionObjectRanges(g_players[g_localPlayer].regionIndex);
		searchCursor = g_activeRegionObjectSlotStart;
		searchIdx = (uint16_t)g_activeRegionObjectSlotStart;
		if ((uint16_t)g_activeRegionObjectSlotStart < g_activeRegionCraftObjectSlotEnd) {
			while (1) {
				ObjectRecord* obj = &g_objectTable[searchIdx];
				if (obj->objectType == OBJ_ContainerHanger && obj->world_x == 25600 &&
					obj->world_y == 25600 && obj->world_z == 25600) {
					containerFgIdx = g_objectTable[searchCursor].flightGroupIdx;
					g_objectTable[searchCursor].objectType = OBJ_None;
					break;
				}
				searchIdx = (int)++searchCursor;
				if (searchCursor >= g_activeRegionCraftObjectSlotEnd) {
					break;
				}
			}
		}

		if (containerFgIdx != -1) {
			for (searchIdx = (uint16_t)savedSlotStart; (uint16_t)savedSlotStart < savedSlotEnd;
				 searchIdx = (uint16_t)++savedSlotStart) {
				int srcIdx = searchIdx;
				int srcObjType = (uint16_t)g_objectTable[srcIdx].objectType;

				if (srcObjType <= 0x50) {
					if (srcObjType == 80 || srcObjType == 0) {
						continue;
					}
				} else if (srcObjType == 179 || srcObjType == 308 || srcObjType == 489) {
					continue;
				}

				g_currentFlightGroupIdx = (uint16_t)containerFgIdx;
				{
					ObjectTypeId spawnObjectType = g_objectTable[srcIdx].objectType;
					g_spawnLinkedObjectFlag = 1;
					g_spawnGenusId = g_modelTypeTable[(uint16_t)spawnObjectType].genusId;
				}
				g_spawnStatus1 = g_missionFlightGroups[containerFgIdx].fg.status1;
				g_spawnStatus2 = g_missionFlightGroups[containerFgIdx].fg.status2;
				newIdx =
					(uint16_t)Mission_InitFlightGroupObjectSlot(g_objectTable[srcIdx].objectType, 0xffff);
				if (newIdx != 0xffff) {
					g_objectTable[newIdx].objectType = g_objectTable[srcIdx].objectType;
					g_objectTable[newIdx].roll = g_objectTable[srcIdx].roll;
					g_objectTable[newIdx].yaw = g_objectTable[srcIdx].yaw;
					g_objectTable[newIdx].pitch = g_objectTable[srcIdx].pitch;
					g_objectTable[newIdx].world_x = g_objectTable[srcIdx].world_x;
					g_objectTable[newIdx].world_y = g_objectTable[srcIdx].world_y;
					g_objectTable[newIdx].world_z = g_objectTable[srcIdx].world_z;
					g_objectTable[newIdx].world_x =
						g_objectTable[newIdx].world_x - g_objectTable[g_launchRefObjIdx].world_x;
					g_objectTable[newIdx].world_y =
						g_objectTable[newIdx].world_y - g_objectTable[g_launchRefObjIdx].world_y;
					g_objectTable[newIdx].world_z =
						g_objectTable[newIdx].world_z - g_objectTable[g_launchRefObjIdx].world_z;
					g_objectTable[newIdx].mobj->prevWorldX = g_objectTable[newIdx].world_x;
					g_objectTable[newIdx].mobj->prevWorldY = g_objectTable[newIdx].world_y;
					g_objectTable[newIdx].mobj->prevWorldZ = g_objectTable[newIdx].world_z;
					g_objectTable[newIdx].mobj->iff = g_objectTable[g_launchRefObjIdx].mobj->iff;
					g_objectTable[newIdx].mobj->team = g_objectTable[g_launchRefObjIdx].mobj->team;
					FVIEW_calcrotatemove(g_objectTable[newIdx].pitch, g_objectTable[newIdx].yaw,
										 &g_objectTable[newIdx]);
					FVIEW_calcrotateorient(g_objectTable[newIdx].roll, g_objectTable[newIdx].angleD,
										   &g_objectTable[newIdx]);
				}
			}
		}
	}

#ifdef XWA_MODERN
	Hangar_ModernResetCadenceSensitiveState();
#endif
	Mission_SetActiveRegionObjectRanges(g_players[g_localPlayer].regionIndex);
}

// FUNCTION: XWA 0x462640
int Hangar_GetLaunchModelZOffset(int modelType) {
	modelType &= 0xffff;
	if (modelType != 4) {
		return ModelBounds_GetSizeZ(modelType) / 2;
	}

	return 50;
}

// FUNCTION: XWA 0x456AE0
int16_t Hangar_SpawnObjectRelativeToLaunchRef(uint16_t modelType, int relX, int relY, int relZ,
											  Q16Angle relYaw, Q16Angle relPitch) {
	int objIdx;
	int componentIdx;
	int systemIdx;

	if (g_loadedModels.byObjectType[modelType] == 0) {
		Hangar_LoadCraftModelByType(modelType);
		Math_SetFpuSinglePrecisionMode();
		if (g_loadedModels.byObjectType[modelType] == 0) {
			return -1;
		}
	}

	g_modelTypeTable[modelType].assetFlags |= 0x10u;
	objIdx = Object_AllocSlotForGenus(g_modelTypeTable[modelType].genusId);
	if (objIdx == 0xffffu) {
		return 0;
	}

	g_objectTable[objIdx].regionIdx = (uint8_t)g_hangarSceneRegionIdx;
	g_objectTable[objIdx].objectType = (ObjectTypeId)modelType;
	g_objectTable[objIdx].objectSignature = 1;
	g_objectTable[objIdx].mobj->state = g_modelTypeTable[modelType].familyId;
	g_objectTable[objIdx].genusId = g_modelTypeTable[modelType].genusId;
	g_objectTable[objIdx].mobj->rollImpulseRate = 0;
	g_objectTable[objIdx].mobj->speed = 0;
	g_objectTable[objIdx].mobj->speedRemainder = 0;
	g_objectTable[objIdx].mobj->damageAmount = 0x7fff;
	g_objectTable[objIdx].mobj->lifetimeTimer = 0;
	g_objectTable[objIdx].mobj->framesAlive = 0;
	g_objectTable[objIdx].mobj->sourceObjIdx = 0;
	g_objectTable[objIdx].mobj->sourceObjectType = 0;
	g_objectTable[objIdx].mobj->iff = g_objectTable[g_launchRefObjIdx].mobj->iff;
	g_objectTable[objIdx].mobj->team = g_objectTable[g_launchRefObjIdx].mobj->team;
	g_objectTable[objIdx].mobj->orientMatrixDirty = 1;
	g_objectTable[objIdx].mobj->moveVectorDirty = 1;
	g_objectTable[objIdx].flightGroupIdx = g_objectTable[g_launchRefObjIdx].flightGroupIdx;
	for (componentIdx = 0; componentIdx < 2; ++componentIdx) {
		g_objectTable[objIdx].typeSpecificByte[componentIdx] = 0;
	}

	g_curCraft = g_objectTable[objIdx].mobj->pCraft;
	if (g_curCraft != NULL) {
		for (componentIdx = 0; componentIdx < 50; ++componentIdx) {
			g_curCraft->componentState[componentIdx] = 0;
			g_curCraft->meshRotation[componentIdx] = 0;
			g_curCraft->componentHp[componentIdx] = 0xffu;
		}

		if (modelType == OBJ_Shuttle) {
			g_curCraft->engineOutputScale = 0xffffu;
		} else {
			g_curCraft->engineOutputScale = 0;
		}
		g_curCraft->hullMax = 0x7fff;
		g_curCraft->systemDamageHullThreshold = 0x7fff;
		g_curCraft->hullDamage = 0;
		g_curCraft->unusedMissionFlag189 = 0;
		g_curCraft->attackedByTeam[0] = 0;
		g_curCraft->notDisabledAccountingSuppress = 0;
		g_curCraft->wasCaptured = 0;
		g_curCraft->sFoilState = 0;
		for (componentIdx = 0; componentIdx < 5; ++componentIdx) {
			g_curCraft->beamEffectAccum[componentIdx] = 0;
		}
		for (systemIdx = 0; systemIdx < 10; ++systemIdx) {
			g_curCraft->systemHealth[systemIdx] = 100;
		}
		g_curCraft->systemFlags = 1023;
		g_curCraft->workingSubsystems = g_curCraft->systemFlags;
		g_curCraft->shieldDistribMode = 0;
		g_curCraft->shieldFront = 0x7fff;
		g_curCraft->shieldRear = 0;
		g_curCraft->carriedObjectIndex = 0xffffu;
	}

	g_objectTable[objIdx].roll = g_objectTable[g_launchRefObjIdx].roll;
	g_objectTable[objIdx].yaw = (uint16_t)(relYaw + g_objectTable[g_launchRefObjIdx].yaw);
	g_objectTable[objIdx].pitch = (uint16_t)(relPitch + g_objectTable[g_launchRefObjIdx].pitch);
	g_objectTable[objIdx].world_x = relX + g_objectTable[g_launchRefObjIdx].world_x;
	g_objectTable[objIdx].world_y = relY + g_objectTable[g_launchRefObjIdx].world_y;
	if (relZ != 0x7fffffff) {
		g_objectTable[objIdx].world_z = relZ + g_objectTable[g_launchRefObjIdx].world_z;
	} else {
		int zOffset;

		zOffset = modelType;
		if (zOffset != 4) {
			zOffset = ModelBounds_GetSizeZ(zOffset) / 2;
		} else {
			zOffset = 50;
		}
		g_objectTable[objIdx].world_z = zOffset + g_launchBaseZ;
	}

	g_objectTable[objIdx].mobj->prevWorldX = g_objectTable[objIdx].world_x;
	g_objectTable[objIdx].mobj->prevWorldY = g_objectTable[objIdx].world_y;
	g_objectTable[objIdx].mobj->prevWorldZ = g_objectTable[objIdx].world_z;
	FlightObject_InitMeshAnimationDefaults(objIdx);
	FVIEW_calcrotatemove(g_objectTable[objIdx].pitch, g_objectTable[objIdx].yaw, &g_objectTable[objIdx]);
	FVIEW_calcrotateorient(g_objectTable[objIdx].roll, g_objectTable[objIdx].angleD, &g_objectTable[objIdx]);
	return (int16_t)objIdx;
}

// FUNCTION: XWA 0x460490
void Hangar_UpdateRoofCraneMotion(int dtTicks) {
	unsigned int objectIdx;
	unsigned int decisionTime;

	objectIdx = g_hangarRoofCraneState.objectIdx;
	decisionTime = 4 * g_gameTime;

	switch (g_hangarRoofCraneState.moveState) {
		case 1: {
			int* worldX;
			int distance;

			worldX = &g_objectTable[objectIdx].world_x;
			distance = abs(*worldX - g_hangarRoofCraneState.targetWorldX);

			if (distance <= 100) {
				g_hangarRoofCraneState.moveState = 0;
				g_hangarRoofCraneState.nextDecisionTime = decisionTime + (GameRand() & 0x3fff);
				return;
			}

			if (distance > 300) {
				if (g_hangarRoofCraneState.moveSpeed < g_hangarSceneCruiseMoveSpeed) {
					g_hangarRoofCraneState.moveSpeed -= (float)dtTicks * g_hangarSceneAccelPerTickNeg;
				}
			} else if (g_hangarRoofCraneState.moveSpeed != g_hangarSceneMinMoveSpeed) {
				g_hangarRoofCraneState.moveSpeed -= (float)dtTicks * g_hangarSceneDecelPerTick;
				if (g_hangarRoofCraneState.moveSpeed < g_hangarSceneMinMoveSpeed) {
					g_hangarRoofCraneState.moveSpeed = 2.0f;
				}
			}

			if (*worldX < g_hangarRoofCraneState.targetWorldX) {
				*worldX += (int)((double)dtTicks * g_hangarRoofCraneState.moveSpeed);
				return;
			}
			if (*worldX > g_hangarRoofCraneState.targetWorldX) {
				*worldX -= (int)((double)dtTicks * g_hangarRoofCraneState.moveSpeed);
				return;
			}
			break;
		}

		case 0:
			if (decisionTime >= (unsigned int)g_hangarRoofCraneState.nextDecisionTime) {
				int offset;
				int launchRefObjIdx;
				int currentWorldX;
				int targetWorldX;

				if (((uint16_t)GameRand() & 0x8000u) != 0) {
					offset = 930;
					fsfx_PlaySound(140, objectIdx, (unsigned int)g_localPlayer);
				} else {
					offset = 370;
					fsfx_PlaySound(141, objectIdx, (unsigned int)g_localPlayer);
				}

				if (((uint16_t)GameRand() & 0x8000u) != 0) {
					offset = -offset;
				}

				launchRefObjIdx = g_launchRefObjIdx;
				currentWorldX = g_objectTable[objectIdx].world_x;
				targetWorldX = offset + currentWorldX;
				if (targetWorldX > g_objectTable[launchRefObjIdx].world_x + 1400 ||
					targetWorldX < g_objectTable[launchRefObjIdx].world_x - 1400) {
					offset = -offset;
				}

				offset += g_objectTable[objectIdx].world_x;
				g_hangarRoofCraneState.moveState = 1;
				g_hangarRoofCraneState.targetWorldX = offset;
			}
			break;
	}
}

// FUNCTION: XWA 0x45D910
void Hangar_UpdateHangarDroidTraffic(int dtTicks) {
	int decisionTime;
	int sceneIdx;

	decisionTime = 4 * g_gameTime;
	sceneIdx = 0;
	while (sceneIdx < g_hangarSceneObjectCount) {
		int launchSeqPhase;
		ObjectRecord* objectTable;
		int playerObjIdx;
		unsigned int objectIdx;

		launchSeqPhase = g_launchSeqPhase;
		objectTable = g_objectTable;
		playerObjIdx = g_hangarPlayerObjIdx;
		objectIdx = g_hangarSceneObjects[sceneIdx].objectIdx;

		if (launchSeqPhase != 0 && launchSeqPhase != 8 && launchSeqPhase != 7 &&
			g_hangarSceneObjects[sceneIdx].moveState != 2) {
			ObjectRecord* playerObj;

			playerObj = &objectTable[playerObjIdx];
			if (Hangar_Abs32(objectTable[objectIdx].world_x - playerObj->world_x) < 400 &&
				Hangar_Abs32(objectTable[objectIdx].world_y - playerObj->world_y) < 400) {
				g_hangarSceneObjects[sceneIdx].moveState = 2;
				if (sceneIdx == 0) {
					ObjectRecord* launchRef;

					g_hangarSceneObjects[0].targetObjIdx = g_hangarDroidTargetObjIdx;
					launchRef = &objectTable[g_launchRefObjIdx];
					g_hangarSceneObjects[0].targetWorldX = launchRef->world_x - 840;
					g_hangarSceneObjects[0].targetWorldY = launchRef->world_y + 1210;
				} else {
					ObjectRecord* launchRef;

					g_hangarSceneObjects[sceneIdx].targetObjIdx = g_hangarDroidRouteTargetObjIdx;
					launchRef = &objectTable[g_launchRefObjIdx];
					g_hangarSceneObjects[sceneIdx].targetWorldX = launchRef->world_x - 1440;
					g_hangarSceneObjects[sceneIdx].targetWorldY = launchRef->world_y + 960;
					if (Hangar_Abs32(objectTable[objectIdx].world_x - playerObj->world_x) < 100 &&
						Hangar_Abs32(objectTable[objectIdx].world_y - playerObj->world_y) < 100) {
						fsfx_PlaySound(124, objectIdx, (unsigned int)g_localPlayer);
					}
					fsfx_PlaySound(123, objectIdx, (unsigned int)g_localPlayer);
					objectTable = g_objectTable;
				}
			}
		}

		if (g_hangarSceneObjects[sceneIdx].moveState != 0) {
			if ((unsigned int)g_hangarSceneObjects[sceneIdx].moveState > 0u &&
				(unsigned int)g_hangarSceneObjects[sceneIdx].moveState <= 2u) {
				if (Hangar_Abs32(objectTable[objectIdx].world_x -
								 g_hangarSceneObjects[sceneIdx].targetWorldX) <= 50 &&
					Hangar_Abs32(objectTable[objectIdx].world_y -
								 g_hangarSceneObjects[sceneIdx].targetWorldY) <= 50) {
					g_hangarSceneObjects[sceneIdx].moveState = 0;
					if (g_hangarDroidRouteNodes[g_hangarSceneObjects[sceneIdx].routeNodeIdx].anchorMode ==
						1) {
						g_hangarSceneObjects[sceneIdx].nextDecisionTime = decisionTime;
					} else {
						g_hangarSceneObjects[sceneIdx].nextDecisionTime =
							(uint16_t)GameRand() % 8000 + decisionTime + 4000;
					}
					if (g_hangarSceneObjects[sceneIdx].targetObjIdx == g_hangarPlayerObjIdx &&
						sceneIdx == 1) {
						switch (GameRand2() & 3) {
							case 0:
								fsfx_PlaySound(125, objectIdx, (unsigned int)g_localPlayer);
								break;
							case 1:
								fsfx_PlaySound(123, objectIdx, (unsigned int)g_localPlayer);
								break;
							case 2:
								fsfx_PlaySound(121, objectIdx, (unsigned int)g_localPlayer);
								break;
							case 3:
								fsfx_PlaySound(122, objectIdx, (unsigned int)g_localPlayer);
								break;
						}
					}
				} else {
					trig2_ctop(g_hangarSceneObjects[sceneIdx].targetWorldX - g_objectTable[objectIdx].world_x,
							   g_hangarSceneObjects[sceneIdx].targetWorldY - g_objectTable[objectIdx].world_y,
							   0);
					{
						Q16Angle yaw;
						int yawDelta;
						int yawStep;

						yaw = g_objectTable[objectIdx].yaw;
						if (yaw != trig2_xyangle) {
							yawDelta = (uint16_t)yaw - (uint16_t)trig2_xyangle;
							if (Hangar_Abs32(yawDelta) <= 100) {
								g_objectTable[objectIdx].yaw = trig2_xyangle;
							} else {
								yawStep = yawDelta / 20;
								if ((uint16_t)yawStep > 200u) {
									yawStep = 200;
								}
								g_objectTable[objectIdx].yaw = (Q16Angle)(yaw - dtTicks * yawStep);
								if (g_objectTable[objectIdx].objectType == OBJ_HangarDroid &&
									g_hangarSceneObjects[sceneIdx].moveState != 2) {
									g_hangarSceneObjects[sceneIdx].moveSpeed = 0.0f;
								}
							}
							FVIEW_calcrotatemove(g_objectTable[objectIdx].pitch, g_objectTable[objectIdx].yaw,
												 &g_objectTable[objectIdx]);
							FVIEW_calcrotateorient(g_objectTable[objectIdx].roll,
												   g_objectTable[objectIdx].angleD,
												   &g_objectTable[objectIdx]);
						}
					}

					if (Hangar_Abs32(g_objectTable[objectIdx].world_x -
									 g_hangarSceneObjects[sceneIdx].targetWorldX) <= 400 &&
						Hangar_Abs32(g_objectTable[objectIdx].world_y -
									 g_hangarSceneObjects[sceneIdx].targetWorldY) <= 400) {
						if (g_hangarSceneObjects[sceneIdx].moveSpeed != 2.0f) {
							g_hangarSceneObjects[sceneIdx].moveSpeed -= dtTicks * g_hangarSceneDecelPerTick;
							if (g_hangarSceneObjects[sceneIdx].moveSpeed < g_hangarSceneMinMoveSpeed) {
								g_hangarSceneObjects[sceneIdx].moveSpeed = 2.0f;
							}
						}
					} else if (g_hangarSceneObjects[sceneIdx].moveState == 2) {
						if (g_hangarSceneObjects[sceneIdx].moveSpeed < g_hangarSceneEscapeMoveSpeed) {
							g_hangarSceneObjects[sceneIdx].moveSpeed -=
								dtTicks * g_hangarSceneEscapeAccelPerTickNeg;
						}
					} else if (g_hangarSceneObjects[sceneIdx].moveSpeed < g_hangarSceneCruiseMoveSpeed) {
						g_hangarSceneObjects[sceneIdx].moveSpeed -= dtTicks * g_hangarSceneAccelPerTickNeg;
					}

					{
						float yawRadians;

						if (g_objectTable[objectIdx].objectType == OBJ_HangarDroid2) {
							yawRadians = (float)trig2_xyangle * g_hangarAngleToRadians;
						} else {
							yawRadians = (float)g_objectTable[objectIdx].yaw * g_hangarAngleToRadians;
						}
						g_objectTable[objectIdx].world_x +=
							(int)(sin(yawRadians) * (double)g_hangarSceneObjects[sceneIdx].moveSpeed *
								  (double)dtTicks);
						g_objectTable[objectIdx].world_y +=
							(int)(cos(yawRadians) * (double)g_hangarSceneObjects[sceneIdx].moveSpeed *
								  (double)dtTicks);
					}
				}
			}
		} else {
			{
				int16_t yawDelta;
				int yawStep;
				int absYawDelta;

				yawDelta = (int16_t)(g_objectTable[objectIdx].yaw - g_hangarSceneObjects[sceneIdx].targetYaw);
				if (yawDelta != 0) {
					if (yawDelta > 0) {
						yawStep = -5 * dtTicks;
					} else {
						yawStep = 5 * dtTicks;
					}
					yawStep *= 20;
					absYawDelta = Xwa_Abs32(yawDelta);
					if (Xwa_Abs32(yawStep) > absYawDelta) {
						yawStep = -yawDelta;
					}
					if (absYawDelta > 0x8000) {
						yawStep = -yawStep;
					}
					g_objectTable[objectIdx].yaw = (Q16Angle)(g_objectTable[objectIdx].yaw + yawStep);
					FVIEW_calcrotatemove(g_objectTable[objectIdx].pitch, g_objectTable[objectIdx].yaw,
										 &g_objectTable[objectIdx]);
					FVIEW_calcrotateorient(g_objectTable[objectIdx].roll, g_objectTable[objectIdx].angleD,
										   &g_objectTable[objectIdx]);
				}
			}

			if ((unsigned int)decisionTime >= (unsigned int)g_hangarSceneObjects[sceneIdx].nextDecisionTime) {
				int nextRouteNodeIdx;
				int choice;
				int targetObjIdx;
				ObjectRecord* targetObj;

				do {
					do {
						choice = (uint16_t)GameRand() % 6;
						nextRouteNodeIdx =
							g_hangarDroidRouteNodes[g_hangarSceneObjects[sceneIdx].routeNodeIdx]
								.nextNode[choice];
					} while (nextRouteNodeIdx == -1);
				} while (g_hangarDroidRouteNodes[g_hangarSceneObjects[sceneIdx].routeNodeIdx].anchorMode ==
							 1 &&
						 nextRouteNodeIdx == g_hangarSceneObjects[sceneIdx].prevRouteNodeIdx);
				if (g_hangarDroidRouteNodes[nextRouteNodeIdx].anchorMode != 3) {
					targetObjIdx = g_launchRefObjIdx;
				} else {
					targetObjIdx = g_hangarPlayerObjIdx;
				}
				g_hangarSceneObjects[sceneIdx].targetObjIdx = targetObjIdx;
				g_hangarSceneObjects[sceneIdx].prevRouteNodeIdx = g_hangarSceneObjects[sceneIdx].routeNodeIdx;
				g_hangarSceneObjects[sceneIdx].routeNodeIdx = nextRouteNodeIdx;
				targetObj = &g_objectTable[targetObjIdx];
				g_hangarSceneObjects[sceneIdx].targetWorldX =
					targetObj->world_x + g_hangarDroidRouteNodes[nextRouteNodeIdx].targetOffsetX;
				g_hangarSceneObjects[sceneIdx].targetWorldY =
					targetObj->world_y + g_hangarDroidRouteNodes[nextRouteNodeIdx].targetOffsetY;
				g_hangarSceneObjects[sceneIdx].targetWorldZ =
					targetObj->world_z + g_hangarDroidRouteNodes[nextRouteNodeIdx].targetOffsetZ;
				g_hangarSceneObjects[sceneIdx].targetYaw = g_hangarDroidRouteNodes[nextRouteNodeIdx].yaw;
				g_hangarSceneObjects[sceneIdx].moveState = 1;
				fsfx_PlaySound((sceneIdx & 3) + 142, objectIdx, (unsigned int)g_localPlayer);
			}
		}

		if (sceneIdx == 1) {
			ObjectRecord* firstDroidObj;

			firstDroidObj = &g_objectTable[g_hangarSceneObjects[0].objectIdx];
			if (Hangar_Abs32(g_objectTable[objectIdx].world_x - firstDroidObj->world_x) < 400 &&
				Hangar_Abs32(g_objectTable[objectIdx].world_y - firstDroidObj->world_y) < 400) {
				double dt;
				double yawRadians;

				trig2_ctop(g_objectTable[objectIdx].world_x - firstDroidObj->world_x,
						   g_objectTable[objectIdx].world_y - firstDroidObj->world_y, 0);
				yawRadians = (double)trig2_xyangle * g_hangarAngleToRadians;
				dt = (double)dtTicks;
				g_objectTable[objectIdx].world_x -=
					(int)((double)(400 -
								   Hangar_Abs32(g_objectTable[objectIdx].world_x - firstDroidObj->world_x)) *
						  sin(yawRadians) * dt * g_hangarDroidSeparationScale);
				g_objectTable[objectIdx].world_y -=
					(int)((double)(400 -
								   Hangar_Abs32(g_objectTable[objectIdx].world_y - firstDroidObj->world_y)) *
						  cos(yawRadians) * dt * g_hangarDroidSeparationScale);
			}
		}

		{
			int routeActive;

			routeActive = g_hangarSceneObjects[sceneIdx].moveState != 0 && (decisionTime & 0x100) == 0;
			switch (sceneIdx) {
				case 0:
					g_hangarDroid0RouteActive = routeActive;
					break;
				case 1:
					g_hangarDroid1RouteActive = routeActive;
					break;
			}
		}

#ifdef XWA_MODERN
		if (XwaModernFlightTiming_IsHighRate()) {
			Hangar_ModernUpdateHangarDroidMesh(sceneIdx, objectIdx, dtTicks);
		} else {
			if (sceneIdx == 0) {
				g_curCraft = g_objectTable[objectIdx].mobj->pCraft;
				Hangar_ModernPrepareOriginalPackedRotation(g_curCraft, 3,
														   &g_modernHangarPrimaryDroidRotation);
			}
#endif
			switch (sceneIdx) {
				case 1: {
					int meshDir;

					g_curCraft = g_objectTable[objectIdx].mobj->pCraft;
					if ((GameRand() & 0xffff) % 100 < 4) {
						meshDir = abs(GameRand() & 0xffff);
						meshDir = abs(meshDir & 1) - 1;
						g_hangarSceneObjects[1].meshRotationDir = meshDir;
					} else {
						meshDir = g_hangarSceneObjects[1].meshRotationDir;
					}
					g_curCraft->meshRotation[1] = (uint8_t)(g_curCraft->meshRotation[1] + 4 * meshDir);
					if (g_hangarSceneObjects[1].moveState == 0) {
						++g_curCraft->meshRotation[2];
					} else {
						g_curCraft->meshRotation[2] = (uint8_t)(g_curCraft->meshRotation[2] + 4u);
					}
					break;
				}
				case 0:
					g_curCraft = g_objectTable[objectIdx].mobj->pCraft;
					if ((g_curCraft->meshRotation[3] & 1u) != 0) {
						g_curCraft->meshRotation[3] = (uint8_t)(g_curCraft->meshRotation[3] - 2u);
						if (g_curCraft->meshRotation[3] == 0xf7u && (uint16_t)GameRand() > 0x200u) {
							g_curCraft->meshRotation[3] = (uint8_t)(g_curCraft->meshRotation[3] + 2u);
						}
						if (g_curCraft->meshRotation[3] == 0xe1u) {
							g_curCraft->meshRotation[3] = (uint8_t)-32;
						}
					} else {
						g_curCraft->meshRotation[3] = (uint8_t)(g_curCraft->meshRotation[3] + 2u);
						if (g_curCraft->meshRotation[3] == 4u && (uint16_t)GameRand() > 0x200u) {
							g_curCraft->meshRotation[3] = (uint8_t)(g_curCraft->meshRotation[3] - 2u);
						}
						if (g_curCraft->meshRotation[3] == 32u) {
							g_curCraft->meshRotation[3] = 33u;
						}
					}
					break;
			}
#ifdef XWA_MODERN
		}
#endif

		++sceneIdx;
	}

#ifdef XWA_MODERN
	if (XwaModernFlightTiming_IsHighRate()) {
		Hangar_ModernUpdateCraneRotation(dtTicks, 0x300u);
	} else {
		Hangar_ModernPrepareOriginalCraneRotation();
#endif
		g_curCraft = g_objectTable[g_hangarAnimatedDroidObjIdx].mobj->pCraft;
		if ((g_curCraft->meshRotation[0] & 1u) != 0) {
			if ((g_hangarSceneObjects[0].routeNodeIdx & 1) != 0) {
				g_curCraft->meshRotation[0] = (uint8_t)(g_curCraft->meshRotation[0] - 2u);
			} else {
				g_curCraft->meshRotation[0] = (uint8_t)(g_curCraft->meshRotation[0] + 2u);
			}
			if ((uint16_t)GameRand() < 0x300u) {
				g_curCraft->meshRotation[0] ^= 1u;
			}
		} else if ((uint16_t)GameRand() < 0x200u) {
			g_curCraft->meshRotation[0] ^= 1u;
		}
#ifdef XWA_MODERN
	}
#endif
}

// FUNCTION: XWA 0x45E320
void Hangar_UpdateFamilyBaseDroidTraffic(int dtTicks) {
	int decisionTime;
	int sceneIdx;

	decisionTime = 4 * g_gameTime;
	sceneIdx = 0;
	while (sceneIdx < g_hangarSceneObjectCount) {
		int objectIdx;

		objectIdx = g_hangarSceneObjects[sceneIdx].objectIdx;

		if (g_launchSeqPhase != 0 && g_launchSeqPhase != 8 && g_launchSeqPhase != 7 &&
			g_hangarSceneObjects[sceneIdx].moveState != 2) {
			ObjectRecord* playerObj;

			playerObj = &g_objectTable[g_hangarPlayerObjIdx];
			if (Xwa_Abs32(g_objectTable[objectIdx].world_x - playerObj->world_x) < 400 &&
				Xwa_Abs32(g_objectTable[objectIdx].world_y - playerObj->world_y) < 400) {
				g_hangarSceneObjects[sceneIdx].moveState = 2;
				if (sceneIdx == 0) {
					ObjectRecord* launchRef;

					g_hangarSceneObjects[sceneIdx].targetObjIdx = g_hangarDroidTargetObjIdx;
					launchRef = &g_objectTable[g_launchRefObjIdx];
					g_hangarSceneObjects[sceneIdx].targetWorldX = launchRef->world_x - 1330;
					g_hangarSceneObjects[sceneIdx].targetWorldY = launchRef->world_y - 8746;
					g_hangarSceneObjects[sceneIdx].targetWorldZ = launchRef->world_z - 4590;
					if (Xwa_Abs32(g_objectTable[objectIdx].world_x - playerObj->world_x) < 100 &&
						Xwa_Abs32(g_objectTable[objectIdx].world_y - playerObj->world_y) < 100) {
						fsfx_PlaySound(124, objectIdx, (unsigned int)g_localPlayer);
					}
					fsfx_PlaySound(123, objectIdx, (unsigned int)g_localPlayer);
				}
			}
		}

		if (g_hangarSceneObjects[sceneIdx].moveState != 0) {
			if ((unsigned int)g_hangarSceneObjects[sceneIdx].moveState > 0u &&
				(unsigned int)g_hangarSceneObjects[sceneIdx].moveState <= 2u) {
				if (Xwa_Abs32(g_objectTable[objectIdx].world_x -
							  g_hangarSceneObjects[sceneIdx].targetWorldX) <= 10 &&
					Xwa_Abs32(g_objectTable[objectIdx].world_y -
							  g_hangarSceneObjects[sceneIdx].targetWorldY) <= 10 &&
					Xwa_Abs32(g_objectTable[objectIdx].world_z -
							  g_hangarSceneObjects[sceneIdx].targetWorldZ) <= 10) {
					g_hangarSceneObjects[sceneIdx].moveState = 0;
					if (g_familyBaseDroidRouteNodes[g_hangarSceneObjects[sceneIdx].routeNodeIdx].anchorMode ==
						1) {
						g_hangarSceneObjects[sceneIdx].nextDecisionTime = decisionTime;
					} else {
						g_hangarSceneObjects[sceneIdx].nextDecisionTime =
							(uint16_t)GameRand() % 8000 + decisionTime + 4000;
					}
					if (g_hangarSceneObjects[sceneIdx].targetObjIdx == g_hangarPlayerObjIdx &&
						sceneIdx == 1) {
						switch (GameRand2() & 3) {
							case 0:
								fsfx_PlaySound(125, objectIdx, (unsigned int)g_localPlayer);
								break;
							case 1:
								fsfx_PlaySound(123, objectIdx, (unsigned int)g_localPlayer);
								break;
							case 2:
								fsfx_PlaySound(121, objectIdx, (unsigned int)g_localPlayer);
								break;
							case 3:
								fsfx_PlaySound(122, objectIdx, (unsigned int)g_localPlayer);
								break;
						}
					}
				} else {
					trig2_ctop(g_hangarSceneObjects[sceneIdx].targetWorldX - g_objectTable[objectIdx].world_x,
							   g_hangarSceneObjects[sceneIdx].targetWorldY - g_objectTable[objectIdx].world_y,
							   g_hangarSceneObjects[sceneIdx].targetWorldZ -
								   g_objectTable[objectIdx].world_z);
					if (g_objectTable[objectIdx].yaw != trig2_xyangle) {
						int yawDelta;
						int yawStep;

						yawDelta = (int16_t)(g_objectTable[objectIdx].yaw - trig2_xyangle);
						if (Xwa_Abs32(yawDelta) > 100) {
							yawStep = yawDelta / 20;
							if ((uint16_t)yawStep > 200u) {
								yawStep = 200;
							}
							g_objectTable[objectIdx].yaw =
								(Q16Angle)(g_objectTable[objectIdx].yaw - dtTicks * yawStep);
						} else {
							g_objectTable[objectIdx].yaw = trig2_xyangle;
						}
						FVIEW_calcrotatemove(g_objectTable[objectIdx].pitch, g_objectTable[objectIdx].yaw,
											 &g_objectTable[objectIdx]);
						FVIEW_calcrotateorient(g_objectTable[objectIdx].roll, g_objectTable[objectIdx].angleD,
											   &g_objectTable[objectIdx]);
					}

					if (Xwa_Abs32(g_objectTable[objectIdx].world_x -
								  g_hangarSceneObjects[sceneIdx].targetWorldX) <= 400 &&
						Xwa_Abs32(g_objectTable[objectIdx].world_y -
								  g_hangarSceneObjects[sceneIdx].targetWorldY) <= 400 &&
						Xwa_Abs32(g_objectTable[objectIdx].world_z -
								  g_hangarSceneObjects[sceneIdx].targetWorldZ) <= 400) {
						if (g_hangarSceneObjects[sceneIdx].moveSpeed != 2.0f) {
							g_hangarSceneObjects[sceneIdx].moveSpeed -= (float)dtTicks * 0.1f;
							if (g_hangarSceneObjects[sceneIdx].moveSpeed < 2.0f) {
								g_hangarSceneObjects[sceneIdx].moveSpeed = 2.0f;
							}
						}
					} else if (g_hangarSceneObjects[sceneIdx].moveState == 2) {
						if (g_hangarSceneObjects[sceneIdx].moveSpeed < 10.0f) {
							g_hangarSceneObjects[sceneIdx].moveSpeed += (float)dtTicks * 0.15000001f;
						}
					} else if (g_hangarSceneObjects[sceneIdx].moveSpeed < 4.0f) {
						g_hangarSceneObjects[sceneIdx].moveSpeed += (float)dtTicks * 0.050000001f;
					}

					{
						double dt;
						float yawRadians;

						dt = (double)dtTicks;
						yawRadians = (float)trig2_xyangle * 0.000095873722f;
						g_objectTable[objectIdx].world_x +=
							(int)(sin(yawRadians) * (double)g_hangarSceneObjects[sceneIdx].moveSpeed * dt);
						g_objectTable[objectIdx].world_y +=
							(int)(cos(yawRadians) * (double)g_hangarSceneObjects[sceneIdx].moveSpeed * dt);
						g_objectTable[objectIdx].world_z +=
							(int)(cos((float)targetPitch * 0.000095873722f) *
								  (double)g_hangarSceneObjects[sceneIdx].moveSpeed * dt);
					}
				}
			}
		} else {
			if (g_objectTable[objectIdx].yaw != g_hangarSceneObjects[sceneIdx].targetYaw) {
				int yawDelta;
				int yawStep;
				int absYawDelta;

				yawDelta = (int16_t)(g_objectTable[objectIdx].yaw - g_hangarSceneObjects[sceneIdx].targetYaw);
				yawStep = yawDelta <= 0 ? 5 * dtTicks : -5 * dtTicks;
				yawStep *= 20;
				absYawDelta = Hangar_Abs32(yawDelta);
				if (Hangar_Abs32(yawStep) > absYawDelta) {
					yawStep = -yawDelta;
				}
				if (absYawDelta > 0x8000) {
					yawStep = -yawStep;
				}
				g_objectTable[objectIdx].yaw = (Q16Angle)(g_objectTable[objectIdx].yaw + yawStep);
				FVIEW_calcrotatemove(g_objectTable[objectIdx].pitch, g_objectTable[objectIdx].yaw,
									 &g_objectTable[objectIdx]);
				FVIEW_calcrotateorient(g_objectTable[objectIdx].roll, g_objectTable[objectIdx].angleD,
									   &g_objectTable[objectIdx]);
			}

			if ((unsigned int)decisionTime >= (unsigned int)g_hangarSceneObjects[sceneIdx].nextDecisionTime) {
				int choice;
				int nextRouteNodeIdx;
				int targetObjIdx;
				ObjectRecord* targetObj;

				do {
					do {
						choice = (uint16_t)GameRand() % 6;
						nextRouteNodeIdx =
							g_familyBaseDroidRouteNodes[g_hangarSceneObjects[sceneIdx].routeNodeIdx]
								.nextNode[choice];
					} while (nextRouteNodeIdx == -1);
				} while (
					g_familyBaseDroidRouteNodes[g_hangarSceneObjects[sceneIdx].routeNodeIdx].anchorMode ==
						1 &&
					nextRouteNodeIdx == g_hangarSceneObjects[sceneIdx].prevRouteNodeIdx);
				if (g_familyBaseDroidRouteNodes[nextRouteNodeIdx].anchorMode != 3) {
					targetObjIdx = g_launchRefObjIdx;
				} else {
					targetObjIdx = g_hangarPlayerObjIdx;
				}
				g_hangarSceneObjects[sceneIdx].targetObjIdx = targetObjIdx;
				g_hangarSceneObjects[sceneIdx].prevRouteNodeIdx = g_hangarSceneObjects[sceneIdx].routeNodeIdx;
				g_hangarSceneObjects[sceneIdx].routeNodeIdx = nextRouteNodeIdx;
				targetObj = &g_objectTable[targetObjIdx];
				g_hangarSceneObjects[sceneIdx].targetWorldX =
					targetObj->world_x + g_familyBaseDroidRouteNodes[nextRouteNodeIdx].targetOffsetX;
				g_hangarSceneObjects[sceneIdx].targetWorldY =
					targetObj->world_y + g_familyBaseDroidRouteNodes[nextRouteNodeIdx].targetOffsetY;
				g_hangarSceneObjects[sceneIdx].targetWorldZ =
					targetObj->world_z + g_familyBaseDroidRouteNodes[nextRouteNodeIdx].targetOffsetZ;
				g_hangarSceneObjects[sceneIdx].targetYaw = g_familyBaseDroidRouteNodes[nextRouteNodeIdx].yaw;
				g_hangarSceneObjects[sceneIdx].moveState = 1;
				fsfx_PlaySound((sceneIdx & 3) + 142, objectIdx, (unsigned int)g_localPlayer);
			}
		}

#ifdef XWA_MODERN
		if (XwaModernFlightTiming_IsHighRate()) {
			Hangar_ModernUpdateFamilyDroidMesh(sceneIdx, objectIdx, dtTicks);
		} else {

			g_curCraft = g_objectTable[objectIdx].mobj->pCraft;
			Hangar_ModernPrepareOriginalPackedRotation(g_curCraft, 4,
													   &g_modernFamilyDroidRotations[sceneIdx][0]);
			Hangar_ModernPrepareOriginalPackedRotation(g_curCraft, 6,
													   &g_modernFamilyDroidRotations[sceneIdx][1]);
#endif
			g_curCraft = g_objectTable[objectIdx].mobj->pCraft;
			if ((GameRand() & 0xffff) % 100 < 4) {
				g_hangarSceneObjects[sceneIdx].meshRotationDir = abs(abs(GameRand() & 0xffff) & 1) - 1;
			}
			g_curCraft->meshRotation[3] =
				(uint8_t)(g_curCraft->meshRotation[3] +
						  4 * (int8_t)g_hangarSceneObjects[sceneIdx].meshRotationDir);
			if (g_hangarSceneObjects[sceneIdx].moveState == 0) {
				++g_curCraft->meshRotation[0];
			} else {
				g_curCraft->meshRotation[0] = (uint8_t)(g_curCraft->meshRotation[0] + 4u);
			}
			{
				int threshold;
				uint8_t meshRotation;

				threshold = g_hangarSceneObjects[sceneIdx].moveState != 0 ? 0x200 : 0xa000;
				meshRotation = g_curCraft->meshRotation[4];
				if ((meshRotation & 1u) != 0) {
					meshRotation -= 2;
					g_curCraft->meshRotation[4] = meshRotation;
					if (g_curCraft->meshRotation[4] == 0xf7u) {
						if ((GameRand() & 0xffff) > threshold) {
							g_curCraft->meshRotation[4] = (uint8_t)(g_curCraft->meshRotation[4] + 2u);
						}
					}
					if (g_curCraft->meshRotation[4] == 0xe1u) {
						g_curCraft->meshRotation[4] = (uint8_t)-32;
					}
				} else {
					meshRotation += 2;
					g_curCraft->meshRotation[4] = meshRotation;
					if (g_curCraft->meshRotation[4] == 4u) {
						if ((GameRand() & 0xffff) > threshold) {
							g_curCraft->meshRotation[4] = (uint8_t)(g_curCraft->meshRotation[4] - 2u);
						}
					}
					if (g_curCraft->meshRotation[4] == 32u) {
						g_curCraft->meshRotation[4] = 33u;
					}
				}

				meshRotation = g_curCraft->meshRotation[6];
				if ((meshRotation & 1u) != 0) {
					meshRotation -= 2;
					g_curCraft->meshRotation[6] = meshRotation;
					if (g_curCraft->meshRotation[6] == 0xf7u) {
						if ((GameRand() & 0xffff) > threshold) {
							g_curCraft->meshRotation[6] = (uint8_t)(g_curCraft->meshRotation[6] + 2u);
						}
					}
					if (g_curCraft->meshRotation[6] == 0xe1u) {
						g_curCraft->meshRotation[6] = (uint8_t)-32;
					}
				} else {
					meshRotation += 2;
					g_curCraft->meshRotation[6] = meshRotation;
					if (g_curCraft->meshRotation[6] == 4u) {
						if ((GameRand() & 0xffff) > threshold) {
							g_curCraft->meshRotation[6] = (uint8_t)(g_curCraft->meshRotation[6] - 2u);
						}
					}
					if (g_curCraft->meshRotation[6] == 32u) {
						g_curCraft->meshRotation[6] = 33u;
					}
				}
			}
#ifdef XWA_MODERN
		}
#endif

		{
			int routeActive;

			routeActive = g_hangarSceneObjects[sceneIdx].moveState != 0 && (decisionTime & 0x100) == 0;
			switch (sceneIdx) {
				case 0:
					g_hangarDroid0RouteActive = routeActive;
					break;
				case 1:
					g_hangarDroid1RouteActive = routeActive;
					break;
			}
		}

		++sceneIdx;
	}

#ifdef XWA_MODERN
	if (XwaModernFlightTiming_IsHighRate()) {
		Hangar_ModernUpdateCraneRotation(dtTicks, 0x390u);
	} else {
		Hangar_ModernPrepareOriginalCraneRotation();
#endif
		g_curCraft = g_objectTable[g_hangarAnimatedDroidObjIdx].mobj->pCraft;
		if ((g_curCraft->meshRotation[0] & 1u) != 0) {
			if ((g_hangarSceneObjects[0].routeNodeIdx & 1) != 0) {
				g_curCraft->meshRotation[0] = (uint8_t)(g_curCraft->meshRotation[0] - 2u);
			} else {
				g_curCraft->meshRotation[0] = (uint8_t)(g_curCraft->meshRotation[0] + 2u);
			}
			if ((uint16_t)GameRand() < 0x390u) {
				g_curCraft->meshRotation[0] ^= 1u;
			}
		} else {
			if ((uint16_t)GameRand() < 0x200u) {
				g_curCraft->meshRotation[0] ^= 1u;
			}
		}
#ifdef XWA_MODERN
	}
#endif
}

// FUNCTION: XWA 0x460650
void Hangar_ServicePlayerCraft(int dtTicks) {
	ModelIndex modelIndex;
	uint16_t systemFlags;
	CraftData* craft;
	int launcherIdx;
	int allSystemsWorking;
	int rearmedAny;
	int rearmIncomplete;
	int countermeasureRestored;
	int warheadsComplete;

	allSystemsWorking = 0;
	countermeasureRestored = 0;
	warheadsComplete = 0;
	rearmedAny = 0;
	rearmIncomplete = 0;

	if (g_hangarServiceCooldown > 0) {
		g_hangarServiceCooldown -= dtTicks;
		if (g_hangarServiceCooldown < 0) {
			g_hangarServiceCooldown = 0;
		}
		return;
	}

	craft = g_objectTable[g_hangarPlayerObjIdx].mobj->pCraft;
	systemFlags = craft->systemFlags;
	modelIndex = craft->modelIndex;

	if ((systemFlags & 1u) != 0) {
		int boundFlightGroupIdx;
		uint8_t status1;
		uint8_t status2;

		craft->shieldFront = g_modelDefs[modelIndex].shieldStrength;
		craft->shieldRear = g_modelDefs[modelIndex].shieldStrength;

		boundFlightGroupIdx = g_players[g_localPlayer].boundFlightGroupIdx;
		status1 = g_missionFlightGroups[boundFlightGroupIdx].fg.status1;
		status2 = g_missionFlightGroups[boundFlightGroupIdx].fg.status2;
		if (status1 == 3 || status2 == 3) {
			craft->shieldFront = 0;
			craft->shieldRear = 0;
			craft->systemFlags = systemFlags ^ 1u;
		} else if (status1 == 4 || status2 == 4) {
			craft->shieldFront >>= 1;
			craft->shieldRear >>= 1;
			craft->systemFlags = systemFlags ^ 1u;
		} else if (status1 == 7 || status2 == 7) {
			craft->shieldFront = 0;
			craft->shieldRear = 0;
		} else if (status1 == 19 || status2 == 19 || status1 == 13 || status2 == 13) {
			craft->shieldFront >>= 1;
			craft->shieldRear >>= 1;
		} else if (status1 == 18 || status2 == 18 || status1 == 12 || status2 == 12) {
			craft->shieldFront *= 2;
			craft->shieldRear *= 2;
		}
	}

	{
		int laserSlotIdx;

		laserSlotIdx = 0;
		craft->hullDamage = 0;
		craft->subsystemDamage = 0;
		if (craft->laserSlotCount > 0) {
			do {
				if (craft->warheadData[laserSlotIdx].weaponType == 1 ||
					craft->warheadData[laserSlotIdx].weaponType == 2) {
					craft->warheadData[laserSlotIdx].laserCharge = 127;
				}
				++laserSlotIdx;
			} while (laserSlotIdx < (int)craft->laserSlotCount);
		}
	}

	if ((craft->systemFlags & 0x100u) != 0) {
		craft->beamPresent = 9999;
	} else {
		craft->beamPresent = 0;
	}

	{
		launcherIdx = 0;
		if (craft->warheadLauncherCount > 0) {
			do {
				if (craft->warheadSlotTypeIds[launcherIdx] != 0) {
					int firstSlot;
					int lastSlot;

					lastSlot = g_modelDefs[modelIndex].warheadLauncherLastSlot[launcherIdx];
					firstSlot = g_modelDefs[modelIndex].warheadLauncherFirstSlot[launcherIdx];
					if (firstSlot <= lastSlot) {
						int slotIdx;

						for (slotIdx = firstSlot; slotIdx <= lastSlot; ++slotIdx) {
							uint16_t warheadCatalogIdx;
							uint16_t baseMaxCount;
							uint16_t adjustedMaxCount;
							const uint16_t* warheadType;
							uint8_t status1;
							uint8_t status2;

							warheadCatalogIdx = 0;
							warheadType = g_warheadTypeIds;
							while ((intptr_t)warheadType < (intptr_t)(g_warheadTypeIds + 11) &&
								   craft->warheadSlotTypeIds[launcherIdx] != *warheadType) {
								++warheadType;
								++warheadCatalogIdx;
							}
							if (launcherIdx == 1) {
								(void)GetModelIndexFromType(OBJ_MissileBoat);
							}

							baseMaxCount = (uint16_t)MATH2_fraction(
								g_modelDefs[modelIndex].warheadLauncherValue[launcherIdx],
								g_warheadAmmoCounts[warheadCatalogIdx]);
							adjustedMaxCount = baseMaxCount;
							if (baseMaxCount == 0) {
								baseMaxCount = 1;
								adjustedMaxCount = 1;
							}

							status1 =
								g_missionFlightGroups[g_objectTable[g_hangarPlayerObjIdx].flightGroupIdx]
									.fg.status1;
							if (status1 == 1 ||
								(status2 =
									 g_missionFlightGroups[g_objectTable[g_hangarPlayerObjIdx].flightGroupIdx]
										 .fg.status2) == 1) {
								adjustedMaxCount = (uint16_t)(2 * baseMaxCount);
							} else if (status1 == 2 || status2 == 2) {
								adjustedMaxCount >>= 1;
							}
							if (adjustedMaxCount == 0) {
								adjustedMaxCount = 1;
							}
							if (adjustedMaxCount > 9u &&
								g_objectTable[g_hangarPlayerObjIdx].objectType != OBJ_MissileBoat) {
								adjustedMaxCount = 9;
							}

							if (craft->warheadData[slotIdx].count < adjustedMaxCount) {
								uint8_t count;

								count = (uint8_t)(craft->warheadData[slotIdx].count + 1u);
								craft->warheadData[slotIdx].count = count;
								rearmedAny = 1;
								if (count < adjustedMaxCount) {
									rearmIncomplete = 1;
								}

								if (warheadCatalogIdx == 291u || warheadCatalogIdx == 292u ||
									warheadCatalogIdx == 293u || warheadCatalogIdx == 294u) {
									fsfx_PlaySound(136, g_hangarPlayerObjIdx, g_localPlayer);
								} else {
									fsfx_PlaySound(137, g_hangarPlayerObjIdx, g_localPlayer);
								}
							}
							craft->warheadData[slotIdx].laserCharge = 127;
						}
					}
				}
				++launcherIdx;
			} while (launcherIdx < (int)craft->warheadLauncherCount);
		}
	}

	if (craft->cmTypeId != 0) {
		uint8_t countermeasureCount;

		countermeasureCount = g_modelDefs[modelIndex].countermeasureCount;
		if (craft->cmAmmoCount != countermeasureCount) {
			craft->cmAmmoCount = countermeasureCount;
			countermeasureRestored = 1;
		}
	}

	{
		int systemIdx;
		int systemMask;

		systemIdx = 0;
		systemMask = 1;
		while (systemIdx < 10) {
			if ((craft->systemFlags & systemMask) != 0 &&
				((craft->workingSubsystems & systemMask) == 0 || craft->systemHealth[systemIdx] == 0)) {
				craft->workingSubsystems |= systemMask;
				craft->systemHealth[systemIdx] = 100;
				g_msgArgTable[0] = g_subsystemMessageArgById[systemIdx];
				g_msgArgTable[1] = MSG_REPAIRED;
				msg_emitInFlightMessage(MSG_SYSTEMCOND, g_localPlayer);
				fsfx_PlaySound(125, 0xffffu, g_localPlayer);
				if (craft->workingSubsystems == 1023u) {
					allSystemsWorking = 1;
				}
				break;
			}

			systemMask <<= 1;
			++systemIdx;
		}
	}

	if (rearmedAny && !rearmIncomplete) {
		warheadsComplete = 1;
	}
	if (countermeasureRestored && allSystemsWorking && warheadsComplete) {
		int soundChoice;

		soundChoice = (uint16_t)GameRand2() % 3;
		switch (soundChoice) {
			case 0:
				fsfx_PlaySound(2634, 0xffffu, g_localPlayer);
				break;
			case 1:
				fsfx_PlaySound(2629, 0xffffu, g_localPlayer);
				break;
			case 2:
				fsfx_PlaySound(2632, 0xffffu, g_localPlayer);
				break;
		}
	} else if (!countermeasureRestored && allSystemsWorking && warheadsComplete) {
		int soundChoice;

		soundChoice = (uint16_t)GameRand2() % 2;
		switch (soundChoice) {
			case 0:
				fsfx_PlaySound(2632, 0xffffu, g_localPlayer);
				break;
			case 1:
				fsfx_PlaySound(2629, 0xffffu, g_localPlayer);
				break;
		}
	} else if (countermeasureRestored && !allSystemsWorking && warheadsComplete) {
		int soundChoice;

		soundChoice = (uint16_t)GameRand2() % 2;
		switch (soundChoice) {
			case 0:
				fsfx_PlaySound(2634, 0xffffu, g_localPlayer);
				break;
			case 1:
				fsfx_PlaySound(2632, 0xffffu, g_localPlayer);
				break;
		}
	} else if (countermeasureRestored && allSystemsWorking && !warheadsComplete) {
		int soundChoice;

		soundChoice = (uint16_t)GameRand2() % 2;
		switch (soundChoice) {
			case 0:
				fsfx_PlaySound(2634, 0xffffu, g_localPlayer);
				break;
			case 1:
				fsfx_PlaySound(2629, 0xffffu, g_localPlayer);
				break;
		}
	} else if (countermeasureRestored) {
		fsfx_PlaySound(2634, 0xffffu, g_localPlayer);
	} else if (allSystemsWorking) {
		fsfx_PlaySound(2629, 0xffffu, g_localPlayer);
	} else if (warheadsComplete) {
		fsfx_PlaySound(2632, 0xffffu, g_localPlayer);
	}

	g_hangarServiceCooldown = 236;
}

// FUNCTION: XWA 0x462070
void Hangar_DrawProvingGroundStatusPanel(void) {
	enum {
		HANGAR_MENU_TITLE_HALL_OF_FAME = 10,
		HANGAR_MENU_ITEM_SELECT_CHALLENGE_FIRST = 40,
		HANGAR_MENU_ITEM_SELECT_CHALLENGE_NONE = 48,
		HANGAR_YARD_SCORE_ROWS_PER_CHALLENGE = 10
	};
	char text[100];
	int scoreHeaderY;
	int16_t lineStep;
	int savedLineStep;
	int rowY;
	int challengeMode;
	int16_t panelTextWidth;
	ModelIndex modelIndex;
	HANGAR_HUD_PANE_PUSH(XWA_HUD_PANE_CMD, dstX, dstY, g_hudCmdPanelWidth, g_hudCmdPanelHeight);

	challengeMode = g_yardChallengeMode;
	rowY = 18;
	FlightText_SetFontTier(0);
	FlightText_SetShadowEnabled(1u);
	g_curCraft = g_objectTable[g_hangarPlayerObjIdx].mobj->pCraft;

	if (g_useHardware3D) {
		lineStep = (uint8_t)g_flightFontScale + 1;
	} else {
		lineStep = (uint8_t)g_flightFontLineHeight + 2;
	}

	panelTextWidth = (int16_t)(g_hudCmdPanelWidth - 20);
	if (!g_useHardware3D) {
		FlightSw_SetRenderTarget(g_hudCmdTexPixels, (uint16_t)g_hudCmdPanelWidth,
								 (uint16_t)g_hudCmdPanelHeight,
								 g_hudCmdPanelWidth * g_flight16bppBytesPerPixel);
	} else {
		FlightText_SetRenderOffset(dstX, dstY);
	}

	FlightText_SetClipRect(0, 0, (uint16_t)g_hudCmdPanelWidth, (uint16_t)g_hudCmdPanelHeight);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	if (!g_useHardware3D) {
		g_flightFillClipRectFn();
	}

	switch (g_yardChallengeMode) {
		case 0:
			strcpy(text, g_strHangarMenuItems[HANGAR_MENU_ITEM_SELECT_CHALLENGE_FIRST + 0]);
			break;
		case 1:
			strcpy(text, g_strHangarMenuItems[HANGAR_MENU_ITEM_SELECT_CHALLENGE_FIRST + 1]);
			break;
		case 2:
			strcpy(text, g_strHangarMenuItems[HANGAR_MENU_ITEM_SELECT_CHALLENGE_FIRST + 2]);
			break;
		case 3:
			strcpy(text, g_strHangarMenuItems[HANGAR_MENU_ITEM_SELECT_CHALLENGE_FIRST + 3]);
			break;
		case 4:
			strcpy(text, g_strHangarMenuItems[HANGAR_MENU_ITEM_SELECT_CHALLENGE_FIRST + 4]);
			break;
		case 5:
			strcpy(text, g_strHangarMenuItems[HANGAR_MENU_ITEM_SELECT_CHALLENGE_FIRST + 5]);
			break;
		case 6:
			strcpy(text, g_strHangarMenuItems[HANGAR_MENU_ITEM_SELECT_CHALLENGE_FIRST + 6]);
			break;
		case 7:
			strcpy(text, g_strHangarMenuItems[HANGAR_MENU_ITEM_SELECT_CHALLENGE_FIRST + 7]);
			break;
		default:
			strcpy(text, g_strHangarMenuItems[HANGAR_MENU_ITEM_SELECT_CHALLENGE_NONE]);
			break;
	}
	FlightText_SetCursor(20, 18);
	FlightText_SetColor(0x3au);
	FlightText_DrawStringCentered(text);

	modelIndex = GetModelIndexFromType(g_objectTable[g_players[g_localPlayer].objectIndex].objectType);
	strcpy(text, g_strHangarMiscStrings[HANGAR_MISC_YOUR_CRAFT]);
	savedLineStep = (uint16_t)lineStep;
	rowY = (uint16_t)lineStep + 18;
	strcpy(text, g_modelDefs[modelIndex].nameAlt);
	FlightText_SetCursor(20, rowY);
	FlightText_DrawStringCentered(text);

	rowY += (uint16_t)lineStep;
	FlightText_SetColor(0x36u);
	rowY += (uint16_t)lineStep;
	FlightText_SetCursor(20, rowY);
	strcpy(text, g_strHangarMenuTitles[HANGAR_MENU_TITLE_HALL_OF_FAME]);
	FlightText_DrawStringCentered(text);

	if (g_yardCurrentCraftScoreTable != NULL) {
		uint16_t rank;
		int16_t columnX;

		rank = 0;
		columnX = 20;
		scoreHeaderY = rowY;
		do {
			int score;
			int minutes;
			int seconds;
			uint16_t rowIndex;
			int cursorX;

			cursorX = savedLineStep;
			if (rank == 5) {
				rowY = scoreHeaderY;
				columnX = (int16_t)(columnX + panelTextWidth / 2);
			}

			rowY += cursorX;
			cursorX = columnX;
			rowIndex = rank;
			score = g_yardCurrentCraftScoreTable->scores[challengeMode][rowIndex];
			minutes = score / 60;
			seconds = score % 60;
			FlightText_SetCursor(cursorX, rowY);
			if (g_yardFinishPlacementResultCode != rowIndex + 1) {
				FlightText_SetColor(0x36u);
			} else {
				FlightText_SetColor(0x3au);
			}

			rank++;
			FlightText_DrawDecimalNumber(rank, 1u, 1u);
			if (g_flightResolutionMode >= 2) {
				FlightText_SetCursor(cursorX + 20, rowY);
			} else {
				FlightText_SetCursor(cursorX + 10, rowY);
			}
			FlightText_DrawDecimalNumber((uint16_t)minutes, 2u, 1u);
			g_flightDrawCharFn(':');
			FlightText_DrawDecimalNumber((uint16_t)seconds, 2u, 2u);
			if (g_flightResolutionMode >= 2) {
				cursorX += 70;
			} else {
				cursorX += 40;
			}
			FlightText_SetCursor(cursorX, rowY);
			FlightText_DrawString(g_yardCurrentCraftScoreTable->pilotNames[challengeMode][rowIndex]);

		} while (rank < 10u);
	}

	if (!g_useHardware3D) {
		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		Blit16ToFlightSurface(g_hudCmdTexPixels, g_flightColorEscapeBypassChar, 0, 0, (uint16_t)dstX,
							  (uint16_t)dstY, (uint16_t)g_hudCmdPanelWidth, (uint16_t)g_hudCmdPanelHeight,
							  (uint16_t)(g_flight16bppBytesPerPixel * g_hudCmdPanelWidth));
	} else {
		FlightText_SetRenderOffset(0, 0);
	}
	HANGAR_HUD_PANE_POP();
}

// FUNCTION: XWA 0x461A90
void Hangar_DrawMenu(void) {
	if (g_filmPlaybackMode && g_filmOverlayMfdVisible) {
		Hud_DrawHangarFilmMfdOverlay();
		return;
	}

	Hangar_DrawMenuColumn(0, 1);

	if (g_provingGroundsModeActive ||
		(g_hangarMenuCursor[0] > 1 && !g_hangarMissionResolved &&
		 g_hangarPlayerObjIdx != g_hangarShuttleState.objectIdx &&
		 g_missionFlightRuntimeState
				 .teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff][TEAM_GOAL_PRIMARY] == 0 &&
		 g_missionFlightRuntimeState
				 .teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff][TEAM_GOAL_SECONDARY] != 1)) {
		Hangar_DrawMenuColumn(1, 2);
	} else {
		HANGAR_HUD_PANE_PUSH(XWA_HUD_PANE_MFD_RIGHT_BODY, g_screenWidth - g_hudMfdPaneWidth,
							 g_screenHeight - g_hudMfdPaneHeight, g_hudMfdPaneWidth, g_hudMfdPaneHeight);
		Mfd_DrawMissionGoalsPage(2, g_hudMfdRightTexPixels);
		HANGAR_HUD_PANE_POP();
	}
}

// FUNCTION: XWA 0x461B30
void Hangar_DrawMenuColumn(int level, int side) {
	int16_t lineStep;
	unsigned int savedLineStep;
	int visibleCount;
	int itemIndex;
	int endItem;
	HANGAR_HUD_PANE_PUSH(side == 1 ? XWA_HUD_PANE_MFD_LEFT_BODY : XWA_HUD_PANE_MFD_RIGHT_BODY,
						 side == 1 ? 0 : g_screenWidth - g_hudMfdPaneWidth,
						 g_screenHeight - g_hudMfdPaneHeight, g_hudMfdPaneWidth, g_hudMfdPaneHeight);

	if (g_useHardware3D) {
		if (side == 1) {
			FlightText_SetRenderOffset(0, (int16_t)(g_screenHeight - g_hudMfdPaneHeight + 5));
		} else {
			FlightText_SetRenderOffset((int16_t)(g_screenWidth - g_hudMfdPaneWidth),
									   (int16_t)(g_screenHeight - g_hudMfdPaneHeight + 5));
		}
	} else if (side == 1) {
		FlightSw_SetRenderTarget(g_hudMfdLeftTexPixels, (uint16_t)g_hudMfdPaneWidth,
								 (uint16_t)g_hudMfdPaneHeight,
								 (uint16_t)g_hudMfdPaneWidth * g_flight16bppBytesPerPixel);
	} else {
		FlightSw_SetRenderTarget(g_hudMfdRightTexPixels, (uint16_t)g_hudMfdPaneWidth,
								 (uint16_t)g_hudMfdPaneHeight,
								 (uint16_t)g_hudMfdPaneWidth * g_flight16bppBytesPerPixel);
	}

	FlightText_SetShadowEnabled(1u);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetClipRect(0, 0, (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight);
	if (!g_useHardware3D) {
		g_flightFillClipRectFn();
	}
	if (g_useHardware3D) {
		lineStep = (int16_t)(uint8_t)g_flightFontScale;
		++lineStep;
	} else {
		lineStep = (int16_t)((uint8_t)g_flightFontLineHeight + 1u);
	}

	FlightText_SetFontTier(0);
	FlightText_SetCursor(0, 0);
	if (level == g_hangarMenuLevel && g_inHangarReady) {
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		FlightText_SetColor(0x3au);
	} else {
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		FlightText_SetColor(0x36u);
	}
	FlightText_DrawStringCentered(g_hangarMenuColTitle[level]);
	FlightText_SetCursor(-1, -1);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);

	visibleCount = g_hangarMenuItemCount[level];
	if (visibleCount >= 8) {
		visibleCount = 8;
	}
	itemIndex = g_hangarMenuScroll[level];
	endItem = itemIndex + visibleCount;

	if (itemIndex < endItem) {
		savedLineStep = (uint16_t)lineStep;
		do {
			int highlighted;
			char text[52];

			highlighted = 0;
			if (g_hangarMenuCursor[level] == itemIndex && g_inHangarReady) {
				if (!g_provingGroundsModeActive) {
					highlighted = 1;
				} else if (level == 0) {
					if (g_hangarMenuCursor[0] != 3 || !g_hangarMenuLevel) {
						highlighted = 1;
					}
				} else {
					if (g_hangarMenuCursor[0] == 3 || g_hangarMenuCursor[0] == 5) {
						highlighted = 1;
					}
				}
			}

			FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
			if (highlighted) {
				FlightText_SetColor(0x3au);
			} else if (g_hangarMenuItemDisabled[level][itemIndex]) {
				FlightText_SetColor(0x2du);
			} else {
				FlightText_SetColor(0x36u);
			}
			FlightText_SetCursor(
				0, (int16_t)(lineStep * (int16_t)((uint16_t)((uint16_t)itemIndex -
															 (uint16_t)g_hangarMenuScroll[level] + 2u))));

			if (!g_provingGroundsModeActive || level != 1 || g_hangarMenuCursor[0] == 3 ||
				g_hangarMenuCursor[0] == 5) {
				if (highlighted) {
					int blinkSpaced;

					if (level == g_hangarMenuLevel) {
						blinkSpaced = (g_gameTime >> 5) & 1;
					} else {
						blinkSpaced = 1;
					}

					switch (blinkSpaced) {
						case 0:
							strcpy(text, ">");
							break;
						case 1:
							strcpy(text, "> ");
							break;
					}
					strcat(text, g_hangarMenuItemLabels[level][itemIndex]);
					switch (blinkSpaced) {
						case 0:
							strcat(text, "<");
							break;
						case 1:
							strcat(text, " <");
							break;
					}
					FlightText_DrawStringCentered(text);
					lineStep = savedLineStep;
				} else {
					FlightText_DrawStringCentered(g_hangarMenuItemLabels[level][itemIndex]);
				}
			} else {
				FlightText_DrawString(g_hangarMenuItemLabels[level][itemIndex]);
			}

			++itemIndex;
		} while (itemIndex < endItem);

		lineStep = savedLineStep;
	}

	FlightText_SetColor(0x36u);
	if (g_hangarMenuScroll[level] > 0) {
		FlightText_SetCursor(0, lineStep);
		FlightText_DrawStringCentered("^");
	}
	if (endItem < g_hangarMenuItemCount[level]) {
		FlightText_SetCursor(0, (int16_t)(10 * lineStep));
		FlightText_DrawStringCentered("v");
	}

	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
		HANGAR_HUD_PANE_POP();
		return;
	}

	FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
	if (side == 1) {
		Blit16ToFlightSurface(g_hudMfdLeftTexPixels, g_flightColorEscapeBypassChar, 0, 0, 0,
							  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight + 5),
							  (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
							  (uint16_t)(g_flight16bppBytesPerPixel * g_hudMfdPaneWidth));
	} else {
		Blit16ToFlightSurface(g_hudMfdRightTexPixels, g_flightColorEscapeBypassChar, 0, 0,
							  (uint16_t)(g_screenWidth - g_hudMfdPaneWidth),
							  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight + 5),
							  (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
							  (uint16_t)(g_flight16bppBytesPerPixel * g_hudMfdPaneWidth));
	}
	HANGAR_HUD_PANE_POP();
}

// FUNCTION: XWA 0x4619F0
void Hangar_PopulateProvingGroundMenu(int level) {
	int row;

	strcpy(g_hangarMenuColTitle[level], g_strHangarMenuTitles[9]);

	for (row = 0; row < 8; ++row) {
		strcpy(g_hangarMenuItemLabels[level][row], g_strProvingGroundDescs[8 * g_yardChallengeMode + row]);
	}

	g_hangarMenuItemCount[level] = 8;
}

// FUNCTION: XWA 0x461740
void Hangar_BuildCraftSelectMenu(void) {
	int listIndex;

	strcpy(g_hangarMenuColTitle[1], g_strHangarMenuTitles[4]);
	g_hangarMenuItemCount[1] = 0;

	for (listIndex = 0; listIndex < HANGAR_CRAFT_LIST_COUNT; ++listIndex) {
		uint16_t craftType;

		craftType = g_hangarCraftList[listIndex];
		if (craftType != OBJ_None) {
			const char* nameAlt;

			nameAlt = g_modelDefs[(uint16_t)GetModelIndexFromType(craftType)].nameAlt;
			if (nameAlt != NULL) {
				strcpy(g_hangarMenuItemLabels[1][g_hangarMenuItemCount[1]], nameAlt);
				++g_hangarMenuItemCount[1];
			}
		}

		if (craftType == g_objectTable[g_players[g_localPlayer].objectIndex].objectType) {
			g_hangarMenuCursor[1] = listIndex;
		}
	}
}

// FUNCTION: XWA 0x461840
void Hangar_BuildCounterSelectMenu(void) {
	enum {
		HANGAR_MENU_ITEM_COUNTER_NONE = 30,
		HANGAR_MENU_ITEM_COUNTER_CHAFF = 31,
		HANGAR_MENU_ITEM_COUNTER_FLARE = 32,
		HANGAR_MENU_ITEM_COUNTER_MINE = 33,
	};
	int listIndex;

	g_curCraft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
	strcpy(g_hangarMenuColTitle[1], g_strHangarMenuTitles[7]);
	g_hangarMenuCursor[1] = 0;
	g_hangarMenuItemCount[1] = 0;

	for (listIndex = 0; listIndex < HANGAR_COUNTERMEASURE_LIST_COUNT; ++listIndex) {
		int16_t cmTypeId;

		cmTypeId = g_hangarCountermeasureTypeList[listIndex];
		if (cmTypeId != 0 || listIndex == 0) {
			switch ((uint16_t)cmTypeId) {
				case 0:
					strcpy(g_hangarMenuItemLabels[1][g_hangarMenuItemCount[1]],
						   g_strHangarMenuItems[HANGAR_MENU_ITEM_COUNTER_NONE]);
					break;
				case 1:
					strcpy(g_hangarMenuItemLabels[1][g_hangarMenuItemCount[1]],
						   g_strHangarMenuItems[HANGAR_MENU_ITEM_COUNTER_CHAFF]);
					break;
				case 2:
					strcpy(g_hangarMenuItemLabels[1][g_hangarMenuItemCount[1]],
						   g_strHangarMenuItems[HANGAR_MENU_ITEM_COUNTER_FLARE]);
					break;
				case 3:
					strcpy(g_hangarMenuItemLabels[1][g_hangarMenuItemCount[1]],
						   g_strHangarMenuItems[HANGAR_MENU_ITEM_COUNTER_MINE]);
					break;
				default:
					break;
			}

			++g_hangarMenuItemCount[1];
			if (cmTypeId == g_curCraft->cmTypeId) {
				g_hangarMenuCursor[1] = listIndex;
			}
		}
	}
}

// FUNCTION: XWA 0x460CB0
char Hangar_BuildMenu(int resetCursor) {
	enum {
		HANGAR_MENU_TITLE_MISSION_OVER = 0,
		HANGAR_MENU_TITLE_MISSION_FAIL = 1,
		HANGAR_MENU_TITLE_CRAFT_MENU = 2,
		HANGAR_MENU_TITLE_PROVING_GROUND = 3,
		HANGAR_MENU_TITLE_SELECT_WARHEAD = 5,
		HANGAR_MENU_TITLE_SELECT_BEAM = 6,
		HANGAR_MENU_TITLE_SELECT_CHALLENGE = 8,

		HANGAR_MENU_ITEM_MISSION_DEBRIEF = 0,
		HANGAR_MENU_ITEM_MISSION_EMPTY = 1,
		HANGAR_MENU_ITEM_MISSION_REFLY = 2,
		HANGAR_MENU_ITEM_SWITCH_CRAFT = 10,
		HANGAR_MENU_ITEM_LOAD_WARHEADS = 11,
		HANGAR_MENU_ITEM_INSTALL_BEAMS = 12,
		HANGAR_MENU_ITEM_INSTALL_COUNTERS = 13,
		HANGAR_MENU_ITEM_CRAFT_LAUNCH = 14,
		HANGAR_MENU_ITEM_CRAFT_RETURN = 15,
		HANGAR_MENU_ITEM_CRAFT_DEBRIEF = 16,
		HANGAR_MENU_ITEM_MB_LOAD_PRIMARY = 17,
		HANGAR_MENU_ITEM_MB_LOAD_SECONDARY = 18,
		HANGAR_MENU_ITEM_CRAFT_EMPTY = 19,
		HANGAR_MENU_ITEM_PG_SELECT_CHALL = 20,
		HANGAR_MENU_ITEM_PG_SELECT_CRAFT = 21,
		HANGAR_MENU_ITEM_PG_EMPTY = 22,
		HANGAR_MENU_ITEM_PG_INSTRUCTIONS = 23,
		HANGAR_MENU_ITEM_PG_EMPTY2 = 24,
		HANGAR_MENU_ITEM_PG_LAUNCH = 25,
		HANGAR_MENU_ITEM_PG_RETURN = 26,
		HANGAR_MENU_ITEM_CHALLENGE_FIRST = 40
	};

	int result;

	result = 0;

	if (g_provingGroundsModeActive) {
		int selectedOption;

		g_yardHighScoreTable = Yard_LoadOrCreateHighScoreTable();
		g_yardCurrentCraftScoreTable = Yard_FindCraftScoreTableByObjectType(
			g_objectTable[g_players[g_localPlayer].objectIndex].objectType, g_yardHighScoreTable);

		if (g_hangarMissionResolved ||
			g_missionFlightRuntimeState
					.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff][TEAM_GOAL_PRIMARY] != 0) {
			strcpy(g_hangarMenuColTitle[0], g_strHangarMenuTitles[HANGAR_MENU_TITLE_MISSION_OVER]);
			strcpy(g_hangarMenuItemLabels[0][0], g_strHangarMenuItems[HANGAR_MENU_ITEM_MISSION_DEBRIEF]);
			strcpy(g_hangarMenuItemLabels[0][1], g_strHangarMenuItems[HANGAR_MENU_ITEM_MISSION_EMPTY]);
			strcpy(g_hangarMenuItemLabels[0][2], g_strHangarMenuItems[HANGAR_MENU_ITEM_MISSION_REFLY]);
			g_hangarMenuItemCount[0] = 3;
			if (g_missionFlightRuntimeState
					.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff][TEAM_GOAL_PRIMARY] == 1) {
				result = (int)strlen(g_strHangarMenuTitles[HANGAR_MENU_TITLE_MISSION_FAIL]) + 1;
				memcpy(g_hangarMenuColTitle[1], g_strHangarMenuTitles[HANGAR_MENU_TITLE_MISSION_FAIL],
					   (unsigned int)result);
			} else {
				strcpy(g_hangarMenuColTitle[1], g_strHangarMenuTitles[HANGAR_MENU_TITLE_MISSION_OVER]);
				result = 0;
			}
			g_hangarMenuItemCount[1] = 0;
			return (char)result;
		}

		strcpy(g_hangarMenuColTitle[0], g_strHangarMenuTitles[HANGAR_MENU_TITLE_PROVING_GROUND]);
		strcpy(g_hangarMenuItemLabels[0][0], g_strHangarMenuItems[HANGAR_MENU_ITEM_PG_LAUNCH]);
		strcpy(g_hangarMenuItemLabels[0][1], g_strHangarMenuItems[HANGAR_MENU_ITEM_PG_RETURN]);
		strcpy(g_hangarMenuItemLabels[0][2], g_strHangarMenuItems[HANGAR_MENU_ITEM_PG_EMPTY]);
		strcpy(g_hangarMenuItemLabels[0][3], g_strHangarMenuItems[HANGAR_MENU_ITEM_PG_SELECT_CHALL]);
		strcpy(g_hangarMenuItemLabels[0][5], g_strHangarMenuItems[HANGAR_MENU_ITEM_PG_SELECT_CRAFT]);
		strcpy(g_hangarMenuItemLabels[0][4], g_strHangarMenuItems[HANGAR_MENU_ITEM_PG_INSTRUCTIONS]);
		strcpy(g_hangarMenuItemLabels[0][6], g_strHangarMenuItems[HANGAR_MENU_ITEM_PG_EMPTY2]);
		g_hangarMenuItemCount[0] = 7;

		if (g_hangarMenuCursor[0] != 3) {
			if (g_hangarMenuCursor[0] != 5) {
				Hangar_PopulateProvingGroundMenu(1);
				g_hangarMenuCursor[1] = result;
				selectedOption = result;
			} else {
				Hangar_BuildCraftSelectMenu();
				selectedOption = g_hangarMenuCursor[1];
			}
		} else {
			strcpy(g_hangarMenuColTitle[1], g_strHangarMenuTitles[HANGAR_MENU_TITLE_SELECT_CHALLENGE]);
			strcpy(g_hangarMenuItemLabels[1][0], g_strHangarMenuItems[HANGAR_MENU_ITEM_CHALLENGE_FIRST + 0]);
			strcpy(g_hangarMenuItemLabels[1][1], g_strHangarMenuItems[HANGAR_MENU_ITEM_CHALLENGE_FIRST + 1]);
			strcpy(g_hangarMenuItemLabels[1][2], g_strHangarMenuItems[HANGAR_MENU_ITEM_CHALLENGE_FIRST + 2]);
			strcpy(g_hangarMenuItemLabels[1][3], g_strHangarMenuItems[HANGAR_MENU_ITEM_CHALLENGE_FIRST + 3]);
			strcpy(g_hangarMenuItemLabels[1][4], g_strHangarMenuItems[HANGAR_MENU_ITEM_CHALLENGE_FIRST + 4]);
			strcpy(g_hangarMenuItemLabels[1][5], g_strHangarMenuItems[HANGAR_MENU_ITEM_CHALLENGE_FIRST + 5]);
			strcpy(g_hangarMenuItemLabels[1][6], g_strHangarMenuItems[HANGAR_MENU_ITEM_CHALLENGE_FIRST + 6]);
			strcpy(g_hangarMenuItemLabels[1][7], g_strHangarMenuItems[HANGAR_MENU_ITEM_CHALLENGE_FIRST + 7]);
			g_hangarMenuItemCount[1] = 8;
			g_hangarMenuCursor[1] = g_yardChallengeMode;
			selectedOption = g_yardChallengeMode;
		}

		if (selectedOption < g_hangarMenuScroll[1]) {
			g_hangarMenuScroll[1] = selectedOption;
		}
		if (selectedOption > g_hangarMenuScroll[1] + 7) {
			selectedOption -= 7;
			g_hangarMenuScroll[1] = selectedOption;
		}
		return (char)selectedOption;
	}

	if (!g_hangarMissionResolved && g_hangarPlayerObjIdx != g_hangarShuttleState.objectIdx) {
		int localPlayerIdx;
		int localPlayerIff;

		localPlayerIdx = g_localPlayer;
		localPlayerIff = (uint16_t)g_players[localPlayerIdx].playerIff;
		if (g_missionFlightRuntimeState.teamGoalStatus[localPlayerIff][TEAM_GOAL_PRIMARY] != 1 &&
			g_missionFlightRuntimeState.teamGoalStatus[localPlayerIff][TEAM_GOAL_SECONDARY] != 1) {
			int objectIndex;
			ObjectRecord* playerObj;
			int scrollLimit;

			strcpy(g_hangarMenuColTitle[0], g_strHangarMenuTitles[HANGAR_MENU_TITLE_CRAFT_MENU]);
			strcpy(g_hangarMenuItemLabels[0][0], g_strHangarMenuItems[HANGAR_MENU_ITEM_CRAFT_LAUNCH]);
			if (g_hangarSourceObjIdx == 0xffff && g_hangarReturnToFlightAvailable) {
				strcpy(g_hangarMenuItemLabels[0][1], g_strHangarMenuItems[HANGAR_MENU_ITEM_CRAFT_RETURN]);
			} else {
				strcpy(g_hangarMenuItemLabels[0][1], g_strHangarMenuItems[HANGAR_MENU_ITEM_CRAFT_DEBRIEF]);
				g_flightReturnToMissionSetupRequested = 0;
			}
			strcpy(g_hangarMenuItemLabels[0][2], g_strHangarMenuItems[HANGAR_MENU_ITEM_CRAFT_EMPTY]);
			strcpy(g_hangarMenuItemLabels[0][3], g_strHangarMenuItems[HANGAR_MENU_ITEM_SWITCH_CRAFT]);
			strcpy(g_hangarMenuItemLabels[0][4], g_strHangarMenuItems[HANGAR_MENU_ITEM_LOAD_WARHEADS]);
			strcpy(g_hangarMenuItemLabels[0][5], g_strHangarMenuItems[HANGAR_MENU_ITEM_INSTALL_BEAMS]);
			strcpy(g_hangarMenuItemLabels[0][6], g_strHangarMenuItems[HANGAR_MENU_ITEM_INSTALL_COUNTERS]);
			g_hangarMenuItemCount[0] = 7;

			objectIndex = g_players[localPlayerIdx].objectIndex;
			playerObj = &g_objectTable[objectIndex];
			if (playerObj->objectType == OBJ_MissileBoat) {
				strcpy(g_hangarMenuItemLabels[0][4], g_strHangarMenuItems[HANGAR_MENU_ITEM_MB_LOAD_PRIMARY]);
				strcpy(g_hangarMenuItemLabels[0][2],
					   g_strHangarMenuItems[HANGAR_MENU_ITEM_MB_LOAD_SECONDARY]);
			}

			g_hangarMenuScroll[1] = 0;
			switch (g_hangarMenuCursor[0]) {
				case 3:
					Hangar_BuildCraftSelectMenu();
					break;
				case 4: {
					uint16_t* warheadListEntry;
					int listIndex;
					int itemIndex;

					g_curCraft = playerObj->mobj->pCraft;
					strcpy(g_hangarMenuColTitle[1], g_strHangarMenuTitles[HANGAR_MENU_TITLE_SELECT_WARHEAD]);
					g_hangarMenuCursor[1] = 0;
					g_hangarMenuItemCount[1] = 0;
					warheadListEntry = g_hangarWarheadList;
					listIndex = 0;
					itemIndex = 0;
					while (warheadListEntry < g_hangarWarheadList + 9) {
						uint16_t warheadType;

						warheadType = g_warheadTypeIds[*warheadListEntry];
						if (warheadType != 0 || warheadListEntry == g_hangarWarheadList) {
							const char* label;

							if (warheadType == 0) {
								label = g_strHangarMiscStrings[HANGAR_MISC_WARHEAD_NONE];
							} else {
								label = g_strWarheadNames[warheadType - OBJ_WarheadTorpedo];
							}
							strcpy(g_hangarMenuItemLabels[1][itemIndex], label);
							g_hangarMenuItemCount[1] = ++itemIndex;
							if (warheadType == g_curCraft->warheadSlotTypeIds[0]) {
								g_hangarMenuCursor[1] = listIndex;
							}
						}
						++warheadListEntry;
						++listIndex;
					}
					break;
				}
				case 5: {
					uint16_t* beamListEntry;
					int listIndex;
					int itemIndex;

					g_curCraft = playerObj->mobj->pCraft;
					strcpy(g_hangarMenuColTitle[1], g_strHangarMenuTitles[HANGAR_MENU_TITLE_SELECT_BEAM]);
					g_hangarMenuCursor[1] = 0;
					g_hangarMenuItemCount[1] = 0;
					beamListEntry = g_hangarBeamList;
					listIndex = 0;
					itemIndex = 0;
					while (beamListEntry < g_hangarBeamList + 5) {
						uint16_t beamType;
						const char* label;

						beamType = *beamListEntry;
						if (beamType != 0 || beamListEntry == g_hangarBeamList) {
							switch (beamType) {
								case 1:
									label = g_strHangarMiscStrings[HANGAR_MISC_BEAM_TRACTOR];
									break;
								case 2:
									label = g_strHangarMiscStrings[HANGAR_MISC_BEAM_JAMMING];
									break;
								case 3:
									label = g_strHangarMiscStrings[HANGAR_MISC_BEAM_DECOY];
									break;
								case 4:
									label = g_strHangarMiscStrings[HANGAR_MISC_BEAM_ENERGY];
									break;
								default:
									label = g_strHangarMiscStrings[HANGAR_MISC_BEAM_NONE];
									break;
							}
							strcpy(g_hangarMenuItemLabels[1][itemIndex], label);
							g_hangarMenuItemCount[1] = ++itemIndex;
							if (beamType == g_curCraft->beamTypeId) {
								g_hangarMenuCursor[1] = listIndex;
							}
						}
						++beamListEntry;
						++listIndex;
					}
					break;
				}
				case 6:
					Hangar_BuildCounterSelectMenu();
					break;
				default:
					break;
			}

			scrollLimit = g_hangarMenuScroll[1] + 7;
			if (g_hangarMenuCursor[1] < g_hangarMenuScroll[1]) {
				g_hangarMenuScroll[1] = g_hangarMenuCursor[1];
				scrollLimit = g_hangarMenuScroll[1] + 7;
			}
			if (g_hangarMenuCursor[1] > scrollLimit) {
				g_hangarMenuScroll[1] = g_hangarMenuCursor[1] - 7;
			}
			return (char)scrollLimit;
		}
	}

	{
		const char* title0;
		const char* title1;
		uint16_t localPlayerIff;
		uint8_t primary;

		localPlayerIff = (uint16_t)g_players[g_localPlayer].playerIff;
		primary = g_missionFlightRuntimeState.teamGoalStatus[localPlayerIff][TEAM_GOAL_PRIMARY];
		title0 = primary != 1 ? g_strHangarMenuTitles[HANGAR_MENU_TITLE_MISSION_FAIL]
							  : g_strHangarMenuTitles[HANGAR_MENU_TITLE_MISSION_OVER];
		strcpy(g_hangarMenuColTitle[0], title0);
		strcpy(g_hangarMenuItemLabels[0][0], g_strHangarMenuItems[HANGAR_MENU_ITEM_MISSION_DEBRIEF]);
		strcpy(g_hangarMenuItemLabels[0][1], g_strHangarMenuItems[HANGAR_MENU_ITEM_MISSION_EMPTY]);
		strcpy(g_hangarMenuItemLabels[0][2], g_strHangarMenuItems[HANGAR_MENU_ITEM_MISSION_REFLY]);
		g_hangarMenuItemCount[0] = 3;

		title1 = primary == 1 ? g_strHangarMenuTitles[HANGAR_MENU_TITLE_MISSION_FAIL]
							  : g_strHangarMenuTitles[HANGAR_MENU_TITLE_MISSION_OVER];
		strcpy(g_hangarMenuColTitle[1], title1);
		g_hangarMenuItemCount[1] = 0;
		if (!g_hangarInitialReadyEntryPending) {
			if (resetCursor) {
				g_hangarMenuCursor[0] = primary != 1 ? 2 : 0;
			}
			return (char)resetCursor;
		}
		return (char)g_hangarInitialReadyEntryPending;
	}
}

// FUNCTION: XWA 0x4625C0
void Hangar_FormatCountermeasureName(int16_t cmTypeId, char* outBuf) {
	switch ((uint16_t)cmTypeId) {
		case 1:
			strcpy(outBuf, g_strHangarMenuItems[31]);
			break;
		case 2:
			strcpy(outBuf, g_strHangarMenuItems[32]);
			break;
		case 3:
			strcpy(outBuf, g_strHangarMenuItems[33]);
			break;
		default:
			strcpy(outBuf, g_strHangarMenuItems[30]);
			break;
	}
}

// FUNCTION: XWA 0x462670
ObjectIndex Hangar_SwitchPlayerCraft(int newModelType) {
	uint16_t playerObjIdx;
	int componentIdx;
	int componentsLeft;
	int oldObjectType;
	int engineSfxSlot;
	uint16_t modelType;
	ModelIndex modelIndex;
	uint16_t turretModelType;
	ObjectIndex newObjIdx;

	playerObjIdx = (uint16_t)g_hangarPlayerObjIdx;
	for (componentIdx = 0; componentIdx < 2; ++componentIdx) {
		g_objectTable[playerObjIdx].typeSpecificByte[componentIdx] = 0;
	}

	componentIdx = 0;
	componentsLeft = 50;
	do {
		++componentIdx;
		--componentsLeft;
		g_curCraft->componentState[componentIdx - 1] = 0;
		g_curCraft->meshRotation[componentIdx - 1] = 0;
	} while (componentsLeft != 0);

	FlightObject_AnimateCrewMeshRotations(g_hangarPlayerObjIdx, 1);

	{
		uint16_t flightGroupIdx;

		flightGroupIdx = g_objectTable[playerObjIdx].flightGroupIdx;
		g_spawnStatus1 = g_missionFlightGroups[flightGroupIdx].fg.status1;
		g_spawnStatus2 = g_missionFlightGroups[flightGroupIdx].fg.status2;
	}

	g_curCraft = g_objectTable[playerObjIdx].mobj->pCraft;
	g_curCraft->effectiveAiObjectLink = NULL;
	g_curCraft->turretAim.effectiveAiObjectSignature = 0;

	oldObjectType = g_objectTable[g_players[g_localPlayer].objectIndex].objectType;
	switch (oldObjectType) {
		case OBJ_TIEFighter:
		case OBJ_TIEInterceptor:
		case OBJ_TIEBomber:
		case OBJ_TIEAdvanced:
		case OBJ_TIEDefender:
			engineSfxSlot = 97;
			break;
		case OBJ_MissileBoat:
		case OBJ_AssaultGunboat:
		case OBJ_RazorFighter:
		case OBJ_PlanetaryFighter:
		case OBJ_PreybirdFighter:
			engineSfxSlot = 98;
			break;
		case OBJ_XWing:
		case OBJ_BWing:
		case OBJ_Z95:
		case OBJ_R41:
		case OBJ_SlaveOne:
			engineSfxSlot = 94;
			break;
		case OBJ_YWing:
		case OBJ_ToscanFighter:
		case OBJ_CloakshapeFighter:
			engineSfxSlot = 95;
			break;
		case OBJ_AWing:
		case OBJ_IrdFighter:
		case OBJ_Twing:
		case OBJ_Piggyback:
			engineSfxSlot = 96;
			break;
		case OBJ_SkiprayBlastBoat:
		case OBJ_CorellianTransport2:
		case OBJ_FamilyTransport:
			engineSfxSlot = 99;
			break;
		case OBJ_SupaFighter:
		case OBJ_SlaveTwo:
		case OBJ_MilleniumFalcon2:
			engineSfxSlot = 100;
			break;
		case 0xffffu:
			engineSfxSlot = -1;
			break;
		default:
			engineSfxSlot = newModelType;
			break;
	}

	if (engineSfxSlot != -1 && Sound_CountPlayingInstances(g_sfxIds[engineSfxSlot]) != 0) {
		Sound_StopOldestInstance(g_sfxIds[engineSfxSlot]);
	}

	modelType = (uint16_t)newModelType;
	if (g_loadedModels.byObjectType[modelType] == 0) {
		if (Hangar_LoadCraftModelByType((ObjectTypeId)newModelType) == 0) {
			return (ObjectIndex)-1;
		}
		Math_SetFpuSinglePrecisionMode();
	}

	modelIndex = GetModelIndexFromType((ObjectTypeId)newModelType);
	if (modelIndex != (ModelIndex)0xffffu) {
		turretModelType = g_modelDefs[(uint16_t)modelIndex].turretModelIndex[0];
		if (turretModelType != OBJ_None && g_loadedModels.byObjectType[turretModelType] == 0) {
			Hangar_LoadCraftModelByType((ObjectTypeId)turretModelType);
			Math_SetFpuSinglePrecisionMode();
		}
	}

	oldObjectType = g_objectTable[g_hangarPlayerObjIdx].objectType;
	if (g_modelTypeTable[(uint16_t)oldObjectType].assetFlags == 0 &&
		g_loadedModels.byObjectType[(uint16_t)oldObjectType] != 0) {
		OptModel_FreeHandle(g_loadedModels.byObjectType[(uint16_t)oldObjectType]);
		g_loadedModels.byObjectType[(uint16_t)g_objectTable[g_hangarPlayerObjIdx].objectType] = 0;
	}

	g_currentFlightGroupIdx = g_objectTable[g_hangarPlayerObjIdx].flightGroupIdx;
	g_spawnObjectKind = 0;
	g_spawnLinkedObjectFlag = 1;
	g_spawnIff = g_missionFlightGroups[g_currentFlightGroupIdx].fg.iff;
	g_spawnTeamId = g_missionFlightGroups[g_currentFlightGroupIdx].fg.team;
	g_spawnGenusId = (ModelGenusId)g_modelTypeTable[(uint16_t)newModelType].genusId;
	g_spawnStatus1 = g_missionFlightGroups[g_currentFlightGroupIdx].fg.status1;
	g_spawnStatus2 = g_missionFlightGroups[g_currentFlightGroupIdx].fg.status2;

	newObjIdx =
		Mission_InitFlightGroupObjectSlot((ObjectTypeId)newModelType, (ObjectIndex)g_hangarPlayerObjIdx);
	if (newObjIdx != (ObjectIndex)-1) {
		uint16_t objectIdx;

		objectIdx = (uint16_t)newObjIdx;
		FVIEW_calcrotatemove(g_objectTable[objectIdx].pitch, g_objectTable[objectIdx].yaw,
							 &g_objectTable[objectIdx]);
		FVIEW_calcrotateorient(g_objectTable[objectIdx].roll, g_objectTable[objectIdx].angleD,
							   &g_objectTable[objectIdx]);
		g_players[g_localPlayer].boundObjectSignature = g_objectTable[g_hangarPlayerObjIdx].objectSignature;
	}

	g_reticleDirty = 1;
	if (g_provingGroundsModeActive) {
		uint16_t craftType;

		for (craftType = 0; craftType < XWA_CRAFT_TYPE_REVERSE_SEARCH_COUNT; ++craftType) {
			if (g_objectTypeTables.craftTypeToObjectType[craftType] == modelType) {
				g_yardSelectedCraftType = (uint8_t)craftType;
				g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType = (uint8_t)craftType;
				break;
			}
		}
		g_yardCurrentCraftScoreTable =
			Yard_FindCraftScoreTableByObjectType((ObjectTypeId)modelType, g_yardHighScoreTable);
	}

	return newObjIdx;
}

// FUNCTION: XWA 0x462530
void Hangar_FormatBeamSystemName(int16_t beamTypeId, char* outBuf) {

	switch ((uint16_t)beamTypeId) {
		case 1:
			strcpy(outBuf, g_strHangarMiscStrings[HANGAR_MISC_BEAM_TRACTOR]);
			break;
		case 2:
			strcpy(outBuf, g_strHangarMiscStrings[HANGAR_MISC_BEAM_JAMMING]);
			break;
		case 3:
			strcpy(outBuf, g_strHangarMiscStrings[HANGAR_MISC_BEAM_DECOY]);
			break;
		case 4:
			strcpy(outBuf, g_strHangarMiscStrings[HANGAR_MISC_BEAM_ENERGY]);
			break;
		default:
			strcpy(outBuf, g_strHangarMiscStrings[HANGAR_MISC_BEAM_NONE]);
			break;
	}
}
