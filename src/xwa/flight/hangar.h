#ifndef XWA_FLIGHT_HANGAR_H
#define XWA_FLIGHT_HANGAR_H

#include <stdint.h>

#include "xwa/flight/object/object.h"  /* Q16Angle, ObjectTypeId, ObjectIndex */

#ifdef __cplusplus
extern "C" {
#endif

/* Hangar scene: ready room, launch, droid/shuttle traffic, and craft menus. */

#pragma pack(push, 1)
typedef struct HangarSceneObjectState {
	int      objectIdx;
	int      moveState;
	int      targetObjIdx;
	int      routeNodeIdx;
	int      prevRouteNodeIdx;
	int      targetWorldX;
	int      targetWorldY;
	int      targetWorldZ;
	Q16Angle targetYaw;
	float    moveSpeed;
	int      nextDecisionTime;
	int      meshRotationDir;
} HangarSceneObjectState;
#pragma pack(pop)

typedef char xwa_hangar_scene_object_state_size[(sizeof(HangarSceneObjectState) == 0x2e) ? 1 : -1];

enum {
	HANGAR_MENU_LEVEL_COUNT          = 2,
	HANGAR_MENU_ROW_COUNT            = 200,
	HANGAR_MENU_SELECTABLE_ROW_COUNT = 50,
	HANGAR_MENU_TEXT_LENGTH          = 50,
	// block; g_hangarCraftListEnd at index 126 is only the scan/display limit).
	HANGAR_CRAFT_LIST_STORAGE        = 512,
	HANGAR_CRAFT_LIST_COUNT          = 126,
	HANGAR_COUNTERMEASURE_LIST_COUNT = 4,
};

extern int                    g_inHangarReady;
extern int                    g_hangarAutoCam;
extern int                    g_hangarSavedMissionRegionIdx;
extern int                    g_hangarSavedPlayerWorldX;
extern int                    g_hangarSavedPlayerWorldY;
extern int                    g_hangarSavedPlayerWorldZ;
extern int                    g_hangarSavedPlayerPitch;
extern int                    g_hangarTipStep;
extern int                    g_hangarSavedPlayerAngleD;
extern int                    g_hangarSavedThrottleSpeed;
extern int                    g_hangarSavedPlayerRoll;
extern int                    g_hangarSavedBeamLevel;
extern int                    g_hangarSavedLaserRedirect;
extern int                    g_hangarSavedShieldRedirect;
extern int                    g_hangarSavedPlayerYaw;
extern int                    g_hangarSceneObjectCount;
extern HangarSceneObjectState g_hangarSceneObjects[10];
extern HangarSceneObjectState g_hangarShuttleState;
extern HangarSceneObjectState g_hangarRoofCraneState;
extern int                    g_hangarEntryTime4x;
extern int                    g_hangarReadyElapsedMs;
extern int                    g_hangarSceneRegionIdx;
extern int                    g_hangarDroidTargetObjIdx;
extern int                    g_hangarDroidRouteTargetObjIdx;
extern int                    g_hangarAnimatedDroidObjIdx;
extern int                    g_hangarDroid0RouteActive;
extern int                    g_hangarDroid1RouteActive;
extern int                    g_unusedHangarShuttleMeshFlagLatch;
extern int                    g_hangarReservedFilmState0;
extern int                    g_hangarHullDamageWarningPlayed;
extern int                    g_hangarEvacuateWarningPlayed;
extern int                    g_hangarLaunchMirrorAttachOffsets;
extern int                    g_hangarReturnToFlightAvailable;
extern int                    g_hangarServiceCooldown;
extern int                    g_hangarPlayerObjIdx;
extern int                    g_hangarCamFocusObj;
extern int16_t                g_hangarBackdropModelType;
extern int                    g_hangarMissionResolved;
extern int                    g_hangarSourceObjIdx;
extern int                    g_hangarNextAmbientSoundTime;
extern int                    g_unusedHangarPrimaryDroidObjIdx;
extern int                    g_unusedHangarPrimaryDroidObjIdxMirror;
extern float                  g_hangarLaunchMoveSpeed;
extern float                  g_hangarLaunchRollRate;
extern int                    g_hangarInitialReadyEntryPending;
extern char                   g_hangarMenuColTitle[2][HANGAR_MENU_TEXT_LENGTH];
extern char g_hangarMenuItemLabels[HANGAR_MENU_LEVEL_COUNT][HANGAR_MENU_ROW_COUNT][HANGAR_MENU_TEXT_LENGTH];
extern int  g_hangarMenuItemDisabled[HANGAR_MENU_LEVEL_COUNT][HANGAR_MENU_SELECTABLE_ROW_COUNT];
extern int  g_hangarMenuItemCount[HANGAR_MENU_LEVEL_COUNT];
extern int  g_hangarMenuLevel;
extern int  g_hangarMenuCursor[HANGAR_MENU_LEVEL_COUNT];
extern int  g_hangarMenuScroll[HANGAR_MENU_LEVEL_COUNT];
extern uint16_t       g_hangarCraftList[HANGAR_CRAFT_LIST_STORAGE];
extern int16_t        g_hangarCountermeasureTypeList[HANGAR_COUNTERMEASURE_LIST_COUNT];
extern uint16_t       g_hangarWarheadList[9];
extern uint16_t       g_hangarBeamList[5];

void         Hangar_SetupReadyScene(void);
void         Hangar_ClearSceneObjectCount(void);
int          Hangar_FilmWriteSceneObjectState(void);
int          Hangar_FilmReadSceneObjectState(void);
void         Hangar_RenderScene(void);
void         Hangar_RenderFourStepCameraTransition(void);
void         Hangar_LaunchPlayerCraft(void);
int          Hangar_UpdateLaunch(int dtMs);
int          Hangar_BeginEnterCraft(uint16_t fromObjIdx);
#ifdef XWA_MODERN
/* Consume a request to resume the original ready loop from the host task. */
int          Hangar_TakeReadyLoopRequest(void);
/* Complete a pending nonblocking options modal before hangar timing resumes. */
int          Hangar_ContinueOptionsModal(void);
#endif
int          Hangar_EnterCraft(uint16_t fromObjIdx);
void         Hangar_HandleInput(void);
void         Hangar_SetCameraShot(int shotIndex);
int          Hangar_GetLaunchModelZOffset(int modelType);
int16_t      Hangar_LoadCraftModelByType(uint16_t objectType);
int16_t      Hangar_SpawnObjectRelativeToLaunchRef(uint16_t modelType, int relX, int relY, int relZ,
												   Q16Angle relYaw, Q16Angle relPitch);
void         Hangar_UpdateRoofCraneMotion(int dtTicks);
void         Hangar_UpdateShuttleTrafficCycle(int dtTicks);
void         Hangar_UpdateHangarDroidTraffic(int dtTicks);
void         Hangar_UpdateFamilyBaseDroidTraffic(int dtTicks);
void         Hangar_ServicePlayerCraft(int dtTicks);
void         Hangar_DrawProvingGroundStatusPanel(void);
void         Hangar_DrawMenu(void);
void         Hangar_DrawMenuColumn(int level, int side);
char         Hangar_BuildMenu(int resetCursor);
void         Hangar_PopulateProvingGroundMenu(int level);
void         Hangar_BuildCraftSelectMenu(void);
void         Hangar_BuildCounterSelectMenu(void);
ObjectIndex  Hangar_SwitchPlayerCraft(int newModelType);
void         Hangar_FormatCountermeasureName(int16_t cmTypeId, char* outBuf);
void         Hangar_FormatBeamSystemName(int16_t beamTypeId, char* outBuf);
void         Hangar_RenderReadyScreen(void);

/* Hangar-TU globals with non-hangar names (grouped in the hangar data block). */
extern int                    g_launchSeqPhase;
extern int                    g_launchAnimDone;
extern int                    g_launchBaseZ;
extern int                    g_launchRefObjIdx;
extern int16_t                dstX;
extern int16_t                dstY;
extern struct YardHighScoreTable* g_yardHighScoreTable;        /* full def in xwa/flight/yard.h */
extern struct YardCraftScoreTable* g_yardCurrentCraftScoreTable;
extern uint8_t g_yardSelectedCraftType;

#ifdef __cplusplus
}
#endif

#endif
