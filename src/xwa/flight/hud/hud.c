#include "xwa/flight/hud/hud.h"
#include "xwa/flight/fediskio.h"
#include "xwa/flight/hangar.h"

#include "xwa/flight/ai/pai.h"
#include "xwa/flight/ai/pai_plan.h"
#include "xwa/flight/ai/paifight.h"
#include "xwa/flight/ai/paiman.h"
#include "xwa/flight/ai/paiorder.h"
#include "xwa/assets/file_io.h"
#include "xwa/assets/flight_model.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_texture.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/sprite_resource.h"
#include "xwa/assets/string_table.h"
#include "xwa/console/console.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/object/damage.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/flight_light.h"
#include "xwa/audio/fsfx.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/math/fixed.h"
#include "xwa/math/scalar.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/yard.h"
#include "xwa/flight/flight_net.h"
#include "xwa/flight/net_session.h"
#include "xwa/flight/player/player.h"
#ifdef XWA_MODERN
#include "xwa_runtime/snapshot/snapshot_hud.h"
#endif
#include "xwa/render/effects.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/debug.h"
#include "xwa/util/string.h"
#include "xwa/util/time.h"
#include "xwa_runtime/timing/host_clock.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef XWA_MODERN
// GLOBAL: XWA 0x9CF624
extern char* g_goalAiLevelString_BiasedBase[];
#endif

#ifdef XWA_MODERN
#define HUD_PANE_PUSH(id_, x_, y_, w_, h_) XwaSnapshotHud_PushPane((id_), (x_), (y_), (w_), (h_))
#define HUD_RELATIVE_PANE_PUSH(id_, x_, y_, w_, h_)                                                          \
	XwaSnapshotHud_PushRelativePane((id_), (x_), (y_), (w_), (h_))
#define HUD_PANE_POP() XwaSnapshotHud_PopPane()
#define HUD_PANE_PARAM XwaHudPaneId pane,
#define HUD_PANE_ARG(id_) (id_),
static int32_t g_hudCrtCameraDistance;
#else
#define HUD_PANE_PUSH(id_, x_, y_, w_, h_) ((void)0)
#define HUD_RELATIVE_PANE_PUSH(id_, x_, y_, w_, h_) ((void)0)
#define HUD_PANE_POP() ((void)0)
#define HUD_PANE_PARAM
#define HUD_PANE_ARG(id_)
#endif

#ifndef XWA_MODERN
#define XWA_HUD_STDCALL __stdcall
__declspec(dllimport) void XWA_HUD_STDCALL OutputDebugStringA(const char* lpOutputString);
typedef struct HudLocalTime {
	uint16_t year;
	uint16_t month;
	uint16_t dayOfWeek;
	uint16_t day;
	uint16_t hour;
	uint16_t minute;
	uint16_t second;
	uint16_t milliseconds;
} HudLocalTime;
__declspec(dllimport) void XWA_HUD_STDCALL GetLocalTime(HudLocalTime* localTime);
// GLOBAL: XWA 0x5A90B0
void(XWA_HUD_STDCALL* g_GetLocalTime)(HudLocalTime* localTime) = GetLocalTime;
// GLOBAL: XWA 0x5A90B4
void(XWA_HUD_STDCALL* g_OutputDebugStringA)(const char* lpOutputString) = OutputDebugStringA;
#define XWA_HUD_OUTPUT_DEBUG_STRING g_OutputDebugStringA
#else
#define XWA_HUD_STDCALL
static void OutputDebugStringA(const char* outputString) { DebugPrintf("%s", outputString); }
#define XWA_HUD_OUTPUT_DEBUG_STRING OutputDebugStringA
#endif

// GLOBAL: XWA 0x5A99B4
const float flt_5A99B4 = 0.000030518499f;

// GLOBAL: XWA 0x9B6340
HudInFlightMessageRecord g_systemMessagePane;
// GLOBAL: XWA 0x9B63A0
HudInFlightMessageRecord g_flightGroupMessagePane;
// GLOBAL: XWA 0x7C9DC0
HudInFlightMessageRecord g_readyMessagePaneQueue[11];
// GLOBAL: XWA 0x6003B8
uint16_t g_messageLogWriteIndex = 0xffff;
// GLOBAL: XWA 0x782828
uint16_t g_messageLogTotalCount;
// GLOBAL: XWA 0x78282C
uint16_t g_messageLogWrapped;
// GLOBAL: XWA 0x8C1CDC
int g_messageLogFileWriteRequested;
// GLOBAL: XWA 0x6003BC
int g_flightMessagePaneLayoutInitSentinel = -1;
// GLOBAL: XWA 0x6937BC
int g_flightMessagePanesForceExpire;
// GLOBAL: XWA 0x7FFA3C
int g_unusedFlightMessagePaneLayoutScalar0;
// GLOBAL: XWA 0x7CA344
int g_unusedFlightMessagePaneLayoutScalar1;
// GLOBAL: XWA 0x7CA16C
int g_unusedFlightMessagePaneLayoutScalar2;
// GLOBAL: XWA 0x7FFD68
int g_unusedFlightMessagePaneLayoutScalar3;
// GLOBAL: XWA 0x7D4BCC
int g_unusedFlightMessagePaneLayoutScalar4;
// GLOBAL: XWA 0x7D4BC8
int g_unusedFlightMessagePaneLayoutScalar5;
// GLOBAL: XWA 0x8C1CD0
int g_unusedFlightMessagePaneLayoutScalar6;
// GLOBAL: XWA 0x7FFD64
int g_unusedFlightMessagePaneLayoutScalar7;
// GLOBAL: XWA 0x8053C4
int g_unusedFlightMessagePaneLayoutScalar8;
// GLOBAL: XWA 0x7CA6C0
int g_unusedFlightMessagePaneLayout505;
// GLOBAL: XWA 0x8BF38C
int g_unusedFlightMessagePaneLayout150;
// GLOBAL: XWA 0x808126
uint16_t g_unusedReadyMessagePaneInitialState;
// GLOBAL: XWA 0x808134
int g_targetDescriptionMessageId;
// GLOBAL: XWA 0x9B6320
HudInFlightMessageRecord* g_messageLogRecords;
// GLOBAL: XWA 0x8C163C
MemoryHandle g_messageLogHandle;
// GLOBAL: XWA 0x80AD28
uint8_t g_readyMessageQueueCount;
// GLOBAL: XWA 0x808124
uint16_t g_pendingHudMessageVoiceSfxId;
// GLOBAL: XWA 0x91AC9C
uint16_t g_msgSenderIff;
// GLOBAL: XWA 0x5B6860
const uint8_t g_messageTextPrefixColorCodes[24] = {
	0x42, 0x4a, 0x46, 0x4e, 0x52, 0x45, 0x42, 0x52, 0x4a, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x4a, 0x52, 0x46, 0x4e, 0x4a, 0x4e, 0x00, 0x00,
};
// GLOBAL: XWA 0x5B6878
const uint8_t g_messageSenderIffColorCodes[8] = {
	0x52, 0x4a, 0x46, 0x4e, 0x4a, 0x56, 0x00, 0x00,
};
// GLOBAL: XWA 0x5B6898
const char g_messageLogLineFormat[] = "%s\t%d:%d:%d\n";
// GLOBAL: XWA 0x5B68AC
const char g_messageLogFileNameFormat[] = "msglog%d.txt";
// GLOBAL: XWA 0x80DB80
PlayerFlightTransientTimers g_playerFlightTransientTimers[8];
// GLOBAL: XWA 0x910788
uint16_t g_msgArgTable[4];
// GLOBAL: XWA 0x910E10
const char* g_msgPtrs[4];
// GLOBAL: XWA 0x68C8A0
uint8_t g_sceneBypassCockpitMask;
// GLOBAL: XWA 0x68C8A4
uint8_t g_savedHudCmdPanelEnabled;
// GLOBAL: XWA 0x78284C
int g_flightSwRotSpriteCoeffCacheValid;
// GLOBAL: XWA 0x8C1500
char outName[256];
// GLOBAL: XWA 0x5B6880
const uint8_t g_targetDescDesignationUsesRelationText[24] = {
	0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0,
};
// GLOBAL: XWA 0x7CA34C
int g_flightSystemMessagesEnabled;
// GLOBAL: XWA 0x8D9744
uint16_t g_replayViewMode;
// GLOBAL: XWA 0x9AEE60
char g_mfdCommandSecondaryTargetLabels[10][30];
// GLOBAL: XWA 0x9AF060
char g_mfdCommandPrimaryTargetLabels[7][30];
// GLOBAL: XWA 0x749ACC
int g_mfdCommandNodeSwitchColorChar;
// GLOBAL: XWA 0x5A97BC
const int g_mfdCommandSubMenuItemCount[6] = { 0, 8, 2, 8, 2, 5 };
// GLOBAL: XWA 0x5A97D4
const int g_mfdCommandSubMenuFirstItemIndex[6] = { 0, 8, 12, 20, 22, 27 };
// GLOBAL: XWA 0x5A9998
const float g_hudQuadBaseSize = 256.0f;
// GLOBAL: XWA 0x5A99BC
const float g_hudFpsAverageScale = 0.25f;
// GLOBAL: XWA 0x5A9AC4
const float flt_5A9AC4 = 225.0f;
// GLOBAL: XWA 0x5A9AC8
const float flt_5A9AC8 = 5.0f;
// GLOBAL: XWA 0x5A9ACC
const float flt_5A9ACC = 230.0f;
// GLOBAL: XWA 0x5B5334
// Elements 0-11 default to enabled, matching the original's static data segment at
// 0x5B5334 (12 x {enabled=1}). The initial HUD state (e.g. the CMD/3D-CRT inset gated
// on element 0) relies on this; runtime setters only re-assert it on reload/refresh.
HudElementEnabled g_hudElementEnabled[12] = {
	{ 1, { 0, 0, 0 } }, { 1, { 0, 0, 0 } }, { 1, { 0, 0, 0 } }, { 1, { 0, 0, 0 } },
	{ 1, { 0, 0, 0 } }, { 1, { 0, 0, 0 } }, { 1, { 0, 0, 0 } }, { 1, { 0, 0, 0 } },
	{ 1, { 0, 0, 0 } }, { 1, { 0, 0, 0 } }, { 1, { 0, 0, 0 } }, { 1, { 0, 0, 0 } },
};
HudDrawTarget g_defaultHudDrawTarget;
// GLOBAL: XWA 0x5B5314
HudDrawTarget* g_drawTarget = &g_defaultHudDrawTarget;
// GLOBAL: XWA 0x68C878
uint16_t* g_curImageBlendLut;
// GLOBAL: XWA 0x68C87C
uint16_t* g_curImagePalette;
// GLOBAL: XWA 0x68C880
uint8_t* g_curImageRLE;
// GLOBAL: XWA 0x68C884
uint16_t g_curImageWidth;
// GLOBAL: XWA 0x68C888
uint16_t g_curImageHeight;
// GLOBAL: XWA 0x68C88C
int g_curImageRunRemaining;
// GLOBAL: XWA 0x68C890
struct Sprite* g_curImage;
// GLOBAL: XWA 0x68C93C
uint8_t g_hudUseAlphaSpriteAtlas10100;
// GLOBAL: XWA 0x5A969C
int g_hudAlphaSpriteGroupOffset = 100;
// GLOBAL: XWA 0x5A9608
uint16_t g_shieldSilhouetteSpriteIdByObjectType[72] = {
	0,    100,  200,  300,  400,  500,  600,  700,  800,  900,  1000, 1100, 1200, 1300, 1400,
	1500, 1600, 1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900,
	0,    3000, 3100, 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
	0,    0,    0,    0,    0,    3200, 3300, 3400, 3500, 3600, 3700, 3800, 3900, 4000, 4000,
	4100, 4200, 4300, 4400, 4500, 4600, 0,    0,    0,    0,    0,    0,
};
// GLOBAL: XWA 0x68C550
int32_t g_reticleLaserHardpointCount;
// GLOBAL: XWA 0x68BD68
int g_reticleLaserHardpointIndices[16];
// GLOBAL: XWA 0x68C554
int g_reticleWarheadHardpointCount;
// GLOBAL: XWA 0x68BCE0
int g_reticleWarheadHardpointIndices[16];
// GLOBAL: XWA 0x68C140
HudPoint g_reticleLaserAimPoints[16];
// GLOBAL: XWA 0x68C558
int g_reticleCenterX;
// GLOBAL: XWA 0x68C55C
int g_reticleCenterY;
// GLOBAL: XWA 0x68C894
uint8_t g_reticleDirty;
// GLOBAL: XWA 0x68C528
int g_reticleDrawX;
// GLOBAL: XWA 0x68C52C
int g_reticleDrawY;
// GLOBAL: XWA 0x68C924
uint8_t g_hudLaserChargeDisplayDrawn;
// GLOBAL: XWA 0x5A96E4
int g_reticleAimPointDistanceBias = 40;
// GLOBAL: XWA 0x68BFE8
FlightTexQuad g_hudLaserChargeQuads[16];
// GLOBAL: XWA 0x68C420
FlightTexQuad g_hudIonChargeQuads[16];
// GLOBAL: XWA 0x5A96A0
int g_hudEnergyBankLaserSelector = 0;
// GLOBAL: XWA 0x5A96A4
int g_hudEnergyBankIonSelector = 1;
// GLOBAL: XWA 0x5B5304
uint16_t g_hudEnergyChargeTripleSegmentStepX = 7;
// GLOBAL: XWA 0x5B5308
uint16_t g_hudEnergyChargeTripleInitialBackstepX = 17;
// GLOBAL: XWA 0x5B530C
uint16_t g_hudEnergyChargeNonTripleSegmentStepX = 10;
// GLOBAL: XWA 0x5B5310
uint16_t g_hudEnergyChargeNonTripleInitialBackstepX = 24;
// GLOBAL: XWA 0x5A96A8
int g_hudEnergyBarHighChargeSpriteOffset = 0;
// GLOBAL: XWA 0x5A96AC
int g_hudEnergyBarLowChargeSpriteOffset = 100;
// GLOBAL: XWA 0x5A96B0
int g_hudEnergyBarTripleBankSpriteOffset = 600;
// GLOBAL: XWA 0x5A96C0
uint32_t g_hudEnergyBarMaxSegments = 6;
// GLOBAL: XWA 0x5A96C4
uint16_t g_hudBeamChargeUnitsPerSegment = 1000;
// GLOBAL: XWA 0x5A96C8
uint16_t g_hudBeamChargeColorBandUnits = 333;
// GLOBAL: XWA 0x5A96CC
uint16_t g_hudBeamChargeSegmentCount = 9;
// GLOBAL: XWA 0x5A96BC
int g_hudWeaponModeWarhead = 1;
// GLOBAL: XWA 0x5A9750
const uint32_t g_hudSubsystemLaserLabelSurfaceWidth = 10;
// GLOBAL: XWA 0x5A9754
const uint32_t g_hudSubsystemLaserLabelSurfaceHeight = 10;
// GLOBAL: XWA 0x5A9758
const uint32_t g_hudSubsystemShieldLabelSurfaceWidth = 10;
// GLOBAL: XWA 0x5A975C
const uint32_t g_hudSubsystemShieldLabelSurfaceHeight = 10;
// GLOBAL: XWA 0x5A9760
const uint32_t g_hudSubsystemEngineLabelSurfaceWidth = 10;
// GLOBAL: XWA 0x5A9764
const uint32_t g_hudSubsystemEngineLabelSurfaceHeight = 10;
// GLOBAL: XWA 0x5A9768
const uint32_t g_hudSubsystemBeamLabelSurfaceWidth = 10;
// GLOBAL: XWA 0x5A976C
const uint32_t g_hudSubsystemBeamLabelSurfaceHeight = 10;
// GLOBAL: XWA 0x68C928
uint8_t g_hudShieldPercentLabelsInitialized;
// GLOBAL: XWA 0x68C958
uint16_t g_hudShieldFrontPercentCached;
// GLOBAL: XWA 0x68C95C
uint16_t g_hudShieldRearPercentCached;
// GLOBAL: XWA 0x8D6BA0
uint8_t g_lastShieldDamageSide;
// GLOBAL: XWA 0x5B52C0
const uint32_t g_hudShieldBarArgbBySegmentCount[11] = {
	0xff000000u, 0xff700000u, 0xffc00d0du, 0xffff1111u, 0xff888900u, 0xffc9b400u,
	0xffffe603u, 0xff007c00u, 0xff00b80au, 0xff00f40du, 0xffffffffu,
};
// GLOBAL: XWA 0x5B52F0
const uint32_t argbColor[4] = {
	0xffff0000u,
	0xffffff00u,
	0xff00ff00u,
	0xffffffffu,
};
// GLOBAL: XWA 0x68C930
uint8_t g_systemMessagePaneVisible;
// GLOBAL: XWA 0x68C934
uint8_t g_flightGroupMessagePaneVisible;
// GLOBAL: XWA 0x68C938
uint8_t g_readyMessagePaneVisible;
// GLOBAL: XWA 0x68C92C
uint8_t g_incomingMissileWarningFlashActive;
// GLOBAL: XWA 0x68C960
int g_panelLastLaserThreatVoiceTime;
// GLOBAL: XWA 0x68C964
int g_panelLastBeamThreatVoiceTime;
// GLOBAL: XWA 0x68C968
int g_incomingMissileWarningFlashFrame;
// GLOBAL: XWA 0x5A96D4
const HudThreatIndicatorSlot g_hudThreatIndicatorSlotCenter = HUD_THREAT_INDICATOR_CENTER;
// GLOBAL: XWA 0x5A96D8
const HudThreatIndicatorSlot g_hudThreatIndicatorSlotLeft = HUD_THREAT_INDICATOR_LEFT;
// GLOBAL: XWA 0x5A96DC
const HudThreatIndicatorSlot g_hudThreatIndicatorSlotRight = HUD_THREAT_INDICATOR_RIGHT;
// GLOBAL: XWA 0x5A96E0
const HudThreatIndicatorSlot g_hudThreatIndicatorSlotBottom = HUD_THREAT_INDICATOR_BOTTOM;
// GLOBAL: XWA 0x68C568
void* g_hudSystemMessagePaneSurface;
// GLOBAL: XWA 0x68C56C
void* g_hudFlightGroupMessagePaneSurface;
// GLOBAL: XWA 0x68C570
void* g_hudReadyMessagePaneSurface;
// GLOBAL: XWA 0x5A96E8
int g_hudSystemMessagePaneSurfaceWidth = 470;
// GLOBAL: XWA 0x5A96EC
int g_hudSystemMessagePaneSurfaceHeight = 11;
// GLOBAL: XWA 0x5A96F0
int g_hudFlightGroupMessagePaneSurfaceWidth = 380;
// GLOBAL: XWA 0x5A96F4
int g_hudFlightGroupMessagePaneSurfaceHeight = 11;
// GLOBAL: XWA 0x5A96F8
int g_hudReadyMessagePaneSurfaceWidth = 420;
// GLOBAL: XWA 0x5A96FC
int g_hudReadyMessagePaneSurfaceHeight = 47;
// GLOBAL: XWA 0x68C5C4
int16_t g_hudSystemMessagePaneX;
// GLOBAL: XWA 0x68C5C8
int16_t g_hudSystemMessagePaneY;
// GLOBAL: XWA 0x68C5CC
int16_t g_hudFlightGroupMessagePaneX;
// GLOBAL: XWA 0x68C5D0
int16_t g_hudFlightGroupMessagePaneY;
// GLOBAL: XWA 0x68C5D4
int16_t g_hudReadyMessagePaneX;
// GLOBAL: XWA 0x68C5D8
int16_t g_hudReadyMessagePaneY;
// GLOBAL: XWA 0x5B5318
uint32_t g_hudColors[7] = {
	0xffffffffu, 0xffffffffu, 0xff6464ffu, 0xfff06b00u, 0xff2b41ffu, 0xff641400u, 0xff10bc00u,
};
// GLOBAL: XWA 0x5A9778
const int16_t g_hudTargetArrowQ15Scale = 32760;
// GLOBAL: XWA 0x5A99A0
const double g_hudPadlockYawScreenScale = 0.05405405405405406;
// GLOBAL: XWA 0x5A99A8
const double g_hudPadlockPitchScreenScale = -0.05405405405405406;
// GLOBAL: XWA 0x68C520
int g_hudCenterX;
// GLOBAL: XWA 0x68C524
int g_hudCenterY;
// GLOBAL: XWA 0x68C5BC
int g_hudTargetInsetWidth;
// GLOBAL: XWA 0x68C5C0
int g_hudTargetInsetHeight;
// GLOBAL: XWA 0x68C7AC
int g_hudCmdFrameY;
// GLOBAL: XWA 0x68C6BC
uint16_t g_hudRadarScopeOffsetX;
// GLOBAL: XWA 0x68C6C0
uint16_t g_hudRadarScopeOffsetY;
// GLOBAL: XWA 0x68C704
uint16_t g_hudRadarFrameLeftOffsetX;
// GLOBAL: XWA 0x68C708
uint16_t g_hudRadarFrameRightOffsetX;
// GLOBAL: XWA 0x68C70C
uint16_t g_hudRadarFrameOffsetY;
// GLOBAL: XWA 0x68C6C4
int g_hudLaserThreatSlot0OffsetX;
// GLOBAL: XWA 0x68C6C8
int g_hudLaserThreatSlot0OffsetY;
// GLOBAL: XWA 0x68C6CC
int g_hudWarheadThreatSlot0OffsetX;
// GLOBAL: XWA 0x68C6D0
int g_hudWarheadThreatSlot0OffsetY;
// GLOBAL: XWA 0x68C6D4
int g_hudLaserThreatSlot1OffsetX;
// GLOBAL: XWA 0x68C6D8
int g_hudLaserThreatSlot1OffsetY;
// GLOBAL: XWA 0x68C6DC
int g_hudWarheadThreatSlot1OffsetX;
// GLOBAL: XWA 0x68C6E0
int g_hudWarheadThreatSlot1OffsetY;
// GLOBAL: XWA 0x68C6E4
int g_hudLaserThreatSlot2OffsetX;
// GLOBAL: XWA 0x68C6E8
int g_hudLaserThreatSlot2OffsetY;
// GLOBAL: XWA 0x68C6EC
int g_hudWarheadThreatSlot2OffsetX;
// GLOBAL: XWA 0x68C6F0
int g_hudWarheadThreatSlot2OffsetY;
// GLOBAL: XWA 0x68C6F4
int g_hudLaserThreatSlot3OffsetX;
// GLOBAL: XWA 0x68C6F8
int g_hudLaserThreatSlot3OffsetY;
// GLOBAL: XWA 0x68C6FC
int g_hudWarheadThreatSlot3OffsetX;
// GLOBAL: XWA 0x68C700
int g_hudWarheadThreatSlot3OffsetY;
// GLOBAL: XWA 0x5A9700
const uint32_t g_hudReticleWarheadCountSurfaceWidth = 40;
// GLOBAL: XWA 0x5A9704
const uint32_t g_hudReticleWarheadCountSurfaceHeight = 11;
// GLOBAL: XWA 0x5A9708
const uint32_t g_hudWarheadCountTextSurfaceWidth = 75;
// GLOBAL: XWA 0x5A970C
const uint32_t g_hudWarheadCountTextSurfaceHeight = 11;
// GLOBAL: XWA 0x68C6B0
int g_hudWarheadCountLeftReticleOffsetX;
// GLOBAL: XWA 0x68C6B4
int g_hudWarheadCountRightReticleOffsetX;
// GLOBAL: XWA 0x68C6B8
int g_hudWarheadCountReticleOffsetY;
// GLOBAL: XWA 0x5A9710
const uint32_t g_hudShieldPercentTextSurfaceWidth = 30;
// GLOBAL: XWA 0x5A9714
const uint32_t g_hudShieldPercentTextSurfaceHeight = 11;
// GLOBAL: XWA 0x68C6A0
uint16_t g_hudFrontShieldPercentTextX;
// GLOBAL: XWA 0x68C6A4
uint16_t g_hudFrontShieldPercentTextY;
// GLOBAL: XWA 0x68C6A8
uint16_t g_hudRearShieldPercentTextX;
// GLOBAL: XWA 0x68C6AC
uint16_t g_hudRearShieldPercentTextY;
// GLOBAL: XWA 0x68C5E8
uint16_t g_hudSubsystemLabelLaserX;
// GLOBAL: XWA 0x68C5EC
uint16_t g_hudSubsystemLabelLaserY;
// GLOBAL: XWA 0x68C5F0
uint16_t g_hudSubsystemLabelShieldX;
// GLOBAL: XWA 0x68C5F4
uint16_t g_hudSubsystemLabelShieldY;
// GLOBAL: XWA 0x68C5F8
uint16_t g_hudSubsystemLabelEngineY;
// GLOBAL: XWA 0x68C5FC
uint16_t g_hudSubsystemLabelEngineX;
// GLOBAL: XWA 0x68C600
uint16_t g_hudSubsystemLabelBeamX;
// GLOBAL: XWA 0x68C604
uint16_t g_hudSubsystemLabelBeamY;
// GLOBAL: XWA 0x5A9718
const uint32_t g_hudSpeedTextSurfaceWidth = 75;
// GLOBAL: XWA 0x5A971C
const uint32_t g_hudSpeedTextSurfaceHeight = 11;
// GLOBAL: XWA 0x5A9720
const uint32_t g_hudThrottleTextSurfaceWidth = 75;
// GLOBAL: XWA 0x5A9724
const uint32_t g_hudThrottleTextSurfaceHeight = 11;
// GLOBAL: XWA 0x5A9728
const uint32_t g_hudCraftNameTextSurfaceWidth = 100;
// GLOBAL: XWA 0x5A972C
const uint32_t g_hudCraftNameTextSurfaceHeight = 10;
// GLOBAL: XWA 0x5A9730
const uint32_t g_hudMissionClockTextSurfaceWidth = 80;
// GLOBAL: XWA 0x5A9734
const uint32_t g_hudMissionClockTextSurfaceHeight = 11;
// GLOBAL: XWA 0x5A9738
const uint32_t g_hudCountermeasureCountTextSurfaceWidth = 25;
// GLOBAL: XWA 0x5A973C
const uint32_t g_hudCountermeasureCountTextSurfaceHeight = 11;
// GLOBAL: XWA 0x5A9748
const uint32_t g_hudProvingGroundStatusTextSurfaceWidth = 130;
// GLOBAL: XWA 0x5A974C
const uint32_t g_hudProvingGroundStatusTextSurfaceHeight = 11;
// GLOBAL: XWA 0x68C608
uint16_t g_hudSpeedTextY;
// GLOBAL: XWA 0x68C60C
uint16_t g_hudSpeedTextX;
// GLOBAL: XWA 0x68C610
uint16_t g_hudCraftNameTextY;
// GLOBAL: XWA 0x68C614
uint16_t g_hudCraftNameTextX;
// GLOBAL: XWA 0x68C618
uint16_t g_hudThrottleTextY;
// GLOBAL: XWA 0x68C61C
uint16_t g_hudThrottleTextX;
// GLOBAL: XWA 0x68C620
uint16_t g_hudMissionClockTextY;
// GLOBAL: XWA 0x68C624
uint16_t g_hudMissionClockTextX;
// GLOBAL: XWA 0x68C628
uint16_t g_hudWarheadCountTextY;
// GLOBAL: XWA 0x68C62C
uint16_t g_hudWarheadCountTextX;
// GLOBAL: XWA 0x68C630
uint16_t g_hudDualWarheadCountTextX;
// GLOBAL: XWA 0x68C634
uint16_t g_hudCountermeasureCountTextY;
// GLOBAL: XWA 0x68C638
uint16_t g_hudCountermeasureCountTextX;
// GLOBAL: XWA 0x68C63C
uint16_t g_hudProvingGroundStatusTextX;
// GLOBAL: XWA 0x68C640
uint16_t g_hudProvingGroundStatusTextY;
// GLOBAL: XWA 0x68C5DC
unsigned int g_hudCmdPanelOriginX;
// GLOBAL: XWA 0x68C5E0
unsigned int g_hudCmdPanelOriginY;
// GLOBAL: XWA 0x68C64C
unsigned int g_hudCmdTargetNameTextY;
// GLOBAL: XWA 0x68C650
unsigned int g_hudCmdOrderLineY;
// GLOBAL: XWA 0x68C654
unsigned int g_hudCmdShieldLabelX;
// GLOBAL: XWA 0x68C658
unsigned int g_hudCmdShieldLabelY;
// GLOBAL: XWA 0x68C65C
unsigned int g_hudCmdShieldPercentX;
// GLOBAL: XWA 0x68C660
unsigned int g_hudCmdShieldPercentY;
// GLOBAL: XWA 0x68C664
unsigned int g_hudCmdHullLabelX;
// GLOBAL: XWA 0x68C668
unsigned int g_hudCmdHullLabelY;
// GLOBAL: XWA 0x68C66C
unsigned int g_hudCmdHullPercentX;
// GLOBAL: XWA 0x68C670
unsigned int g_hudCmdHullPercentY;
// GLOBAL: XWA 0x68C674
unsigned int g_hudCmdSystemLabelX;
// GLOBAL: XWA 0x68C678
unsigned int g_hudCmdSystemLabelY;
// GLOBAL: XWA 0x68C67C
unsigned int g_hudCmdSystemPercentX;
// GLOBAL: XWA 0x68C680
unsigned int g_hudCmdSystemPercentY;
// GLOBAL: XWA 0x68C684
unsigned int g_hudCmdDistanceLabelX;
// GLOBAL: XWA 0x68C688
unsigned int g_hudCmdDistanceLabelY;
// GLOBAL: XWA 0x68C68C
unsigned int g_hudCmdDistanceValueX;
// GLOBAL: XWA 0x68C690
unsigned int g_hudCmdDistanceValueY;
// GLOBAL: XWA 0x68C694
unsigned int g_hudCmdTargetStatusX;
// GLOBAL: XWA 0x68C698
unsigned int g_hudCmdTargetStatusY;
// GLOBAL: XWA 0x68C69C
unsigned int g_hudCmdComponentLineY;
// GLOBAL: XWA 0x68C8BC
uint16_t g_hudCmdPanelWidth;
// GLOBAL: XWA 0x68C8C0
uint16_t g_hudCmdPanelHeight;
// GLOBAL: XWA 0x68C914
void* g_hudCmdTexPixels;
// GLOBAL: XWA 0x68BD48
char g_hudTargetNameText[30];
// GLOBAL: XWA 0x68C1C0
char g_hudTargetStatusText[30];
// GLOBAL: XWA 0x68C844
unsigned int g_hudTargetShieldDisplayPct;
// GLOBAL: XWA 0x68C848
unsigned int g_hudTargetSystemDisplayPct;
// GLOBAL: XWA 0x68C84C
unsigned int g_hudTargetHullDisplayPct;
// GLOBAL: XWA 0x68C850
unsigned int g_hudTargetDistanceWhole;
// GLOBAL: XWA 0x68C854
unsigned int g_hudTargetDistanceFrac;
// GLOBAL: XWA 0x5A96B4
const int g_hudOffscreenTargetTextMargin = 5;
// GLOBAL: XWA 0x5B12D8
const uint8_t g_meshTypeComponentMaxHp[32] = {
	0xff, 0xff, 0xff, 0xff, 0x18, 0x04, 0xff, 0xff, 0x60, 0x10, 0x20, 0x30, 0x30, 0x30, 0x70, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x18, 0x20, 0x30, 0x30, 0x30, 0x10, 0xff, 0x07, 0x0e, 0x20, 0x20,
};
// GLOBAL: XWA 0x5A99C0
const float g_hudComponentPercentScale = 0.0099999998f;
// GLOBAL: XWA 0x5A99C4
const float g_hudComponentPercentRoundingBias = -0.5f;
// GLOBAL: XWA 0x5A9770
const unsigned int g_hudFilmRecordingIndicatorSurfaceWidth = 50;
// GLOBAL: XWA 0x5A9774
const unsigned int g_hudFilmRecordingIndicatorSurfaceHeight = 11;
// GLOBAL: XWA 0x68C5B8
void* g_hudFilmRecordingIndicatorSurface;
// GLOBAL: XWA 0x68C644
uint16_t g_hudFilmRecTextX;
// GLOBAL: XWA 0x68C648
uint16_t g_hudFilmRecTextY;
// GLOBAL: XWA 0x5A9740
int g_hudMfdSurfaceWidth = 200;
// GLOBAL: XWA 0x5A9744
int g_hudMfdSurfaceHeight = 11;
// GLOBAL: XWA 0x68C5E4
int g_hudMfdSurfaceY;
// GLOBAL: XWA 0x68C918
int g_hudMfdTextInsetX;
// GLOBAL: XWA 0x68C7A4
int g_hudMfdFrameSideOffsetX;
// GLOBAL: XWA 0x68C7A8
int g_hudMfdFrameY;
// GLOBAL: XWA 0x68C7B0
int g_hudLaserChargeSingleY;
// GLOBAL: XWA 0x68C7B4
int g_hudLaserChargePairLeftOffsetX;
// GLOBAL: XWA 0x68C7B8
int g_hudLaserChargePairRightOffsetX;
// GLOBAL: XWA 0x68C7BC
int g_hudLaserChargePairY;
// GLOBAL: XWA 0x68C7C0
int g_hudLaserChargeTripleLeftOffsetX;
// GLOBAL: XWA 0x68C7C4
int g_hudLaserChargeTripleRightOuterOffsetX;
// GLOBAL: XWA 0x68C7C8
int g_hudLaserChargeTripleRightInnerOffsetX;
// GLOBAL: XWA 0x68C7CC
int g_hudLaserChargeTripleY;
// GLOBAL: XWA 0x68C7D0
int g_hudLaserChargeQuadLeftOffsetX;
// GLOBAL: XWA 0x68C7D4
int g_hudLaserChargeQuadRightOffsetX;
// GLOBAL: XWA 0x68C7D8
int g_hudLaserChargeQuadUpperY;
// GLOBAL: XWA 0x68C7DC
int g_hudLaserChargeQuadLowerY;
// GLOBAL: XWA 0x68C7E0
int g_hudLaserChargeFiveLeftOffsetX;
// GLOBAL: XWA 0x68C7E4
int g_hudLaserChargeFiveReserved0;
// GLOBAL: XWA 0x68C7E8
int g_hudLaserChargeFiveRightOffsetX;
// GLOBAL: XWA 0x68C7EC
int g_hudLaserChargeFiveUpperY;
// GLOBAL: XWA 0x68C7F0
int g_hudLaserChargeFiveLowerY;
// GLOBAL: XWA 0x68C7F4
int g_hudLaserChargeFiveSixReserved0;
// GLOBAL: XWA 0x68C7F8
int g_hudLaserChargeFiveSixReserved1;
// GLOBAL: XWA 0x68C7FC
int g_hudLaserChargeSixLeftOffsetX;
// GLOBAL: XWA 0x68C800
int g_hudLaserChargeSixRightOuterOffsetX;
// GLOBAL: XWA 0x68C804
int g_hudLaserChargeSixRightInnerOffsetX;
// GLOBAL: XWA 0x68C808
int g_hudLaserChargeSixUpperY;
// GLOBAL: XWA 0x68C80C
int g_hudLaserChargeSixLowerY;
// GLOBAL: XWA 0x68C810
int g_hudIonChargeYOffsetFromLaser;
// GLOBAL: XWA 0x68C814
int g_hudIonChargeSingleY;
// GLOBAL: XWA 0x68C818
int g_hudIonChargePairLeftOffsetX;
// GLOBAL: XWA 0x68C81C
int g_hudIonChargePairRightOffsetX;
// GLOBAL: XWA 0x68C820
int g_hudIonChargePairY;
// GLOBAL: XWA 0x68C824
int g_hudIonChargeTripleLeftOffsetX;
// GLOBAL: XWA 0x68C828
int g_hudIonChargeTripleRightInnerOffsetX;
// GLOBAL: XWA 0x68C82C
int g_hudIonChargeTripleRightOuterOffsetX;
// GLOBAL: XWA 0x68C830
int g_hudIonChargeTripleY;
// GLOBAL: XWA 0x68C834
int g_hudIonChargeQuadLeftOffsetX;
// GLOBAL: XWA 0x68C838
int g_hudIonChargeQuadRightOffsetX;
// GLOBAL: XWA 0x68C83C
int g_hudIonChargeQuadUpperY;
// GLOBAL: XWA 0x68C840
int g_hudIonChargeQuadLowerY;
// GLOBAL: XWA 0x68C76C
int g_hudBeamGaugeRightOffsetX;
// GLOBAL: XWA 0x68C770
int g_hudBeamGaugeBottomOffsetY;
// GLOBAL: XWA 0x68C774
int g_hudBeamIconRightOffsetX;
// GLOBAL: XWA 0x68C778
int g_hudBeamIconBottomOffsetY;
// GLOBAL: XWA 0x68C77C
int g_hudBeamChargeRightOffsetX;
// GLOBAL: XWA 0x68C780
int g_hudBeamChargeBottomOffsetY;
// GLOBAL: XWA 0x68C784
int g_hudBeamChargeSegmentOffsetY0;
// GLOBAL: XWA 0x68C788
int g_hudBeamChargeSegmentOffsetY1;
// GLOBAL: XWA 0x68C78C
int g_hudBeamChargeSegmentOffsetY2;
// GLOBAL: XWA 0x68C790
int g_hudBeamChargeSegmentOffsetY3;
// GLOBAL: XWA 0x68C794
int g_hudBeamChargeSegmentOffsetY4;
// GLOBAL: XWA 0x68C798
int g_hudBeamChargeSegmentOffsetY5;
// GLOBAL: XWA 0x68C79C
int g_hudBeamChargeSegmentOffsetY6;
// GLOBAL: XWA 0x68C7A0
int g_hudBeamChargeSegmentOffsetY7;
// GLOBAL: XWA 0x68C73C
int g_hudShieldGaugeSideOffsetX;
// GLOBAL: XWA 0x68C740
int g_hudShieldGaugeBottomOffsetY;
// GLOBAL: XWA 0x68C748
uint16_t g_hudShieldLayoutInitOnlyY;
// GLOBAL: XWA 0x5A9990
const double g_hudPowerBeamReserveStep = 3.33;
// GLOBAL: XWA 0x68C754
int g_hudShieldHullIconSideOffsetX;
// GLOBAL: XWA 0x68C750
int g_hudShieldHullIconBottomOffsetY;
// GLOBAL: XWA 0x68C758
int g_hudShieldBarSideOffsetX;
// GLOBAL: XWA 0x68C75C
int g_hudShieldFrontUpperBarY;
// GLOBAL: XWA 0x68C760
int g_hudShieldFrontLowerBarY;
// GLOBAL: XWA 0x68C764
int g_hudShieldRearUpperBarY;
// GLOBAL: XWA 0x68C768
int g_hudShieldRearLowerBarY;
// GLOBAL: XWA 0x68C710
int g_hudPowerLaserPipX;
// GLOBAL: XWA 0x68C714
int g_hudPowerLaserPipTopY;
// GLOBAL: XWA 0x68C718
int g_hudPowerShieldPipX;
// GLOBAL: XWA 0x68C71C
int g_hudPowerShieldPipTopY;
// GLOBAL: XWA 0x68C720
int g_hudPowerBeamPipRightOffsetX;
// GLOBAL: XWA 0x68C724
int g_hudPowerBeamPipTopY;
// GLOBAL: XWA 0x68C728
int g_hudPowerReservePipRightOffsetX;
// GLOBAL: XWA 0x68C72C
int g_hudPowerBeamReserveTopY;
// GLOBAL: XWA 0x68C730
int g_hudPowerUnallocatedPipTopY;
// GLOBAL: XWA 0x68C734
int g_hudPowerPipSpacingY;
// GLOBAL: XWA 0x68C738
int g_hudPowerReservePipSpacingY;
// GLOBAL: XWA 0x68C8B4
uint16_t g_hudMfdPaneWidth;
// GLOBAL: XWA 0x68C8B8
uint16_t g_hudMfdPaneHeight;
// GLOBAL: XWA 0x68C8A8
uint16_t g_mfdCommandMenuNumberX;
// GLOBAL: XWA 0x68C8AC
uint16_t g_mfdCommandMenuTextX;
// GLOBAL: XWA 0x68C8B0
uint16_t g_mfdCommandMenuLineExtraSpacingY;
// GLOBAL: XWA 0x68C90C
void* g_hudMfdLeftTexPixels;
// GLOBAL: XWA 0x68C910
void* g_hudMfdRightTexPixels;
// GLOBAL: XWA 0x68C5A0
void* g_hudMfdTitleTexPixels;
// GLOBAL: XWA 0x68C578
void* hudTex4;
// GLOBAL: XWA 0x68C57C
void* hudTex5;
// GLOBAL: XWA 0x68C580
void* hudTex6;
// GLOBAL: XWA 0x68C584
void* hudTex7;
// GLOBAL: XWA 0x68C588
void* hudTex8;
// GLOBAL: XWA 0x68C58C
void* hudTex9;
// GLOBAL: XWA 0x68C590
void* hudTex10;
// GLOBAL: XWA 0x68C594
void* hudTex11;
// GLOBAL: XWA 0x68C598
void* hudTex12;
// GLOBAL: XWA 0x68C59C
void* hudTex13;
// GLOBAL: XWA 0x68C5A4
void* hudTex15;
// GLOBAL: XWA 0x68C5A8
void* hudTex16;
// GLOBAL: XWA 0x68C5AC
void* hudTex17;
// GLOBAL: XWA 0x68C5B0
void* hudTex18;
// GLOBAL: XWA 0x68C5B4
void* hudTex19;
// GLOBAL: XWA 0x68C574
void* g_hudFpsCountPixels;
// GLOBAL: XWA 0x7712B0
int g_flightFpsOverlayMode;
// GLOBAL: XWA 0x6003E8
uint8_t g_filmOverlayMfdVisible = 1;
// GLOBAL: XWA 0x6003EC
uint8_t g_mfdLeftNeedsRedraw;
// GLOBAL: XWA 0x6003F0
uint8_t g_mfdRightNeedsRedraw;
// GLOBAL: XWA 0x692868
uint16_t g_panelBoxSpanScratch[1600];
// GLOBAL: XWA 0x5B4AE0
uint8_t g_mfdGoalsRedrawNeeded = 1;
// GLOBAL: XWA 0x686B40
uint8_t g_mfdGoalsCachedSecondaryStatus;
// GLOBAL: XWA 0x686B44
int g_mfdGoalsCurrentTotalLines;
// GLOBAL: XWA 0x686B48
int g_mfdGoalsCachedVisibleGoalCount;
// GLOBAL: XWA 0x686B4C
int g_mfdGoalsCachedMissionScore;
// GLOBAL: XWA 0x686B50
int g_mfdGoalsCachedLineCounts[8];
// GLOBAL: XWA 0x686B70
uint8_t g_mfdGoalsCachedPrimaryStatus;
// GLOBAL: XWA 0x686B74
int g_mfdGoalsCachedBonusScore;
// GLOBAL: XWA 0x686B78
int g_mfdGoalsLeftScrollTop;
// GLOBAL: XWA 0x686B7C
int g_mfdGoalsRightScrollTop;
// GLOBAL: XWA 0x686B80
int g_mfdGoalsLeftTotalLines;
// GLOBAL: XWA 0x686B84
int g_mfdGoalsRightTotalLines;
// GLOBAL: XWA 0x686B88
int g_mfdGoalsCurrentScrollTop;
// GLOBAL: XWA 0x686B8C
uint8_t g_mfdGoalsBothSidesShowingPage;
// GLOBAL: XWA 0x7CA200
int g_mfdGoalsLineCounts[8];
// GLOBAL: XWA 0x749B28
uint8_t g_unusedMfdMapLegendRightRedrawFlag;
typedef struct MfdFriendlyCraftPageState {
	uint16_t topRowIndex;
	uint16_t pad02;
	uint16_t cachedRowCount;
	uint16_t pad06;
	int currentTextTopY;
	uint16_t leftTopRowIndex;
	uint16_t pad0E;
	uint16_t rightTopRowIndex;
	uint16_t pad12;
	uint16_t leftTextTopY;
	uint16_t pad16;
	uint16_t rightTextTopY;
	uint16_t pad1A;
	uint8_t mirrorRedrawPending;
	uint8_t pad1D[3];
} MfdFriendlyCraftPageState;
// GLOBAL: XWA 0x749AE8
MfdFriendlyCraftPageState g_mfdFriendlyCraftPageState;
// GLOBAL: XWA 0x5BA320
const char g_mfdCraftListStatusLetters[11] = "CJNRFCJNRF";
// GLOBAL: XWA 0x5BA32C
int16_t g_mfdFriendlyCraftSelectedRowCache = -1;
// GLOBAL: XWA 0x68C8C8
uint16_t g_mfdFriendlyCraftShieldHullHeaderX;
// GLOBAL: XWA 0x68C8C4
uint16_t g_mfdFriendlyCraftLayoutInitOnlyX;
// GLOBAL: XWA 0x68C8CC
uint16_t g_mfdFriendlyCraftNameColumnWidth;
// GLOBAL: XWA 0x68C8D0
uint16_t g_mfdFriendlyCraftTargetColumnWidth;
typedef MfdFriendlyCraftPageState MfdFlightGroupsPageState;
// GLOBAL: XWA 0x749B08
MfdFlightGroupsPageState g_mfdFlightGroupsPageState;
// GLOBAL: XWA 0x5BA330
int16_t g_mfdFlightGroupsSelectedRowCache = -1;
// GLOBAL: XWA 0x68C8D4
uint16_t g_mfdFlightGroupsNameColumnWidth;
// GLOBAL: XWA 0x68C8D8
uint16_t g_mfdFlightGroupsShieldHullHeaderX;
// GLOBAL: XWA 0x68C8DC
uint16_t g_mfdFlightGroupsShieldHullValueX;
// GLOBAL: XWA 0x68C8E0
uint16_t g_mfdFlightGroupsTargetColumnX;
// GLOBAL: XWA 0x68C8E4
uint16_t g_mfdFlightGroupsTargetColumnWidth;
// GLOBAL: XWA 0x68C8E8
uint16_t g_mfdFlightGroupsStatusColumnX;
// GLOBAL: XWA 0x68C8EC
uint16_t g_mfdFlightGroupsStatusColumnWidth;
// GLOBAL: XWA 0x5B4A48
const uint16_t g_mfdGoalsDisplayStateBySectionType[4][3] = {
	{ 2, 1, 1 },
	{ 4, 0, 0 },
	{ 1, 0, 1 },
	{ 0, 4, 0 },
};
// GLOBAL: XWA 0x5B4A60
const uint16_t g_mfdGoalsCountAltStateBySectionType[4][3] = {
	{ 2, 1, 1 },
	{ 4, 0, 0 },
	{ 4, 0, 1 },
	{ 0, 4, 0 },
};
// GLOBAL: XWA 0x5B4A78
const uint16_t g_mfdGoalsConditionMaskBySectionType[4][3] = {
	{ 2, 1, 1 },
	{ 6, 0, 0 },
	{ 1, 0, 1 },
	{ 0, 6, 0 },
};
// GLOBAL: XWA 0x5B4A90
const uint8_t g_goalTitleColorByIndex[8] = {
	0x4a, 0x4e, 0x52, 0x46, 0, 0, 0, 0,
};
// GLOBAL: XWA 0x68C91C
int g_mfdMessageLogLeftScrollOffset;
// GLOBAL: XWA 0x68C920
int g_mfdMessageLogRightScrollOffset;
// GLOBAL: XWA 0x6937C0
int g_mfdMessageLogLastDrawTotalCount;
// GLOBAL: XWA 0x6937C4
uint8_t g_mfdMessageLogSharedPageLeftDrawn;
// GLOBAL: XWA 0x6937C8
int g_mfdMessageLogLeftWrappedLineCount;
// GLOBAL: XWA 0x6937CC
int g_mfdMessageLogRightWrappedLineCount;
// GLOBAL: XWA 0x9B6324
uint16_t g_messageLogDrawTotalCount;
// GLOBAL: XWA 0x749ADC
int g_mfdRaceScoreboardLastWidth;
// GLOBAL: XWA 0x749AE0
int g_mfdRaceScoreboardLastPlayerCount;
// GLOBAL: XWA 0x749AE4
int g_mfdRaceScoreboardFirstVisibleRow;
// GLOBAL: XWA 0x749AD0
int g_mfdMissionScoreboardLastWidth;
// GLOBAL: XWA 0x749AD4
int g_mfdMissionScoreboardLastPlayerCount;
// GLOBAL: XWA 0x749AD8
int g_mfdMissionScoreboardFirstVisibleRow;
// GLOBAL: XWA 0x68C898
uint8_t* g_cockpitMaskBitmap;
// GLOBAL: XWA 0x68C89C
uint8_t* g_cockpitMaskRle;
// GLOBAL: XWA 0x68BD20
HudCockpitMaskSprite g_hudCockpitMaskSprites[6] = {
	{ 0x4ee8, 0, 0 },           { 0x4e84, 0x0213, 0 }, { 0x4fb0, 0x005e, 0 },
	{ 0x50dc, 0x00c2, 0x0167 }, { 0x5014, 0x014f, 0 }, { 0x5078, 0x01b9, 0x014f },
};
// GLOBAL: XWA 0x5B6720
uint8_t g_radarEllipseClampTable[74];
// GLOBAL: XWA 0x68C8F0
int g_radarEllipseClampRadius;
// GLOBAL: XWA 0x68BDA8
RadarBlip g_radarForeBlipBufferA[48];
// GLOBAL: XWA 0x68BEC8
RadarBlip g_radarForeBlipBufferB[48];
// GLOBAL: XWA 0x68C1E0
RadarBlip g_radarAftBlipBufferA[48];
// GLOBAL: XWA 0x68C300
RadarBlip g_radarAftBlipBufferB[48];
// GLOBAL: XWA 0x68C858
RadarBlipCount g_radarForeBlipCount;
// GLOBAL: XWA 0x68C85C
RadarBlipCount g_radarAftBlipCount;
// GLOBAL: XWA 0x68C860
uint16_t g_radarForePrevBlipCount;
// GLOBAL: XWA 0x68C864
uint16_t g_radarAftPrevBlipCount;
// GLOBAL: XWA 0x68C868
RadarBlip* g_radarForeEraseBlips;
// GLOBAL: XWA 0x68C86C
RadarBlip* g_radarAftEraseBlips;
// GLOBAL: XWA 0x68C870
RadarBlip* g_radarForeDrawBlips;
// GLOBAL: XWA 0x68C874
RadarBlip* g_radarAftDrawBlips;
// GLOBAL: XWA 0x68C560
int g_hudRadarCenterOffsetX;
// GLOBAL: XWA 0x68C564
int g_hudRadarCenterY;
// GLOBAL: XWA 0x68C950
uint8_t g_radarBlipBufferParity;
// GLOBAL: XWA 0x68C954
uint8_t g_radarTargetMarkerBackgroundSaved;
// GLOBAL: XWA 0x68C8F4
int16_t radarx;
// GLOBAL: XWA 0x68C8F8
int16_t radary;
// GLOBAL: XWA 0x68C8FC
uint16_t g_radarTargetMarkerDrawX;
// GLOBAL: XWA 0x68C900
uint16_t g_radarTargetMarkerDrawY;
// GLOBAL: XWA 0x68C904
int16_t g_radarTargetMarkerRestoreX;
// GLOBAL: XWA 0x68C908
int16_t g_radarTargetMarkerRestoreY;

// ---- file-local data/type declarations hoisted for address-order layout ----

// GLOBAL: XWA 0x5A9780
// Top-level command-menu item -> menu row.
static const unsigned int g_mfdCommandMainMenuRowByItem[9] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
// GLOBAL: XWA 0x5A97A4
// Object-row command item -> command submenu row.
static const unsigned int g_mfdCommandSubMenuRowByItem[6] = { 0, 10, 20, 30, 40, 50 };

// HUD message id / voice-variant tables for the attack command, indexed by the
// command-menu item. XWA 0x5A97F0 and 0x5A9800.
static const uint16_t g_mfdAttackRadioMsgId[8] = { 162, 164, 165, 166, 167, 160, 161, 157 };
static const uint16_t g_mfdAttackRadioMsgArg[8] = { 6, 6, 6, 6, 11, 4, 5, 3 };

// GLOBAL: XWA 0x5B4B10
const uint16_t g_goalStatusConditionRowBlock[6] = {
	0, 2, 0, 0, 1, 3,
};

// GLOBAL: XWA 0x5B4AE8
const uint16_t g_goalAmountTextVariantByOp[20] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 10, 11, 12, 13,
};

enum {
	GOALS_TARGET_FLIGHT_GROUP = 1,
	GOALS_TARGET_SPECIES = 2,
	GOALS_TARGET_GENUS = 3,
	GOALS_TARGET_FAMILY = 4,
	GOALS_TARGET_IFF = 5,
	GOALS_TARGET_CRAFT_WHEN = 7,
	GOALS_TARGET_GLOBAL_GROUP = 8,
	GOALS_TARGET_AI_LEVEL = 9,
	GOALS_TARGET_STATUS = 10,
	GOALS_AMOUNT_SPECIAL_CARGO = 6,
	GOALS_AMOUNT_INSPECT_SPECIAL_CARGO = 7,
	GOALS_AMOUNT_ALL_BUT_ONE = 5,
	GOALS_AMOUNT_ALL_EXCEPT_PLAYER = 8
};

typedef struct HudOffscreenTargetMarker {
	int x;
	int y;
	uint16_t spriteId;
	uint8_t drawText;
	uint8_t targetBehind;
} HudOffscreenTargetMarker;

// ---- forward declarations for functions used before their definition ----
static int16_t goals_ExtraLineIfWouldWrap(const char* text);
static const char* goals_GetObjectTypeDisplayName(uint16_t objectType, int plural);
static __inline int Hud_AbsLookDegreesFromOffset(int16_t offset);
void Hud_DrawRadarBlips(void);
static inline CraftData* Hud_GetCraftPointerInlined(void);
static __inline CraftData*
Hud_GetCraftPointerInlinedWithDebug(void(XWA_HUD_STDCALL* debugOutput)(const char*));
static __inline void Hud_ClearSoftwareTextPane(void* surface, int width, int height);
static int16_t Hud_EmitBoxOverlayQuadHW(float left, float top, float right, float bottom, float depthZ,
										uint32_t color);
static void Hud_FlushBoxOverlayBatchHW(void);
static void Hud_FlushBoxOverlayBatchIfNeededHW(int neededVertices, int neededTriangles);
static int Hud_EmitClippedBoxOverlayQuadHW(int left, int top, int right, int bottom, float depthZ,
										   uint32_t color, int16_t* result);
static __inline void Mfd_SetupPaneRenderTarget(int mfdSide, void* mfdSurface);

static __inline int16_t Hud_EmitBoxOverlayQuadHW(float left, float top, float right, float bottom,
												 float depthZ, uint32_t color) {
	int baseVertex;
	int triIndex;
	int i;

	baseVertex = g_d3dVertexCount;

	g_flightVertexBuffer[baseVertex].sx = g_flightVpOriginX + left;
	g_flightVertexBuffer[baseVertex].sy = g_flightVpOriginY + top;
	g_flightVertexBuffer[baseVertex + 1].sx = g_flightVpOriginX + right;
	g_flightVertexBuffer[baseVertex + 1].sy = g_flightVpOriginY + top;
	g_flightVertexBuffer[baseVertex + 2].sx = g_flightVpOriginX + right;
	g_flightVertexBuffer[baseVertex + 2].sy = g_flightVpOriginY + bottom;
	g_flightVertexBuffer[baseVertex + 3].sx = g_flightVpOriginX + left;
	g_flightVertexBuffer[baseVertex + 3].sy = g_flightVpOriginY + bottom;

	for (i = 0; i < 4; ++i) {
		D3DTLVERTEX* vert;

		vert = &g_flightVertexBuffer[baseVertex + i];
		vert->sz = depthZ;
		vert->rhw = depthZ;
		vert->tu = 0.0f;
		vert->tv = 0.0f;
		vert->color = color;
		vert->specular = 0;
	}

	triIndex = g_d3dIndexCount;
	g_triBuffer[triIndex].v0 = baseVertex;
	g_triBuffer[triIndex].v1 = baseVertex + 1;
	g_triBuffer[triIndex].v2 = baseVertex + 2;
	g_triBuffer[triIndex].texture = NULL;
	g_triBuffer[triIndex].flags = 0x8000;
	g_triBuffer[triIndex + 1].v0 = baseVertex;
	g_triBuffer[triIndex + 1].v1 = baseVertex + 2;
	g_triBuffer[triIndex + 1].v2 = baseVertex + 3;
	g_triBuffer[triIndex + 1].texture = NULL;
	g_triBuffer[triIndex + 1].flags = 0x8000;

	g_d3dIndexCount += 2;
	g_d3dVertexCount += 4;
	return (int16_t)g_d3dVertexCount;
}

// FUNCTION: XWA 0x448BE0
int16_t Hud_DrawBoxOverlayHW(int x, int y, int width, int height, int colorIdx, int depth) {
	const RgbTriplet* rgb;
	uint32_t color;
	float depthZ;
	int right;
	int bottom;
	int cornerWidth;
	int cornerHeight;
	int16_t result = 0;

	if (depth == 1 && width == 4 && height == 4) {
		int left;
		int top;
		int markerRight;
		int markerBottom;

		left = x;
		top = y;
		markerRight = x + 4;
		markerBottom = y + 4;

		rgb = &g_swPalette[colorIdx];
		color = 4u * ((uint32_t)rgb->b + (((((uint32_t)rgb->r - 64u) << 8) + (uint32_t)rgb->g) << 8));
		Hud_FlushBoxOverlayBatchIfNeededHW(16, 8);
		result = (int16_t)g_d3dVertexCount;
		Hud_EmitClippedBoxOverlayQuadHW(left, top, markerRight, top + 1, 1.0f, color, &result);
		Hud_EmitClippedBoxOverlayQuadHW(left, markerBottom - 1, markerRight, markerBottom, 1.0f, color,
										&result);
		Hud_EmitClippedBoxOverlayQuadHW(left, top + 1, left + 1, markerBottom - 1, 1.0f, color, &result);
		Hud_EmitClippedBoxOverlayQuadHW(markerRight - 1, top + 1, markerRight, markerBottom - 1, 1.0f, color,
										&result);
		Hud_FlushBoxOverlayBatchHW();
		return result;
	}

	if ((g_players[g_localPlayer].mfd.enabled[0] || g_players[g_localPlayer].viewState.externalCameraActive ||
		 (g_filmPlaybackMode && g_filmOverlayActive == 1)) &&
		!g_inHangarReady && colorIdx == 59) {
		int centerX;
		int centerY;

		centerX = x + width / 2;
		if (centerX > 0 && centerX < (int16_t)g_screenWidth) {
			centerY = y + height / 2;
			if (centerY > 0 && centerY < (int16_t)g_screenHeight) {
				Hud_DrawTargetBoxReadout((int16_t)x, (int16_t)y, width, (int16_t)height);
			}
		}
	}

	Hud_FlushBoxOverlayBatchIfNeededHW(32, 16);

	right = x + width;
	bottom = y + height;
	cornerWidth = width >> 3;
	cornerHeight = height >> 3;
	if (cornerWidth < 3) {
		cornerWidth = 3;
	}
	if (cornerHeight < 3) {
		cornerHeight = 3;
	}
	if (cornerWidth > width) {
		cornerWidth = width;
	}
	if (cornerHeight > height) {
		cornerHeight = height;
	}

	rgb = &g_swPalette[colorIdx];
	color = 4u * ((uint32_t)rgb->b + (((((uint32_t)rgb->r - 64u) << 8) + (uint32_t)rgb->g) << 8));

	depthZ = g_depthProjScale / (g_depthProjScale + 1.0f);
	if (depthZ < 0.000015259022f) {
		depthZ = 0.000015259022f;
	}
	if (g_std3DZCmpMode == 2) {
		depthZ = 1.0f - depthZ;
	}

	result = (int16_t)y;
	if (y >= 0 && y < g_flightVpHeight) {
		int leftStart;
		int leftEnd;
		int rightStart;
		int rightEnd;

		leftStart = x;
		leftEnd = x + cornerWidth;
		if (leftStart < 0) {
			leftStart = 0;
		}
		if (leftEnd >= g_flightVpWidth) {
			leftEnd = g_flightVpWidth - 1;
		}
		if (leftStart < leftEnd) {
			result = Hud_EmitBoxOverlayQuadHW((float)leftStart, (float)y, (float)leftEnd, (float)(y + 1),
											  depthZ, color);
		}

		rightStart = right - cornerWidth;
		rightEnd = right;
		if (rightStart < 0) {
			rightStart = 0;
		}
		if (rightEnd >= g_flightVpWidth) {
			rightEnd = g_flightVpWidth - 1;
		}
		if (rightStart < rightEnd) {
			result = Hud_EmitBoxOverlayQuadHW((float)rightStart, (float)y, (float)rightEnd, (float)(y + 1),
											  depthZ, color);
		}
	}

	if (bottom >= 0) {
		result = (int16_t)g_flightVpHeight;
		if (bottom < g_flightVpHeight) {
			int leftStart;
			int leftEnd;
			int rightStart;
			int rightEnd;

			leftStart = x;
			leftEnd = x + cornerWidth;
			if (leftStart < 0) {
				leftStart = 0;
			}
			if (leftEnd >= g_flightVpWidth) {
				leftEnd = g_flightVpWidth - 1;
			}
			if (leftStart < leftEnd) {
				result = Hud_EmitBoxOverlayQuadHW((float)leftStart, (float)bottom, (float)leftEnd,
												  (float)(bottom + 1), depthZ, color);
			}

			rightStart = right - cornerWidth;
			rightEnd = right + 1;
			if (rightStart < 0) {
				rightStart = 0;
			}
			if (rightEnd >= g_flightVpWidth) {
				rightEnd = g_flightVpWidth - 1;
			}
			if (rightStart < rightEnd) {
				result = Hud_EmitBoxOverlayQuadHW((float)rightStart, (float)bottom, (float)rightEnd,
												  (float)(bottom + 1), depthZ, color);
			}
		}
	}

	if (x >= 0 && x < g_flightVpWidth) {
		int topStart;
		int topEnd;
		int bottomStart;
		int bottomEnd;

		topStart = y;
		topEnd = y + cornerHeight;
		if (topStart < 0) {
			topStart = 0;
		}
		if (topEnd >= g_flightVpHeight) {
			topEnd = g_flightVpHeight - 1;
		}
		if (topStart < topEnd) {
			result = Hud_EmitBoxOverlayQuadHW((float)x, (float)topStart, (float)(x + 1), (float)topEnd,
											  depthZ, color);
		}

		bottomStart = bottom - cornerHeight;
		bottomEnd = bottom;
		if (bottomStart < 0) {
			bottomStart = 0;
		}
		if (bottomEnd >= g_flightVpHeight) {
			bottomEnd = g_flightVpHeight - 1;
		}
		if (bottomStart < bottomEnd) {
			result = Hud_EmitBoxOverlayQuadHW((float)x, (float)bottomStart, (float)(x + 1), (float)bottomEnd,
											  depthZ, color);
		}
	}

	if (right >= 0) {
		result = (int16_t)g_flightVpWidth;
		if (right < g_flightVpWidth) {
			int topStart;
			int topEnd;
			int bottomStart;
			int bottomEnd;

			topStart = y;
			topEnd = y + cornerHeight;
			if (topStart < 0) {
				topStart = 0;
			}
			if (topEnd >= g_flightVpHeight) {
				topEnd = g_flightVpHeight - 1;
			}
			if (topStart < topEnd) {
				result = Hud_EmitBoxOverlayQuadHW((float)right, (float)topStart, (float)(right + 1),
												  (float)topEnd, depthZ, color);
			}

			bottomStart = bottom - cornerHeight;
			bottomEnd = bottom;
			if (bottomStart < 0) {
				bottomStart = 0;
			}
			if (bottomEnd >= g_flightVpHeight) {
				bottomEnd = g_flightVpHeight - 1;
			}
			if (bottomStart < bottomEnd) {
				result = Hud_EmitBoxOverlayQuadHW((float)right, (float)bottomStart, (float)(right + 1),
												  (float)bottomEnd, depthZ, color);
			}
		}
	}

	return result;
}

static __inline int Mfd_GoalsShouldSkipSection(int sectionIdx, uint16_t playerIff) {
	return sectionIdx == 1 && !g_inHangarReady &&
		   (g_missionFlightRuntimeState.teamGoalStatus[playerIff][TEAM_GOAL_SECONDARY] == 1 ||
			g_missionFlightRuntimeState.teamGoalStatus[playerIff][TEAM_GOAL_PRIMARY] == 2);
}

static __inline int Mfd_GoalsTriggerCanShowPercent(uint16_t condition) {
	return condition != 6 && condition != 7 && condition != 30 && condition != 31 && condition != 32 &&
		   condition != 33;
}

static __inline uint8_t Mfd_GoalsEvaluateTriggerForSection(const XwaTrigger* trigger,
														   const XwaTriggerPair* pair, int triggerIdx,
														   uint16_t playerIff, int16_t sectionIdx,
														   uint16_t goalKind, uint8_t activeSequence) {
	uint16_t teamOrVariable;
	uint8_t result;

	if (trigger->condition == 0 || trigger->condition == 10 || trigger->condition == 39) {
		return 0;
	}

	teamOrVariable = playerIff;
	if (triggerIdx == 0 && pair->triggers[1].condition == 39 &&
		pair->triggers[1].variableType == TRIGVAR_TEAM) {
		teamOrVariable = pair->triggers[1].variable;
	}

	result = Mission_EvaluateCondition(trigger, 0, teamOrVariable);
	if ((result & g_mfdGoalsConditionMaskBySectionType[sectionIdx][goalKind]) == 0) {
		return 0;
	}
	if (result == 4 && activeSequence != 0 &&
		activeSequence != g_missionFlightRuntimeState.teamActiveGoalSequence[playerIff]) {
		return 0;
	}
	return result;
}

static __inline void Mfd_GoalsDrawSectionTitleIfNeeded(int sectionIdx, uint8_t* titlePending, int* lineIndex,
													   int* cursorY, int lineStep, int lastVisibleLine) {
	if (*titlePending) {
		if (*lineIndex >= g_mfdGoalsCurrentScrollTop && *lineIndex < lastVisibleLine) {
			FlightText_SetCursor(0, (int16_t)*cursorY);
			FlightText_DrawStringCentered(g_strGoalTitles[sectionIdx]);
			*cursorY += lineStep;
		}
		*titlePending = 0;
		++*lineIndex;
	}
}

static __inline int Mfd_GoalsAdvanceLineIndex(int lineIndex, int16_t extraHeight, int lineStep) {
	if ((uint16_t)extraHeight > 2 * lineStep) {
		return lineIndex + (uint16_t)extraHeight / lineStep;
	}
	return lineIndex + (extraHeight != 0 ? 2 : 1);
}

static __inline int16_t Mfd_GoalsDrawOverrideText(const char* text, uint16_t condition, uint16_t totalCount,
												  uint16_t currentCount, int sectionIdx, int* cursorY,
												  int includePercent, char* buffer) {
	int16_t savedX;
	int16_t extraHeight;

	savedX = g_flightCursorX;
	extraHeight = 0;
	FlightText_DrawString(text);
	if (!includePercent || totalCount <= 1 || !Mfd_GoalsTriggerCanShowPercent(condition)) {
		g_flightDrawCharFn('\n');
		g_flightCursorX = savedX;
		if ((int)g_flightCursorX + (int)FlightText_MeasureStringWidth(text) >
			(int)g_flightClipRight - (uint16_t)g_hudMfdTextInsetX) {
			extraHeight = (uint16_t)g_flightFontLineHeight + 1;
		}
		*cursorY += (uint8_t)g_flightFontLineHeight + extraHeight + 2;
		return extraHeight;
	}

	sprintf(buffer, " (%ld%%)", (long)(100 * currentCount / totalCount));
	if (sectionIdx == 0 || sectionIdx == 3) {
		FlightText_SetColor(0x4au);
	} else if (sectionIdx == 2 || sectionIdx == 1) {
		FlightText_SetColor(0x52u);
	} else {
		FlightText_SetColor(0x43u);
	}
	FlightText_SetScratch(text);
	FlightText_AppendScratchString(buffer);
	FlightText_DrawString(buffer);
	FlightText_SetColor(g_goalTitleColorByIndex[sectionIdx]);
	g_flightDrawCharFn('\n');
	g_flightCursorX = savedX;
	if ((int)g_flightCursorX + (int)FlightText_MeasureStringWidth(g_flightTextScratchBuffer) >
		(int)g_flightClipRight - (uint16_t)g_hudMfdTextInsetX) {
		extraHeight = (uint16_t)g_flightFontLineHeight + 1;
	}
	*cursorY += (uint8_t)g_flightFontLineHeight + extraHeight + 2;
	return extraHeight;
}

static __inline int16_t Mfd_GoalsDrawGeneratedGoal(uint16_t targetId, uint16_t condition, uint16_t targetType,
												   uint16_t goalStatus, uint16_t amountOp,
												   uint16_t timeLimit5SecUnits, int percentComplete,
												   int sectionIdx, int* cursorY, int lineStep) {
	int16_t height;

	height = goals_outputgoal(targetId, condition, targetType, goalStatus, amountOp, timeLimit5SecUnits, NULL,
							  percentComplete, sectionIdx);
	*cursorY += height;
	if ((uint16_t)height <= lineStep) {
		height = 0;
	}
	return height;
}

static __inline void Mfd_GoalsUpdateScrollAndCaches(int mfdSide, int visibleLineCount, int paneHeight,
													int lineStep) {
	int playerIdx;
	int16_t i;

	if ((mfdSide == 1 && g_mfdLeftNeedsRedraw) || (mfdSide == 2 && g_mfdRightNeedsRedraw)) {
		if (mfdSide == 1) {
			g_mfdGoalsLeftScrollTop = 0;
			g_mfdGoalsCurrentScrollTop = 0;
			g_mfdGoalsRedrawNeeded = 1;
			g_mfdGoalsCachedVisibleGoalCount = 0;
			g_mfdGoalsCurrentTotalLines = 0;
			g_mfdGoalsCachedPrimaryStatus = -1;
			g_mfdGoalsCachedSecondaryStatus = -1;
			g_mfdGoalsCachedMissionScore = 0;
			g_mfdGoalsCachedBonusScore = 0;
			g_mfdLeftNeedsRedraw = 0;
		}
		if (mfdSide == 2) {
			g_mfdGoalsRightScrollTop = 0;
			g_mfdGoalsCurrentScrollTop = 0;
			g_mfdGoalsRedrawNeeded = 1;
			g_mfdGoalsCachedVisibleGoalCount = 0;
			g_mfdGoalsCurrentTotalLines = 0;
			g_mfdGoalsCachedPrimaryStatus = -1;
			g_mfdGoalsCachedSecondaryStatus = -1;
			g_mfdGoalsCachedMissionScore = 0;
			g_mfdGoalsCachedBonusScore = 0;
			g_mfdRightNeedsRedraw = 0;
		}
	} else {
		playerIdx = g_localPlayer;
		if (mfdSide == g_players[playerIdx].mfd.activeIndex) {
			switch (g_currentActionKey) {
				case 0x00a6:
					if (g_mfdGoalsCurrentScrollTop > 0) {
						if (mfdSide == 1) {
							g_mfdGoalsCurrentScrollTop = --g_mfdGoalsLeftScrollTop;
						} else if (mfdSide == 2) {
							g_mfdGoalsCurrentScrollTop = --g_mfdGoalsRightScrollTop;
						}
						g_mfdGoalsRedrawNeeded = 1;
					}
					break;

				case 0x00a7:
					if (g_mfdGoalsCurrentScrollTop <=
						g_mfdGoalsCurrentTotalLines - (int16_t)paneHeight / (int16_t)lineStep) {
						if (mfdSide == 1) {
							++g_mfdGoalsLeftScrollTop;
							++g_mfdGoalsCurrentScrollTop;
						} else if (mfdSide == 2) {
							++g_mfdGoalsRightScrollTop;
							++g_mfdGoalsCurrentScrollTop;
						}
						g_mfdGoalsRedrawNeeded = 1;
					}
					break;
			}
		}

		if (visibleLineCount != g_mfdGoalsCachedVisibleGoalCount) {
			g_mfdGoalsCachedVisibleGoalCount = visibleLineCount;
			g_mfdGoalsRedrawNeeded = 1;
		}
		for (i = 0; i < 8; ++i) {
			if (g_mfdGoalsCachedLineCounts[i] != g_mfdGoalsLineCounts[i]) {
				g_mfdGoalsRedrawNeeded = 1;
				break;
			}
		}
		if (g_mfdGoalsCachedPrimaryStatus !=
				g_missionFlightRuntimeState
					.teamGoalStatus[(uint16_t)g_players[playerIdx].playerIff][TEAM_GOAL_PRIMARY] ||
			g_mfdGoalsCachedSecondaryStatus !=
				g_missionFlightRuntimeState
					.teamGoalStatus[(uint16_t)g_players[playerIdx].playerIff][TEAM_GOAL_SECONDARY]) {
			g_mfdGoalsRedrawNeeded = 1;
		}
		if (g_mfdGoalsCachedMissionScore != g_players[playerIdx].missionStats.missionScore ||
			g_mfdGoalsCachedBonusScore !=
				(g_players[playerIdx].missionStats.missionBonusScoreTenths +
				 g_missionFlightRuntimeState
					 .teamScores[TEAM_SCORE_BONUS_TENTHS][(uint16_t)g_players[playerIdx].playerIff]) /
					10) {
			g_mfdGoalsRedrawNeeded = 1;
		}
	}
}

// FUNCTION: XWA 0x4531F0
void Mfd_DrawMissionGoalsPage(int mfdSide, void* mfdSurface) {
	uint16_t playerIff;
	uint16_t paneWidth;
	int16_t paneHeight;
	int lineStep;
	int visibleLineCount;
	int lastVisibleLine;
	int cursorY;
	int lineIndex;
	char buffer[80];
	int16_t sectionIdx;
	uint16_t goalKind;
	int pairIdx;
	int triggerIdx;
	uint16_t goalIdx;
	int16_t fgIdx;

	paneWidth = g_hudMfdPaneWidth;
	paneHeight = g_hudMfdPaneHeight;
	visibleLineCount = 0;

	FlightText_SetFontTier(0);
	lineStep = (uint8_t)g_flightFontLineHeight + 2;
	if (mfdSide == 2 && g_mfdGoalsBothSidesShowingPage) {
		g_mfdGoalsRedrawNeeded = 1;
	}
	if (mfdSide == 1) {
		g_mfdGoalsCurrentTotalLines = g_mfdGoalsLeftTotalLines;
	} else {
		g_mfdGoalsCurrentTotalLines = g_mfdGoalsRightTotalLines;
	}
	if (mfdSide == 1) {
		g_mfdGoalsCurrentScrollTop = g_mfdGoalsLeftScrollTop;
	} else {
		g_mfdGoalsCurrentScrollTop = g_mfdGoalsRightScrollTop;
	}

	playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
	for (sectionIdx = 0; sectionIdx < 4; ++sectionIdx) {
		g_mfdGoalsLineCounts[sectionIdx] = 0;
		if (Mfd_GoalsShouldSkipSection(sectionIdx, playerIff)) {
			continue;
		}

		for (goalKind = 0; goalKind <= 2u; ++goalKind) {
			uint16_t globalState;
			uint16_t displayState;

			globalState = g_missionFlightRuntimeState.teamGlobalGoalState[playerIff][goalKind];
			if (globalState != 0 &&
				(globalState == g_mfdGoalsDisplayStateBySectionType[sectionIdx][goalKind] ||
				 globalState == g_mfdGoalsCountAltStateBySectionType[sectionIdx][goalKind])) {
				XwaGlobalGoal* goal;
				const XwaTriggerPair* pair;

				goal = &g_missionGlobalGoals[playerIff][goalKind];
				pair = &goal->triggerPairs[0];
				for (pairIdx = 0; pairIdx < 2; ++pairIdx) {
					const XwaTrigger* trigger;

					trigger = &pair->triggers[0];
					for (triggerIdx = 0; triggerIdx < 2; ++triggerIdx, ++trigger) {
						uint16_t teamOrVariable;
						uint8_t result;

						if (trigger->condition == 39 || trigger->condition == 10 || trigger->condition == 0) {
							continue;
						}
						teamOrVariable = playerIff;
						if (triggerIdx == 0 && pair->triggers[1].condition == 39 &&
							pair->triggers[1].variableType == TRIGVAR_TEAM) {
							teamOrVariable = pair->triggers[1].variable;
						}
						result = Mission_EvaluateCondition(trigger, 0, teamOrVariable);
						if ((result & g_mfdGoalsConditionMaskBySectionType[sectionIdx][goalKind]) != 0 &&
							(result != 4 || goal->activeSequence == 0 ||
							 goal->activeSequence ==
								 g_missionFlightRuntimeState.teamActiveGoalSequence[playerIff])) {
							++g_mfdGoalsLineCounts[sectionIdx];
							if (goalKind == 2) {
								++g_mfdGoalsLineCounts[sectionIdx];
							}
						}
					}
					pair = &goal->triggerPairs[1];
				}
			}

			displayState = g_mfdGoalsDisplayStateBySectionType[sectionIdx][goalKind];
			for (goalIdx = 0; goalIdx < 8u; ++goalIdx) {
				int countFgIdx;

				for (countFgIdx = 0; countFgIdx < (int16_t)g_missionHeader.numFlightGroups; ++countFgIdx) {
					const XwaFlightGroupGoalPayload* goal;
					uint8_t activeSequence;

					goal = &g_missionFlightGroups[countFgIdx].fg.fgGoals[goalIdx].payload;
					if (goal->enabledForTeam[playerIff] == 0 ||
						!g_missionFgStats[countFgIdx].arrivalEnabled || goal->argument != goalKind ||
						g_missionFgStats[countFgIdx].goalState[8u * playerIff + goalIdx] != displayState) {
						continue;
					}

					activeSequence = goal->activeSequence;
					if (activeSequence >= 7u) {
						if (displayState == 2 || displayState == 1) {
							++g_mfdGoalsLineCounts[sectionIdx];
						}
					} else {
						if (goalKind == 2) {
							if (sectionIdx != 0 || 250 * (int)goal->points >= 0) {
								if (sectionIdx == 2 && 250 * (int)goal->points >= 0) {
									g_mfdGoalsLineCounts[2] += 2;
								}
							} else {
								g_mfdGoalsLineCounts[0] += 2;
							}
						} else if (displayState == 2 || displayState == 1 || activeSequence == 0 ||
								   activeSequence ==
									   g_missionFlightRuntimeState.teamActiveGoalSequence[playerIff]) {
							++g_mfdGoalsLineCounts[sectionIdx];
						}
					}
				}
			}

			if (g_mfdGoalsLineCounts[sectionIdx] != 0) {
				++g_mfdGoalsLineCounts[sectionIdx];
			}
			visibleLineCount += g_mfdGoalsLineCounts[sectionIdx];
		}
	}
	Mfd_GoalsUpdateScrollAndCaches(mfdSide, visibleLineCount, paneHeight, lineStep);

	if ((int16_t)g_playerFlightTransientTimers[g_localPlayer].field_0E <= 0) {
		g_playerFlightTransientTimers[g_localPlayer].field_0E = 472;
		g_mfdGoalsRedrawNeeded = 1;
	}

	lastVisibleLine = paneHeight / (int16_t)lineStep + g_mfdGoalsCurrentScrollTop - 2;

	if (g_mfdGoalsRedrawNeeded || g_useHardware3D) {
		if (g_useHardware3D) {
			if (mfdSide == 1) {
				FlightText_SetRenderOffset((int16_t)g_hudMfdTextInsetX,
										   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
			} else {
				FlightText_SetRenderOffset((int16_t)(g_screenWidth - g_hudMfdPaneWidth - g_hudMfdTextInsetX),
										   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
			}
		} else {
			FlightSw_SetRenderTarget(mfdSurface, (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
									 g_hudMfdPaneWidth * g_flight16bppBytesPerPixel);
		}
		FlightText_SetClipRect(0, 0, paneWidth, paneHeight);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_SetWordWrap(1);
		FlightText_SetClearLineBackground(1u);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		{
			int playerIdx;
			uint16_t headerPlayerIff;
			int16_t statusTitleIdx;
			int color;
			int bonusScoreTenths;

			FlightText_SetScratch(g_strGoalTitles[4]);
			playerIdx = g_localPlayer;
			headerPlayerIff = (uint16_t)g_players[playerIdx].playerIff;
			statusTitleIdx = 4;
			color = 0x43u;

			switch (g_missionFlightRuntimeState.teamGoalStatus[headerPlayerIff][TEAM_GOAL_PRIMARY]) {
				case 0:
					switch (
						g_missionFlightRuntimeState.teamGoalStatus[headerPlayerIff][TEAM_GOAL_SECONDARY]) {
						case 0:
						case 2:
							color = 0x4eu;
							statusTitleIdx = 7;
							break;
						case 1:
							color = 0x4au;
							statusTitleIdx = 6;
							break;
					}
					break;

				case 1:
					switch (
						g_missionFlightRuntimeState.teamGoalStatus[headerPlayerIff][TEAM_GOAL_SECONDARY]) {
						case 0:
						case 2:
							color = 0x52u;
							statusTitleIdx = 5;
							break;
						case 1:
							color = 0x46u;
							statusTitleIdx = 8;
							break;
					}
					break;

				case 2:
					color = 0x4au;
					statusTitleIdx = 6;
					break;
			}

			FlightText_AppendScratchChar(' ');
			FlightText_AppendScratchString(g_strGoalTitles[statusTitleIdx]);
			FlightText_SetCursor(0, 0);
			FlightText_SetColor(color);
			FlightText_DrawStringCentered(g_flightTextScratchBuffer);

			bonusScoreTenths =
				g_players[g_localPlayer].missionStats.missionBonusScoreTenths +
				g_missionFlightRuntimeState
					.teamScores[TEAM_SCORE_BONUS_TENTHS][(uint16_t)g_players[g_localPlayer].playerIff];
			g_mfdGoalsCachedMissionScore = g_players[g_localPlayer].missionStats.missionScore;
			g_mfdGoalsCachedBonusScore = bonusScoreTenths / 10;
			sprintf(buffer, "%s: %ld   %s %ld", g_strOverlayStrings[11], (long)g_mfdGoalsCachedMissionScore,
					g_strOverlayStrings[22], (long)g_mfdGoalsCachedBonusScore);
			FlightText_SetCursor(0, (int16_t)lineStep);
			FlightText_SetColor(color);
			FlightText_DrawStringCentered(buffer);
			cursorY = 2 * lineStep;
		}

		lineIndex = 0;
		for (sectionIdx = 0; sectionIdx < 4; ++sectionIdx) {
			uint8_t titlePending;

			if (Mfd_GoalsShouldSkipSection(sectionIdx, playerIff)) {
				continue;
			}

			FlightText_SetColor(g_goalTitleColorByIndex[sectionIdx]);
			titlePending = 1;
			for (goalKind = 0; goalKind <= 2u; ++goalKind) {
				uint16_t globalState;
				uint16_t displayState;

				if (g_mfdGoalsLineCounts[sectionIdx] == 0) {
					continue;
				}

				globalState = g_missionFlightRuntimeState.teamGlobalGoalState[playerIff][goalKind];
				if (globalState != 0 &&
					(globalState == g_mfdGoalsDisplayStateBySectionType[sectionIdx][goalKind] ||
					 globalState == g_mfdGoalsCountAltStateBySectionType[sectionIdx][goalKind])) {
					XwaGlobalGoal* goal;
					const XwaTriggerPair* pair;
					int16_t triggerOrdinal;
					uint8_t bonusPrefixWidth;

					goal = &g_missionGlobalGoals[playerIff][goalKind];
					pair = &goal->triggerPairs[0];
					triggerOrdinal = 0;
					bonusPrefixWidth = 0;
					for (pairIdx = 0; pairIdx < 2; ++pairIdx) {
						const XwaTrigger* trigger;

						trigger = &pair->triggers[0];
						for (triggerIdx = 0; triggerIdx < 2; ++triggerIdx, ++triggerOrdinal, ++trigger) {
							uint16_t condition;
							uint16_t targetType;
							uint16_t targetId;
							uint16_t amountOp;

							condition = trigger->condition;
							targetType = trigger->variableType;
							targetId = trigger->variable;
							amountOp = trigger->amount;
							if (!Mfd_GoalsEvaluateTriggerForSection(trigger, pair, triggerIdx, playerIff,
																	sectionIdx, goalKind,
																	goal->activeSequence)) {
								continue;
							}

							if (goalKind == 2 && bonusPrefixWidth == 0) {
								int points;

								points = 250 * (int)g_missionGlobalGoals[playerIff][2].rawPoints;
								if ((sectionIdx == 0 && points >= 0) || (sectionIdx == 2 && points < 0)) {
									goto draw_flight_group_goals;
								}
								Mfd_GoalsDrawSectionTitleIfNeeded(sectionIdx, &titlePending, &lineIndex,
																  &cursorY, lineStep, lastVisibleLine);
								FlightText_SetCursor(0, (int16_t)cursorY);
								sprintf(buffer, "(%s %ld) ", g_strOverlayStrings[(points < 0) + 22],
										(long)(points / 10));
								FlightText_SetColor(points >= 0 ? 0x52u : 0x4au);
								FlightText_DrawString(buffer);
								FlightText_SetColor(g_goalTitleColorByIndex[sectionIdx]);
								bonusPrefixWidth = FlightText_MeasureStringWidth(buffer);
							}

							Mfd_GoalsDrawSectionTitleIfNeeded(sectionIdx, &titlePending, &lineIndex, &cursorY,
															  lineStep, lastVisibleLine);
							if (lineIndex >= g_mfdGoalsCurrentScrollTop && lineIndex <= lastVisibleLine) {
								MemoryHandle handle;
								const char* overrideText;
								uint16_t goalStatus;
								int extraHeight;
								int percentComplete;
								int countIndex;

								handle = g_globalGoalOverrideStringHandles
									[playerIff][goalKind][triggerOrdinal]
									[g_mfdGoalsDisplayStateBySectionType[sectionIdx][goalKind] & 3u];
								overrideText = handle != 0 ? (const char*)Memory_LockHandle(handle) : NULL;
								FlightText_SetCursor(0, (int16_t)cursorY);
								goalStatus = (sectionIdx == 2 && goalKind == 1)
												 ? 5u
												 : g_mfdGoalsDisplayStateBySectionType[sectionIdx][goalKind];
								if (goalKind == 2) {
									g_flightCursorX += bonusPrefixWidth;
								}
								countIndex = triggerOrdinal;
								if (overrideText != NULL) {
									extraHeight = Mfd_GoalsDrawOverrideText(
										overrideText, condition,
										g_missionFlightRuntimeState
											.globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_TOTAL][playerIff]
																	[goalKind][countIndex],
										g_missionFlightRuntimeState
											.globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_CURRENT][playerIff]
																	[goalKind][countIndex],
										sectionIdx, &cursorY, 1, buffer);
								} else {
									if (g_missionFlightRuntimeState
												.globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_TOTAL]
																		[playerIff][goalKind][countIndex] >
											1u &&
										Mfd_GoalsTriggerCanShowPercent(condition)) {
										percentComplete =
											100 *
											g_missionFlightRuntimeState
												.globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_CURRENT]
																		[playerIff][goalKind][countIndex] /
											g_missionFlightRuntimeState
												.globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_TOTAL]
																		[playerIff][goalKind][countIndex];
									} else {
										percentComplete = -1;
									}
									extraHeight = Mfd_GoalsDrawGeneratedGoal(
										targetId, condition, targetType, goalStatus, amountOp, 0,
										percentComplete, sectionIdx, &cursorY, lineStep);
								}
								if (overrideText != NULL) {
									Memory_UnlockHandle(
										g_globalGoalOverrideStringHandles
											[playerIff][goalKind][triggerOrdinal]
											[g_mfdGoalsDisplayStateBySectionType[sectionIdx][goalKind] & 3u]);
								}
								lineIndex = Mfd_GoalsAdvanceLineIndex(lineIndex, extraHeight, lineStep);
							} else {
								++lineIndex;
							}
						}
						pair = &goal->triggerPairs[1];
					}
				}

			draw_flight_group_goals:
				displayState = g_mfdGoalsDisplayStateBySectionType[sectionIdx][goalKind];
				for (goalIdx = 0; goalIdx < 8u; ++goalIdx) {
					for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
						const XwaFlightGroupGoalPayload* goal;
						uint8_t activeSequence;
						int points;

						goal = &g_missionFlightGroups[fgIdx].fg.fgGoals[goalIdx].payload;
						if (goal->enabledForTeam[playerIff] == 0 || !g_missionFgStats[fgIdx].arrivalEnabled ||
							goal->argument != goalKind ||
							g_missionFgStats[fgIdx].goalState[8u * playerIff + goalIdx] != displayState ||
							goal->condition == 10 || goal->condition == 0) {
							continue;
						}

						activeSequence = goal->activeSequence;
						if (activeSequence >= 7u && displayState != 2 && displayState != 1) {
							continue;
						}
						points = 250 * (int)goal->points;
						if (goalKind == 2 &&
							((sectionIdx == 0 && points >= 0) || (sectionIdx == 2 && points < 0))) {
							continue;
						}
						if (displayState != 2 && displayState != 1 && activeSequence != 0 &&
							activeSequence != g_missionFlightRuntimeState.teamActiveGoalSequence[playerIff]) {
							continue;
						}

						Mfd_GoalsDrawSectionTitleIfNeeded(sectionIdx, &titlePending, &lineIndex, &cursorY,
														  lineStep, lastVisibleLine);
						if (lineIndex >= g_mfdGoalsCurrentScrollTop && lineIndex <= lastVisibleLine) {
							MemoryHandle handle;
							const char* overrideText;
							uint16_t goalStatus;
							int extraHeight;

							handle = g_missionFgOverrideStringHandles[fgIdx][goalIdx][displayState & 3u];
							overrideText = handle != 0 ? (const char*)Memory_LockHandle(handle) : NULL;
							FlightText_SetCursor(0, (int16_t)cursorY);
							goalStatus = (sectionIdx == 2 && goalKind == 1) ? 5u : displayState;
							if (goalKind == 2) {
								points = 250 * (int)goal->points;
								sprintf(buffer, "(%s %ld) ", g_strOverlayStrings[(points < 0) + 22],
										(long)(points / 10));
								FlightText_SetColor(points >= 0 ? 0x52u : 0x4au);
								FlightText_DrawString(buffer);
								FlightText_SetColor(g_goalTitleColorByIndex[sectionIdx]);
							}
							if (overrideText != NULL) {
								extraHeight = Mfd_GoalsDrawOverrideText(overrideText, goal->condition, 0, 0,
																		sectionIdx, &cursorY, 0, buffer);
							} else {
								extraHeight = Mfd_GoalsDrawGeneratedGoal(
									(uint16_t)fgIdx, goal->condition, 1u, goalStatus, goal->amount,
									goal->parameter, -1, sectionIdx, &cursorY, lineStep);
							}
							if (overrideText != NULL) {
								Memory_UnlockHandle(
									g_missionFgOverrideStringHandles[fgIdx][goalIdx][displayState & 3u]);
							}
							lineIndex = Mfd_GoalsAdvanceLineIndex(lineIndex, extraHeight, lineStep);
						} else {
							++lineIndex;
						}
					}
				}
			}
		}

		if (mfdSide == 1) {
			g_mfdGoalsLeftTotalLines = lineIndex;
		} else {
			g_mfdGoalsRightTotalLines = lineIndex;
		}
		playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
		memcpy(g_mfdGoalsCachedLineCounts, g_mfdGoalsLineCounts, sizeof(g_mfdGoalsCachedLineCounts));
		g_mfdGoalsCachedPrimaryStatus =
			g_missionFlightRuntimeState.teamGoalStatus[playerIff][TEAM_GOAL_PRIMARY];
		g_mfdGoalsCachedSecondaryStatus =
			g_missionFlightRuntimeState.teamGoalStatus[playerIff][TEAM_GOAL_SECONDARY];
	}

	if (mfdSide == 1 && g_players[g_localPlayer].mfd.page[1] == g_players[g_localPlayer].mfd.page[2]) {
		g_mfdGoalsBothSidesShowingPage = 1;
	}
	if (mfdSide == 2 && g_mfdGoalsBothSidesShowingPage) {
		g_mfdGoalsBothSidesShowingPage = 0;
	}
	g_mfdGoalsRedrawNeeded = 0;
	FlightText_SetWordWrap(0);
	FlightText_SetClearLineBackground(0);
	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		if (mfdSide == 1) {
			Blit16ToFlightSurface(mfdSurface, g_flightColorEscapeBypassChar, 0, 0, 5u,
								  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight),
								  (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
								  (uint16_t)(g_flight16bppBytesPerPixel * g_hudMfdPaneWidth));
		} else {
			Blit16ToFlightSurface(mfdSurface, g_flightColorEscapeBypassChar, 0, 0,
								  (uint16_t)(g_screenWidth - g_hudMfdPaneWidth),
								  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight),
								  (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
								  (uint16_t)(g_flight16bppBytesPerPixel * g_hudMfdPaneWidth));
		}
	}
}

// FUNCTION: XWA 0x4547B0
int16_t goals_outputgoal(uint16_t targetId, uint16_t condition, uint16_t targetType, uint16_t goalStatus,
						 uint16_t amountOp, uint16_t timeLimit5SecUnits, const char* conditionTextOverride,
						 int percentComplete, int goalTitleIndex) {
	int16_t lineHeight;
	int16_t consumedHeight;
	int amountTextVariant;
	const char* text;
	char timeText[32];
	char percentText[80];

	goalStatus = (uint16_t)(47 * g_goalStatusConditionRowBlock[goalStatus]);
	amountTextVariant = g_goalAmountTextVariantByOp[amountOp];
	lineHeight = (int16_t)((uint16_t)g_flightFontLineHeight + 2u);
	consumedHeight = lineHeight;

	if (targetType == GOALS_TARGET_FLIGHT_GROUP) {
		if (amountOp == GOALS_AMOUNT_SPECIAL_CARGO) {
			uint16_t objectType;

			objectType =
				g_objectTypeTables.craftTypeToObjectType[g_missionFlightGroups[targetId].fg.craftType];
			text = goals_GetObjectTypeDisplayName(objectType, 0);
			if (text != NULL) {
				(void)FlightText_MeasureStringWidth(text);
			}
			FlightText_DrawString(text);
			g_flightDrawCharFn(' ');
			FlightText_DrawString(g_missionFlightGroups[targetId].fg.name);
			g_flightDrawCharFn(' ');
			if (g_missionFgStats[targetId].specialCargoOutcome[8]) {
				g_flightDrawCharFn(g_missionFlightGroups[targetId].fg.specialCargoCraft + '1');
			} else {
				g_flightDrawCharFn('?');
			}
			FlightText_DrawString(": ");
			if (conditionTextOverride != NULL) {
				consumedHeight = (int16_t)(lineHeight + goals_ExtraLineIfWouldWrap(conditionTextOverride));
				FlightText_DrawString(conditionTextOverride);
			} else {
				consumedHeight =
					(int16_t)(lineHeight +
							  goals_DrawConditionText(g_missionFlightGroups[targetId].fg.craftType, condition,
													  amountTextVariant, (int16_t)goalStatus));
			}
		} else if (amountOp == GOALS_AMOUNT_INSPECT_SPECIAL_CARGO) {
			uint16_t objectType;

			objectType =
				g_objectTypeTables.craftTypeToObjectType[g_missionFlightGroups[targetId].fg.craftType];
			text = goals_GetObjectTypeDisplayName(objectType, 0);
			if (text != NULL) {
				(void)FlightText_MeasureStringWidth(text);
			}
			FlightText_DrawString(text);
			g_flightDrawCharFn(' ');
			FlightText_DrawString(g_missionFlightGroups[targetId].fg.name);
			g_flightDrawCharFn(' ');
			if (g_missionFgStats[targetId].specialCargoOutcome[8]) {
				g_flightDrawCharFn(g_missionFlightGroups[targetId].fg.specialCargoCraft + '1');
			} else {
				g_flightDrawCharFn('?');
			}
			FlightText_DrawString(": ");
			if (conditionTextOverride != NULL) {
				consumedHeight = (int16_t)(lineHeight + goals_ExtraLineIfWouldWrap(conditionTextOverride));
				FlightText_DrawString(conditionTextOverride);
			} else {
				consumedHeight =
					(int16_t)(lineHeight +
							  goals_DrawConditionText(g_missionFlightGroups[targetId].fg.craftType, condition,
													  amountTextVariant, (int16_t)goalStatus));
			}
		} else if (g_missionFgStats[targetId].outcomeCount[0] > 1u) {
			uint16_t objectType;
			int16_t usePlural;

			usePlural =
				(amountOp == GOALS_AMOUNT_ALL_BUT_ONE || amountOp == GOALS_AMOUNT_ALL_EXCEPT_PLAYER) ? 10 : 0;
			objectType =
				g_objectTypeTables.craftTypeToObjectType[g_missionFlightGroups[targetId].fg.craftType];
			text = goals_GetObjectTypeDisplayName(objectType, usePlural);
			if (text != NULL) {
				(void)FlightText_MeasureStringWidth(text);
			}
			FlightText_DrawString(text);
			g_flightDrawCharFn(' ');
			FlightText_DrawString(g_missionFlightGroups[targetId].fg.name);
			FlightText_DrawString(": ");
			if (conditionTextOverride != NULL) {
				consumedHeight = (int16_t)(lineHeight + goals_ExtraLineIfWouldWrap(conditionTextOverride));
				FlightText_DrawString(conditionTextOverride);
			} else {
				consumedHeight =
					(int16_t)(lineHeight +
							  goals_DrawConditionText(g_missionFlightGroups[targetId].fg.craftType, condition,
													  amountTextVariant, (int16_t)goalStatus));
			}
		} else {
			int16_t extraHeight;
			uint16_t objectType;

			objectType =
				g_objectTypeTables.craftTypeToObjectType[g_missionFlightGroups[targetId].fg.craftType];
			text = goals_GetObjectTypeDisplayName(objectType, 0);
			if (text != NULL && (int)g_flightCursorX + (int)FlightText_MeasureStringWidth(text) >
									(int)g_flightClipRight - (uint16_t)g_hudMfdTextInsetX) {
				extraHeight = (int16_t)((uint16_t)g_flightFontLineHeight + 1u);
			} else {
				extraHeight = 0;
			}
			FlightText_DrawString(text);
			consumedHeight = (int16_t)(consumedHeight + extraHeight);
			g_flightDrawCharFn(' ');
			FlightText_DrawString(g_missionFlightGroups[targetId].fg.name);
			FlightText_DrawString(": ");
			if (conditionTextOverride != NULL) {
				if ((int)g_flightCursorX + (int)FlightText_MeasureStringWidth(conditionTextOverride) >
					(int)g_flightClipRight - (uint16_t)g_hudMfdTextInsetX) {
					extraHeight = (int16_t)((uint16_t)g_flightFontLineHeight + 1u);
				} else {
					extraHeight = 0;
				}
				consumedHeight = (int16_t)(consumedHeight + extraHeight);
				FlightText_DrawString(conditionTextOverride);
			} else {
				consumedHeight = (int16_t)(consumedHeight + goals_DrawConditionText(
																g_missionFlightGroups[targetId].fg.craftType,
																condition, 9u, (int16_t)goalStatus));
			}
		}

		if (timeLimit5SecUnits != 0) {
			uint16_t timeSeconds;
			int minutes;
			int seconds;

			timeSeconds = (uint16_t)(5u * timeLimit5SecUnits);
			minutes = timeSeconds / 60;
			seconds = timeSeconds - minutes * 60;
			if ((uint16_t)seconds < 10u) {
				sprintf(timeText, " (%s %ld:0%ld)", g_strConjunctions[7], (long)(uint16_t)minutes,
						(long)(uint16_t)seconds);
			} else {
				sprintf(timeText, " (%s %ld:%ld)", g_strConjunctions[7], (long)(uint16_t)minutes,
						(long)(uint16_t)seconds);
			}
			consumedHeight = (int16_t)(consumedHeight + goals_ExtraLineIfWouldWrap(timeText));
			FlightText_DrawString(timeText);
		}
	} else if (targetType == GOALS_TARGET_GLOBAL_GROUP) {
		if (conditionTextOverride != NULL) {
			int16_t extraHeight;
			uint16_t objectType;

			objectType =
				g_objectTypeTables.craftTypeToObjectType[g_missionFlightGroups[targetId].fg.craftType];
			text = goals_GetObjectTypeDisplayName(objectType, 0);
			extraHeight = goals_ExtraLineIfWouldWrap(text);
			FlightText_DrawString(text);
			consumedHeight = (int16_t)(lineHeight + extraHeight);
			extraHeight = goals_ExtraLineIfWouldWrap(g_missionFlightGroups[targetId].fg.name);
			consumedHeight = (int16_t)(consumedHeight + extraHeight);
			FlightText_DrawString(g_missionFlightGroups[targetId].fg.name);
			FlightText_DrawString(": ");
			if ((int)g_flightCursorX + (int)FlightText_MeasureStringWidth(conditionTextOverride) >
				(int)g_flightClipRight - (uint16_t)g_hudMfdTextInsetX) {
				extraHeight = (int16_t)((uint16_t)g_flightFontLineHeight + 1u);
			} else {
				extraHeight = 0;
			}
			consumedHeight = (int16_t)(consumedHeight + extraHeight);
			FlightText_DrawString(conditionTextOverride);
		} else {
			FlightText_DrawString(g_missionHeader.body.globalGroups[targetId].name);
			FlightText_DrawString(": ");
			consumedHeight = (int16_t)(lineHeight + goals_DrawConditionText(
														g_missionFlightGroups[targetId].fg.craftType,
														condition, amountTextVariant, (int16_t)goalStatus));
		}
	} else {
		switch (targetType) {
			case GOALS_TARGET_SPECIES: {
				int16_t extraHeight;
				uint16_t objectType;

				objectType = g_objectTypeTables.craftTypeToObjectType[(uint16_t)(targetId + 1u)];
				text = goals_GetObjectTypeDisplayName(objectType, 1);
				if (text != NULL && (int)g_flightCursorX + (int)FlightText_MeasureStringWidth(text) >
										(int)g_flightClipRight - (uint16_t)g_hudMfdTextInsetX) {
					extraHeight = (int16_t)((uint16_t)g_flightFontLineHeight + 1u);
				} else {
					extraHeight = 0;
				}
				FlightText_DrawString(text);
				consumedHeight = (int16_t)(lineHeight + extraHeight);
				FlightText_DrawString(": ");
				break;
			}

			case GOALS_TARGET_GENUS:
				FlightText_DrawString(g_strShipGenus[g_genusConvert[targetId]]);
				FlightText_DrawString(": ");
				break;

			case GOALS_TARGET_FAMILY:
				FlightText_DrawString(g_strShipFamily[(uint8_t)g_familyConvert[targetId]]);
				FlightText_DrawString(": ");
				break;

			case GOALS_TARGET_IFF:
				if (targetId >= 2u) {
					uint16_t nameOffset;

					nameOffset = (uint16_t)(g_missionHeader.body.iffNames[targetId - 2u][0] == '1');
					FlightText_DrawString(&g_missionHeader.body.iffNames[targetId - 2u][nameOffset]);
					FlightText_DrawString(g_strSides[2]);
				} else {
					FlightText_DrawString(g_strSides[targetId]);
				}
				FlightText_DrawString(": ");
				break;

			case GOALS_TARGET_CRAFT_WHEN:
			case GOALS_TARGET_STATUS:
				consumedHeight =
					(int16_t)(lineHeight + goals_DrawConditionText((uint16_t)(targetId + 1u), 0,
																   amountTextVariant, (int16_t)goalStatus));
				break;

			case GOALS_TARGET_AI_LEVEL:
#ifdef XWA_MODERN
				text = targetId == 1u ? g_strPressSpaceBar : NULL;
				consumedHeight = (int16_t)(consumedHeight + goals_ExtraLineIfWouldWrap(text));
				FlightText_DrawString(text);
#else
				if (g_goalAiLevelString_BiasedBase[targetId] != NULL &&
					(int)g_flightCursorX +
							(int)FlightText_MeasureStringWidth(g_goalAiLevelString_BiasedBase[targetId]) >
						(int)g_flightClipRight - (uint16_t)g_hudMfdTextInsetX) {
					consumedHeight =
						(int16_t)(consumedHeight + (int16_t)((uint16_t)g_flightFontLineHeight + 1u));
				}
				FlightText_DrawString(g_goalAiLevelString_BiasedBase[targetId]);
#endif
				break;

			default:
				break;
		}

		consumedHeight =
			(int16_t)(consumedHeight + goals_DrawConditionText((uint16_t)(targetId + 1u), condition,
															   amountTextVariant, (int16_t)goalStatus));
	}

	if (percentComplete >= 0) {
		sprintf(percentText, " (%ld%%)", (long)percentComplete);
		if (goalTitleIndex == 0 || goalTitleIndex == 3) {
			FlightText_SetColor(0x4au);
		} else if (goalTitleIndex == 2 || goalTitleIndex == 1) {
			FlightText_SetColor(0x52u);
		} else {
			FlightText_SetColor(0x43u);
		}
		consumedHeight += goals_ExtraLineIfWouldWrap(percentText);
		FlightText_DrawString(percentText);
		FlightText_SetColor(g_goalTitleColorByIndex[goalTitleIndex]);
	}

	g_flightDrawCharFn('\n');
	return consumedHeight;
}

static __inline int16_t goals_ExtraLineIfWouldWrap(const char* text) {
	uint16_t width;
	uint16_t inset;
	int textEnd;
	int clipRight;

	if (text == NULL) {
		return 0;
	}

	width = FlightText_MeasureStringWidth(text);
	textEnd = g_flightCursorX + width;
	inset = g_hudMfdTextInsetX;
	clipRight = g_flightClipRight - inset;
	if (textEnd > clipRight) {
		return (int16_t)((uint16_t)g_flightFontLineHeight + 1u);
	}
	return 0;
}

static __inline const char* goals_GetObjectTypeDisplayName(uint16_t objectType, int plural) {
	ModelIndex modelIdx;

	modelIdx = GetModelIndexFromType(objectType);
	if (modelIdx != (ModelIndex)0xffff) {
		return plural ? g_strSpeciesNamesPlural[modelIdx] : g_modelDefs[modelIdx].nameLong;
	}
	if ((uint16_t)objectType >= (uint16_t)OBJ_CommSat1 && (uint16_t)objectType <= (uint16_t)OBJ_NavBuoy2) {
		return g_strBuoyNames[(uint16_t)objectType - (uint16_t)OBJ_CommSat1];
	}
	return NULL;
}

// FUNCTION: XWA 0x455330
int16_t goals_DrawConditionText(uint16_t craftType, uint16_t condition, uint16_t amountTextVariant,
								int16_t conditionRowBase) {
	int16_t result;
	const char* text = NULL;
	int16_t modelIndex;

	if (g_goalConditionTextVariantCount[condition] == 1) {
		amountTextVariant = 0;
	}

	condition = (uint16_t)(condition + conditionRowBase);
	craftType = g_objectTypeTables.craftTypeToObjectType[craftType];
	modelIndex = GetModelIndexFromType((ObjectTypeId)craftType);
	if (modelIndex != -1) {
		switch (g_craftGender[(uint16_t)modelIndex]) {
			case 0:
				text = g_strGoalCondMasculine[(size_t)condition * XWA_GOAL_CONDITION_TEXT_SLOTS +
											  amountTextVariant];
				break;
			case 1:
				text = g_strGoalCondFeminine[(size_t)condition * XWA_GOAL_CONDITION_TEXT_SLOTS +
											 amountTextVariant];
				break;
			case 2:
				text = g_strGoalCondNeutered[(size_t)condition * XWA_GOAL_CONDITION_TEXT_SLOTS +
											 amountTextVariant];
				break;
		}

		if (text != NULL) {
			result = goals_ExtraLineIfWouldWrap(text);
			FlightText_DrawString(text);
			return result;
		}

		return 0;
	}

	if (craftType >= OBJ_CommSat1 && craftType <= OBJ_NavBuoy2) {
		text = g_strGoalCondMasculine[(size_t)condition * XWA_GOAL_CONDITION_TEXT_SLOTS + amountTextVariant];
		if (text != NULL) {
			result = goals_ExtraLineIfWouldWrap(text);
			FlightText_DrawString(text);
			return result;
		}
	}

	return 0;
}

static void Hud_FreeTaggedResource(const char* tag, void** resource) {
	if (*resource != NULL) {
		Memory_FreeTagged(tag, *resource);
		*resource = NULL;
	}
}

static __inline int Hud_Scaled(double value) { return (int)value; }

static __inline size_t Hud_SurfaceBytes(uint32_t width, uint32_t height) {
	return (uint16_t)width * (uint16_t)height * g_flight16bppBytesPerPixel;
}

static __inline void* Hud_AllocSurface(const char* tag, uint32_t width, uint32_t height) {
	return Memory_AllocTagged(tag, Hud_SurfaceBytes(width, height));
}

static __inline void Hud_InitPlayerDisplayState(void) {
	uint8_t goalsUnimportant;
	uint8_t hangar;
	PlayerData* player;
	uint8_t* mfdEnabled;

	goalsUnimportant = g_missionHeader.body.goalsUnimportant;
	hangar = g_missionHeader.body.hangar;

	for (mfdEnabled = &g_players[0].mfd.enabled[1];
		 mfdEnabled < &g_players[XWA_PLAYER_COUNT - 1].mfd.enabled[1] + sizeof(PlayerData);
		 mfdEnabled += sizeof(PlayerData)) {
		player = (PlayerData*)(mfdEnabled - offsetof(PlayerData, mfd.enabled[1]));
		player->hudEnabled = 1;
		player->savedHudEnabled = 1;
		player->mfd.enabled[1] = 1;
		player->mfd.enabled[2] = 1;
		player->mfd.savedSideEnabled[0] = 1;
		player->mfd.savedSideEnabled[1] = 1;
		player->mfd.activeIndex = 1;
		player->mfd.savedActiveIndex = 1;
		player->mfdCommandMenuItemCount[0] = 9;
		player->mfdCommandMenuItemCount[1] = 4;
		player->mfdCommandMenuItemCount[2] = 4;
		player->mfdCommandMenuItemCount[3] = 4;
		player->mfdCommandMenuItemCount[4] = 4;
		player->mfdCommandMenuItemCount[5] = 4;
		player->mfdCommandMenuItemCount[6] = 4;
		player->mfdCommandMenuItemCount[7] = 4;
		player->mfdCommandMenuItemCount[8] = 2;
		player->cockpitVisible = 1;

		if (hangar == XWA_HANGAR_SKIRMISH) {
			player->mfd.page[1] = 4;
			if (goalsUnimportant) {
				player->mfd.page[2] = 0;
				player->mfd.savedPage[1] = 4;
				player->mfd.savedPage[2] = 0;
			} else {
				player->mfd.page[2] = 1;
				player->mfd.savedPage[1] = 4;
				player->mfd.savedPage[2] = 1;
			}
		} else if (goalsUnimportant) {
			player->mfd.page[1] = 4;
			player->mfd.page[2] = 5;
			player->mfd.savedPage[1] = 4;
			player->mfd.savedPage[2] = 5;
		} else {
			player->mfd.page[1] = 2;
			player->mfd.page[2] = 1;
			player->mfd.savedPage[1] = 2;
			player->mfd.savedPage[2] = 1;
			player->savedHudEnabled = 1;
		}
	}
}

static __inline void Hud_InitBlendLut(void) {
	uint16_t* blendLut;
	int color;
	int alpha;

	if (!g_hudUseAlphaSpriteAtlas10100) {
		return;
	}

	g_curImageBlendLut = (uint16_t*)Memory_CallocTagged("HUDOPTABLE", 1u, 0x100000u);
	blendLut = g_curImageBlendLut;
	if (blendLut == NULL) {
		return;
	}

	if (Display_IsPixelFormat555()) {
		for (color = 0; color < 0x10000; ++color) {
			for (alpha = 0; alpha <= 224; alpha += 32) {
				*blendLut++ = (uint16_t)((((alpha * (color & 0x001f)) & 0x001f00) |
										  ((alpha * (color & 0x03e0)) & 0x03e000) |
										  ((alpha * (color & 0x7c00)) & 0x7c0000)) >>
										 8);
			}
		}
	} else {
		for (color = 0; color < 0x10000; ++color) {
			for (alpha = 0; alpha <= 224; alpha += 32) {
				*blendLut++ = (uint16_t)((((alpha * (color & 0x001f)) & 0x001f00) |
										  ((alpha * (color & 0x07e0)) & 0x07e000) |
										  ((alpha * (color & 0xf800)) & 0xf80000)) >>
										 8);
			}
		}
	}
}

static __inline void Hud_InitSoftwareDrawTarget(void) {
	g_drawTarget->flags = 0;
	g_drawTarget->pixels = g_surfacePixels;
	g_drawTarget->width = g_screenWidth;
	g_drawTarget->width2 = g_screenWidth;
	g_drawTarget->maxX = g_screenWidth - 1;
	g_drawTarget->height = g_screenHeight;
	g_drawTarget->height2 = g_screenHeight;
	g_drawTarget->maxY = g_screenHeight - 1;
	g_drawTarget->bpp = 16;
	g_drawTarget->bytesPerPixel = 2;
	g_drawTarget->pitch = g_surfacePitch;
	g_drawTarget->flipY = 1;
	g_drawTarget->field78 = 0;
	g_drawTarget->field82 = 1;
	g_drawTarget->clipX0 = 0;
	g_drawTarget->clipY0 = 0;
	g_drawTarget->clipX1 = g_screenWidth;
	g_drawTarget->clipY1 = g_screenHeight;
	g_drawTarget->viewX0 = 0;
	g_drawTarget->viewY0 = 0;
	g_drawTarget->viewX1 = g_screenWidth - 1;
	g_drawTarget->viewY1 = g_screenHeight - 1;
}

static __inline void Hud_InitCockpitMaskSprite(int maskIndex) {
	HudCockpitMaskSprite* maskSprite;
	uint8_t* maskCursor;
	uint8_t* rowData;
	int row;

	maskSprite = &g_hudCockpitMaskSprites[maskIndex];
	Hud_SetupResourceData(10000, maskSprite->spriteId);
	if (g_curImage == NULL) {
		return;
	}

	rowData = SpriteResource_GetRowData(g_curImage);
	maskCursor = &g_cockpitMaskBitmap[g_screenWidth * maskSprite->y];
	for (row = 0; row < SpriteResource_GetSpriteHeight(g_curImage); ++row) {
		int x;
		uint8_t runCount;

		x = maskSprite->x;
		runCount = *rowData++;
		maskCursor += x;
		while (runCount != 0) {
			uint8_t control;
			uint8_t runLength;

			control = *rowData++;
			runLength = (uint8_t)(control & 0x7fu);
			if ((int)runLength + x > (int)g_screenWidth) {
				runLength = (uint8_t)(g_screenWidth - x);
			}

			if ((control & 0x80u) != 0) {
				maskCursor += runLength;
				x += runLength;
			} else {
				memset(maskCursor, 1, runLength);
				maskCursor += runLength;
				x += runLength;
				rowData += runLength;
			}
			--runCount;
		}

		if (x < (int)g_screenWidth) {
			maskCursor += g_screenWidth - x;
		}
	}
}

static __inline void Hud_InitCockpitMask(void) {
	const CraftData* craft;
	int i;

	craft = Hud_GetCraftPointer();
	if (craft == NULL) {
		OutputDebugStringA("NULL craft pointer in InitHUDMask()\n");
		return;
	}

	g_cockpitMaskBitmap =
		(uint8_t*)Memory_CallocTagged("HUDPMASKBUF", 1u, (size_t)g_screenWidth * (size_t)g_screenHeight);
	g_cockpitMaskRle = (uint8_t*)Memory_CallocTagged("HUDPMASKDATA", 1u, 0x1f40u);
	if (g_cockpitMaskBitmap == NULL || g_cockpitMaskRle == NULL) {
		return;
	}

	g_hudCockpitMaskSprites[0].spriteId = 20200;
	g_hudCockpitMaskSprites[0].x = 0;
	g_hudCockpitMaskSprites[0].y = 0;
	g_hudCockpitMaskSprites[1].spriteId = (craft->systemFlags & 0x100u) != 0 ? 20300 : 20100;
	g_hudCockpitMaskSprites[1].x = 531;
	g_hudCockpitMaskSprites[1].y = 0;
	g_hudCockpitMaskSprites[2].spriteId = 20400;
	g_hudCockpitMaskSprites[2].x = 94;
	g_hudCockpitMaskSprites[2].y = 0;
	g_hudCockpitMaskSprites[3].spriteId = 20700;
	g_hudCockpitMaskSprites[3].x = 194;
	g_hudCockpitMaskSprites[3].y = 359;
	g_hudCockpitMaskSprites[4].spriteId = 20500;
	g_hudCockpitMaskSprites[4].x = 0;
	g_hudCockpitMaskSprites[4].y = 335;
	g_hudCockpitMaskSprites[5].spriteId = 20600;
	g_hudCockpitMaskSprites[5].x = 441;
	g_hudCockpitMaskSprites[5].y = 335;

	for (i = 0; i < 6; ++i) {
		Hud_InitCockpitMaskSprite(i);
	}
	Hud_CompressMask();
}

// FUNCTION: XWA 0x462BE0
void Hud_InitHUD(void) {
	int statusColumnWidth;
	int mfdGap;

	Hud_InitPlayerDisplayState();
	g_hudCenterX = g_screenWidth >> 1;
	g_savedHudCmdPanelEnabled = 1;
	g_hudCenterY = g_screenHeight >> 1;

	{
		volatile float centerXf;

		g_hudColors[0] = (uint32_t)FrontendColor_GetIndexed(g_gameConfig.hudColor[g_flightPlayerCount > 1]);
		g_hudColors[1] = g_hudColors[g_gameConfig.hudColor[g_flightPlayerCount > 1] + 2];

		g_radarEllipseClampRadius = Hud_Scaled(g_flightHudScaleFactor * 42.0f);
		g_hudRadarCenterOffsetX = Hud_Scaled(g_flightHudScaleFactor * 61.0f);
		g_hudRadarCenterY = Hud_Scaled(g_flightHudScaleFactor * 45.0f);
		g_hudMfdPaneWidth = (uint16_t)Hud_Scaled(g_flightHudScaleFactor * 177.0f);
		g_hudMfdPaneHeight = (uint16_t)Hud_Scaled(g_flightHudScaleFactor * 120.0f);
		g_hudCmdPanelWidth = Hud_Scaled(g_flightHudScaleFactor * 240.0f);
		g_hudCmdPanelHeight = Hud_Scaled(g_flightHudScaleFactor * 125.0f);
		g_hudTargetInsetWidth = Hud_Scaled(g_flightHudScaleFactor * 140.0f);
		g_hudTargetInsetHeight = Hud_Scaled(g_flightHudScaleFactor * 114.0f);
		g_hudSystemMessagePaneY = (int16_t)Hud_Scaled((double)g_screenHeight * 0.1333333333333333);
		g_hudSystemMessagePaneX =
			(int16_t)((g_screenWidth - (uint16_t)g_hudSystemMessagePaneSurfaceWidth) >> 1);
		g_hudFlightGroupMessagePaneY = (int16_t)Hud_Scaled((double)g_screenHeight * 0.6666666666666666);
		g_hudFlightGroupMessagePaneX =
			(int16_t)((g_screenWidth - (uint16_t)g_hudFlightGroupMessagePaneSurfaceWidth) >> 1);
		g_hudReadyMessagePaneY = (int16_t)Hud_Scaled((double)g_screenHeight * 0.07299270072992702);
		g_hudReadyMessagePaneX =
			(int16_t)((g_screenWidth - (uint16_t)g_hudReadyMessagePaneSurfaceWidth) >> 1);
		g_hudCmdPanelOriginX = (uint16_t)((g_screenWidth - g_hudCmdPanelWidth) >> 1);
		g_hudCmdPanelOriginY = (uint16_t)(g_screenHeight - g_hudCmdPanelHeight);
		g_hudSpeedTextY = Hud_Scaled(g_flightHudScaleFactor + g_flightHudScaleFactor);
		centerXf = (float)g_hudCenterX;
		g_hudSpeedTextX = Hud_Scaled(centerXf - g_flightHudScaleFactor * 160.0f);
		g_hudCraftNameTextY = g_hudSpeedTextY;
		g_hudCraftNameTextX = Hud_Scaled(g_flightHudScaleFactor * 100.0f + centerXf);
		g_hudThrottleTextY = Hud_Scaled(g_flightHudScaleFactor * 11.0f);
		g_hudThrottleTextX = g_hudSpeedTextX;
		g_hudMissionClockTextY = g_hudThrottleTextY;
		g_hudMissionClockTextX = g_hudCraftNameTextX;
		g_hudWarheadCountTextY = g_hudThrottleTextY;
		g_hudWarheadCountTextX = Hud_Scaled(centerXf - g_flightHudScaleFactor * 25.0f);
		g_hudDualWarheadCountTextX = Hud_Scaled(centerXf - g_flightHudScaleFactor * 35.0f);
		g_hudCountermeasureCountTextY = g_hudThrottleTextY;
		g_hudCountermeasureCountTextX = Hud_Scaled(centerXf + g_flightHudScaleFactor * 45.0f);
		g_hudProvingGroundStatusTextX = g_hudDualWarheadCountTextX;
		g_hudProvingGroundStatusTextY = g_hudThrottleTextY;
		g_hudFilmRecTextX = Hud_Scaled(centerXf - g_flightHudScaleFactor * 200.0f);
		g_hudFilmRecTextY = g_hudThrottleTextY;
		g_hudCmdTargetNameTextY = Hud_Scaled((double)g_hudCmdPanelHeight * 0.115473441108545);
		g_hudCmdOrderLineY = (int)g_flightFontLineHeight + g_hudCmdTargetNameTextY -
							 Hud_Scaled(g_flightHudScaleFactor * -2.0f);
		g_hudCmdShieldLabelX = (int16_t)g_hudCmdPanelWidth / 20;
		g_hudCmdShieldLabelY = Hud_Scaled((double)g_hudCmdPanelHeight * 0.4694835680751174);
		g_hudCmdShieldPercentX = (int16_t)g_hudCmdPanelWidth / 15;
		g_hudCmdShieldPercentY = (int)g_flightFontLineHeight + g_hudCmdShieldLabelY;
		g_hudCmdHullLabelX = g_hudCmdPanelWidth >> 4;
		g_hudCmdHullLabelY = Hud_Scaled((double)g_hudCmdPanelHeight * 0.6944444444444444);
		g_hudCmdHullPercentX = Hud_Scaled((double)g_hudCmdPanelWidth * 0.07501875468867217);
		g_hudCmdHullPercentY = (int)g_flightFontLineHeight + g_hudCmdHullLabelY;
		g_hudCmdSystemLabelX = g_hudCmdPanelWidth - Hud_Scaled(g_flightHudScaleFactor * 36.0f);
		g_hudCmdSystemLabelY = g_hudCmdShieldLabelY;
		g_hudCmdSystemPercentX = g_hudCmdSystemLabelX + FlightText_MeasureStringWidth(" ");
		g_hudCmdSystemPercentY = (int)g_flightFontLineHeight + g_hudCmdSystemLabelY;
		g_hudCmdDistanceLabelX = g_hudCmdPanelWidth - Hud_Scaled(g_flightHudScaleFactor * 10.0f) -
								 FlightText_MeasureStringWidth(g_strPanelStrings[PANEL_STRING_DIST]);
		g_hudCmdDistanceLabelY = Hud_Scaled((double)g_hudCmdPanelHeight * 0.6944444444444444);
		g_hudCmdDistanceValueX = g_hudCmdPanelWidth - Hud_Scaled(g_flightHudScaleFactor * 32.0f);
		g_hudCmdDistanceValueY = (int)g_flightFontLineHeight + g_hudCmdDistanceLabelY;
		g_hudCmdTargetStatusX = g_hudCmdPanelWidth >> 4;
		g_hudCmdTargetStatusY = g_hudCmdPanelHeight - (g_flightFontLineHeight >> 1) - g_flightFontLineHeight;
		g_hudCmdComponentLineY = g_hudCmdTargetStatusY;
		g_hudMfdSurfaceY = (uint16_t)(g_screenHeight - g_hudMfdPaneHeight - g_flightFontLineHeight -
									  Hud_Scaled(g_flightHudScaleFactor * 5.0f));
		g_hudMfdTextInsetX = Hud_Scaled(g_flightHudScaleFactor * 3.0f);
		g_hudFrontShieldPercentTextX = Hud_Scaled(g_flightHudScaleFactor * 7.0f);
		g_hudFrontShieldPercentTextY = Hud_Scaled(g_flightHudScaleFactor * 91.0f);
		g_hudRearShieldPercentTextX = g_hudFrontShieldPercentTextX;
		g_hudRearShieldPercentTextY = Hud_Scaled(g_flightHudScaleFactor * 179.0f);
		g_hudSubsystemLabelLaserX = Hud_Scaled(g_flightHudScaleFactor * 9.0f);
		g_hudSubsystemLabelLaserY = Hud_Scaled(g_flightHudScaleFactor * 6.0f);
		g_hudSubsystemLabelShieldX = g_hudSubsystemLabelLaserX;
		g_hudSubsystemLabelShieldY = Hud_Scaled(g_flightHudScaleFactor * 80.0f);
		g_hudSubsystemLabelEngineY = g_hudSubsystemLabelLaserY;
		g_hudSubsystemLabelEngineX = g_screenWidth - Hud_Scaled(g_flightHudScaleFactor * 14.0f);
		g_hudSubsystemLabelBeamX = g_hudSubsystemLabelEngineX;
		g_hudSubsystemLabelBeamY = g_hudSubsystemLabelShieldY;
		g_hudWarheadCountLeftReticleOffsetX = Hud_Scaled(g_flightHudScaleFactor * 20.0f);
		g_hudWarheadCountRightReticleOffsetX = Hud_Scaled(g_flightHudScaleFactor * 15.0f);
		g_hudWarheadCountReticleOffsetY = Hud_Scaled(g_flightHudScaleFactor * 30.0f);
		g_hudRadarScopeOffsetX = Hud_Scaled(g_flightHudScaleFactor * 55.0f);
		g_hudRadarScopeOffsetY = Hud_Scaled(g_flightHudScaleFactor * 47.0f);
		g_hudLaserThreatSlot0OffsetX = Hud_Scaled(g_flightHudScaleFactor * 18.0f);
		g_hudLaserThreatSlot0OffsetY = Hud_Scaled(g_flightHudScaleFactor * 28.0f);
		g_hudWarheadThreatSlot0OffsetX = Hud_Scaled(g_flightHudScaleFactor * 19.0f);
		g_hudWarheadThreatSlot0OffsetY = Hud_Scaled(g_flightHudScaleFactor * 34.0f);
		g_hudLaserThreatSlot1OffsetX = Hud_Scaled(g_flightHudScaleFactor * 8.0f);
		g_hudLaserThreatSlot1OffsetY = Hud_Scaled(g_flightHudScaleFactor * 33.0f);
		g_hudLaserThreatSlot2OffsetY = g_hudLaserThreatSlot1OffsetY;
		g_hudWarheadThreatSlot1OffsetX = g_hudSubsystemLabelLaserY;
		g_hudLaserThreatSlot3OffsetX = g_hudWarheadThreatSlot0OffsetX;
		g_hudWarheadThreatSlot3OffsetX = g_hudWarheadThreatSlot0OffsetX;
		g_hudWarheadThreatSlot1OffsetY = Hud_Scaled(g_flightHudScaleFactor * 32.0f);
		g_hudLaserThreatSlot2OffsetX = g_hudSubsystemLabelLaserX;
		g_hudWarheadThreatSlot2OffsetX = g_hudFrontShieldPercentTextX;
		g_hudWarheadThreatSlot2OffsetY = g_hudWarheadThreatSlot1OffsetY;
		g_hudLaserThreatSlot3OffsetY = g_hudLaserThreatSlot0OffsetY;
		g_hudWarheadThreatSlot3OffsetY = g_hudWarheadThreatSlot0OffsetY;
		g_hudRadarFrameLeftOffsetX = Hud_Scaled(g_flightHudScaleFactor * 114.0f);
		g_hudRadarFrameRightOffsetX = g_hudRadarFrameLeftOffsetX;
		if (g_flightHudScaleFactor > 1.0) {
			g_hudRadarFrameRightOffsetX = Hud_Scaled(g_flightHudScaleFactor * 113.0f);
		}
		g_hudRadarFrameOffsetY = Hud_Scaled(g_flightHudScaleFactor * 16.0f);
		g_hudPowerLaserPipX = Hud_Scaled(0.5f - g_flightHudScaleFactor * -3.0f);
		g_hudPowerLaserPipTopY = Hud_Scaled(0.5f - g_flightHudScaleFactor * -38.0f);
		g_hudPowerShieldPipX = g_hudPowerLaserPipX;
		g_hudPowerShieldPipTopY = Hud_Scaled(0.5f - g_flightHudScaleFactor * -83.0f);
		g_hudPowerBeamPipRightOffsetX = g_hudPowerLaserPipX;
		g_hudPowerBeamPipTopY = g_hudPowerShieldPipTopY;
		g_hudPowerReservePipRightOffsetX = g_hudPowerLaserPipX;
		g_hudPowerBeamReserveTopY = Hud_Scaled(0.5f - g_flightHudScaleFactor * -42.0f);
		g_hudPowerUnallocatedPipTopY = Hud_Scaled(0.5f - g_flightHudScaleFactor * -41.0f);
		g_hudPowerPipSpacingY = Hud_Scaled(0.5f - g_flightHudScaleFactor * -10.0f);
		g_hudPowerReservePipSpacingY = Hud_Scaled(0.5f - g_flightHudScaleFactor * -5.0f);
		g_hudShieldGaugeSideOffsetX = Hud_Scaled(g_flightHudScaleFactor * 38.0f);
		g_hudShieldGaugeBottomOffsetY = Hud_Scaled(g_flightHudScaleFactor * 142.0f);
		g_hudShieldHullIconSideOffsetX = Hud_Scaled(g_flightHudScaleFactor * 43.0f);
		g_hudShieldBarSideOffsetX = Hud_Scaled(g_flightHudScaleFactor * 44.0f);
		g_hudShieldLayoutInitOnlyY = Hud_Scaled(g_flightHudScaleFactor * 132.0f);
		g_hudShieldHullIconBottomOffsetY = Hud_Scaled(g_flightHudScaleFactor * 140.0f);
		g_hudShieldFrontUpperBarY = Hud_Scaled(g_flightHudScaleFactor * 125.0f);
		g_hudShieldFrontLowerBarY = Hud_Scaled(g_flightHudScaleFactor * 127.0f);
		g_hudShieldRearUpperBarY = Hud_Scaled(g_flightHudScaleFactor * 157.0f);
		g_hudShieldRearLowerBarY = Hud_Scaled(g_flightHudScaleFactor * 155.0f);
		g_hudBeamGaugeRightOffsetX = g_hudShieldGaugeSideOffsetX;
		g_hudBeamGaugeBottomOffsetY = g_hudShieldGaugeBottomOffsetY;
		g_hudBeamChargeRightOffsetX = g_hudShieldHullIconSideOffsetX;
		g_hudBeamChargeBottomOffsetY = Hud_Scaled(g_flightHudScaleFactor * 163.0f);
		g_hudBeamIconRightOffsetX = g_hudShieldHullIconSideOffsetX;
		g_hudBeamIconBottomOffsetY = Hud_Scaled(g_flightHudScaleFactor * 141.0f);
		g_hudBeamChargeSegmentOffsetY0 = Hud_Scaled(g_flightHudScaleFactor * 3.0f);
		g_hudBeamChargeSegmentOffsetY1 = Hud_Scaled(g_flightHudScaleFactor * 4.0f);
		g_hudBeamChargeSegmentOffsetY2 = Hud_Scaled(g_flightHudScaleFactor + g_flightHudScaleFactor);
		g_hudBeamChargeSegmentOffsetY3 = g_hudBeamChargeSegmentOffsetY0;
		g_hudBeamChargeSegmentOffsetY4 = Hud_Scaled(g_flightHudScaleFactor * 5.0f);
		g_hudBeamChargeSegmentOffsetY5 = g_hudBeamChargeSegmentOffsetY4;
		g_hudBeamChargeSegmentOffsetY6 = Hud_Scaled(g_flightHudScaleFactor * 6.0f);
		g_hudBeamChargeSegmentOffsetY7 = Hud_Scaled(g_flightHudScaleFactor * 7.0f);
		g_hudMfdFrameSideOffsetX = Hud_Scaled(g_flightHudScaleFactor * 100.0f);
		g_hudMfdFrameY = Hud_Scaled(g_flightHudScaleFactor * 74.0f);
		g_hudCmdFrameY = Hud_Scaled(g_flightHudScaleFactor * 62.0f);
		g_hudLaserChargeSingleY = Hud_Scaled(g_flightHudScaleFactor * 126.0f);
		g_hudLaserChargePairLeftOffsetX = Hud_Scaled(g_flightHudScaleFactor * 36.0f);
		g_hudLaserChargePairRightOffsetX = g_hudShieldGaugeSideOffsetX;
		g_hudLaserChargePairY = g_hudLaserChargeSingleY;
		g_hudLaserChargeTripleLeftOffsetX = Hud_Scaled(g_flightHudScaleFactor * 49.0f);
		g_hudLaserChargeTripleRightOuterOffsetX = Hud_Scaled(g_flightHudScaleFactor * 50.0f);
		g_hudLaserChargeTripleRightInnerOffsetX = Hud_Scaled(g_flightHudScaleFactor);
		g_hudLaserChargeTripleY = g_hudLaserChargeSingleY;
		g_hudLaserChargeQuadLeftOffsetX = g_hudLaserChargePairLeftOffsetX;
		g_hudLaserChargeQuadRightOffsetX = g_hudShieldGaugeSideOffsetX;
		g_hudIonChargeTripleLeftOffsetX = g_hudLaserChargeTripleLeftOffsetX;
		g_hudIonChargePairLeftOffsetX = g_hudLaserChargePairLeftOffsetX;
		g_hudIonChargeTripleRightInnerOffsetX = Hud_Scaled(g_flightHudScaleFactor);
		g_hudIonChargeQuadLeftOffsetX = g_hudLaserChargePairLeftOffsetX;
		g_hudLaserChargeQuadUpperY = Hud_Scaled(g_flightHudScaleFactor * 133.0f);
		g_hudLaserChargeQuadLowerY = g_hudLaserChargeSingleY;
		g_hudIonChargeYOffsetFromLaser = g_hudBeamChargeSegmentOffsetY7;
		g_hudIonChargeSingleY = g_hudLaserChargeQuadUpperY;
		g_hudIonChargePairRightOffsetX = g_hudShieldGaugeSideOffsetX;
		g_hudIonChargePairY = g_hudLaserChargeQuadUpperY;
		g_hudIonChargeTripleRightOuterOffsetX = g_hudLaserChargeTripleRightOuterOffsetX;
		g_hudIonChargeTripleY = g_hudLaserChargeQuadUpperY;
		g_hudIonChargeQuadRightOffsetX = g_hudShieldGaugeSideOffsetX;
		g_hudIonChargeQuadUpperY = g_hudShieldHullIconBottomOffsetY;
		g_hudIonChargeQuadLowerY = g_hudLaserChargeQuadUpperY;
		g_hudEnergyChargeTripleSegmentStepX = g_hudBeamChargeSegmentOffsetY7;
		g_hudEnergyChargeTripleInitialBackstepX = Hud_Scaled(g_flightHudScaleFactor * 17.0f);
		g_hudEnergyChargeNonTripleSegmentStepX = Hud_Scaled(g_flightHudScaleFactor * 10.0f);
		g_hudEnergyChargeNonTripleInitialBackstepX = Hud_Scaled(g_flightHudScaleFactor * 24.0f);
		g_mfdCommandMenuNumberX = Hud_Scaled(g_flightHudScaleFactor * 30.0f);
		g_mfdCommandMenuTextX = Hud_Scaled(g_flightHudScaleFactor * 60.0f);
		g_mfdCommandMenuLineExtraSpacingY = g_hudBeamChargeSegmentOffsetY2;
	}

	if (g_useHardware3D) {
		g_hudUseAlphaSpriteAtlas10100 = 0;
	} else {
		if (g_hudUseAlphaSpriteAtlas10100) {
			SpriteResource_LoadGroup(10100);
		} else {
			SpriteResource_LoadGroup(10000);
		}

		g_flightLockBackBufferForHudDraw = 1;
		FlightSurface_Lock();
		FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		g_flightFillClipRectFn();
		Hud_InitBlendLut();
		Hud_InitSoftwareDrawTarget();
		FlightSurface_Unlock();
		g_flightLockBackBufferForHudDraw = 0;

		Hud_InitCockpitMask();

		g_hudSystemMessagePaneSurface = Hud_AllocSurface("HUDTEX1", g_hudSystemMessagePaneSurfaceWidth,
														 g_hudSystemMessagePaneSurfaceHeight);
		g_hudFlightGroupMessagePaneSurface = Hud_AllocSurface(
			"HUDTEX2", g_hudFlightGroupMessagePaneSurfaceWidth, g_hudFlightGroupMessagePaneSurfaceHeight);
		g_hudReadyMessagePaneSurface = Hud_AllocSurface("HUDTEX3", g_hudReadyMessagePaneSurfaceWidth,
														g_hudReadyMessagePaneSurfaceHeight);
		hudTex4 = Hud_AllocSurface("HUDTEX4", g_hudReticleWarheadCountSurfaceWidth,
								   g_hudReticleWarheadCountSurfaceHeight);
		hudTex5 = Hud_AllocSurface("HUDTEX5", g_hudReticleWarheadCountSurfaceWidth,
								   g_hudReticleWarheadCountSurfaceHeight);
		hudTex6 = Hud_AllocSurface("HUDTEX6", g_hudWarheadCountTextSurfaceWidth,
								   g_hudWarheadCountTextSurfaceHeight);
		hudTex7 = Hud_AllocSurface("HUDTEX7", g_hudShieldPercentTextSurfaceWidth,
								   g_hudShieldPercentTextSurfaceHeight);
		hudTex8 = Hud_AllocSurface("HUDTEX8", g_hudShieldPercentTextSurfaceWidth,
								   g_hudShieldPercentTextSurfaceHeight);
		hudTex9 = Hud_AllocSurface("HUDTEX9", g_hudSpeedTextSurfaceWidth, g_hudSpeedTextSurfaceHeight);
		hudTex10 =
			Hud_AllocSurface("HUDTEX10", g_hudThrottleTextSurfaceWidth, g_hudThrottleTextSurfaceHeight);
		hudTex11 =
			Hud_AllocSurface("HUDTEX11", g_hudCraftNameTextSurfaceWidth, g_hudCraftNameTextSurfaceHeight);
		hudTex12 = Hud_AllocSurface("HUDTEX12", g_hudMissionClockTextSurfaceWidth,
									g_hudMissionClockTextSurfaceHeight);
		hudTex13 = Hud_AllocSurface("HUDTEX13", g_hudCountermeasureCountTextSurfaceWidth,
									g_hudCountermeasureCountTextSurfaceHeight);
		g_hudMfdTitleTexPixels = Hud_AllocSurface("HUDTEX14", g_hudMfdSurfaceWidth, g_hudMfdSurfaceHeight);
		hudTex15 = Hud_AllocSurface("HUDTEX15", g_hudProvingGroundStatusTextSurfaceWidth,
									g_hudProvingGroundStatusTextSurfaceHeight);
		hudTex16 = Hud_AllocSurface("HUDTEX16", g_hudSubsystemLaserLabelSurfaceWidth,
									g_hudSubsystemLaserLabelSurfaceHeight);
		hudTex17 = Hud_AllocSurface("HUDTEX17", g_hudSubsystemShieldLabelSurfaceWidth,
									g_hudSubsystemShieldLabelSurfaceHeight);
		hudTex18 = Hud_AllocSurface("HUDTEX18", g_hudSubsystemEngineLabelSurfaceWidth,
									g_hudSubsystemEngineLabelSurfaceHeight);
		hudTex19 = Hud_AllocSurface("HUDTEX19", g_hudSubsystemBeamLabelSurfaceWidth,
									g_hudSubsystemBeamLabelSurfaceHeight);
		g_hudFilmRecordingIndicatorSurface = Hud_AllocSurface(
			"HUDTEXFILM", g_hudFilmRecordingIndicatorSurfaceWidth, g_hudFilmRecordingIndicatorSurfaceHeight);

		Hud_ClearSoftwareTextPane(g_hudMfdTitleTexPixels, g_hudMfdSurfaceWidth, g_hudMfdSurfaceHeight);
		Hud_ClearSoftwareTextPane(g_hudSystemMessagePaneSurface, g_hudSystemMessagePaneSurfaceWidth,
								  g_hudSystemMessagePaneSurfaceHeight);
		Hud_ClearSoftwareTextPane(g_hudFlightGroupMessagePaneSurface, g_hudFlightGroupMessagePaneSurfaceWidth,
								  g_hudFlightGroupMessagePaneSurfaceHeight);
		Hud_ClearSoftwareTextPane(g_hudReadyMessagePaneSurface, g_hudReadyMessagePaneSurfaceWidth,
								  g_hudReadyMessagePaneSurfaceHeight);

		g_hudCmdTexPixels =
			Memory_AllocTagged("HUDCMDTEX", Hud_SurfaceBytes(g_hudCmdPanelWidth, g_hudCmdPanelHeight));
		Hud_ClearSoftwareTextPane(g_hudCmdTexPixels, g_hudCmdPanelWidth, g_hudCmdPanelHeight);
		g_hudMfdLeftTexPixels =
			Memory_AllocTagged("HUDMFDTEX1", Hud_SurfaceBytes(g_hudMfdPaneWidth, g_hudMfdPaneHeight));
		g_hudMfdRightTexPixels =
			Memory_AllocTagged("HUDMFDTEX2", Hud_SurfaceBytes(g_hudMfdPaneWidth, g_hudMfdPaneHeight));
		Hud_ClearSoftwareTextPane(g_hudMfdLeftTexPixels, (uint16_t)g_hudMfdPaneWidth,
								  (uint16_t)g_hudMfdPaneHeight);
		Hud_ClearSoftwareTextPane(g_hudMfdRightTexPixels, (uint16_t)g_hudMfdPaneWidth,
								  (uint16_t)g_hudMfdPaneHeight);

		Hud_UpdateHUDMask(4, 1);
		Hud_UpdateHUDMask(5, 1);
		g_hudElementEnabled[4].enabled = 1;
		g_hudElementEnabled[5].enabled = 1;
		g_hudElementEnabled[6].enabled = 1;
		g_hudElementEnabled[7].enabled = 1;
		g_hudElementEnabled[8].enabled = 1;
		g_hudElementEnabled[9].enabled = 1;
		if (g_filmPlaybackMode && !g_useHardware3D) {
			g_hudElementEnabled[10].enabled = 1;
			g_hudElementEnabled[11].enabled = 1;
		}
		{
			int fpsCountBytes;

			fpsCountBytes = g_flight16bppBytesPerPixel * 5;
			fpsCountBytes *= 9;
			fpsCountBytes *= 5;
			fpsCountBytes *= 5;
			g_hudFpsCountPixels = Memory_AllocTagged("HUDFPSCOUNT", (size_t)(fpsCountBytes * 2));
		}
	}

	FlightText_SetFontTier(0);

	g_mfdFriendlyCraftShieldHullHeaderX = (uint16_t)g_hudMfdPaneWidth >> 1;
	g_mfdFriendlyCraftShieldHullHeaderX -=
		FlightText_MeasureStringWidth(g_strPanelStrings[PANEL_STRING_HUD_S]) >> 1;
	g_mfdFriendlyCraftShieldHullHeaderX -= FlightText_MeasureStringWidth("/") >> 1;
	g_mfdFriendlyCraftShieldHullHeaderX -=
		FlightText_MeasureStringWidth(g_strPanelStrings[PANEL_STRING_HUD_H]) >> 1;
	g_mfdFriendlyCraftShieldHullHeaderX += Hud_Scaled(g_flightHudScaleFactor * -8.0f);

	g_mfdFriendlyCraftNameColumnWidth = (uint16_t)g_hudMfdPaneWidth >> 1;
	g_mfdFriendlyCraftNameColumnWidth -= FlightText_MeasureStringWidth("200/100") >> 1;
	g_mfdFriendlyCraftNameColumnWidth += Hud_Scaled(g_flightHudScaleFactor * -8.0f);
	g_mfdFriendlyCraftLayoutInitOnlyX =
		g_mfdFriendlyCraftNameColumnWidth - Hud_Scaled(g_flightHudScaleFactor + g_flightHudScaleFactor);
	g_mfdFriendlyCraftTargetColumnWidth =
		g_hudMfdPaneWidth - g_mfdFriendlyCraftNameColumnWidth - FlightText_MeasureStringWidth("200/100");
	g_mfdFriendlyCraftTargetColumnWidth += Hud_Scaled(g_flightHudScaleFactor * -2.0f);

	g_mfdFlightGroupsNameColumnWidth = Hud_Scaled(g_flightHudScaleFactor * 46.0f);
	g_mfdFlightGroupsShieldHullHeaderX = Hud_Scaled(g_flightHudScaleFactor * 65.0f);
	g_mfdFlightGroupsShieldHullHeaderX -=
		FlightText_MeasureStringWidth(g_strPanelStrings[PANEL_STRING_HUD_S]) >> 1;
	g_mfdFlightGroupsShieldHullHeaderX -= FlightText_MeasureStringWidth("/") >> 1;
	g_mfdFlightGroupsShieldHullHeaderX -=
		FlightText_MeasureStringWidth(g_strPanelStrings[PANEL_STRING_HUD_H]) >> 1;
	g_mfdFlightGroupsShieldHullValueX = -(FlightText_MeasureStringWidth("200/100") >> 1);
	g_mfdFlightGroupsShieldHullValueX -= Hud_Scaled(g_flightHudScaleFactor * -65.0f);
	mfdGap = Hud_Scaled(g_flightHudScaleFactor * -2.0f);
	g_mfdFlightGroupsTargetColumnX =
		g_mfdFlightGroupsShieldHullValueX - mfdGap + FlightText_MeasureStringWidth("200/100");
	g_mfdFlightGroupsTargetColumnWidth = Hud_Scaled(g_flightHudScaleFactor * 51.0f);
	g_mfdFlightGroupsStatusColumnX =
		g_mfdFlightGroupsTargetColumnX + g_mfdFlightGroupsTargetColumnWidth - mfdGap;
	statusColumnWidth = Hud_Scaled(g_flightHudScaleFactor * 39.0f);
	g_mfdMessageLogLeftScrollOffset = 0;
	g_mfdMessageLogRightScrollOffset = 0;
	g_mfdFlightGroupsStatusColumnWidth = statusColumnWidth;
}

// FUNCTION: XWA 0x464A20
void Hud_FreeHUDResources(void) {
	Hud_FreeTaggedResource("HUDOPTABLE", (void**)&g_curImageBlendLut);
	Hud_FreeTaggedResource("HUDTEX1", &g_hudSystemMessagePaneSurface);
	Hud_FreeTaggedResource("HUDTEX2", &g_hudFlightGroupMessagePaneSurface);
	Hud_FreeTaggedResource("HUDTEX3", &g_hudReadyMessagePaneSurface);
	Hud_FreeTaggedResource("HUDTEX4", &hudTex4);
	Hud_FreeTaggedResource("HUDTEX5", &hudTex5);
	Hud_FreeTaggedResource("HUDTEX6", &hudTex6);
	Hud_FreeTaggedResource("HUDTEX7", &hudTex7);
	Hud_FreeTaggedResource("HUDTEX8", &hudTex8);
	Hud_FreeTaggedResource("HUDTEX9", &hudTex9);
	Hud_FreeTaggedResource("HUDTEX10", &hudTex10);
	Hud_FreeTaggedResource("HUDTEX11", &hudTex11);
	Hud_FreeTaggedResource("HUDTEX12", &hudTex12);
	Hud_FreeTaggedResource("HUDTEX13", &hudTex13);
	Hud_FreeTaggedResource("HUDTEX14", &g_hudMfdTitleTexPixels);
	Hud_FreeTaggedResource("HUDTEX15", &hudTex15);
	Hud_FreeTaggedResource("HUDTEX16", &hudTex16);
	Hud_FreeTaggedResource("HUDTEX17", &hudTex17);
	Hud_FreeTaggedResource("HUDTEX18", &hudTex18);
	Hud_FreeTaggedResource("HUDTEX19", &hudTex19);
	Hud_FreeTaggedResource("HUDTEXFILM", &g_hudFilmRecordingIndicatorSurface);
	Hud_FreeTaggedResource("HUDCMDTEX", &g_hudCmdTexPixels);
	Hud_FreeTaggedResource("HUDMFDTEX1", &g_hudMfdLeftTexPixels);
	Hud_FreeTaggedResource("HUDMFDTEX2", &g_hudMfdRightTexPixels);
	Hud_FreeTaggedResource("HUDFPSCOUNT", &g_hudFpsCountPixels);
	Hud_FreeTaggedResource("HUDPMASKBUF", (void**)&g_cockpitMaskBitmap);
	Hud_FreeTaggedResource("HUDPMASKDATA", (void**)&g_cockpitMaskRle);
}

// FUNCTION: XWA 0x464D40
void Hud_SetDrawTargetSurface(void) {
	g_drawTarget->pixels = g_surfacePixels;
	g_drawTarget->pitch = g_surfacePitch;
}

// FUNCTION: XWA 0x464D60
void Hud_ClearFlightSurface(void) {
	uint8_t savedLockBackBufferForHudDraw;

	savedLockBackBufferForHudDraw = (uint8_t)g_flightLockBackBufferForHudDraw;
	g_flightLockBackBufferForHudDraw = 1;
	FlightSurface_Lock();
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
	g_flightFillClipRectFn();
	FlightSurface_Unlock();
	g_flightLockBackBufferForHudDraw = savedLockBackBufferForHudDraw;
}

// FUNCTION: XWA 0x464DC0
void Hud_UpdateHUDMask(int hudMaskIndex, int enabled) {
	uint8_t* maskCursor;
	uint8_t* rowData;
	uint16_t spriteId;
	unsigned int row;

	maskCursor = g_cockpitMaskBitmap;
	if (!g_useHardware3D) {
		if (maskCursor == NULL) {
			OutputDebugStringA("Null pMaskBuffer in UpdateHUDMask()!\n");
			return;
		}

		spriteId = g_hudCockpitMaskSprites[hudMaskIndex].spriteId;
		if (g_hudUseAlphaSpriteAtlas10100) {
			g_curImage = SpriteResource_ResolveSprite(10000 + g_hudAlphaSpriteGroupOffset, spriteId);
		} else {
			g_curImage = SpriteResource_ResolveSprite(10000, spriteId);
		}

		if (g_curImage == NULL) {
			OutputDebugStringA("Null pointer to image in SetupResourceData()\n");
		} else {
			uint8_t* payload;

			g_curImageWidth = g_curImage->width;
			g_curImageHeight = g_curImage->height;
			g_curImageRLE = SpriteResource_GetRowData(g_curImage);
#ifdef XWA_MODERN
			payload = SpriteResource_GetMutableSpritePayload(g_curImage);
#else
			payload = g_curImage->pixels;
#endif
			g_curImagePalette = (uint16_t*)payload;
			g_curImagePalette = (uint16_t*)(payload + ((SpritePayload*)payload)->palette16Offset);
		}

		if (g_curImage != NULL) {
			rowData = SpriteResource_GetRowData(g_curImage);
			{
				int screenWidth;
				int rowOffset;

				screenWidth = (int)g_screenWidth;
				rowOffset = g_hudCockpitMaskSprites[hudMaskIndex].y;
				rowOffset *= screenWidth;
				maskCursor += rowOffset;
			}

			for (row = 0; row < g_curImage->height; ++row) {
				int x;
				uint8_t runCount;

				x = g_hudCockpitMaskSprites[hudMaskIndex].x;
				runCount = *rowData++;
				maskCursor += x;
				while (runCount-- != 0) {
					uint8_t control;
					uint16_t runLength;

					control = *rowData;
					runLength = (uint16_t)(control & 0x7fu);
					++rowData;
					if ((control & 0x80u) != 0) {
						if ((unsigned int)((int)runLength + x) > g_screenWidth) {
							runLength = (uint8_t)(g_screenWidth - x);
						}
						maskCursor += (uint8_t)runLength;
						x += runLength;
					} else {
						if ((unsigned int)((int)runLength + x) > g_screenWidth) {
							runLength = (uint8_t)(g_screenWidth - x);
						}
						if (enabled) {
							memset(maskCursor, 1, runLength);
						} else {
							memset(maskCursor, 0, runLength);
						}
						maskCursor += (uint8_t)runLength;
						x += runLength;
						rowData += (uint8_t)runLength;
					}
				}

				if ((unsigned int)x < g_screenWidth) {
					maskCursor += g_screenWidth - x;
				}
			}

			Hud_CompressMask();
		}
	}
}

// FUNCTION: XWA 0x464FF0
void Hud_CompressMask(void) {
	uint8_t* src;
	uint8_t* dst;
	int compressedSize;
	unsigned int width;
	unsigned int x;
	unsigned int y;
	uint8_t value;
	uint8_t runCount;
	uint8_t len;

	src = g_cockpitMaskBitmap;
	dst = g_cockpitMaskRle;
	compressedSize = 0;
	runCount = 0;

	if (g_useHardware3D) {
		return;
	}

	if (src == NULL) {
		XWA_HUD_OUTPUT_DEBUG_STRING("NULL pMask in CompressMask()!\n");
		return;
	}
	if (dst == NULL) {
		XWA_HUD_OUTPUT_DEBUG_STRING("NULL pMaskData in CompressMask()!\n");
		return;
	}

#ifdef XWA_MODERN
	memset(dst, 0, sizeof(uint32_t));
#else
	*(uint32_t*)dst = 0;
#endif

	y = 0;
	if ((unsigned int)g_screenHeight > 0u) {
		do {
			width = (unsigned int)g_screenWidth;
			x = 0;
			if (width > 0u) {
				do {
					len = 0;
					if (*src == 0) {
						while (*src == 0) {
							if (x >= width) {
								break;
							}
							if ((int)len + 1 > 255) {
								break;
							}
							++len;
							++src;
							++x;
						}
					} else {
						if (*src == 1) {
							while (*src == 1) {
								if (x >= width) {
									break;
								}
								if ((int)len + 1 > 255) {
									break;
								}
								++len;
								++src;
								++x;
							}
						}
					}
					++runCount;
				} while (x < width);
			}

			*dst++ = runCount;
			++compressedSize;

			width = (unsigned int)g_screenWidth;
			src = &g_cockpitMaskBitmap[width * y];
			for (x = 0; x < (unsigned int)g_screenWidth; ++x) {
				if (runCount != 0) {
					while (runCount != 0) {
						if (x >= (unsigned int)g_screenWidth) {
							break;
						}

						len = 0;
						if (*src == 0) {
							value = 0;
							while (*src == 0) {
								if (x >= (unsigned int)g_screenWidth) {
									break;
								}
								if ((int)len + 1 > 255) {
									break;
								}
								++len;
								++src;
								++x;
							}
						} else {
							value = 1;
							if (*src == 1) {
								while (*src == 1) {
									if (x >= (unsigned int)g_screenWidth) {
										break;
									}
									if ((int)len + 1 > 255) {
										break;
									}
									++len;
									++src;
									++x;
								}
							}
						}

						*dst++ = value;
						compressedSize += 2;
						*dst++ = len;
						--runCount;
					}
				}

				*dst++ = 0xff;
				++compressedSize;
			}
			++y;
		} while (y < (unsigned int)g_screenHeight);
	}
	(void)compressedSize;
}

// FUNCTION: XWA 0x4651F0
void Hud_RedrawSoftwareHudFrame(void) {
	uint8_t savedLockBackBufferForHudDraw;

	if (g_useHardware3D) {
		return;
	}

	savedLockBackBufferForHudDraw = (uint8_t)g_flightLockBackBufferForHudDraw;
	g_flightLockBackBufferForHudDraw = 1;
	FlightSurface_Lock();
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
	g_flightFillClipRectFn();
	FlightSurface_Unlock();
	g_flightLockBackBufferForHudDraw = savedLockBackBufferForHudDraw;

	if (g_players[g_localPlayer].hudEnabled || g_filmPlaybackMode) {
		RenderScene_Initialize(1);
		g_hudElementEnabled[4].enabled = 1;
		g_hudElementEnabled[5].enabled = 1;
		g_hudElementEnabled[6].enabled = 1;
		g_hudElementEnabled[7].enabled = 1;
		g_hudElementEnabled[8].enabled = 1;
		g_hudElementEnabled[9].enabled = 1;
		if (g_filmPlaybackMode && !g_useHardware3D) {
			g_hudElementEnabled[10].enabled = 1;
			g_hudElementEnabled[11].enabled = 1;
		}
		Hud_RenderHud();
		FlightDisplay_BlitRenderSurface();
	} else {
		g_sceneBypassCockpitMask = 1;
		RenderScene_Initialize(1);
		g_sceneBypassCockpitMask = 0;
		FlightDisplay_BlitRenderSurface();
	}
}

// FUNCTION: XWA 0x4652F0
void Hud_SetupReticle(void) {
	void(XWA_HUD_STDCALL * outputDebugString)(const char*);
	int objectIndex;
	MobileObject* mobj;
	CraftData* craft;
	int hardpointSlot;
	int reticleSlot;
	int projOffsetY;

	outputDebugString = XWA_HUD_OUTPUT_DEBUG_STRING;
	objectIndex = g_players[g_localPlayer].objectIndex;
	if (objectIndex != 0xffff) {
		mobj = g_objectTable[objectIndex].mobj;
		if (mobj != NULL && mobj->pCraft != NULL) {
			craft = mobj->pCraft;
		} else {
			outputDebugString("GetCraftPointer() returned NULL in HUD.c\n");
			craft = NULL;
		}
	} else {
		outputDebugString("GetCraftPointer() returned NULL in HUD.c\n");
		craft = NULL;
	}

	if (craft == NULL) {
		outputDebugString("NULL craft data pointer in SetupReticle()!\n");
		return;
	}

	projOffsetY = g_projOffsetY;

	memset(g_reticleLaserAimPoints, 0, sizeof(g_reticleLaserAimPoints));
	memset(g_reticleLaserHardpointIndices, 0xff, sizeof(g_reticleLaserHardpointIndices));
	g_hudLaserChargeDisplayDrawn = 0;
	memset(g_reticleWarheadHardpointIndices, 0xff, sizeof(g_reticleWarheadHardpointIndices));

	{
		int laserCount;

		laserCount = 0;
		g_reticleCenterX = g_flightVpCenterX;
		g_reticleCenterY = g_flightVpCenterY + projOffsetY;
		g_reticleLaserHardpointCount = 0;
		g_reticleWarheadHardpointCount = 0;

		if (craft->laserSlotCount > 0) {
			int* laserIndexOut;
			int* warheadIndexOut;
			const uint8_t* weaponTypePtr;

			laserIndexOut = g_reticleLaserHardpointIndices;
			warheadIndexOut = g_reticleWarheadHardpointIndices;
			weaponTypePtr = &craft->warheadData[0].weaponType;
			hardpointSlot = 0;
			do {
				uint8_t weaponType;

				weaponType = *weaponTypePtr;
				if (weaponType == 1 || weaponType == 2) {
					++laserCount;
					*laserIndexOut = hardpointSlot;
					g_reticleLaserHardpointCount = laserCount;
					++laserIndexOut;
				} else if (weaponType == 4) {
					*warheadIndexOut = hardpointSlot;
					++g_reticleWarheadHardpointCount;
					++warheadIndexOut;
				}
				++hardpointSlot;
				weaponTypePtr += sizeof(WarheadInventoryEntry);
			} while (hardpointSlot < craft->laserSlotCount);
		}

		for (reticleSlot = 0; reticleSlot < laserCount; ++reticleSlot) {
			int worldZ;
			int screenX;
			int screenY;
			int centerX;
			int centerY;

			if (g_reticleLaserHardpointIndices[reticleSlot] == -1) {
				continue;
			}

			pai_RotateLocalVectorToWorldScratch(
				&g_objectTable[g_players[g_localPlayer].objectIndex],
				g_modelDefs[craft->modelIndex].weaponHardpoints[reticleSlot].x,
				g_modelDefs[craft->modelIndex].weaponHardpoints[reticleSlot].z,
				-g_modelDefs[craft->modelIndex].weaponHardpoints[reticleSlot].y);

			g_camRelWorldX = g_objectTable[g_players[g_localPlayer].objectIndex].world_x + g_rotatedX;
			g_camRelWorldY = g_objectTable[g_players[g_localPlayer].objectIndex].world_y + g_rotatedY;
			worldZ = g_objectTable[g_players[g_localPlayer].objectIndex].world_z + g_rotatedZ;

			g_camRelWorldX -= g_players[g_localPlayer].viewState.savedTargetX;
			g_camRelWorldY -= g_players[g_localPlayer].viewState.savedTargetY;
			g_camRelWorldZ = worldZ - g_players[g_localPlayer].viewState.savedTargetZ;

			viewX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
			viewY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
			viewZ = 1;

			screenX = TRANSFM2_ProjectScreenX(viewX, viewZ);
			screenY = TRANSFM2_ProjectScreenY(viewY, viewZ);
			{
				float deltaX;
				float deltaY;

				centerX = g_reticleCenterX;
				centerY = g_reticleCenterY;
				deltaX = (float)(screenX - centerX);
				deltaY = (float)(screenY - centerY);
				g_reticleLaserAimPoints[reticleSlot].x =
					centerX +
					(int)(g_reticleAimPointDistanceBias *
						  (deltaX / sqrt(deltaX * deltaX + deltaY * deltaY) * g_flightHudScaleFactor));
				g_reticleLaserAimPoints[reticleSlot].y =
					centerY +
					(int)(g_reticleAimPointDistanceBias *
						  (deltaY / sqrt(deltaX * deltaX + deltaY * deltaY) * g_flightHudScaleFactor));
			}
		}
	}

	if (g_useHardware3D) {
		Hud_SetupLaserChargePositions3D();
	}
	g_reticleDirty = 0;
}

// FUNCTION: XWA 0x465660
void Hud_SetupCraftEntryHudMasks(void) {
	uint8_t savedLockBackBufferForHudDraw;

	if (g_hangarAutoCam) {
		Hud_UpdateHUDMask(0, 0);
		Hud_UpdateHUDMask(1, 0);
		Hud_UpdateHUDMask(2, 0);
		Hud_UpdateHUDMask(3, 0);
		if (!g_filmPlaybackMode) {
			Hud_UpdateHUDMask(4, 0);
			Hud_UpdateHUDMask(5, 0);
		} else {
			Hud_UpdateHUDMask(4, (char)g_filmOverlayMfdVisible);
			Hud_UpdateHUDMask(5, (char)g_filmOverlayMfdVisible);
		}
	} else {
		Hud_UpdateHUDMask(0, 0);
		Hud_UpdateHUDMask(1, 0);
		Hud_UpdateHUDMask(2, 0);
		Hud_UpdateHUDMask(3, 1);
		Hud_UpdateHUDMask(4, 1);
		Hud_UpdateHUDMask(5, 1);
	}

	if (g_useHardware3D) {
		return;
	}

	savedLockBackBufferForHudDraw = (uint8_t)g_flightLockBackBufferForHudDraw;
	g_flightLockBackBufferForHudDraw = 1;
	FlightSurface_Lock();
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
	g_flightFillClipRectFn();
	FlightSurface_Unlock();
	g_flightLockBackBufferForHudDraw = savedLockBackBufferForHudDraw;

	if (g_players[g_localPlayer].hudEnabled || g_filmPlaybackMode) {
		RenderScene_Initialize(1);
		g_hudElementEnabled[4].enabled = 1;
		g_hudElementEnabled[5].enabled = 1;
		g_hudElementEnabled[6].enabled = 1;
		g_hudElementEnabled[7].enabled = 1;
		g_hudElementEnabled[8].enabled = 1;
		g_hudElementEnabled[9].enabled = 1;
		if (g_filmPlaybackMode && !g_useHardware3D) {
			g_hudElementEnabled[10].enabled = 1;
			g_hudElementEnabled[11].enabled = 1;
		}
		Hud_RenderHud();
		FlightDisplay_BlitRenderSurface();
	} else {
		g_sceneBypassCockpitMask = 1;
		RenderScene_Initialize(1);
		g_sceneBypassCockpitMask = 0;
		FlightDisplay_BlitRenderSurface();
	}
}

// FUNCTION: XWA 0x465810
void Hud_RestorePlayerHudState(int playerIdx) {
	g_mfdLeftNeedsRedraw = 1;
	g_mfdRightNeedsRedraw = 1;

	g_players[playerIdx].hudEnabled = g_players[playerIdx].savedHudEnabled;
	g_players[playerIdx].mfd.enabled[1] = g_players[playerIdx].mfd.savedSideEnabled[0];
	g_players[playerIdx].mfd.enabled[2] = g_players[playerIdx].mfd.savedSideEnabled[1];
	g_players[playerIdx].mfd.activeIndex = g_players[playerIdx].mfd.savedActiveIndex;
	g_players[playerIdx].mfd.page[1] = g_players[playerIdx].mfd.savedPage[1];
	g_players[playerIdx].mfd.page[2] = g_players[playerIdx].mfd.savedPage[2];

	if (playerIdx == g_localPlayer) {
		g_hudElementEnabled[0].enabled = g_savedHudCmdPanelEnabled;

		if (!g_useHardware3D) {
			if (g_players[g_localPlayer].hudEnabled) {
				if (g_players[g_localPlayer].mfd.enabled[0]) {
					Hud_UpdateHUDMask(3, 0);
					Hud_UpdateHUDMask(0, 0);
					Hud_UpdateHUDMask(1, 0);
					Hud_UpdateHUDMask(2, 0);
					if (g_filmPlaybackMode) {
						if (g_filmOverlayMfdVisible) {
							Hud_UpdateHUDMask(4, 1);
							Hud_UpdateHUDMask(5, 1);
						} else {
							Hud_UpdateHUDMask(4, 0);
							Hud_UpdateHUDMask(5, 0);
						}
					} else {
						Hud_UpdateHUDMask(4, 0);
						Hud_UpdateHUDMask(5, 0);
					}
				} else {
					Hud_UpdateHUDMask(0, (char)g_hudElementEnabled[1].enabled);
					Hud_UpdateHUDMask(1, (char)g_hudElementEnabled[2].enabled);
					Hud_UpdateHUDMask(2, (char)g_hudElementEnabled[3].enabled);
					Hud_UpdateHUDMask(3, (char)g_hudElementEnabled[0].enabled);
					Hud_UpdateHUDMask(4, g_players[g_localPlayer].mfd.enabled[1] || g_filmOverlayMfdVisible);
					Hud_UpdateHUDMask(5, g_players[g_localPlayer].mfd.enabled[2] || g_filmOverlayMfdVisible);
				}
			} else {
				Hud_UpdateHUDMask(0, 0);
				Hud_UpdateHUDMask(1, 0);
				Hud_UpdateHUDMask(2, 0);
				Hud_UpdateHUDMask(3, 0);
				if (g_filmPlaybackMode) {
					if (g_filmOverlayMfdVisible) {
						Hud_UpdateHUDMask(4, 1);
						Hud_UpdateHUDMask(5, 1);
					} else {
						Hud_UpdateHUDMask(4, 0);
						Hud_UpdateHUDMask(5, 0);
					}
				} else {
					Hud_UpdateHUDMask(4, 0);
					Hud_UpdateHUDMask(5, 0);
				}
			}
		}
	}

	if (!g_useHardware3D && playerIdx == g_localPlayer) {
		// Inlined copy of Hud_RedrawSoftwareHudFrame (minus its hardware-3D
		// guard, folded into the condition above), as in the original binary.
		char savedLockBackBufferForHudDraw;

		savedLockBackBufferForHudDraw = (char)g_flightLockBackBufferForHudDraw;
		g_flightLockBackBufferForHudDraw = 1;
		FlightSurface_Lock();
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
		g_flightFillClipRectFn();
		FlightSurface_Unlock();
		g_flightLockBackBufferForHudDraw = savedLockBackBufferForHudDraw;

		if (g_players[g_localPlayer].hudEnabled || g_filmPlaybackMode) {
			RenderScene_Initialize(1);
			g_hudElementEnabled[4].enabled = 1;
			g_hudElementEnabled[5].enabled = 1;
			g_hudElementEnabled[6].enabled = 1;
			g_hudElementEnabled[7].enabled = 1;
			g_hudElementEnabled[8].enabled = 1;
			g_hudElementEnabled[9].enabled = 1;
			if (g_filmPlaybackMode && !g_useHardware3D) {
				g_hudElementEnabled[10].enabled = 1;
				g_hudElementEnabled[11].enabled = 1;
			}
			Hud_RenderHud();
			FlightDisplay_BlitRenderSurface();
		} else {
			g_sceneBypassCockpitMask = 1;
			RenderScene_Initialize(1);
			g_sceneBypassCockpitMask = 0;
			FlightDisplay_BlitRenderSurface();
		}
	}
}

// FUNCTION: XWA 0x465B30
void Hud_RenderHud(void) {
	uint8_t filmPlaybackMode;
	int16_t paneType;

	filmPlaybackMode = g_filmPlaybackMode;

	if (filmPlaybackMode) {
		if (g_useHardware3D) {
			Hud_DrawFilmMfdFrames3D();
			if (g_filmOverlayActive) {
				if (g_systemMessagePaneVisible) {
					Hud_DrawSystemTextPane(g_systemMessagePane.text, g_systemMessagePane.paneType);
				}
				if (g_flightGroupMessagePaneVisible) {
					Hud_DrawFlightGroupTextPane(g_flightGroupMessagePane.text,
												g_flightGroupMessagePane.paneType);
				}
				if (g_readyMessagePaneVisible) {
					Hud_DrawReadyMessageTextPane(g_readyMessagePaneQueue[0].text,
												 g_readyMessagePaneQueue[0].paneType);
				}
			}
		} else {
			g_flightLockBackBufferForHudDraw = 1;
			FlightSurface_Lock();
			g_drawTarget->pixels = g_surfacePixels;
			g_drawTarget->pitch = g_surfacePitch;
			Hud_DrawFilmMfdFrames2D();
			FlightSurface_Unlock();
			g_flightLockBackBufferForHudDraw = 0;
		}

		if (g_filmOverlayActive == 1) {
			return;
		}
		filmPlaybackMode = g_filmPlaybackMode;
	}

	if (g_players[g_localPlayer].hudEnabled) {
		if (g_inHangarReady) {
			if (g_useHardware3D) {
				Hud_DrawCMD3D();
				Hud_DrawMfdFrames3D();
				if (g_systemMessagePaneVisible) {
					Hud_DrawSystemTextPane(g_systemMessagePane.text, g_systemMessagePane.paneType);
				}
				if (g_flightGroupMessagePaneVisible) {
					Hud_DrawFlightGroupTextPane(g_flightGroupMessagePane.text,
												g_flightGroupMessagePane.paneType);
				}
				if (g_readyMessagePaneVisible) {
					Hud_DrawReadyMessageTextPane(g_readyMessagePaneQueue[0].text,
												 g_readyMessagePaneQueue[0].paneType);
				}
			} else {
				g_flightLockBackBufferForHudDraw = 1;
				FlightSurface_Lock();
				g_drawTarget->pixels = g_surfacePixels;
				g_drawTarget->pitch = g_surfacePitch;
				Hud_DrawCMD2D();
				Hud_DrawMfdFrames2D();
				FlightSurface_Unlock();
				g_flightLockBackBufferForHudDraw = 0;
			}
			return;
		}

		if (g_players[g_localPlayer].mapCameraState) {
			if (g_useHardware3D) {
				Hud_DrawCMD3D();
				Hud_DrawMfdFrames3D();
				if (g_systemMessagePaneVisible) {
					Hud_DrawSystemTextPane(g_systemMessagePane.text, g_systemMessagePane.paneType);
				}
				if (g_flightGroupMessagePaneVisible) {
					Hud_DrawFlightGroupTextPane(g_flightGroupMessagePane.text,
												g_flightGroupMessagePane.paneType);
				}
				if (g_readyMessagePaneVisible) {
					Hud_DrawReadyMessageTextPane(g_readyMessagePaneQueue[0].text,
												 g_readyMessagePaneQueue[0].paneType);
				}
			} else {
				g_flightLockBackBufferForHudDraw = 1;
				FlightSurface_Lock();
				g_drawTarget->pixels = g_surfacePixels;
				g_drawTarget->pitch = g_surfacePitch;
				Hud_DrawCMD2D();
				Hud_DrawMfdFrames2D();
				FlightSurface_Unlock();
				g_flightLockBackBufferForHudDraw = 0;
			}
			return;
		}

		if (g_players[g_localPlayer].mfd.enabled[0]) {
			if (!g_players[g_localPlayer].regionSessionId && !g_flightMissionEndPending) {
				if (g_players[g_localPlayer].hyperspacePhase) {
					return;
				}
				if (!g_useHardware3D) {
					return;
				}

				Hud_DrawReticle3D();
				Hud_UpdateThreatIndicators();
				if (g_players[g_localPlayer].hyperspacePhase) {
					return;
				}
				if (!g_filmPlaybackMode || g_filmOverlayActive != 1) {
					Hud_DrawTargetArrow3D();
				}
				if (g_systemMessagePaneVisible) {
					paneType = g_systemMessagePane.paneType;
					DebugPrintfChannel(0x20, "SYSTEM TEXT: %s\n", g_systemMessagePane.text);
					HUD_PANE_PUSH(XWA_HUD_PANE_MESSAGE_SYSTEM, g_hudSystemMessagePaneX,
								  g_hudSystemMessagePaneY, g_hudSystemMessagePaneSurfaceWidth,
								  g_hudSystemMessagePaneSurfaceHeight);
					FlightText_SetRenderOffset(g_hudSystemMessagePaneX, g_hudSystemMessagePaneY);
					Hud_DrawHudMessageTextPane(
						g_hudSystemMessagePaneSurface, (uint16_t)g_hudSystemMessagePaneSurfaceWidth,
						g_hudSystemMessagePaneSurfaceHeight, g_systemMessagePane.text, paneType);
					FlightText_SetRenderOffset(0, 0);
					HUD_PANE_POP();
				}
				if (g_flightGroupMessagePaneVisible) {
					Hud_DrawFlightGroupTextPane(g_flightGroupMessagePane.text,
												g_flightGroupMessagePane.paneType);
				}
				if (g_readyMessagePaneVisible) {
					Hud_DrawReadyMessageTextPane(g_readyMessagePaneQueue[0].text,
												 g_readyMessagePaneQueue[0].paneType);
				}
				return;
			}
			fsfx_UpdateTargetingTone(0);
			return;
		}

		if (!g_flightMissionEndPending) {
			if (filmPlaybackMode && g_filmOverlayActive == 1) {
				return;
			}

			if (g_players[g_localPlayer].viewState.externalCameraActive &&
				!g_players[g_localPlayer].regionSessionId) {
				if (g_players[g_localPlayer].hyperspacePhase) {
					return;
				}
				if (g_useHardware3D) {
					Hud_DrawTargetArrow3D();
				}
				Hud_DetermineLockStatus();
				return;
			}

			if (!g_useHardware3D) {
				g_flightLockBackBufferForHudDraw = 1;
				FlightSurface_Lock();
				g_drawTarget->pixels = g_surfacePixels;
				g_drawTarget->pitch = g_surfacePitch;
				if (g_hudElementEnabled[6].enabled) {
					if (g_hudElementEnabled[3].enabled) {
						Hud_SetupResourceData(10000, 0x0a8c);
						Hud_DrawImageToDIB(g_hudCenterX - ((uint16_t)g_curImageWidth >> 1) - 114, 0);
						Hud_SetupResourceData(10000, 0x0af0);
						Hud_DrawImageToDIB(g_hudCenterX - ((uint16_t)g_curImageWidth >> 1) + 114, 0);
					} else {
						Hud_SetupResourceData(10000, 0x4fb0);
						Hud_DrawImageToDIB(g_hudCockpitMaskSprites[2].x, g_hudCockpitMaskSprites[2].y);
					}
				}
				g_hudElementEnabled[6].enabled = 0;
				Hud_DrawRadars2D();
				if (g_players[g_localPlayer].hyperspacePhase == PLAYER_HYPERSPACE_PHASE_NONE) {
					Hud_DrawRadarBlips();
				}
				Hud_DrawPowerSettings2D();
				Hud_DrawShieldStrength2D();
				Hud_DrawBeamStrength2D();
				Hud_DrawCMD2D();
				Hud_DrawMfdFrames2D();
				Hud_DrawHudTargetInsetIfEnabled(g_localPlayer);
				FlightSurface_Unlock();
				g_flightLockBackBufferForHudDraw = 0;
				return;
			}

			Hud_DrawRadarFrames3D();
			Hud_DrawRadars3D();
			if (g_players[g_localPlayer].hyperspacePhase == PLAYER_HYPERSPACE_PHASE_NONE) {
				Hud_DrawRadarBlips();
			}
			Hud_DrawPowerSettings3D();
			Hud_DrawLaserCharge3D();
			Hud_DrawShieldStrength3D();
			Hud_DrawBeamStrength3D();
			Hud_DrawReticle3D();
			Hud_UpdateThreatIndicators();
			if (g_players[g_localPlayer].hyperspacePhase == PLAYER_HYPERSPACE_PHASE_NONE) {
				Hud_DrawTargetArrow3D();
			}
			Hud_DrawCMD3D();
			Hud_DrawMfdFrames3D();
			if (g_players[g_localPlayer].hyperspacePhase == PLAYER_HYPERSPACE_PHASE_NONE) {
				if (g_systemMessagePaneVisible) {
					paneType = g_systemMessagePane.paneType;
					DebugPrintfChannel(0x20, "SYSTEM TEXT: %s\n", g_systemMessagePane.text);
					HUD_PANE_PUSH(XWA_HUD_PANE_MESSAGE_SYSTEM, g_hudSystemMessagePaneX,
								  g_hudSystemMessagePaneY, g_hudSystemMessagePaneSurfaceWidth,
								  g_hudSystemMessagePaneSurfaceHeight);
					FlightText_SetRenderOffset(g_hudSystemMessagePaneX, g_hudSystemMessagePaneY);
					Hud_DrawHudMessageTextPane(
						g_hudSystemMessagePaneSurface, (uint16_t)g_hudSystemMessagePaneSurfaceWidth,
						g_hudSystemMessagePaneSurfaceHeight, g_systemMessagePane.text, paneType);
					FlightText_SetRenderOffset(0, 0);
					HUD_PANE_POP();
				}
				if (g_flightGroupMessagePaneVisible) {
					Hud_DrawFlightGroupTextPane(g_flightGroupMessagePane.text,
												g_flightGroupMessagePane.paneType);
				}
				if (g_readyMessagePaneVisible) {
					Hud_DrawReadyMessageTextPane(g_readyMessagePaneQueue[0].text,
												 g_readyMessagePaneQueue[0].paneType);
				}
			}
			return;
		}
		fsfx_UpdateTargetingTone(0);
		return;
	}

	if (g_useHardware3D) {
		if ((!filmPlaybackMode || g_filmOverlayActive != 1) &&
			g_players[g_localPlayer].viewState.externalCameraActive) {
			Hud_DrawTargetArrow3D();
		}
		if (g_systemMessagePaneVisible) {
			paneType = g_systemMessagePane.paneType;
			DebugPrintfChannel(0x20, "SYSTEM TEXT: %s\n", g_systemMessagePane.text);
			HUD_PANE_PUSH(XWA_HUD_PANE_MESSAGE_SYSTEM, g_hudSystemMessagePaneX, g_hudSystemMessagePaneY,
						  g_hudSystemMessagePaneSurfaceWidth, g_hudSystemMessagePaneSurfaceHeight);
			FlightText_SetRenderOffset(g_hudSystemMessagePaneX, g_hudSystemMessagePaneY);
			Hud_DrawHudMessageTextPane(
				g_hudSystemMessagePaneSurface, (uint16_t)g_hudSystemMessagePaneSurfaceWidth,
				g_hudSystemMessagePaneSurfaceHeight, g_systemMessagePane.text, paneType);
			FlightText_SetRenderOffset(0, 0);
			HUD_PANE_POP();
		}
		if (g_flightGroupMessagePaneVisible) {
			Hud_DrawFlightGroupTextPane(g_flightGroupMessagePane.text, g_flightGroupMessagePane.paneType);
		}
		if (g_readyMessagePaneVisible) {
			Hud_DrawReadyMessageTextPane(g_readyMessagePaneQueue[0].text,
										 g_readyMessagePaneQueue[0].paneType);
		}
	}
	Hud_DetermineLockStatus();
}

// FUNCTION: XWA 0x4661F0
void Hud_SetHudEnabled(int playerIdx, char hudEnabled) {
	g_players[playerIdx].hudEnabled = (uint8_t)hudEnabled;
	if (!g_useHardware3D) {
		Hud_SyncSoftwareHudMasks(playerIdx, hudEnabled);
	}
	return;
}

// FUNCTION: XWA 0x466220
void Hud_ToggleMfdOverlay(int playerIdx) {
	uint8_t savedLockBackBufferForHudDraw;

	if (!g_players[playerIdx].hudEnabled) {
		return;
	}

	g_players[playerIdx].mfd.enabled[0] = (uint8_t)(g_players[playerIdx].mfd.enabled[0] == 0);
	if (playerIdx != g_localPlayer || g_useHardware3D) {
		return;
	}

	if (g_players[g_localPlayer].mfd.enabled[0]) {
		Hud_UpdateHUDMask(3, 0);
		Hud_UpdateHUDMask(0, 0);
		Hud_UpdateHUDMask(1, 0);
		Hud_UpdateHUDMask(2, 0);
		if (!g_filmPlaybackMode) {
			Hud_UpdateHUDMask(4, 0);
			Hud_UpdateHUDMask(5, 0);
		} else {
			Hud_UpdateHUDMask(4, (char)g_filmOverlayMfdVisible);
			Hud_UpdateHUDMask(5, (char)g_filmOverlayMfdVisible);
		}
	} else {
		if (!g_filmPlaybackMode) {
			Hud_UpdateHUDMask(3, (char)g_hudElementEnabled[0].enabled);
			Hud_UpdateHUDMask(0, (char)g_hudElementEnabled[1].enabled);
			Hud_UpdateHUDMask(1, (char)g_hudElementEnabled[2].enabled);
			Hud_UpdateHUDMask(2, (char)g_hudElementEnabled[3].enabled);
		} else if (!g_filmOverlayActive) {
			Hud_UpdateHUDMask(3, (char)g_hudElementEnabled[0].enabled);
			Hud_UpdateHUDMask(0, (char)g_hudElementEnabled[1].enabled);
			Hud_UpdateHUDMask(1, (char)g_hudElementEnabled[2].enabled);
			Hud_UpdateHUDMask(2, (char)g_hudElementEnabled[3].enabled);
		}

		if (!g_filmPlaybackMode) {
			Hud_UpdateHUDMask(4, (char)g_players[g_localPlayer].mfd.enabled[1]);
			Hud_UpdateHUDMask(5, (char)g_players[g_localPlayer].mfd.enabled[2]);
		} else if (!g_filmOverlayActive) {
			Hud_UpdateHUDMask(4, g_filmOverlayMfdVisible || g_players[g_localPlayer].mfd.enabled[1]);
			Hud_UpdateHUDMask(5, g_filmOverlayMfdVisible || g_players[g_localPlayer].mfd.enabled[2]);
		}
	}

	if (g_useHardware3D) {
		return;
	}

	savedLockBackBufferForHudDraw = (uint8_t)g_flightLockBackBufferForHudDraw;
	g_flightLockBackBufferForHudDraw = 1;
	FlightSurface_Lock();
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
	g_flightFillClipRectFn();
	FlightSurface_Unlock();
	g_flightLockBackBufferForHudDraw = savedLockBackBufferForHudDraw;

	if (g_players[g_localPlayer].hudEnabled || g_filmPlaybackMode) {
		RenderScene_Initialize(1);
		g_hudElementEnabled[4].enabled = 1;
		g_hudElementEnabled[5].enabled = 1;
		g_hudElementEnabled[6].enabled = 1;
		g_hudElementEnabled[7].enabled = 1;
		g_hudElementEnabled[8].enabled = 1;
		g_hudElementEnabled[9].enabled = 1;
		if (g_filmPlaybackMode && !g_useHardware3D) {
			g_hudElementEnabled[10].enabled = 1;
			g_hudElementEnabled[11].enabled = 1;
		}
		Hud_RenderHud();
		FlightDisplay_BlitRenderSurface();
	} else {
		g_sceneBypassCockpitMask = 1;
		RenderScene_Initialize(1);
		g_sceneBypassCockpitMask = 0;
		FlightDisplay_BlitRenderSurface();
	}
}

// FUNCTION: XWA 0x466530
void Hud_SetFilmOverlayMfdVisible(char visible) {
	uint8_t savedLockBackBufferForHudDraw;

	g_filmOverlayMfdVisible = (uint8_t)visible;
	if (!g_useHardware3D) {
		if (visible) {
			Hud_UpdateHUDMask(4, 1);
			Hud_UpdateHUDMask(5, 1);
		} else {
			if (g_filmOverlayActive || !g_players[g_localPlayer].hudEnabled ||
				g_players[g_localPlayer].mfd.enabled[0] == 1) {
				Hud_UpdateHUDMask(4, 0);
				Hud_UpdateHUDMask(5, 0);
			} else {
				if (!g_players[g_localPlayer].mfd.enabled[1]) {
					Hud_UpdateHUDMask(4, 0);
				}
				if (!g_players[g_localPlayer].mfd.enabled[2]) {
					Hud_UpdateHUDMask(5, 0);
				}
			}
		}

		if (!g_useHardware3D) {
			savedLockBackBufferForHudDraw = (uint8_t)g_flightLockBackBufferForHudDraw;
			g_flightLockBackBufferForHudDraw = 1;
			FlightSurface_Lock();
			FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
			FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
			g_flightFillClipRectFn();
			FlightSurface_Unlock();
			g_flightLockBackBufferForHudDraw = savedLockBackBufferForHudDraw;

			if (g_players[g_localPlayer].hudEnabled || g_filmPlaybackMode) {
				RenderScene_Initialize(1);
				g_hudElementEnabled[4].enabled = 1;
				g_hudElementEnabled[5].enabled = 1;
				g_hudElementEnabled[6].enabled = 1;
				g_hudElementEnabled[7].enabled = 1;
				g_hudElementEnabled[8].enabled = 1;
				g_hudElementEnabled[9].enabled = 1;
				if (g_filmPlaybackMode && !g_useHardware3D) {
					g_hudElementEnabled[10].enabled = 1;
					g_hudElementEnabled[11].enabled = 1;
				}
				Hud_RenderHud();
			} else {
				g_sceneBypassCockpitMask = 1;
				RenderScene_Initialize(1);
				g_sceneBypassCockpitMask = 0;
			}
			FlightDisplay_BlitRenderSurface();
		}
	}

	if (!g_useHardware3D) {
		g_hudElementEnabled[10].enabled = 1;
		g_hudElementEnabled[11].enabled = 1;
	}

	g_mfdLeftNeedsRedraw = 1;
	g_mfdRightNeedsRedraw = 1;
}

// FUNCTION: XWA 0x4666F0
void Hud_MarkFilmOverlayElementsVisible(void) {
	if (!g_useHardware3D) {
		g_hudElementEnabled[10].enabled = 1;
		g_hudElementEnabled[11].enabled = 1;
	}
}

// FUNCTION: XWA 0x466710
void Hud_SyncLocalSoftwareHudMasks(char hudEnabled) {
	if (!g_useHardware3D) {
		Hud_SyncSoftwareHudMasks(g_localPlayer, hudEnabled);
		if (!g_useHardware3D) {
			g_hudElementEnabled[10].enabled = 1;
			g_hudElementEnabled[11].enabled = 1;
		}
	}
}

// FUNCTION: XWA 0x466750
void Hud_SyncSoftwareHudMasks(int playerIdx, char hudEnabled) {
	uint8_t savedLockBackBufferForHudDraw;

	if (g_useHardware3D || playerIdx != g_localPlayer) {
		return;
	}

	if (hudEnabled) {
		if (g_inHangarReady) {
			Hud_UpdateHUDMask(3, 1);
			if (!g_filmPlaybackMode) {
				Hud_UpdateHUDMask(4, 1);
				Hud_UpdateHUDMask(5, 1);
			} else {
				Hud_UpdateHUDMask(4, !g_hangarAutoCam || g_filmOverlayMfdVisible);
				Hud_UpdateHUDMask(5, !g_hangarAutoCam || g_filmOverlayMfdVisible);
			}
			if (g_useHardware3D) {
				return;
			}

			savedLockBackBufferForHudDraw = (uint8_t)g_flightLockBackBufferForHudDraw;
			g_flightLockBackBufferForHudDraw = 1;
			FlightSurface_Lock();
			FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
			FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
			g_flightFillClipRectFn();
			FlightSurface_Unlock();
			g_flightLockBackBufferForHudDraw = savedLockBackBufferForHudDraw;

			if (g_players[g_localPlayer].hudEnabled || g_filmPlaybackMode) {
				RenderScene_Initialize(1);
				g_hudElementEnabled[4].enabled = 1;
				g_hudElementEnabled[5].enabled = 1;
				g_hudElementEnabled[6].enabled = 1;
				g_hudElementEnabled[7].enabled = 1;
				g_hudElementEnabled[8].enabled = 1;
				g_hudElementEnabled[9].enabled = 1;
				if (g_filmPlaybackMode && !g_useHardware3D) {
					g_hudElementEnabled[10].enabled = 1;
					g_hudElementEnabled[11].enabled = 1;
				}
				Hud_RenderHud();
				FlightDisplay_BlitRenderSurface();
			} else {
				g_sceneBypassCockpitMask = 1;
				RenderScene_Initialize(1);
				g_sceneBypassCockpitMask = 0;
				FlightDisplay_BlitRenderSurface();
			}
			return;
		}

		if (g_players[g_localPlayer].mapCameraState) {
			Hud_UpdateHUDMask(3, (char)g_hudElementEnabled[0].enabled);
			if (!g_filmPlaybackMode) {
				Hud_UpdateHUDMask(4, (char)g_players[g_localPlayer].mfd.enabled[1]);
				Hud_UpdateHUDMask(5, (char)g_players[g_localPlayer].mfd.enabled[2]);
			} else {
				Hud_UpdateHUDMask(4, (char)g_filmOverlayMfdVisible);
				Hud_UpdateHUDMask(5, (char)g_filmOverlayMfdVisible);
			}
		}
		if (g_players[g_localPlayer].mfd.enabled[0]) {
			Hud_UpdateHUDMask(3, 0);
			Hud_UpdateHUDMask(0, 0);
			Hud_UpdateHUDMask(1, 0);
			Hud_UpdateHUDMask(2, 0);
			if (g_filmPlaybackMode) {
				Hud_UpdateHUDMask(4, (char)g_filmOverlayMfdVisible);
				Hud_UpdateHUDMask(5, (char)g_filmOverlayMfdVisible);
			} else {
				Hud_UpdateHUDMask(4, 0);
				Hud_UpdateHUDMask(5, 0);
			}
		} else if (g_players[g_localPlayer].hudEnabled) {
			if (!g_filmPlaybackMode) {
				Hud_UpdateHUDMask(3, (char)g_hudElementEnabled[0].enabled);
				Hud_UpdateHUDMask(0, (char)g_hudElementEnabled[1].enabled);
				Hud_UpdateHUDMask(1, (char)g_hudElementEnabled[2].enabled);
				Hud_UpdateHUDMask(2, (char)g_hudElementEnabled[3].enabled);
			} else if (!g_filmOverlayActive) {
				Hud_UpdateHUDMask(3, (char)g_hudElementEnabled[0].enabled);
				Hud_UpdateHUDMask(0, (char)g_hudElementEnabled[1].enabled);
				Hud_UpdateHUDMask(1, (char)g_hudElementEnabled[2].enabled);
				Hud_UpdateHUDMask(2, (char)g_hudElementEnabled[3].enabled);
			}

			if (!g_filmPlaybackMode) {
				Hud_UpdateHUDMask(4, (char)g_players[g_localPlayer].mfd.enabled[1]);
				Hud_UpdateHUDMask(5, (char)g_players[g_localPlayer].mfd.enabled[2]);
			} else if (!g_filmOverlayActive) {
				Hud_UpdateHUDMask(4, g_players[g_localPlayer].mfd.enabled[1] || g_filmOverlayMfdVisible);
				if (g_players[g_localPlayer].mfd.enabled[2] || g_filmOverlayMfdVisible) {
					Hud_UpdateHUDMask(5, 1);
				} else {
					Hud_UpdateHUDMask(5, 0);
				}
			}
		} else {
			Hud_UpdateHUDMask(3, 0);
			Hud_UpdateHUDMask(0, 0);
			Hud_UpdateHUDMask(1, 0);
			Hud_UpdateHUDMask(2, 0);
			if (g_filmPlaybackMode && g_filmOverlayMfdVisible) {
				Hud_UpdateHUDMask(4, 1);
				Hud_UpdateHUDMask(5, 1);
			} else {
				Hud_UpdateHUDMask(4, 0);
				Hud_UpdateHUDMask(5, 0);
			}
		}
	} else {
		Hud_UpdateHUDMask(3, 0);
		Hud_UpdateHUDMask(0, 0);
		Hud_UpdateHUDMask(1, 0);
		Hud_UpdateHUDMask(2, 0);
		if (g_filmPlaybackMode && g_filmOverlayMfdVisible) {
			Hud_UpdateHUDMask(4, 1);
			Hud_UpdateHUDMask(5, 1);
		} else {
			Hud_UpdateHUDMask(4, 0);
			Hud_UpdateHUDMask(5, 0);
		}
	}

	if (g_useHardware3D) {
		return;
	}

	savedLockBackBufferForHudDraw = (uint8_t)g_flightLockBackBufferForHudDraw;
	g_flightLockBackBufferForHudDraw = 1;
	FlightSurface_Lock();
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
	g_flightFillClipRectFn();
	FlightSurface_Unlock();
	g_flightLockBackBufferForHudDraw = savedLockBackBufferForHudDraw;

	if (g_players[g_localPlayer].hudEnabled || g_filmPlaybackMode) {
		RenderScene_Initialize(1);
		g_hudElementEnabled[4].enabled = 1;
		g_hudElementEnabled[5].enabled = 1;
		g_hudElementEnabled[6].enabled = 1;
		g_hudElementEnabled[7].enabled = 1;
		g_hudElementEnabled[8].enabled = 1;
		g_hudElementEnabled[9].enabled = 1;
		if (g_filmPlaybackMode && !g_useHardware3D) {
			g_hudElementEnabled[10].enabled = 1;
			g_hudElementEnabled[11].enabled = 1;
		}
		Hud_RenderHud();
		FlightDisplay_BlitRenderSurface();
	} else {
		g_sceneBypassCockpitMask = 1;
		RenderScene_Initialize(1);
		g_sceneBypassCockpitMask = 0;
		FlightDisplay_BlitRenderSurface();
	}
}

// FUNCTION: XWA 0x466CA0
void Hud_EnableHudDrawElements(void) {
	g_hudElementEnabled[4].enabled = 1;
	g_hudElementEnabled[5].enabled = 1;
	g_hudElementEnabled[6].enabled = 1;
	g_hudElementEnabled[7].enabled = 1;
	g_hudElementEnabled[8].enabled = 1;
	g_hudElementEnabled[9].enabled = 1;

	if (g_filmPlaybackMode && !g_useHardware3D) {
		g_hudElementEnabled[10].enabled = 1;
		g_hudElementEnabled[11].enabled = 1;
	}
}

#ifndef XWA_MODERN
#pragma auto_inline(off)
#pragma inline_depth(0)
#endif
// FUNCTION: XWA 0x466CE0
void Hud_DrawRadars2D(void) {
	enum {
		HUD_RADAR_FORE_SCOPE_SPRITE = 0x1194u,
		HUD_RADAR_FORE_MASK_OK_SPRITE = 0x4e20u,
		HUD_RADAR_FORE_MASK_DOWN_SPRITE = 0x4ee8u,
		HUD_RADAR_AFT_SCOPE_OK_SPRITE = 0x0190u,
		HUD_RADAR_AFT_SCOPE_DOWN_SPRITE = 0x11f8u,
		HUD_RADAR_AFT_MASK_OK_SPRITE = 0x4e84u,
		HUD_RADAR_AFT_MASK_DOWN_SPRITE = 0x4f4cu,
		HUD_RADAR_FORE_SYSTEM_DOWN = 0x0001u,
		HUD_RADAR_AFT_SYSTEM_DOWN = 0x0100u
	};
	void(XWA_HUD_STDCALL * debugOutput)(const char*);
	CraftData* craft;
	int objectIndex;
	MobileObject* mobj;

	debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
	objectIndex = g_players[g_localPlayer].objectIndex;
	if (objectIndex != 0xffff && (mobj = g_objectTable[objectIndex].mobj) != NULL && mobj->pCraft != NULL) {
		craft = mobj->pCraft;
	} else {
		debugOutput("GetCraftPointer() returned NULL in HUD.c\n");
		craft = NULL;
	}

	if (craft == NULL) {
		debugOutput("NULL Craft pointer in DrawRadars2D()!\n");
		return;
	}

	if (g_hudElementEnabled[1].enabled) {
		if (g_hudElementEnabled[4].enabled) {
			uint8_t* palette;

			if (g_hudUseAlphaSpriteAtlas10100) {
				g_curImage = SpriteResource_ResolveSprite(10000 + g_hudAlphaSpriteGroupOffset,
														  HUD_RADAR_FORE_SCOPE_SPRITE);
			} else {
				g_curImage = SpriteResource_ResolveSprite(10000, HUD_RADAR_FORE_SCOPE_SPRITE);
			}
			if (g_curImage == NULL) {
				debugOutput("Null pointer to image in SetupResourceData()\n");
			} else {
				g_curImageWidth = g_curImage->width;
				g_curImageHeight = g_curImage->height;
				g_curImageRLE = SpriteResource_GetRowData(g_curImage);
#ifdef XWA_MODERN
				palette = SpriteResource_GetMutableSpritePayload(g_curImage);
#else
				palette = g_curImage->pixels;
#endif
				g_curImagePalette = (uint16_t*)palette;
				g_curImagePalette = (uint16_t*)(palette + ((SpritePayload*)palette)->palette16Offset);
			}

			if (g_curImage == NULL) {
				debugOutput("Null image pointer in DrawImageToDIB()!\n");
			} else if (g_drawTarget->clipY1 > 0 && (int16_t)g_curImage->height >= g_drawTarget->clipY0 &&
					   g_drawTarget->clipX1 > 0 && (int16_t)g_curImage->width >= g_drawTarget->clipX0) {
				int spriteType;

				spriteType = g_curImage->type;
				if (spriteType != 7) {
					if (spriteType == 23) {
						Hud_BlitSpriteType23(0, 0);
					}
				} else {
					Hud_BlitSpriteType7(0, 0);
				}
			}
		}
	} else if (g_hudElementEnabled[4].enabled) {
		if ((craft->systemFlags & HUD_RADAR_FORE_SYSTEM_DOWN) != 0) {
			uint8_t* palette;

			if (g_hudUseAlphaSpriteAtlas10100) {
				g_curImage = SpriteResource_ResolveSprite(10000 + g_hudAlphaSpriteGroupOffset,
														  HUD_RADAR_FORE_MASK_DOWN_SPRITE);
			} else {
				g_curImage = SpriteResource_ResolveSprite(10000, HUD_RADAR_FORE_MASK_DOWN_SPRITE);
			}
			if (g_curImage == NULL) {
				debugOutput("Null pointer to image in SetupResourceData()\n");
			} else {
				g_curImageWidth = g_curImage->width;
				g_curImageHeight = g_curImage->height;
				g_curImageRLE = SpriteResource_GetRowData(g_curImage);
#ifdef XWA_MODERN
				palette = SpriteResource_GetMutableSpritePayload(g_curImage);
#else
				palette = g_curImage->pixels;
#endif
				g_curImagePalette = (uint16_t*)palette;
				g_curImagePalette = (uint16_t*)(palette + ((SpritePayload*)palette)->palette16Offset);
			}

			if (g_curImage == NULL) {
				debugOutput("Null image pointer in DrawImageToDIB()!\n");
			} else if (g_hudCockpitMaskSprites[0].y < g_drawTarget->clipY1 &&
					   g_hudCockpitMaskSprites[0].y + (int16_t)g_curImage->height >= g_drawTarget->clipY0 &&
					   g_hudCockpitMaskSprites[0].x < g_drawTarget->clipX1 &&
					   g_hudCockpitMaskSprites[0].x + (int16_t)g_curImage->width >= g_drawTarget->clipX0) {
				int spriteType;

				spriteType = g_curImage->type;
				if (spriteType != 7) {
					if (spriteType == 23) {
						Hud_BlitSpriteType23(g_hudCockpitMaskSprites[0].x, g_hudCockpitMaskSprites[0].y);
					}
				} else {
					Hud_BlitSpriteType7(g_hudCockpitMaskSprites[0].x, g_hudCockpitMaskSprites[0].y);
				}
			}
		} else {
			uint8_t* palette;

			if (g_hudUseAlphaSpriteAtlas10100) {
				g_curImage = SpriteResource_ResolveSprite(10000 + g_hudAlphaSpriteGroupOffset,
														  HUD_RADAR_FORE_MASK_OK_SPRITE);
			} else {
				g_curImage = SpriteResource_ResolveSprite(10000, HUD_RADAR_FORE_MASK_OK_SPRITE);
			}
			if (g_curImage == NULL) {
				debugOutput("Null pointer to image in SetupResourceData()\n");
			} else {
				g_curImageWidth = g_curImage->width;
				g_curImageHeight = g_curImage->height;
				g_curImageRLE = SpriteResource_GetRowData(g_curImage);
#ifdef XWA_MODERN
				palette = SpriteResource_GetMutableSpritePayload(g_curImage);
#else
				palette = g_curImage->pixels;
#endif
				g_curImagePalette = (uint16_t*)palette;
				g_curImagePalette = (uint16_t*)(palette + ((SpritePayload*)palette)->palette16Offset);
			}
			Hud_DrawImageToDIB(g_hudCockpitMaskSprites[0].x, g_hudCockpitMaskSprites[0].y);
		}
	}

	if (g_hudElementEnabled[2].enabled) {
		if (g_hudElementEnabled[5].enabled) {
			if ((craft->activeHudFeatureMask & HUD_RADAR_AFT_SYSTEM_DOWN) != 0) {
				if ((craft->systemFlags & HUD_RADAR_AFT_SYSTEM_DOWN) != 0) {
					Hud_SetupResourceData(10000, HUD_RADAR_AFT_SCOPE_DOWN_SPRITE);
				} else {
					Hud_SetupResourceData(10000, HUD_RADAR_AFT_SCOPE_OK_SPRITE);
				}
				Hud_DrawImageToDIB(g_screenWidth - g_curImageWidth, 0);
			} else {
				if ((craft->systemFlags & HUD_RADAR_AFT_SYSTEM_DOWN) != 0) {
					Hud_SetupResourceData(10000, HUD_RADAR_AFT_SCOPE_DOWN_SPRITE);
				} else {
					Hud_SetupResourceData(10000, HUD_RADAR_AFT_SCOPE_OK_SPRITE);
				}
				Hud_DrawImageToDIB(g_screenWidth - g_curImageWidth, 0);
			}
		}
	} else if (g_hudElementEnabled[5].enabled) {
		if ((craft->systemFlags & HUD_RADAR_AFT_SYSTEM_DOWN) != 0) {
			Hud_SetupResourceData(10000, HUD_RADAR_AFT_MASK_DOWN_SPRITE);
			Hud_DrawImageToDIB(g_hudCockpitMaskSprites[1].x, g_hudCockpitMaskSprites[1].y);
		} else {
			Hud_SetupResourceData(10000, HUD_RADAR_AFT_MASK_OK_SPRITE);
			Hud_DrawImageToDIB(g_hudCockpitMaskSprites[1].x, g_hudCockpitMaskSprites[1].y);
		}
	}

	if (g_hudElementEnabled[4].enabled) {
		g_hudElementEnabled[4].enabled = 0;
	}
	if (g_hudElementEnabled[5].enabled) {
		g_hudElementEnabled[5].enabled = 0;
	}
}
#ifndef XWA_MODERN
#pragma inline_depth(255)
#pragma auto_inline(on)
#endif

static __inline void Hud_SetupPowerSprite2D(uint16_t spriteId) {
	uint8_t* palette;

	if (g_hudUseAlphaSpriteAtlas10100) {
		g_curImage = SpriteResource_ResolveSprite(10000 + g_hudAlphaSpriteGroupOffset, spriteId);
	} else {
		g_curImage = SpriteResource_ResolveSprite(10000, spriteId);
	}

	if (g_curImage == NULL) {
		XWA_HUD_OUTPUT_DEBUG_STRING("Null pointer to image in SetupResourceData()\n");
	} else {
		g_curImageWidth = g_curImage->width;
		g_curImageHeight = g_curImage->height;
		g_curImageRLE = SpriteResource_GetRowData(g_curImage);
#ifdef XWA_MODERN
		palette = SpriteResource_GetMutableSpritePayload(g_curImage);
#else
		palette = g_curImage->pixels;
#endif
		g_curImagePalette = (uint16_t*)palette;
		g_curImagePalette = (uint16_t*)(palette + ((SpritePayload*)palette)->palette16Offset);
	}
}

static __inline void Hud_DrawPowerBlank2D(int16_t x, int16_t y) {
	Hud_SetupPowerSprite2D(0x5140);

	if (g_curImage == NULL) {
		XWA_HUD_OUTPUT_DEBUG_STRING("Null image pointer in DrawImageToDIB()!\n");
	} else if (y < g_drawTarget->clipY1 && y + (int16_t)g_curImage->height >= g_drawTarget->clipY0 &&
			   x < g_drawTarget->clipX1 && x + (int16_t)g_curImage->width >= g_drawTarget->clipX0) {
		int spriteType;

		spriteType = g_curImage->type;
		if (spriteType != 7) {
			if (spriteType == 23) {
				Hud_BlitSpriteType23(x, y);
			}
		} else {
			Hud_BlitSpriteType7(x, y);
		}
	}
}

static __inline void Hud_DrawPowerPipColumn2D(uint16_t count, int xCenter, int yCenter, int yStep,
											  int16_t blankX, int16_t blankY) {
	int drawX;
	int drawY;

	drawX = xCenter - ((uint16_t)g_curImageWidth >> 1);
	drawY = yCenter - ((uint16_t)g_curImageHeight >> 1);
	if (count != 0) {
		unsigned int i;

		for (i = 0; i < count; ++i) {
			if (g_curImage == NULL) {
				XWA_HUD_OUTPUT_DEBUG_STRING("Null image pointer in DrawImageToDIB()!\n");
			} else if ((int16_t)drawY < g_drawTarget->clipY1 &&
					   (int16_t)drawY + (int16_t)g_curImage->height >= g_drawTarget->clipY0 &&
					   (int16_t)drawX < g_drawTarget->clipX1 &&
					   (int16_t)drawX + (int16_t)g_curImage->width >= g_drawTarget->clipX0) {
				int spriteType;

				spriteType = g_curImage->type;
				if (spriteType != 7) {
					if (spriteType == 23) {
						Hud_BlitSpriteType23((int16_t)drawX, drawY);
					}
				} else {
					Hud_BlitSpriteType7((int16_t)drawX, drawY);
				}
			}
			drawY += yStep;
		}
	} else {
		Hud_DrawPowerBlank2D(blankX, blankY);
	}
}

// FUNCTION: XWA 0x467110
void Hud_DrawPowerSettings2D(void) {
	enum {
		HUD_POWER_2D_LASER_REBEL_PIP_SPRITE = 0x28a0,
		HUD_POWER_2D_LASER_IMPERIAL_PIP_SPRITE = 0x28d2,
		HUD_POWER_2D_SHIELD_PIP_SPRITE = 0x2904,
		HUD_POWER_2D_BEAM_PIP_SPRITE = 0x2968,
		HUD_POWER_2D_BLANK_SPRITE = 0x5140,
		HUD_POWER_2D_RESERVE_SPRITE = 0x0514,
		HUD_POWER_2D_BEAM_RESERVE_SPRITE = 0x0578
	};
	CraftData* craft;
	void(XWA_HUD_STDCALL * debugOutput)(const char*);
	uint8_t hasBeamSystem;
	unsigned int reserveIdx;

	hasBeamSystem = 0;
	debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
	craft = Hud_GetCraftPointerInlinedWithDebug(debugOutput);
	if (craft == NULL) {
		debugOutput("NULL craft data pointer in DrawPowerSettings2D()!\n");
		return;
	}

	if (g_hudElementEnabled[1].enabled && g_hudElementEnabled[9].enabled) {
		if ((craft->activeHudFeatureMask & 0x200u) != 0) {
			uint16_t laserRedirect;

			laserRedirect = craft->laserRedirect;
			if (craft->laserProjectileTypeId[0] == OBJ_LaserRebel) {
				Hud_SetupPowerSprite2D(HUD_POWER_2D_LASER_REBEL_PIP_SPRITE);
			} else {
				Hud_SetupPowerSprite2D(HUD_POWER_2D_LASER_IMPERIAL_PIP_SPRITE);
			}
			Hud_DrawPowerPipColumn2D(laserRedirect, 3, 38, -10, 2, 3);
		}

		if ((craft->systemFlags & 1u) != 0 && (craft->activeHudFeatureMask & 0x800u) != 0) {
			uint16_t shieldRedirect;

			shieldRedirect = craft->shieldRedirect;
			Hud_SetupPowerSprite2D(HUD_POWER_2D_SHIELD_PIP_SPRITE);
			Hud_DrawPowerPipColumn2D(shieldRedirect, 3, 83, -10, 2, 48);
		}
	}

	if (g_hudElementEnabled[2].enabled && g_hudElementEnabled[9].enabled) {
		if ((craft->systemFlags & 0x100u) != 0) {
			hasBeamSystem = 1;
			if ((craft->activeHudFeatureMask & 0x1000u) != 0) {
				uint16_t beamLevel;
				int drawX;
				int drawY;
				uint8_t* palette;

				beamLevel = craft->beamLevel;
				if (g_hudUseAlphaSpriteAtlas10100) {
					g_curImage = SpriteResource_ResolveSprite(10000 + g_hudAlphaSpriteGroupOffset,
															  HUD_POWER_2D_BEAM_PIP_SPRITE);
				} else {
					g_curImage = SpriteResource_ResolveSprite(10000, HUD_POWER_2D_BEAM_PIP_SPRITE);
				}
				if (g_curImage == NULL) {
					XWA_HUD_OUTPUT_DEBUG_STRING("Null pointer to image in SetupResourceData()\n");
				} else {
					g_curImageWidth = g_curImage->width;
					g_curImageHeight = g_curImage->height;
					g_curImageRLE = SpriteResource_GetRowData(g_curImage);
#ifdef XWA_MODERN
					palette = SpriteResource_GetMutableSpritePayload(g_curImage);
#else
					palette = g_curImage->pixels;
#endif
					g_curImagePalette = (uint16_t*)palette;
					g_curImagePalette = (uint16_t*)(palette + ((SpritePayload*)palette)->palette16Offset);
				}

				drawX = g_screenWidth - ((uint16_t)g_curImageWidth >> 1) - 3;
				drawY = 83 - ((uint16_t)g_curImageHeight >> 1);
				if (beamLevel != 0) {
					unsigned int i;

					for (i = 0; i < beamLevel; ++i) {
						if (g_curImage == NULL) {
							XWA_HUD_OUTPUT_DEBUG_STRING("Null image pointer in DrawImageToDIB()!\n");
						} else if ((int16_t)drawY < g_drawTarget->clipY1 &&
								   (int16_t)drawY + (int16_t)g_curImage->height >= g_drawTarget->clipY0 &&
								   (int16_t)drawX < g_drawTarget->clipX1 &&
								   (int16_t)drawX + (int16_t)g_curImage->width >= g_drawTarget->clipX0) {
							int spriteType;

							spriteType = g_curImage->type;
							if (spriteType != 7) {
								if (spriteType == 23) {
									Hud_BlitSpriteType23((int16_t)drawX, drawY);
								}
							} else {
								Hud_BlitSpriteType7((int16_t)drawX, drawY);
							}
						}
						drawY -= 10;
					}
				} else {
					Hud_SetupResourceData(10000, HUD_POWER_2D_BLANK_SPRITE);
					Hud_DrawImageToDIB(636, 49);
				}
			}
		}

		if ((craft->activeHudFeatureMask & 0x400u) != 0) {
			Hud_SetupResourceData(10000, HUD_POWER_2D_BLANK_SPRITE);
			Hud_DrawImageToDIB(636, 3);
			if (hasBeamSystem) {
				uint16_t reserveCount;
				int16_t drawX;

				reserveCount = (uint16_t)(8u - craft->laserRedirect);
				if ((craft->systemFlags & 1u) != 0) {
					reserveCount = (uint16_t)(reserveCount + 2u - craft->shieldRedirect);
				}
				if ((craft->systemFlags & 0x100u) != 0) {
					reserveCount = (uint16_t)(reserveCount + 2u - craft->beamLevel);
				}

				Hud_SetupResourceData(10000, HUD_POWER_2D_BEAM_RESERVE_SPRITE);
				drawX = (int16_t)(g_screenWidth - ((uint16_t)g_curImageWidth >> 1) - 3);
				if (reserveCount != 0) {
					for (reserveIdx = 0; reserveIdx < reserveCount; ++reserveIdx) {
						int drawY;

						drawY = (int)((double)(42 - ((uint16_t)g_curImageWidth >> 1)) -
									  (double)reserveIdx * g_hudPowerBeamReserveStep);
						Hud_DrawImageToDIB(drawX, drawY);
					}
				}
			} else {
				int16_t drawX;
				int16_t drawY;
				uint16_t reserveCount;

				reserveCount = 8;
				reserveCount = (uint16_t)(reserveCount - craft->shieldRedirect);
				reserveCount = (uint16_t)(reserveCount - craft->laserRedirect);
				Hud_SetupResourceData(10000, HUD_POWER_2D_RESERVE_SPRITE);
				drawX = (int16_t)(g_screenWidth - ((uint16_t)g_curImageWidth >> 1) - 3);
				drawY = 41 - ((uint16_t)g_curImageHeight >> 1);
				if (reserveCount != 0) {
					for (reserveIdx = 0; reserveIdx < reserveCount; ++reserveIdx) {
						Hud_DrawImageToDIB(drawX, drawY);
						drawY -= 5;
					}
				}
			}
		}
	}
	if (g_hudElementEnabled[9].enabled) {
		g_hudElementEnabled[9].enabled = 0;
	}
}

static uint16_t Hud_GetShieldHullIconSpriteId(void) {
	int objectIndex;
	ObjectTypeId objectType;
	uint16_t spriteId;

	objectIndex = g_players[g_localPlayer].objectIndex;
	if (objectIndex == 0xffff) {
		return 0;
	}

	objectType = g_objectTable[objectIndex].objectType;
	if (objectType == OBJ_None || (uint16_t)objectType > (uint16_t)OBJ_ContainerBrick) {
		return 0;
	}

	spriteId = g_shieldSilhouetteSpriteIdByObjectType[(uint16_t)objectType];
	return spriteId != 0 ? spriteId : 0;
}

static uint16_t Hud_GetHullDamageColorIndex(const CraftData* craft) {
	uint16_t colorIndex;

	if (g_playerFlightTransientTimers[g_localPlayer].field_04) {
		return 3;
	}

	if ((uint32_t)(craft->hullMax / 3u) != 0) {
		uint32_t damageLevel;

		damageLevel = (uint32_t)craft->hullDamage / (uint32_t)(craft->hullMax / 3u);
		if ((uint16_t)damageLevel > 2u) {
			damageLevel = 2;
		}
		colorIndex = (uint16_t)(2u - (uint16_t)damageLevel);
	} else {
		colorIndex = 2;
	}
	return colorIndex;
}

static int Hud_GetHullDamageSpriteGroup(uint16_t colorIndex) {
	switch (colorIndex) {
		case 0:
			return 25020;
		case 1:
			return 25030;
		case 2:
			return 25010;
		case 3:
			return 25000;
		default:
			return 0;
	}
}

static void Hud_ComputeShieldBarSegments(int shield, int halfMaxShield, uint16_t* lowerSegments,
										 uint16_t* upperSegments) {
	if (shield < 0) {
		shield = 0;
	}

	if (shield < halfMaxShield) {
		uint16_t percent;

		percent = (uint16_t)MATH2_percentage((uint32_t)shield, (uint32_t)halfMaxShield);
		*lowerSegments = (uint16_t)MATH2_longfraction(9u, percent);
		*upperSegments = 0;
	} else {
		uint16_t percent;

		*lowerSegments = 9;
		percent = (uint16_t)MATH2_percentage((uint32_t)(shield - halfMaxShield), (uint32_t)halfMaxShield);
		*upperSegments = (uint16_t)MATH2_longfraction(9u, percent);
	}
}

// FUNCTION: XWA 0x467980
void Hud_DrawShieldStrength2D(void) {
	int objectIndex;
	CraftData* craft;
	int halfMaxShield;
	uint16_t frontLowerSegments;
	uint16_t frontUpperSegments;
	uint16_t rearLowerSegments;
	uint16_t rearUpperSegments;
	uint16_t hullColorIndex;
	uint16_t spriteId;
	int hullGroup;

	if (!g_hudElementEnabled[1].enabled) {
		return;
	}

	objectIndex = g_players[g_localPlayer].objectIndex;
	if (objectIndex == 0xffff || g_objectTable[objectIndex].mobj == NULL ||
		g_objectTable[objectIndex].mobj->pCraft == NULL) {
		DebugPrintf("%s", "GetCraftPointer() returned NULL in HUD.c\n");
		DebugPrintf("%s", "NULL craft pointer in DrawShieldStrength2D()!\n");
		return;
	}

	craft = g_objectTable[objectIndex].mobj->pCraft;
	if ((craft->installedHudFeatureMask & 0x20u) == 0) {
		Hud_SetupResourceData(10000, 0x10ccu);
		Hud_DrawImageToDIB(0, 92);

		hullColorIndex = Hud_GetHullDamageColorIndex(craft);
		hullGroup = Hud_GetHullDamageSpriteGroup(hullColorIndex);
		spriteId = Hud_GetPlayerShieldSilhouetteSpriteId();
		if (spriteId != 0) {
			Hud_SetupResourceData(hullGroup, spriteId);
			Hud_DrawImageToDIB(28, 124);
		}
		return;
	}

	Hud_SetupResourceData(10000, 0x10ccu);
	Hud_DrawImageToDIB(0, 92);

	halfMaxShield = Craft_GetObjectMaxShield((uint16_t)objectIndex) / 2;
	craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
	if ((craft->activeHudFeatureMask & 0x20u) == 0) {
		return;
	}
	Hud_ComputeShieldBarSegments(craft->shieldFront, halfMaxShield, &frontLowerSegments, &frontUpperSegments);
	if ((craft->workingSubsystems & 1u) == 0) {
		frontLowerSegments = 0;
		frontUpperSegments = 0;
	}
	if (g_playerFlightTransientTimers[g_localPlayer].field_02 && !g_lastShieldDamageSide) {
		if (frontUpperSegments != 0) {
			frontUpperSegments = 10;
		} else {
			frontLowerSegments = 10;
		}
	}

	hullColorIndex = Hud_GetHullDamageColorIndex(craft);
	hullGroup = Hud_GetHullDamageSpriteGroup(hullColorIndex);
	spriteId = Hud_GetShieldHullIconSpriteId();
	if (spriteId != 0) {
		Hud_SetupResourceData(hullGroup, spriteId);
		Hud_DrawImageToDIB(28, 124);
	}

	if (frontLowerSegments != 0) {
		Hud_SetupResourceData(10000, (uint16_t)(16000u + frontLowerSegments));
		Hud_DrawImageToDIB(23, 115);
	}
	if (frontUpperSegments != 0) {
		Hud_SetupResourceData(10000, (uint16_t)(16100u + frontUpperSegments));
		Hud_DrawImageToDIB(21, 110);
	}

	craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
	Hud_ComputeShieldBarSegments(craft->shieldRear, halfMaxShield, &rearLowerSegments, &rearUpperSegments);
	if ((craft->workingSubsystems & 1u) == 0) {
		rearLowerSegments = 0;
		rearUpperSegments = 0;
	}
	if (g_playerFlightTransientTimers[g_localPlayer].field_02 && !g_lastShieldDamageSide) {
		if (rearUpperSegments != 0) {
			rearUpperSegments = 10;
		} else {
			rearLowerSegments = 10;
		}
	}

	if (rearLowerSegments != 0) {
		Hud_SetupResourceData(10000, (uint16_t)(16200u + rearLowerSegments));
		Hud_DrawImageToDIB(23, 139);
	}
	if (rearUpperSegments != 0) {
		Hud_SetupResourceData(10000, (uint16_t)(16300u + rearUpperSegments));
		Hud_DrawImageToDIB(21, 141);
	}
}

// FUNCTION: XWA 0x468480
void Hud_DrawBeamStrength2D(void) {
	enum {
		HUD_BEAM_FEATURE_MASK = 0x10,
		HUD_BEAM_SUBSYSTEM_MASK = 0x100,
		HUD_BEAM_GAUGE_SPRITE = 0x1130u,
		HUD_BEAM_ICON_ACTIVE_SPRITE = 13900u,
		HUD_BEAM_ICON_INACTIVE_SPRITE = 14000u,
		HUD_BEAM_FIRST_CHARGE_HIGH_SPRITE = 15003u
	};
	void(XWA_HUD_STDCALL * debugOutput)(const char*);
	CraftData* craft;
	Sprite* sprite;
	HudDrawTarget* target;
	uint8_t* payload;
	int objectIndex;
	int16_t beamPresent;
	int originalBeamPresent;
	uint8_t beamIsActive;
	int16_t segmentIndex;
	uint16_t spriteId;

	if (!g_hudElementEnabled[2].enabled) {
		return;
	}

	debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
	craft = Hud_GetCraftPointerInlinedWithDebug(debugOutput);
	if (craft == NULL) {
		debugOutput("NULL craft pointer in DrawBeamStrength2D()!\n");
		return;
	}

	if ((craft->installedHudFeatureMask & HUD_BEAM_FEATURE_MASK) == 0) {
		return;
	}

	objectIndex = g_players[g_localPlayer].objectIndex;
	craft = g_objectTable[objectIndex].mobj->pCraft;
	if ((craft->activeHudFeatureMask & HUD_BEAM_FEATURE_MASK) != 0) {

		beamPresent = (int16_t)craft->beamPresent;
		if (beamPresent < 0) {
			beamPresent = 0;
		}
		if ((craft->workingSubsystems & HUD_BEAM_SUBSYSTEM_MASK) == 0) {
			beamPresent = 0;
		}
		originalBeamPresent = (int16_t)beamPresent;

		beamIsActive = craft->beamActive != 0;
		if ((craft->workingSubsystems & HUD_BEAM_SUBSYSTEM_MASK) == 0) {
			beamIsActive = 0;
		}

		if (g_hudUseAlphaSpriteAtlas10100) {
			sprite = SpriteResource_ResolveSprite(g_hudAlphaSpriteGroupOffset + 10000, HUD_BEAM_GAUGE_SPRITE);
		} else {
			sprite = SpriteResource_ResolveSprite(10000, HUD_BEAM_GAUGE_SPRITE);
		}
		g_curImage = sprite;
		if (sprite == NULL) {
			debugOutput("Null pointer to image in SetupResourceData()\n");
		} else {
			g_curImageWidth = sprite->width;
			g_curImageHeight = sprite->height;
			g_curImageRLE = SpriteResource_GetRowData(sprite);
			sprite = g_curImage;
			payload = SpriteResource_GetMutableSpritePayload(sprite);
			g_curImagePalette = (uint16_t*)payload;
			g_curImagePalette = (uint16_t*)(payload + ((SpritePayload*)payload)->palette16Offset);
		}

		{
			int16_t drawX;
			int16_t drawY;

			drawX = (int16_t)(g_screenWidth - 75);
			drawY = (int16_t)(142 - ((uint16_t)g_curImageHeight >> 1));
			if (g_curImage == NULL) {
				debugOutput("Null image pointer in DrawImageToDIB()!\n");
			} else {
				target = g_drawTarget;
				if (drawY < target->clipY1 && drawY + (int16_t)g_curImage->height >= target->clipY0 &&
					drawX < target->clipX1 && drawX + (int16_t)g_curImage->width >= target->clipX0) {
					int spriteType;

					spriteType = g_curImage->type;
					if (spriteType != 7) {
						if (spriteType == 23) {
							Hud_BlitSpriteType23(drawX, drawY);
						}
					} else {
						Hud_BlitSpriteType7(drawX, drawY);
					}
				}
			}
		}

		spriteId = beamIsActive ? HUD_BEAM_ICON_ACTIVE_SPRITE : HUD_BEAM_ICON_INACTIVE_SPRITE;
		if (g_hudUseAlphaSpriteAtlas10100) {
			sprite = SpriteResource_ResolveSprite(g_hudAlphaSpriteGroupOffset + 10000, spriteId);
		} else {
			sprite = SpriteResource_ResolveSprite(10000, spriteId);
		}
		g_curImage = sprite;
		if (sprite == NULL) {
			XWA_HUD_OUTPUT_DEBUG_STRING("Null pointer to image in SetupResourceData()\n");
		} else {
			g_curImageWidth = sprite->width;
			g_curImageHeight = sprite->height;
			g_curImageRLE = SpriteResource_GetRowData(sprite);
			sprite = g_curImage;
			payload = SpriteResource_GetMutableSpritePayload(sprite);
			g_curImagePalette = (uint16_t*)payload;
			g_curImagePalette = (uint16_t*)(payload + ((SpritePayload*)payload)->palette16Offset);
		}

		{
			int16_t drawX;
			int16_t drawY;

			drawX = (int16_t)(g_screenWidth - 70);
			drawY = 107;
			if (g_curImage == NULL) {
				XWA_HUD_OUTPUT_DEBUG_STRING("Null image pointer in DrawImageToDIB()!\n");
			} else {
				target = g_drawTarget;
				if (drawY < target->clipY1 && drawY + (int16_t)g_curImage->height >= target->clipY0 &&
					drawX < target->clipX1 && drawX + (int16_t)g_curImage->width >= target->clipX0) {
					int spriteType;

					spriteType = g_curImage->type;
					if (spriteType != 7) {
						if (spriteType == 23) {
							Hud_BlitSpriteType23(drawX, drawY);
						}
					} else {
						Hud_BlitSpriteType7(drawX, drawY);
					}
				}
			}
		}

		if (beamPresent <= 0) {
			return;
		}

		for (segmentIndex = 0; segmentIndex < (int16_t)g_hudBeamChargeSegmentCount; ++segmentIndex) {
			int16_t level;
			int16_t drawY;

			level = beamPresent;
			spriteId = (uint16_t)(HUD_BEAM_FIRST_CHARGE_HIGH_SPRITE + segmentIndex * 10);
			if (originalBeamPresent <= (int16_t)g_hudBeamChargeUnitsPerSegment * (segmentIndex + 1)) {
				int16_t segmentUnits;

				segmentUnits =
					(int16_t)(beamPresent - (int16_t)g_hudBeamChargeUnitsPerSegment * segmentIndex);
				if (segmentUnits > (int16_t)g_hudBeamChargeUnitsPerSegment) {
					segmentUnits = (int16_t)g_hudBeamChargeUnitsPerSegment;
				}
				level = (int16_t)(segmentUnits / (int16_t)g_hudBeamChargeColorBandUnits);
				switch (level) {
					case 1:
						spriteId = (uint16_t)(HUD_BEAM_FIRST_CHARGE_HIGH_SPRITE + segmentIndex * 10 - 2);
						break;
					case 2:
						spriteId = (uint16_t)(HUD_BEAM_FIRST_CHARGE_HIGH_SPRITE + segmentIndex * 10 - 1);
						break;
					case 3:
						spriteId = (uint16_t)(HUD_BEAM_FIRST_CHARGE_HIGH_SPRITE + segmentIndex * 10);
						break;
					default:
						spriteId = (uint16_t)(HUD_BEAM_FIRST_CHARGE_HIGH_SPRITE + segmentIndex * 10 - 3);
						break;
				}
			}

			if (level <= 0) {
				continue;
			}

			if (g_hudUseAlphaSpriteAtlas10100) {
				sprite = SpriteResource_ResolveSprite(g_hudAlphaSpriteGroupOffset + 10000, spriteId);
			} else {
				sprite = SpriteResource_ResolveSprite(10000, spriteId);
			}
			g_curImage = sprite;
			if (sprite == NULL) {
				XWA_HUD_OUTPUT_DEBUG_STRING("Null pointer to image in SetupResourceData()\n");
			} else {
				g_curImageWidth = sprite->width;
				g_curImageHeight = sprite->height;
				g_curImageRLE = SpriteResource_GetRowData(sprite);
				sprite = g_curImage;
				payload = SpriteResource_GetMutableSpritePayload(sprite);
				g_curImagePalette = (uint16_t*)payload;
				g_curImagePalette = (uint16_t*)(payload + ((SpritePayload*)payload)->palette16Offset);
			}

			switch (segmentIndex) {
				case 0:
					drawY = 159;
					break;
				case 1:
					drawY = 156;
					break;
				case 2:
					drawY = 152;
					break;
				case 3:
					drawY = 148;
					break;
				case 4:
					drawY = 143;
					break;
				case 5:
					drawY = 137;
					break;
				case 6:
					drawY = 130;
					break;
				case 7:
					drawY = 122;
					break;
				case 8:
				default:
					drawY = 112;
					break;
			}

			{
				int16_t drawX;

				drawX = (int16_t)(g_screenWidth - ((uint16_t)g_curImageWidth >> 1) - 43);
				if (g_curImage == NULL) {
					XWA_HUD_OUTPUT_DEBUG_STRING("Null image pointer in DrawImageToDIB()!\n");
				} else {
					target = g_drawTarget;
					if (drawY < target->clipY1 && drawY + (int16_t)g_curImage->height >= target->clipY0 &&
						drawX < target->clipX1 && drawX + (int16_t)g_curImage->width >= target->clipX0) {
						int spriteType;

						spriteType = g_curImage->type;
						if (spriteType != 7) {
							if (spriteType == 23) {
								Hud_BlitSpriteType23(drawX, drawY);
							}
						} else {
							Hud_BlitSpriteType7(drawX, drawY);
						}
					}
				}
			}
		}
		return;
	}

	if (g_hudUseAlphaSpriteAtlas10100) {
		sprite = SpriteResource_ResolveSprite(g_hudAlphaSpriteGroupOffset + 10000, HUD_BEAM_GAUGE_SPRITE);
	} else {
		sprite = SpriteResource_ResolveSprite(10000, HUD_BEAM_GAUGE_SPRITE);
	}
	g_curImage = sprite;
	if (sprite == NULL) {
		debugOutput("Null pointer to image in SetupResourceData()\n");
	} else {
		g_curImageWidth = sprite->width;
		g_curImageHeight = sprite->height;
		g_curImageRLE = SpriteResource_GetRowData(sprite);
		sprite = g_curImage;
		payload = SpriteResource_GetMutableSpritePayload(sprite);
		g_curImagePalette = (uint16_t*)payload;
		g_curImagePalette = (uint16_t*)(payload + ((SpritePayload*)payload)->palette16Offset);
	}

	{
		int16_t drawX;
		int16_t drawY;

		drawX = (int16_t)(g_screenWidth - 75);
		drawY = (int16_t)(142 - ((uint16_t)g_curImageHeight >> 1));
		if (g_curImage == NULL) {
			debugOutput("Null image pointer in DrawImageToDIB()!\n");
		} else {
			target = g_drawTarget;
			if (drawY < target->clipY1 && drawY + (int16_t)g_curImage->height >= target->clipY0 &&
				drawX < target->clipX1 && drawX + (int16_t)g_curImage->width >= target->clipX0) {
				int spriteType;

				spriteType = g_curImage->type;
				if (spriteType != 7) {
					if (spriteType == 23) {
						Hud_BlitSpriteType23(drawX, drawY);
					}
				} else {
					Hud_BlitSpriteType7(drawX, drawY);
				}
			}
		}
	}
}

static __inline void Hud_DrawCMDPanelImage2D(void(XWA_HUD_STDCALL* debugOutput)(const char*), int x, int y) {
	HudDrawTarget* target;
	int drawY;
	int clipY1;
	int clipY0;
	int imageWidth;
	int imageHeight;
	int spriteType;
	int drawX;

	if (g_curImage == NULL) {
		debugOutput("Null image pointer in DrawImageToDIB()!\n");
		return;
	}

	target = g_drawTarget;
	drawY = (int16_t)y;
	clipY1 = target->clipY1;
	if (drawY >= clipY1) {
		return;
	}

	imageHeight = (int16_t)g_curImage->height;
	clipY0 = target->clipY0;
	if (drawY + imageHeight < clipY0) {
		return;
	}

	drawX = (int16_t)x;
	if (drawX >= target->clipX1) {
		return;
	}

	imageWidth = (int16_t)g_curImage->width;
	if (drawX + imageWidth < target->clipX0) {
		return;
	}

	spriteType = g_curImage->type;
	if (spriteType != 7) {
		if (spriteType != 23) {
			return;
		}
		Hud_BlitSpriteType23((int16_t)x, y);
		return;
	}

	Hud_BlitSpriteType7((int16_t)x, y);
}

// FUNCTION: XWA 0x468AA0
void Hud_DrawCMD2D(void) {
	enum { HUD_CMD_MASK_SPRITE = 0x50dcu, HUD_CMD_PANEL_SPRITE = 0x044cu };

	void(XWA_HUD_STDCALL * debugOutput)(const char*);
	CraftData* craft;
	Sprite* sprite;

	if (!g_hudElementEnabled[0].enabled && !g_inHangarReady) {
		Hud_SetupResourceData(10000, HUD_CMD_MASK_SPRITE);
		Hud_DrawImageToDIB(g_hudCockpitMaskSprites[3].x, g_hudCockpitMaskSprites[3].y);
		return;
	}

	if (!g_players[g_localPlayer].mapCameraState) {
		debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
		craft = Hud_GetCraftPointerInlinedWithDebug(debugOutput);
		if (craft == NULL) {
			debugOutput("NULL craft data pointer in DrawCMD2D()!\n");
			return;
		}

		if ((craft->activeHudFeatureMask & 1u) != 0) {
			if (g_hudUseAlphaSpriteAtlas10100) {
				sprite =
					SpriteResource_ResolveSprite(g_hudAlphaSpriteGroupOffset + 10000, HUD_CMD_PANEL_SPRITE);
			} else {
				sprite = SpriteResource_ResolveSprite(10000, HUD_CMD_PANEL_SPRITE);
			}
			g_curImage = sprite;
		} else {
			Hud_SetupResourceData(10000, HUD_CMD_PANEL_SPRITE);
			Hud_DrawCMDPanelImage2D(debugOutput, g_hudCenterX - ((uint16_t)g_curImageWidth >> 1),
									g_screenHeight - g_curImageHeight);
			return;
		}
	} else {
		if (g_hudUseAlphaSpriteAtlas10100) {
			sprite = SpriteResource_ResolveSprite(g_hudAlphaSpriteGroupOffset + 10000, HUD_CMD_PANEL_SPRITE);
		} else {
			sprite = SpriteResource_ResolveSprite(10000, HUD_CMD_PANEL_SPRITE);
		}
		debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
		g_curImage = sprite;
	}

	if (sprite == NULL) {
		debugOutput("Null pointer to image in SetupResourceData()\n");
	} else {
		uint8_t* palette;

		g_curImageWidth = sprite->width;
		g_curImageHeight = sprite->height;
		g_curImageRLE = SpriteResource_GetRowData(sprite);
#ifdef XWA_MODERN
		palette = SpriteResource_GetMutableSpritePayload(sprite);
#else
		palette = sprite->pixels;
#endif
		g_curImagePalette = (uint16_t*)palette;
		g_curImagePalette = (uint16_t*)(palette + ((SpritePayload*)palette)->palette16Offset);
	}

	Hud_DrawCMDPanelImage2D(debugOutput, g_hudCenterX - ((uint16_t)g_curImageWidth >> 1),
							g_screenHeight - g_curImageHeight);
}

// FUNCTION: XWA 0x468E10
void Hud_DrawMfdFrames2D(void) {
	enum {
		HUD_MFD_LEFT_ACTIVE_SPRITE = 0x2710u,
		HUD_MFD_LEFT_INACTIVE_SPRITE = 0x2774u,
		HUD_MFD_LEFT_MASK_SPRITE = 0x5014u,
		HUD_MFD_RIGHT_ACTIVE_SPRITE = 0x27d8u,
		HUD_MFD_RIGHT_INACTIVE_SPRITE = 0x283cu,
		HUD_MFD_RIGHT_MASK_SPRITE = 0x5078u
	};
	Sprite* sprite;
	int spriteGroup;

	if (g_filmPlaybackMode) {
		return;
	}

	FlightSw_SetRenderTarget(NULL, 320, 200u, 0);

	if ((g_players[g_localPlayer].mfd.enabled[1] && g_hudElementEnabled[7].enabled) ||
		(g_inHangarReady && g_hudElementEnabled[7].enabled)) {
		int drawY;

		if (g_players[g_localPlayer].mfd.activeIndex == 1) {
			if (g_hudUseAlphaSpriteAtlas10100) {
				sprite = SpriteResource_ResolveSprite(g_hudAlphaSpriteGroupOffset + 10000,
													  HUD_MFD_LEFT_ACTIVE_SPRITE);
			} else {
				sprite = SpriteResource_ResolveSprite(10000, HUD_MFD_LEFT_ACTIVE_SPRITE);
			}
		} else {
			if (g_hudUseAlphaSpriteAtlas10100) {
				sprite = SpriteResource_ResolveSprite(g_hudAlphaSpriteGroupOffset + 10000,
													  HUD_MFD_LEFT_INACTIVE_SPRITE);
			} else {
				sprite = SpriteResource_ResolveSprite(10000, HUD_MFD_LEFT_INACTIVE_SPRITE);
			}
		}
		g_curImage = sprite;
		if (sprite == NULL) {
			OutputDebugStringA("Null pointer to image in SetupResourceData()\n");
		} else {
			uint8_t* payload;

			g_curImageWidth = sprite->width;
			g_curImageHeight = sprite->height;
			g_curImageRLE = SpriteResource_GetRowData(sprite);
			sprite = g_curImage;
			payload = SpriteResource_GetMutableSpritePayload(sprite);
			g_curImagePalette = (uint16_t*)payload;
			g_curImagePalette = (uint16_t*)(payload + ((SpritePayload*)payload)->palette16Offset);
		}

		drawY = g_screenHeight - g_curImageHeight;
		if (g_curImage == NULL) {
			OutputDebugStringA("Null image pointer in DrawImageToDIB()!\n");
		} else {
			HudDrawTarget* target;

			target = g_drawTarget;
			if ((int16_t)drawY < target->clipY1 &&
				(int16_t)(drawY + (int16_t)g_curImage->height) >= target->clipY0 && target->clipX1 > 0 &&
				(int16_t)g_curImage->width >= target->clipX0) {
				int spriteType;

				spriteType = g_curImage->type;
				if (spriteType != 7) {
					if (spriteType == 23) {
						Hud_BlitSpriteType23(0, drawY);
					}
				} else {
					Hud_BlitSpriteType7(0, drawY);
				}
			}
		}
	} else if (!g_inHangarReady && g_hudElementEnabled[7].enabled) {
		if (g_hudUseAlphaSpriteAtlas10100) {
			sprite =
				SpriteResource_ResolveSprite(g_hudAlphaSpriteGroupOffset + 10000, HUD_MFD_LEFT_MASK_SPRITE);
		} else {
			sprite = SpriteResource_ResolveSprite(10000, HUD_MFD_LEFT_MASK_SPRITE);
		}
		g_curImage = sprite;
		if (sprite == NULL) {
			OutputDebugStringA("Null pointer to image in SetupResourceData()\n");
		} else {
			uint8_t* payload;

			g_curImageWidth = sprite->width;
			g_curImageHeight = sprite->height;
			g_curImageRLE = SpriteResource_GetRowData(sprite);
			sprite = g_curImage;
			payload = SpriteResource_GetMutableSpritePayload(sprite);
			g_curImagePalette = (uint16_t*)payload;
			g_curImagePalette = (uint16_t*)(payload + ((SpritePayload*)payload)->palette16Offset);
		}

		if (g_curImage == NULL) {
			OutputDebugStringA("Null image pointer in DrawImageToDIB()!\n");
		} else {
			HudDrawTarget* target;

			target = g_drawTarget;
			if (g_hudCockpitMaskSprites[4].y < target->clipY1 &&
				g_hudCockpitMaskSprites[4].y + (int16_t)g_curImage->height >= target->clipY0 &&
				g_hudCockpitMaskSprites[4].x < target->clipX1 &&
				g_hudCockpitMaskSprites[4].x + (int16_t)g_curImage->width >= target->clipX0) {
				int spriteType;

				spriteType = g_curImage->type;
				if (spriteType != 7) {
					if (spriteType == 23) {
						Hud_BlitSpriteType23(g_hudCockpitMaskSprites[4].x, g_hudCockpitMaskSprites[4].y);
					}
				} else {
					Hud_BlitSpriteType7(g_hudCockpitMaskSprites[4].x, g_hudCockpitMaskSprites[4].y);
				}
			}
		}
	}

	if ((g_players[g_localPlayer].mfd.enabled[2] && g_hudElementEnabled[8].enabled) ||
		(g_inHangarReady && g_hudElementEnabled[8].enabled)) {
		uint16_t spriteId;

		if (g_players[g_localPlayer].mfd.activeIndex == 2) {
			spriteId = HUD_MFD_RIGHT_ACTIVE_SPRITE;
			if (g_hudUseAlphaSpriteAtlas10100) {
				sprite = SpriteResource_ResolveSprite(g_hudAlphaSpriteGroupOffset + 10000, spriteId);
			} else {
				sprite = SpriteResource_ResolveSprite(10000, spriteId);
			}
			g_curImage = sprite;
			if (sprite == NULL) {
				OutputDebugStringA("Null pointer to image in SetupResourceData()\n");
			} else {
				uint8_t* payload;

				g_curImageWidth = sprite->width;
				g_curImageHeight = sprite->height;
				g_curImageRLE = SpriteResource_GetRowData(sprite);
				sprite = g_curImage;
				payload = SpriteResource_GetMutableSpritePayload(sprite);
				g_curImagePalette = (uint16_t*)payload;
				g_curImagePalette = (uint16_t*)(payload + ((SpritePayload*)payload)->palette16Offset);
			}

			Hud_DrawImageToDIB(g_screenWidth - g_curImageWidth, g_screenHeight - g_curImageHeight);
		} else {
			Hud_SetupResourceData(10000, HUD_MFD_RIGHT_INACTIVE_SPRITE);
			Hud_DrawImageToDIB(g_screenWidth - g_curImageWidth, g_screenHeight - g_curImageHeight);
		}
	} else if (!g_inHangarReady && g_hudElementEnabled[8].enabled) {
		Hud_SetupResourceData(10000, HUD_MFD_RIGHT_MASK_SPRITE);
		Hud_DrawImageToDIB(g_hudCockpitMaskSprites[5].x, g_hudCockpitMaskSprites[5].y);
	}

	if (g_hudElementEnabled[7].enabled) {
		g_hudElementEnabled[7].enabled = 0;
	}
	if (g_hudElementEnabled[8].enabled) {
		g_hudElementEnabled[8].enabled = 0;
	}
}

static void Hud_ClearMfdFrameElementFlags(void) {
	if (g_hudElementEnabled[7].enabled) {
		g_hudElementEnabled[7].enabled = 0;
	}
	if (g_hudElementEnabled[8].enabled) {
		g_hudElementEnabled[8].enabled = 0;
	}
}

// FUNCTION: XWA 0x4691D0
void Hud_DrawFilmMfdFrames2D(void) {
	enum {
		HUD_FILM_MFD_LEFT_FRAME_SPRITE = 0x2774u,
		HUD_FILM_MFD_LEFT_MASK_SPRITE = 0x5014u,
		HUD_FILM_MFD_RIGHT_FRAME_SPRITE = 0x283cu,
		HUD_FILM_MFD_RIGHT_MASK_SPRITE = 0x5078u
	};
	PlayerData* player;
	uint8_t filmMfdVisible;

	if (!g_filmPlaybackMode) {
		return;
	}

	FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
	player = &g_players[g_localPlayer];
	filmMfdVisible = g_filmOverlayMfdVisible;

	if ((filmMfdVisible && g_hudElementEnabled[7].enabled) ||
		(player->mfd.enabled[1] && g_hudElementEnabled[7].enabled)) {
		if (g_inHangarReady) {
			if (g_hangarAutoCam && !filmMfdVisible) {
				return;
			}
		} else if ((!player->hudEnabled && !filmMfdVisible) ||
				   (!filmMfdVisible && ((player->mfd.enabled[1] && g_filmOverlayActive) ||
										(!filmMfdVisible && player->mfd.enabled[0])))) {
			return;
		}

		Hud_SetupResourceData(10000, HUD_FILM_MFD_LEFT_FRAME_SPRITE);
		Hud_DrawImageToDIB(0, g_screenHeight - (uint16_t)g_curImageHeight);
		filmMfdVisible = g_filmOverlayMfdVisible;
	} else if (!player->mfd.enabled[1] && g_hudElementEnabled[7].enabled) {
		Hud_SetupResourceData(10000, HUD_FILM_MFD_LEFT_MASK_SPRITE);
		Hud_DrawImageToDIB(g_hudCockpitMaskSprites[4].x, g_hudCockpitMaskSprites[4].y);
		filmMfdVisible = g_filmOverlayMfdVisible;
	}

	if (!((filmMfdVisible && g_hudElementEnabled[8].enabled) || player->mfd.enabled[2])) {
		if (g_hudElementEnabled[8].enabled) {
			Hud_SetupResourceData(10000, HUD_FILM_MFD_RIGHT_MASK_SPRITE);
			Hud_DrawImageToDIB(g_hudCockpitMaskSprites[5].x, g_hudCockpitMaskSprites[5].y);
		}
		Hud_ClearMfdFrameElementFlags();
		return;
	}

	if (!g_hudElementEnabled[8].enabled) {
		Hud_ClearMfdFrameElementFlags();
		return;
	}

	if (g_inHangarReady ||
		((filmMfdVisible || ((!player->mfd.enabled[2] || !g_filmOverlayActive) && !player->mfd.enabled[0])) &&
		 (player->hudEnabled || filmMfdVisible))) {
		Hud_SetupResourceData(10000, HUD_FILM_MFD_RIGHT_FRAME_SPRITE);
		Hud_DrawImageToDIB(g_screenWidth - g_curImageWidth, g_screenHeight - (uint16_t)g_curImageHeight);
		Hud_ClearMfdFrameElementFlags();
	}
}

// FUNCTION: XWA 0x4696F0
void Hud_DrawRadarFrames3D(void) {
	enum {
		HUD_RADAR_FRAME_MODEL_TYPE = OBJ_HudTextureGroup12000,
		HUD_RADAR_FRAME_BASE_SIZE = 256,
		HUD_RADAR_FRAME_LEFT_FRAME = 27,
		HUD_RADAR_FRAME_RIGHT_FRAME = 28,
		HUD_RADAR_FRAME_BOTTOM_LEFT_FRAME = 49,
		HUD_RADAR_FRAME_BOTTOM_RIGHT_FRAME = 50
	};

	FlightTexQuad quad;

	quad.screenX = 0;
	quad.screenY = 0;
	quad.depthZ = 1;
	quad.rotationAngle = 0;
	quad.screenSize = HUD_RADAR_FRAME_BASE_SIZE;

	if (g_hudElementEnabled[3].enabled) {
		quad.screenSize = (uint16_t)(int)(g_flightHudScaleFactor * 256.0f);

		quad.screenX = g_hudCenterX - (uint16_t)g_hudRadarFrameLeftOffsetX;
		quad.screenY = g_screenHeight - (uint16_t)g_hudRadarFrameOffsetY;
		FeDiskIo_SelectTextureFrame(HUD_RADAR_FRAME_MODEL_TYPE, HUD_RADAR_FRAME_LEFT_FRAME,
									HUD_RADAR_FRAME_BASE_SIZE);
		RenderQuad_DrawModelTexture(HUD_RADAR_FRAME_MODEL_TYPE, &quad, g_hudColors[0]);

		quad.screenX = g_hudCenterX + (uint16_t)g_hudRadarFrameRightOffsetX;
		quad.screenY = g_screenHeight - (uint16_t)g_hudRadarFrameOffsetY;
		FeDiskIo_SelectTextureFrame(HUD_RADAR_FRAME_MODEL_TYPE, HUD_RADAR_FRAME_RIGHT_FRAME,
									HUD_RADAR_FRAME_BASE_SIZE);
		RenderQuad_DrawModelTexture(HUD_RADAR_FRAME_MODEL_TYPE, &quad, g_hudColors[0]);

		if (!g_provingGroundsModeActive) {
			quad.screenX = g_hudCenterX - 48;
			quad.screenY = g_screenHeight - 16;
			FeDiskIo_SelectTextureFrame(HUD_RADAR_FRAME_MODEL_TYPE, HUD_RADAR_FRAME_BOTTOM_LEFT_FRAME,
										HUD_RADAR_FRAME_BASE_SIZE);
			RenderQuad_DrawModelTexture(HUD_RADAR_FRAME_MODEL_TYPE, &quad, -1);

			quad.screenX = g_hudCenterX + 30;
			quad.screenY = g_screenHeight - 16;
			FeDiskIo_SelectTextureFrame(HUD_RADAR_FRAME_MODEL_TYPE, HUD_RADAR_FRAME_BOTTOM_RIGHT_FRAME,
										HUD_RADAR_FRAME_BASE_SIZE);
			RenderQuad_DrawModelTexture(HUD_RADAR_FRAME_MODEL_TYPE, &quad, -1);
		}
	}
}

// FUNCTION: XWA 0x469890
void Hud_DrawRadars3D(void) {
	enum {
		HUD_RADAR_SCOPE_MODEL_TYPE = OBJ_HudTextureGroup12000,
		HUD_RADAR_SCOPE_BASE_SIZE = 256,
		HUD_RADAR_SCOPE_FRONT_FRAME = 45,
		HUD_RADAR_SCOPE_REAR_FRAME = 46,
		HUD_RADAR_SCOPE_DEAD_FRAME = 4
	};

	FlightTexQuad quad;
	int baseSize;
	CraftData* craft;

	baseSize = HUD_RADAR_SCOPE_BASE_SIZE;

	quad.screenX = 0;
	quad.screenY = 0;
	quad.depthZ = 1;
	quad.rotationAngle = 0;
	quad.screenSize = (uint16_t)baseSize;

	craft = Hud_GetCraftPointerInlined();
	if (craft == NULL) {
		OutputDebugStringA("NULL craft data pointer in DrawRadars3D()!\n");
		return;
	}

	quad.screenSize = (uint16_t)(int)((double)quad.screenSize * (double)g_flightHudScaleFactor);

	if (g_hudElementEnabled[1].enabled) {
		quad.screenX = (uint16_t)g_hudRadarScopeOffsetX;
		quad.screenY = g_screenHeight - (uint16_t)g_hudRadarScopeOffsetY;
		if ((craft->activeHudFeatureMask & 0x80u) != 0) {
			FeDiskIo_SelectTextureFrame(HUD_RADAR_SCOPE_MODEL_TYPE, HUD_RADAR_SCOPE_FRONT_FRAME, baseSize);
			RenderQuad_DrawModelTexture(HUD_RADAR_SCOPE_MODEL_TYPE, &quad, g_hudColors[0]);
		} else {
			FeDiskIo_SelectTextureFrame(HUD_RADAR_SCOPE_MODEL_TYPE, HUD_RADAR_SCOPE_FRONT_FRAME, baseSize);
			RenderQuad_DrawModelTexture(HUD_RADAR_SCOPE_MODEL_TYPE, &quad, g_hudColors[0]);
		}
	}

	if (g_hudElementEnabled[2].enabled) {
		quad.screenX = g_screenWidth - (uint16_t)g_hudRadarScopeOffsetX;
		quad.screenY = g_screenHeight - (uint16_t)g_hudRadarScopeOffsetY;

		if ((craft->activeHudFeatureMask & 0x100u) != 0) {
			if ((craft->systemFlags & 0x100u) != 0) {
				FeDiskIo_SelectTextureFrame(HUD_RADAR_SCOPE_MODEL_TYPE, HUD_RADAR_SCOPE_REAR_FRAME, baseSize);
			} else {
				FeDiskIo_SelectTextureFrame(HUD_RADAR_SCOPE_MODEL_TYPE, HUD_RADAR_SCOPE_DEAD_FRAME, baseSize);
			}
			RenderQuad_DrawModelTexture(HUD_RADAR_SCOPE_MODEL_TYPE, &quad, g_hudColors[0]);
			return;
		}

		if ((craft->systemFlags & 0x100u) != 0) {
			FeDiskIo_SelectTextureFrame(HUD_RADAR_SCOPE_MODEL_TYPE, HUD_RADAR_SCOPE_REAR_FRAME, baseSize);
		} else {
			FeDiskIo_SelectTextureFrame(HUD_RADAR_SCOPE_MODEL_TYPE, HUD_RADAR_SCOPE_DEAD_FRAME, baseSize);
		}

		RenderQuad_DrawModelTexture(HUD_RADAR_SCOPE_MODEL_TYPE, &quad, g_hudColors[0]);
	}
}

// FUNCTION: XWA 0x469A80
void Hud_DrawPowerSettings3D(void) {
	enum {
		HUD_POWER_MODEL_TYPE = OBJ_HudTextureGroup12000,
		HUD_POWER_BASE_SIZE = 256,
		HUD_POWER_PIP_FRAME = 0x0c,
		HUD_POWER_RESERVE_FRAME = 0x0d,
		HUD_POWER_BEAM_RESERVE_FRAME = 0x0e
	};
	const int HUD_POWER_LASER_GREEN_COLOR = (int)0xff00fc0fu;
	const int HUD_POWER_LASER_RED_COLOR = (int)0xffff3a06u;
	const int HUD_POWER_SHIELD_COLOR = (int)0xffffff00u;
	const int HUD_POWER_BEAM_COLOR = (int)0xffc600d7u;
	FlightTexQuad quad;
	void(XWA_HUD_STDCALL * debugOutput)(const char*);
	CraftData* craft;
	int screenHeight;
	int screenWidth;
	uint8_t hasBeamSystem;
	unsigned int reserveIdx;

	quad.screenX = 0;
	quad.screenY = 0;
	quad.depthZ = 1;
	quad.rotationAngle = 0;
	quad.screenSize = HUD_POWER_BASE_SIZE;
	hasBeamSystem = 0;

	debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
	craft = Hud_GetCraftPointerInlinedWithDebug(debugOutput);
	if (craft == NULL) {
		debugOutput("NULL craft data pointer in DrawPowerSettings3D()!\n");
		return;
	}

	quad.screenSize = (uint16_t)(int)((double)quad.screenSize * (double)g_flightHudScaleFactor);
	if (g_hudElementEnabled[1].enabled) {
		if ((craft->activeHudFeatureMask & 0x200u) != 0) {
			uint16_t laserPipCount;
			int color;

			laserPipCount = craft->laserRedirect;
			quad.screenX = (uint16_t)g_hudPowerLaserPipX;
			quad.screenY = g_screenHeight - (uint16_t)g_hudPowerLaserPipTopY;
			color = craft->laserProjectileTypeId[0] != OBJ_LaserRebel ? HUD_POWER_LASER_GREEN_COLOR
																	  : HUD_POWER_LASER_RED_COLOR;
			while (laserPipCount > 0) {
				FeDiskIo_SelectTextureFrame(HUD_POWER_MODEL_TYPE, HUD_POWER_PIP_FRAME, HUD_POWER_BASE_SIZE);
				RenderQuad_DrawModelTexture(HUD_POWER_MODEL_TYPE, &quad, color);
				--laserPipCount;
				quad.screenY += (uint16_t)g_hudPowerPipSpacingY;
			}
		}
		screenHeight = g_screenHeight;

		if ((craft->systemFlags & 0x1u) != 0 && (craft->activeHudFeatureMask & 0x800u) != 0) {
			uint16_t shieldPipCount;

			shieldPipCount = craft->shieldRedirect;
			quad.screenX = (uint16_t)g_hudPowerShieldPipX;
			quad.screenY = screenHeight - (uint16_t)g_hudPowerShieldPipTopY;
			while (shieldPipCount > 0) {
				FeDiskIo_SelectTextureFrame(HUD_POWER_MODEL_TYPE, HUD_POWER_PIP_FRAME, HUD_POWER_BASE_SIZE);
				RenderQuad_DrawModelTexture(HUD_POWER_MODEL_TYPE, &quad, HUD_POWER_SHIELD_COLOR);
				--shieldPipCount;
				quad.screenY += (uint16_t)g_hudPowerPipSpacingY;
			}
		}
	}
	screenHeight = g_screenHeight;

	if (!g_hudElementEnabled[2].enabled) {
		return;
	}

	if ((craft->systemFlags & 0x100u) != 0) {
		hasBeamSystem = 1;
		if ((craft->activeHudFeatureMask & 0x1000u) != 0) {
			uint16_t beamPipCount;
			int beamPipRightOffset;

			screenWidth = g_screenWidth;
			beamPipRightOffset = (uint16_t)g_hudPowerBeamPipRightOffsetX;
			beamPipCount = craft->beamLevel;
			quad.screenX = screenWidth;
			quad.screenX -= beamPipRightOffset;
			quad.screenY = screenHeight - (uint16_t)g_hudPowerBeamPipTopY;
			while (beamPipCount > 0) {
				FeDiskIo_SelectTextureFrame(HUD_POWER_MODEL_TYPE, HUD_POWER_PIP_FRAME, HUD_POWER_BASE_SIZE);
				RenderQuad_DrawModelTexture(HUD_POWER_MODEL_TYPE, &quad, HUD_POWER_BEAM_COLOR);
				--beamPipCount;
				quad.screenY += (uint16_t)g_hudPowerPipSpacingY;
			}
		}
	}
	screenHeight = g_screenHeight;

	screenWidth = g_screenWidth;
	if ((craft->activeHudFeatureMask & 0x400u) == 0) {
		return;
	}

	if (hasBeamSystem) {
		uint16_t reserveCount;

		reserveCount = (uint16_t)(8u - craft->laserRedirect);
		if ((craft->systemFlags & 0x1u) != 0) {
			reserveCount = (uint16_t)(reserveCount + 2u - craft->shieldRedirect);
		}
		if ((craft->systemFlags & 0x100u) != 0) {
			reserveCount = (uint16_t)(reserveCount + 2u - craft->beamLevel);
		}

		quad.screenX = screenWidth - (uint16_t)g_hudPowerReservePipRightOffsetX;
		for (reserveIdx = 0; reserveIdx < reserveCount; ++reserveIdx) {
			double reserveY;

			reserveY = (double)(g_screenHeight - (uint16_t)g_hudPowerBeamReserveTopY);
			reserveY += (double)reserveIdx * g_hudPowerBeamReserveStep;
			quad.screenY = (int)reserveY;
			FeDiskIo_SelectTextureFrame(HUD_POWER_MODEL_TYPE, HUD_POWER_BEAM_RESERVE_FRAME,
										HUD_POWER_BASE_SIZE);
			RenderQuad_DrawModelTexture(HUD_POWER_MODEL_TYPE, &quad, -1);
		}
	} else {
		uint16_t reserveCount;
		unsigned int drawCount;

		reserveCount = (uint16_t)(8u - craft->shieldRedirect - craft->laserRedirect);
		quad.screenX = screenWidth - (uint16_t)g_hudPowerReservePipRightOffsetX;
		quad.screenY = screenHeight - (uint16_t)g_hudPowerUnallocatedPipTopY;
		drawCount = reserveCount;
		while (drawCount > 0) {
			FeDiskIo_SelectTextureFrame(HUD_POWER_MODEL_TYPE, HUD_POWER_RESERVE_FRAME, HUD_POWER_BASE_SIZE);
			RenderQuad_DrawModelTexture(HUD_POWER_MODEL_TYPE, &quad, -1);
			--drawCount;
			quad.screenY += (uint16_t)g_hudPowerReservePipSpacingY;
		}
	}
}

static __inline int Hud_U16Layout(int value) { return value & 0xffff; }

static __inline void Hud_ResetChargeQuads(FlightTexQuad quads[16]) {
	int i;

	for (i = 0; i < 16; ++i) {
		memset(&quads[i], 0, sizeof(quads[i]));
		quads[i].depthZ = 1;
		quads[i].screenSize = 256;
	}
}

static __inline void Hud_SetChargeQuad(FlightTexQuad quads[16], int slot, int x, int y) {
	quads[slot].screenX = x;
	quads[slot].screenY = y;
}

static __inline void Hud_SetIonChargeQuadFromHardpoint(int order[10], int laserCount, int orderIndex, int x,
													   int y) {
	Hud_SetChargeQuad(g_hudIonChargeQuads + 1, order[orderIndex] - laserCount, x, y);
}

// FUNCTION: XWA 0x469EA0
void Hud_SetupLaserChargePositions3D(void) {
	void(XWA_HUD_STDCALL * debugOutput)(const char*);
	CraftData* craft;
	int order[10];
	unsigned int laserCount;
	unsigned int ionCount;
	int ionYOffset;
	int centerX;
	int i;
	int pass;
	int scan;
	int tmp;

	laserCount = 0;
	ionCount = 0;
	debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
	craft = Hud_GetCraftPointerInlinedWithDebug(debugOutput);
	if (craft == NULL) {
		debugOutput("NULL craft data pointer in SetupLaserChargePositions3D()!\n");
		return;
	}

	for (i = 0; i < 10; ++i) {
		order[i] = i;
	}

	Hud_ResetChargeQuads(g_hudLaserChargeQuads);
	Hud_ResetChargeQuads(g_hudIonChargeQuads);

	if ((uint16_t)g_reticleLaserHardpointCount != 0) {
		i = (uint16_t)g_reticleLaserHardpointCount;
		scan = 0;
		do {
			if (craft->warheadData[scan].weaponType == 1) {
				++laserCount;
			} else if (craft->warheadData[scan].weaponType == 2) {
				++ionCount;
			}
			++scan;
			--i;
		} while (i != 0);
	}

	if (laserCount > 6 || ionCount > 4) {
		debugOutput("Craft has more than MAX hardpoints in SetupLaserChargePositions3D()!\n");
		return;
	}

	ionYOffset = 0;
	centerX = g_hudCenterX;
	if (laserCount != 0) {
		if (laserCount == 1) {
			Hud_SetChargeQuad(g_hudLaserChargeQuads, 1, centerX, Hud_U16Layout(g_hudLaserChargeSingleY));
		} else if (laserCount == 2) {
			int leftX = centerX - Hud_U16Layout(g_hudLaserChargePairLeftOffsetX);
			int rightX = centerX + Hud_U16Layout(g_hudLaserChargePairRightOffsetX);
			int y = Hud_U16Layout(g_hudLaserChargePairY);

			if (g_reticleLaserAimPoints[0].x >= g_reticleLaserAimPoints[1].x) {
				Hud_SetChargeQuad(g_hudLaserChargeQuads, 1, rightX, y);
				Hud_SetChargeQuad(g_hudLaserChargeQuads, 2, leftX, y);
			} else {
				Hud_SetChargeQuad(g_hudLaserChargeQuads, 1, leftX, y);
				Hud_SetChargeQuad(g_hudLaserChargeQuads, 2, rightX, y);
			}
		} else if (laserCount == 3) {
			int leftX = centerX - Hud_U16Layout(g_hudLaserChargeTripleLeftOffsetX);
			int rightInnerX = centerX + Hud_U16Layout(g_hudLaserChargeTripleRightInnerOffsetX);
			int rightOuterX = centerX + Hud_U16Layout(g_hudLaserChargeTripleRightOuterOffsetX);
			int y = Hud_U16Layout(g_hudLaserChargeTripleY);

			for (pass = 2; pass >= 0; --pass) {
				if (pass > 0) {
					for (scan = 0; scan < pass; ++scan) {
						if (g_reticleLaserAimPoints[order[scan]].x >
							g_reticleLaserAimPoints[order[scan + 1]].x) {
							tmp = order[scan];
							order[scan] = order[scan + 1];
							order[scan + 1] = tmp;
						}
					}
				}
			}
			g_hudLaserChargeQuads[order[0] + 1].screenX = leftX;
			g_hudLaserChargeQuads[order[0] + 1].screenY = y;
			g_hudLaserChargeQuads[order[1] + 1].screenX = rightInnerX;
			g_hudLaserChargeQuads[order[1] + 1].screenY = y;
			g_hudLaserChargeQuads[order[2] + 1].screenX = rightOuterX;
			g_hudLaserChargeQuads[order[2] + 1].screenY = y;
		} else if (laserCount == 4) {
			int leftX = centerX - Hud_U16Layout(g_hudLaserChargeQuadLeftOffsetX);
			int rightX = centerX + Hud_U16Layout(g_hudLaserChargeQuadRightOffsetX);
			int upperY = Hud_U16Layout(g_hudLaserChargeQuadUpperY);
			int lowerY = Hud_U16Layout(g_hudLaserChargeQuadLowerY);

			for (pass = 3; pass >= 0; --pass) {
				for (scan = 0; scan < pass; ++scan) {
					if (g_reticleLaserAimPoints[order[scan]].y > g_reticleLaserAimPoints[order[scan + 1]].y) {
						tmp = order[scan];
						order[scan] = order[scan + 1];
						order[scan + 1] = tmp;
					}
				}
			}
			scan = 0;
			pass = 2;
			do {
				if (g_reticleLaserAimPoints[order[scan]].x > g_reticleLaserAimPoints[order[scan + 1]].x) {
					tmp = order[scan];
					order[scan] = order[scan + 1];
					order[scan + 1] = tmp;
				}
				--pass;
				scan = 2;
			} while (pass != 0);
			Hud_SetChargeQuad(g_hudLaserChargeQuads + 1, order[0], leftX, upperY);
			Hud_SetChargeQuad(g_hudLaserChargeQuads + 1, order[1], rightX, upperY);
			Hud_SetChargeQuad(g_hudLaserChargeQuads + 1, order[2], leftX, lowerY);
			Hud_SetChargeQuad(g_hudLaserChargeQuads + 1, order[3], rightX, lowerY);
			ionYOffset = Hud_U16Layout(g_hudIonChargeYOffsetFromLaser);
		} else if (laserCount == 5) {
			int leftX = centerX - Hud_U16Layout(g_hudLaserChargeFiveLeftOffsetX);
			int rightX = centerX + Hud_U16Layout(g_hudLaserChargeFiveRightOffsetX);
			int upperY = Hud_U16Layout(g_hudLaserChargeFiveUpperY);
			int lowerY = Hud_U16Layout(g_hudLaserChargeFiveLowerY);

			for (pass = 4; pass >= 0; --pass) {
				for (scan = 0; scan < pass; ++scan) {
					if (g_reticleLaserAimPoints[order[scan]].y > g_reticleLaserAimPoints[order[scan + 1]].y) {
						tmp = order[scan];
						order[scan] = order[scan + 1];
						order[scan + 1] = tmp;
					}
				}
			}
			scan = 0;
			pass = 2;
			do {
				if (g_reticleLaserAimPoints[order[scan]].x > g_reticleLaserAimPoints[order[scan + 1]].x) {
					tmp = order[scan];
					order[scan] = order[scan + 1];
					order[scan + 1] = tmp;
				}
				--pass;
				scan = 2;
			} while (pass != 0);
			Hud_SetChargeQuad(g_hudLaserChargeQuads + 1, order[0], leftX, upperY);
			Hud_SetChargeQuad(g_hudLaserChargeQuads + 1, order[1], rightX, upperY);
			Hud_SetChargeQuad(g_hudLaserChargeQuads + 1, order[2], leftX, lowerY);
			Hud_SetChargeQuad(g_hudLaserChargeQuads + 1, order[3], rightX, lowerY);
			Hud_SetChargeQuad(g_hudLaserChargeQuads + 1, order[4], rightX, lowerY);
			ionYOffset = Hud_U16Layout(g_hudIonChargeYOffsetFromLaser);
		} else {
			int leftX = centerX - Hud_U16Layout(g_hudLaserChargeSixLeftOffsetX);
			int rightInnerX = centerX + Hud_U16Layout(g_hudLaserChargeSixRightInnerOffsetX);
			int rightOuterX = centerX + Hud_U16Layout(g_hudLaserChargeSixRightOuterOffsetX);
			int upperY = Hud_U16Layout(g_hudLaserChargeSixUpperY);
			int lowerY = Hud_U16Layout(g_hudLaserChargeSixLowerY);

			for (pass = laserCount - 1; pass >= 0; --pass) {
				for (scan = 0; scan < pass; ++scan) {
					if (g_reticleLaserAimPoints[order[scan]].y > g_reticleLaserAimPoints[order[scan + 1]].y) {
						tmp = order[scan];
						order[scan] = order[scan + 1];
						order[scan + 1] = tmp;
					}
				}
			}
			scan = 0;
			pass = 3;
			do {
				if (g_reticleLaserAimPoints[order[scan]].x > g_reticleLaserAimPoints[order[scan + 1]].x) {
					tmp = order[scan];
					order[scan] = order[scan + 1];
					order[scan + 1] = tmp;
				}
				--pass;
				scan = 3;
			} while (pass != 0);
			Hud_SetChargeQuad(g_hudLaserChargeQuads + 1, order[0], leftX, upperY);
			Hud_SetChargeQuad(g_hudLaserChargeQuads + 1, order[1], rightInnerX, upperY);
			Hud_SetChargeQuad(g_hudLaserChargeQuads + 1, order[2], rightOuterX, upperY);
			Hud_SetChargeQuad(g_hudLaserChargeQuads + 1, order[3], leftX, lowerY);
			Hud_SetChargeQuad(g_hudLaserChargeQuads + 1, order[4], rightInnerX, lowerY);
			Hud_SetChargeQuad(g_hudLaserChargeQuads + 1, order[5], rightOuterX, lowerY);
			ionYOffset = Hud_U16Layout(g_hudIonChargeYOffsetFromLaser);
		}
	}

	if (ionCount == 0) {
		return;
	}

	if (ionCount == 1) {
		Hud_SetChargeQuad(g_hudIonChargeQuads, 1, centerX, Hud_U16Layout(g_hudIonChargeSingleY) + ionYOffset);
	} else if (ionCount == 2) {
		int leftX = centerX - Hud_U16Layout(g_hudIonChargePairLeftOffsetX);
		int rightX = centerX + Hud_U16Layout(g_hudIonChargePairRightOffsetX);
		int y = Hud_U16Layout(g_hudIonChargePairY) + ionYOffset;

		if (g_reticleLaserAimPoints[laserCount].x >= g_reticleLaserAimPoints[laserCount + 1].x) {
			Hud_SetChargeQuad(g_hudIonChargeQuads, 1, rightX, y);
			Hud_SetChargeQuad(g_hudIonChargeQuads, 2, leftX, y);
		} else {
			Hud_SetChargeQuad(g_hudIonChargeQuads, 1, leftX, y);
			Hud_SetChargeQuad(g_hudIonChargeQuads, 2, rightX, y);
		}
	} else if (ionCount == 3) {
		int leftX = centerX - Hud_U16Layout(g_hudIonChargeTripleLeftOffsetX);
		int rightInnerX = centerX + Hud_U16Layout(g_hudIonChargeTripleRightInnerOffsetX);
		int rightOuterX = centerX + Hud_U16Layout(g_hudIonChargeTripleRightOuterOffsetX);
		int y = Hud_U16Layout(g_hudIonChargeTripleY) + ionYOffset;

		for (scan = laserCount; scan < (uint16_t)g_reticleLaserHardpointCount - 1; ++scan) {
			for (pass = scan + 1; pass < (uint16_t)g_reticleLaserHardpointCount; ++pass) {
				if (g_reticleLaserAimPoints[order[pass]].x < g_reticleLaserAimPoints[order[scan]].x) {
					tmp = order[pass];
					order[pass] = order[scan];
					order[scan] = tmp;
				}
			}
		}
		Hud_SetIonChargeQuadFromHardpoint(order, laserCount, laserCount, leftX, y);
		Hud_SetIonChargeQuadFromHardpoint(order, laserCount, laserCount + 1, rightInnerX, y);
		Hud_SetIonChargeQuadFromHardpoint(order, laserCount, laserCount + 2, rightOuterX, y);
	} else {
		int leftX = centerX - Hud_U16Layout(g_hudIonChargeQuadLeftOffsetX);
		int rightX = centerX + Hud_U16Layout(g_hudIonChargeQuadRightOffsetX);
		int upperY = Hud_U16Layout(g_hudIonChargeQuadUpperY) + ionYOffset;
		int lowerY = Hud_U16Layout(g_hudIonChargeQuadLowerY) + ionYOffset;
		int end = (uint16_t)g_reticleLaserHardpointCount - 1;
		int total = (uint16_t)g_reticleLaserHardpointCount;
		int start = total - ionCount;
		pass = end;
		while (start <= pass) {
			if (start < pass) {
				for (scan = start; scan < pass; ++scan) {
					if (g_reticleLaserAimPoints[order[scan]].y > g_reticleLaserAimPoints[order[scan + 1]].y) {
						tmp = order[scan];
						order[scan] = order[scan + 1];
						order[scan + 1] = tmp;
					}
				}
			}
			--pass;
		}
		scan = start;
		pass = 2;
		do {
			if (g_reticleLaserAimPoints[order[scan]].x > g_reticleLaserAimPoints[order[scan + 1]].x) {
				tmp = order[scan];
				order[scan] = order[scan + 1];
				order[scan + 1] = tmp;
			}
			scan += 2;
			--pass;
		} while (pass != 0);
		/*
		 * The original four-ion path indexes the scratch order array as
		 * order[total - 3]..order[total], then subtracts ionCount for the
		 * destination slot. Keep that stack-indexing quirk instead of using
		 * the cleaner laserCount-relative mapping used by the three-ion case.
		 */
		Hud_SetChargeQuad(g_hudIonChargeQuads, order[total - 3] - ionCount, leftX, upperY);
		Hud_SetChargeQuad(g_hudIonChargeQuads, order[total - 2] - ionCount, rightX, upperY);
		Hud_SetChargeQuad(g_hudIonChargeQuads, order[total - 1] - ionCount, leftX, lowerY);
		Hud_SetChargeQuad(g_hudIonChargeQuads, order[total] - ionCount, rightX, lowerY);
	}
}

// FUNCTION: XWA 0x46A7F0
void Hud_DrawLaserCharge3D(void) {
	enum {
		HUD_LASER_CHARGE_MODEL_TYPE = OBJ_HudTextureGroup12000,
		HUD_LASER_CHARGE_BASE_SIZE = 256,
		HUD_LASER_CHARGE_DUAL_FRAME_BASE = 2300,
		HUD_LASER_CHARGE_TRIPLE_FRAME_BASE = 2500,
		HUD_LASER_CHARGE_FRAME_DIVISOR = 100
	};
	FlightTexQuad quad;
	CraftData* craft;
	void(XWA_HUD_STDCALL * debugOutput)(const char*);
	unsigned int laserCount;
	unsigned int ionCount;
	int weaponSlot;
	int i;

	laserCount = 0;
	quad.screenX = 0;
	quad.screenY = 0;
	quad.depthZ = 1;
	quad.rotationAngle = 0;
	quad.screenSize = HUD_LASER_CHARGE_BASE_SIZE;
	ionCount = 0;

	if (!g_hudElementEnabled[0].enabled) {
		return;
	}

	debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
	craft = Hud_GetCraftPointerInlinedWithDebug(debugOutput);
	if (craft == NULL) {
		debugOutput("NULL craft data pointer in DrawLaserCharge3D()!\n");
		return;
	}

	/* Scale the texture quad to the active flight HUD resolution. */
	quad.screenSize = (uint16_t)(int)((double)quad.screenSize * (double)g_flightHudScaleFactor);

	for (weaponSlot = 0; (uint16_t)weaponSlot < g_reticleLaserHardpointCount; ++weaponSlot) {
		uint8_t weaponType;

		weaponType = craft->warheadData[(uint16_t)weaponSlot].weaponType;
		if (weaponType == 1) {
			++laserCount;
		} else if (weaponType == 2) {
			++ionCount;
		}
	}

	if ((craft->installedHudFeatureMask & 2u) == 0 || (craft->installedHudFeatureMask & 4u) == 0) {
		return;
	}

	if (laserCount != 0) {
		int frameBase;

		if (laserCount == 3 || laserCount == 6) {
			frameBase = HUD_LASER_CHARGE_TRIPLE_FRAME_BASE;
		} else {
			frameBase = HUD_LASER_CHARGE_DUAL_FRAME_BASE;
		}

		for (i = 1; (unsigned int)i <= laserCount; ++i) {
			quad.screenX = g_hudLaserChargeQuads[i].screenX;
			quad.screenY = g_hudLaserChargeQuads[i].screenY;
			FeDiskIo_SelectTextureFrame(HUD_LASER_CHARGE_MODEL_TYPE,
										frameBase / HUD_LASER_CHARGE_FRAME_DIVISOR,
										HUD_LASER_CHARGE_BASE_SIZE);
			RenderQuad_DrawModelTexture(HUD_LASER_CHARGE_MODEL_TYPE, &quad, g_hudColors[0]);
		}

		Hud_DrawEnergyBar3D(g_hudEnergyBankLaserSelector, laserCount);
	}

	if (ionCount != 0) {
		int frameBase;

		if (ionCount == 3) {
			frameBase = HUD_LASER_CHARGE_TRIPLE_FRAME_BASE;
		} else {
			frameBase = HUD_LASER_CHARGE_DUAL_FRAME_BASE;
		}

		for (i = 1; (unsigned int)i <= ionCount; ++i) {
			quad.screenX = g_hudIonChargeQuads[i].screenX;
			quad.screenY = g_hudIonChargeQuads[i].screenY;
			FeDiskIo_SelectTextureFrame(HUD_LASER_CHARGE_MODEL_TYPE,
										frameBase / HUD_LASER_CHARGE_FRAME_DIVISOR,
										HUD_LASER_CHARGE_BASE_SIZE);
			RenderQuad_DrawModelTexture(HUD_LASER_CHARGE_MODEL_TYPE, &quad, g_hudColors[0]);
		}

		Hud_DrawEnergyBar3D(g_hudEnergyBankIonSelector, ionCount);
	}
}

// FUNCTION: XWA 0x46AA20
void Hud_DrawEnergyBar3D(int isIonBank, int bankWeaponCount) {
	enum {
		HUD_ENERGY_BAR_MODEL_TYPE = OBJ_HudTextureGroup12000,
		HUD_ENERGY_BAR_BASE_SIZE = 256,
		HUD_ENERGY_BAR_FRAME_DEFAULT = 0x18,
		HUD_ENERGY_BAR_FRAME_TRIPLE = 0x1a,
		HUD_ENERGY_BAR_OVERCHARGE_BASE = 64,
		HUD_ENERGY_BAR_SEGMENT_CHARGE = 10
	};
	void(XWA_HUD_STDCALL * debugOutput)(const char*);
	FlightTexQuad quad;
	FlightTexQuad chargeQuads[16];
	CraftData* craft;
	uint8_t* chargePtr;
	int* anchorY;
	uint16_t startWeapon;
	uint16_t endWeapon;
	int weaponCount;
	int chargeSegmentStepX;
	int firstSegmentBackstepX;
	int primaryColor;
	int secondaryColor;
	int i;

	quad.screenX = 0;
	quad.screenY = 0;
	quad.depthZ = 1;
	quad.rotationAngle = 0;
	quad.screenSize = HUD_ENERGY_BAR_BASE_SIZE;

	debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
	craft = Hud_GetCraftPointerInlinedWithDebug(debugOutput);
	if (craft == NULL) {
		debugOutput("NULL craft data pointer in DrawEnergyBar3D()!\n");
		return;
	}

	quad.screenSize = (uint16_t)(int)((double)quad.screenSize * (double)g_flightHudScaleFactor);
	{
		FlightTexQuad* initQuad;

		initQuad = chargeQuads;
		i = 16;
		do {
			memset(initQuad, 0, sizeof(*initQuad));
			initQuad->screenSize = HUD_ENERGY_BAR_BASE_SIZE;
			initQuad->depthZ = 1;
			++initQuad;
			--i;
		} while (i != 0);
	}

	if (isIonBank == g_hudEnergyBankLaserSelector) {
		memcpy(chargeQuads, g_hudLaserChargeQuads, sizeof(chargeQuads));
		endWeapon = (uint16_t)bankWeaponCount;
		startWeapon = 0;
		if (craft->laserProjectileTypeId[0] == OBJ_LaserRebel) {
			primaryColor = (int)0xffff0000u;
			secondaryColor = (int)0xff9b1e00u;
		} else {
			primaryColor = (int)0xff00fc0fu;
			secondaryColor = (int)0xff008c00u;
		}
	} else {
		memcpy(chargeQuads, g_hudIonChargeQuads, sizeof(chargeQuads));
		startWeapon = (uint16_t)(g_reticleLaserHardpointCount - bankWeaponCount);
		endWeapon = (uint16_t)g_reticleLaserHardpointCount;
		primaryColor = (int)0xff00b4ffu;
		secondaryColor = (int)0xff006992u;
	}

	if (bankWeaponCount == 3) {
		chargeSegmentStepX = (uint16_t)g_hudEnergyChargeTripleSegmentStepX;
		firstSegmentBackstepX = (uint16_t)g_hudEnergyChargeTripleInitialBackstepX;
	} else {
		chargeSegmentStepX = (uint16_t)g_hudEnergyChargeNonTripleSegmentStepX;
		firstSegmentBackstepX = (uint16_t)g_hudEnergyChargeNonTripleInitialBackstepX;
	}

	if (startWeapon >= endWeapon) {
		return;
	}

	anchorY = &chargeQuads[1].screenY;
	chargePtr = &craft->warheadData[startWeapon].laserCharge;
	weaponCount = endWeapon - startWeapon;
	do {
		uint32_t emptySegments;
		int16_t charge;

		emptySegments = 0;
		charge = (int8_t)*chargePtr;
		if ((craft->activeHudFeatureMask & 2u) != 0 && (craft->activeHudFeatureMask & 4u) != 0 &&
			charge > 0 && (craft->workingSubsystems & 0x10u) != 0) {
			uint16_t displayCharge;
			int color;
			uint32_t filledSegments;
			uint32_t segment;

			++charge;
			displayCharge = (uint16_t)charge;
			if (charge <= HUD_ENERGY_BAR_OVERCHARGE_BASE) {
				color = secondaryColor;
			} else {
				color = primaryColor;
				displayCharge = (uint16_t)(charge - HUD_ENERGY_BAR_OVERCHARGE_BASE);
			}

			filledSegments = displayCharge / HUD_ENERGY_BAR_SEGMENT_CHARGE;
			if (filledSegments == 0 &&
				(charge < HUD_ENERGY_BAR_SEGMENT_CHARGE || charge > HUD_ENERGY_BAR_OVERCHARGE_BASE)) {
				filledSegments = 1;
			}
			if (filledSegments > g_hudEnergyBarMaxSegments) {
				filledSegments = g_hudEnergyBarMaxSegments;
			}
			if (charge > HUD_ENERGY_BAR_OVERCHARGE_BASE) {
				emptySegments = g_hudEnergyBarMaxSegments - filledSegments;
			}

			for (segment = 1; segment <= filledSegments; ++segment) {
				if (segment == 1) {
					quad.screenX = anchorY[-1] - firstSegmentBackstepX;
					quad.screenY = *anchorY;
				} else {
					quad.screenX += chargeSegmentStepX;
				}
				if (bankWeaponCount == 3) {
					FeDiskIo_SelectTextureFrame(HUD_ENERGY_BAR_MODEL_TYPE, HUD_ENERGY_BAR_FRAME_TRIPLE,
												HUD_ENERGY_BAR_BASE_SIZE);
				} else {
					FeDiskIo_SelectTextureFrame(HUD_ENERGY_BAR_MODEL_TYPE, HUD_ENERGY_BAR_FRAME_DEFAULT,
												HUD_ENERGY_BAR_BASE_SIZE);
				}
				RenderQuad_DrawModelTexture(HUD_ENERGY_BAR_MODEL_TYPE, &quad, color);
			}

			if (emptySegments != 0) {
				for (segment = emptySegments; segment >= 1; --segment) {
					quad.screenX += chargeSegmentStepX;
					if (bankWeaponCount == 3) {
						FeDiskIo_SelectTextureFrame(HUD_ENERGY_BAR_MODEL_TYPE, HUD_ENERGY_BAR_FRAME_TRIPLE,
													HUD_ENERGY_BAR_BASE_SIZE);
					} else {
						FeDiskIo_SelectTextureFrame(HUD_ENERGY_BAR_MODEL_TYPE, HUD_ENERGY_BAR_FRAME_DEFAULT,
													HUD_ENERGY_BAR_BASE_SIZE);
					}
					RenderQuad_DrawModelTexture(HUD_ENERGY_BAR_MODEL_TYPE, &quad, secondaryColor);
				}
			}
		}
		chargePtr += sizeof(*craft->warheadData);
		anchorY += sizeof(*chargeQuads) / sizeof(*anchorY);
		--weaponCount;
	} while (weaponCount != 0);
}

// FUNCTION: XWA 0x46AD90
uint16_t Hud_GetPlayerShieldSilhouetteSpriteId(void) {
	int objectIndex;
	uint16_t objectType;

	objectIndex = g_players[g_localPlayer].objectIndex;
	if (objectIndex == 0xffff) {
		return 0;
	}

	objectType = g_objectTable[objectIndex].objectType;
	if (objectType == OBJ_None || (uint16_t)objectType > OBJ_ContainerBrick) {
		return 0;
	}

	return g_shieldSilhouetteSpriteIdByObjectType[(uint16_t)objectType];
}

// FUNCTION: XWA 0x46ADE0
void Hud_DrawShieldStrength3D(void) {
	FlightTexQuad quad;
	uint16_t lowerSegments;
	CraftData* craft;
	int objectIndex;
	void(XWA_HUD_STDCALL * debugOutput)(const char*);

	quad.screenX = 0;
	quad.screenY = 0;
	quad.depthZ = 1;
	quad.rotationAngle = 0;
	quad.screenSize = 256;

	if (!g_hudElementEnabled[1].enabled) {
		return;
	}

	debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
	objectIndex = g_players[g_localPlayer].objectIndex;
	if (objectIndex != 0xffff && g_objectTable[objectIndex].mobj != NULL &&
		g_objectTable[objectIndex].mobj->pCraft != NULL) {
		craft = g_objectTable[objectIndex].mobj->pCraft;
	} else {
		debugOutput("GetCraftPointer() returned NULL in HUD.c\n");
		craft = NULL;
	}

	if (craft == NULL) {
		debugOutput("NULL craft data pointer in DrawShieldStrength3D()!\n");
		return;
	}

	quad.screenSize = (int)(double)((double)quad.screenSize * g_flightHudScaleFactor);

	if ((craft->systemFlags & 1u) != 0) {
		int halfMaxShield;
		uint16_t upperSegments;
		int shieldValue;

		quad.screenX = (uint16_t)g_hudShieldGaugeSideOffsetX;
		quad.screenY = g_screenHeight - (uint16_t)g_hudShieldGaugeBottomOffsetY;
		FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, 0x2bu, 256);
		RenderQuad_DrawModelTexture(OBJ_HudTextureGroup12000, &quad, (int)g_hudColors[0]);

		halfMaxShield = Craft_GetObjectMaxShield((uint16_t)g_players[g_localPlayer].objectIndex) / 2;
		craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
		if ((craft->activeHudFeatureMask & 0x20u) == 0) {
			return;
		}

		shieldValue = craft->shieldFront;
		if (shieldValue < 0) {
			shieldValue = 0;
		}
		if ((craft->workingSubsystems & 1u) == 0) {
			shieldValue = 0;
		}
		if (shieldValue >= halfMaxShield) {
			lowerSegments = 9;
			upperSegments = (uint16_t)MATH2_longfraction(
				9u,
				(uint16_t)MATH2_percentage((uint32_t)(shieldValue - halfMaxShield), (uint32_t)halfMaxShield));
		} else {
			lowerSegments = (uint16_t)MATH2_longfraction(
				9u, (uint16_t)MATH2_percentage((uint32_t)shieldValue, (uint32_t)halfMaxShield));
			upperSegments = 0;
		}

		if (g_playerFlightTransientTimers[g_localPlayer].field_02 && !g_lastShieldDamageSide) {
			if (upperSegments == 0) {
				lowerSegments = 10;
			} else {
				upperSegments = 10;
			}
		}

		{
			uint16_t colorIndex;
			int iconObjectIndex;

			if (g_playerFlightTransientTimers[g_localPlayer].field_04) {
				colorIndex = 3;
			} else {
				CraftData* hullCraft;

				hullCraft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
				if ((uint32_t)(hullCraft->hullMax / 3u) == 0) {
					colorIndex = 2;
				} else {
					uint32_t damageLevel;

					damageLevel = (uint32_t)hullCraft->hullDamage / (uint32_t)(hullCraft->hullMax / 3u);
					if ((uint16_t)damageLevel > 2u) {
						damageLevel = 2;
					}
					colorIndex = (uint16_t)(2u - (uint16_t)damageLevel);
				}
			}

			iconObjectIndex = g_players[g_localPlayer].objectIndex;
			{
				uint16_t spriteId;

				if (iconObjectIndex == 0xffff) {
					spriteId = 0;
				} else {
					uint16_t objectType;

					objectType = g_objectTable[iconObjectIndex].objectType;
					if (objectType == OBJ_None || objectType > OBJ_ContainerBrick) {
						spriteId = 0;
					} else {
						spriteId = g_shieldSilhouetteSpriteIdByObjectType[objectType];
						spriteId = spriteId != 0 ? spriteId : 0;
					}
				}
				if (spriteId != 0) {
					quad.screenX = (uint16_t)g_hudShieldHullIconSideOffsetX;
					quad.screenY = g_screenHeight - (uint16_t)g_hudShieldHullIconBottomOffsetY;
					FeDiskIo_SelectTextureFrame(OBJ_HullIconTextureGroup26000,
												(uint16_t)((int)spriteId / 100), 256);
					RenderQuad_DrawModelTexture(OBJ_HullIconTextureGroup26000, &quad,
												(int)argbColor[colorIndex]);
				}
			}
		}

		if (upperSegments != 0) {
			int argbColor;

			quad.screenX = (uint16_t)g_hudShieldBarSideOffsetX;
			quad.screenY = g_screenHeight - (uint16_t)g_hudShieldFrontUpperBarY;
			argbColor = (int)g_hudShieldBarArgbBySegmentCount[upperSegments];
			FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, 0x29u, 256);
			RenderQuad_DrawModelTexture(OBJ_HudTextureGroup12000, &quad, argbColor);
		}
		if (lowerSegments != 0) {
			int argbColor;

			quad.screenX = (uint16_t)g_hudShieldBarSideOffsetX;
			quad.screenY = g_screenHeight - (uint16_t)g_hudShieldFrontLowerBarY;
			argbColor = (int)g_hudShieldBarArgbBySegmentCount[lowerSegments];
			FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, 0x27u, 256);
			RenderQuad_DrawModelTexture(OBJ_HudTextureGroup12000, &quad, argbColor);
		}

		craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
		shieldValue = craft->shieldRear;
		if (shieldValue < 0) {
			shieldValue = 0;
		}
		if ((craft->workingSubsystems & 1u) == 0) {
			shieldValue = 0;
		}
		if (shieldValue >= halfMaxShield) {
			lowerSegments = 9;
			upperSegments = (uint16_t)MATH2_longfraction(
				9u,
				(uint16_t)MATH2_percentage((uint32_t)(shieldValue - halfMaxShield), (uint32_t)halfMaxShield));
		} else {
			lowerSegments = (uint16_t)MATH2_longfraction(
				9u, (uint16_t)MATH2_percentage((uint32_t)shieldValue, (uint32_t)halfMaxShield));
			upperSegments = 0;
		}

		if (g_playerFlightTransientTimers[g_localPlayer].field_02 && !g_lastShieldDamageSide) {
			if (upperSegments == 0) {
				lowerSegments = 10;
			} else {
				upperSegments = 10;
			}
		}

		if (upperSegments != 0) {
			int argbColor;

			quad.screenX = (uint16_t)g_hudShieldBarSideOffsetX;
			quad.screenY = g_screenHeight - (uint16_t)g_hudShieldRearUpperBarY;
			argbColor = (int)g_hudShieldBarArgbBySegmentCount[upperSegments];
			FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, 0x2au, 256);
			RenderQuad_DrawModelTexture(OBJ_HudTextureGroup12000, &quad, argbColor);
		}
		if (lowerSegments != 0) {
			int argbColor;

			quad.screenX = (uint16_t)g_hudShieldBarSideOffsetX;
			quad.screenY = g_screenHeight - (uint16_t)g_hudShieldRearLowerBarY;
			argbColor = (int)g_hudShieldBarArgbBySegmentCount[lowerSegments];
			FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, 0x28u, 256);
			RenderQuad_DrawModelTexture(OBJ_HudTextureGroup12000, &quad, argbColor);
		}
	} else {
		uint16_t colorIndex;

		quad.screenX = (uint16_t)g_hudShieldGaugeSideOffsetX;
		quad.screenY = g_screenHeight - (uint16_t)g_hudShieldGaugeBottomOffsetY;
		FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, 0x2bu, 256);
		RenderQuad_DrawModelTexture(OBJ_HudTextureGroup12000, &quad, (int)g_hudColors[0]);

		if (g_playerFlightTransientTimers[g_localPlayer].field_04) {
			colorIndex = 3;
		} else {
			CraftData* hullCraft;

			hullCraft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
			if ((uint32_t)(hullCraft->hullMax / 3u) == 0) {
				colorIndex = 2;
			} else {
				uint32_t damageLevel;

				damageLevel = (uint32_t)hullCraft->hullDamage / (uint32_t)(hullCraft->hullMax / 3u);
				if ((uint16_t)damageLevel > 2u) {
					damageLevel = 2;
				}
				colorIndex = (uint16_t)(2u - (uint16_t)damageLevel);
			}
		}

		objectIndex = g_players[g_localPlayer].objectIndex;
		{
			uint16_t spriteId;

			if (objectIndex == 0xffff) {
				spriteId = 0;
			} else {
				uint16_t objectType;

				objectType = g_objectTable[objectIndex].objectType;
				if (objectType == OBJ_None || objectType > OBJ_ContainerBrick) {
					spriteId = 0;
				} else {
					spriteId = g_shieldSilhouetteSpriteIdByObjectType[objectType];
					spriteId = spriteId != 0 ? spriteId : 0;
				}
			}
			if (spriteId != 0) {
				quad.screenX = (uint16_t)g_hudShieldHullIconSideOffsetX;
				quad.screenY = g_screenHeight - (uint16_t)g_hudShieldHullIconBottomOffsetY;
				FeDiskIo_SelectTextureFrame(OBJ_HullIconTextureGroup26000, (uint16_t)((int)spriteId / 100),
											256);
				RenderQuad_DrawModelTexture(OBJ_HullIconTextureGroup26000, &quad, (int)argbColor[colorIndex]);
			}
		}
	}
}

// FUNCTION: XWA 0x46B4F0
void Hud_DrawBeamStrength3D(void) {
	enum {
		HUD_BEAM_MODEL_TYPE = OBJ_HudTextureGroup12000,
		HUD_BEAM_BASE_SIZE = 256,
		HUD_BEAM_GAUGE_FRAME = 0x2c,
		HUD_BEAM_ICON_FRAME = 0x1d,
		HUD_BEAM_FIRST_CHARGE_FRAME = 0x1e,
		HUD_BEAM_FEATURE_MASK = 0x10,
		HUD_BEAM_SUBSYSTEM_MASK = 0x100
	};
	const int HUD_BEAM_ICON_INACTIVE_COLOR = (int)0xff730000u;
	const int HUD_BEAM_ICON_ACTIVE_COLOR = (int)0xffff0000u;
	const int HUD_BEAM_CHARGE_LOW_COLOR = (int)0xff2e13c3u;
	const int HUD_BEAM_CHARGE_MEDIUM_COLOR = (int)0xff557fd7u;
	const int HUD_BEAM_CHARGE_HIGH_COLOR = (int)0xff6fb1dbu;
	FlightTexQuad quad;
	CraftData* craft;
	void(XWA_HUD_STDCALL * debugOutput)(const char*);
	int objectIndex;
	int16_t beamPresent;
	int8_t beamIsActive;
	int color;
	uint16_t frame;
	int16_t segmentIndex;

	quad.screenX = 0;
	quad.screenY = 0;
	quad.depthZ = 1;
	quad.rotationAngle = 0;
	quad.screenSize = HUD_BEAM_BASE_SIZE;

	if (!g_hudElementEnabled[2].enabled) {
		return;
	}

	debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
	craft = Hud_GetCraftPointerInlinedWithDebug(debugOutput);
	if (craft == NULL) {
		debugOutput("NULL craft data pointer in DrawBeamStrength3D()!\n");
		return;
	}

	if ((craft->installedHudFeatureMask & HUD_BEAM_FEATURE_MASK) == 0) {
		return;
	}

	objectIndex = g_players[g_localPlayer].objectIndex;
	craft = g_objectTable[objectIndex].mobj->pCraft;
	if ((craft->activeHudFeatureMask & HUD_BEAM_FEATURE_MASK) != 0) {

		beamPresent = (int16_t)craft->beamPresent;
		if (beamPresent < 0) {
			beamPresent = 0;
		}
		if ((craft->workingSubsystems & HUD_BEAM_SUBSYSTEM_MASK) == 0) {
			beamPresent = 0;
		}

		beamIsActive = craft->beamActive != 0;
		if ((craft->workingSubsystems & HUD_BEAM_SUBSYSTEM_MASK) == 0) {
			beamIsActive = 0;
		}

		quad.screenX = g_screenWidth - (uint16_t)g_hudBeamGaugeRightOffsetX;
		quad.screenY = g_screenHeight - (uint16_t)g_hudBeamGaugeBottomOffsetY;
		FeDiskIo_SelectTextureFrame(HUD_BEAM_MODEL_TYPE, HUD_BEAM_GAUGE_FRAME, HUD_BEAM_BASE_SIZE);
		RenderQuad_DrawModelTexture(HUD_BEAM_MODEL_TYPE, &quad, g_hudColors[0]);

		quad.screenX = g_screenWidth - (uint16_t)g_hudBeamIconRightOffsetX;
		quad.screenY = g_screenHeight - (uint16_t)g_hudBeamIconBottomOffsetY;
		FeDiskIo_SelectTextureFrame(HUD_BEAM_MODEL_TYPE, HUD_BEAM_ICON_FRAME, HUD_BEAM_BASE_SIZE);
		color = beamIsActive ? (int)HUD_BEAM_ICON_ACTIVE_COLOR : (int)HUD_BEAM_ICON_INACTIVE_COLOR;
		RenderQuad_DrawModelTexture(HUD_BEAM_MODEL_TYPE, &quad, color);

		quad.screenX = g_screenWidth - (uint16_t)g_hudBeamChargeRightOffsetX;
		quad.screenY = g_screenHeight - (uint16_t)g_hudBeamChargeBottomOffsetY;
		frame = HUD_BEAM_FIRST_CHARGE_FRAME;
		if (beamPresent <= 0) {
			return;
		}

		for (segmentIndex = 0; segmentIndex < (int16_t)g_hudBeamChargeSegmentCount; ++segmentIndex) {
			if ((int16_t)beamPresent > (int16_t)g_hudBeamChargeUnitsPerSegment * (segmentIndex + 1)) {
				color = (int)HUD_BEAM_CHARGE_HIGH_COLOR;
			} else {
				beamPresent -= (int16_t)(g_hudBeamChargeUnitsPerSegment * segmentIndex);
				if (beamPresent > (int16_t)g_hudBeamChargeUnitsPerSegment) {
					beamPresent = (int16_t)g_hudBeamChargeUnitsPerSegment;
				}
				beamPresent /= (int16_t)g_hudBeamChargeColorBandUnits;
				switch (beamPresent) {
					case 1:
						color = (int)HUD_BEAM_CHARGE_LOW_COLOR;
						break;
					case 2:
						color = (int)HUD_BEAM_CHARGE_MEDIUM_COLOR;
						break;
					case 3:
						color = (int)HUD_BEAM_CHARGE_HIGH_COLOR;
						break;
					default:
						break;
				}
			}

			if (beamPresent > 0) {
				FeDiskIo_SelectTextureFrame(HUD_BEAM_MODEL_TYPE, frame, HUD_BEAM_BASE_SIZE);
				RenderQuad_DrawModelTexture(HUD_BEAM_MODEL_TYPE, &quad, color);
			}

			++frame;
			switch (segmentIndex) {
				case 0:
					quad.screenY += (uint16_t)g_hudBeamChargeSegmentOffsetY0;
					break;
				case 1:
					quad.screenY += (uint16_t)g_hudBeamChargeSegmentOffsetY1;
					break;
				case 2:
					quad.screenY += (uint16_t)g_hudBeamChargeSegmentOffsetY2;
					break;
				case 3:
					quad.screenY += (uint16_t)g_hudBeamChargeSegmentOffsetY3;
					break;
				case 4:
					quad.screenY += (uint16_t)g_hudBeamChargeSegmentOffsetY4;
					break;
				case 5:
					quad.screenY += (uint16_t)g_hudBeamChargeSegmentOffsetY5;
					break;
				case 6:
					quad.screenY += (uint16_t)g_hudBeamChargeSegmentOffsetY6;
					break;
				case 7:
					quad.screenY += (uint16_t)g_hudBeamChargeSegmentOffsetY7;
					break;
				default:
					break;
			}
		}
		return;
	}

	quad.screenX = g_screenWidth - (uint16_t)g_hudBeamGaugeRightOffsetX;
	quad.screenY = g_screenHeight - (uint16_t)g_hudBeamGaugeBottomOffsetY;
	FeDiskIo_SelectTextureFrame(HUD_BEAM_MODEL_TYPE, HUD_BEAM_GAUGE_FRAME, HUD_BEAM_BASE_SIZE);
	RenderQuad_DrawModelTexture(HUD_BEAM_MODEL_TYPE, &quad, g_hudColors[0]);
}

// FUNCTION: XWA 0x46B8A0
void Hud_DrawMfdFrames3D(void) {
	enum {
		HUD_MFD_FRAME_MODEL_TYPE = OBJ_HudTextureGroup12000,
		HUD_MFD_FRAME_BASE_SIZE = 256,
		HUD_MFD_FRAME_BACK_FRAME = 1,
		HUD_MFD_FRAME_FRONT_FRAME = 2
	};
	FlightTexQuad quad;

	quad.screenX = 0;
	quad.screenY = 0;
	quad.depthZ = 1;
	quad.rotationAngle = 0;
	quad.screenSize = HUD_MFD_FRAME_BASE_SIZE;

	if (g_filmPlaybackMode) {
		return;
	}

	{
		float scaledSize = g_flightHudScaleFactor;
		scaledSize *= g_hudQuadBaseSize;
		quad.screenSize = (uint16_t)(int)scaledSize;
	}

	if (g_players[g_localPlayer].mfd.enabled[1] || g_inHangarReady) {
		quad.screenX = (uint16_t)g_hudMfdFrameSideOffsetX;
		quad.screenY = (uint16_t)g_hudMfdFrameY;
		FeDiskIo_SelectTextureFrame(HUD_MFD_FRAME_MODEL_TYPE, HUD_MFD_FRAME_BACK_FRAME,
									HUD_MFD_FRAME_BASE_SIZE);
		quad.screenSize <<= 1;
		RenderQuad_DrawModelTexture(HUD_MFD_FRAME_MODEL_TYPE, &quad, g_hudColors[0]);
		quad.screenSize >>= 1;

		if (g_players[g_localPlayer].mfd.activeIndex == 1) {
			FeDiskIo_SelectTextureFrame(HUD_MFD_FRAME_MODEL_TYPE, HUD_MFD_FRAME_FRONT_FRAME,
										HUD_MFD_FRAME_BASE_SIZE);
			RenderQuad_DrawModelTexture(HUD_MFD_FRAME_MODEL_TYPE, &quad, g_hudColors[1]);
		} else {
			FeDiskIo_SelectTextureFrame(HUD_MFD_FRAME_MODEL_TYPE, HUD_MFD_FRAME_FRONT_FRAME,
										HUD_MFD_FRAME_BASE_SIZE);
			RenderQuad_DrawModelTexture(HUD_MFD_FRAME_MODEL_TYPE, &quad, g_hudColors[0]);
		}
	}

	if (g_players[g_localPlayer].mfd.enabled[2] || g_inHangarReady) {
		quad.screenX = g_screenWidth - (uint16_t)g_hudMfdFrameSideOffsetX;
		quad.screenY = (uint16_t)g_hudMfdFrameY;
		g_currentQuadTexCoords = g_mfdFrameQuadTexCoords;
		FeDiskIo_SelectTextureFrame(HUD_MFD_FRAME_MODEL_TYPE, HUD_MFD_FRAME_BACK_FRAME,
									HUD_MFD_FRAME_BASE_SIZE);
		quad.screenSize <<= 1;
		RenderQuad_DrawModelTexture(HUD_MFD_FRAME_MODEL_TYPE, &quad, g_hudColors[0]);
		quad.screenSize >>= 1;

		if (g_players[g_localPlayer].mfd.activeIndex == 2) {
			FeDiskIo_SelectTextureFrame(HUD_MFD_FRAME_MODEL_TYPE, HUD_MFD_FRAME_FRONT_FRAME,
										HUD_MFD_FRAME_BASE_SIZE);
			RenderQuad_DrawModelTexture(HUD_MFD_FRAME_MODEL_TYPE, &quad, g_hudColors[1]);
		} else {
			FeDiskIo_SelectTextureFrame(HUD_MFD_FRAME_MODEL_TYPE, HUD_MFD_FRAME_FRONT_FRAME,
										HUD_MFD_FRAME_BASE_SIZE);
			RenderQuad_DrawModelTexture(HUD_MFD_FRAME_MODEL_TYPE, &quad, g_hudColors[0]);
		}
		g_currentQuadTexCoords = g_defaultQuadTexCoords;
	}
}

// FUNCTION: XWA 0x46BAB0
void Hud_DrawFilmMfdFrames3D(void) {
	enum {
		HUD_MFD_FRAME_MODEL_TYPE = OBJ_HudTextureGroup12000,
		HUD_MFD_FRAME_BASE_SIZE = 256,
		HUD_MFD_FRAME_BACK_FRAME = 1,
		HUD_MFD_FRAME_FRONT_FRAME = 2
	};
	FlightTexQuad quad;
	int localPlayer;
	uint8_t leftMfdEnabled;
	uint8_t rightMfdEnabled;

	quad.screenX = 0;
	quad.screenY = 0;
	quad.depthZ = 1;
	quad.rotationAngle = 0;
	quad.screenSize = HUD_MFD_FRAME_BASE_SIZE;

	if (!g_filmPlaybackMode) {
		g_currentQuadTexCoords = g_defaultQuadTexCoords;
		return;
	}

	{
		float scaledSize = g_flightHudScaleFactor;
		scaledSize *= g_hudQuadBaseSize;
		quad.screenSize = (uint16_t)(int)scaledSize;
	}
	localPlayer = g_localPlayer;
	leftMfdEnabled = g_players[localPlayer].mfd.enabled[1];
	if (leftMfdEnabled || g_filmOverlayMfdVisible) {
		if (!g_players[localPlayer].mapCameraState) {
			if (!g_players[localPlayer].hudEnabled && !g_filmOverlayMfdVisible) {
				return;
			}
			if (!g_filmOverlayMfdVisible && leftMfdEnabled && g_filmOverlayActive) {
				return;
			}
			if (!g_filmOverlayMfdVisible && g_players[localPlayer].mfd.enabled[0]) {
				return;
			}
		}

		quad.screenX = (uint16_t)g_hudMfdFrameSideOffsetX;
		quad.screenY = (uint16_t)g_hudMfdFrameY;
		FeDiskIo_SelectTextureFrame(HUD_MFD_FRAME_MODEL_TYPE, HUD_MFD_FRAME_BACK_FRAME,
									HUD_MFD_FRAME_BASE_SIZE);
		quad.screenSize <<= 1;
		RenderQuad_DrawModelTexture(HUD_MFD_FRAME_MODEL_TYPE, &quad, g_hudColors[0]);
		quad.screenSize >>= 1;
		FeDiskIo_SelectTextureFrame(HUD_MFD_FRAME_MODEL_TYPE, HUD_MFD_FRAME_FRONT_FRAME,
									HUD_MFD_FRAME_BASE_SIZE);
		RenderQuad_DrawModelTexture(HUD_MFD_FRAME_MODEL_TYPE, &quad, g_hudColors[0]);
	}

	localPlayer = g_localPlayer;
	rightMfdEnabled = g_players[localPlayer].mfd.enabled[2];
	if (rightMfdEnabled || g_filmOverlayMfdVisible) {
		if (g_filmOverlayMfdVisible || rightMfdEnabled) {
			if (!g_players[localPlayer].mapCameraState) {
				if (!g_players[localPlayer].hudEnabled && !g_filmOverlayMfdVisible) {
					return;
				}
				if (!g_filmOverlayMfdVisible && g_players[localPlayer].mfd.enabled[1] &&
					g_filmOverlayActive) {
					return;
				}
				if (!g_filmOverlayMfdVisible && g_players[localPlayer].mfd.enabled[0]) {
					return;
				}
			}

			quad.screenX = g_screenWidth - (uint16_t)g_hudMfdFrameSideOffsetX;
			quad.screenY = (uint16_t)g_hudMfdFrameY;
			g_currentQuadTexCoords = g_mfdFrameQuadTexCoords;
			FeDiskIo_SelectTextureFrame(HUD_MFD_FRAME_MODEL_TYPE, HUD_MFD_FRAME_BACK_FRAME,
										HUD_MFD_FRAME_BASE_SIZE);
			quad.screenSize <<= 1;
			RenderQuad_DrawModelTexture(HUD_MFD_FRAME_MODEL_TYPE, &quad, g_hudColors[0]);
			quad.screenSize >>= 1;
			FeDiskIo_SelectTextureFrame(HUD_MFD_FRAME_MODEL_TYPE, HUD_MFD_FRAME_FRONT_FRAME,
										HUD_MFD_FRAME_BASE_SIZE);
			RenderQuad_DrawModelTexture(HUD_MFD_FRAME_MODEL_TYPE, &quad, g_hudColors[0]);
		}
	}
	g_currentQuadTexCoords = g_defaultQuadTexCoords;
}

// FUNCTION: XWA 0x46BD00
void Hud_DrawReticle3D(void) {
	enum {
		HUD_RETICLE_MODEL_TYPE = OBJ_HudTextureGroup12000,
		HUD_RETICLE_BASE_SIZE = 256,
		HUD_RETICLE_LASER_FRAME = 5,
		HUD_RETICLE_HIT_FRAME = 6,
		HUD_RETICLE_WARHEAD_FRAME = 7,
		HUD_RETICLE_WARHEAD_LOCK_FRAME = 8,
		HUD_RETICLE_LASER_READY_FRAME = 9,
		HUD_RETICLE_LASER_PIP_FRAME = 10
	};

	void(XWA_HUD_STDCALL * debugOutput)(const char*);
	FlightTexQuad quad;
	CraftData* craft;
	uint16_t objectType;
	ModelIndex missileBoatModelIndex;
	ModelIndex modelIndex;
	uint16_t lockFrame;
	uint8_t inRange;
	uint16_t lockRange;
	unsigned int lockRangeDivisor;

	g_reticleDrawX = g_reticleCenterX;
	g_reticleDrawY = g_reticleCenterY;

	quad.screenX = 0;
	quad.screenY = 0;
	quad.depthZ = 1;
	quad.rotationAngle = 0;
	quad.screenSize = HUD_RETICLE_BASE_SIZE;
	lockFrame = HUD_RETICLE_LASER_FRAME;
	inRange = 0;

	debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
	craft = Hud_GetCraftPointerInlinedWithDebug(debugOutput);
	if (craft == NULL) {
		debugOutput("NULL craft data pointer in DrawReticle3D()!\n");
		return;
	}

	objectType = g_objectTable[g_players[g_localPlayer].objectIndex].objectType;
	if (objectType == OBJ_None) {
		return;
	}

	modelIndex = GetModelIndexFromType(objectType);
	if (modelIndex == (ModelIndex)0xffff) {
		return;
	}

	missileBoatModelIndex = GetModelIndexFromType(OBJ_MissileBoat);
	if (missileBoatModelIndex == (ModelIndex)0xffff) {
		return;
	}

	quad.screenSize = (uint16_t)(int)((double)quad.screenSize * (double)g_flightHudScaleFactor);
	lockRange = modelIndex == missileBoatModelIndex ? 354 : 708;
	lockRangeDivisor = lockRange / 200;

	if (Hud_AbsLookDegreesFromOffset(g_players[g_localPlayer].lookYawOffset) >= 45 ||
		Hud_AbsLookDegreesFromOffset(g_players[g_localPlayer].lookPitchOffset) >= 45) {
		return;
	}

	if (g_players[g_localPlayer].lookYawOffset || g_players[g_localPlayer].lookPitchOffset) {
		int worldZ;

		pai_RotateLocalVectorToWorldScratch(&g_objectTable[g_players[g_localPlayer].objectIndex], 0, 0,
											1000000);
		g_camRelWorldX = g_objectTable[g_players[g_localPlayer].objectIndex].world_x + g_rotatedX;
		g_camRelWorldY = g_objectTable[g_players[g_localPlayer].objectIndex].world_y + g_rotatedY;
		worldZ = g_objectTable[g_players[g_localPlayer].objectIndex].world_z + g_rotatedZ;
		g_camRelWorldX -= g_players[g_localPlayer].viewState.savedTargetX;
		g_camRelWorldY -= g_players[g_localPlayer].viewState.savedTargetY;
		g_camRelWorldZ = worldZ - g_players[g_localPlayer].viewState.savedTargetZ;
		viewX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
		viewY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
		viewZ = TRANSFM2_CamMatDotRow2(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
		g_reticleDrawX = TRANSFM2_ProjectScreenX(viewX, viewZ);
		g_reticleDrawY = TRANSFM2_ProjectScreenY(viewY, viewZ);
	}

	if (g_players[g_localPlayer].selectedWeaponMode == 0) {
		int laserSlot;

		laserSlot = 0;
		while (laserSlot < g_reticleLaserHardpointCount) {
			if (g_reticleLaserHardpointIndices[laserSlot] != -1) {
				int16_t laserReadyState;
				int linkGroup;

				laserReadyState = 10;
				linkGroup = laserSlot > g_modelDefs[craft->modelIndex].laserGroupLastSlot[0];
				if ((int8_t)craft->warheadData[laserSlot].laserCharge <= 0) {
					laserReadyState = 10;
				} else if ((g_players[g_localPlayer].selectedWeaponMode == 0 &&
							g_players[g_localPlayer].selectedWarhead == linkGroup) ||
						   craft->laserLinkMode[linkGroup] == 4) {
					switch (craft->laserLinkMode[linkGroup]) {
						case 1:
							laserReadyState =
								(int16_t)((craft->laserLinkNextSlot[linkGroup] != laserSlot) + 9);
							break;
						case 2:
							if (craft->laserLinkNextSlot[linkGroup] == laserSlot) {
								laserReadyState = 9;
							} else if (g_reticleLaserHardpointCount >= 4 &&
									   craft->laserLinkNextSlot[linkGroup] + 2 == laserSlot) {
								laserReadyState = 9;
							} else {
								laserReadyState = 10;
							}
							break;
						case 3:
						case 4:
							laserReadyState = 9;
							break;
						case 0:
							laserReadyState = 10;
							break;
						default:
							break;
					}
				}

				if (g_players[g_localPlayer].lookYawOffset || g_players[g_localPlayer].lookPitchOffset) {
					int aimX;
					int drawX;

					aimX = g_reticleLaserAimPoints[laserSlot].x;
					drawX = g_reticleDrawX;
					quad.screenX = aimX + drawX - g_reticleCenterX;
					quad.screenY = g_screenHeight + g_reticleCenterY - g_reticleDrawY -
								   g_reticleLaserAimPoints[laserSlot].y;
				} else {
					quad.screenX = g_reticleLaserAimPoints[laserSlot].x;
					quad.screenY = g_screenHeight - g_reticleLaserAimPoints[laserSlot].y;
				}

				if (g_players[g_localPlayer].currentSeatIdx == 0) {
#ifdef XWA_MODERN
					XwaSnapshotHud_NoteReticleReady(laserSlot, laserReadyState == 9);
#endif
					FeDiskIo_SelectTextureFrame(HUD_RETICLE_MODEL_TYPE, HUD_RETICLE_LASER_PIP_FRAME,
												HUD_RETICLE_BASE_SIZE);
					RenderQuad_DrawModelTexture(HUD_RETICLE_MODEL_TYPE, &quad, g_hudColors[0]);
					if (laserReadyState == 9) {
						FeDiskIo_SelectTextureFrame(HUD_RETICLE_MODEL_TYPE, HUD_RETICLE_LASER_READY_FRAME,
													HUD_RETICLE_BASE_SIZE);
						RenderQuad_DrawModelTexture(HUD_RETICLE_MODEL_TYPE, &quad, -1);
					}
				}

				if (((craft->workingSubsystems & 4u) != 0 && laserReadyState != 10) ||
					((craft->workingSubsystems & 4u) != 0 && g_reticleWarheadHardpointCount &&
					 !g_players[g_localPlayer].turretAutoFireState) ||
					g_players[g_localPlayer].currentSeatIdx) {
					if (g_players[g_localPlayer].currentTargetObjectIdx != 0xffffu) {
						if (g_players[g_localPlayer].currentSeatIdx && g_reticleWarheadHardpointCount) {
							if ((uint16_t)collide_targetinrange(
									(uint16_t)g_players[g_localPlayer].objectIndex,
									(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx,
									(uint16_t)g_reticleWarheadHardpointIndices[0]) != 0) {
								lockFrame = HUD_RETICLE_HIT_FRAME;
								inRange = 1;
							}
						} else {
							if ((uint16_t)collide_targetinrange(
									(uint16_t)g_players[g_localPlayer].objectIndex,
									(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx,
									laserSlot) != 0) {
								lockFrame = HUD_RETICLE_HIT_FRAME;
								inRange = 1;
							}
							if (!inRange && g_reticleWarheadHardpointCount &&
								!g_players[g_localPlayer].turretAutoFireState) {
								unsigned int warheadSlot;

								warheadSlot = 0;
								while (warheadSlot < (unsigned int)g_reticleWarheadHardpointCount) {
									if ((uint16_t)collide_targetinrange(
											(uint16_t)g_players[g_localPlayer].objectIndex,
											(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx,
											(uint16_t)g_reticleWarheadHardpointIndices[warheadSlot]) != 0) {
										lockFrame = HUD_RETICLE_HIT_FRAME;
										inRange = 1;
										break;
									}
									++warheadSlot;
								}
							}
						}
					}
				}
			}
			++laserSlot;
		}

		if (inRange) {
			fsfx_UpdateTargetingTone(1);
		} else {
			fsfx_UpdateTargetingTone(0);
		}
	}

	quad.screenX = g_reticleCenterX;
	quad.screenY = g_screenHeight - g_reticleCenterY;
	if (g_players[g_localPlayer].lookYawOffset || g_players[g_localPlayer].lookPitchOffset) {
		quad.screenX = g_reticleDrawX;
		quad.screenY = g_screenHeight - g_reticleDrawY;
	}
#ifdef XWA_MODERN
	XwaSnapshotHud_NoteReticleInRange(inRange);
#endif

	if (g_players[g_localPlayer].selectedWeaponMode == 0) {
		FeDiskIo_SelectTextureFrame(HUD_RETICLE_MODEL_TYPE, HUD_RETICLE_LASER_FRAME, HUD_RETICLE_BASE_SIZE);
		RenderQuad_DrawModelTexture(HUD_RETICLE_MODEL_TYPE, &quad, g_hudColors[0]);
		if (inRange) {
			FeDiskIo_SelectTextureFrame(HUD_RETICLE_MODEL_TYPE, lockFrame, HUD_RETICLE_BASE_SIZE);
			RenderQuad_DrawModelTexture(HUD_RETICLE_MODEL_TYPE, &quad, -1);
		}
		return;
	}

	if (g_players[g_localPlayer].selectedWeaponMode != 1) {
		return;
	}

	FeDiskIo_SelectTextureFrame(HUD_RETICLE_MODEL_TYPE, HUD_RETICLE_WARHEAD_FRAME, HUD_RETICLE_BASE_SIZE);
	RenderQuad_DrawModelTexture(HUD_RETICLE_MODEL_TYPE, &quad, g_hudColors[0]);
	if (g_players[g_localPlayer].currentTargetObjectIdx != 0xffffu) {
		if (g_players[g_localPlayer].missileLockState == 2) {
			int16_t lockTimer;
			int lockStep;
			unsigned int color;

			lockTimer = (int16_t)craft->warheadLockTicks;
			lockStep = lockTimer / (int16_t)lockRangeDivisor;
			if (lockTimer <= lockStep) {
				color = -3604736;
			} else {
				if (lockTimer != lockRange) {
					int colorStep;

					colorStep = lockStep;
					color = -++colorStep;
					color *= 256;
				} else {
					color = -65536;
				}
			}
			FeDiskIo_SelectTextureFrame(HUD_RETICLE_MODEL_TYPE, HUD_RETICLE_WARHEAD_LOCK_FRAME,
										HUD_RETICLE_BASE_SIZE);
			RenderQuad_DrawModelTexture(HUD_RETICLE_MODEL_TYPE, &quad, color);
			fsfx_UpdateTargetingTone(2);
			return;
		}
		if (g_players[g_localPlayer].missileLockState == 3) {
			FeDiskIo_SelectTextureFrame(HUD_RETICLE_MODEL_TYPE, HUD_RETICLE_WARHEAD_LOCK_FRAME,
										HUD_RETICLE_BASE_SIZE);
			RenderQuad_DrawModelTexture(HUD_RETICLE_MODEL_TYPE, &quad, -65536);
			fsfx_UpdateTargetingTone(3);
			return;
		}
	}
	fsfx_UpdateTargetingTone(0);
}

// FUNCTION: XWA 0x46C570
void Hud_DrawTargetArrow3D(void) {
	enum {
		HUD_TARGET_ARROW_BASE_SIZE = 256,
		HUD_TARGET_ARROW_EDGE_PADDING = 6,
		HUD_TARGET_ARROW_CLIP_SIZE = 0x32,
		HUD_TARGET_ARROW_TEXT_VISIBLE = 0x4e,
		HUD_TARGET_ARROW_TEXT_BEHIND = 0x4d,
		HUD_TARGET_ARROW_COLOR_VISIBLE = -1645056,
		HUD_TARGET_ARROW_COLOR_BEHIND = -3618816
	};

	FlightTexQuad quad;
	CraftData* craft;
	int nameX;
	int nameY;
	int distanceX;
	int distanceY;
	uint16_t targetObjIdx;
	uint8_t behindCamera;

	nameX = 0;
	nameY = 0;
	distanceX = 0;
	distanceY = 0;
	quad.screenX = 0;
	quad.screenY = 0;
	quad.depthZ = 1;
	quad.rotationAngle = 0;
	quad.screenSize = HUD_TARGET_ARROW_BASE_SIZE;
	behindCamera = 0;

	if (g_inHangarReady) {
		return;
	}

	if (g_players[g_localPlayer].objectIndex == 0xffff ||
		g_objectTable[g_players[g_localPlayer].objectIndex].mobj == NULL ||
		g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft == NULL) {
		XWA_HUD_OUTPUT_DEBUG_STRING("GetCraftPointer() returned NULL in HUD.c\n");
		craft = NULL;
	} else {
		craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
	}

	if (craft == NULL) {
		XWA_HUD_OUTPUT_DEBUG_STRING("NULL craft data pointer in DrawTargetArrow3D()!\n");
		return;
	}

	quad.screenSize = (uint16_t)(int)((double)quad.screenSize * (double)g_flightHudScaleFactor);
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetShadowEnabled(0);

	if ((craft->workingSubsystems & 4u) == 0) {
		return;
	}

	targetObjIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
	if (targetObjIdx == 0xffffu) {
		return;
	}

	FlightView_ComputeObjectViewPosition(targetObjIdx);
	TRANSFM2_ProjectScreenX(viewX, viewZ);
	TRANSFM2_ProjectScreenY(viewY, viewZ);

	if (g_players[g_localPlayer].padlockActive) {
		int padlockX;
		int padlockY;
		int angle;

		if (g_players[g_localPlayer].lookYawOffset || g_players[g_localPlayer].lookPitchOffset) {
			padlockX = g_reticleCenterX -
					   (int)((double)g_players[g_localPlayer].lookYawOffset * g_hudPadlockYawScreenScale);
			padlockY = g_reticleCenterY -
					   (int)((double)g_players[g_localPlayer].lookPitchOffset * g_hudPadlockPitchScreenScale);
		} else {
			padlockX = g_reticleCenterX;
			padlockY = g_reticleCenterY;
		}

		if (padlockX >= 0 && padlockX <= (int)g_screenWidth && padlockY >= 0 &&
			padlockY <= (int)g_screenHeight) {
			return;
		}

		angle = trig2_arctan(abs(g_hudCenterX - padlockX), abs(g_hudCenterY - padlockY)) & 0xffff;
		quad.screenX = padlockX;
		if (padlockY < 0 || padlockY > (int)g_screenHeight) {
			quad.screenY = padlockY;
		} else {
			quad.screenY = g_screenHeight - padlockY;
		}

		if (padlockX < 0) {
			quad.screenX = HUD_TARGET_ARROW_EDGE_PADDING;
			if (quad.screenY < g_hudCenterY) {
				quad.rotationAngle = (int16_t)-angle;
			} else {
				quad.rotationAngle = (int16_t)(angle + g_hudTargetArrowQ15Scale);
			}
		}
		if (quad.screenX > (int)g_screenWidth) {
			quad.screenX = g_screenWidth - HUD_TARGET_ARROW_EDGE_PADDING;
			if (quad.screenY < g_hudCenterY) {
				quad.rotationAngle = (int16_t)angle;
			} else {
				quad.rotationAngle = (int16_t)(g_hudTargetArrowQ15Scale - angle);
			}
		}
		if (quad.screenY < 0) {
			quad.screenY = g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING;
			if (quad.screenX < g_reticleCenterX) {
				quad.rotationAngle = (int16_t)(angle + 32760);
			} else {
				quad.rotationAngle = (int16_t)(g_hudTargetArrowQ15Scale - angle);
			}
		}
		if (quad.screenY > (int)g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING) {
			quad.screenY = HUD_TARGET_ARROW_EDGE_PADDING;
			if (quad.screenX < g_hudCenterX) {
				angle = (uint16_t)(-(int16_t)angle);
			}
			quad.rotationAngle = (int16_t)angle;
		}

		FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup13000_Sprite000, 1u, HUD_TARGET_ARROW_BASE_SIZE);
		RenderQuad_DrawModelTexture(OBJ_HudTextureGroup13000_Sprite000, &quad,
									HUD_TARGET_ARROW_COLOR_VISIBLE);
		{
			FlightText_SetRenderOffset((int16_t)quad.screenX,
									   (int16_t)(g_screenHeight - (int16_t)quad.screenY));
			FlightText_SetClipRect(0, 0, HUD_TARGET_ARROW_CLIP_SIZE, HUD_TARGET_ARROW_CLIP_SIZE);

			if (quad.screenX == HUD_TARGET_ARROW_EDGE_PADDING) {
				nameX = 12;
				distanceX = 9;
				nameY = -g_flightFontLineHeight;
				distanceY = 0;
			} else if (quad.screenX == (int)g_screenWidth - HUD_TARGET_ARROW_EDGE_PADDING) {
				nameX = -(int)FlightText_MeasureStringWidth(g_hudTargetNameText) - 15;
				distanceX = -35;
				nameY = -g_flightFontLineHeight;
				distanceY = 0;
			} else if (quad.screenY == HUD_TARGET_ARROW_EDGE_PADDING) {
				nameX = -(int)(FlightText_MeasureStringWidth(g_hudTargetNameText) >> 1);
				nameY = -15 - g_flightFontLineHeight;
				distanceX = -12;
				distanceY = -15;
			} else if (quad.screenY == (int)g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING) {
				nameX = -(int)(FlightText_MeasureStringWidth(g_hudTargetNameText) >> 1);
				nameY = g_flightFontLineHeight + 5;
				distanceX = -12;
				distanceY = 7;
			}
			if (quad.screenY <= 10) {
				nameY += quad.screenY - 10;
				distanceY += quad.screenY - 10;
			}
			if (quad.screenX <= -nameX) {
				nameX = -quad.screenX;
			}
			if (quad.screenX - nameX >= (int)g_screenWidth &&
				(quad.screenY == HUD_TARGET_ARROW_EDGE_PADDING ||
				 quad.screenY == (int)g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING) &&
				quad.screenX != HUD_TARGET_ARROW_EDGE_PADDING &&
				quad.screenX != (int)g_screenWidth - HUD_TARGET_ARROW_EDGE_PADDING) {
				nameX = g_screenWidth + 2 * nameX - quad.screenX;
			}

			FlightText_SetCursor(nameX, nameY);
			FlightText_DrawString(g_hudTargetNameText);
			FlightText_SetCursor(distanceX, distanceY);
			FlightText_SetFontTier(0);
			FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
			FlightText_SetColor(HUD_TARGET_ARROW_TEXT_VISIBLE);
			FlightText_DrawDecimalNumber(g_hudTargetDistanceWhole, 2u, 1u);
			FlightText_SetCursor(distanceX + 10, distanceY);
			g_flightDrawCharFn('.');
			FlightText_SetCursor(distanceX + 13, distanceY);
			FlightText_DrawDecimalNumber(g_hudTargetDistanceFrac, 2u, 2u);
			FlightText_SetRenderOffset(0, 0);
		}
		return;
	}

	if (viewZ < 0) {
		behindCamera = 1;
		viewZ = -viewZ;
	}

	{
		int projectedX;
		int projectedY;

		projectedX = TRANSFM2_ProjectScreenX(viewX, viewZ);
		projectedY = TRANSFM2_ProjectScreenY(viewY, viewZ);

		if (projectedX >= 0 && projectedX <= (int)g_screenWidth && projectedY >= 0 &&
			projectedY <= (int)g_screenHeight) {
			if (!behindCamera) {
				return;
			}

			quad.screenX = projectedX;
			projectedY = HUD_TARGET_ARROW_EDGE_PADDING;
			if (quad.screenY >= g_reticleCenterY) {
				projectedY = g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING;
			}
			quad.screenY = projectedY;
			if (projectedY == HUD_TARGET_ARROW_EDGE_PADDING) {
				quad.rotationAngle = g_hudTargetArrowQ15Scale;
			}
			{
				FlightText_SetRenderOffset((int16_t)quad.screenX,
										   (int16_t)(g_screenHeight - (int16_t)quad.screenY));
				FlightText_SetClipRect(0, 0, HUD_TARGET_ARROW_CLIP_SIZE, HUD_TARGET_ARROW_CLIP_SIZE);

				if (quad.screenX == HUD_TARGET_ARROW_EDGE_PADDING) {
					nameX = 12;
					distanceX = 9;
					nameY = -g_flightFontLineHeight;
					distanceY = 0;
				} else if (quad.screenX == (int)g_screenWidth - HUD_TARGET_ARROW_EDGE_PADDING) {
					nameX = -(int)FlightText_MeasureStringWidth(g_hudTargetNameText) - 15;
					distanceX = -35;
					nameY = -g_flightFontLineHeight;
					distanceY = 0;
				} else if (quad.screenY == HUD_TARGET_ARROW_EDGE_PADDING) {
					nameX = -(int)(FlightText_MeasureStringWidth(g_hudTargetNameText) >> 1);
					nameY = -15 - g_flightFontLineHeight;
					distanceX = -12;
					distanceY = -15;
				} else if (quad.screenY == (int)g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING) {
					nameX = -(int)(FlightText_MeasureStringWidth(g_hudTargetNameText) >> 1);
					nameY = g_flightFontLineHeight + 5;
					distanceX = -12;
					distanceY = 7;
				}
				if (quad.screenY >= (int)g_screenHeight - 10) {
					nameY += quad.screenY - g_screenHeight + 10;
					distanceY += quad.screenY - g_screenHeight + 10;
				}
				if (quad.screenY <= 10) {
					nameY += quad.screenY - 10;
					distanceY += quad.screenY - 10;
				}
				if (quad.screenX <= -nameX) {
					nameX = -quad.screenX;
				}
				if (quad.screenX - nameX >= (int)g_screenWidth &&
					(quad.screenY == HUD_TARGET_ARROW_EDGE_PADDING ||
					 quad.screenY == (int)g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING) &&
					quad.screenX != HUD_TARGET_ARROW_EDGE_PADDING &&
					quad.screenX != (int)g_screenWidth - HUD_TARGET_ARROW_EDGE_PADDING) {
					nameX = g_screenWidth + 2 * nameX - quad.screenX;
				}

				FlightText_SetCursor(nameX, nameY);
				FlightText_DrawString(g_hudTargetNameText);
				FlightText_SetCursor(distanceX, distanceY);
				FlightText_SetFontTier(0);
				FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
				FlightText_SetColor(HUD_TARGET_ARROW_TEXT_BEHIND);
				FlightText_DrawDecimalNumber(g_hudTargetDistanceWhole, 2u, 1u);
				FlightText_SetCursor(distanceX + 10, distanceY);
				g_flightDrawCharFn('.');
				FlightText_SetCursor(distanceX + 13, distanceY);
				FlightText_DrawDecimalNumber(g_hudTargetDistanceFrac, 2u, 2u);
				FlightText_SetRenderOffset(0, 0);
			}
			FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup13000_Sprite000, 1u, HUD_TARGET_ARROW_BASE_SIZE);
			RenderQuad_DrawModelTexture(OBJ_HudTextureGroup13000_Sprite000, &quad,
										HUD_TARGET_ARROW_COLOR_BEHIND);
			return;
		}

		{
			int angle;
			int edgeX;
			int edgeY;

			angle = trig2_arctan(abs(g_hudCenterX - projectedX), abs(g_hudCenterY - projectedY)) & 0xffff;
			edgeY = projectedY;
			edgeX = projectedX;
			quad.screenX = edgeX;
			if (edgeY < 0 || edgeY > (int)g_screenHeight) {
				quad.screenY = edgeY;
			} else {
				edgeY = g_screenHeight - edgeY;
				quad.screenY = edgeY;
			}

			if (projectedX < 0) {
				edgeX = HUD_TARGET_ARROW_EDGE_PADDING;
				quad.screenX = HUD_TARGET_ARROW_EDGE_PADDING;
				if (edgeY < g_hudCenterY) {
					quad.rotationAngle = (int16_t)(g_hudTargetArrowQ15Scale - angle);
				} else {
					quad.rotationAngle = (int16_t)angle;
				}
			}
			if (edgeX > (int)g_screenWidth) {
				edgeX = g_screenWidth - HUD_TARGET_ARROW_EDGE_PADDING;
				quad.screenX = g_screenWidth - HUD_TARGET_ARROW_EDGE_PADDING;
				if (edgeY < g_hudCenterY) {
					quad.rotationAngle = (int16_t)(angle + g_hudTargetArrowQ15Scale);
				} else {
					quad.rotationAngle = (int16_t)-angle;
				}
			}
			if (edgeY < 0) {
				edgeY = g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING;
				quad.screenY = g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING;
				if (edgeX < g_reticleCenterX) {
					quad.rotationAngle = (int16_t)angle;
				} else {
					quad.rotationAngle = (int16_t)-angle;
				}
			}
			if (edgeY > (int)g_screenHeight) {
				edgeY = HUD_TARGET_ARROW_EDGE_PADDING;
				quad.screenY = HUD_TARGET_ARROW_EDGE_PADDING;
				if (edgeX < g_hudCenterX) {
					quad.rotationAngle = (int16_t)(g_hudTargetArrowQ15Scale - angle);
				} else {
					quad.rotationAngle = (int16_t)(g_hudTargetArrowQ15Scale + angle);
				}
				edgeX = quad.screenX;
			}
			if (edgeX <= HUD_TARGET_ARROW_EDGE_PADDING &&
				edgeY >= (int)g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING) {
				edgeY = g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING;
				edgeX = HUD_TARGET_ARROW_EDGE_PADDING;
				quad.screenY = g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING;
				quad.screenX = HUD_TARGET_ARROW_EDGE_PADDING;
				quad.rotationAngle = (int16_t)angle;
			}
			if (edgeX >= (int)g_screenWidth - HUD_TARGET_ARROW_EDGE_PADDING &&
				edgeY >= (int)g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING) {
				edgeX = g_screenWidth - HUD_TARGET_ARROW_EDGE_PADDING;
				edgeY = g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING;
				quad.screenX = g_screenWidth - HUD_TARGET_ARROW_EDGE_PADDING;
				quad.screenY = g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING;
				quad.rotationAngle = (int16_t)-angle;
			}
			if (edgeX <= HUD_TARGET_ARROW_EDGE_PADDING && edgeY <= HUD_TARGET_ARROW_EDGE_PADDING) {
				edgeY = HUD_TARGET_ARROW_EDGE_PADDING;
				quad.screenX = HUD_TARGET_ARROW_EDGE_PADDING;
				quad.rotationAngle = (int16_t)(g_hudTargetArrowQ15Scale - angle);
				edgeX = HUD_TARGET_ARROW_EDGE_PADDING;
				quad.screenY = HUD_TARGET_ARROW_EDGE_PADDING;
			}
			if (edgeX >= (int)g_screenWidth - HUD_TARGET_ARROW_EDGE_PADDING &&
				edgeY <= HUD_TARGET_ARROW_EDGE_PADDING) {
				quad.screenX = g_screenWidth - HUD_TARGET_ARROW_EDGE_PADDING;
				quad.screenY = HUD_TARGET_ARROW_EDGE_PADDING;
				quad.rotationAngle = (int16_t)(g_hudTargetArrowQ15Scale + angle);
			}

			FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup13000_Sprite000, 1u, HUD_TARGET_ARROW_BASE_SIZE);
			RenderQuad_DrawModelTexture(OBJ_HudTextureGroup13000_Sprite000, &quad,
										behindCamera ? HUD_TARGET_ARROW_COLOR_BEHIND
													 : HUD_TARGET_ARROW_COLOR_VISIBLE);
			{
				FlightText_SetRenderOffset((int16_t)quad.screenX,
										   (int16_t)(g_screenHeight - (int16_t)quad.screenY));
				FlightText_SetClipRect(0, 0, HUD_TARGET_ARROW_CLIP_SIZE, HUD_TARGET_ARROW_CLIP_SIZE);

				if (quad.screenX == HUD_TARGET_ARROW_EDGE_PADDING) {
					nameX = 12;
					distanceX = 9;
					nameY = -g_flightFontLineHeight;
					distanceY = 0;
				} else if (quad.screenX == (int)g_screenWidth - HUD_TARGET_ARROW_EDGE_PADDING) {
					nameX = -(int)FlightText_MeasureStringWidth(g_hudTargetNameText) - 15;
					distanceX = -35;
					nameY = -g_flightFontLineHeight;
					distanceY = 0;
				} else if (quad.screenY == HUD_TARGET_ARROW_EDGE_PADDING) {
					nameX = -(int)(FlightText_MeasureStringWidth(g_hudTargetNameText) >> 1);
					nameY = -15 - g_flightFontLineHeight;
					distanceX = -12;
					distanceY = -15;
				} else if (quad.screenY == (int)g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING) {
					nameX = -(int)(FlightText_MeasureStringWidth(g_hudTargetNameText) >> 1);
					nameY = g_flightFontLineHeight + 5;
					distanceX = -12;
					distanceY = 7;
				}
				if (quad.screenY >= (int)g_screenHeight - 10) {
					nameY += quad.screenY - g_screenHeight + 10;
					distanceY += quad.screenY - g_screenHeight + 10;
				}
				if (quad.screenY <= 10) {
					nameY += quad.screenY - 10;
					distanceY += quad.screenY - 10;
				}
				if (quad.screenX <= -nameX) {
					nameX = -quad.screenX;
				}
				if (quad.screenX - nameX >= (int)g_screenWidth &&
					(quad.screenY == HUD_TARGET_ARROW_EDGE_PADDING ||
					 quad.screenY == (int)g_screenHeight - HUD_TARGET_ARROW_EDGE_PADDING) &&
					quad.screenX != HUD_TARGET_ARROW_EDGE_PADDING &&
					quad.screenX != (int)g_screenWidth - HUD_TARGET_ARROW_EDGE_PADDING) {
					nameX = g_screenWidth + 2 * nameX - quad.screenX;
				}

				FlightText_SetCursor(nameX, nameY);
				FlightText_DrawString(g_hudTargetNameText);
				FlightText_SetCursor(distanceX, distanceY);
				FlightText_SetFontTier(0);
				FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
				FlightText_SetColor(behindCamera ? HUD_TARGET_ARROW_TEXT_BEHIND
												 : HUD_TARGET_ARROW_TEXT_VISIBLE);
				FlightText_DrawDecimalNumber(g_hudTargetDistanceWhole, 2u, 1u);
				FlightText_SetCursor(distanceX + 10, distanceY);
				g_flightDrawCharFn('.');
				FlightText_SetCursor(distanceX + 13, distanceY);
				FlightText_DrawDecimalNumber(g_hudTargetDistanceFrac, 2u, 2u);
				FlightText_SetRenderOffset(0, 0);
			}
		}
	}
}

// FUNCTION: XWA 0x46D1D0
void Hud_DrawTargetBoxReadout(int boxX, int boxY, int boxWidth, int boxHeight) {
	int targetNameY;
	int bottomY;
	int upperReadoutY;
	int lowerReadoutY;
	int centerX;
	uint16_t textWidth;

	if (g_players[g_localPlayer].currentTargetObjectIdx == 0xffffu) {
		return;
	}

	FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetColor(0x43u);
	FlightText_SetShadowEnabled(0);

	targetNameY = boxY - (uint8_t)g_flightFontLineHeight;
	textWidth = FlightText_MeasureStringWidth(g_hudTargetNameText);
	centerX = boxX + boxWidth / 2;
	FlightText_SetCursor(centerX - (textWidth >> 1), targetNameY);
	FlightText_DrawString(g_hudTargetNameText);

	FlightText_SetColor(0x47u);
	textWidth = FlightText_MeasureStringWidth("100");
	bottomY = boxY + boxHeight;
	FlightText_SetCursor(boxX - textWidth, bottomY - (uint8_t)g_flightFontLineHeight + 2);
	FlightText_DrawDecimalNumber(g_hudTargetHullDisplayPct, 3u, 1u);

	upperReadoutY = boxY - 2;
	FlightText_SetCursor(boxX + boxWidth + 3, upperReadoutY);
	FlightText_DrawDecimalNumber(g_hudTargetSystemDisplayPct, 3u, 1u);

	textWidth = FlightText_MeasureStringWidth("200");
	FlightText_SetCursor(boxX - textWidth, upperReadoutY);
	FlightText_DrawDecimalNumber(g_hudTargetShieldDisplayPct, 3u, 1u);

	lowerReadoutY = bottomY - (uint8_t)g_flightFontLineHeight + 2;
	FlightText_SetCursor(boxX + boxWidth, lowerReadoutY);
	FlightText_DrawDecimalNumber(g_hudTargetDistanceWhole, 2u, 1u);
	FlightText_SetCursor(boxX + boxWidth + 10, lowerReadoutY);
	g_flightDrawCharFn('.');
	FlightText_SetCursor(boxX + boxWidth + 13, lowerReadoutY);
	FlightText_DrawDecimalNumber(g_hudTargetDistanceFrac, 2u, 2u);

	textWidth = FlightText_MeasureStringWidth(g_hudTargetStatusText);
	FlightText_SetCursor(centerX - (textWidth >> 1), bottomY);
	FlightText_DrawString(g_hudTargetStatusText);
	FlightText_SetRenderOffset(0, 0);
}

// FUNCTION: XWA 0x46D3F0
void Hud_DetermineLockStatus(void) {
	CraftData* craft;
	int16_t laserReadyState = 10;
	uint8_t targetToneActive = 0;
	void(XWA_HUD_STDCALL * debugOutput)(const char*);

	debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
	craft = Hud_GetCraftPointerInlinedWithDebug(debugOutput);
	if (craft == NULL) {
		debugOutput("NULL craft data pointer in DetermineLockStatus()!\n");
		return;
	}

	if (g_players[g_localPlayer].currentTargetObjectIdx == 0xffffu) {
		fsfx_UpdateTargetingTone(0);
		return;
	}

	if (g_players[g_localPlayer].selectedWeaponMode == 0) {
		int laserSlot;

		laserSlot = 0;
		if (g_reticleLaserHardpointCount > 0) {
			do {
				if (g_reticleLaserHardpointIndices[laserSlot] != -1) {
					int linkGroup;

					laserReadyState = 10;
					linkGroup = laserSlot > g_modelDefs[craft->modelIndex].laserGroupLastSlot[0];
					if ((int8_t)craft->warheadData[laserSlot].laserCharge > 0) {
						if (g_players[g_localPlayer].selectedWarhead == linkGroup) {
							switch (craft->laserLinkMode[linkGroup]) {
								case 1:
									laserReadyState =
										(int16_t)((craft->laserLinkNextSlot[linkGroup] != laserSlot) + 9);
									break;
								case 2:
									if (craft->laserLinkNextSlot[linkGroup] == laserSlot) {
										laserReadyState = 9;
										break;
									}
									if (g_reticleLaserHardpointCount < 4 ||
										craft->laserLinkNextSlot[linkGroup] + 2 != laserSlot) {
										laserReadyState = 10;
										break;
									}
									/* Fall through. */
								case 3:
								case 4:
									laserReadyState = 9;
									break;
								case 0:
									laserReadyState = 10;
									break;
								default:
									break;
							}
						}
					} else {
						laserReadyState = 10;
					}
				}
			} while (++laserSlot < g_reticleLaserHardpointCount);
		}

		if ((craft->workingSubsystems & 4u) != 0 && laserReadyState != 10) {
			if (collide_targetinrange(g_players[g_localPlayer].objectIndex,
									  g_players[g_localPlayer].currentTargetObjectIdx, laserSlot) != 0) {
				targetToneActive = 1;
			}
		}

		if (targetToneActive) {
			fsfx_UpdateTargetingTone(1);
			return;
		}
	} else if (g_players[g_localPlayer].selectedWeaponMode == 1) {
		if (g_players[g_localPlayer].missileLockState == 2) {
			fsfx_UpdateTargetingTone(2);
			return;
		}
		if (g_players[g_localPlayer].missileLockState == 3) {
			fsfx_UpdateTargetingTone(3);
			return;
		}
	} else {
		return;
	}

	fsfx_UpdateTargetingTone(0);
}

static __inline void Hud_UpdateLeftSubsystemLabels(void) {
	CraftData* craft;

	craft = Hud_GetCraftPointerInlined();
	if (craft == NULL) {
		OutputDebugStringA("NULL craft data pointer in UpdateRadarLeftLabels()!\n");
		return;
	}
	HUD_PANE_PUSH(XWA_HUD_PANE_LEFT_SUBSYSTEM, 0, 0, (int)(40.0f * g_flightHudScaleFactor),
				  (int)(200.0f * g_flightHudScaleFactor));

	FlightText_SetColor(0x42u);
	FlightText_SetFontTier(0);
	if ((craft->systemFlags & 0x10u) != 0) {
		FlightText_SetCursor(0, 0);
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(g_hudSubsystemLabelLaserX, g_hudSubsystemLabelLaserY);
		} else {
			FlightSw_SetRenderTarget(hudTex16, (uint16_t)g_hudSubsystemLaserLabelSurfaceWidth,
									 (uint16_t)g_hudSubsystemLaserLabelSurfaceHeight,
									 (uint16_t)g_hudSubsystemLaserLabelSurfaceWidth *
										 g_flight16bppBytesPerPixel);
		}
		FlightText_SetClipRect(0, 0, g_hudSubsystemLaserLabelSurfaceWidth,
							   g_hudSubsystemLaserLabelSurfaceHeight);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_DrawString(g_strPanelStrings[PANEL_STRING_HUD_L]);
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
		} else {
			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			{
				uint32_t w = g_hudSubsystemLaserLabelSurfaceWidth;
				Blit16ToFlightSurface(hudTex16, g_flightColorEscapeBypassChar, 0, 0,
									  g_hudSubsystemLabelLaserX, g_hudSubsystemLabelLaserY, w,
									  g_hudSubsystemLaserLabelSurfaceHeight, w * g_flight16bppBytesPerPixel);
			}
		}
	}
	if ((craft->systemFlags & 0x1u) != 0) {
		FlightText_SetCursor(0, 0);
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(g_hudSubsystemLabelShieldX, g_hudSubsystemLabelShieldY);
		} else {
			FlightSw_SetRenderTarget(hudTex17, (uint16_t)g_hudSubsystemShieldLabelSurfaceWidth,
									 (uint16_t)g_hudSubsystemShieldLabelSurfaceHeight,
									 (uint16_t)g_hudSubsystemShieldLabelSurfaceWidth *
										 g_flight16bppBytesPerPixel);
		}
		FlightText_SetClipRect(0, 0, g_hudSubsystemShieldLabelSurfaceWidth,
							   g_hudSubsystemShieldLabelSurfaceHeight);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_DrawString(g_strPanelStrings[PANEL_STRING_HUD_S]);
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
		} else {
			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			{
				uint32_t w = g_hudSubsystemShieldLabelSurfaceWidth;
				Blit16ToFlightSurface(hudTex17, g_flightColorEscapeBypassChar, 0, 0,
									  g_hudSubsystemLabelShieldX, g_hudSubsystemLabelShieldY, w,
									  g_hudSubsystemShieldLabelSurfaceHeight, w * g_flight16bppBytesPerPixel);
			}
		}
	}
	HUD_PANE_POP();
}

static __inline void Hud_UpdateRightSubsystemLabels(void) {
	CraftData* craft;

	craft = Hud_GetCraftPointer();
	if (craft == NULL) {
		OutputDebugStringA("NULL craft data pointer in UpdateRadarRightLabels()!\n");
		return;
	}
	HUD_PANE_PUSH(XWA_HUD_PANE_RIGHT_SUBSYSTEM, g_screenWidth - (int)(40.0f * g_flightHudScaleFactor), 0,
				  (int)(40.0f * g_flightHudScaleFactor), (int)(200.0f * g_flightHudScaleFactor));

	FlightText_SetColor(0x42u);
	FlightText_SetFontTier(0);
	if ((craft->systemFlags & 0x40u) != 0) {
		FlightText_SetCursor(0, 0);
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(g_hudSubsystemLabelEngineX, g_hudSubsystemLabelEngineY);
		} else {
			FlightSw_SetRenderTarget(hudTex18, (uint16_t)g_hudSubsystemEngineLabelSurfaceWidth,
									 (uint16_t)g_hudSubsystemEngineLabelSurfaceHeight,
									 (uint16_t)g_hudSubsystemEngineLabelSurfaceWidth *
										 g_flight16bppBytesPerPixel);
		}
		FlightText_SetClipRect(0, 0, g_hudSubsystemEngineLabelSurfaceWidth,
							   g_hudSubsystemEngineLabelSurfaceHeight);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_DrawString(g_strPanelStrings[PANEL_STRING_HUD_E]);
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
		} else {
			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			{
				uint32_t w = g_hudSubsystemEngineLabelSurfaceWidth;
				Blit16ToFlightSurface(hudTex18, g_flightColorEscapeBypassChar, 0, 0,
									  g_hudSubsystemLabelEngineX, g_hudSubsystemLabelEngineY, w,
									  g_hudSubsystemEngineLabelSurfaceHeight, w * g_flight16bppBytesPerPixel);
			}
		}
	}
	if ((craft->systemFlags & 0x100u) != 0) {
		FlightText_SetCursor(0, 0);
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(g_hudSubsystemLabelBeamX, g_hudSubsystemLabelBeamY);
		} else {
			FlightSw_SetRenderTarget(hudTex19, (uint16_t)g_hudSubsystemBeamLabelSurfaceWidth,
									 (uint16_t)g_hudSubsystemBeamLabelSurfaceHeight,
									 (uint16_t)g_hudSubsystemBeamLabelSurfaceWidth *
										 g_flight16bppBytesPerPixel);
		}
		FlightText_SetClipRect(0, 0, g_hudSubsystemBeamLabelSurfaceWidth,
							   g_hudSubsystemBeamLabelSurfaceHeight);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_DrawString(g_strPanelStrings[PANEL_STRING_HUD_B]);
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
		} else {
			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			{
				uint32_t w = g_hudSubsystemBeamLabelSurfaceWidth;
				Blit16ToFlightSurface(hudTex19, g_flightColorEscapeBypassChar, 0, 0, g_hudSubsystemLabelBeamX,
									  g_hudSubsystemLabelBeamY, w, g_hudSubsystemBeamLabelSurfaceHeight,
									  w * g_flight16bppBytesPerPixel);
			}
		}
	}
	HUD_PANE_POP();
}

// FUNCTION: XWA 0x46D610
int Hud_UpdateHUD(void) {
	int result;

	Hud_UpdateTargetInfoCache();
	if (!g_useHardware3D) {
		FlightSurface_Lock();
		g_drawTarget->pixels = g_surfacePixels;
		g_drawTarget->pitch = g_surfacePitch;
	}

	if (g_players[g_localPlayer].hudEnabled) {
		FlightText_SetWordWrap(0);
		FlightText_SetClearLineBackground(0);
		FlightText_SetShadowEnabled(0);

		if (!g_inHangarReady && (g_players[g_localPlayer].regionSessionId || g_flightMissionEndPending)) {
			goto draw_overlays;
		}

		if (!g_players[g_localPlayer].mapCameraState && (!g_filmPlaybackMode || g_filmOverlayActive != 1)) {
			if ((g_players[g_localPlayer].viewState.externalCameraActive ||
				 (g_filmPlaybackMode && g_filmOverlayActive == 1)) &&
				g_hudElementEnabled[3].enabled) {
				Hud_UpdateHUDText();
			}

			if (!g_players[g_localPlayer].viewState.externalCameraActive) {
				if (!g_players[g_localPlayer].mfd.enabled[0]) {
					if (g_hudElementEnabled[3].enabled) {
						Hud_UpdateHUDText();
					}
					if (g_hudElementEnabled[1].enabled) {
						Hud_UpdateLeftSubsystemLabels();
						Hud_UpdateShieldPercentLabels();
					}
					if (g_hudElementEnabled[2].enabled) {
						Hud_UpdateRightSubsystemLabels();
					}
					Hud_UpdateWarheadCnt();
					if (g_useHardware3D && !g_flightConfPowerVr) {
						Hud_DrawHudTargetInsetIfEnabled(g_localPlayer);
					}
					if (!g_hudElementEnabled[0].enabled) {
						goto update_mfd;
					}
					goto update_cmd;
				}

				Hud_UpdateWarheadCnt();
				Hud_UpdateHUDText();
				goto draw_overlays;
			} else {
				if (((g_players[g_localPlayer].mfd.enabled[1] == 1 &&
					  g_players[g_localPlayer].mfd.page[1] == 6) ||
					 (g_players[g_localPlayer].mfd.enabled[2] == 1 &&
					  g_players[g_localPlayer].mfd.page[2] == 6)) &&
					g_players[g_localPlayer].mfd.menuRow == 30) {
					if (g_players[g_localPlayer].mfd.page[1] == 6) {
						Mfd_DrawCommandMenuPage(1, g_hudMfdLeftTexPixels);
					} else {
						Mfd_DrawCommandMenuPage(2, g_hudMfdRightTexPixels);
					}
				}
				goto draw_overlays;
			}
		}

		Hud_UpdateTargetInfoCache();
		Hud_DrawHudTargetInsetIfEnabled(g_localPlayer);
		if ((g_filmPlaybackMode && g_filmOverlayActive == 1) || !g_hudElementEnabled[0].enabled) {
			goto update_mfd;
		}
	update_cmd:
		Hud_UpdateCMDText();
	update_mfd:
		Hud_UpdateMfdPages();

	draw_overlays:
		if (!g_useHardware3D) {
			if (!g_players[g_localPlayer].viewState.externalCameraActive && !g_inHangarReady &&
				!g_players[g_localPlayer].mapCameraState) {
				Hud_DrawReticle2D();
				Hud_UpdateThreatIndicators();
				if (!g_players[g_localPlayer].mfd.enabled[0] && g_hudElementEnabled[0].enabled) {
					Hud_DrawLaserCharge2D();
				}
			}
			if (g_players[g_localPlayer].hyperspacePhase == PLAYER_HYPERSPACE_PHASE_NONE &&
				!g_players[g_localPlayer].mapCameraState &&
				(!g_filmPlaybackMode || g_filmOverlayActive != 1)) {
				goto draw_offscreen;
			}
		}
		goto finish;
	}

	if (!g_useHardware3D && (!g_filmPlaybackMode || g_filmOverlayActive != 1) &&
		g_players[g_localPlayer].viewState.externalCameraActive) {
	draw_offscreen:
		Hud_DrawOffscreenTargetIndicator2D();
	}

finish:
	Hud_DrawNetworkStatusIndicators();
	Hud_DrawFilmRecordingIndicator();
	if (g_filmPlaybackMode && g_filmOverlayMfdVisible) {
		Hud_DrawFilmOverlayMfdTitles();
		Mfd_DrawFilmLeftStatusPage();
		Mfd_DrawFilmRightOptionsPage();
	}

	result = g_useHardware3D;
	if (!g_useHardware3D) {
		return FlightSurface_Unlock();
	}
	return result;
}

// FUNCTION: XWA 0x46DE50
CraftData* Hud_GetCraftPointer(void) {
	int objectIndex;
	MobileObject* mobj;

	objectIndex = g_players[g_localPlayer].objectIndex;
	if (objectIndex != 0xffff) {
		mobj = g_objectTable[objectIndex].mobj;
		if (mobj && mobj->pCraft) {
			return mobj->pCraft;
		}
	}

	XWA_HUD_OUTPUT_DEBUG_STRING("GetCraftPointer() returned NULL in HUD.c\n");
	return NULL;
}

static void Hud_DrawChargeSprite2D(FlightTexQuad quads[16], int slot, uint16_t spriteId) {
	Hud_SetupResourceData(10000, spriteId);
	if (g_curImage != NULL) {
		int drawX = quads[slot].screenX - ((uint16_t)g_curImageWidth >> 1);
		int drawY = quads[slot].screenY - ((uint16_t)g_curImageHeight >> 1);

		Hud_DrawImageToDIB((int16_t)drawX, (int16_t)drawY);
	}
}

// FUNCTION: XWA 0x46DEA0
void Hud_DrawLaserCharge2D(void) {
	enum {
		HUD_LASER_CHARGE_GROUP = 10000,
		HUD_LASER_CHARGE_DUAL_SPRITE_BASE = 2300,
		HUD_LASER_CHARGE_TRIPLE_SPRITE_BASE = 2500,
		HUD_LASER_CHARGE_REQUIRED_FEATURES = 0x0006
	};
	CraftData* craft;
	int laserCount;
	int ionCount;
	int weaponSlot;
	int i;

	if (!g_hudElementEnabled[0].enabled) {
		return;
	}

	craft = Hud_GetCraftPointer();
	if (craft == NULL) {
		DebugPrintf("NULL craft data pointer in DrawLaserCharge3D()!\n");
		return;
	}

	laserCount = 0;
	ionCount = 0;
	for (weaponSlot = 0; (uint16_t)weaponSlot < (uint16_t)g_reticleLaserHardpointCount; ++weaponSlot) {
		if (craft->warheadData[weaponSlot].weaponType == 1) {
			++laserCount;
		} else if (craft->warheadData[weaponSlot].weaponType == 2) {
			++ionCount;
		}
	}

	if ((craft->installedHudFeatureMask & HUD_LASER_CHARGE_REQUIRED_FEATURES) !=
		HUD_LASER_CHARGE_REQUIRED_FEATURES) {
		return;
	}

	if (laserCount > 6 || ionCount > 4) {
		DebugPrintf("Craft has more than MAX laser hardpoints in DrawLaserCharge2D()!\n");
		return;
	}

	Hud_SetupLaserChargePositions3D();

	if (laserCount != 0) {
		uint16_t spriteId;

		if (laserCount == 3 || laserCount == 6) {
			spriteId = HUD_LASER_CHARGE_TRIPLE_SPRITE_BASE;
		} else {
			spriteId = HUD_LASER_CHARGE_DUAL_SPRITE_BASE;
		}

		for (i = 0; i < laserCount; ++i) {
			Hud_DrawChargeSprite2D(g_hudLaserChargeQuads, i + 1, spriteId);
		}

		Hud_DrawEnergyBar2D(g_hudEnergyBankLaserSelector, laserCount);
	}

	if (ionCount != 0) {
		uint16_t spriteId;

		if (ionCount == 3) {
			spriteId = HUD_LASER_CHARGE_TRIPLE_SPRITE_BASE;
		} else {
			spriteId = HUD_LASER_CHARGE_DUAL_SPRITE_BASE;
		}

		for (i = 0; i < ionCount; ++i) {
			Hud_DrawChargeSprite2D(g_hudIonChargeQuads, i + 1, spriteId);
		}

		Hud_DrawEnergyBar2D(g_hudEnergyBankIonSelector, ionCount);
	}
}

static void Hud_DrawEnergyBarSprite2D(uint16_t spriteId, int x, int y) {
	Hud_SetupResourceData(10000, spriteId);
	Hud_DrawImageToDIB((int16_t)x, (int16_t)y);
}

// FUNCTION: XWA 0x4700A0
void Hud_DrawEnergyBar2D(int isIonBank, int bankWeaponCount) {
	enum {
		HUD_ENERGY_BAR_GROUP = 10000,
		HUD_ENERGY_BAR_LASER_BASE_SPRITE = 12700,
		HUD_ENERGY_BAR_ALT_LASER_SPRITE_DELTA = 0x38,
		HUD_ENERGY_BAR_ION_BASE_SPRITE = 12900,
		HUD_ENERGY_BAR_OVERCHARGE_BASE = 64,
		HUD_ENERGY_BAR_SEGMENT_CHARGE = 10,
		HUD_ENERGY_BAR_REQUIRED_FEATURES = 0x0006,
		HUD_ENERGY_BAR_REQUIRED_SUBSYSTEM = 0x0010
	};
	FlightTexQuad chargeQuads[16];
	CraftData* craft;
	uint16_t startWeapon;
	uint16_t endWeapon;
	int spriteBase;
	int segmentStepX;
	int firstSegmentBackstepX;
	int bankSlot;
	int i;
	uint16_t weaponSlot;
	uint32_t segment;

	craft = Hud_GetCraftPointer();
	if (craft == NULL) {
		DebugPrintf("NULL craft data pointer in DrawEnergyBar2D()!\n");
		return;
	}

	for (i = 0; i < 16; ++i) {
		chargeQuads[i].screenX = 0;
		chargeQuads[i].screenY = 0;
		chargeQuads[i].depthZ = 1;
		chargeQuads[i].rotationAngle = 0;
		chargeQuads[i].screenSize = 256;
	}

	if (isIonBank == g_hudEnergyBankLaserSelector) {
		memcpy(chargeQuads, g_hudLaserChargeQuads, sizeof(chargeQuads));
		startWeapon = 0;
		endWeapon = (uint16_t)bankWeaponCount;
		spriteBase = HUD_ENERGY_BAR_LASER_BASE_SPRITE;
		if (craft->laserProjectileTypeId[0] != OBJ_LaserRebel) {
			spriteBase += HUD_ENERGY_BAR_ALT_LASER_SPRITE_DELTA;
		}
	} else {
		memcpy(chargeQuads, g_hudIonChargeQuads, sizeof(chargeQuads));
		endWeapon = craft->laserSlotCount;
		startWeapon = (uint16_t)(craft->laserSlotCount - bankWeaponCount);
		spriteBase = HUD_ENERGY_BAR_ION_BASE_SPRITE;
	}

	if (bankWeaponCount == 3) {
		segmentStepX = g_hudEnergyChargeTripleSegmentStepX;
		firstSegmentBackstepX = g_hudEnergyChargeTripleInitialBackstepX;
	} else {
		segmentStepX = g_hudEnergyChargeNonTripleSegmentStepX;
		firstSegmentBackstepX = g_hudEnergyChargeNonTripleInitialBackstepX;
	}

	bankSlot = 1;
	for (weaponSlot = startWeapon; weaponSlot < endWeapon; ++weaponSlot, ++bankSlot) {
		int8_t rawCharge;
		int16_t chargePlusOne;
		uint16_t displayCharge;
		uint32_t filledSegments;
		uint32_t emptySegments;
		int chargeSpriteOffset;
		int barSpriteId;
		int drawX;
		int drawY;

		rawCharge = (int8_t)craft->warheadData[weaponSlot].laserCharge;
		if ((craft->activeHudFeatureMask & HUD_ENERGY_BAR_REQUIRED_FEATURES) !=
				HUD_ENERGY_BAR_REQUIRED_FEATURES ||
			rawCharge <= 0 || (craft->workingSubsystems & HUD_ENERGY_BAR_REQUIRED_SUBSYSTEM) == 0) {
			continue;
		}

		chargePlusOne = (int16_t)(rawCharge + 1);
		displayCharge = (uint16_t)chargePlusOne;
		if (chargePlusOne > HUD_ENERGY_BAR_OVERCHARGE_BASE) {
			displayCharge = (uint16_t)(chargePlusOne - HUD_ENERGY_BAR_OVERCHARGE_BASE);
			chargeSpriteOffset = g_hudEnergyBarHighChargeSpriteOffset;
		} else {
			chargeSpriteOffset = g_hudEnergyBarLowChargeSpriteOffset;
		}

		filledSegments = displayCharge / HUD_ENERGY_BAR_SEGMENT_CHARGE;
		if (filledSegments == 0 && (chargePlusOne < HUD_ENERGY_BAR_SEGMENT_CHARGE ||
									chargePlusOne > HUD_ENERGY_BAR_OVERCHARGE_BASE)) {
			filledSegments = 1;
		}
		if (filledSegments > g_hudEnergyBarMaxSegments) {
			filledSegments = g_hudEnergyBarMaxSegments;
		}

		emptySegments = 0;
		if (chargePlusOne > HUD_ENERGY_BAR_OVERCHARGE_BASE) {
			emptySegments = g_hudEnergyBarMaxSegments - filledSegments;
		}

		barSpriteId = spriteBase + chargeSpriteOffset;
		if (bankWeaponCount == 3) {
			barSpriteId += g_hudEnergyBarTripleBankSpriteOffset;
		}

		drawX = 0;
		drawY = 0;
		for (segment = 1; segment <= filledSegments; ++segment) {
			Hud_SetupResourceData(HUD_ENERGY_BAR_GROUP, (uint16_t)barSpriteId);
			if (g_curImage != NULL) {
				if (segment == 1) {
					drawX = chargeQuads[bankSlot].screenX - ((uint16_t)g_curImageWidth >> 1) -
							firstSegmentBackstepX;
					drawY = chargeQuads[bankSlot].screenY - ((uint16_t)g_curImageHeight >> 1);
				} else {
					drawX += segmentStepX;
				}
				Hud_DrawImageToDIB((int16_t)drawX, (int16_t)drawY);
			}
		}

		if (emptySegments == 0) {
			continue;
		}

		barSpriteId = spriteBase + g_hudEnergyBarLowChargeSpriteOffset;
		if (bankWeaponCount == 3) {
			barSpriteId += g_hudEnergyBarTripleBankSpriteOffset;
		}

		while (emptySegments != 0) {
			drawX = (int16_t)(drawX + segmentStepX);
			Hud_DrawEnergyBarSprite2D((uint16_t)barSpriteId, drawX, drawY);
			--emptySegments;
		}
	}
}

// FUNCTION: XWA 0x470860
void Hud_DrawReticle2D(void) {
	enum {
		HUD_RETICLE_GROUP = 10000,
		HUD_RETICLE_LASER_SPRITE = 500,
		HUD_RETICLE_HIT_SPRITE = 600,
		HUD_RETICLE_WARHEAD_SPRITE = 700,
		HUD_RETICLE_LOCKING_SPRITE = 13700,
		HUD_RETICLE_LOCKED_SPRITE = 13800,
		HUD_RETICLE_LASER_READY_BASE = 900
	};

	CraftData* craft;
	PlayerData* player;
	ObjectTypeId objectType;
	uint16_t reticleSprite;
	uint8_t inRange;
	int laserSlot;

	g_reticleDrawY = g_reticleCenterY;
	g_reticleDrawX = g_reticleCenterX;
	reticleSprite = HUD_RETICLE_LASER_SPRITE;
	inRange = 0;

	player = &g_players[g_localPlayer];
	craft = Hud_GetCraftPointer();
	if (craft == NULL) {
		DebugPrintf("NULL craft data pointer in DrawReticle2D()!\n");
		return;
	}

	objectType = g_objectTable[player->objectIndex].objectType;
	if (objectType == OBJ_None) {
		return;
	}
	if (GetModelIndexFromType(objectType) == (ModelIndex)0xffff) {
		return;
	}
	if (GetModelIndexFromType(OBJ_MissileBoat) == (ModelIndex)0xffff) {
		return;
	}

	if (Hud_AbsLookDegreesFromOffset(player->lookYawOffset) >= 45 ||
		Hud_AbsLookDegreesFromOffset(player->lookPitchOffset) >= 45) {
		return;
	}

	if (player->lookYawOffset || player->lookPitchOffset) {
		ObjectRecord* playerObj;
		PlayerViewState* viewState;
		int relX;
		int relY;
		int relZ;

		pai_RotateLocalVectorToWorldScratch(&g_objectTable[player->objectIndex], 0, 0, 1000000);
		playerObj = &g_objectTable[player->objectIndex];
		viewState = &player->viewState;
		relX = g_rotatedX + playerObj->world_x - viewState->savedTargetX;
		relY = g_rotatedY + playerObj->world_y - viewState->savedTargetY;
		relZ = g_rotatedZ + playerObj->world_z - viewState->savedTargetZ;

		g_camRelWorldX = relX;
		g_camRelWorldY = relY;
		g_camRelWorldZ = relZ;
		viewX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
		viewY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
		viewZ = TRANSFM2_CamMatDotRow2(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
		g_reticleDrawX = TRANSFM2_ProjectScreenX(viewX, viewZ);
		g_reticleDrawY = TRANSFM2_ProjectScreenY(viewY, viewZ);
	}

	if (player->selectedWeaponMode == 0) {
		for (laserSlot = 0; laserSlot < g_reticleLaserHardpointCount; ++laserSlot) {
			if (g_reticleLaserHardpointIndices[laserSlot] != -1) {
				int16_t laserReadyState;
				int linkGroup;

				laserReadyState = 10;
				linkGroup = laserSlot > g_modelDefs[craft->modelIndex].laserGroupLastSlot[0];
				if ((int8_t)craft->warheadData[laserSlot].laserCharge > 0 &&
					player->selectedWarhead == linkGroup) {
					switch (craft->laserLinkMode[linkGroup]) {
						case 0:
							laserReadyState = 10;
							break;
						case 1:
							laserReadyState =
								(int16_t)((craft->laserLinkNextSlot[linkGroup] != laserSlot) + 9);
							break;
						case 2:
							if (craft->laserLinkNextSlot[linkGroup] == laserSlot) {
								laserReadyState = 9;
							} else if (g_reticleLaserHardpointCount >= 4 &&
									   craft->laserLinkNextSlot[linkGroup] + 2 == laserSlot) {
								laserReadyState = 9;
							} else {
								laserReadyState = 10;
							}
							break;
						case 3:
						case 4:
							laserReadyState = 9;
							break;
						default:
							break;
					}
				}

				if (player->currentSeatIdx == 0) {
					int drawX;
					int drawY;

					Hud_SetupResourceData(HUD_RETICLE_GROUP, (uint16_t)(HUD_RETICLE_LASER_READY_BASE +
																		(laserReadyState - 9) * 100));
					drawX = (int16_t)(g_reticleDrawX + g_reticleLaserAimPoints[laserSlot].x -
									  ((uint16_t)g_curImageWidth >> 1) - g_reticleCenterX);
					drawY = (int16_t)(g_reticleDrawY + g_reticleLaserAimPoints[laserSlot].y -
									  ((uint16_t)g_curImageHeight >> 1) - g_reticleCenterY);
					Hud_DrawImageToDIB(drawX, drawY);
				}

				if ((craft->workingSubsystems & 4u) != 0 && laserReadyState != 10 &&
					player->currentTargetObjectIdx != 0xffffu &&
					(uint16_t)collide_targetinrange((uint16_t)player->objectIndex,
													(uint16_t)player->currentTargetObjectIdx,
													(uint16_t)laserSlot) != 0) {
					reticleSprite = HUD_RETICLE_HIT_SPRITE;
					inRange = 1;
				}
			}
		}

		fsfx_UpdateTargetingTone(inRange != 0);
	}

	Hud_SetupResourceData(HUD_RETICLE_GROUP, reticleSprite);
	{
		int drawX;
		int drawY;

		drawX = (int16_t)(g_reticleCenterX - ((uint16_t)g_curImageWidth >> 1));
		drawY = (int16_t)(g_reticleCenterY - ((uint16_t)g_curImageHeight >> 1));
		if (player->lookYawOffset || player->lookPitchOffset) {
			drawX = (int16_t)(drawX + g_reticleDrawX - g_reticleCenterX);
			drawY = (int16_t)(drawY + g_reticleDrawY - g_reticleCenterY);
		}

		if (player->selectedWeaponMode == 0) {
			Hud_DrawImageToDIB(drawX, drawY);
			return;
		}
		if (player->selectedWeaponMode != 1) {
			return;
		}

		Hud_SetupResourceData(HUD_RETICLE_GROUP, HUD_RETICLE_WARHEAD_SPRITE);
		Hud_DrawImageToDIB(drawX, drawY);

		if (player->currentTargetObjectIdx == 0xffffu) {
			fsfx_UpdateTargetingTone(0);
			return;
		}
		if (player->missileLockState == 2) {
			Hud_SetupResourceData(HUD_RETICLE_GROUP, HUD_RETICLE_LOCKING_SPRITE);
			drawX = (int16_t)(g_reticleCenterX - ((uint16_t)g_curImageWidth >> 1));
			drawY = (int16_t)(g_reticleCenterY - ((uint16_t)g_curImageHeight >> 1));
			if (player->lookYawOffset || player->lookPitchOffset) {
				drawX = (int16_t)(drawX + g_reticleDrawX - g_reticleCenterX);
				drawY = (int16_t)(drawY + g_reticleDrawY - g_reticleCenterY);
			}
			Hud_DrawImageToDIB(drawX, drawY);
			fsfx_UpdateTargetingTone(2);
			return;
		}
		if (player->missileLockState == 3) {
			Hud_SetupResourceData(HUD_RETICLE_GROUP, HUD_RETICLE_LOCKED_SPRITE);
			drawX = (int16_t)(g_reticleCenterX - ((uint16_t)g_curImageWidth >> 1));
			drawY = (int16_t)(g_reticleCenterY - ((uint16_t)g_curImageHeight >> 1));
			if (player->lookYawOffset || player->lookPitchOffset) {
				drawX = (int16_t)(drawX + g_reticleDrawX - g_reticleCenterX);
				drawY = (int16_t)(drawY + g_reticleDrawY - g_reticleCenterY);
			}
			Hud_DrawImageToDIB(drawX, drawY);
			fsfx_UpdateTargetingTone(3);
			return;
		}
	}

	fsfx_UpdateTargetingTone(0);
}

static int Hud_OffscreenTargetDistanceWidth(void) {
	return (int)FlightText_MeasureStringWidth(g_hudTargetDistanceWhole >= 10u ? "00.00" : "0.00") + 2;
}

static void Hud_ClampProjectedOffscreenMarker(int screenX, int screenY, int targetBehind,
											  HudOffscreenTargetMarker* marker) {
	int screenWidth;
	int screenHeight;

	screenWidth = (int16_t)g_screenWidth;
	screenHeight = (int16_t)g_screenHeight;
	marker->x = screenX;
	marker->y = screenY;
	marker->spriteId = 0;
	marker->drawText = 1;
	marker->targetBehind = (uint8_t)targetBehind;

	if (screenY < 0) {
		marker->y = 0;
		marker->spriteId = 16500;
	} else if (screenY > screenHeight) {
		marker->y = g_screenHeight - 16;
		marker->spriteId = 16600;
	}

	if (screenX < 0) {
		marker->x = 0;
		marker->spriteId = 16700;
	} else if (screenX > screenWidth) {
		marker->x = g_screenWidth - 16;
		marker->spriteId = 16800;
	}

	if (marker->x + 16 > screenWidth) {
		marker->x = g_screenWidth - 16;
	}
	if (marker->y + 16 > screenHeight) {
		marker->y = g_screenHeight - 16;
	}

	if (marker->x == 0 && marker->y == 0) {
		marker->spriteId = 16900;
	}
	if (marker->x == screenWidth - 16 && marker->y == 0) {
		marker->spriteId = 17000;
	}
	if (marker->x == 0 && marker->y == screenHeight - 16) {
		marker->spriteId = 17100;
	}
	if (marker->x == screenWidth - 16 && marker->y == screenHeight - 16) {
		marker->spriteId = 17200;
	}
}

static void Hud_ClampPadlockOffscreenMarker(int screenX, int screenY, HudOffscreenTargetMarker* marker) {
	int screenWidth;
	int screenHeight;

	screenWidth = (int16_t)g_screenWidth;
	screenHeight = (int16_t)g_screenHeight;
	marker->x = screenX;
	marker->y = screenY;
	marker->spriteId = 0;
	marker->drawText = 1;
	marker->targetBehind = 0;

	if (screenY < 0) {
		marker->y = 0;
		marker->spriteId = 16600;
	} else if (screenY > screenHeight) {
		marker->y = g_screenHeight - 16;
		marker->spriteId = 16500;
	}

	if (screenX < 0) {
		marker->x = 0;
		marker->spriteId = 16800;
	} else if (screenX > screenWidth) {
		marker->x = g_screenWidth - 16;
		marker->spriteId = 16700;
	}

	if (marker->x + 16 > screenWidth) {
		marker->x = g_screenWidth - 16;
	}
	if (marker->y + 16 > screenHeight) {
		marker->y = g_screenHeight - 16;
	}

	if (marker->x == 0 && marker->y == 0) {
		marker->spriteId = 17200;
	}
	if (marker->x == screenWidth - 16 && marker->y == 0) {
		marker->spriteId = 17100;
	}
	if (marker->x == 0 && marker->y == screenHeight - 16) {
		marker->spriteId = 17000;
	}
	if (marker->x == screenWidth - 16 && marker->y == screenHeight - 16) {
		marker->spriteId = 16900;
	}
}

static void Hud_DrawOffscreenTargetMarker(const HudOffscreenTargetMarker* marker) {
	Hud_SetupResourceData(10000, marker->spriteId);
	Hud_DrawImageToDIB(marker->x, marker->y);
}

static void Hud_DrawOffscreenTargetReadout(const HudOffscreenTargetMarker* marker, int distanceWidth) {
	int nameWidth;
	int nameX;
	int nameY;
	int distanceX;
	int distanceY;
	int markerHalfWidth;
	int screenWidth;
	int screenHeight;
	int wholeDigitsWidth;

	nameWidth = FlightText_MeasureStringWidth(g_hudTargetNameText);
	screenWidth = (int16_t)g_screenWidth;
	screenHeight = (int16_t)g_screenHeight;

	if (marker->x != 0 && marker->x != screenWidth - 16) {
		if (marker->y == screenHeight - 16) {
			markerHalfWidth = (uint16_t)g_curImageWidth >> 1;
			nameX = markerHalfWidth + marker->x - (nameWidth >> 1);
			nameY = marker->y - 2 * g_flightFontLineHeight - g_hudOffscreenTargetTextMargin;
			distanceX = markerHalfWidth + marker->x - distanceWidth / 2;
			distanceY = marker->y - 2 * g_flightFontLineHeight + g_flightFontLineHeight - 2;
		} else if (marker->y == 0) {
			markerHalfWidth = (uint16_t)g_curImageWidth >> 1;
			nameX = markerHalfWidth + marker->x - (nameWidth >> 1);
			nameY = g_hudOffscreenTargetTextMargin + (uint16_t)g_curImageHeight;
			distanceX = markerHalfWidth + marker->x - distanceWidth / 2;
			distanceY = g_flightFontLineHeight + nameY + g_hudOffscreenTargetTextMargin;
		} else {
			return;
		}
	} else {
		if (marker->x != 0) {
			nameX = marker->x - g_hudOffscreenTargetTextMargin - nameWidth;
		} else {
			nameX = g_hudOffscreenTargetTextMargin + (uint16_t)g_curImageWidth;
		}
		nameY = marker->y;
		distanceX = nameX + (nameWidth >> 1) - distanceWidth / 2;
		distanceY = g_flightFontLineHeight + g_hudOffscreenTargetTextMargin + marker->y;
	}

	if (nameX < 0) {
		distanceX -= nameX;
		nameX = 0;
	}
	if (nameX + nameWidth > screenWidth) {
		distanceX += g_screenWidth - nameX - nameWidth;
		nameX = g_screenWidth - nameWidth;
	}
	if (distanceY + g_flightFontLineHeight > screenHeight) {
		nameY += g_screenHeight - distanceY - g_flightFontLineHeight;
		distanceY = g_screenHeight - g_flightFontLineHeight;
	}

	FlightText_SetCursor((int16_t)nameX, (int16_t)nameY);
	FlightText_DrawString(g_hudTargetNameText);
	FlightText_SetCursor((int16_t)distanceX, (int16_t)distanceY);
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetColor(marker->targetBehind ? 0x4Du : 0x4Eu);
	if (g_hudTargetDistanceWhole >= 10u) {
		FlightText_DrawDecimalNumber(g_hudTargetDistanceWhole, 2u, 2u);
		wholeDigitsWidth = distanceX + 2 * g_flightFontHalfHeight;
	} else {
		FlightText_DrawDecimalNumber(g_hudTargetDistanceWhole, 1u, 1u);
		wholeDigitsWidth = distanceX + g_flightFontHalfHeight;
	}
	FlightText_SetCursor((int16_t)(wholeDigitsWidth + 2), (int16_t)distanceY);
	g_flightDrawCharFn('.');
	FlightText_SetCursor((int16_t)(g_flightFontHalfHeight + wholeDigitsWidth), (int16_t)distanceY);
	FlightText_DrawDecimalNumber(g_hudTargetDistanceFrac, 2u, 2u);
}

// FUNCTION: XWA 0x471A30
void Hud_DrawOffscreenTargetIndicator2D(void) {
	CraftData* craft;
	PlayerData* player;
	HudOffscreenTargetMarker marker;
	int distanceWidth;
	int screenX;
	int screenY;

	memset(&marker, 0, sizeof(marker));
	if (g_inHangarReady) {
		return;
	}

	craft = Hud_GetCraftPointer();
	if (craft == NULL) {
		return;
	}

	FlightSw_SetRenderTarget(NULL, 0, 0, 0);
	FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetShadowEnabled(0);
	if ((craft->workingSubsystems & 4u) == 0) {
		return;
	}

	player = &g_players[g_localPlayer];
	if (player->currentTargetObjectIdx == 0xffffu) {
		return;
	}

	FlightView_ComputeObjectViewPosition((uint16_t)player->currentTargetObjectIdx);
	TRANSFM2_ProjectScreenX(viewX, viewZ);
	TRANSFM2_ProjectScreenY(viewY, viewZ);
	distanceWidth = Hud_OffscreenTargetDistanceWidth();

	if (!player->padlockActive) {
		int targetBehind;
		int projectZ;

		targetBehind = 0;
		projectZ = viewZ;
		if (projectZ < 0) {
			projectZ = -projectZ;
			targetBehind = 1;
			viewZ = -viewZ;
		}

		screenX = TRANSFM2_ProjectScreenX(viewX, projectZ);
		screenY = TRANSFM2_ProjectScreenY(viewY, viewZ);
		if (screenX >= 0 && screenX <= (int16_t)g_screenWidth && screenY >= 0 &&
			screenY <= (int16_t)g_screenHeight) {
			if (!targetBehind) {
				return;
			}
			marker.x = screenX;
			marker.y = g_reticleCenterY <= 0 ? 0 : g_screenHeight;
			marker.spriteId = marker.y != 0 ? 16600 : 16500;
			marker.drawText = 0;
			marker.targetBehind = 1;
		} else {
			Hud_ClampProjectedOffscreenMarker(screenX, screenY, targetBehind, &marker);
		}
	} else {
		if (player->lookYawOffset != 0 || player->lookPitchOffset != 0) {
			screenX =
				(int)(g_reticleCenterX - (int64_t)((double)player->lookYawOffset * 0.05405405405405406));
			screenY =
				(int)(g_reticleCenterY - (int64_t)((double)player->lookPitchOffset * -0.05405405405405406));
		} else {
			screenX = g_reticleCenterX;
			screenY = g_reticleCenterY;
		}

		if (screenX >= 0 && screenX <= (int16_t)g_screenWidth && screenY >= 0 &&
			screenY <= (int16_t)g_screenHeight) {
			return;
		}
		Hud_ClampPadlockOffscreenMarker(screenX, screenY, &marker);
	}

	Hud_DrawOffscreenTargetMarker(&marker);
	if (marker.drawText) {
		Hud_DrawOffscreenTargetReadout(&marker, distanceWidth);
	}
}

// FUNCTION: XWA 0x472DC0
void Hud_DrawFilmRecordingIndicator(void) {
#ifndef XWA_MODERN
	HudLocalTime localTime;
	char blinkVisible = 0;

	g_GetLocalTime(&localTime);
	if (localTime.milliseconds > 500u) {
		blinkVisible = 1;
	}
	if (!g_filmRecording || !blinkVisible) {
		return;
	}
#else
	if (!g_filmRecording || XwaTime_GetElapsedTicks() % 1000u <= 500u) {
		return;
	}
#endif
	HUD_PANE_PUSH(XWA_HUD_PANE_FILM_RECORDING, g_hudFilmRecTextX, g_hudFilmRecTextY,
				  g_hudFilmRecordingIndicatorSurfaceWidth, g_hudFilmRecordingIndicatorSurfaceHeight);

	if (g_useHardware3D) {
		FlightText_SetRenderOffset((int16_t)g_hudFilmRecTextX, (int16_t)g_hudFilmRecTextY);
	} else {
		FlightSw_SetRenderTarget(
			g_hudFilmRecordingIndicatorSurface, (uint16_t)g_hudFilmRecordingIndicatorSurfaceWidth,
			(uint16_t)g_hudFilmRecordingIndicatorSurfaceHeight,
			(uint16_t)g_hudFilmRecordingIndicatorSurfaceWidth * g_flight16bppBytesPerPixel);
	}

	FlightText_SetClipRect(0, 0, (uint16_t)g_hudFilmRecordingIndicatorSurfaceWidth,
						   (uint16_t)g_hudFilmRecordingIndicatorSurfaceHeight);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	if (!g_useHardware3D) {
		g_flightFillClipRectFn();
	}
	FlightText_SetColor(0x4Au);
	FlightText_SetFontTier(0);
	FlightText_SetCursor(0, 0);
	if (g_filmRecording) {
		FlightText_DrawString(g_strPanelStrings[PANEL_STRING_HUD_FILM_REC]);
	}

	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		{
			unsigned int surfaceWidth = g_hudFilmRecordingIndicatorSurfaceWidth;

			Blit16ToFlightSurface(g_hudFilmRecordingIndicatorSurface, g_flightColorEscapeBypassChar, 0, 0,
								  (uint16_t)g_hudFilmRecTextX, (uint16_t)g_hudFilmRecTextY,
								  (uint16_t)surfaceWidth, (uint16_t)g_hudFilmRecordingIndicatorSurfaceHeight,
								  surfaceWidth * g_flight16bppBytesPerPixel);
		}
	}
	HUD_PANE_POP();
}

// FUNCTION: XWA 0x472F30
void Hud_AppendObjectDisplayName(uint16_t objectOrWaypointIdx, char formatFlags) {
	uint16_t objectIdx;
	uint16_t objectType;
	int16_t craftNumber;

	g_flightTextScratchBuffer[0] = '\0';

	if (objectOrWaypointIdx >= 0x8000u) {
		if ((formatFlags & 1) != 0) {
			FlightText_AppendScratchChar(0xfe);
			FlightText_AppendScratchChar('C');
			FlightText_AppendScratchString(g_strWaypointStrings[(uint16_t)(objectOrWaypointIdx + 0x8000u)]);
		}
		return;
	}

	objectIdx = objectOrWaypointIdx;
	objectType = g_objectTable[objectIdx].objectType;
	if (g_objectTable[objectIdx].mobj != NULL) {
		if (objectType != OBJ_None) {
			char iff;

			FlightText_AppendScratchChar(0xfe);
			iff = g_objectTable[objectIdx].mobj->iff;
			if (iff == 0) {
				FlightText_AppendScratchChar('Q');
			} else if (iff == 1 || iff == 4) {
				FlightText_AppendScratchChar('I');
			} else if (iff == 2) {
				FlightText_AppendScratchChar('E');
			} else if (iff == 5) {
				FlightText_AppendScratchChar('U');
			} else {
				FlightText_AppendScratchChar('M');
			}
		} else {
			FlightText_SetScratch(g_strComponentStrings[32]);
			return;
		}

		if (g_objectTable[objectIdx].mobj->pCraft != NULL) {
			CraftData* craft;

			craft = g_objectTable[objectIdx].mobj->pCraft;
			if ((formatFlags & 1) != 0) {
				ModelIndex modelIndex;

				modelIndex = GetModelIndexFromType(objectType);
				if (modelIndex != 0xffffu) {
					modelIndex = GetModelIndexFromType(objectType);
					FlightText_AppendScratchString(g_modelDefs[modelIndex].nameLong);
				} else {
					FlightText_SetScratch(g_strComponentStrings[32]);
				}
			}

			if ((formatFlags & 4) != 0) {
				FlightText_AppendScratchChar(':');
			} else if ((formatFlags & 3) == 3) {
				FlightText_AppendScratchChar(':');
				FlightText_AppendScratchChar(' ');
			}

			if ((formatFlags & 2) != 0) {
				uint16_t flightGroupIdx;
				char numberSeparator;

				{
					char iff;

					FlightText_AppendScratchChar(0xfe);
					iff = g_objectTable[objectIdx].mobj->iff;
					if (iff == 0) {
						FlightText_AppendScratchChar('R');
					} else if (iff == 1 || iff == 4) {
						FlightText_AppendScratchChar('J');
					} else if (iff == 2) {
						FlightText_AppendScratchChar('F');
					} else if (iff == 5) {
						FlightText_AppendScratchChar('V');
					} else {
						FlightText_AppendScratchChar('N');
					}
				}
				flightGroupIdx = g_objectTable[objectIdx].flightGroupIdx;
				if ((int8_t)craft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] >= 0) {
					int numberingFlightGroupIdx;

					FlightText_AppendScratchString(g_missionFlightGroups[flightGroupIdx].fg.name);
					numberingFlightGroupIdx = flightGroupIdx;
					if (g_missionFlightGroups[numberingFlightGroupIdx].fg.disableWaveNumbering == 1 ||
						(g_missionFlightGroups[numberingFlightGroupIdx].fg.globalUnit == 0 &&
						 g_missionFlightGroups[numberingFlightGroupIdx].fg.numberOfCraft == 1 &&
						 g_missionFlightGroups[numberingFlightGroupIdx].fg.numberOfWaves == 0)) {
						craftNumber = 0;
					} else {
						craftNumber = (uint16_t)craft->craftIndexInGroup;
					}
					numberSeparator = ' ';
				} else {
					uint16_t displayFlightGroup;

					FlightText_AppendScratchString(g_strPanelStrings[PANEL_STRING_NAME]);
					displayFlightGroup = (uint16_t)(flightGroupIdx + 1u);
					if (displayFlightGroup >= 10) {
						int tens;
						int ones;

						tens = displayFlightGroup / 10;
						ones = displayFlightGroup % 10;
						FlightText_AppendScratchChar((char)('0' + tens));
						FlightText_AppendScratchChar((char)('0' + ones));
					} else {
						FlightText_AppendScratchChar((char)('0' + displayFlightGroup));
					}
					numberSeparator = '-';
					craftNumber = (uint16_t)craft->waveNumber;
					++craftNumber;
				}

				if ((uint16_t)craftNumber != 0) {
					FlightText_AppendScratchChar(numberSeparator);
					if ((uint16_t)craftNumber >= 1000u) {
						craftNumber = 999;
					}

					if ((uint16_t)craftNumber >= 100u) {
						int hundreds;
						int remainder;
						int firstDigit;
						int secondDigit;

						hundreds = (uint16_t)craftNumber / 100;
						remainder = (uint16_t)craftNumber % 100;
						firstDigit = (uint16_t)remainder / 10;
						secondDigit = (uint16_t)remainder % 10;
						FlightText_AppendScratchChar((char)('0' + hundreds));
						FlightText_AppendScratchChar((char)('0' + firstDigit));
						FlightText_AppendScratchChar((char)('0' + secondDigit));
					} else if ((uint16_t)craftNumber >= 10u) {
						int tens;
						int ones;

						tens = (uint16_t)craftNumber / 10;
						ones = (uint16_t)craftNumber % 10;
						FlightText_AppendScratchChar((char)('0' + tens));
						FlightText_AppendScratchChar((char)('0' + ones));
					} else {
						FlightText_AppendScratchChar((char)('0' + craftNumber));
					}
				}
			}
			return;
		}

		if ((formatFlags & 1) != 0) {
			if (objectType >= OBJ_WarheadTorpedo && objectType <= OBJ_WarheadFlare) {
				FlightText_AppendScratchString(g_strWarheadNames[objectType - OBJ_WarheadTorpedo]);
			} else if (objectType >= OBJ_CommSat1 && objectType <= OBJ_NavBuoy2) {
				FlightText_AppendScratchString(g_strBuoyNames[objectType - OBJ_CommSat1]);
			}
		}
		return;
	}

	if (objectType != OBJ_None) {
		char flightGroupIff;

		FlightText_AppendScratchChar(0xfe);
		flightGroupIff = (char)g_missionFlightGroups[g_objectTable[objectIdx].flightGroupIdx].fg.iff;
		if (flightGroupIff == 0) {
			FlightText_AppendScratchChar('Q');
		} else if (flightGroupIff == 1 || flightGroupIff == 4) {
			FlightText_AppendScratchChar('I');
		} else if (flightGroupIff == 2) {
			FlightText_AppendScratchChar('E');
		} else if (flightGroupIff == 5) {
			FlightText_AppendScratchChar('V');
		} else {
			FlightText_AppendScratchChar('M');
		}
		if ((formatFlags & 1) != 0 && objectType >= OBJ_CommSat1 && objectType <= OBJ_Mine3) {
			FlightText_AppendScratchString(g_strBuoyNames[objectType - OBJ_CommSat1]);
		}
		if ((formatFlags & 4) != 0) {
			FlightText_AppendScratchChar(':');
		} else if ((formatFlags & 3) == 3) {
			FlightText_AppendScratchChar(':');
			FlightText_AppendScratchChar(' ');
		}
		if ((formatFlags & 2) != 0) {
			FlightText_AppendScratchChar(0xfe);
			if (flightGroupIff == 0) {
				FlightText_AppendScratchChar('R');
			} else if (flightGroupIff == 1 || flightGroupIff == 4) {
				FlightText_AppendScratchChar('J');
			} else if (flightGroupIff == 2) {
				FlightText_AppendScratchChar('F');
			} else if (flightGroupIff == 5) {
				FlightText_AppendScratchChar('V');
			} else {
				FlightText_AppendScratchChar('N');
			}
			FlightText_AppendScratchString(
				g_missionFlightGroups[g_objectTable[objectIdx].flightGroupIdx].fg.name);
		}
	} else {
		FlightText_SetScratch(g_strComponentStrings[32]);
	}
}

// FUNCTION: XWA 0x4734A0
int Hud_MissionFG_GetCraftNumberIfShown(int flightGroupIdx, const CraftData* craft) {
	XwaFlightGroup* flightGroup;

	flightGroup = &g_missionFlightGroups[flightGroupIdx].fg;
	if (g_missionFlightGroups[flightGroupIdx].fg.disableWaveNumbering == 1 ||
		(!flightGroup->globalUnit && flightGroup->numberOfCraft == 1 && !flightGroup->numberOfWaves)) {
		return 0;
	}

	return craft->craftIndexInGroup;
}

// FUNCTION: XWA 0x4734F0
void Hud_SetHudViewState(int hudViewState, int playerIdx) {
	g_players[playerIdx].viewState.hudStateLive = (uint8_t)hudViewState;
	g_players[playerIdx].viewState.hudStateMirror = (uint8_t)hudViewState;

	if (playerIdx == g_localPlayer) {
		if (g_players[g_localPlayer].mapCameraState) {
			g_projOffsetY = 0;
		} else {
			if (g_players[g_localPlayer].objectIndex == 0xffff || hudViewState == 18) {
				g_projOffsetY = 0;
			} else {
				g_projOffsetY =
					g_modelDefs[g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft->modelIndex]
						.gunsightOffsetY;
			}
		}
		g_projOffsetYf = (float)g_projOffsetY;
	}
}

// FUNCTION: XWA 0x473590
void Hud_UpdateTargetInfoCache(void) {
	uint32_t shieldTotal;
	uint32_t shieldNumerator;
	unsigned int targetObjIdx;
	int nameFlags;

	memset(g_hudTargetNameText, 0, sizeof(g_hudTargetNameText));
	memset(g_hudTargetStatusText, 0, sizeof(g_hudTargetStatusText));
	targetObjIdx = 0;
	shieldNumerator = 0;
	shieldTotal = 0;
	g_hudTargetShieldDisplayPct = 0;
	g_hudTargetSystemDisplayPct = 0;
	g_hudTargetHullDisplayPct = 0;
	g_hudTargetDistanceWhole = 0;
	g_hudTargetDistanceFrac = 0;
	targetObjIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;

	if (targetObjIdx == 0xffffu) {
		return;
	}

	nameFlags = 3;
	if ((g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
		 g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) &&
		g_flightPlayerCount > 1) {
		MobileObject* targetMobj;

		targetMobj = g_objectTable[targetObjIdx].mobj;
		if (targetMobj != NULL && targetMobj->pCraft != NULL && !g_flightLocatePlayersEnabled) {
			int playerIff;

			playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
			if ((int8_t)targetMobj->pCraft->iffVisibility[playerIff] < 1 &&
				Object_IsHostileToTeam(targetObjIdx, playerIff) == 1 &&
				g_missionFlightGroups[g_objectTable[targetObjIdx].flightGroupIdx].fg.playerNumber) {
				nameFlags = 1;
			}
		}
	}

	Hud_AppendObjectDisplayName(targetObjIdx, nameFlags);

	if (g_objectTable[targetObjIdx].playerOwnerIdx != -1) {
		CraftData* targetCraft;
		int playerIff;

		targetCraft = g_objectTable[targetObjIdx].mobj->pCraft;
		if (g_flightLocatePlayersEnabled ||
			(playerIff = (uint16_t)g_players[g_localPlayer].playerIff,
			 (int8_t)targetCraft->iffVisibility[playerIff] > 0) ||
			!Object_IsHostileToTeam(targetObjIdx, playerIff)) {
			FlightText_AppendScratchString(" (");
			FlightText_AppendScratchString(
				NetSession_GetPlayerName(g_objectTable[targetObjIdx].playerOwnerIdx));
			FlightText_AppendScratchChar(')');
		}
	}
	strcpy(g_hudTargetNameText, g_flightTextScratchBuffer);

	if (g_objectTable[targetObjIdx].genusId != 0) {
		const char* statusText;

		statusText = g_strComponentStrings[32];
		if (g_objectTable[targetObjIdx].mobj != NULL) {
			MobileObject* targetMobj;
			CraftData* targetCraft;

			targetMobj = g_objectTable[targetObjIdx].mobj;
			targetCraft = targetMobj->pCraft;
			if (targetObjIdx >= g_activeRegionObjectSlotStart &&
				targetObjIdx < g_activeRegionCraftObjectSlotEnd && targetCraft != NULL &&
				targetMobj->state == 0) {
				if ((int8_t)targetCraft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] <= 0 ||
					targetCraft->objectKind == 3 || targetCraft->objectKind == 4) {
					statusText = g_strWarheadUnknown;
				} else {
					if (targetCraft->cargoIndex != 0xffu) {
						statusText = g_missionHeader.body.globalCargos[targetCraft->cargoIndex].name;
					} else {
						statusText = targetCraft->specialCargoName;
					}
					if (targetCraft->carriedObjectIndex == 0xffffu) {
						if (statusText[0] == '\0') {
							statusText = g_strPanelStrings[PANEL_STRING_NO_CARGO];
						}
					} else {
						CraftData* carriedCraft;
						ModelIndex modelIndex;

						if (statusText[0] != '\0') {
							strcpy(g_hudTargetStatusText, statusText);
							strcat(g_hudTargetStatusText, ", ");
						}

						if (g_objectTable[targetCraft->carriedObjectIndex].objectType == OBJ_None ||
							(modelIndex = GetModelIndexFromType(
								 g_objectTable[targetCraft->carriedObjectIndex].objectType)) == 0xffffu) {
							strcat(g_hudTargetStatusText, g_strWarheadUnknown);
						} else {
							strcat(g_hudTargetStatusText, g_modelDefs[modelIndex].nameLong);
						}
						statusText = NULL;
						strcat(g_hudTargetStatusText, " : ");
						carriedCraft = g_objectTable[targetCraft->carriedObjectIndex].mobj->pCraft;
						if (carriedCraft->cargoIndex != 0xffu) {
							strcat(g_hudTargetStatusText,
								   g_missionHeader.body.globalCargos[carriedCraft->cargoIndex].name);
						} else if (carriedCraft->specialCargoName[0] != '\0') {
							strcat(g_hudTargetStatusText, carriedCraft->specialCargoName);
						} else {
							strcat(g_hudTargetStatusText, g_strPanelStrings[PANEL_STRING_NO_CARGO]);
						}
					}
				}
			}
		}

		if (statusText != NULL) {
			strcpy(g_hudTargetStatusText, statusText);
		}
	} else {
		memset(g_hudTargetStatusText, 0, sizeof(g_hudTargetStatusText));
	}

	{
		MobileObject* targetMobj;
		CraftData* targetCraft;
		uint32_t shieldDenominator;
		uint32_t shieldPct;

		targetMobj = g_objectTable[targetObjIdx].mobj;
		if (targetMobj != NULL) {
			targetCraft = targetMobj->pCraft;
			if (targetCraft == NULL || targetCraft->objectKind == 3 || targetCraft->objectKind == 4) {
				shieldDenominator = 0;
			} else {
				shieldTotal = targetCraft->shieldRear + targetCraft->shieldFront;
				shieldNumerator = shieldTotal >> 1;
				shieldDenominator = (uint32_t)(2 * g_modelDefs[targetCraft->modelIndex].shieldStrength);
			}
			if (shieldDenominator != 0) {
				shieldPct = 2u * ((uint16_t)MATH2_percentage(shieldNumerator, shieldDenominator) / 0x28fu);
				if (shieldTotal != 0 && shieldPct == 0) {
					shieldPct = 1;
				}
			} else {
				shieldPct = 0;
			}
		} else {
			shieldPct = 0;
		}
		g_hudTargetShieldDisplayPct = shieldPct;
	}

	{
		MobileObject* targetMobj;
		CraftData* targetCraft;
		uint32_t systemPct;

		targetMobj = g_objectTable[targetObjIdx].mobj;
		if (targetMobj != NULL) {
			targetCraft = targetMobj->pCraft;
			if (targetCraft == NULL || targetCraft->objectKind == 3 || targetCraft->objectKind == 4) {
				systemPct = g_objectTable[targetObjIdx].typeSpecificWord != 0 ? 100 : 0;
			} else {
				if (targetCraft->workingSubsystems == 0) {
					systemPct = 0;
				} else {
					uint16_t subsystemDamage;
					uint16_t systemStrength;

					subsystemDamage = targetCraft->subsystemDamage;
					systemStrength = g_modelDefs[targetCraft->modelIndex].systemStrength;
					if (subsystemDamage >= systemStrength) {
						systemPct = 0;
					} else {
						systemPct = (uint16_t)MATH2_divide((uint16_t)(systemStrength - subsystemDamage),
														   systemStrength) /
									0x28fu;
					}
					if (systemPct > 25 && targetCraft->weaponFireInhibitTimer != 0) {
						systemPct = 25;
					}
				}
			}
		} else {
			systemPct = g_objectTable[targetObjIdx].typeSpecificWord != 0 ? 100 : 0;
		}
		g_hudTargetSystemDisplayPct = systemPct;
	}

	{
		MobileObject* targetMobj;
		CraftData* targetCraft;
		uint32_t hullPct;

		targetMobj = g_objectTable[targetObjIdx].mobj;
		if (targetMobj != NULL) {
			targetCraft = targetMobj->pCraft;
			if (targetObjIdx >= g_activeRegionObjectSlotStart &&
				targetObjIdx < g_activeRegionCraftObjectSlotEnd && targetCraft != NULL) {
				if (targetCraft->objectKind == 3 || targetCraft->objectKind == 4) {
					hullPct = 0;
				} else if ((uint32_t)targetCraft->hullDamage > (uint32_t)targetCraft->hullMax) {
					hullPct = 1;
				} else {
					hullPct =
						(uint16_t)MATH2_percentage((uint32_t)(targetCraft->hullMax - targetCraft->hullDamage),
												   (uint32_t)targetCraft->hullMax) /
						0x28fu;
					if (hullPct == 0) {
						hullPct = 1;
					}
				}
			} else {
				hullPct = 100;
			}
		} else {
			hullPct = 100;
		}
		g_hudTargetHullDisplayPct = hullPct;
	}

	if (g_players[g_localPlayer].mapCameraState ||
		g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].genusId != GENUS_Starship) {
		Player_ComputePolarToObjectRef(g_localPlayer,
									   (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx);
	} else {
		Object_DirectionAndDistanceToMeshCenter((uint16_t)g_players[g_localPlayer].objectIndex,
												(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx,
												(uint16_t)g_players[g_localPlayer].selectedTargetComponent);
	}

	{
		int displayDistance;
		unsigned int distanceWhole;

		displayDistance = trig2_polardistance * 161;
		trig2_polardistance = displayDistance;
		displayDistance >>= 16;
		if ((uint16_t)displayDistance >= 10000u) {
			displayDistance = 9999;
		}
		distanceWhole = (uint16_t)displayDistance / 100;
		g_hudTargetDistanceWhole = (uint16_t)distanceWhole;
		distanceWhole = g_hudTargetDistanceWhole;
		g_hudTargetDistanceFrac = (uint16_t)(displayDistance - distanceWhole * 100);
	}
}

static __inline int Hud_AbsLookDegreesFromOffset(int16_t offset) {
	int degrees;

	degrees = offset / 182;
	return abs(degrees);
}

static __inline void Hud_BeginTopTextPanel(HUD_PANE_PARAM void* surface, uint32_t width, uint32_t height,
										   int x, int y) {
	HUD_PANE_PUSH(pane, x, y, width, height);
	if (g_useHardware3D) {
		FlightText_SetRenderOffset((int16_t)x, (int16_t)y);
	} else {
		FlightSw_SetRenderTarget(surface, (uint16_t)width, (uint16_t)height,
								 (uint16_t)(width * g_flight16bppBytesPerPixel));
	}

	FlightText_SetClipRect(0, 0, (uint16_t)width, (uint16_t)height);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	if (!g_useHardware3D) {
		g_flightFillClipRectFn();
	}
	FlightText_SetColor(0x43u);
	FlightText_SetFontTier(0);
	FlightText_SetCursor(0, 0);
}

static __inline void Hud_EndTopTextPanel(void* surface, uint32_t width, uint32_t height, uint16_t dstX,
										 uint16_t dstY) {
	if (g_useHardware3D) {
		(void)surface;
		(void)width;
		(void)height;
		(void)dstX;
		(void)dstY;
		FlightText_SetRenderOffset(0, 0);
		HUD_PANE_POP();
		return;
	}

	FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
	Blit16ToFlightSurface(surface, g_flightColorEscapeBypassChar, 0, 0, dstX, dstY, (uint16_t)width,
						  (uint16_t)height, (uint16_t)(g_flight16bppBytesPerPixel * width));
	HUD_PANE_POP();
}

static __inline void Hud_DrawSpeedTextPanel(MobileObject* playerMobj) {
	uint16_t labelWidth;
	uint16_t spaceWidth;
	uint16_t speed;

	speed = (uint16_t)MATH2_fraction(playerMobj->speed, 0x71c7u);
	Hud_BeginTopTextPanel(HUD_PANE_ARG(XWA_HUD_PANE_TOP_SPEED) hudTex9, g_hudSpeedTextSurfaceWidth,
						  g_hudSpeedTextSurfaceHeight, g_hudSpeedTextX, g_hudSpeedTextY);
	FlightText_DrawString(g_strOverlayStrings[1]);
	labelWidth = FlightText_MeasureStringWidth(g_strOverlayStrings[3]);
	spaceWidth = FlightText_MeasureStringWidth(" ");
	FlightText_SetCursor((int16_t)(labelWidth + spaceWidth), 0);
	if (g_players[g_localPlayer].hyperspacePhase == PLAYER_HYPERSPACE_REGION_TRANSFER) {
		FlightText_DrawDecimalNumber(999u, 3u, 1u);
		FlightText_DrawString("+");
	} else {
		FlightText_DrawDecimalNumber(speed, 3u, 1u);
	}
	Hud_EndTopTextPanel(hudTex9, g_hudSpeedTextSurfaceWidth, g_hudSpeedTextSurfaceHeight,
						(uint16_t)(g_hudCenterX - 160), 2u);
}

static __inline void Hud_DrawThrottleTextPanel(CraftData* craft) {
	uint16_t labelWidth;
	uint16_t spaceWidth;
	uint16_t throttlePercent;

	if ((craft->activeHudFeatureMask & 0x40u) == 0) {
		return;
	}

	throttlePercent = (uint16_t)(craft->throttleSpeed / 655u);
	if (craft->slamActive != 0) {
		throttlePercent = (uint16_t)(2u * throttlePercent);
	}

	Hud_BeginTopTextPanel(HUD_PANE_ARG(XWA_HUD_PANE_TOP_THROTTLE) hudTex10, g_hudThrottleTextSurfaceWidth,
						  g_hudThrottleTextSurfaceHeight, g_hudThrottleTextX, g_hudThrottleTextY);
	labelWidth = FlightText_MeasureStringWidth(g_strOverlayStrings[3]);
	FlightText_DrawString(g_strOverlayStrings[3]);
	spaceWidth = FlightText_MeasureStringWidth(" ");
	FlightText_SetCursor((int16_t)(labelWidth + spaceWidth), 0);
	FlightText_DrawDecimalNumber(throttlePercent, 3u, 1u);
	g_flightDrawCharFn('%');
	Hud_EndTopTextPanel(hudTex10, g_hudThrottleTextSurfaceWidth, g_hudThrottleTextSurfaceHeight,
						(uint16_t)(g_hudCenterX - 160), 11u);
}

static __inline void Hud_DrawCraftNameTextPanel(void) {
	Hud_AppendObjectDisplayName((uint16_t)g_players[g_localPlayer].objectIndex, 3);
	Hud_BeginTopTextPanel(HUD_PANE_ARG(XWA_HUD_PANE_TOP_CRAFT_NAME) hudTex11, g_hudCraftNameTextSurfaceWidth,
						  g_hudCraftNameTextSurfaceHeight, g_hudCraftNameTextX, g_hudCraftNameTextY);
	FlightText_DrawString(g_flightTextScratchBuffer);
	Hud_EndTopTextPanel(hudTex11, g_hudCraftNameTextSurfaceWidth, g_hudCraftNameTextSurfaceHeight,
						(uint16_t)(g_hudCenterX + 100), 2u);
}

static __inline int Hud_ShouldUseElapsedMissionClock(void) {
	return g_provingGroundsModeActive && !g_yardContext.playerChallengeStates[g_localPlayer].finished;
}

static __inline void Hud_DrawMissionClockTextPanel(void) {
	uint16_t labelWidth;
	uint16_t spaceWidth;
	int useElapsedClock;

	Hud_BeginTopTextPanel(HUD_PANE_ARG(XWA_HUD_PANE_TOP_CLOCK) hudTex12, g_hudMissionClockTextSurfaceWidth,
						  g_hudMissionClockTextSurfaceHeight, g_hudMissionClockTextX, g_hudMissionClockTextY);
	FlightText_DrawString(g_strOverlayStrings[4]);
	labelWidth = FlightText_MeasureStringWidth(g_strOverlayStrings[4]);
	spaceWidth = FlightText_MeasureStringWidth(" ");
	useElapsedClock = Hud_ShouldUseElapsedMissionClock();
	FlightText_SetCursor((int16_t)(labelWidth + spaceWidth), 0);
	if (!g_missionTimeLimitActive || useElapsedClock) {
		FlightText_DrawDecimalNumber(g_missionElapsedClock.minutes, 2u, 1u);
	} else {
		FlightText_DrawDecimalNumber(g_missionCountdownClock.minutes, 2u, 2u);
	}
	g_flightDrawCharFn(':');
	if (!g_missionTimeLimitActive || useElapsedClock) {
		FlightText_DrawDecimalNumber(g_missionElapsedClock.seconds, 2u, 2u);
	} else {
		FlightText_DrawDecimalNumber(g_missionCountdownClock.seconds, 2u, 2u);
	}
	Hud_EndTopTextPanel(hudTex12, g_hudMissionClockTextSurfaceWidth, g_hudMissionClockTextSurfaceHeight,
						(uint16_t)g_hudMissionClockTextX, (uint16_t)g_hudMissionClockTextY);
}

static __inline void Hud_DrawProvingGroundStatusTextPanel(void) {
	YardPlayerChallengeState* state;
	uint16_t nowSeconds;

	if (!g_provingGroundsModeActive) {
		return;
	}

	state = &g_yardContext.playerChallengeStates[g_localPlayer];
	nowSeconds = (uint16_t)Mission_GameTimeToSeconds(
		g_missionElapsedClock.hours, g_missionElapsedClock.minutes, g_missionElapsedClock.seconds);
	Hud_BeginTopTextPanel(HUD_PANE_ARG(XWA_HUD_PANE_TOP_PROVING_STATUS) hudTex15,
						  g_hudProvingGroundStatusTextSurfaceWidth, g_hudProvingGroundStatusTextSurfaceHeight,
						  g_hudProvingGroundStatusTextX, g_hudProvingGroundStatusTextY);
	if (state->penaltyUntilSeconds <= nowSeconds) {
		if (state->finished) {
			int minutes;
			int seconds;

			minutes = state->finishTimeSeconds / 60;
			seconds = state->finishTimeSeconds % 60;
			FlightText_SetColor(0x4Bu);
			FlightText_DrawString(g_strHangarMiscStrings[HANGAR_MISC_COMPLETED]);
			FlightText_DrawDecimalNumber((uint16_t)minutes, 2u, 1u);
			g_flightDrawCharFn(':');
			FlightText_DrawDecimalNumber((uint16_t)seconds, 2u, 2u);
		} else if (g_yardChallengeMode >= 6u) {
			if (state->carriedObjectPickedUp) {
				FlightText_SetColor(0x4Bu);
				FlightText_DrawString(g_strHangarMiscStrings[HANGAR_MISC_YARD_HUD_GOT_R2]);
			}
		} else {
			FlightText_SetColor(0x4Au);
			FlightText_DrawString(g_strHangarMiscStrings[HANGAR_MISC_YARD_HUD_RINGS]);
			FlightText_DrawDecimalNumber((uint16_t)state->remainingCheckpointCount, 3u, 1u);
			FlightText_DrawString(g_strHangarMiscStrings[HANGAR_MISC_YARD_HUD_LAPS]);
			FlightText_DrawDecimalNumber((uint16_t)state->lapsRemaining, 3u, 1u);
		}
	} else {
		FlightText_SetColor(0x43u);
		FlightText_DrawString(g_strHangarMiscStrings[HANGAR_MISC_YARD_HUD_PENALTY]);
		FlightText_DrawString(":  ");
		FlightText_DrawDecimalNumber((uint16_t)((uint16_t)state->penaltyUntilSeconds - nowSeconds), 4u, 1u);
	}
	FlightText_SetColor(0x43u);
	Hud_EndTopTextPanel(hudTex15, g_hudProvingGroundStatusTextSurfaceWidth,
						g_hudProvingGroundStatusTextSurfaceHeight, 0x113u, 11u);
}

void Hud_DrawRadarBlips(void) {
	void(XWA_HUD_STDCALL * debugOutput)(const char*);
	CraftData* playerCraft;
	uint32_t objIdx;

	debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
#ifdef XWA_MODERN
	XwaSnapshotHud_BeginRadar(g_radarEllipseClampRadius);
#endif
	playerCraft = Hud_GetCraftPointerInlinedWithDebug(debugOutput);
	if (playerCraft == NULL) {
		debugOutput("NULL craft data pointer in DrawRadarBlips()!\n");
		return;
	}

	if ((g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft->activeHudFeatureMask & 0x80u) ==
			0 ||
		(g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft->activeHudFeatureMask & 0x100u) ==
			0) {
		return;
	}

	g_radarForePrevBlipCount = g_radarForeBlipCount.lowWord;
	g_radarAftPrevBlipCount = g_radarAftBlipCount.lowWord;
	g_radarTargetMarkerRestoreX = g_radarTargetMarkerDrawX;
	g_radarAftBlipCount.lowWord = 0;
	g_radarForeBlipCount.lowWord = 0;
	g_radarTargetMarkerRestoreY = g_radarTargetMarkerDrawY;
	if (g_radarBlipBufferParity) {
		g_radarForeEraseBlips = g_radarForeBlipBufferB;
		g_radarForeDrawBlips = g_radarForeBlipBufferA;
		g_radarAftEraseBlips = g_radarAftBlipBufferB;
		g_radarAftDrawBlips = g_radarAftBlipBufferA;
	} else {
		g_radarForeEraseBlips = g_radarForeBlipBufferA;
		g_radarForeDrawBlips = g_radarForeBlipBufferB;
		g_radarAftEraseBlips = g_radarAftBlipBufferA;
		g_radarAftDrawBlips = g_radarAftBlipBufferB;
	}

	for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
		CraftData* craft;
		uint8_t objectKind;

		if (objIdx == (uint32_t)g_players[g_localPlayer].objectIndex ||
			(g_modelTypeTable[g_objectTable[objIdx].objectType].flags &
			 MODEL_TYPE_FLAG_FILM_OVERLAY_SELECTABLE) == 0) {
			continue;
		}

		craft = g_objectTable[objIdx].mobj->pCraft;
		if (objIdx == (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx) {
			Hud_AddBlipToRadar((uint16_t)objIdx);
			continue;
		}
		if (Object_HasActiveDecoyBeam((uint16_t)objIdx)) {
			continue;
		}
		objectKind = craft->objectKind;
		if (objectKind == 3 || objectKind == 4) {
			continue;
		}

		Hud_AddBlipToRadar((uint16_t)objIdx);
	}

	for (objIdx = g_projectileObjectSlotStart; objIdx < g_projectileObjectSlotEnd; ++objIdx) {
		if ((g_modelTypeTable[(uint16_t)g_objectTable[objIdx].objectType].flags &
			 MODEL_TYPE_FLAG_FILM_OVERLAY_SELECTABLE) != 0) {
			Hud_AddBlipToRadar((uint16_t)objIdx);
		}
	}

	for (objIdx = g_objScanStart; objIdx < g_regionStaticObjectSlotEnd; ++objIdx) {
		if ((g_modelTypeTable[(uint16_t)g_objectTable[objIdx].objectType].flags &
			 MODEL_TYPE_FLAG_FILM_OVERLAY_SELECTABLE) != 0) {
			Hud_AddBlipToRadar((uint16_t)objIdx);
		}
	}

	if (g_provingGroundsModeActive) {
		for (objIdx = g_salvageJunkObjectSlotStart; objIdx < g_debrisObjectSlotStart; ++objIdx) {
			if ((g_modelTypeTable[(uint16_t)g_objectTable[objIdx].objectType].flags &
				 MODEL_TYPE_FLAG_FILM_OVERLAY_SELECTABLE) != 0) {
				Hud_AddBlipToRadar((uint16_t)objIdx);
			}
		}
	}

	if (g_radarTargetMarkerBackgroundSaved && !g_useHardware3D) {
		g_flightRestoreRadarTargetMarkerFn();
	}
	if (g_radarForePrevBlipCount && !g_useHardware3D) {
		g_flightDrawPointArrayMaskedFn(&g_radarForeEraseBlips->x, g_radarForePrevBlipCount);
	}
	if (g_radarForeBlipCount.lowWord) {
		g_flightDrawPointArrayFn(&g_radarForeDrawBlips->x, g_radarForeBlipCount.lowWord);
	}
	if (g_radarAftPrevBlipCount && !g_useHardware3D) {
		g_flightDrawPointArrayMaskedFn(&g_radarAftEraseBlips->x, g_radarAftPrevBlipCount);
	}
	if (g_radarAftBlipCount.lowWord) {
		g_flightDrawPointArrayFn(&g_radarAftDrawBlips->x, g_radarAftBlipCount.lowWord);
	}

	if (g_players[g_localPlayer].currentTargetObjectIdx != 0xffffu) {
		g_flightDrawRadarTargetMarkerFn();
		g_radarTargetMarkerBackgroundSaved = 1;
	} else {
		g_radarTargetMarkerBackgroundSaved = 0;
	}
	g_radarBlipBufferParity ^= 1u;
}

// FUNCTION: XWA 0x475DB0
void Hud_AddBlipToRadar(uint16_t targetObjIdx) {
	int playerObjIdx;
	int relWorldX;
	int relWorldY;
	int relWorldZ;
	int radarRelX;
	int radarRelY;
	int depth;
	int color;
	char foreRadar;

	playerObjIdx = g_players[g_localPlayer].objectIndex;
	color = 0;
	{
		ObjectRecord* relativeObj = &g_objectTable[targetObjIdx];

		relWorldX = relativeObj->world_x - g_objectTable[playerObjIdx].world_x;
		relWorldY = relativeObj->world_y - g_objectTable[playerObjIdx].world_y;
		relWorldZ = relativeObj->world_z - g_objectTable[playerObjIdx].world_z;
	}
	if (g_objectTable[playerObjIdx].mobj->orientMatrixDirty) {
		FVIEW_calcrotatemove(g_objectTable[playerObjIdx].pitch, g_objectTable[playerObjIdx].yaw,
							 &g_objectTable[playerObjIdx]);
		FVIEW_calcrotateorient(g_objectTable[playerObjIdx].roll, g_objectTable[playerObjIdx].angleD,
							   &g_objectTable[playerObjIdx]);
	}

	if (g_players[g_localPlayer].currentSeatIdx) {
		depth = Xwa_Q15MulReuseFirstSlot(relWorldX, g_players[g_localPlayer].turretCamMat[0]) +
				Xwa_Q15MulReuseFirstSlot(relWorldY, g_players[g_localPlayer].turretCamMat[1]) +
				Xwa_Q15MulReuseFirstSlot(relWorldZ, g_players[g_localPlayer].turretCamMat[2]);
		radarRelX = Xwa_Q15MulReuseFirstSlot(relWorldX, g_players[g_localPlayer].turretCamMat[3]) +
					Xwa_Q15MulReuseFirstSlot(relWorldY, g_players[g_localPlayer].turretCamMat[4]) +
					Xwa_Q15MulReuseFirstSlot(relWorldZ, g_players[g_localPlayer].turretCamMat[5]);
		radarRelY = Xwa_Q15MulReuseFirstSlot(relWorldX, g_players[g_localPlayer].turretCamMat[6]) +
					Xwa_Q15MulReuseFirstSlot(relWorldY, g_players[g_localPlayer].turretCamMat[7]) +
					Xwa_Q15MulReuseFirstSlot(relWorldZ, g_players[g_localPlayer].turretCamMat[8]);
	} else {
		depth = Xwa_Q15MulReuseFirstSlot(relWorldX, g_objectTable[playerObjIdx].mobj->cachedFwdX) +
				Xwa_Q15MulReuseFirstSlot(relWorldY, g_objectTable[playerObjIdx].mobj->cachedFwdY) +
				Xwa_Q15MulReuseFirstSlot(relWorldZ, g_objectTable[playerObjIdx].mobj->cachedFwdZ);
		radarRelX = Xwa_Q15MulReuseFirstSlot(relWorldX, g_objectTable[playerObjIdx].mobj->cachedSideX) +
					Xwa_Q15MulReuseFirstSlot(relWorldY, g_objectTable[playerObjIdx].mobj->cachedSideY) +
					Xwa_Q15MulReuseFirstSlot(relWorldZ, g_objectTable[playerObjIdx].mobj->cachedSideZ);
		radarRelY = -(Xwa_Q15MulReuseFirstSlot(relWorldX, g_objectTable[playerObjIdx].mobj->cachedUpX) +
					  Xwa_Q15MulReuseFirstSlot(relWorldY, g_objectTable[playerObjIdx].mobj->cachedUpY) +
					  Xwa_Q15MulReuseFirstSlot(relWorldZ, g_objectTable[playerObjIdx].mobj->cachedUpZ));
	}

	if (depth < 0) {
		depth = -depth;
		foreRadar = 0;
	} else {
		foreRadar = 1;
	}

	if (g_objectTable[targetObjIdx].mobj == NULL) {
		color = 47;
	} else {
		if (g_objectTable[targetObjIdx].mobj->state == 1) {
			if (((g_missionElapsedClock.subsecondTicks / 4) & 1) != 0) {
				color = 59;
			} else {
				color = 55;
			}
		} else {
			switch ((uint8_t)g_objectTable[targetObjIdx].mobj->iff) {
				case 0:
					color = 63;
					break;
				case 1:
				case 4:
					color = 55;
					break;
				case 2:
					color = 51;
					break;
				case 3:
					color = 59;
					break;
				case 5:
					color = 211;
					break;
				default:
					break;
			}
		}
	}

	pai_ObjectRefUpdateApproxRangeScore(g_players[g_localPlayer].objectIndex, targetObjIdx);
	if ((unsigned int)g_targetRangeScore > 0x1dd36u) {
		if ((uint16_t)color == 47u) {
			color = 45;
		} else if ((uint16_t)color == 211u) {
			color = 213;
		} else {
			color += 0xfffe;
		}
	} else if ((unsigned int)g_targetRangeScore > 0xee9bu) {
		if ((uint16_t)color == 47u) {
			color = 46;
		} else if ((uint16_t)color == 211u) {
			color = 212;
		} else {
			color += 0xffff;
		}
	}

	MATH2_getradarcoord(radarRelX, radarRelY, depth);
	if (foreRadar && g_hudElementEnabled[1].enabled) {
		uint16_t blipX;

#ifdef XWA_MODERN
		uint16_t blipIdx = g_radarForeBlipCount.lowWord;
		if (blipIdx < 47u) {
			XwaSnapshotHud_NoteRadarBlip(targetObjIdx, g_objectTable[targetObjIdx].objectSignature, 0,
										 targetObjIdx ==
											 (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx,
										 radarx, radary, (uint16_t)color);
		}
		if (targetObjIdx == (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx) {
			XwaSnapshotHud_NoteRadarTargetMarker(0, radarx, radary);
		}
#endif
		blipX = (uint16_t)(g_hudRadarCenterOffsetX + radarx);
		radary = (int16_t)(radary + g_hudRadarCenterY);
		radarx = (int16_t)(radarx + g_hudRadarCenterOffsetX);
		if (radary < 0) {
			radary = 0;
		}
		g_radarForeDrawBlips[(uint16_t)g_radarForeBlipCount.value].x = blipX;
		g_radarForeDrawBlips[(uint16_t)g_radarForeBlipCount.value].y = (uint16_t)radary;
		g_radarForeDrawBlips[(uint16_t)g_radarForeBlipCount.value].color = (uint16_t)color;
		g_radarForeBlipCount.lowWord += 1;
		if (g_radarForeBlipCount.lowWord == 48u) {
			g_radarForeBlipCount.lowWord = 47;
		}
	} else if (!foreRadar && g_hudElementEnabled[2].enabled) {
#ifdef XWA_MODERN
		uint16_t blipIdx = g_radarAftBlipCount.lowWord;
		if (blipIdx < 47u) {
			XwaSnapshotHud_NoteRadarBlip(targetObjIdx, g_objectTable[targetObjIdx].objectSignature, 1,
										 targetObjIdx ==
											 (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx,
										 radarx, radary, (uint16_t)color);
		}
		if (targetObjIdx == (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx) {
			XwaSnapshotHud_NoteRadarTargetMarker(1, radarx, radary);
		}
#endif
		radarx = (int16_t)(radarx + g_screenWidth - g_hudRadarCenterOffsetX);
		radary = (int16_t)(radary + g_hudRadarCenterY);
		if (radary < 0) {
			radary = 0;
		}
		g_radarAftDrawBlips[(uint16_t)g_radarAftBlipCount.value].x = (uint16_t)radarx;
		g_radarAftDrawBlips[(uint16_t)g_radarAftBlipCount.value].y = (uint16_t)radary;
		g_radarAftDrawBlips[(uint16_t)g_radarAftBlipCount.value].color = (uint16_t)color;
		g_radarAftBlipCount.lowWord += 1;
		if (g_radarAftBlipCount.lowWord == 48u) {
			g_radarAftBlipCount.lowWord = 47;
		}
	}

	if ((uint16_t)g_players[g_localPlayer].currentTargetObjectIdx == targetObjIdx) {
		if ((foreRadar && !g_hudElementEnabled[1].enabled) ||
			(!foreRadar && !g_hudElementEnabled[2].enabled)) {
			int invalidCoord = 0xffff;

			g_radarTargetMarkerDrawX = (int16_t)invalidCoord;
			g_radarTargetMarkerDrawY = (int16_t)invalidCoord;
			return;
		}
		g_radarTargetMarkerDrawX = radarx;
		g_radarTargetMarkerDrawY = radary;
	}
}

// FUNCTION: XWA 0x476490
void Hud_UpdateShieldPercentLabels(void) {
	MobileObject** playerMobileObject;
	CraftData* craft;
	int halfMaxShield;
	int shieldFront;
	int shieldRear;
	uint16_t frontPercent;
	uint16_t rearPercent;

	playerMobileObject = &g_objectTable[g_players[g_localPlayer].objectIndex].mobj;
	if (((*playerMobileObject)->pCraft->systemFlags & 1u) == 0) {
		return;
	}

	if (!g_hudShieldPercentLabelsInitialized) {
		g_hudShieldFrontPercentCached = 0xffffu;
		g_hudShieldRearPercentCached = 0xffffu;
		g_hudShieldPercentLabelsInitialized = 1;
	}

	if (((*playerMobileObject)->pCraft->activeHudFeatureMask & 0x20u) == 0) {
		return;
	}
	HUD_PANE_PUSH(XWA_HUD_PANE_LEFT_SUBSYSTEM, 0, 0, (int)(40.0f * g_flightHudScaleFactor),
				  (int)(200.0f * g_flightHudScaleFactor));

	FlightText_SetClipRect(0, 0,
						   (uint16_t)(int)((double)(uint16_t)g_hudShieldPercentTextSurfaceWidth *
										   (double)g_flightHudScaleFactor),
						   (uint16_t)(int)((double)(uint16_t)g_hudShieldPercentTextSurfaceHeight *
										   (double)g_flightHudScaleFactor));
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetColor(0x43u);
	FlightText_SetShadowEnabled(1u);
	FlightText_SetShadowColor(0x40u);

	halfMaxShield = Craft_GetObjectMaxShield((uint16_t)g_players[g_localPlayer].objectIndex) / 2;

	craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
	shieldFront = craft->shieldFront;
	if (shieldFront < 0) {
		shieldFront = 0;
	}
	if ((craft->workingSubsystems & 1u) == 0) {
		shieldFront = 0;
	}

	if (shieldFront >= halfMaxShield) {
		frontPercent =
			(uint16_t)MATH2_percentage((uint32_t)(shieldFront - halfMaxShield), (uint32_t)halfMaxShield);
		frontPercent /= 655;
		frontPercent += 100;
	} else {
		frontPercent = (uint16_t)MATH2_percentage((uint32_t)shieldFront, (uint32_t)halfMaxShield);
		(void)MATH2_longfraction(9u, (uint16_t)frontPercent);
		frontPercent = (uint16_t)frontPercent / 655;
	}

	if (frontPercent != g_hudShieldFrontPercentCached || g_useHardware3D) {
		if (g_useHardware3D) {
			FlightText_SetRenderOffset((int16_t)g_hudFrontShieldPercentTextX,
									   (int16_t)g_hudFrontShieldPercentTextY);
		} else {
			FlightSw_SetRenderTarget(hudTex7, (uint16_t)g_hudShieldPercentTextSurfaceWidth,
									 (uint16_t)g_hudShieldPercentTextSurfaceHeight,
									 (uint16_t)g_hudShieldPercentTextSurfaceWidth *
										 g_flight16bppBytesPerPixel);
		}
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_SetCursor(0, 0);
		FlightText_FormatScratchInt(frontPercent);
		FlightText_AppendScratchChar('%');
		FlightText_DrawStringRightAligned(g_flightTextScratchBuffer);
		g_hudShieldFrontPercentCached = frontPercent;
	}

	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		uint32_t width;

		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		width = g_hudShieldPercentTextSurfaceWidth;
		Blit16ToFlightSurface(hudTex7, g_flightColorEscapeBypassChar, 0, 0,
							  (uint16_t)g_hudFrontShieldPercentTextX, (uint16_t)g_hudFrontShieldPercentTextY,
							  (uint16_t)width, (uint16_t)g_hudShieldPercentTextSurfaceHeight,
							  width * g_flight16bppBytesPerPixel);
	}

	craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
	shieldRear = craft->shieldRear;
	if (shieldRear < 0) {
		shieldRear = 0;
	}
	if ((craft->workingSubsystems & 1u) == 0) {
		shieldRear = 0;
	}

	if (shieldRear >= halfMaxShield) {
		rearPercent =
			(uint16_t)MATH2_percentage((uint32_t)(shieldRear - halfMaxShield), (uint32_t)halfMaxShield);
		rearPercent /= 655;
		rearPercent += 100;
	} else {
		rearPercent = (uint16_t)MATH2_percentage((uint32_t)shieldRear, (uint32_t)halfMaxShield);
		(void)MATH2_longfraction(9u, (uint16_t)rearPercent);
		rearPercent = (uint16_t)rearPercent / 655;
	}

	if (rearPercent != g_hudShieldRearPercentCached || g_useHardware3D) {
		if (g_useHardware3D) {
			FlightText_SetRenderOffset((int16_t)g_hudRearShieldPercentTextX,
									   (int16_t)g_hudRearShieldPercentTextY);
		} else {
			FlightSw_SetRenderTarget(hudTex8, (uint16_t)g_hudShieldPercentTextSurfaceWidth,
									 (uint16_t)g_hudShieldPercentTextSurfaceHeight,
									 (uint16_t)g_hudShieldPercentTextSurfaceWidth *
										 g_flight16bppBytesPerPixel);
		}
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_SetCursor(0, 0);
		FlightText_FormatScratchInt(rearPercent);
		FlightText_AppendScratchChar('%');
		FlightText_DrawStringRightAligned(g_flightTextScratchBuffer);
		g_hudShieldRearPercentCached = rearPercent;
	}

	FlightText_SetShadowEnabled(0);
	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		uint32_t width;

		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		width = g_hudShieldPercentTextSurfaceWidth;
		Blit16ToFlightSurface(hudTex8, g_flightColorEscapeBypassChar, 0, 0,
							  (uint16_t)g_hudRearShieldPercentTextX, (uint16_t)g_hudRearShieldPercentTextY,
							  (uint16_t)width, (uint16_t)g_hudShieldPercentTextSurfaceHeight,
							  width * g_flight16bppBytesPerPixel);
	}
	HUD_PANE_POP();
}

// FUNCTION: XWA 0x476940
void Hud_UpdateWarheadCnt(void) {
	void(XWA_HUD_STDCALL * debugOutput)(const char*);
	CraftData* craft;
	uint16_t objectType;
	int16_t modelIndex;

	debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
	craft = Hud_GetCraftPointerInlinedWithDebug(debugOutput);
	if (craft == NULL) {
		debugOutput("NULL Craft Data Pointer in UpdateWarheadCnt()!\n");
		return;
	}

	if ((craft->activeHudFeatureMask & 8u) == 0) {
		return;
	}

	if (!craft->warheadLauncherCount) {
		return;
	}

	objectType = g_objectTable[g_players[g_localPlayer].objectIndex].objectType;
	if (!objectType) {
		return;
	}

	modelIndex = GetModelIndexFromType(objectType);
	if (modelIndex == -1) {
		return;
	}

	if (g_players[g_localPlayer].selectedWeaponMode != g_hudWeaponModeWarhead) {
		return;
	}

	if ((uint16_t)(g_modelDefs[(uint16_t)modelIndex].warheadLauncherSlotCount[1] +
				   g_modelDefs[(uint16_t)modelIndex].warheadLauncherSlotCount[0]) == 0) {
		return;
	}

	if (!g_players[g_localPlayer].selectedWarhead) {
		Hud_OutputWarheadCount(g_modelDefs[(uint16_t)modelIndex].warheadLauncherFirstSlot[0], 0, 0);
		Hud_OutputWarheadCount(g_modelDefs[(uint16_t)modelIndex].warheadLauncherLastSlot[0], 1, 0);
	} else {
		Hud_OutputWarheadCount(g_modelDefs[(uint16_t)modelIndex].warheadLauncherFirstSlot[1], 2, 1);
		Hud_OutputWarheadCount(g_modelDefs[(uint16_t)modelIndex].warheadLauncherLastSlot[1], 3, 1);
	}
}

// FUNCTION: XWA 0x476AA0
void Hud_OutputWarheadCount(uint16_t warheadSlotIdx, int16_t displaySlot, int16_t warheadBank) {
	void(XWA_HUD_STDCALL * debugOutput)(const char*);
	int16_t padlockYaw;
	int16_t padlockPitch;
	uint16_t objectType;
	int16_t modelIndex;
	int16_t missileBoatModelIndex;
	uint16_t leftDstX;
	uint16_t rightDstX;
	uint16_t dstY;
	CraftData* craft;
	uint16_t count;
	int16_t highlightTier;
	uint8_t warheadFlags;

	debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
	craft = Hud_GetCraftPointerInlinedWithDebug(debugOutput);
	if (craft == NULL) {
		debugOutput("NULL Craft data pointer in OutputWarheadCount()!\n");
		return;
	}

	padlockYaw = g_players[g_localPlayer].lookYawOffset;
	if (Hud_AbsLookDegreesFromOffset(padlockYaw) >= 10) {
		return;
	}

	padlockPitch = g_players[g_localPlayer].lookPitchOffset;
	if (Hud_AbsLookDegreesFromOffset(padlockPitch) >= 10) {
		return;
	}

	if (padlockYaw || padlockPitch) {
		int worldZ;

		pai_RotateLocalVectorToWorldScratch(&g_objectTable[g_players[g_localPlayer].objectIndex], 0, 0,
											1000000);
		g_camRelWorldX = g_objectTable[g_players[g_localPlayer].objectIndex].world_x + g_rotatedX;
		g_camRelWorldY = g_objectTable[g_players[g_localPlayer].objectIndex].world_y + g_rotatedY;
		worldZ = g_objectTable[g_players[g_localPlayer].objectIndex].world_z + g_rotatedZ;
		g_camRelWorldX -= g_players[g_localPlayer].viewState.savedTargetX;
		g_camRelWorldY -= g_players[g_localPlayer].viewState.savedTargetY;
		g_camRelWorldZ = worldZ - g_players[g_localPlayer].viewState.savedTargetZ;
		viewX = TRANSFM2_CamMatDotRow0(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
		viewY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
		viewZ = TRANSFM2_CamMatDotRow2(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
		g_reticleDrawX = TRANSFM2_ProjectScreenX(viewX, viewZ);
		g_reticleDrawY = TRANSFM2_ProjectScreenY(viewY, viewZ);
	}

	objectType = g_objectTable[g_players[g_localPlayer].objectIndex].objectType;
	if (!objectType) {
		return;
	}

	modelIndex = GetModelIndexFromType(objectType);
	if (modelIndex == -1) {
		return;
	}

	missileBoatModelIndex = GetModelIndexFromType(OBJ_MissileBoat);
	if (missileBoatModelIndex == -1) {
		return;
	}

	leftDstX = (uint16_t)(g_reticleDrawX - g_hudWarheadCountLeftReticleOffsetX);
	rightDstX = (uint16_t)(g_hudWarheadCountRightReticleOffsetX + g_reticleDrawX);
	dstY = (uint16_t)(g_reticleDrawY + g_hudWarheadCountReticleOffsetY);
	if (displaySlot == 0 || displaySlot == 2) {
		if (g_useHardware3D) {
			FlightText_SetRenderOffset((int16_t)(g_reticleDrawX - g_hudWarheadCountLeftReticleOffsetX),
									   (int16_t)(g_reticleDrawY + g_hudWarheadCountReticleOffsetY));
		} else {
			FlightSw_SetRenderTarget(hudTex4, (uint16_t)g_hudReticleWarheadCountSurfaceWidth,
									 (uint16_t)g_hudReticleWarheadCountSurfaceHeight,
									 (uint16_t)g_hudReticleWarheadCountSurfaceWidth *
										 g_flight16bppBytesPerPixel);
		}
	} else if (g_useHardware3D) {
		FlightText_SetRenderOffset((int16_t)(g_hudWarheadCountRightReticleOffsetX + g_reticleDrawX),
								   (int16_t)(g_reticleDrawY + g_hudWarheadCountReticleOffsetY));
	} else {
		FlightSw_SetRenderTarget(hudTex5, (uint16_t)g_hudReticleWarheadCountSurfaceWidth,
								 (uint16_t)g_hudReticleWarheadCountSurfaceHeight,
								 (uint16_t)g_hudReticleWarheadCountSurfaceWidth * g_flight16bppBytesPerPixel);
	}

	craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
	count = craft->warheadData[warheadSlotIdx].count;
	if (!count) {
		return;
	}
	HUD_RELATIVE_PANE_PUSH(XWA_HUD_PANE_RETICLE_COUNTS, g_reticleDrawX, g_reticleDrawY,
						   g_hudReticleWarheadCountSurfaceWidth, g_hudReticleWarheadCountSurfaceHeight);

	if (g_players[g_localPlayer].selectedWeaponMode != 1) {
		highlightTier = 1;
	} else if (g_players[g_localPlayer].selectedWarhead != warheadBank) {
		highlightTier = 1;
	} else {
		warheadFlags = craft->warheadLauncherFlags[g_players[g_localPlayer].selectedWarhead];
		if ((warheadFlags & 0x7fu) == 3) {
			highlightTier = 2;
		} else {
			highlightTier = (int16_t)(2 + (((warheadFlags >> 7) == (displaySlot & 1)) ? 0 : -1));
		}
	}

	FlightText_SetClipRect(0, 0, (uint16_t)g_hudReticleWarheadCountSurfaceWidth,
						   (uint16_t)g_hudReticleWarheadCountSurfaceHeight);
	FlightText_SetCursor(0, 0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	if (!g_useHardware3D) {
		g_flightFillClipRectFn();
	}
	if (highlightTier == 2) {
		FlightText_SetColor(0x52u);
	} else {
		FlightText_SetColor(0x4Au);
	}
	g_flightTextShadowEnabled = 0;
	FlightText_SetFontTier(0);
	if (modelIndex != missileBoatModelIndex) {
		FlightText_DrawDecimalNumber(count, 1u, 1u);
	} else {
		FlightText_DrawDecimalNumber(count, 2u, 1u);
	}

	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		if (displaySlot == 0 || displaySlot == 2) {
			uint16_t surfaceWidth;
			uint16_t sourcePitch;

			surfaceWidth = (uint16_t)g_hudReticleWarheadCountSurfaceWidth;
			sourcePitch = (uint16_t)(surfaceWidth * g_flight16bppBytesPerPixel);
			Blit16ToFlightSurface(hudTex4, g_flightColorEscapeBypassChar, 0, 0, leftDstX, dstY, surfaceWidth,
								  (uint16_t)g_hudReticleWarheadCountSurfaceHeight, sourcePitch);
		} else {
			uint16_t surfaceWidth;
			uint16_t sourcePitch;

			surfaceWidth = (uint16_t)g_hudReticleWarheadCountSurfaceWidth;
			sourcePitch = (uint16_t)(surfaceWidth * g_flight16bppBytesPerPixel);
			Blit16ToFlightSurface(hudTex5, g_flightColorEscapeBypassChar, 0, 0, rightDstX, dstY, surfaceWidth,
								  (uint16_t)g_hudReticleWarheadCountSurfaceHeight, sourcePitch);
		}
	}
	HUD_PANE_POP();
}

static void Hud_DrawThreatIndicatorSprite(uint16_t spriteId, int offsetX, int offsetY) {
	int drawX;
	int drawY;

	Hud_SetupResourceData(10000, spriteId);
	drawX = g_reticleDrawX - ((uint16_t)g_curImageWidth >> 1) + offsetX;
	drawY = g_reticleDrawY - ((uint16_t)g_curImageHeight >> 1) + offsetY;
	Hud_DrawImageToDIB(drawX, drawY);
}

// FUNCTION: XWA 0x476FB0
void Hud_UpdateThreatIndicators(void) {
	uint16_t laserThreatState;
	uint16_t turretThreatState;
	uint16_t beamThreatState;
	int localPlayerObjIdx;
	uint32_t objectIdx;
	int cannonIdx;
	int laserSlotIdx;

	if (g_players[g_localPlayer].hyperspacePhase != PLAYER_HYPERSPACE_PHASE_NONE) {
		return;
	}

	localPlayerObjIdx = g_players[g_localPlayer].objectIndex;
	laserThreatState = 0;
	turretThreatState = 0;
	beamThreatState = 0;
	for (objectIdx = g_activeRegionObjectSlotStart; objectIdx < g_activeRegionCraftObjectSlotEnd;
		 ++objectIdx) {
		MobileObject* mobj;
		CraftData* craft;

		if (g_objectTable[objectIdx].objectType == OBJ_None) {
			continue;
		}

		mobj = g_objectTable[objectIdx].mobj;
		if (mobj->state != 0) {
			continue;
		}

		craft = mobj->pCraft;
		if (craft->workingSubsystems == 0 || craft->objectKind != 0) {
			continue;
		}

		if (g_objectTable[objectIdx].playerOwnerIdx == -1) {
			AiController* ai;

			ai = pai_GetEffectiveAIController(craft);
			if (ai->targetObjIdx == localPlayerObjIdx && (ai->maneuverMode == 12 || ai->maneuverMode == 23)) {
				for (cannonIdx = 0; cannonIdx < craft->cannonClassCount; ++cannonIdx) {
					if ((craft->laserProjectileTypeId[cannonIdx] == OBJ_LaserImperial ||
						 craft->laserProjectileTypeId[cannonIdx] == OBJ_LaserRebel) &&
						craft->laserLinkMode[cannonIdx] != 0) {
						laserThreatState = 1;
					}
				}

				if (craft->beamActive != 0 && craft->beamPresent != 0) {
					uint8_t beamTypeId;

					beamTypeId = craft->beamTypeId;
					if (beamTypeId > 0u) {
						if (beamTypeId < 3u) {
							beamThreatState = beamTypeId;
						}
					}
				}
			}
		} else {
			int playerOwnerIdx;
			CraftData* localCraft;

			localCraft = g_objectTable[localPlayerObjIdx].mobj->pCraft;
			if (localCraft->lastAttackerObjIdx == objectIdx) {
				int currentSeconds;

				currentSeconds =
					Mission_GameTimeToSeconds(g_missionElapsedClock.hours, g_missionElapsedClock.minutes,
											  g_missionElapsedClock.seconds);
				if ((uint16_t)currentSeconds - localCraft->lastHitTimestamp < 5) {
					laserThreatState = 1;
				}
			}

			playerOwnerIdx = g_objectTable[objectIdx].playerOwnerIdx;
			if ((uint16_t)g_players[playerOwnerIdx].currentTargetObjectIdx == localPlayerObjIdx &&
				g_players[playerOwnerIdx].selectedWeaponMode == 0) {
				pai_ObjectRefUpdateApproxRangeScore(objectIdx, (unsigned int)localPlayerObjIdx);
				if ((unsigned int)g_targetRangeScore < 0x10000u &&
					Targeting_ScoreCandidate(localPlayerObjIdx, 0, playerOwnerIdx, 0xffffu)) {
					laserThreatState = 1;
				}
			}

			if ((uint16_t)g_players[playerOwnerIdx].currentTargetObjectIdx == localPlayerObjIdx) {
				if (craft->beamActive != 0 && craft->beamPresent != 0) {
					uint8_t beamTypeId;

					beamTypeId = craft->beamTypeId;
					if (beamTypeId > 0u) {
						if (beamTypeId < 3u) {
							beamThreatState = beamTypeId;
						}
					}
				}
			}
		}

		if (g_missionFlightGroups[g_objectTable[objectIdx].flightGroupIdx].fg.status1 == 5) {
			continue;
		}

		{
			char alreadyCheckedLaserVoice;

			alreadyCheckedLaserVoice = 0;
			if (g_objectTable[objectIdx].genusId == GENUS_Fighter) {
				alreadyCheckedLaserVoice = 1;
			}
			for (laserSlotIdx = 0; laserSlotIdx < craft->laserSlotCount; ++laserSlotIdx) {
				uint8_t meshIdx;

				if (craft->warheadData[laserSlotIdx].weaponType < 4u) {
					continue;
				}

				if (!alreadyCheckedLaserVoice) {
					if ((unsigned int)(g_gameTime - g_panelLastLaserThreatVoiceTime) > 0xec0u &&
						Object_IsHostileToTeam((uint16_t)objectIdx,
											   (uint16_t)g_players[g_localPlayer].playerIff)) {
						pai_ObjectRefDirectionToObjectRef(objectIdx,
														  (unsigned int)g_players[g_localPlayer].objectIndex);
						if ((int)trig2_polardistance < 40960) {
							g_panelLastLaserThreatVoiceTime = g_gameTime;
							fsfx_speakorderack(g_localPlayer, -1, 25, 6,
											   (unsigned int)g_players[g_localPlayer].objectIndex, 0x6000u);
						}
					}
					alreadyCheckedLaserVoice = 1;
				}

				if ((uint16_t)craft->warheadData[laserSlotIdx].turretTargetObjIdx == localPlayerObjIdx) {
					meshIdx = g_modelDefs[craft->modelIndex].weaponHardpoints[laserSlotIdx].meshIdx;
					if (craft->componentHp[meshIdx] != 0) {
						turretThreatState = 1;
					}
				}
			}
		}

		if (craft->beamTypeId == 1 && (unsigned int)(g_gameTime - g_panelLastBeamThreatVoiceTime) > 0xec0u &&
			Object_IsHostileToTeam((uint16_t)objectIdx, (uint16_t)g_players[g_localPlayer].playerIff)) {
			pai_ObjectRefDirectionToObjectRef(objectIdx, (unsigned int)g_players[g_localPlayer].objectIndex);
			if ((int)trig2_polardistance < 0x8000) {
				g_panelLastBeamThreatVoiceTime = g_gameTime;
				fsfx_speakorderack(g_localPlayer, -1, 25, 5,
								   (unsigned int)g_players[g_localPlayer].objectIndex, 0xc000u);
			}
		}

		if (craft->beamActive != 0 && craft->beamPresent != 0 &&
			(uint16_t)craft->beamTargetObjIdx == localPlayerObjIdx) {
			uint8_t beamTypeId;

			beamTypeId = craft->beamTypeId;
			if (beamTypeId > 0u) {
				if (beamTypeId < 3u) {
					beamThreatState = beamTypeId;
				}
			}
		}
	}

	if (g_useHardware3D) {
		Hud_DrawThreatIndicator3D(g_hudThreatIndicatorSlotCenter, laserThreatState);
		Hud_DrawThreatIndicator3D(g_hudThreatIndicatorSlotLeft, turretThreatState);
		Hud_DrawThreatIndicator3D(g_hudThreatIndicatorSlotRight, (uint16_t)beamThreatState);
	} else {
		Hud_DrawThreatIndicator2D(g_hudThreatIndicatorSlotCenter, laserThreatState);
		Hud_DrawThreatIndicator2D(g_hudThreatIndicatorSlotLeft, turretThreatState);
		Hud_DrawThreatIndicator2D(g_hudThreatIndicatorSlotRight, (uint16_t)beamThreatState);
	}

	FlightLight_SetLocalPlayerPulseEnabled(2, (uint16_t)beamThreatState);
	if (g_incomingMissileWarningFlashActive) {
		if (g_useHardware3D) {
			Hud_DrawThreatIndicator3D(g_hudThreatIndicatorSlotBottom, 2);
		} else {
			Hud_DrawThreatIndicator2D(g_hudThreatIndicatorSlotBottom, 2);
		}

		++g_incomingMissileWarningFlashFrame;
		if (g_incomingMissileWarningFlashFrame == 4) {
			g_incomingMissileWarningFlashFrame = 0;
			g_incomingMissileWarningFlashActive = 0;
		}
	} else if (g_useHardware3D) {
		Hud_DrawThreatIndicator3D(g_hudThreatIndicatorSlotBottom, g_incomingMissileWarningState);
	} else {
		Hud_DrawThreatIndicator2D(g_hudThreatIndicatorSlotBottom, g_incomingMissileWarningState);
	}

	FlightLight_SetLocalPlayerPulseEnabled(1, g_incomingMissileWarningState);
	{
		CraftData* localCraft;
		uint32_t hullDamageBucket;
		uint32_t hullDivisor;
		uint32_t shieldTotal;

		localCraft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
		shieldTotal = localCraft->shieldFront + localCraft->shieldRear;
		hullDivisor = localCraft->hullMax / 3u;
		if (hullDivisor == 0) {
			hullDamageBucket = 2;
		} else {
			hullDamageBucket = localCraft->hullDamage / hullDivisor;
		}

		if (shieldTotal < 0x64u && hullDamageBucket == 2) {
			FlightLight_SetLocalPlayerPulseEnabled(0, 1);
		} else {
			FlightLight_SetLocalPlayerPulseEnabled(0, 0);
		}
	}
}

// FUNCTION: XWA 0x477600
void Hud_DrawThreatIndicator2D(HudThreatIndicatorSlot indicatorSlot, int state) {
	enum { HUD_THREAT_2D_ALERT_FRAME_OFFSET = 100, HUD_THREAT_2D_LOCK_FRAME_OFFSET = 200 };

	PlayerData* player;
	int laserMode;
	int frameOffset;

	player = &g_players[g_localPlayer];
	if (Hud_AbsLookDegreesFromOffset(player->lookYawOffset) >= 45 ||
		Hud_AbsLookDegreesFromOffset(player->lookPitchOffset) >= 45) {
		return;
	}

	laserMode = player->selectedWeaponMode == 0;
	if (indicatorSlot == g_hudThreatIndicatorSlotCenter) {
		frameOffset = state != 0 ? HUD_THREAT_2D_ALERT_FRAME_OFFSET : 0;
		if (laserMode) {
			Hud_DrawThreatIndicatorSprite((uint16_t)(10700 + frameOffset), -18, -28);
		} else {
			Hud_DrawThreatIndicatorSprite((uint16_t)(11600 + frameOffset), -19, -34);
		}
	}

	if (indicatorSlot == g_hudThreatIndicatorSlotLeft) {
		frameOffset = state != 0 ? HUD_THREAT_2D_ALERT_FRAME_OFFSET : 0;
		if (laserMode) {
			Hud_DrawThreatIndicatorSprite((uint16_t)(10900 + frameOffset), -8, -33);
		} else {
			Hud_DrawThreatIndicatorSprite((uint16_t)(11800 + frameOffset), -6, -32);
		}
	}

	if (indicatorSlot == g_hudThreatIndicatorSlotRight) {
		frameOffset = state != 0 ? HUD_THREAT_2D_ALERT_FRAME_OFFSET : 0;
		if (laserMode) {
			Hud_DrawThreatIndicatorSprite((uint16_t)(11100 + frameOffset), 9, -33);
		} else {
			Hud_DrawThreatIndicatorSprite((uint16_t)(12000 + frameOffset), 7, -32);
		}
	}

	if (indicatorSlot == g_hudThreatIndicatorSlotBottom) {
		if (state == 0) {
			frameOffset = 0;
		} else if (state == 1) {
			frameOffset = HUD_THREAT_2D_ALERT_FRAME_OFFSET;
		} else {
			frameOffset = HUD_THREAT_2D_LOCK_FRAME_OFFSET;
		}

		if (laserMode) {
			Hud_DrawThreatIndicatorSprite((uint16_t)(11300 + frameOffset), 19, -28);
		} else {
			Hud_DrawThreatIndicatorSprite((uint16_t)(12200 + frameOffset), 19, -34);
		}
	}
}

// FUNCTION: XWA 0x477E50
void Hud_DrawThreatIndicator3D(HudThreatIndicatorSlot indicatorSlot, int state) {
	enum {
		HUD_THREAT_LASER_CENTER_FRAME = 15,
		HUD_THREAT_LASER_LEFT_FRAME = 16,
		HUD_THREAT_LASER_RIGHT_FRAME = 17,
		HUD_THREAT_LASER_BOTTOM_FRAME = 18,
		HUD_THREAT_WARHEAD_CENTER_FRAME = 19,
		HUD_THREAT_WARHEAD_LEFT_FRAME = 20,
		HUD_THREAT_WARHEAD_RIGHT_FRAME = 21,
		HUD_THREAT_WARHEAD_BOTTOM_FRAME = 22
	};

	const int alertColor = (int)0xff10bc00u;
	const int bottomWarnColor = (int)0xfffcd400u;
	const int bottomLockColor = (int)0xffff0000u;
	FlightTexQuad quad;
	int localPlayer;
	uint8_t laserMode;
	int color;

#ifdef XWA_MODERN
	XwaSnapshotHud_NoteThreat((int)indicatorSlot, state);
#endif

	quad.depthZ = 1;
	quad.screenX = 0;
	quad.screenY = 0;
	quad.rotationAngle = 0;
	laserMode = 0;
	{
		float scaledSize = g_flightHudScaleFactor;

		scaledSize *= g_hudQuadBaseSize;
		quad.screenSize = (uint16_t)(int)scaledSize;
	}

	localPlayer = g_localPlayer;
	if (g_players[localPlayer].selectedWeaponMode == 0) {
		laserMode = 1;
	}
	if (Hud_AbsLookDegreesFromOffset(g_players[localPlayer].lookYawOffset) >= 45 ||
		Hud_AbsLookDegreesFromOffset(g_players[localPlayer].lookPitchOffset) >= 45) {
		return;
	}

	if (indicatorSlot == g_hudThreatIndicatorSlotCenter) {
		if (state == 0) {
			color = (int)g_hudColors[0];
		} else {
			color = alertColor;
		}
		if (laserMode) {
			quad.screenX = g_reticleDrawX - (uint16_t)g_hudLaserThreatSlot0OffsetX;
			quad.screenY = g_screenHeight + (uint16_t)g_hudLaserThreatSlot0OffsetY - g_reticleDrawY;
			FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, HUD_THREAT_LASER_CENTER_FRAME, 256);
		} else {
			quad.screenX = g_reticleDrawX - (uint16_t)g_hudWarheadThreatSlot0OffsetX;
			quad.screenY = g_screenHeight + (uint16_t)g_hudWarheadThreatSlot0OffsetY - g_reticleDrawY;
			FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, HUD_THREAT_WARHEAD_CENTER_FRAME, 256);
		}
		RenderQuad_DrawModelTexture(OBJ_HudTextureGroup12000, &quad, color);
	}

	if (indicatorSlot == g_hudThreatIndicatorSlotLeft) {
		if (state == 0) {
			color = (int)g_hudColors[0];
		} else {
			color = alertColor;
		}
		if (laserMode) {
			quad.screenX = g_reticleDrawX - (uint16_t)g_hudLaserThreatSlot1OffsetX;
			quad.screenY = g_screenHeight + (uint16_t)g_hudLaserThreatSlot1OffsetY - g_reticleDrawY;
			FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, HUD_THREAT_LASER_LEFT_FRAME, 256);
		} else {
			quad.screenX = g_reticleDrawX - (uint16_t)g_hudWarheadThreatSlot1OffsetX;
			quad.screenY = g_screenHeight + (uint16_t)g_hudWarheadThreatSlot1OffsetY - g_reticleDrawY;
			FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, HUD_THREAT_WARHEAD_LEFT_FRAME, 256);
		}
		RenderQuad_DrawModelTexture(OBJ_HudTextureGroup12000, &quad, color);
	}

	if (indicatorSlot == g_hudThreatIndicatorSlotRight) {
		if (state == 0) {
			color = (int)g_hudColors[0];
		} else {
			color = alertColor;
		}
		if (laserMode) {
			int reticleY = g_reticleDrawY;

			quad.screenX = g_reticleDrawX + (uint16_t)g_hudLaserThreatSlot2OffsetX;
			quad.screenY = g_screenHeight + (uint16_t)g_hudLaserThreatSlot2OffsetY - reticleY;
			FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, HUD_THREAT_LASER_RIGHT_FRAME, 256);
		} else {
			quad.screenX = (uint16_t)g_hudWarheadThreatSlot2OffsetX;
			quad.screenX += g_reticleDrawX;
			quad.screenY = g_screenHeight + (uint16_t)g_hudWarheadThreatSlot2OffsetY - g_reticleDrawY;
			FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, HUD_THREAT_WARHEAD_RIGHT_FRAME, 256);
		}
		RenderQuad_DrawModelTexture(OBJ_HudTextureGroup12000, &quad, color);
	}

	if (indicatorSlot == g_hudThreatIndicatorSlotBottom) {
		if (state == 0) {
			color = (int)g_hudColors[0];
		} else if (state == 1) {
			color = bottomWarnColor;
		} else {
			color = bottomLockColor;
		}

		if (laserMode) {
			quad.screenX = (uint16_t)g_hudLaserThreatSlot3OffsetX;
			quad.screenX += g_reticleDrawX;
			quad.screenY = g_screenHeight + (uint16_t)g_hudLaserThreatSlot3OffsetY - g_reticleDrawY;
			FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, HUD_THREAT_LASER_BOTTOM_FRAME, 256);
		} else {
			quad.screenX = g_reticleDrawX + (uint16_t)g_hudWarheadThreatSlot3OffsetX;
			quad.screenY = g_screenHeight + (uint16_t)g_hudWarheadThreatSlot3OffsetY - g_reticleDrawY;
			FeDiskIo_SelectTextureFrame(OBJ_HudTextureGroup12000, HUD_THREAT_WARHEAD_BOTTOM_FRAME, 256);
		}
		RenderQuad_DrawModelTexture(OBJ_HudTextureGroup12000, &quad, color);
	}
}

static inline CraftData* Hud_GetCraftPointerInlined(void) {
	int objectIndex;
	MobileObject* mobj;
	CraftData* craft;

	objectIndex = g_players[g_localPlayer].objectIndex;
	if (objectIndex != 0xffff) {
		mobj = g_objectTable[objectIndex].mobj;
		if (mobj != NULL) {
			craft = mobj->pCraft;
			if (craft != NULL) {
				return craft;
			}
		}
	}

	XWA_HUD_OUTPUT_DEBUG_STRING("GetCraftPointer() returned NULL in HUD.c\n");
	return NULL;
}

static __inline CraftData*
Hud_GetCraftPointerInlinedWithDebug(void(XWA_HUD_STDCALL* debugOutput)(const char*)) {
	int objectIndex;
	MobileObject* mobj;
	CraftData* craft;

	objectIndex = g_players[g_localPlayer].objectIndex;
	if (objectIndex != 0xffff) {
		mobj = g_objectTable[objectIndex].mobj;
		if (mobj != NULL) {
			craft = mobj->pCraft;
			if (craft != NULL) {
				return craft;
			}
		}
	}

	debugOutput("GetCraftPointer() returned NULL in HUD.c\n");
	return NULL;
}

// FUNCTION: XWA 0x478200
void Hud_DrawCMD3D(void) {
	enum {
		HUD_CMD_FRAME_MODEL_TYPE = OBJ_HudTextureGroup12000,
		HUD_CMD_FRAME_TEXTURE_FRAME = 11,
		HUD_CMD_FRAME_BASE_SIZE = 256,
		HUD_CMD_FRAME_D3D_FLAGS = 0x9612
	};

	void(XWA_HUD_STDCALL * debugOutput)(const char*);
	FlightTexQuad quad;
	CraftData* craft;
	ModelTypeInfo* modelTypeInfo;
	TexLevel* texLevel;

	craft = NULL;
	quad.screenX = 0;
	quad.screenY = 0;
	quad.depthZ = 0x10000000;
	quad.rotationAngle = 0;
	quad.screenSize = HUD_CMD_FRAME_BASE_SIZE;

	if (!g_hudElementEnabled[0].enabled && !g_inHangarReady) {
		return;
	}

	if (!g_players[g_localPlayer].mapCameraState) {
		debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
		craft = Hud_GetCraftPointerInlinedWithDebug(debugOutput);
		if (craft == NULL) {
			debugOutput("NULL craft data pointer in DrawCMD3D()!\n");
			return;
		}
	}

	quad.screenSize = (uint16_t)(int)((double)quad.screenSize * (double)g_flightHudScaleFactor);
	if (g_players[g_localPlayer].mapCameraState) {
		FeDiskIo_SelectTextureFrame(HUD_CMD_FRAME_MODEL_TYPE, HUD_CMD_FRAME_TEXTURE_FRAME,
									HUD_CMD_FRAME_BASE_SIZE);
	} else {
		if ((craft->activeHudFeatureMask & 1u) != 0) {
			FeDiskIo_SelectTextureFrame(HUD_CMD_FRAME_MODEL_TYPE, HUD_CMD_FRAME_TEXTURE_FRAME,
										HUD_CMD_FRAME_BASE_SIZE);
		} else {
			FeDiskIo_SelectTextureFrame(HUD_CMD_FRAME_MODEL_TYPE, HUD_CMD_FRAME_TEXTURE_FRAME,
										HUD_CMD_FRAME_BASE_SIZE);
		}
	}

	modelTypeInfo = &g_modelTypeTable[HUD_CMD_FRAME_MODEL_TYPE];
	texLevel = modelTypeInfo->curTexLevel;
	if ((modelTypeInfo->assetFlags & (MODEL_TYPE_ASSET_TEXTURE_DRAW | MODEL_TYPE_ASSET_TEXTURE_READY)) == 0 ||
		texLevel == NULL) {
		return;
	}

	quad.screenX = (uint16_t)g_hudCenterX;
	quad.screenY = (uint16_t)g_hudCmdFrameY;
	texLevel->argbColor = g_hudColors[0];
	if ((uint8_t)g_flightConfPowerVr) {
		quad.depthZ = 1;
	}

	RenderQuad_DrawRotatedSprite(&quad, texLevel, HUD_CMD_FRAME_D3D_FLAGS);
}

// FUNCTION: XWA 0x478360
void Hud_DrawHudTargetInsetIfEnabled(int playerIdx) {
	int insetHeight;
	uint16_t insetWidth;
	int insetY;
	int insetX;

	insetX = g_hudCenterX;
	insetY = g_screenHeight;
	insetWidth = g_hudTargetInsetWidth;
	insetHeight = g_hudTargetInsetHeight;
	insetX -= insetWidth >> 1;
	insetY -= insetHeight;

	if (!g_players[g_localPlayer].hudEnabled || !g_hudElementEnabled[0].enabled ||
		playerIdx != g_localPlayer ||
		(!g_inHangarReady && (g_players[g_localPlayer].regionSessionId || g_flightMissionEndPending)) ||
		g_players[g_localPlayer].hyperspacePhase != PLAYER_HYPERSPACE_PHASE_NONE) {
		return;
	}

	if (g_players[playerIdx].mapCameraState) {
		if (g_players[playerIdx].currentTargetObjectIdx == 0xffffu) {
			return;
		}
		Hud_Update3DCrt((uint16_t)insetX, (uint16_t)insetY, insetWidth, (uint16_t)insetHeight, 1);
		return;
	}

	if ((g_players[playerIdx].viewState.hudStateLive == 19 ||
		 g_players[playerIdx].viewState.hudStateLive == 0) &&
		(!g_players[playerIdx].mfd.enabled[0] || g_inHangarReady) &&
		(g_players[playerIdx].currentTargetObjectIdx != 0xffffu || g_inHangarReady)) {
		CraftData* craft;

		craft = g_objectTable[g_players[playerIdx].objectIndex].mobj->pCraft;
		if ((craft->activeHudFeatureMask & 1u) != 0) {
			Hud_Update3DCrt((uint16_t)insetX, (uint16_t)insetY, insetWidth, (uint16_t)insetHeight, 1);
			return;
		}
	}
}

// FUNCTION: XWA 0x478490
void Hud_Update3DCrt(uint16_t x, uint16_t y, uint16_t width, uint16_t height, int unused) {
	uint16_t targetObjIdx;
	int savedProjOffsetY;
	int savedTargetX;
	int savedTargetY;
	int savedTargetZ;
	int relX;
	int relY;
	int relZ;
	int componentRelX;
	int componentRelY;
	int componentRelZ;
#ifdef XWA_MODERN
	XwaHudCrt capturedCrt;
#endif

	componentRelX = 0;
	componentRelY = 0;
	componentRelZ = 0;
#ifdef XWA_MODERN
	memset(&capturedCrt, 0, sizeof capturedCrt);
#endif
	if (g_inHangarReady) {
		targetObjIdx = g_players[g_localPlayer].objectIndex;
	} else {
		targetObjIdx = g_players[g_localPlayer].currentTargetObjectIdx;
	}

	if (g_objectTable[targetObjIdx].objectType != 0) {
		unsigned int baseOffset;

		savedProjOffsetY = g_projOffsetY;
		savedTargetX = g_players[g_localPlayer].viewState.savedTargetX;
		savedTargetY = g_players[g_localPlayer].viewState.savedTargetY;
		savedTargetZ = g_players[g_localPlayer].viewState.savedTargetZ;
		g_projOffsetY = 0;
		g_projOffsetYf = 0.0f;

		baseOffset = (unsigned int)g_flightComputePixelOffsetFn(x, y);
		PushFlightViewport(width, height, unused, baseOffset);
#ifdef XWA_MODERN
		g_hudCrtCameraDistance = 0;
#endif
		Hud_PointCamera(targetObjIdx, 1, g_localPlayer);

#ifdef XWA_MODERN
		capturedCrt.visible = 1;
		capturedCrt.self_view = (uint8_t)(g_inHangarReady != 0);
		capturedCrt.map_view = (uint8_t)(g_players[g_localPlayer].mapCameraState != 0);
		capturedCrt.target_slot = targetObjIdx;
		capturedCrt.target_signature = g_objectTable[targetObjIdx].objectSignature;
		capturedCrt.selected_component = (uint16_t)g_players[g_localPlayer].selectedTargetComponent;
		capturedCrt.projectile_exclude_slots[0] = 0xffffu;
		capturedCrt.projectile_exclude_slots[1] = 0xffffu;
		if (!g_replayViewMode) {
			if (!g_players[g_localPlayer].viewState.externalCameraActive) {
				capturedCrt.projectile_exclude_slots[0] =
					(uint16_t)g_players[g_localPlayer].viewState.cameraFocusObjIdx;
			}
			if (g_filmPlaybackMode && g_filmOverlayActive == 1 &&
				!g_filmOverlayViewState.externalCameraActive) {
				capturedCrt.projectile_exclude_slots[1] = (uint16_t)g_filmOverlayViewState.cameraFocusObjIdx;
			}
		}
		capturedCrt.classic_viewport_w = width;
		capturedCrt.classic_viewport_h = height;
		capturedCrt.proj_aspect_y_q16 = g_projAspectY;
		capturedCrt.proj_scale = g_projScaleInt;
		capturedCrt.camera_distance = g_hudCrtCameraDistance;
		capturedCrt.camera_back_step[0] = worldlocx - g_players[g_localPlayer].viewState.savedTargetX;
		capturedCrt.camera_back_step[1] = worldlocy - g_players[g_localPlayer].viewState.savedTargetY;
		capturedCrt.camera_back_step[2] = worldlocz - g_players[g_localPlayer].viewState.savedTargetZ;
		capturedCrt.camera_rows_q15[0] = g_camMatR0_X;
		capturedCrt.camera_rows_q15[1] = g_camMatR0_Y;
		capturedCrt.camera_rows_q15[2] = g_camMatR0_Z;
		capturedCrt.camera_rows_q15[3] = g_camMatR1_X;
		capturedCrt.camera_rows_q15[4] = g_camMatR1_Y;
		capturedCrt.camera_rows_q15[5] = g_camMatR1_Z;
		capturedCrt.camera_rows_q15[6] = g_camMatR2_X;
		capturedCrt.camera_rows_q15[7] = g_camMatR2_Y;
		capturedCrt.camera_rows_q15[8] = g_camMatR2_Z;
#endif

		g_unusedFlightRenderColorByte = 48;
		g_sceneBypassCockpitMask = 1;
		RenderScene_Initialize(1);
		g_sceneBillboardQueueCount = 0;

		relX = worldlocx - g_players[g_localPlayer].viewState.savedTargetX;
		relY = worldlocy - g_players[g_localPlayer].viewState.savedTargetY;
		relZ = worldlocz - g_players[g_localPlayer].viewState.savedTargetZ;
		g_sceneBypassCockpitMask = 0;
		g_camRelWorldX = relX;
		g_camRelWorldY = relY;
		g_camRelWorldZ = relZ;

		if (g_players[g_localPlayer].hyperspaceRuntime.targetBoxEnabled &&
			targetObjIdx >= g_activeRegionObjectSlotStart &&
			targetObjIdx < g_activeRegionCraftObjectSlotEnd) {
			ObjectRecord* obj;
			uint16_t componentIdx;

			obj = &g_objectTable[targetObjIdx];
			componentIdx = (uint16_t)g_players[g_localPlayer].selectedTargetComponent;
#ifdef XWA_MODERN
			capturedCrt.component_focus[0] = ModelMesh_GetComponentFocusX(obj->objectType, componentIdx);
			capturedCrt.component_focus[1] = ModelMesh_GetComponentFocusY(obj->objectType, componentIdx);
			capturedCrt.component_focus[2] = ModelMesh_GetComponentFocusZ(obj->objectType, componentIdx);
#endif
			g_rotatedX = ModelMesh_GetComponentFocusX(obj->objectType, componentIdx);
			g_rotatedY = ModelMesh_GetComponentFocusZ(obj->objectType, componentIdx);
			g_rotatedZ = -ModelMesh_GetComponentFocusY(obj->objectType, componentIdx);
			pai_RotateLocalVectorToWorldScratch(obj, g_rotatedX, g_rotatedY, g_rotatedZ);
			componentRelX = worldlocx + g_rotatedX - g_players[g_localPlayer].viewState.savedTargetX;
			componentRelY = worldlocy + g_rotatedY - g_players[g_localPlayer].viewState.savedTargetY;
			componentRelZ = worldlocz + g_rotatedZ - g_players[g_localPlayer].viewState.savedTargetZ;
			relX = g_camRelWorldX;
			relY = g_camRelWorldY;
			relZ = g_camRelWorldZ;
		}

		viewX = TRANSFM2_CamMatDotRow0(relX, relY, relZ);
		viewY = TRANSFM2_CamMatDotRow1(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);
		viewZ = TRANSFM2_CamMatDotRow2(g_camRelWorldX, g_camRelWorldY, g_camRelWorldZ);

		{
			ObjectRecord* targetObj;
			MobileObject* targetMobj;

			targetObj = &g_objectTable[targetObjIdx];
			targetMobj = targetObj->mobj;
			if (targetMobj != NULL) {
				switch (targetObj->genusId) {
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
					case GENUS_Rubble:
						g_curCraft = targetMobj->pCraft;
						FVIEW_SetObjectTransform(targetObj->roll, targetObj->pitch, targetObj->yaw,
												 targetObj->angleD, targetObj);
						FlightLight_SetupTargetInsetObjectLighting(targetObjIdx);
						Damage_QueueCraftBillboards(targetObjIdx);
						RenderScene_DrawObjectModel(targetObj);
						break;

					case GENUS_PlayerProjectile:
					case GENUS_NpcProjectile:
						g_curCraft = NULL;
						FVIEW_SetObjectTransform(targetObj->roll, targetObj->pitch, targetObj->yaw,
												 targetObj->angleD, targetObj);
						RenderBillboard_DrawRollAlignedObjectModel(targetObjIdx);
						break;

					case GENUS_SalvageJunk:
						g_curCraft = NULL;
						FVIEW_SetObjectTransform(targetObj->roll, targetObj->pitch, targetObj->yaw,
												 targetObj->angleD, targetObj);
						FlightLight_SetupTargetInsetObjectLighting(targetObjIdx);
						Damage_QueueCraftBillboards(targetObjIdx);
						RenderScene_DrawObjectModel(targetObj);
						break;

					default:
						break;
				}

				if (g_curCraft != NULL) {
					uint16_t carriedObjIdx;

					carriedObjIdx = g_curCraft->carriedObjectIndex;
					if (carriedObjIdx != 0xffffu && carriedObjIdx != g_players[g_localPlayer].objectIndex) {
						MobileObject* carriedMobj;

						if (g_objectTable[carriedObjIdx].objectType != 0) {
							carriedMobj = g_objectTable[carriedObjIdx].mobj;
							if (carriedMobj != NULL) {
								g_curCraft = carriedMobj->pCraft;
								if (g_curCraft != NULL) {
									FVIEW_SetObjectTransform(
										g_objectTable[carriedObjIdx].roll, g_objectTable[carriedObjIdx].pitch,
										g_objectTable[carriedObjIdx].yaw, g_objectTable[carriedObjIdx].angleD,
										&g_objectTable[carriedObjIdx]);
									FlightLight_SetupTargetInsetObjectLighting(carriedObjIdx);
									Damage_QueueCraftBillboards(carriedObjIdx);
									RenderScene_DrawObjectModel(&g_objectTable[carriedObjIdx]);
								}
							}
						}
					}
				}
			} else {
				switch (targetObj->genusId) {
					case GENUS_Mine:
					case GENUS_Asteroid:
					case GENUS_Debris:
					case GENUS_DeathStarTunnelSegment:
						FVIEW_SetObjectTransform(targetObj->roll, targetObj->pitch, targetObj->yaw,
												 targetObj->angleD, NULL);
						RenderNonCraftSceneObject(targetObjIdx);
						break;

					default:
						break;
				}
			}
		}

		{
			uint16_t warheadObjIdx;
			uint32_t objIdx;

			for (warheadObjIdx = (uint16_t)g_projectileObjectSlotStart, objIdx = warheadObjIdx;
				 warheadObjIdx < g_regionMainObjectSlotEnd; objIdx = ++warheadObjIdx) {
				ObjectRecord* obj;
				int maxBoundsExtent;
				int16_t genusId;
				if ((objIdx == (uint32_t)g_players[g_localPlayer].viewState.cameraFocusObjIdx &&
					 !g_players[g_localPlayer].viewState.externalCameraActive && !g_replayViewMode) ||
					(g_filmPlaybackMode && g_filmOverlayActive == 1 &&
					 objIdx == (uint32_t)g_filmOverlayViewState.cameraFocusObjIdx &&
					 !g_filmOverlayViewState.externalCameraActive && !g_replayViewMode) ||
					(g_filmPlaybackMode && g_filmOverlayActive == 1 &&
					 objIdx == (uint32_t)g_filmOverlayViewState.cameraFocusObjIdx &&
					 !g_filmOverlayViewState.externalCameraActive && !g_replayViewMode)) {
					continue;
				}

				obj = &g_objectTable[objIdx];
				if (obj->objectType == 0) {
					continue;
				}

				maxBoundsExtent = g_modelTypeTable[obj->objectType].maxBoundsExtent;
				g_curModelMaxExtent = maxBoundsExtent;
				genusId = obj->genusId;
				if (genusId >= GENUS_PlayerProjectile) {
					if (genusId > GENUS_NpcProjectile) {
						if (genusId != GENUS_Explosion) {
							continue;
						}
						if (!FlightView_IsObjectSphereVisible((int)objIdx, maxBoundsExtent)) {
							continue;
						}
						FVIEW_SetObjectTransform(g_objectTable[objIdx].roll, g_objectTable[objIdx].pitch,
												 g_objectTable[objIdx].yaw, g_objectTable[objIdx].angleD,
												 &g_objectTable[objIdx]);
						SceneBillboard_QueueObjectTextured(warheadObjIdx);
						if (!g_useHardware3D) {
							continue;
						}
						if (g_objRenderState[objIdx].particleEffects != NULL) {
							Particle_DrawObjectEffectsForCrt(warheadObjIdx);
						}
						if (g_objRenderState[objIdx].trailHead == NULL) {
							continue;
						}
					} else {
						if (g_players[g_localPlayer].mapCameraState) {
							if (obj->mobj->sourceObjIdx == (int16_t)targetObjIdx &&
								FlightView_IsObjectSphereVisible((int)objIdx, maxBoundsExtent)) {
								FVIEW_SetObjectTransform(
									g_objectTable[objIdx].roll, g_objectTable[objIdx].pitch,
									g_objectTable[objIdx].yaw, g_objectTable[objIdx].angleD,
									&g_objectTable[objIdx]);
								RenderBillboard_DrawRollAlignedObjectModel(warheadObjIdx);
							}
							continue;
						}

						if (obj->mobj->sourceObjIdx != (int16_t)targetObjIdx &&
							(uint16_t)obj->mobj->sourceObjIdx != g_players[g_localPlayer].objectIndex) {
							continue;
						}
						if (!FlightView_IsObjectSphereVisible((int)objIdx, maxBoundsExtent)) {
							continue;
						}
						FVIEW_SetObjectTransform(g_objectTable[objIdx].roll, g_objectTable[objIdx].pitch,
												 g_objectTable[objIdx].yaw, g_objectTable[objIdx].angleD,
												 &g_objectTable[objIdx]);
						RenderBillboard_DrawRollAlignedObjectModel(warheadObjIdx);
						if (!g_useHardware3D) {
							continue;
						}
						if (g_objRenderState[objIdx].particleEffects != NULL) {
							Particle_DrawObjectEffectsForCrt(warheadObjIdx);
						}
						if (g_objRenderState[objIdx].trailHead == NULL) {
							continue;
						}
					}
					ObjectTrail_DrawEmittersForObject(warheadObjIdx);
				}
			}

			for (objIdx = (uint16_t)g_localTransientSlotStart; objIdx < g_localTransientSlotEnd; ++objIdx) {
				if (g_objectTable[objIdx].objectType != 0 &&
					g_objectTable[objIdx].genusId == GENUS_Explosion) {
					g_curModelMaxExtent = g_modelTypeTable[g_objectTable[objIdx].objectType].maxBoundsExtent;
					if (FlightView_IsObjectSphereVisible((int)objIdx, g_curModelMaxExtent)) {
						FVIEW_SetObjectTransform(g_objectTable[objIdx].roll, g_objectTable[objIdx].pitch,
												 g_objectTable[objIdx].yaw, g_objectTable[objIdx].angleD,
												 &g_objectTable[objIdx]);
						SceneBillboard_QueueObjectTextured((uint16_t)objIdx);
					}
				}
			}
		}

		if (g_useHardware3D) {
			unsigned int effectObjIdx;

			effectObjIdx = targetObjIdx;
			if (!g_objRenderState[effectObjIdx].drawnThisFrame) {
				GlowMark_QueueCraftDamageSurfaceEffects((uint16_t)effectObjIdx);
				if (g_objRenderState[effectObjIdx].pendingGlowMarks != NULL) {
					GlowMark_ProcessPendingRequests((uint16_t)effectObjIdx);
				}
				if (g_objRenderState[effectObjIdx].particleEffects != NULL) {
					Particle_UpdateObjectEffectsForCrt((uint16_t)effectObjIdx);
				}
				if (g_objRenderState[effectObjIdx].trailHead != NULL) {
					ObjectTrail_DrawEmittersForObject((uint16_t)effectObjIdx);
				}
				EngineGlow_RenderForObject(effectObjIdx);
				g_objRenderState[effectObjIdx].drawnThisFrame = 1;
			} else {
				if (g_objRenderState[effectObjIdx].particleEffects != NULL) {
					Particle_DrawObjectEffectsForCrt((uint16_t)effectObjIdx);
				}
				if (g_objRenderState[effectObjIdx].trailHead != NULL) {
					ObjectTrail_DrawEmittersForObject((uint16_t)effectObjIdx);
				}
				EngineGlow_RenderForObject(effectObjIdx);
			}
		}

		RenderScene_DrawVisibleFaces();
		if (g_sceneBillboardQueueCount != 0) {
			g_flightSwRotSpriteCoeffCacheValid = 0;
			SceneBillboard_RenderQueuedTextured(0);
			g_flightSwRotSpriteCoeffCacheValid = 0;
		}

		if (g_players[g_localPlayer].hyperspaceRuntime.targetBoxEnabled &&
			targetObjIdx >= g_activeRegionObjectSlotStart &&
			targetObjIdx < g_activeRegionCraftObjectSlotEnd && g_objectTable[targetObjIdx].genusId != 0) {
			int componentMarkerX;
			int componentMarkerY;

			viewX = TRANSFM2_CamMatDotRow0(componentRelX, componentRelY, componentRelZ);
			viewY = TRANSFM2_CamMatDotRow1(componentRelX, componentRelY, componentRelZ);
			viewZ = TRANSFM2_CamMatDotRow2(componentRelX, componentRelY, componentRelZ);
			componentMarkerX = TRANSFM2_ProjectScreenX(viewX, viewZ);
			componentMarkerY = TRANSFM2_ProjectScreenY(viewY, viewZ);
			Hud_DrawBoxInXTrans(componentMarkerX - 2, componentMarkerY - 2, 4, 4, 63, 1);
#ifdef XWA_MODERN
			capturedCrt.component_marker_visible = 1;
#endif
		}

#ifdef XWA_MODERN
		XwaSnapshotHud_NoteCrt(&capturedCrt);
#endif

		g_unusedFlightRenderColorByte = g_flightColorEscapeBypassChar;
		PopFlightViewport();
		g_projOffsetYf = (float)savedProjOffsetY;
		g_players[g_localPlayer].viewState.savedTargetX = savedTargetX;
		g_players[g_localPlayer].viewState.savedTargetY = savedTargetY;
		g_players[g_localPlayer].viewState.savedTargetZ = savedTargetZ;
		g_projOffsetY = savedProjOffsetY;
	}
}

// FUNCTION: XWA 0x478F00
int Hud_PointCamera(uint16_t targetObjIdx, int16_t fitDistanceFlag, int playerIdx) {
	uint16_t resolvedTargetIdx;
	int relX;
	int relY;
	int relZ;
	int scaledRelX;
	int scaledRelY;
	int scaledRelZ;
	int useTargetOrientation;
	int normRelX;
	int normRelY;
	int normRelZ;
	uint16_t distanceDivisor;
	int zero;

	zero = 0;
	useTargetOrientation = zero;
	distanceDivisor = (uint16_t)zero;
	resolvedTargetIdx = targetObjIdx;
	Mission_ResolveObjectOrMissionPointWorldLoc(resolvedTargetIdx, zero, zero, zero);

	if (g_players[playerIdx].mapCameraState) {
		relY = worldlocy - g_players[playerIdx].viewState.savedTargetY;
		relX = worldlocx - g_players[playerIdx].viewState.savedTargetX;
		relZ = worldlocz - g_players[playerIdx].viewState.savedTargetZ;
	} else if (g_inHangarReady) {
		relX = 0x20000;
		relZ = -0x20000;
		relY = 0x20000;
	} else {
		ObjectRecord* playerObj;

		playerObj = &g_objectTable[g_players[playerIdx].objectIndex];
		relX = worldlocx - playerObj->world_x;
		relY = worldlocy - playerObj->world_y;
		relZ = worldlocz - playerObj->world_z;
	}

	scaledRelY = relY << 1;
	scaledRelX = relX << 1;
	scaledRelZ = relZ << 1;
	{
		int absRelX;
		int absRelY;
		int absRelZ;
		uint16_t absRelX16;
		uint16_t absRelY16;
		uint16_t absRelZ16;

		absRelY = scaledRelY >> 16;
		absRelX = scaledRelX >> 16;
		absRelZ = scaledRelZ >> 16;
		if (((uint16_t)absRelX & 0x8000u) != 0) {
			absRelX = -absRelX;
		}
		if (((uint16_t)absRelY & 0x8000u) != 0) {
			absRelY = -absRelY;
		}
		if (((uint16_t)absRelZ & 0x8000u) != 0) {
			absRelZ = -absRelZ;
		}

		absRelX16 = (uint16_t)absRelX;
		absRelY16 = (uint16_t)absRelY;
		absRelZ16 = (uint16_t)absRelZ;
		do {
			do {
				absRelY16 >>= 1;
				absRelZ16 >>= 1;
				absRelX16 >>= 1;
				scaledRelX >>= 1;
				scaledRelY >>= 1;
				scaledRelZ >>= 1;
			} while (absRelX16 != 0);
		} while (absRelY16 != 0 || absRelZ16 != 0);
	}

	normRelX = scaledRelX >> 1;
	normRelY = scaledRelY >> 1;
	normRelZ = scaledRelZ >> 1;
	if (g_objectTable[resolvedTargetIdx].mobj != NULL) {
		int playerObjIdx;

		playerObjIdx = g_players[playerIdx].objectIndex;
		if (playerObjIdx != 0xffff) {
			MobileObject* playerMobj;

			playerMobj = g_objectTable[playerObjIdx].mobj;
			if (playerMobj != NULL) {
				CraftData* playerCraft;

				playerCraft = playerMobj->pCraft;
				g_curCraft = playerCraft;
				if (playerCraft != NULL && playerCraft->aiFlight.orderActionFlag) {
					AiController* ai;

					ai = pai_GetEffectiveAIController(playerCraft);
					if (ai != NULL && ai->targetObjIdx == targetObjIdx) {
						useTargetOrientation = 1;
					}
				}
			}
		}

		g_curCraft = g_objectTable[resolvedTargetIdx].mobj->pCraft;
		if (g_curCraft != NULL) {
			int localPlayerObjIdx;

			localPlayerObjIdx = g_players[g_localPlayer].objectIndex;
			if ((int)g_curCraft->carrierObjIdx == localPlayerObjIdx) {
				useTargetOrientation = 1;
			}
		}

		if ((uint16_t)useTargetOrientation != 0) {
			int q15One;
			ObjectRecord* targetObj;

			targetObj = &g_objectTable[resolvedTargetIdx];
			g_camMatR1_X = -targetObj->mobj->cachedFwdX;
			g_viewMtx10 = (float)g_camMatR1_X * flt_5A99B4;
			g_camMatR1_Y = -targetObj->mobj->cachedFwdY;
			g_viewMtx11 = (float)g_camMatR1_Y * flt_5A99B4;
			g_camMatR1_Z = -targetObj->mobj->cachedFwdZ;
			g_viewMtx12 = (float)g_camMatR1_Z * flt_5A99B4;
			g_fviewFwdX_Q15 = targetObj->mobj->cachedFwdX;
			g_fviewFwdY_Q15 = targetObj->mobj->cachedFwdY;
			g_fviewFwdZ_Q15 = targetObj->mobj->cachedFwdZ;

			g_camMatR0_X = -targetObj->mobj->cachedSideX;
			g_viewMtx00 = (float)g_camMatR0_X * flt_5A99B4;
			g_camMatR0_Y = -targetObj->mobj->cachedSideY;
			g_viewMtx01 = (float)g_camMatR0_Y * flt_5A99B4;
			g_camMatR0_Z = -targetObj->mobj->cachedSideZ;
			g_viewMtx02 = (float)g_camMatR0_Z * flt_5A99B4;

			g_fviewUpX_Q15 = targetObj->mobj->cachedUpX;
			g_fviewUpY_Q15 = targetObj->mobj->cachedUpY;
			g_fviewUpZ_Q15 = targetObj->mobj->cachedUpZ;
			g_camMatR2_X = -targetObj->mobj->cachedUpX;
			g_viewMtx20 = (float)g_camMatR2_X * flt_5A99B4;
			g_camMatR2_Y = -targetObj->mobj->cachedUpY;
			g_viewMtx21 = (float)g_camMatR2_Y * flt_5A99B4;
			g_camMatR2_Z = -targetObj->mobj->cachedUpZ;
			g_viewMtx22 = (float)g_camMatR2_Z * flt_5A99B4;

			q15One = 0x7fff;
			g_curMatR2_X = -g_fviewFwdX_Q15;
			g_curMatR2_Y = -g_fviewFwdY_Q15;
			g_curMatR2_Z = -g_fviewFwdZ_Q15;
			g_curMatR0_X = g_fviewSideX_Q15;
			g_curMatR0_Y = g_fviewSideY_Q15;
			g_curMatR0_Z = g_fviewSideZ_Q15;
			g_curMatR1_X = g_fviewUpX_Q15;
			g_curMatR1_Y = g_fviewUpY_Q15;
			g_curMatR1_Z = g_fviewUpZ_Q15;

			g_objViewMat_R0_Y = 0;
			g_objViewMatF_R0_Y = 0.0f;
			g_objViewMat_R0_Z = 0;
			g_objViewMatF_R0_Z = 0.0f;
			g_objViewMat_R1_X = 0;
			g_objViewMatF_R1_X = 0.0f;
			g_objViewMat_R1_Z = 0;
			g_objViewMatF_R1_Z = 0.0f;
			g_objViewMat_R2_X = 0;
			g_objViewMatF_R2_X = 0.0f;
			g_objViewMat_R2_Y = 0;
			g_objViewMatF_R2_Y = 0.0f;
			g_objViewMat_R0_X = q15One;
			g_objViewMatF_R0_X = 1.0f;
			g_objViewMat_R1_Y = q15One;
			g_objViewMatF_R1_Y = 1.0f;
			g_objViewMat_R2_Z = -32767;
			g_objViewMatF_R2_Z = -1.0f;
		}
	}

	if ((uint16_t)useTargetOrientation == 0) {
		int localX;
		int localY;
		int localZ;

		if (g_players[playerIdx].mapCameraState) {
			FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll,
									(int16_t)g_players[playerIdx].viewState.viewPitch,
									(int16_t)g_players[playerIdx].viewState.viewYaw, 0, 0, 0, NULL, -1);

			localX = Xwa_WrappedMulAdd3Q15(g_camMatR0_Z, (int16_t)normRelZ, g_camMatR0_X, (int16_t)normRelX,
										   g_camMatR0_Y, (int16_t)normRelY);
			localZ = Xwa_Dot3Q15Wrapped(g_camMatR2_X, g_camMatR2_Y, g_camMatR2_Z, (int16_t)normRelX,
										(int16_t)normRelY, (int16_t)normRelZ);
			localY = Xwa_Dot3Q15Wrapped(g_camMatR1_X, g_camMatR1_Y, g_camMatR1_Z, (int16_t)normRelX,
										(int16_t)normRelY, (int16_t)normRelZ);

			trig2_ctop(localX, localZ, localY);
			FVIEW_BuildCameraOrient(g_players[playerIdx].viewState.viewRoll,
									(int16_t)g_players[playerIdx].viewState.viewPitch,
									(int16_t)g_players[playerIdx].viewState.viewYaw, 0,
									(int16_t)(0x4000 - targetPitch), (int16_t)trig2_xyangle, NULL, -1);
		} else {
			ObjectRecord* playerObj;
			MobileObject* playerMobj;

			playerObj = &g_objectTable[g_players[playerIdx].objectIndex];
			if (playerObj->mobj->orientMatrixDirty) {
				FVIEW_calcrotatemove(playerObj->pitch, playerObj->yaw, playerObj);
				playerObj = &g_objectTable[g_players[playerIdx].objectIndex];
				FVIEW_calcrotateorient(playerObj->roll, playerObj->angleD, playerObj);
			}

			playerObj = &g_objectTable[g_players[playerIdx].objectIndex];
			playerMobj = playerObj->mobj;
#ifdef XWA_MODERN
			localX =
				Xwa_WrappedMulAdd3Q15(playerMobj->cachedSideY, (int16_t)normRelY, playerMobj->cachedSideX,
									  (int16_t)normRelX, playerMobj->cachedSideZ, (int16_t)normRelZ);
#else
			localX = playerMobj->cachedSideZ * (int16_t)normRelZ +
					 playerMobj->cachedSideY * (int16_t)normRelY +
					 playerMobj->cachedSideX * (int16_t)normRelX;
			localX = Xwa_SaturateWrappedQ30ToQ15(localX);
#endif
			localZ =
				Xwa_Dot3Q15Wrapped(playerMobj->cachedFwdX, playerMobj->cachedFwdY, playerMobj->cachedFwdZ,
								   (int16_t)normRelX, (int16_t)normRelY, (int16_t)normRelZ);
			localY = Xwa_Dot3Q15Wrapped(playerMobj->cachedUpX, playerMobj->cachedUpY, playerMobj->cachedUpZ,
										(int16_t)normRelX, (int16_t)normRelY, (int16_t)normRelZ);

			trig2_ctop(localX, localZ, localY);
			playerObj = &g_objectTable[g_players[playerIdx].objectIndex];
			FVIEW_BuildCameraOrient(playerObj->roll, (int16_t)playerObj->pitch, (int16_t)playerObj->yaw, 0,
									(int16_t)(0x4000 - targetPitch), (int16_t)trig2_xyangle, NULL, -1);
		}
	}

	{
		int maxBoundsExtent;
		uint32_t fitDistance;
		uint16_t fitDistanceShift;
		uint16_t cameraDistance;

		if (g_objectTable[resolvedTargetIdx].mobj != NULL) {
			if (g_objectTable[resolvedTargetIdx].mobj->pCraft != NULL) {
				ModelIndex modelIndex;

				modelIndex = GetModelIndexFromType(g_objectTable[resolvedTargetIdx].objectType);
				if (modelIndex != 0xffffu) {
					int16_t sizeX;
					int16_t sizeY;
					int16_t sizeZ;
					int largestA;
					int largestB;

					sizeX = (int16_t)g_modelDefs[modelIndex].boundSizeX;
					sizeY = (int16_t)g_modelDefs[modelIndex].boundSizeY;
					sizeZ = (int16_t)g_modelDefs[modelIndex].boundSizeZ;
					if (sizeX <= sizeY && sizeX <= sizeZ) {
						largestA = sizeY;
						largestB = sizeZ;
					} else if (sizeY <= sizeX && sizeY <= sizeZ) {
						largestA = sizeX;
						largestB = sizeZ;
					} else {
						largestA = sizeX;
						largestB = sizeY;
					}
					maxBoundsExtent = (int)((uint32_t)(largestA + largestB) >> 1)
									  << (uint8_t)g_modelDefs[modelIndex].boundSizeShift;
				} else {
					maxBoundsExtent = g_modelTypeTable[(uint16_t)g_objectTable[resolvedTargetIdx].objectType]
										  .maxBoundsExtent;
				}
			} else {
				maxBoundsExtent =
					g_modelTypeTable[(uint16_t)g_objectTable[resolvedTargetIdx].objectType].maxBoundsExtent;
			}
		} else {
			maxBoundsExtent =
				g_modelTypeTable[(uint16_t)g_objectTable[resolvedTargetIdx].objectType].maxBoundsExtent;
		}
		distanceDivisor = 102;
		if (!fitDistanceFlag) {
			distanceDivisor = 0;
		}
		fitDistance = ((uint32_t)maxBoundsExtent << 9) / (unsigned int)distanceDivisor;
		fitDistanceShift = 0;
		while (fitDistance > 0x3fffu) {
			fitDistance >>= 1;
			++fitDistanceShift;
		}

		cameraDistance = (uint16_t)(fitDistance + ((uint16_t)fitDistance >> 2));
#ifdef XWA_MODERN
		/* Preserve the scalar before the legacy Q15 projection changes its
		 * effective magnitude as the camera rotates. */
		g_hudCrtCameraDistance = (int32_t)((uint32_t)cameraDistance << (uint8_t)fitDistanceShift);
#endif
		g_players[playerIdx].viewState.savedTargetX = Xwa_Q15MulReuseFirstSlot(cameraDistance, g_camMatR2_X);
		g_players[playerIdx].viewState.savedTargetY = Xwa_Q15MulReuseFirstSlot(cameraDistance, g_camMatR2_Y);
		g_players[playerIdx].viewState.savedTargetZ = Xwa_Q15MulReuseFirstSlot(cameraDistance, g_camMatR2_Z);
		if ((uint16_t)fitDistanceShift != 0) {
			g_players[playerIdx].viewState.savedTargetX <<= fitDistanceShift;
			g_players[playerIdx].viewState.savedTargetY <<= fitDistanceShift;
			g_players[playerIdx].viewState.savedTargetZ <<= fitDistanceShift;
		}

		g_players[playerIdx].viewState.savedTargetX = worldlocx - g_players[playerIdx].viewState.savedTargetX;
		g_players[playerIdx].viewState.savedTargetY = worldlocy - g_players[playerIdx].viewState.savedTargetY;
		{
			int result;

			result = worldlocz - g_players[playerIdx].viewState.savedTargetZ;
			g_players[playerIdx].viewState.savedTargetZ = result;
			return result;
		}
	}
}

// FUNCTION: XWA 0x479950
void Hud_SetupResourceData(int group, uint16_t index) {
	uint8_t* palette;

	if (g_hudUseAlphaSpriteAtlas10100) {
		g_curImage = SpriteResource_ResolveSprite(group + g_hudAlphaSpriteGroupOffset, index);
	} else {
		g_curImage = SpriteResource_ResolveSprite(group, index);
	}

	if (g_curImage == NULL) {
		XWA_HUD_OUTPUT_DEBUG_STRING("Null pointer to image in SetupResourceData()\n");
		return;
	}

	g_curImageWidth = g_curImage->width;
	g_curImageHeight = g_curImage->height;
	g_curImageRLE = SpriteResource_GetRowData(g_curImage);
#ifdef XWA_MODERN
	palette = SpriteResource_GetMutableSpritePayload(g_curImage);
#else
	palette = g_curImage->pixels;
#endif
	g_curImagePalette = (uint16_t*)palette;
	g_curImagePalette = (uint16_t*)(palette + ((SpritePayload*)palette)->palette16Offset);
}

// FUNCTION: XWA 0x4799D0
uint8_t* Hud_DrawTarget_GetPixelPtr(int x, int y) {
	if (g_drawTarget->flipY == 1) {
		return (uint8_t*)g_drawTarget->pixels + x * g_drawTarget->bytesPerPixel + y * g_drawTarget->pitch;
	}

	return (uint8_t*)g_drawTarget->pixels + x * g_drawTarget->bytesPerPixel +
		   (g_drawTarget->maxY - y) * g_drawTarget->pitch;
}

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x479F40
__inline uint8_t* Hud_BlitSpriteType23(int16_t x, int y) {
	uint8_t* row;
	HudDrawTarget* target;
	int drawY;
	int endY;
	int visibleRows;
	volatile int pixelBits;
	volatile int paletteBias;

	row = g_curImageRLE;
	target = g_drawTarget;
	drawY = (int16_t)y;
	endY = (uint16_t)g_curImageHeight + drawY;
	pixelBits = 16;
	paletteBias = 0;
	if (endY >= target->clipY1) {
		endY = target->clipY1;
	}

	visibleRows = endY - drawY;
	if (drawY < target->clipY0) {
		int skipRows;

		skipRows = target->clipY0 - drawY;
		drawY = target->clipY0;
		visibleRows -= skipRows;
		while (skipRows != 0) {
			uint8_t runCount;
			uint8_t runCountMinusOne;

			runCount = *row++;
			runCountMinusOne = (uint8_t)(runCount - 1);
			if (runCount != 0) {
				int remainingRuns;

				remainingRuns = runCountMinusOne + 1;
				do {
					int control;
					int runLen;
					int mode;

					control = *row++;
					runLen = control & 0x3f;
					mode = control & 0xc0;
					if (mode == 0) {
						row += runLen;
					} else if (mode == 0x80) {
						row += 2 * runLen;
					}
					--remainingRuns;
				} while (remainingRuns != 0);
			}
			--skipRows;
		}
	}

	if (visibleRows == 0) {
		return row;
	}

	while (visibleRows != 0) {
		uint8_t runCount;
		uint8_t runCountMinusOne;
		int dstX;

		runCount = *row++;
		runCountMinusOne = (uint8_t)(runCount - 1);
		dstX = x;
		if (runCount != 0) {
			int remainingRuns;

			remainingRuns = runCountMinusOne + 1;
			do {
				uint8_t* runData;
				uint8_t* rowEnd;
				int control;
				int runLen;
				int mode;
				int runEndX;

				control = *row++;
				runData = row;
				runLen = control & 0x3f;
				mode = control & 0xc0;
				runEndX = dstX + runLen;

				if (mode != 0xc0) {
					rowEnd = row + runLen;
					if (mode == 0x80) {
						rowEnd = row + 2 * runLen;
					}

					if (runEndX >= target->clipX0 && dstX < target->clipX1) {
						int drawX;
						int drawLen;

						drawX = dstX;
						drawLen = runLen;
						if (drawX < target->clipX0) {
							int clippedLeft;

							clippedLeft = target->clipX0 - drawX;
							runData += clippedLeft;
							if (mode == 0x80) {
								runData += clippedLeft;
							}
							drawLen -= clippedLeft;
							drawX = target->clipX0;
						}
						if (runEndX >= target->clipX1) {
							drawLen -= runEndX - target->clipX1;
						}

						target = g_drawTarget;
						if (drawLen != 0) {
							uint8_t* dstBytes;

							if (target->flipY == 1) {
								dstBytes = (uint8_t*)target->pixels + drawX * target->bytesPerPixel +
										   drawY * target->pitch;
							} else {
								dstBytes = (uint8_t*)target->pixels + drawX * target->bytesPerPixel +
										   target->pitch * (target->maxY - drawY);
							}

							g_curImageRunRemaining = drawLen;
							if (mode == 0) {
								uint8_t* src;
								int remainingRunLen;

								src = runData;
								remainingRunLen = drawLen;
								if (pixelBits == 16 || pixelBits == 15) {
									uint8_t* palette;

									palette = (uint8_t*)g_curImagePalette + 2 * paletteBias;
									do {
										uint32_t color;

										memcpy(&color, palette + 2 * *src, sizeof(color));
										dstBytes[0] = (uint8_t)color;
										++src;
										dstBytes[1] = (uint8_t)(color >> 8);
										dstBytes += 2;
										--remainingRunLen;
									} while (remainingRunLen != 0);
								} else if (pixelBits == 8) {
									do {
										*dstBytes++ =
											(uint8_t)g_curImagePalette[(uint8_t)(paletteBias + *src++)];
										--remainingRunLen;
									} while (remainingRunLen != 0);
								} else {
									uint32_t* dstWords;

									dstWords = (uint32_t*)dstBytes;
									do {
										*dstWords++ = ((uint32_t*)g_curImagePalette)[paletteBias + *src++];
										--remainingRunLen;
									} while (remainingRunLen != 0);
								}
							} else {
								uint8_t* src;
								uint16_t* dstPixels;
								uint16_t* blendLut;

								src = runData;
								dstPixels = (uint16_t*)dstBytes;
								blendLut = g_curImageBlendLut;
								if (pixelBits == 16) {
									do {
										int alpha;

										alpha = src[0];
										if ((uint8_t)alpha == 0xff) {
											*dstPixels = g_curImagePalette[src[1]];
										} else if ((uint8_t)alpha >= 0x20u) {
											uint16_t dstColor;
											uint16_t srcColor;

											alpha >>= 5;
											dstColor = *dstPixels;
											srcColor = g_curImagePalette[src[1]];
											*dstPixels = blendLut[8 * srcColor + alpha] +
														 blendLut[8 * dstColor + 8 - alpha];
										}
										src += 2;
										++dstPixels;
										--g_curImageRunRemaining;
									} while (g_curImageRunRemaining != 0);
								} else if (pixelBits == 8) {
									uint8_t* dst;

									dst = dstBytes;
									do {
										int alpha;

										alpha = *src++;
										if (alpha >= 0x80) {
											*dst = g_curImagePalette[(uint8_t)(paletteBias + *src)];
										}
										++src;
										++dst;
										--g_curImageRunRemaining;
									} while (g_curImageRunRemaining != 0);
								}
							}

							target = g_drawTarget;
						}
					}

					row = rowEnd;
				}

				dstX = runEndX;
				--remainingRuns;
			} while (remainingRuns != 0);
		}

		++drawY;
		--visibleRows;
	}

	return row;
}
#ifndef XWA_MODERN
#pragma optimize("y", on)
#endif

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x479A20
void Hud_DrawImageToDIB(int16_t x, int16_t y) {
	HudDrawTarget* target;
	int drawY;
	int clipY1;
	int clipY0;
	int imageWidth;
	int imageHeight;
	int spriteType;
	uint8_t* row;
	int endY;
	volatile int pixelBits;
	volatile int paletteBias;
	uint8_t* volatile rowEnd;
	int visibleRows;
	int drawX;
	int remainingRuns;
	volatile int runEndX;
	uint8_t* volatile dstBytes;
	uint8_t* src;
	int drawLen;

	if (g_curImage == NULL) {
		XWA_HUD_OUTPUT_DEBUG_STRING("Null image pointer in DrawImageToDIB()!\n");
		return;
	}

	target = g_drawTarget;
	drawY = (int16_t)y;
	clipY1 = target->clipY1;
	if (drawY >= clipY1) {
		return;
	}

	imageHeight = (int16_t)g_curImage->height;
	clipY0 = target->clipY0;
	if (drawY + imageHeight < clipY0) {
		return;
	}
	drawX = (int16_t)x;
	if (drawX >= target->clipX1) {
		return;
	}
	imageWidth = (int16_t)g_curImage->width;
	if (drawX + imageWidth < target->clipX0) {
		return;
	}

	spriteType = g_curImage->type;
	if (spriteType != 7) {
		if (spriteType != 23) {
			return;
		}
		Hud_BlitSpriteType23((int16_t)x, y);
		return;
	}

	row = g_curImageRLE;
	endY = (uint16_t)g_curImageHeight + drawY;
	pixelBits = 16;
	paletteBias = 0;
	if (endY >= clipY1) {
		endY = clipY1;
	}

	visibleRows = endY - drawY;
	y = drawY;
	if (drawY < clipY0) {
		int skipRows;

		skipRows = clipY0 - drawY;
		y = clipY0;
		visibleRows -= skipRows;
		while (skipRows != 0) {
			uint8_t runCount;
			uint8_t runCountMinusOne;

			runCount = *row++;
			runCountMinusOne = (uint8_t)(runCount - 1);
			if (runCount != 0) {
				remainingRuns = runCountMinusOne + 1;
				do {
					int control;

					control = *row++;
					if ((control & 0x80) == 0) {
						row += control;
					}
					--remainingRuns;
				} while (remainingRuns != 0);
			}
			--skipRows;
		}
	}

	if (visibleRows != 0) {
		do {
			uint8_t runCount;
			uint8_t runCountMinusOne;
			int dstX;

			runCount = *row++;
			runCountMinusOne = (uint8_t)(runCount - 1);
			dstX = drawX;
			if (runCount != 0) {
				remainingRuns = runCountMinusOne + 1;
				do {
					uint8_t control;
					int runLen;

					control = *row++;
					runLen = control & 0x7f;
					runEndX = dstX + runLen;
					if ((control & 0x80u) == 0) {
						src = row;
						rowEnd = row + runLen;
						if (runEndX >= target->clipX0 && dstX < target->clipX1) {
							int drawRunX;

							drawRunX = dstX;
							drawLen = runLen;
							if (drawRunX < target->clipX0) {
								int clippedLeft;

								clippedLeft = target->clipX0 - drawRunX;
								src += clippedLeft;
								drawLen -= clippedLeft;
								drawRunX = target->clipX0;
							}
							if (runEndX >= target->clipX1) {
								drawLen -= runEndX - target->clipX1;
							}

							{
								if (target->flipY == 1) {
									dstBytes = (uint8_t*)target->pixels + target->pitch * y +
											   target->bytesPerPixel * drawRunX;
								} else {
									dstBytes = (uint8_t*)target->pixels + target->pitch * (target->maxY - y) +
											   target->bytesPerPixel * drawRunX;
								}

								if (drawLen != 0) {
									if (pixelBits != 16 && pixelBits != 15) {
										if (pixelBits == 8) {
											uint8_t* dst;
											uint8_t* palette8;

											dst = dstBytes;
											palette8 = (uint8_t*)g_curImagePalette;
											do {
												*dst++ = palette8[(uint8_t)(paletteBias + *src++)];
												--drawLen;
											} while (drawLen != 0);
										} else {
											uint32_t* dstWords;

											dstWords = (uint32_t*)dstBytes;
											do {
												*dstWords++ =
													((uint32_t*)g_curImagePalette)[paletteBias + *src++];
												--drawLen;
											} while (drawLen != 0);
										}
									} else {
										uint16_t* palette;

										palette = g_curImagePalette;
										if (paletteBias == 0) {
											uint16_t* dstPixels;

											dstPixels = (uint16_t*)dstBytes;
											do {
												uint16_t firstColor;

												firstColor = palette[*src];
												dstPixels += 2;
												--drawLen;
												if (drawLen == 0) {
													++src;
													dstPixels[-2] = firstColor;
												} else {
													dstPixels[-2] = firstColor;
													dstPixels[-1] = palette[src[1]];
													--drawLen;
													src += 2;
												}
											} while (drawLen != 0);
										} else {
											uint16_t* dstPixels;

											dstPixels = (uint16_t*)dstBytes;
											do {
												*dstPixels++ = palette[(uint8_t)(paletteBias + *src++)];
												--drawLen;
											} while (drawLen != 0);
										}
									}

									target = g_drawTarget;
								}
							}
						}

						row = rowEnd;
					}

					dstX = runEndX;
					--remainingRuns;
				} while (remainingRuns != 0);
			}

			++y;
			--visibleRows;
		} while (visibleRows != 0);
	}
}

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x479CF0
void Hud_BlitSpriteType7(int16_t x, int y) {
	uint8_t* row;
	HudDrawTarget* target;
	int drawY;
	int endY;
	int visibleRows;
	volatile int pixelBits;
	volatile int paletteBias;
	volatile int drawXBase;
	uint8_t* volatile rowEnd;
	uint8_t* src;

	row = g_curImageRLE;
	target = g_drawTarget;
	drawY = (int16_t)y;
	endY = (uint16_t)g_curImageHeight + drawY;
	pixelBits = 16;
	paletteBias = 0;
	if (endY >= target->clipY1) {
		endY = target->clipY1;
	}

	visibleRows = endY - drawY;
	drawXBase = x;
	if (drawY < target->clipY0) {
		int skipRows;

		skipRows = target->clipY0 - drawY;
		drawY = target->clipY0;
		visibleRows -= skipRows;
		while (skipRows != 0) {
			uint8_t runCount;
			uint8_t runCountMinusOne;

			runCount = *row++;
			runCountMinusOne = (uint8_t)(runCount - 1);
			if (runCount != 0) {
				int remainingRuns;

				remainingRuns = runCountMinusOne + 1;
				do {
					int control;

					control = *row++;
					if ((control & 0x80) == 0) {
						row += control;
					}
					--remainingRuns;
				} while (remainingRuns != 0);
			}
			--skipRows;
		}
	}

	if (visibleRows == 0) {
		return;
	}

	while (visibleRows != 0) {
		uint8_t runCount;
		uint8_t runCountMinusOne;
		int dstX;

		runCount = *row++;
		runCountMinusOne = (uint8_t)(runCount - 1);
		dstX = drawXBase;
		if (runCount != 0) {
			int remainingRuns;

			remainingRuns = runCountMinusOne + 1;
			do {
				uint8_t control;
				int runLen;
				int runEndX;

				control = *row++;
				runLen = control & 0x7f;
				runEndX = dstX + runLen;
				if ((control & 0x80u) == 0) {

					src = row;
					rowEnd = row + runLen;
					if (runEndX >= target->clipX0 && dstX < target->clipX1) {
						int drawX;
						int drawLen;

						drawX = dstX;
						drawLen = runLen;
						if (drawX < target->clipX0) {
							int clippedLeft;

							clippedLeft = target->clipX0 - drawX;
							src += clippedLeft;
							drawLen -= clippedLeft;
							drawX = target->clipX0;
						}
						if (runEndX >= target->clipX1) {
							drawLen -= runEndX - target->clipX1;
						}

						{
							uint8_t* dstBytes;

							if (target->flipY == 1) {
								dstBytes = (uint8_t*)target->pixels + target->pitch * drawY +
										   target->bytesPerPixel * drawX;
							} else {
								dstBytes = (uint8_t*)target->pixels + target->pitch * (target->maxY - drawY) +
										   target->bytesPerPixel * drawX;
							}

							if (drawLen != 0) {
								if (pixelBits != 16 && pixelBits != 15) {
									if (pixelBits == 8) {
										uint8_t* dst;
										uint8_t* palette8;

										dst = dstBytes;
										palette8 = (uint8_t*)g_curImagePalette;
										do {
											*dst++ = palette8[(uint8_t)(paletteBias + *src++)];
											--drawLen;
										} while (drawLen != 0);
									} else {
										uint32_t* dstWords;

										dstWords = (uint32_t*)dstBytes;
										do {
											*dstWords++ =
												((uint32_t*)g_curImagePalette)[paletteBias + *src++];
											--drawLen;
										} while (drawLen != 0);
									}
								} else {
									uint16_t* palette;

									palette = g_curImagePalette;
									if (paletteBias == 0) {
										uint16_t* dstPixels;

										dstPixels = (uint16_t*)dstBytes;
										do {
											uint16_t firstColor;

											firstColor = palette[*src];
											dstPixels += 2;
											--drawLen;
											if (drawLen == 0) {
												++src;
												dstPixels[-2] = firstColor;
											} else {
												dstPixels[-2] = firstColor;
												dstPixels[-1] = palette[src[1]];
												--drawLen;
												src += 2;
											}
										} while (drawLen != 0);
									} else {
										int remainingPixels;
										uint16_t* dstPixels;

										remainingPixels = drawLen;
										dstPixels = (uint16_t*)dstBytes;
										do {
											*dstPixels++ = palette[(uint8_t)(paletteBias + *src++)];
											--remainingPixels;
										} while (remainingPixels != 0);
									}
								}

								target = g_drawTarget;
							}
						}
					}

					row = rowEnd;
				}

				dstX = runEndX;
				--remainingRuns;
			} while (remainingRuns != 0);
		}

		++drawY;
		--visibleRows;
	}
}
#ifndef XWA_MODERN
#pragma optimize("y", on)
#endif

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x473D00
void Hud_UpdateHUDText(void) {
#ifdef XWA_MODERN
	CraftData* craft;
	MobileObject* playerMobj;

	craft = Hud_GetCraftPointerInlined();
	if (craft == NULL) {
		OutputDebugStringA("NULL craft data pointer in UpdateHUDText()!\n");
		return;
	}

	playerMobj = g_objectTable[g_players[g_localPlayer].objectIndex].mobj;
	Hud_DrawSpeedTextPanel(playerMobj);
	Hud_DrawThrottleTextPanel(craft);
	Hud_DrawCraftNameTextPanel();
	Hud_DrawMissionClockTextPanel();
	if (g_provingGroundsModeActive) {
		Hud_DrawProvingGroundStatusTextPanel();
		return;
	}

	{
		uint16_t warheadLauncherCount;
		ObjectTypeId objectType;

		warheadLauncherCount = craft->warheadLauncherCount;
		objectType = (ObjectTypeId)g_objectTable[g_players[g_localPlayer].objectIndex].objectType;
		if (objectType != OBJ_None) {
			ModelIndex modelIndex;

			modelIndex = GetModelIndexFromType(objectType);
			if (modelIndex != 0xffffu) {
				ModelDef* modelDef;
				uint16_t launcherSlotCount;

				modelDef = &g_modelDefs[modelIndex];
				launcherSlotCount =
					(uint16_t)(modelDef->warheadLauncherSlotCount[0] + modelDef->warheadLauncherSlotCount[1]);
				if (warheadLauncherCount == 0 || launcherSlotCount == 0) {
					HUD_PANE_PUSH(XWA_HUD_PANE_TOP_WEAPONS, g_hudWarheadCountTextX, g_hudWarheadCountTextY,
								  g_hudWarheadCountTextSurfaceWidth, g_hudWarheadCountTextSurfaceHeight);
					if (g_useHardware3D) {
						FlightText_SetRenderOffset((int16_t)g_hudWarheadCountTextX,
												   (int16_t)g_hudWarheadCountTextY);
					} else {
						FlightSw_SetRenderTarget(
							hudTex6, (uint16_t)g_hudWarheadCountTextSurfaceWidth,
							(uint16_t)g_hudWarheadCountTextSurfaceHeight,
							(uint16_t)(g_hudWarheadCountTextSurfaceWidth * g_flight16bppBytesPerPixel));
					}
					FlightText_SetClipRect(0, 0, (uint16_t)g_hudWarheadCountTextSurfaceWidth,
										   (uint16_t)g_hudWarheadCountTextSurfaceHeight);
					FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
					if (!g_useHardware3D) {
						g_flightFillClipRectFn();
					}
					FlightText_SetColor(0x43u);
					FlightText_SetFontTier(0);
					FlightText_SetCursor(0, 0);
					FlightText_DrawDecimalNumber(0, 2u, 1u);
					if (g_useHardware3D) {
						FlightText_SetRenderOffset(0, 0);
					} else {
						FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
						Blit16ToFlightSurface(
							hudTex6, g_flightColorEscapeBypassChar, 0, 0, (uint16_t)(g_hudCenterX - 25), 11u,
							(uint16_t)g_hudWarheadCountTextSurfaceWidth,
							(uint16_t)g_hudWarheadCountTextSurfaceHeight,
							(uint16_t)(g_flight16bppBytesPerPixel * g_hudWarheadCountTextSurfaceWidth));
					}
					HUD_PANE_POP();
				} else if (warheadLauncherCount == 1u) {
					HUD_PANE_PUSH(XWA_HUD_PANE_TOP_WEAPONS, g_hudWarheadCountTextX, g_hudWarheadCountTextY,
								  g_hudWarheadCountTextSurfaceWidth, g_hudWarheadCountTextSurfaceHeight);
					if (g_useHardware3D) {
						FlightText_SetRenderOffset((int16_t)g_hudWarheadCountTextX,
												   (int16_t)g_hudWarheadCountTextY);
					} else {
						FlightSw_SetRenderTarget(
							hudTex6, (uint16_t)g_hudWarheadCountTextSurfaceWidth,
							(uint16_t)g_hudWarheadCountTextSurfaceHeight,
							(uint16_t)(g_hudWarheadCountTextSurfaceWidth * g_flight16bppBytesPerPixel));
					}
					FlightText_SetClipRect(0, 0, (uint16_t)g_hudWarheadCountTextSurfaceWidth,
										   (uint16_t)g_hudWarheadCountTextSurfaceHeight);
					FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
					if (!g_useHardware3D) {
						g_flightFillClipRectFn();
					}
					FlightText_SetColor(0x43u);
					FlightText_SetFontTier(0);
					g_flightTextShadowEnabled = 0;
					FlightText_SetCursor(0, 0);
					FlightText_DrawDecimalNumber(
						craft->warheadData[modelDef->warheadLauncherFirstSlot[0]].count, 2u, 1u);
					FlightText_SetCursor((int16_t)FlightText_MeasureStringWidth("00 "), 0);
					FlightText_DrawString(":");
					FlightText_SetCursor((int16_t)FlightText_MeasureStringWidth("00 : "), 0);
					FlightText_DrawDecimalNumber(
						craft->warheadData[modelDef->warheadLauncherLastSlot[0]].count, 2u, 1u);
					if (g_useHardware3D) {
						FlightText_SetRenderOffset(0, 0);
					} else {
						FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
						Blit16ToFlightSurface(
							hudTex6, g_flightColorEscapeBypassChar, 0, 0, (uint16_t)(g_hudCenterX - 25), 11u,
							(uint16_t)g_hudWarheadCountTextSurfaceWidth,
							(uint16_t)g_hudWarheadCountTextSurfaceHeight,
							(uint16_t)(g_flight16bppBytesPerPixel * g_hudWarheadCountTextSurfaceWidth));
					}
					HUD_PANE_POP();
				} else if (warheadLauncherCount == 2u) {
					uint16_t cursorX;

					HUD_PANE_PUSH(XWA_HUD_PANE_TOP_WEAPONS, g_hudDualWarheadCountTextX,
								  g_hudWarheadCountTextY, g_hudWarheadCountTextSurfaceWidth,
								  g_hudWarheadCountTextSurfaceHeight);
					if (g_useHardware3D) {
						FlightText_SetRenderOffset((int16_t)g_hudDualWarheadCountTextX,
												   (int16_t)g_hudWarheadCountTextY);
					} else {
						FlightSw_SetRenderTarget(
							hudTex6, (uint16_t)g_hudWarheadCountTextSurfaceWidth,
							(uint16_t)g_hudWarheadCountTextSurfaceHeight,
							(uint16_t)(g_hudWarheadCountTextSurfaceWidth * g_flight16bppBytesPerPixel));
					}
					FlightText_SetClipRect(0, 0, (uint16_t)g_hudWarheadCountTextSurfaceWidth,
										   (uint16_t)g_hudWarheadCountTextSurfaceHeight);
					FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
					if (!g_useHardware3D) {
						g_flightFillClipRectFn();
					}
					FlightText_SetColor(0x43u);
					FlightText_SetFontTier(0);
					FlightText_SetCursor(0, 0);
					FlightText_DrawDecimalNumber(
						craft->warheadData[modelDef->warheadLauncherFirstSlot[0]].count, 2u, 1u);
					cursorX = FlightText_MeasureStringWidth("00");
					FlightText_SetCursor((int16_t)cursorX, 0);
					FlightText_DrawString(":");
					cursorX = (uint16_t)(cursorX + FlightText_MeasureStringWidth(":"));
					FlightText_SetCursor((int16_t)cursorX, 0);
					FlightText_DrawDecimalNumber(
						craft->warheadData[modelDef->warheadLauncherLastSlot[0]].count, 2u, 1u);
					cursorX = (uint16_t)(cursorX + FlightText_MeasureStringWidth("00") +
										 FlightText_MeasureStringWidth("-"));
					FlightText_SetCursor((int16_t)cursorX, 0);
					FlightText_DrawDecimalNumber(
						craft->warheadData[modelDef->warheadLauncherFirstSlot[1]].count, 2u, 1u);
					cursorX = (uint16_t)(cursorX + FlightText_MeasureStringWidth("00"));
					FlightText_SetCursor((int16_t)cursorX, 0);
					FlightText_DrawString(":");
					cursorX = (uint16_t)(cursorX + FlightText_MeasureStringWidth(":"));
					FlightText_SetCursor((int16_t)cursorX, 0);
					FlightText_DrawDecimalNumber(
						craft->warheadData[modelDef->warheadLauncherLastSlot[1]].count, 2u, 1u);
					if (g_useHardware3D) {
						FlightText_SetRenderOffset(0, 0);
					} else {
						FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
						Blit16ToFlightSurface(
							hudTex6, g_flightColorEscapeBypassChar, 0, 0, (uint16_t)(g_hudCenterX - 32), 11u,
							(uint16_t)g_hudWarheadCountTextSurfaceWidth,
							(uint16_t)g_hudWarheadCountTextSurfaceHeight,
							(uint16_t)(g_flight16bppBytesPerPixel * g_hudWarheadCountTextSurfaceWidth));
					}
					HUD_PANE_POP();
				}
			}
		}
	}

	{
		uint16_t cmAmmoCount;

		cmAmmoCount = craft->cmAmmoCount;
		HUD_PANE_PUSH(XWA_HUD_PANE_TOP_COUNTERMEASURE, g_hudCountermeasureCountTextX,
					  g_hudCountermeasureCountTextY, g_hudCountermeasureCountTextSurfaceWidth,
					  g_hudCountermeasureCountTextSurfaceHeight);
		if (g_useHardware3D) {
			FlightText_SetRenderOffset((int16_t)g_hudCountermeasureCountTextX,
									   (int16_t)g_hudCountermeasureCountTextY);
		} else {
			FlightSw_SetRenderTarget(
				hudTex13, (uint16_t)g_hudCountermeasureCountTextSurfaceWidth,
				(uint16_t)g_hudCountermeasureCountTextSurfaceHeight,
				(uint16_t)(g_hudCountermeasureCountTextSurfaceWidth * g_flight16bppBytesPerPixel));
		}
		FlightText_SetClipRect(0, 0, (uint16_t)g_hudCountermeasureCountTextSurfaceWidth,
							   (uint16_t)g_hudCountermeasureCountTextSurfaceHeight);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_SetColor(0x43u);
		FlightText_SetFontTier(0);
		FlightText_SetCursor(0, 0);
		FlightText_DrawDecimalNumber(cmAmmoCount, 2u, 1u);
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
		} else {
			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			Blit16ToFlightSurface(
				hudTex13, g_flightColorEscapeBypassChar, 0, 0, (uint16_t)(g_hudCenterX + 47), 11u,
				(uint16_t)g_hudCountermeasureCountTextSurfaceWidth,
				(uint16_t)g_hudCountermeasureCountTextSurfaceHeight,
				(uint16_t)(g_flight16bppBytesPerPixel * g_hudCountermeasureCountTextSurfaceWidth));
		}
		HUD_PANE_POP();
	}

	if (!g_provingGroundsModeActive && !g_useHardware3D) {
		Hud_SetupResourceData(10000, 0x1324u);
		Hud_DrawImageToDIB(g_hudCenterX - 62, 10);
		Hud_SetupResourceData(10000, 0x1388u);
		Hud_DrawImageToDIB(g_hudCenterX + 20, 10);
	}
#else
	CraftData* craft;
	MobileObject* playerMobj;

	craft = Hud_GetCraftPointerInlined();
	if (craft == NULL) {
		XWA_HUD_OUTPUT_DEBUG_STRING("NULL craft data pointer in UpdateHUDText()!\n");
		return;
	}

	playerMobj = g_objectTable[g_players[g_localPlayer].objectIndex].mobj;
	{
		int labelWidth;
		uint16_t speed;

		speed = (uint16_t)MATH2_fraction(playerMobj->speed, 0x71c7u);
		if (g_useHardware3D) {
			FlightText_SetRenderOffset((int16_t)g_hudSpeedTextX, (int16_t)g_hudSpeedTextY);
		} else {
			FlightSw_SetRenderTarget(hudTex9, (uint16_t)g_hudSpeedTextSurfaceWidth,
									 (uint16_t)g_hudSpeedTextSurfaceHeight,
									 (uint16_t)g_hudSpeedTextSurfaceWidth * g_flight16bppBytesPerPixel);
		}
		FlightText_SetClipRect(0, 0, (uint16_t)g_hudSpeedTextSurfaceWidth,
							   (uint16_t)g_hudSpeedTextSurfaceHeight);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_SetColor(0x43u);
		FlightText_SetFontTier(0);
		FlightText_SetCursor(0, 0);
		FlightText_DrawString(g_strOverlayStrings[1]);
		labelWidth = FlightText_MeasureStringWidth(g_strOverlayStrings[3]);
		FlightText_SetCursor(labelWidth + FlightText_MeasureStringWidth(" "), 0);
		if (g_players[g_localPlayer].hyperspacePhase == PLAYER_HYPERSPACE_REGION_TRANSFER) {
			FlightText_DrawDecimalNumber(999u, 3u, 1u);
			FlightText_DrawString("+");
		} else {
			FlightText_DrawDecimalNumber(speed, 3u, 1u);
		}
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
		} else {
			int width;

			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			width = g_hudSpeedTextSurfaceWidth;
			Blit16ToFlightSurface(hudTex9, g_flightColorEscapeBypassChar, 0, 0,
								  (uint16_t)(g_hudCenterX - 160), 2u, (uint16_t)width,
								  (uint16_t)g_hudSpeedTextSurfaceHeight, width * g_flight16bppBytesPerPixel);
		}
	}

	if ((craft->activeHudFeatureMask & 0x40u) != 0) {
		int labelWidth;
		int throttlePercent;

		throttlePercent = craft->throttleSpeed / 655;
		if (craft->slamActive != 0) {
			throttlePercent <<= 1;
		}
		if (g_useHardware3D) {
			FlightText_SetRenderOffset((int16_t)g_hudThrottleTextX, (int16_t)g_hudThrottleTextY);
		} else {
			FlightSw_SetRenderTarget(hudTex10, (uint16_t)g_hudThrottleTextSurfaceWidth,
									 (uint16_t)g_hudThrottleTextSurfaceHeight,
									 (uint16_t)g_hudThrottleTextSurfaceWidth * g_flight16bppBytesPerPixel);
		}
		FlightText_SetClipRect(0, 0, (uint16_t)g_hudThrottleTextSurfaceWidth,
							   (uint16_t)g_hudThrottleTextSurfaceHeight);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_SetColor(0x43u);
		FlightText_SetFontTier(0);
		FlightText_SetCursor(0, 0);
		labelWidth = FlightText_MeasureStringWidth(g_strOverlayStrings[3]);
		FlightText_DrawString(g_strOverlayStrings[3]);
		FlightText_SetCursor(labelWidth + FlightText_MeasureStringWidth(" "), 0);
		FlightText_DrawDecimalNumber((uint16_t)throttlePercent, 3u, 1u);
		g_flightDrawCharFn('%');
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
		} else {
			int width;

			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			width = g_hudThrottleTextSurfaceWidth;
			Blit16ToFlightSurface(hudTex10, g_flightColorEscapeBypassChar, 0, 0,
								  (uint16_t)(g_hudCenterX - 160), 11u, (uint16_t)width,
								  (uint16_t)g_hudThrottleTextSurfaceHeight,
								  width * g_flight16bppBytesPerPixel);
		}
	}

	Hud_AppendObjectDisplayName((uint16_t)g_players[g_localPlayer].objectIndex, 3);
	if (g_useHardware3D) {
		FlightText_SetRenderOffset((int16_t)g_hudCraftNameTextX, (int16_t)g_hudCraftNameTextY);
	} else {
		FlightSw_SetRenderTarget(hudTex11, (uint16_t)g_hudCraftNameTextSurfaceWidth,
								 (uint16_t)g_hudCraftNameTextSurfaceHeight,
								 (uint16_t)g_hudCraftNameTextSurfaceWidth * g_flight16bppBytesPerPixel);
	}
	FlightText_SetClipRect(0, 0, (uint16_t)g_hudCraftNameTextSurfaceWidth,
						   (uint16_t)g_hudCraftNameTextSurfaceHeight);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	if (!g_useHardware3D) {
		g_flightFillClipRectFn();
	}
	FlightText_SetColor(0x43u);
	FlightText_SetFontTier(0);
	FlightText_SetCursor(0, 0);
	FlightText_DrawString(g_flightTextScratchBuffer);
	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		int width;

		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		width = g_hudCraftNameTextSurfaceWidth;
		Blit16ToFlightSurface(hudTex11, g_flightColorEscapeBypassChar, 0, 0, (uint16_t)(g_hudCenterX + 100),
							  2u, (uint16_t)width, (uint16_t)g_hudCraftNameTextSurfaceHeight,
							  width * g_flight16bppBytesPerPixel);
	}

	{
		int useElapsedClock;
		int labelWidth;

		if (g_useHardware3D) {
			FlightText_SetRenderOffset((int16_t)g_hudMissionClockTextX, (int16_t)g_hudMissionClockTextY);
		} else {
			FlightSw_SetRenderTarget(hudTex12, (uint16_t)g_hudMissionClockTextSurfaceWidth,
									 (uint16_t)g_hudMissionClockTextSurfaceHeight,
									 (uint16_t)g_hudMissionClockTextSurfaceWidth *
										 g_flight16bppBytesPerPixel);
		}
		FlightText_SetClipRect(0, 0, (uint16_t)g_hudMissionClockTextSurfaceWidth,
							   (uint16_t)g_hudMissionClockTextSurfaceHeight);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_SetColor(0x43u);
		FlightText_SetFontTier(0);
		FlightText_SetCursor(0, 0);
		FlightText_DrawString(g_strOverlayStrings[4]);
		labelWidth = FlightText_MeasureStringWidth(g_strOverlayStrings[4]);
		FlightText_SetCursor(labelWidth + FlightText_MeasureStringWidth(" "), 0);
		useElapsedClock = 0;
		if (g_provingGroundsModeActive && !g_yardContext.playerChallengeStates[g_localPlayer].finished) {
			useElapsedClock = 1;
		}
		if (!g_missionTimeLimitActive || useElapsedClock) {
			FlightText_DrawDecimalNumber(g_missionElapsedClock.minutes, 2u, 1u);
		} else {
			FlightText_DrawDecimalNumber(g_missionCountdownClock.minutes, 2u, 2u);
		}
		g_flightDrawCharFn(':');
		if (!g_missionTimeLimitActive || useElapsedClock) {
			FlightText_DrawDecimalNumber(g_missionElapsedClock.seconds, 2u, 2u);
		} else {
			FlightText_DrawDecimalNumber(g_missionCountdownClock.seconds, 2u, 2u);
		}
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
		} else {
			int width;

			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			width = g_hudMissionClockTextSurfaceWidth;
			Blit16ToFlightSurface(hudTex12, g_flightColorEscapeBypassChar, 0, 0, g_hudMissionClockTextX,
								  g_hudMissionClockTextY, (uint16_t)width,
								  (uint16_t)g_hudMissionClockTextSurfaceHeight,
								  width * g_flight16bppBytesPerPixel);
		}
	}

	if (g_provingGroundsModeActive) {
		uint16_t nowSeconds;

		if (g_useHardware3D) {
			FlightText_SetRenderOffset((int16_t)g_hudProvingGroundStatusTextX,
									   (int16_t)g_hudProvingGroundStatusTextY);
		} else {
			FlightSw_SetRenderTarget(hudTex15, (uint16_t)g_hudProvingGroundStatusTextSurfaceWidth,
									 (uint16_t)g_hudProvingGroundStatusTextSurfaceHeight,
									 (uint16_t)g_hudProvingGroundStatusTextSurfaceWidth *
										 g_flight16bppBytesPerPixel);
		}
		FlightText_SetClipRect(0, 0, (uint16_t)g_hudProvingGroundStatusTextSurfaceWidth,
							   (uint16_t)g_hudProvingGroundStatusTextSurfaceHeight);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_SetFontTier(0);
		FlightText_SetCursor(0, 0);
		nowSeconds = (uint16_t)Mission_GameTimeToSeconds(
			g_missionElapsedClock.hours, g_missionElapsedClock.minutes, g_missionElapsedClock.seconds);
		if (g_yardContext.playerChallengeStates[g_localPlayer].penaltyUntilSeconds > nowSeconds) {
			FlightText_SetColor(0x43u);
			FlightText_DrawString(g_strHangarMiscStrings[HANGAR_MISC_YARD_HUD_PENALTY]);
			FlightText_DrawString(":  ");
			FlightText_DrawDecimalNumber(
				(uint16_t)(g_yardContext.playerChallengeStates[g_localPlayer].penaltyUntilSeconds -
						   nowSeconds),
				4u, 1u);
		} else if (g_yardContext.playerChallengeStates[g_localPlayer].finished) {
			int minutes;
			int seconds;

			FlightText_SetColor(0x4bu);
			minutes = g_yardContext.playerChallengeStates[g_localPlayer].finishTimeSeconds / 60;
			seconds = g_yardContext.playerChallengeStates[g_localPlayer].finishTimeSeconds % 60;
			FlightText_DrawString(g_strHangarMiscStrings[HANGAR_MISC_COMPLETED]);
			FlightText_DrawDecimalNumber((uint16_t)minutes, 2u, 1u);
			g_flightDrawCharFn(':');
			FlightText_DrawDecimalNumber((uint16_t)seconds, 2u, 2u);
		} else if (g_yardChallengeMode < 6u) {
			FlightText_SetColor(0x4au);
			FlightText_DrawString(g_strHangarMiscStrings[HANGAR_MISC_YARD_HUD_RINGS]);
			FlightText_DrawDecimalNumber(
				(uint16_t)g_yardContext.playerChallengeStates[g_localPlayer].remainingCheckpointCount, 3u,
				1u);
			FlightText_DrawString(g_strHangarMiscStrings[HANGAR_MISC_YARD_HUD_LAPS]);
			FlightText_DrawDecimalNumber(
				(uint16_t)g_yardContext.playerChallengeStates[g_localPlayer].lapsRemaining, 3u, 1u);
		} else if (g_yardContext.playerChallengeStates[g_localPlayer].carriedObjectPickedUp) {
			FlightText_SetColor(0x4bu);
			FlightText_DrawString(g_strHangarMiscStrings[HANGAR_MISC_YARD_HUD_GOT_R2]);
		}
		FlightText_SetColor(0x43u);
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
		} else {
			int width;

			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			width = g_hudProvingGroundStatusTextSurfaceWidth;
			Blit16ToFlightSurface(hudTex15, g_flightColorEscapeBypassChar, 0, 0, 0x113u, 11u, (uint16_t)width,
								  (uint16_t)g_hudProvingGroundStatusTextSurfaceHeight,
								  (uint16_t)(width * g_flight16bppBytesPerPixel));
		}
	}

	if (!g_provingGroundsModeActive) {
		Hud_DrawTopWarheadCountTextPanel(craft);
		{
			uint16_t cmAmmoCount;

			cmAmmoCount = craft->cmAmmoCount;
			if (g_useHardware3D) {
				FlightText_SetRenderOffset((int16_t)g_hudCountermeasureCountTextX,
										   (int16_t)g_hudCountermeasureCountTextY);
			} else {
				FlightSw_SetRenderTarget(hudTex13, (uint16_t)g_hudCountermeasureCountTextSurfaceWidth,
										 (uint16_t)g_hudCountermeasureCountTextSurfaceHeight,
										 (uint16_t)g_hudCountermeasureCountTextSurfaceWidth *
											 g_flight16bppBytesPerPixel);
			}
			FlightText_SetClipRect(0, 0, (uint16_t)g_hudCountermeasureCountTextSurfaceWidth,
								   (uint16_t)g_hudCountermeasureCountTextSurfaceHeight);
			FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
			if (!g_useHardware3D) {
				g_flightFillClipRectFn();
			}
			FlightText_SetColor(0x43u);
			FlightText_SetFontTier(0);
			FlightText_SetCursor(0, 0);
			FlightText_DrawDecimalNumber(cmAmmoCount, 2u, 1u);
			if (g_useHardware3D) {
				FlightText_SetRenderOffset(0, 0);
			} else {
				int width;

				FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
				width = g_hudCountermeasureCountTextSurfaceWidth;
				Blit16ToFlightSurface(hudTex13, g_flightColorEscapeBypassChar, 0, 0,
									  (uint16_t)(g_hudCenterX + 47), 11u, (uint16_t)width,
									  (uint16_t)g_hudCountermeasureCountTextSurfaceHeight,
									  width * g_flight16bppBytesPerPixel);
			}
		}
	}
	if (!g_provingGroundsModeActive && !g_useHardware3D) {
		Hud_SetupResourceData(10000, 0x1324u);
		Hud_DrawImageToDIB(g_hudCenterX - 62, 10);
		Hud_SetupResourceData(10000, 0x1388u);
		{
			int x;
			int y;

			HudDrawTarget* target;
			int drawY;
			int clipY1;
			int clipY0;
			int imageWidth;
			int imageHeight;
			int spriteType;
			uint8_t* row;
			int endY;
			volatile int pixelBits;
			volatile int paletteBias;
			uint8_t* volatile rowEnd;
			int visibleRows;
			int drawX;
			int remainingRuns;
			volatile int runEndX;
			uint8_t* volatile dstBytes;
			uint8_t* src;
			int drawLen;

			x = g_hudCenterX + 20;
			y = 10;
			if (g_curImage == NULL) {
				XWA_HUD_OUTPUT_DEBUG_STRING("Null image pointer in DrawImageToDIB()!\n");
				return;
			}

			target = g_drawTarget;
			drawY = (int16_t)y;
			clipY1 = target->clipY1;
			if (drawY >= clipY1) {
				return;
			}

			imageHeight = (int16_t)g_curImage->height;
			clipY0 = target->clipY0;
			if (drawY + imageHeight < clipY0) {
				return;
			}
			drawX = (int16_t)x;
			if (drawX >= target->clipX1) {
				return;
			}
			imageWidth = (int16_t)g_curImage->width;
			if (drawX + imageWidth < target->clipX0) {
				return;
			}

			spriteType = g_curImage->type;
			if (spriteType != 7) {
				if (spriteType != 23) {
					return;
				}
				Hud_BlitSpriteType23((int16_t)x, y);
				return;
			}

			row = g_curImageRLE;
			endY = (uint16_t)g_curImageHeight + drawY;
			pixelBits = 16;
			paletteBias = 0;
			if (endY >= clipY1) {
				endY = clipY1;
			}

			visibleRows = endY - drawY;
			y = drawY;
			if (drawY < clipY0) {
				int skipRows;

				skipRows = clipY0 - drawY;
				y = clipY0;
				visibleRows -= skipRows;
				while (skipRows != 0) {
					uint8_t runCount;
					uint8_t runCountMinusOne;

					runCount = *row++;
					runCountMinusOne = (uint8_t)(runCount - 1);
					if (runCount != 0) {
						remainingRuns = runCountMinusOne + 1;
						do {
							int control;

							control = *row++;
							if ((control & 0x80) == 0) {
								row += control;
							}
							--remainingRuns;
						} while (remainingRuns != 0);
					}
					--skipRows;
				}
			}

			if (visibleRows != 0) {
				do {
					uint8_t runCount;
					uint8_t runCountMinusOne;
					int dstX;

					runCount = *row++;
					runCountMinusOne = (uint8_t)(runCount - 1);
					dstX = drawX;
					if (runCount != 0) {
						remainingRuns = runCountMinusOne + 1;
						do {
							uint8_t control;
							int runLen;

							control = *row++;
							runLen = control & 0x7f;
							runEndX = dstX + runLen;
							if ((control & 0x80u) == 0) {
								src = row;
								rowEnd = row + runLen;
								if (runEndX >= target->clipX0 && dstX < target->clipX1) {
									int drawRunX;

									drawRunX = dstX;
									drawLen = runLen;
									if (drawRunX < target->clipX0) {
										int clippedLeft;

										clippedLeft = target->clipX0 - drawRunX;
										src += clippedLeft;
										drawLen -= clippedLeft;
										drawRunX = target->clipX0;
									}
									if (runEndX >= target->clipX1) {
										drawLen -= runEndX - target->clipX1;
									}

									{
										if (target->flipY == 1) {
											dstBytes = (uint8_t*)target->pixels + target->pitch * y +
													   target->bytesPerPixel * drawRunX;
										} else {
											dstBytes = (uint8_t*)target->pixels +
													   target->pitch * (target->maxY - y) +
													   target->bytesPerPixel * drawRunX;
										}

										if (drawLen != 0) {
											if (pixelBits != 16 && pixelBits != 15) {
												if (pixelBits == 8) {
													uint8_t* dst;
													uint8_t* palette8;

													dst = dstBytes;
													palette8 = (uint8_t*)g_curImagePalette;
													do {
														*dst++ = palette8[(uint8_t)(paletteBias + *src++)];
														--drawLen;
													} while (drawLen != 0);
												} else {
													uint32_t* dstWords;

													dstWords = (uint32_t*)dstBytes;
													do {
														*dstWords++ =
															((uint32_t*)
																 g_curImagePalette)[paletteBias + *src++];
														--drawLen;
													} while (drawLen != 0);
												}
											} else {
												uint16_t* palette;

												palette = g_curImagePalette;
												if (paletteBias == 0) {
													uint16_t* dstPixels;

													dstPixels = (uint16_t*)dstBytes;
													do {
														uint16_t firstColor;

														firstColor = palette[*src];
														dstPixels += 2;
														--drawLen;
														if (drawLen == 0) {
															++src;
															dstPixels[-2] = firstColor;
														} else {
															dstPixels[-2] = firstColor;
															dstPixels[-1] = palette[src[1]];
															--drawLen;
															src += 2;
														}
													} while (drawLen != 0);
												} else {
													uint16_t* dstPixels;

													dstPixels = (uint16_t*)dstBytes;
													do {
														*dstPixels++ =
															palette[(uint8_t)(paletteBias + *src++)];
														--drawLen;
													} while (drawLen != 0);
												}
											}

											target = g_drawTarget;
										}
									}
								}

								row = rowEnd;
							}

							dstX = runEndX;
							--remainingRuns;
						} while (remainingRuns != 0);
					}

					++y;
					--visibleRows;
				} while (visibleRows != 0);
			}
		}
	}
#endif
}
#ifndef XWA_MODERN
#pragma optimize("y", on)
#endif

// FUNCTION: XWA 0x47A280
void Hud_DrawFpsOverlay(void) {
	float fpsAverage;
	char buffer[96];

	FlightText_SetFontTier(0);
	HUD_PANE_PUSH(XWA_HUD_PANE_FPS, 0, 0, g_screenWidth, g_screenHeight);
	if (g_useHardware3D) {
		FlightText_SetRenderOffset(g_hudCenterX - 23, g_screenHeight - (g_flightFontLineHeight >> 2) -
														  g_flightFontLineHeight - 6);
	} else {
		FlightSw_SetRenderTarget(g_hudFpsCountPixels, 50, 15u, 50 * g_flight16bppBytesPerPixel);
	}

	FlightText_SetClipRect(0, 0, 50u, 15u);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	if (g_useHardware3D) {
		if (g_flightRenderStatTexCreateFailed) {
			FlightText_SetColor(0x4Eu);
		} else if (g_flightRenderStatTexCacheOverflow) {
			FlightText_SetColor(0x4Au);
		} else {
			FlightText_SetColor(0x43u);
		}
	} else {
		FlightText_SetColor(0x43u);
	}
	g_flightTextShadowEnabled = 1;
	FlightText_SetShadowColor(0x40u);
	fpsAverage =
		(g_fpsSampleHistory[1] + g_fpsSampleHistory[2] + g_fpsSampleHistory[3] + g_fpsSampleHistory[4]) *
		g_hudFpsAverageScale;
	FlightText_SetCursor(0, 0);

	if (g_useHardware3D) {
		switch (g_flightFpsOverlayMode) {
			case 1:
				sprintf(buffer, "%3.1f  FPS", (double)fpsAverage);
				break;
			case 2:
				sprintf(buffer, "%3.1f %ld %ld %ld %ld", (double)fpsAverage,
						(long)g_flightRenderStatVertCount, (long)g_flightRenderStatTriCount,
						(long)g_flightRenderStatTexSwitches, (long)g_flightRenderStatStateChanges);
				break;
			case 3:
				sprintf(buffer, "%3.1f %ld %ld", (double)fpsAverage, (long)g_flightRenderStatBytesPurged,
						(long)g_flightRenderStatTexMemUsedBytes);
				break;
			default:
				break;
		}
	} else {
		g_flightFillClipRectFn();
		sprintf(buffer, "%3.1f  FPS", (double)fpsAverage);
	}

	FlightText_SetCursor(0, 3);
	FlightText_DrawStringCentered(buffer);
	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		FlightSurface_Lock();
		Blit16ToFlightSurface(g_hudFpsCountPixels, g_flightColorEscapeBypassChar, 0, 0,
							  (uint16_t)(g_hudCenterX - 23), (uint16_t)(g_screenHeight - 12), 50u, 15u,
							  (uint16_t)(50 * g_flight16bppBytesPerPixel));
		FlightSurface_Unlock();
	}
	HUD_PANE_POP();
}

// FUNCTION: XWA 0x47A4D0
void Hud_BlitSoftwareHudTextPanes(void) {
	if (g_useHardware3D) {
		return;
	}

	FlightSurface_Lock();
	Blit16ToFlightSurface(g_hudSystemMessagePaneSurface, g_flightColorEscapeBypassChar, 0, 0,
						  (uint16_t)g_hudSystemMessagePaneX, (uint16_t)g_hudSystemMessagePaneY,
						  (uint16_t)g_hudSystemMessagePaneSurfaceWidth,
						  (uint16_t)g_hudSystemMessagePaneSurfaceHeight,
						  (uint16_t)(g_flight16bppBytesPerPixel * g_hudSystemMessagePaneSurfaceWidth));
	Blit16ToFlightSurface(g_hudFlightGroupMessagePaneSurface, g_flightColorEscapeBypassChar, 0, 0,
						  (uint16_t)g_hudFlightGroupMessagePaneX, (uint16_t)g_hudFlightGroupMessagePaneY,
						  (uint16_t)g_hudFlightGroupMessagePaneSurfaceWidth,
						  (uint16_t)g_hudFlightGroupMessagePaneSurfaceHeight,
						  (uint16_t)(g_flight16bppBytesPerPixel * g_hudFlightGroupMessagePaneSurfaceWidth));
	Blit16ToFlightSurface(g_hudReadyMessagePaneSurface, g_flightColorEscapeBypassChar, 0, 0,
						  (uint16_t)g_hudReadyMessagePaneX, (uint16_t)g_hudReadyMessagePaneY,
						  (uint16_t)g_hudReadyMessagePaneSurfaceWidth,
						  (uint16_t)g_hudReadyMessagePaneSurfaceHeight,
						  (uint16_t)(g_flight16bppBytesPerPixel * g_hudReadyMessagePaneSurfaceWidth));
	FlightSurface_Unlock();
}

// FUNCTION: XWA 0x47A5B0
void Hud_DrawSystemTextPane(char* text, int16_t resourceTextId) {
	DebugPrintfChannel(0x20, "SYSTEM TEXT: %s\n", text);
	HUD_PANE_PUSH(XWA_HUD_PANE_MESSAGE_SYSTEM, g_hudSystemMessagePaneX, g_hudSystemMessagePaneY,
				  g_hudSystemMessagePaneSurfaceWidth, g_hudSystemMessagePaneSurfaceHeight);
	FlightText_SetRenderOffset(g_hudSystemMessagePaneX, g_hudSystemMessagePaneY);
	Hud_DrawHudMessageTextPane(g_hudSystemMessagePaneSurface, (uint16_t)g_hudSystemMessagePaneSurfaceWidth,
							   g_hudSystemMessagePaneSurfaceHeight, text, resourceTextId);
	FlightText_SetRenderOffset(0, 0);
	HUD_PANE_POP();
}

// FUNCTION: XWA 0x47A610
void Hud_DrawFlightGroupTextPane(char* text, int16_t resourceTextId) {
	DebugPrintfChannel(0x20, "FGROUP TEXT: %s\n", text);
	HUD_PANE_PUSH(XWA_HUD_PANE_MESSAGE_FLIGHT_GROUP, g_hudFlightGroupMessagePaneX,
				  g_hudFlightGroupMessagePaneY, g_hudFlightGroupMessagePaneSurfaceWidth,
				  g_hudFlightGroupMessagePaneSurfaceHeight);
	FlightText_SetRenderOffset(g_hudFlightGroupMessagePaneX, g_hudFlightGroupMessagePaneY);
	Hud_DrawHudMessageTextPane(g_hudFlightGroupMessagePaneSurface,
							   (uint16_t)g_hudFlightGroupMessagePaneSurfaceWidth,
							   g_hudFlightGroupMessagePaneSurfaceHeight, text, resourceTextId);
	FlightText_SetRenderOffset(0, 0);
	HUD_PANE_POP();
}

// FUNCTION: XWA 0x47A670
void Hud_DrawReadyMessageTextPane(char* text, int16_t resourceTextId) {
	DebugPrintfChannel(0x20, "READYM TEXT: %s\n", text);
	HUD_PANE_PUSH(XWA_HUD_PANE_MESSAGE_READY, g_hudReadyMessagePaneX, g_hudReadyMessagePaneY,
				  g_hudReadyMessagePaneSurfaceWidth, g_hudReadyMessagePaneSurfaceHeight);
	FlightText_SetRenderOffset(g_hudReadyMessagePaneX, g_hudReadyMessagePaneY);
	Hud_DrawHudMessageTextPane(g_hudReadyMessagePaneSurface, (uint16_t)g_hudReadyMessagePaneSurfaceWidth,
							   g_hudReadyMessagePaneSurfaceHeight, text, resourceTextId);
	FlightText_SetRenderOffset(0, 0);
	HUD_PANE_POP();
}

static __inline void Hud_ClearSoftwareTextPane(void* surface, int width, int height) {
	if (g_useHardware3D) {
		return;
	}

	if (surface == NULL) {
		XWA_HUD_OUTPUT_DEBUG_STRING("Textbuffer not initialized in hudClearTextBox()!\n");
		return;
	}

	FlightSw_SetRenderTarget(surface, (uint16_t)width, (uint16_t)height,
							 (uint16_t)width * g_flight16bppBytesPerPixel);
	FlightText_SetClipRect(0, 0, (uint16_t)width, (uint16_t)height);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	g_flightFillClipRectFn();
	FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
}

// FUNCTION: XWA 0x47A6D0
void Hud_ClearSystemTextPane(void) {
	int useHardware3D = g_useHardware3D;
	int height = g_hudSystemMessagePaneSurfaceHeight;
	int width = g_hudSystemMessagePaneSurfaceWidth;

	if (useHardware3D) {
		return;
	}

	if (g_hudSystemMessagePaneSurface == NULL) {
		XWA_HUD_OUTPUT_DEBUG_STRING("Textbuffer not initialized in hudClearTextBox()!\n");
		return;
	}

	FlightSw_SetRenderTarget(g_hudSystemMessagePaneSurface, (uint16_t)g_hudSystemMessagePaneSurfaceWidth,
							 (uint16_t)g_hudSystemMessagePaneSurfaceHeight,
							 (uint16_t)g_hudSystemMessagePaneSurfaceWidth * g_flight16bppBytesPerPixel);
	FlightText_SetClipRect(0, 0, (uint16_t)width, (uint16_t)height);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	g_flightFillClipRectFn();
	FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
}

// FUNCTION: XWA 0x47A770
void Hud_ClearFlightGroupTextPane(void) {
	int useHardware3D = g_useHardware3D;
	int height = g_hudFlightGroupMessagePaneSurfaceHeight;
	int width = g_hudFlightGroupMessagePaneSurfaceWidth;

	if (useHardware3D) {
		return;
	}

	if (g_hudFlightGroupMessagePaneSurface == NULL) {
		XWA_HUD_OUTPUT_DEBUG_STRING("Textbuffer not initialized in hudClearTextBox()!\n");
		return;
	}

	FlightSw_SetRenderTarget(g_hudFlightGroupMessagePaneSurface,
							 (uint16_t)g_hudFlightGroupMessagePaneSurfaceWidth,
							 (uint16_t)g_hudFlightGroupMessagePaneSurfaceHeight,
							 (uint16_t)g_hudFlightGroupMessagePaneSurfaceWidth * g_flight16bppBytesPerPixel);
	FlightText_SetClipRect(0, 0, (uint16_t)width, (uint16_t)height);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	g_flightFillClipRectFn();
	FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
}

// FUNCTION: XWA 0x47A810
void Hud_ClearReadyMessageTextPane(void) {
	int useHardware3D = g_useHardware3D;
	int height = g_hudReadyMessagePaneSurfaceHeight;
	int width = g_hudReadyMessagePaneSurfaceWidth;

	if (useHardware3D) {
		return;
	}

	if (g_hudReadyMessagePaneSurface == NULL) {
		XWA_HUD_OUTPUT_DEBUG_STRING("Textbuffer not initialized in hudClearTextBox()!\n");
		return;
	}

	FlightSw_SetRenderTarget(g_hudReadyMessagePaneSurface, (uint16_t)g_hudReadyMessagePaneSurfaceWidth,
							 (uint16_t)g_hudReadyMessagePaneSurfaceHeight,
							 (uint16_t)g_hudReadyMessagePaneSurfaceWidth * g_flight16bppBytesPerPixel);
	FlightText_SetClipRect(0, 0, (uint16_t)width, (uint16_t)height);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	g_flightFillClipRectFn();
	FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
}

// FUNCTION: XWA 0x47A8B0
void Hud_DrawHudMessageTextPane(void* textSurface, uint16_t width, uint16_t height, char* text,
								int16_t resourceTextId) {
	uint16_t textWidth;
	char* cursor;
	uint8_t prefix;
	uint16_t visibleChars;
	char lastChar;

	if (!g_useHardware3D) {
		FlightSw_SetRenderTarget(textSurface, width, (uint16_t)height, width * g_flight16bppBytesPerPixel);
	}

	FlightText_SetClipRect(0, 0, width, (uint16_t)height);
	FlightText_SetFontTier(1);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetShadowEnabled(1u);
	FlightText_SetShadowColor(0x40u);
	if (!g_useHardware3D) {
		g_flightFillClipRectFn();
	}

	textWidth = Hud_MeasureFlightMessagePaneText(resourceTextId);
	FlightText_SetCursor((int16_t)((width >> 1) - (textWidth >> 1)), 0);

	cursor = text;
	prefix = (uint8_t)*cursor;
	if (prefix < 9u) {
		FlightText_SetColor(g_messageTextPrefixColorCodes[prefix]);
		++cursor;
		if (prefix == 2u || prefix == 1u) {
			FlightText_SetColor(g_messageSenderIffColorCodes[g_readyMessagePaneQueue[0].senderIff]);
		}
	} else {
		FlightText_SetColor(0x42u);
	}

	visibleChars = 0;
	lastChar = 0;
	while (*cursor != '\0') {
		uint8_t ch;

		if (visibleChars >= 70u) {
			break;
		}

		ch = (uint8_t)*cursor;
		if (ch == '[') {
			if (g_flightTextColorIndex == 0xd4u) {
				g_flightTextColorIndex = 0xd3u;
			} else {
				++g_flightTextColorIndex;
			}
			++cursor;
		} else if (ch == ']') {
			if (g_flightTextColorIndex == 0xd3u) {
				g_flightTextColorIndex = 0xd4u;
			} else {
				--g_flightTextColorIndex;
			}
			++cursor;
		} else if (ch == 0xfeu) {
			cursor += 2;
		} else {
			g_flightDrawCharFn(ch);
			lastChar = *cursor;
			++cursor;
			++visibleChars;
		}
	}

	if (prefix == 3u || prefix == 4u || prefix == 7u) {
		Hud_EndHudMessageLine(g_systemMessagePane.paneType, lastChar);
	} else if (prefix == 8u) {
		Hud_EndHudMessageLine(g_flightGroupMessagePane.paneType, lastChar);
	} else {
		Hud_EndHudMessageLine(g_readyMessagePaneQueue[0].paneType, lastChar);
	}

	if (!g_useHardware3D) {
		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
	}
}

// FUNCTION: XWA 0x47AAA0
void Hud_DrawCmdTargetComponentLine(void) {
	uint32_t currentTargetObjectIdx;
	uint16_t meshType;
	uint8_t showPercent;
	uint8_t componentPercent;
	uint16_t targetObjIdx;
	MobileObject* mobj;
	CraftData* targetCraft;

	currentTargetObjectIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
	meshType = MESH_Default;
	showPercent = 0;
	componentPercent = 0;

	if ((uint16_t)currentTargetObjectIdx < g_activeRegionObjectSlotStart ||
		currentTargetObjectIdx >= g_activeRegionCraftObjectSlotEnd || currentTargetObjectIdx == 0xffffu) {
		return;
	}

	targetObjIdx = currentTargetObjectIdx;
	if (g_objectTable[targetObjIdx].genusId == GENUS_Fighter) {
		return;
	}

	mobj = g_objectTable[targetObjIdx].mobj;
	if (mobj == NULL) {
		return;
	}

	targetCraft = mobj->pCraft;
	if (targetCraft != NULL && mobj->state == 0 &&
		g_objectTable[currentTargetObjectIdx].objectType != OBJ_None) {
		meshType = (uint16_t)ModelMesh_GetObjectTypeMeshType(
			g_objectTable[currentTargetObjectIdx].objectType,
			(uint16_t)g_players[g_localPlayer].selectedTargetComponent);

		if ((g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].objectType ==
				 OBJ_CorellianTransport2 ||
			 g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].objectType ==
				 OBJ_MilleniumFalcon2) &&
			meshType == MESH_Bridge) {
			meshType = MESH_Hatch;
		}

		if (meshType != MESH_MainHull && meshType != MESH_Fuselage && meshType != MESH_MiscHull &&
			meshType != MESH_Wing &&
			ModelMesh_IsObjectTypeMeshDamageable(
				g_objectTable[targetObjIdx].objectType,
				(uint16_t)g_players[g_localPlayer].selectedTargetComponent)) {
			showPercent = 1;
			if (meshType == MESH_Engine) {
				uint16_t objectType;
				ModelIndex modelIndex;

				objectType = g_objectTable[targetObjIdx].objectType;
				if (objectType != OBJ_None) {
					modelIndex = (ModelIndex)GetModelIndexFromType(objectType);
					if (modelIndex != (ModelIndex)0xffff) {
						uint8_t engineEmitterIdx;
						uint32_t glowIndex;

						glowIndex = 0;
						engineEmitterIdx = 0xffu;
						for (; glowIndex < g_modelDefs[modelIndex].engineGlowCount; ++glowIndex) {
							if (g_modelDefs[modelIndex].engineGlowMeshIdx[glowIndex] ==
								(uint16_t)g_players[g_localPlayer].selectedTargetComponent) {
								engineEmitterIdx = (uint8_t)glowIndex;
								break;
							}
						}

						if (engineEmitterIdx == 0xffu) {
							showPercent = 0;
						} else {
							componentPercent = targetCraft->engineEmitterHealth[engineEmitterIdx];
							if (componentPercent != 0xffu) {
								componentPercent =
									(uint8_t)(int)((float)componentPercent /
													   ((float)g_modelDefs[modelIndex].componentMaxHp *
														g_hudComponentPercentScale) -
												   g_hudComponentPercentRoundingBias);
							} else {
								showPercent = 0;
							}
						}
					} else {
						showPercent = 0;
					}
				} else {
					showPercent = 0;
				}
			} else {
				componentPercent =
					(uint8_t)(int)((float)targetCraft->componentHp[(uint16_t)g_players[g_localPlayer]
																	   .selectedTargetComponent] /
								   ((float)g_meshTypeComponentMaxHp[meshType] * g_hudComponentPercentScale));
			}
		}

		if (meshType > 0x21u) {
			meshType = 0x20u;
		}
	}

	if (g_useHardware3D) {
		FlightText_SetRenderOffset((int16_t)g_hudCmdPanelOriginX, (int16_t)g_hudCmdPanelOriginY);
	} else {
		FlightSw_SetRenderTarget(g_hudCmdTexPixels, (uint16_t)g_hudCmdPanelWidth,
								 (uint16_t)g_hudCmdPanelHeight,
								 (uint16_t)g_hudCmdPanelWidth * g_flight16bppBytesPerPixel);
	}

	FlightText_SetClipRect(0, 0, (uint16_t)g_hudCmdPanelWidth, (uint16_t)g_hudCmdPanelHeight);
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetColor(0x52u);

	if (showPercent) {
		FlightText_SetCursor((uint16_t)g_hudCmdPanelWidth - (uint16_t)(int)(g_flightHudScaleFactor * 10.0f) -
								 FlightText_MeasureStringWidth("100% ") -
								 FlightText_MeasureStringWidth(g_strComponentStrings[meshType]),
							 (uint16_t)g_hudCmdComponentLineY);
		FlightText_SetScratch(g_strComponentStrings[meshType]);
		FlightText_AppendScratchString(" ");
		FlightText_AppendScratchDecimalNumber(componentPercent, 3u, 1u);
		FlightText_AppendScratchString("% ");
		FlightText_DrawString(g_flightTextScratchBuffer);
	} else {
		FlightText_SetCursor((uint16_t)g_hudCmdPanelWidth - (uint16_t)(int)(g_flightHudScaleFactor * 10.0f) -
								 FlightText_MeasureStringWidth(g_strComponentStrings[meshType]),
							 (uint16_t)g_hudCmdComponentLineY);
		FlightText_DrawString(g_strComponentStrings[meshType]);
	}

	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
	}
}

// FUNCTION: XWA 0x47AF30
void Hud_UpdateCMDText(void) {
	void(XWA_HUD_STDCALL * debugOutput)(const char*);
	uint8_t drawCmdText;

	if (g_players[g_localPlayer].hyperspacePhase != PLAYER_HYPERSPACE_PHASE_NONE) {
		return;
	}

	debugOutput = XWA_HUD_OUTPUT_DEBUG_STRING;
	if (!g_players[g_localPlayer].mapCameraState) {
		CraftData* playerCraft;

		playerCraft = Hud_GetCraftPointerInlinedWithDebug(debugOutput);
		if (playerCraft == NULL) {
			debugOutput("NULL craft data pointer in UpdateCMDText()!\n");
			return;
		}
		drawCmdText = (uint8_t)(playerCraft->activeHudFeatureMask & 1u);
	} else {
		drawCmdText = 1;
	}

	if (!drawCmdText) {
		return;
	}
	HUD_PANE_PUSH(XWA_HUD_PANE_CMD, g_hudCmdPanelOriginX, g_hudCmdPanelOriginY, g_hudCmdPanelWidth,
				  g_hudCmdPanelHeight);

	if (!g_useHardware3D) {
		void* cmdPixels;
		uint16_t cmdWidth;
		uint16_t cmdHeight;

		cmdPixels = g_hudCmdTexPixels;
		cmdHeight = (uint16_t)g_hudCmdPanelHeight;
		cmdWidth = (uint16_t)g_hudCmdPanelWidth;
		if (cmdPixels == NULL) {
			debugOutput("Textbuffer not initialized in hudClearTextBox()!\n");
		} else {
			FlightSw_SetRenderTarget(cmdPixels, cmdWidth, cmdHeight, cmdWidth * g_flight16bppBytesPerPixel);
			FlightText_SetClipRect(0, 0, cmdWidth, cmdHeight);
			FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
			g_flightFillClipRectFn();
			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		}
	}

	{
		unsigned int targetObjIdx;

		targetObjIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
		if (targetObjIdx != 0xffffu) {
			uint8_t showTargetDamage;
			MobileObject* targetMobj;

			showTargetDamage = 1;
			targetMobj = g_objectTable[targetObjIdx].mobj;
			if (targetMobj != NULL) {
				CraftData* targetCraft;

				targetCraft = targetMobj->pCraft;
				if (targetCraft != NULL &&
					(int8_t)targetCraft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] < 0) {
					showTargetDamage = 0;
				}
			}

			if (g_useHardware3D) {
				FlightText_SetRenderOffset((uint16_t)g_hudCmdPanelOriginX, (uint16_t)g_hudCmdPanelOriginY);
			} else {
				FlightSw_SetRenderTarget(g_hudCmdTexPixels, (uint16_t)g_hudCmdPanelWidth,
										 (uint16_t)g_hudCmdPanelHeight,
										 (uint16_t)g_hudCmdPanelWidth * g_flight16bppBytesPerPixel);
			}
			FlightText_SetClipRect(0, 0, (uint16_t)g_hudCmdPanelWidth, (uint16_t)g_hudCmdPanelHeight);
			FlightText_SetFontTier(1);
			FlightText_SetColor(0x43u);
			FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
			{
				uint16_t nameX;

				nameX = (uint16_t)(((uint16_t)g_hudCmdPanelWidth -
									FlightText_MeasureStringWidth(g_hudTargetNameText)) /
								   2);
				FlightText_SetCursor(nameX, (uint16_t)g_hudCmdTargetNameTextY);
			}
			FlightText_DrawString(g_hudTargetNameText);
			if (g_useHardware3D) {
				FlightText_SetRenderOffset(0, 0);
			} else {
				FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			}

			if (g_useHardware3D) {
				FlightText_SetRenderOffset((uint16_t)g_hudCmdPanelOriginX, (uint16_t)g_hudCmdPanelOriginY);
			} else {
				FlightSw_SetRenderTarget(g_hudCmdTexPixels, (uint16_t)g_hudCmdPanelWidth,
										 (uint16_t)g_hudCmdPanelHeight,
										 (uint16_t)g_hudCmdPanelWidth * g_flight16bppBytesPerPixel);
			}
			FlightText_SetClipRect(0, 0, (uint16_t)g_hudCmdPanelWidth, (uint16_t)g_hudCmdPanelHeight);
			FlightText_SetCursor((uint16_t)g_hudCmdDistanceLabelX, (uint16_t)g_hudCmdDistanceLabelY);
			FlightText_SetFontTier(0);
			FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
			FlightText_SetColor(0x46u);
			FlightText_DrawString(g_strPanelStrings[PANEL_STRING_DIST]);
			FlightText_SetCursor((uint16_t)g_hudCmdDistanceValueX, (uint16_t)g_hudCmdDistanceValueY);
			FlightText_DrawDecimalNumber(g_hudTargetDistanceWhole, 2u, 1u);
			{
				int displayDistanceWhole;

				if ((uint16_t)g_hudTargetDistanceWhole < 10u) {
					displayDistanceWhole = (uint16_t)g_hudTargetDistanceWhole + 10;
				} else {
					displayDistanceWhole = (uint16_t)g_hudTargetDistanceWhole;
				}
				FlightText_FormatScratchInt(displayDistanceWhole);
				FlightText_SetCursor((uint16_t)g_hudCmdDistanceValueX +
										 FlightText_MeasureStringWidth(g_flightTextScratchBuffer),
									 (uint16_t)g_hudCmdDistanceValueY);
				g_flightDrawCharFn('.');
				FlightText_SetCursor(FlightText_MeasureStringWidth(g_flightTextScratchBuffer) +
										 FlightText_MeasureStringWidth(".") +
										 (uint16_t)g_hudCmdDistanceValueX,
									 (uint16_t)g_hudCmdDistanceValueY);
			}
			FlightText_DrawDecimalNumber(g_hudTargetDistanceFrac, 2u, 2u);
			if (g_useHardware3D) {
				FlightText_SetRenderOffset(0, 0);
			} else {
				FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			}

			if (showTargetDamage) {
				if (g_useHardware3D) {
					FlightText_SetRenderOffset((uint16_t)g_hudCmdPanelOriginX,
											   (uint16_t)g_hudCmdPanelOriginY);
				} else {
					FlightSw_SetRenderTarget(g_hudCmdTexPixels, (uint16_t)g_hudCmdPanelWidth,
											 (uint16_t)g_hudCmdPanelHeight,
											 (uint16_t)g_hudCmdPanelWidth * g_flight16bppBytesPerPixel);
				}
				FlightText_SetClipRect(0, 0, (uint16_t)g_hudCmdPanelWidth, (uint16_t)g_hudCmdPanelHeight);
				FlightText_SetFontTier(0);
				FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
				FlightText_SetColor(0x46u);
				FlightText_SetCursor((uint16_t)g_hudCmdShieldLabelX, (uint16_t)g_hudCmdShieldLabelY);
				FlightText_DrawString(g_strPanelStrings[PANEL_STRING_SHD]);
				FlightText_SetCursor((uint16_t)g_hudCmdShieldPercentX, (uint16_t)g_hudCmdShieldPercentY);
				FlightText_DrawDecimalNumber(g_hudTargetShieldDisplayPct, 3u, 1u);
				if (g_useHardware3D) {
					FlightText_SetRenderOffset(0, 0);
				} else {
					FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
				}

				if (g_useHardware3D) {
					FlightText_SetRenderOffset((uint16_t)g_hudCmdPanelOriginX,
											   (uint16_t)g_hudCmdPanelOriginY);
				} else {
					FlightSw_SetRenderTarget(g_hudCmdTexPixels, (uint16_t)g_hudCmdPanelWidth,
											 (uint16_t)g_hudCmdPanelHeight,
											 (uint16_t)g_hudCmdPanelWidth * g_flight16bppBytesPerPixel);
				}
				FlightText_SetClipRect(0, 0, (uint16_t)g_hudCmdPanelWidth, (uint16_t)g_hudCmdPanelHeight);
				FlightText_SetFontTier(0);
				FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
				FlightText_SetColor(0x46u);
				FlightText_SetCursor((uint16_t)g_hudCmdHullLabelX, (uint16_t)g_hudCmdHullLabelY);
				FlightText_DrawString(g_strPanelStrings[PANEL_STRING_HULL]);
				FlightText_SetCursor((uint16_t)g_hudCmdHullPercentX, (uint16_t)g_hudCmdHullPercentY);
				FlightText_DrawDecimalNumber(g_hudTargetHullDisplayPct, 3u, 1u);
				if (g_useHardware3D) {
					FlightText_SetRenderOffset(0, 0);
				} else {
					FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
				}

				Hud_DrawCmdSystemPercent();
				Hud_DrawCmdTargetOrderLine();
				Hud_DrawCmdTargetStatusText();
				Hud_DrawCmdTargetComponentLine();
			}

			if (!g_useHardware3D) {
				unsigned int blitWidth;

				blitWidth = g_hudCmdPanelWidth;
				Blit16ToFlightSurface(g_hudCmdTexPixels, g_flightColorEscapeBypassChar, 0, 0,
									  (uint16_t)g_hudCmdPanelOriginX, (uint16_t)g_hudCmdPanelOriginY,
									  (uint16_t)blitWidth, (uint16_t)g_hudCmdPanelHeight,
									  g_flight16bppBytesPerPixel * blitWidth);
			}
		}
	}
	HUD_PANE_POP();
}

// FUNCTION: XWA 0x47B640
void Hud_DrawCmdTargetOrderLine(void) {
	CraftData* craft;
	AiController* ai;
	int currentTargetObjectIdx;
	unsigned int fromRef;
	int displayPlanId;
	uint16_t timerMinutes;
	uint16_t timerSeconds;
	uint16_t rangeWhole;
	uint16_t rangeFrac;
	char useDistanceSuffix;
	char useTimerSuffix;
	char drawTimerSuffix;
	const char* orderText;
	unsigned int targetObjIdx;
	MemoryHandle orderStringHandle;
	unsigned int rawDistance;

	currentTargetObjectIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
	useTimerSuffix = 0;
	useDistanceSuffix = 1;
	rangeWhole = 0;
	rangeFrac = 0;
	timerMinutes = 0;
	timerSeconds = 0;
	fromRef = currentTargetObjectIdx;

	if (currentTargetObjectIdx >= g_activeRegionCraftObjectSlotEnd || currentTargetObjectIdx == 0xffffu) {
		return;
	}

	if (g_objectTable[currentTargetObjectIdx].mobj == NULL) {
		return;
	}

	craft = g_objectTable[currentTargetObjectIdx].mobj->pCraft;
	if (craft == NULL) {
		return;
	}

	ai = pai_GetEffectiveAIController(craft);
	if ((g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
		 g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) &&
		g_flightPlayerCount > 1) {
		if (g_objectTable[(uint16_t)g_players[g_localPlayer].currentTargetObjectIdx].mobj != NULL) {
			if (!g_flightLocatePlayersEnabled) {
				int playerIff;

				playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
				if ((int8_t)craft->iffVisibility[playerIff] < 1 &&
					Object_IsHostileToTeam(g_players[g_localPlayer].currentTargetObjectIdx, playerIff) == 1) {
					return;
				}
			}
		}
	}

	displayPlanId = ai->pendingPlanId;
	if (craft->workingSubsystems == 0) {
		displayPlanId = pai_findplanbyname("disabledpln");
	} else if (g_objectTable[currentTargetObjectIdx].mobj->speed == 0) {
		if (strcmp(g_planTable[ai->pendingPlanId].name, "flyhomepln") == 0 ||
			strcmp(g_planTable[ai->pendingPlanId].name, "followhomepln") == 0 ||
			strcmp(g_planTable[ai->pendingPlanId].name, "flyhomeevadepln") == 0 ||
			strcmp(g_planTable[ai->pendingPlanId].name, "followhomeevadepln") == 0 ||
			strcmp(g_planTable[ai->pendingPlanId].name, "enterhangarpln") == 0 ||
			strcmp(g_planTable[ai->pendingPlanId].name, "exithangarpln") == 0 ||
			strcmp(g_planTable[ai->pendingPlanId].name, "intohyperspacepln") == 0 ||
			strcmp(g_planTable[ai->pendingPlanId].name, "outofhyperspacepln") == 0 ||
			strcmp(g_planTable[ai->pendingPlanId].name, "starshipintohyperpln") == 0 ||
			strcmp(g_planTable[ai->pendingPlanId].name, "starshipfollowhomepln") == 0) {
			displayPlanId = pai_findplanbyname("waitpln");
		}
	}

	if (g_useHardware3D) {
		FlightText_SetRenderOffset((int16_t)g_hudCmdPanelOriginX, (int16_t)g_hudCmdPanelOriginY);
	} else {
		FlightSw_SetRenderTarget(g_hudCmdTexPixels, (uint16_t)g_hudCmdPanelWidth,
								 (uint16_t)g_hudCmdPanelHeight,
								 (uint16_t)g_hudCmdPanelWidth * g_flight16bppBytesPerPixel);
	}
	FlightText_SetClipRect(0, 0, (uint16_t)g_hudCmdPanelWidth, (uint16_t)g_hudCmdPanelHeight);
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetColor(0x4Eu);

	if (g_objectTable[currentTargetObjectIdx].playerOwnerIdx != -1) {
		pai_ObjectRefDirectionToObjectRef(fromRef, ai->targetObjIdx);
	} else {
		trig2_ctop(ai->aimPointX - g_objectTable[currentTargetObjectIdx].world_x,
				   ai->aimPointY - g_objectTable[currentTargetObjectIdx].world_y,
				   ai->aimPointZ - g_objectTable[currentTargetObjectIdx].world_z);
	}
	rawDistance = (unsigned int)trig2_polardistance;

	if (g_objectTable[currentTargetObjectIdx].mobj->speed == 0) {
		useTimerSuffix = 1;
		if (strcmp(g_planTable[ai->pendingPlanId].name, "board2pln") == 0 ||
			strcmp(g_planTable[ai->pendingPlanId].name, "waitpln") == 0) {
			int seconds;

			seconds = ai->maneuverTimer / 236;
			timerMinutes = (uint16_t)((uint16_t)seconds / 60);
			timerSeconds = (uint16_t)(seconds - timerMinutes * 60);
		} else if (strcmp(g_planTable[ai->pendingPlanId].name, "stationaryldrpln") == 0 ||
				   strcmp(g_planTable[ai->pendingPlanId].name, "stationaryflwpln") == 0) {
			useTimerSuffix = 0;
			useDistanceSuffix = 0;
		} else if ((uint16_t)trig2_polardistance == 0) {
			timerMinutes = 0;
			timerSeconds = 0;
		}
	} else {
		uint16_t displayDistance;

		trig2_polardistance = (int)(rawDistance * 161u);
		displayDistance = (uint16_t)(((int)(161 * rawDistance)) >> 16);
		if (displayDistance >= 10000u) {
			displayDistance = 9999u;
		}
		rangeWhole = (uint16_t)(displayDistance / 100);
		rangeFrac = (uint16_t)(displayDistance - rangeWhole * 100);
	}

	orderStringHandle = g_missionOrderStringHandles[g_objectTable[currentTargetObjectIdx].flightGroupIdx]
												   [g_objectTable[currentTargetObjectIdx].regionIdx]
												   [((uint8_t)ai->currentOrderSlot)];
	if (orderStringHandle != 0) {
		orderText = (const char*)Memory_LockHandle(orderStringHandle);
		Memory_UnlockHandle(orderStringHandle);
	} else {
		orderText = g_strInFlightMessages[g_planReportMessageIdByPlanId[displayPlanId]];
	}

	if (g_objectTable[currentTargetObjectIdx].playerOwnerIdx != -1) {
		targetObjIdx =
			(uint16_t)g_players[g_objectTable[currentTargetObjectIdx].playerOwnerIdx].currentTargetObjectIdx;
	} else {
		targetObjIdx = ai->targetObjIdx;
	}
	if (craft->workingSubsystems == 0) {
		targetObjIdx = 0xffffu;
	}
	if (strcmp(g_planTable[displayPlanId].name, "waitpln") == 0) {
		targetObjIdx = 0xffffu;
	}

	if (craft->carrierObjIdx != 0xffffu && g_objectTable[craft->carrierObjIdx].mobj->pCraft != NULL &&
		g_objectTable[craft->carrierObjIdx].mobj->pCraft->carriedObjectIndex == fromRef &&
		g_objectTable[craft->carrierObjIdx].objectType != OBJ_None) {
		orderText = g_strPanelStrings[PANEL_STRING_CARRIED_BY];
		targetObjIdx = craft->carrierObjIdx;
		useDistanceSuffix = 0;
		drawTimerSuffix = 0;
	} else {
		drawTimerSuffix = useTimerSuffix;
	}

	if (targetObjIdx == 0xffffu || targetObjIdx == 255u || targetObjIdx >= 0x8000u) {
		if (!drawTimerSuffix && !useDistanceSuffix) {
			FlightText_SetCursor(((uint16_t)g_hudCmdPanelWidth >> 1) -
									 (FlightText_MeasureStringWidth(orderText) >> 1),
								 (uint16_t)g_hudCmdOrderLineY);
		} else {
			FlightText_SetCursor(((uint16_t)g_hudCmdPanelWidth >> 1) -
									 (FlightText_MeasureStringWidth(orderText) >> 1) -
									 (FlightText_MeasureStringWidth(" - 0:00") >> 1),
								 (uint16_t)g_hudCmdOrderLineY);
		}
	} else {
		Hud_AppendObjectDisplayName((uint16_t)targetObjIdx, 3);
		if (drawTimerSuffix || useDistanceSuffix) {
			FlightText_SetCursor(((uint16_t)g_hudCmdPanelWidth >> 1) -
									 (FlightText_MeasureStringWidth(orderText) >> 1) -
									 (FlightText_MeasureStringWidth(" ") >> 1) -
									 (FlightText_MeasureStringWidth(g_flightTextScratchBuffer) >> 1) -
									 (FlightText_MeasureStringWidth(" - 0:00") >> 1),
								 (uint16_t)g_hudCmdOrderLineY);
		} else {
			FlightText_SetCursor(((uint16_t)g_hudCmdPanelWidth >> 1) -
									 (FlightText_MeasureStringWidth(orderText) >> 1) -
									 (FlightText_MeasureStringWidth(" ") >> 1) -
									 (FlightText_MeasureStringWidth(g_flightTextScratchBuffer) >> 1),
								 (uint16_t)g_hudCmdOrderLineY);
		}
	}

	FlightText_DrawString(orderText);
	if (targetObjIdx != 0xffffu && targetObjIdx != 255u && targetObjIdx < 0x8000u) {
		FlightText_DrawString(" ");
		FlightText_DrawString(g_flightTextScratchBuffer);
	}

	FlightText_SetColor(0x4Eu);
	if (drawTimerSuffix) {
		FlightText_DrawString(" -");
		FlightText_DrawDecimalNumber(timerMinutes, 2u, 1u);
		g_flightDrawCharFn(':');
		FlightText_DrawDecimalNumber(timerSeconds, 2u, 2u);
	} else if (useDistanceSuffix) {
		FlightText_DrawString(" -");
		FlightText_DrawDecimalNumber(rangeWhole, 2u, 1u);
		g_flightDrawCharFn('.');
		FlightText_DrawDecimalNumber(rangeFrac, 2u, 2u);
	}

	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
	}
}

// FUNCTION: XWA 0x47C000
void Hud_DrawCmdSystemPercent(void) {
	unsigned int x;
	unsigned int y;

	if (g_useHardware3D) {
		FlightText_SetRenderOffset((int16_t)g_hudCmdPanelOriginX, (int16_t)g_hudCmdPanelOriginY);
	} else {
		FlightSw_SetRenderTarget(g_hudCmdTexPixels, (uint16_t)g_hudCmdPanelWidth,
								 (uint16_t)g_hudCmdPanelHeight,
								 (uint16_t)g_hudCmdPanelWidth * g_flight16bppBytesPerPixel);
	}

	FlightText_SetClipRect(0, 0, (uint16_t)g_hudCmdPanelWidth, (uint16_t)g_hudCmdPanelHeight);
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetColor(0x46u);
	y = (uint16_t)g_hudCmdSystemLabelY;
	x = (uint16_t)g_hudCmdSystemLabelX;
	FlightText_SetCursor(x, y);
	FlightText_DrawString(g_strPanelStrings[PANEL_STRING_SYS]);
	y = (uint16_t)g_hudCmdSystemPercentY;
	x = (uint16_t)g_hudCmdSystemPercentX;
	FlightText_SetCursor(x, y);
	FlightText_DrawDecimalNumber(g_hudTargetSystemDisplayPct, 3u, 1u);

	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
	}
}

// FUNCTION: XWA 0x47C130
void Hud_DrawCmdTargetStatusText(void) {
	if (g_hudTargetStatusText) {
		if (g_useHardware3D) {
			FlightText_SetRenderOffset((int16_t)g_hudCmdPanelOriginX, (int16_t)g_hudCmdPanelOriginY);
		} else {
			FlightSw_SetRenderTarget(g_hudCmdTexPixels, (uint16_t)g_hudCmdPanelWidth,
									 (uint16_t)g_hudCmdPanelHeight,
									 (uint16_t)g_hudCmdPanelWidth * g_flight16bppBytesPerPixel);
		}

		FlightText_SetClipRect(0, 0, (uint16_t)g_hudCmdPanelWidth, (uint16_t)g_hudCmdPanelHeight);
		FlightText_SetFontTier(0);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		FlightText_SetColor(0x46u);
		FlightText_SetCursor((uint16_t)g_hudCmdTargetStatusX, (uint16_t)g_hudCmdTargetStatusY);
		FlightText_DrawString(g_hudTargetStatusText);

		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
		} else {
			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		}
	}
}

// FUNCTION: XWA 0x47C230
void Hud_UpdateMfdPages(void) {
	unsigned int side;

	if (g_filmPlaybackMode && g_filmOverlayActive) {
		return;
	}

	for (side = 1; side < 3; ++side) {
		void* mfdSurface;
		unsigned int page;
		int titleX;

		if ((side == 1 && !g_players[g_localPlayer].mfd.enabled[1]) ||
			(side == 2 && !g_players[g_localPlayer].mfd.enabled[2])) {
			continue;
		}
		if (g_filmPlaybackMode &&
			((side == 1 && g_players[g_localPlayer].mfd.enabled[1] == 1 && g_filmOverlayMfdVisible) ||
			 (side == 2 && g_players[g_localPlayer].mfd.enabled[2] == 1 && g_filmOverlayMfdVisible))) {
			continue;
		}

		mfdSurface = side == 1 ? g_hudMfdLeftTexPixels : g_hudMfdRightTexPixels;
		page = g_players[g_localPlayer].mfd.page[side];
		FlightText_SetClipRect(0, 0, g_hudMfdSurfaceWidth, g_hudMfdSurfaceHeight);
		FlightText_SetFontTier(0);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		FlightText_SetColor(0x43u);
		if (side == 1) {
			titleX = g_hudMfdTextInsetX;
		} else {
			titleX = g_screenWidth - g_hudMfdSurfaceWidth - g_hudMfdTextInsetX;
		}
		HUD_PANE_PUSH(side == 1 ? XWA_HUD_PANE_MFD_LEFT_TITLE : XWA_HUD_PANE_MFD_RIGHT_TITLE,
					  side == 1 ? 0 : g_screenWidth - g_hudMfdSurfaceWidth, g_hudMfdSurfaceY,
					  g_hudMfdSurfaceWidth, g_hudMfdSurfaceHeight);

		if (g_useHardware3D) {
			FlightText_SetRenderOffset((int16_t)titleX, (int16_t)g_hudMfdSurfaceY);
		} else {
			FlightSw_SetRenderTarget(g_hudMfdTitleTexPixels, (uint16_t)g_hudMfdSurfaceWidth,
									 (uint16_t)g_hudMfdSurfaceHeight,
									 (uint16_t)g_hudMfdSurfaceWidth * g_flight16bppBytesPerPixel);
			g_flightFillClipRectFn();
		}

		FlightText_SetCursor(0, 0);
		if (side == 1) {
			FlightText_DrawString(g_strMfdStrings[page]);
		} else {
			FlightText_DrawStringRightAligned(g_strMfdStrings[page]);
		}

		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
		} else {
			int surfaceWidth;

			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			surfaceWidth = g_hudMfdSurfaceWidth;
			Blit16ToFlightSurface(g_hudMfdTitleTexPixels, g_flightColorEscapeBypassChar, 0, 0,
								  (uint16_t)titleX, g_hudMfdSurfaceY, surfaceWidth, g_hudMfdSurfaceHeight,
								  surfaceWidth * g_flight16bppBytesPerPixel);
		}
		HUD_PANE_POP();

		HUD_PANE_PUSH(side == 1 ? XWA_HUD_PANE_MFD_LEFT_BODY : XWA_HUD_PANE_MFD_RIGHT_BODY,
					  side == 1 ? 0 : g_screenWidth - g_hudMfdPaneWidth, g_screenHeight - g_hudMfdPaneHeight,
					  g_hudMfdPaneWidth, g_hudMfdPaneHeight);

		switch (g_players[g_localPlayer].mfd.page[side]) {
			case 0:
				if (g_provingGroundsModeActive) {
					Mfd_DrawRaceScoreboardPage((int)side, mfdSurface);
				} else {
					Mfd_DrawMissionScoreboardPage((int)side, mfdSurface);
				}
				break;

			case 1:
				if (g_provingGroundsModeActive) {
					Hangar_PopulateProvingGroundMenu((int)side - 1);
					Hangar_DrawMenuColumn((int)side - 1, (int)side);
				} else {
					Mfd_DrawMissionGoalsPage((int)side, mfdSurface);
				}
				break;

			case 2:
				Mfd_DrawMessageLogPage((int)side, mfdSurface);
				break;

			case 3:
				Damage_DisplayMfdPage((int)side, mfdSurface);
				break;

			case 4:
				Mfd_DrawFlightGroupsPage((int)side, mfdSurface);
				break;

			case 5:
				Mfd_DrawFriendlyCraftPage((int)side, mfdSurface);
				break;

			case 6:
				Mfd_DrawCommandMenuPage((int)side, mfdSurface);
				break;

			case 7:
				Mfd_DrawConsolePage((int)side, mfdSurface);
				break;

			case 8:
				Mfd_DrawMapLegendPage((int)side, mfdSurface);
				break;

			default:
				break;
		}
		HUD_PANE_POP();
	}
}

// FUNCTION: XWA 0x47C520
void Hud_DrawHangarFilmMfdOverlay(void) {
	unsigned int side;

	if (!g_filmPlaybackMode || !g_filmOverlayMfdVisible) {
		return;
	}

	FlightText_SetClipRect(0, 0, g_hudMfdSurfaceWidth, g_hudMfdSurfaceHeight);
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetColor(0x43u);

	for (side = 1; side < 3; ++side) {
		int16_t titleX;

		if ((side == 1 && !g_filmOverlayMfdVisible) || (side == 2 && !g_filmOverlayMfdVisible)) {
			continue;
		}

		if (side == 1) {
			titleX = (int16_t)g_hudMfdTextInsetX;
		} else {
			titleX = (int16_t)(g_screenWidth - g_hudMfdSurfaceWidth - g_hudMfdTextInsetX);
		}
		HUD_PANE_PUSH(side == 1 ? XWA_HUD_PANE_MFD_LEFT_TITLE : XWA_HUD_PANE_MFD_RIGHT_TITLE,
					  side == 1 ? 0 : g_screenWidth - g_hudMfdSurfaceWidth, g_hudMfdSurfaceY,
					  g_hudMfdSurfaceWidth, g_hudMfdSurfaceHeight);

		if (g_useHardware3D) {
			FlightText_SetRenderOffset(titleX, (int16_t)g_hudMfdSurfaceY);
		} else {
			FlightSw_SetRenderTarget(g_hudMfdTitleTexPixels, g_hudMfdSurfaceWidth, g_hudMfdSurfaceHeight,
									 g_hudMfdSurfaceWidth * g_flight16bppBytesPerPixel);
			g_flightFillClipRectFn();
		}

		FlightText_SetCursor(0, 0);
		if (side == 1) {
			FlightText_DrawString(g_strMfdStrings[MFD_STRING_FILM_COMMANDS]);
		} else {
			FlightText_DrawStringRightAligned(g_strMfdStrings[MFD_STRING_FILM_OPTIONS]);
		}

		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
		} else {
			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			Blit16ToFlightSurface(g_hudMfdTitleTexPixels, g_flightColorEscapeBypassChar, 0, 0,
								  (uint16_t)titleX, g_hudMfdSurfaceY, g_hudMfdSurfaceWidth,
								  g_hudMfdSurfaceHeight, g_flight16bppBytesPerPixel * g_hudMfdSurfaceWidth);
		}
		HUD_PANE_POP();
	}

	Mfd_DrawFilmLeftStatusPage();
	Mfd_DrawFilmRightOptionsPage();
}

// FUNCTION: XWA 0x47C6C0
void Hud_DrawFilmOverlayMfdTitles(void) {
	unsigned int side;

	FlightText_SetClipRect(0, 0, g_hudMfdSurfaceWidth, g_hudMfdSurfaceHeight);
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetColor(0x43u);

	for (side = 1; side < 3; ++side) {
		int surfaceWidth;
		int titleX;

		if ((side == 1 && !g_filmOverlayMfdVisible) || (side == 2 && !g_filmOverlayMfdVisible)) {
			continue;
		}

		surfaceWidth = g_hudMfdSurfaceWidth;
		if (side == 1) {
			titleX = g_hudMfdTextInsetX;
		} else {
			titleX = g_screenWidth - surfaceWidth - g_hudMfdTextInsetX;
		}
		HUD_PANE_PUSH(side == 1 ? XWA_HUD_PANE_MFD_LEFT_TITLE : XWA_HUD_PANE_MFD_RIGHT_TITLE,
					  side == 1 ? 0 : g_screenWidth - g_hudMfdSurfaceWidth, g_hudMfdSurfaceY,
					  g_hudMfdSurfaceWidth, g_hudMfdSurfaceHeight);

		if (g_useHardware3D) {
			FlightText_SetRenderOffset((int16_t)titleX, (int16_t)g_hudMfdSurfaceY);
		} else {
			FlightSw_SetRenderTarget(g_hudMfdTitleTexPixels, (uint16_t)surfaceWidth,
									 (uint16_t)g_hudMfdSurfaceHeight,
									 (uint16_t)surfaceWidth * g_flight16bppBytesPerPixel);
			g_flightFillClipRectFn();
		}

		FlightText_SetCursor(0, 0);
		if (side == 1) {
			FlightText_DrawString(g_strMfdStrings[MFD_STRING_FILM_COMMANDS]);
		} else {
			FlightText_DrawStringRightAligned(g_strMfdStrings[MFD_STRING_FILM_OPTIONS]);
		}

		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
		} else {
			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			surfaceWidth = g_hudMfdSurfaceWidth;
			Blit16ToFlightSurface(g_hudMfdTitleTexPixels, g_flightColorEscapeBypassChar, 0, 0,
								  (uint16_t)titleX, g_hudMfdSurfaceY, surfaceWidth, g_hudMfdSurfaceHeight,
								  surfaceWidth * g_flight16bppBytesPerPixel);
		}
		HUD_PANE_POP();
	}
}

// FUNCTION: XWA 0x47C840
void Hud_CycleActiveMfdPage(uint16_t playerIdx, int direction) {
	unsigned int playerIndex;
	uint8_t activeMfdIndex;
	int step;

	playerIndex = playerIdx;
	activeMfdIndex = g_players[playerIndex].mfd.activeIndex;

	if (activeMfdIndex == 1) {
		g_mfdLeftNeedsRedraw = 1;
		if (!g_useHardware3D) {
			void* mfdPixels;
			uint16_t paneWidth;
			uint16_t paneHeight;

			mfdPixels = g_hudMfdLeftTexPixels;
			paneHeight = (uint16_t)g_hudMfdPaneHeight;
			paneWidth = (uint16_t)g_hudMfdPaneWidth;
			if (mfdPixels == NULL) {
				OutputDebugStringA("Textbuffer not initialized in hudClearTextBox()!\n");
			} else {
				FlightSw_SetRenderTarget(mfdPixels, paneWidth, paneHeight,
										 paneWidth * g_flight16bppBytesPerPixel);
				FlightText_SetClipRect(0, 0, paneWidth, paneHeight);
				FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
				g_flightFillClipRectFn();
				FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			}
		}
	}
	if (g_players[playerIndex].mfd.activeIndex == 2) {
		g_mfdRightNeedsRedraw = 1;
		if (!g_useHardware3D) {
			void* mfdPixels;
			uint16_t paneWidth;
			uint16_t paneHeight;

			mfdPixels = g_hudMfdRightTexPixels;
			paneHeight = (uint16_t)g_hudMfdPaneHeight;
			paneWidth = (uint16_t)g_hudMfdPaneWidth;
			if (mfdPixels == NULL) {
				OutputDebugStringA("Textbuffer not initialized in hudClearTextBox()!\n");
			} else {
				FlightSw_SetRenderTarget(mfdPixels, paneWidth, paneHeight,
										 paneWidth * g_flight16bppBytesPerPixel);
				FlightText_SetClipRect(0, 0, paneWidth, paneHeight);
				FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
				g_flightFillClipRectFn();
				FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			}
		}
	}

	step = 1;
	if (direction == 1) {
		while (step < 8 &&
			   !Hud_IsMfdPageAvailable(
				   playerIdx,
				   (char)(g_players[playerIndex].mfd.page[g_players[playerIndex].mfd.activeIndex] + step))) {
			int activeIndex;

			activeIndex = g_players[playerIndex].mfd.activeIndex;
			if ((int)g_players[playerIndex].mfd.page[activeIndex] + step > 8) {
				if (!g_players[playerIdx].mapCameraState) {
					g_players[playerIdx].mfd.savedPage[activeIndex] = 0;
				}
				g_players[playerIdx].mfd.page[activeIndex] = 0;
				step = -1;
			}
			++step;
			if (step >= 8) {
				return;
			}
		}

		{
			int activeIndex;
			int page;

			activeIndex = g_players[playerIndex].mfd.activeIndex;
			page = g_players[playerIndex].mfd.page[activeIndex] + step;
			if (!g_players[playerIdx].mapCameraState) {
				g_players[playerIdx].mfd.savedPage[activeIndex] = (char)page;
			}
			g_players[playerIdx].mfd.page[activeIndex] = (uint8_t)page;
		}
		return;
	}

	while (step < 8 &&
		   !Hud_IsMfdPageAvailable(
			   playerIdx,
			   (char)(g_players[playerIndex].mfd.page[g_players[playerIndex].mfd.activeIndex] - step))) {
		int activeIndex;

		activeIndex = g_players[playerIndex].mfd.activeIndex;
		if ((uint8_t)(g_players[playerIndex].mfd.page[activeIndex] - step) > 8u) {
			if (!g_players[playerIdx].mapCameraState) {
				g_players[playerIdx].mfd.savedPage[activeIndex] = 8;
			}
			g_players[playerIdx].mfd.page[activeIndex] = 8;
			step = -1;
		}
		++step;
		if (step >= 8) {
			return;
		}
	}

	{
		int activeIndex;
		int page;

		activeIndex = g_players[playerIndex].mfd.activeIndex;
		page = g_players[playerIndex].mfd.page[activeIndex] - step;
		if (!g_players[playerIdx].mapCameraState) {
			g_players[playerIdx].mfd.savedPage[activeIndex] = (char)page;
		}
		g_players[playerIdx].mfd.page[activeIndex] = (uint8_t)page;
	}
}

// FUNCTION: XWA 0x47CB00
char Hud_IsMfdPageAvailable(uint16_t playerIdx, uint8_t page) {
	uint8_t result;

	switch (page) {
		case 0:
			if (g_missionHeader.body.goalsUnimportant ||
				g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
				g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH || g_provingGroundsModeActive) {
				result = 1;
			} else {
				result = 0;
			}
			break;

		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			result = 1;
			break;

		case 6:
			if (g_provingGroundsModeActive) {
				result = 0;
			} else {
				result = 1;
			}
			break;

		case 7:
			if (g_players[playerIdx].mfd.consolePageAvailable) {
				result = 1;
			} else {
				result = 0;
			}
			break;

		case 8:
			if (g_players[playerIdx].mapCameraState) {
				result = 1;
			} else {
				result = 0;
			}
			break;

		default:
			result = 0;
			break;
	}
	return result;
}

// FUNCTION: XWA 0x47CBB0
int Hud_ForceHudRefresh(int playerIdx, int mode) {
	int result;

	g_players[playerIdx].mfd.activeIndex = (uint8_t)mode;
	if (!g_players[playerIdx].mapCameraState && !g_inHangarReady) {
		g_players[playerIdx].mfd.savedActiveIndex = mode;
	}

	result = g_useHardware3D;
	if (!g_useHardware3D && playerIdx == g_localPlayer) {
		uint8_t savedLockBackBufferForHudDraw;

		g_hudElementEnabled[7].enabled = 1;
		g_hudElementEnabled[8].enabled = 1;
		savedLockBackBufferForHudDraw = (uint8_t)g_flightLockBackBufferForHudDraw;
		g_flightLockBackBufferForHudDraw = 1;
		FlightSurface_Lock();
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
		g_flightFillClipRectFn();
		FlightSurface_Unlock();
		g_flightLockBackBufferForHudDraw = savedLockBackBufferForHudDraw;

		if (g_players[g_localPlayer].hudEnabled || g_filmPlaybackMode) {
			RenderScene_Initialize(1);
			g_hudElementEnabled[4].enabled = 1;
			g_hudElementEnabled[5].enabled = 1;
			g_hudElementEnabled[6].enabled = 1;
			g_hudElementEnabled[7].enabled = 1;
			g_hudElementEnabled[8].enabled = 1;
			g_hudElementEnabled[9].enabled = 1;
			if (g_filmPlaybackMode && !g_useHardware3D) {
				g_hudElementEnabled[10].enabled = 1;
				g_hudElementEnabled[11].enabled = 1;
			}
			Hud_RenderHud();
			return FlightDisplay_BlitRenderSurface();
		}

		g_sceneBypassCockpitMask = 1;
		RenderScene_Initialize(1);
		g_sceneBypassCockpitMask = 0;
		return FlightDisplay_BlitRenderSurface();
	}

	return result;
}

// FUNCTION: XWA 0x47CD00
void Hud_SetMfdPage(int playerIdx, int mfdIndex, uint8_t page) {
	if (!g_players[playerIdx].mapCameraState) {
		g_players[playerIdx].mfd.savedPage[mfdIndex] = (char)page;
	}
	g_players[playerIdx].mfd.page[mfdIndex] = page;
}

// FUNCTION: XWA 0x47CD30
void Hud_ToggleMfdSide(int playerIdx, int mfdIndex) {
	uint8_t enabled;

	do {
		if (mfdIndex == 1) {
			enabled = g_players[playerIdx].mfd.enabled[1] == 0;
			g_players[playerIdx].mfd.enabled[1] = (uint8_t)enabled;

			if (enabled == 1) {
				if (!g_players[playerIdx].mapCameraState) {
					g_players[playerIdx].mfd.savedSideEnabled[0] = 1;
				}
				g_players[playerIdx].mfd.activeIndex = 1;
				g_mfdLeftNeedsRedraw = 1;
				if (!g_players[playerIdx].mapCameraState && !g_inHangarReady) {
					g_players[playerIdx].mfd.savedActiveIndex = 1;
				}
				if (!g_useHardware3D) {
					if (playerIdx == g_localPlayer) {
						g_hudElementEnabled[7].enabled = 1;
						g_hudElementEnabled[8].enabled = 1;
						Hud_ClearFlightSurface();
						if (g_players[g_localPlayer].hudEnabled || g_filmPlaybackMode) {
							RenderScene_Initialize(1);
							g_hudElementEnabled[4].enabled = 1;
							g_hudElementEnabled[5].enabled = 1;
							g_hudElementEnabled[6].enabled = 1;
							g_hudElementEnabled[7].enabled = 1;
							g_hudElementEnabled[8].enabled = 1;
							g_hudElementEnabled[9].enabled = 1;
							if (g_filmPlaybackMode && !g_useHardware3D) {
								g_hudElementEnabled[10].enabled = 1;
								g_hudElementEnabled[11].enabled = 1;
							}
							Hud_RenderHud();
						} else {
							g_sceneBypassCockpitMask = 1;
							RenderScene_Initialize(1);
							g_sceneBypassCockpitMask = 0;
						}
						FlightDisplay_BlitRenderSurface();
					}
					if (!g_useHardware3D) {
						if (playerIdx == g_localPlayer) {
							if (!g_filmPlaybackMode) {
								Hud_UpdateHUDMask(4, 1);
							} else if (!g_filmOverlayActive) {
								Hud_UpdateHUDMask(4, 1);
							}
						}
						break;
					}
				}
				return;
			}

			if (!g_players[playerIdx].mapCameraState) {
				g_players[playerIdx].mfd.savedSideEnabled[0] = 0;
			}
			if (g_players[playerIdx].mfd.enabled[2] == 1) {
				g_players[playerIdx].mfd.activeIndex = 2;
				if (!g_players[playerIdx].mapCameraState && !g_inHangarReady) {
					g_players[playerIdx].mfd.savedActiveIndex = 2;
				}
				if (g_useHardware3D) {
					return;
				}
				if (playerIdx == g_localPlayer) {
					g_hudElementEnabled[7].enabled = 1;
					g_hudElementEnabled[8].enabled = 1;
					Hud_ClearFlightSurface();
					if (!g_players[g_localPlayer].hudEnabled && !g_filmPlaybackMode) {
						g_sceneBypassCockpitMask = 1;
						RenderScene_Initialize(1);
						g_sceneBypassCockpitMask = 0;
					} else {
						RenderScene_Initialize(1);
						g_hudElementEnabled[4].enabled = 1;
						g_hudElementEnabled[5].enabled = 1;
						g_hudElementEnabled[6].enabled = 1;
						g_hudElementEnabled[7].enabled = 1;
						g_hudElementEnabled[8].enabled = 1;
						g_hudElementEnabled[9].enabled = 1;
						if (g_filmPlaybackMode && !g_useHardware3D) {
							g_hudElementEnabled[10].enabled = 1;
							g_hudElementEnabled[11].enabled = 1;
						}
						Hud_RenderHud();
					}
					FlightDisplay_BlitRenderSurface();
				}
			} else {
				g_players[playerIdx].mfd.activeIndex = 0;
				if (!g_players[playerIdx].mapCameraState && !g_inHangarReady) {
					g_players[playerIdx].mfd.savedActiveIndex = 0;
				}
				if (g_useHardware3D) {
					return;
				}
				if (playerIdx == g_localPlayer) {
					g_hudElementEnabled[7].enabled = 1;
					g_hudElementEnabled[8].enabled = 1;
					Hud_ClearFlightSurface();
					if (!g_players[g_localPlayer].hudEnabled && !g_filmPlaybackMode) {
						g_sceneBypassCockpitMask = 1;
						RenderScene_Initialize(1);
						g_sceneBypassCockpitMask = 0;
					} else {
						RenderScene_Initialize(1);
						Hud_EnableHudDrawElements();
						Hud_RenderHud();
					}
					FlightDisplay_BlitRenderSurface();
				}
			}
			if (g_useHardware3D) {
				return;
			}
			if (playerIdx == g_localPlayer) {
				if (!g_filmPlaybackMode) {
					Hud_UpdateHUDMask(4, 0);
				} else if (!g_filmOverlayMfdVisible) {
					Hud_UpdateHUDMask(4, 0);
				}
			}
			break;
			return;
		}

		enabled = g_players[playerIdx].mfd.enabled[2] == 0;
		g_players[playerIdx].mfd.enabled[2] = (uint8_t)enabled;

		if (enabled == 1) {
			if (!g_players[playerIdx].mapCameraState) {
				g_players[playerIdx].mfd.savedSideEnabled[1] = 1;
			}
			g_mfdRightNeedsRedraw = 1;
			g_players[playerIdx].mfd.activeIndex = 2;
			if (!g_players[playerIdx].mapCameraState && !g_inHangarReady) {
				g_players[playerIdx].mfd.savedActiveIndex = 2;
			}
			if (!g_useHardware3D) {
				if (playerIdx == g_localPlayer) {
					g_hudElementEnabled[7].enabled = 1;
					g_hudElementEnabled[8].enabled = 1;
					Hud_ClearFlightSurface();
					if (g_players[g_localPlayer].hudEnabled || g_filmPlaybackMode) {
						RenderScene_Initialize(1);
						Hud_EnableHudDrawElements();
						Hud_RenderHud();
					} else {
						g_sceneBypassCockpitMask = 1;
						RenderScene_Initialize(1);
						g_sceneBypassCockpitMask = 0;
					}
					FlightDisplay_BlitRenderSurface();
				}
				if (!g_useHardware3D) {
					if (playerIdx == g_localPlayer) {
						if (!g_filmPlaybackMode) {
							Hud_UpdateHUDMask(5, 1);
						} else if (!g_filmOverlayActive) {
							Hud_UpdateHUDMask(5, 1);
						}
					}
					break;
				}
			}
			return;
		}

		if (!g_players[playerIdx].mapCameraState) {
			g_players[playerIdx].mfd.savedSideEnabled[1] = 0;
		}
		if (g_players[playerIdx].mfd.enabled[1] == 1) {
			g_players[playerIdx].mfd.activeIndex = 1;
			if (!g_players[playerIdx].mapCameraState && !g_inHangarReady) {
				g_players[playerIdx].mfd.savedActiveIndex = 1;
			}
			if (g_useHardware3D) {
				return;
			}
			if (playerIdx == g_localPlayer) {
				g_hudElementEnabled[7].enabled = 1;
				g_hudElementEnabled[8].enabled = 1;
				Hud_ClearFlightSurface();
				if (g_players[g_localPlayer].hudEnabled || g_filmPlaybackMode) {
					RenderScene_Initialize(1);
					Hud_EnableHudDrawElements();
					Hud_RenderHud();
					FlightDisplay_BlitRenderSurface();
				} else {
					g_sceneBypassCockpitMask = 1;
					RenderScene_Initialize(1);
					g_sceneBypassCockpitMask = 0;
					FlightDisplay_BlitRenderSurface();
				}
			}
		} else {
			g_players[playerIdx].mfd.activeIndex = 0;
			if (!g_players[playerIdx].mapCameraState && !g_inHangarReady) {
				g_players[playerIdx].mfd.savedActiveIndex = 0;
			}
			if (g_useHardware3D) {
				return;
			}
			if (playerIdx == g_localPlayer) {
				g_hudElementEnabled[7].enabled = 1;
				g_hudElementEnabled[8].enabled = 1;
				Hud_RedrawSoftwareHudFrame();
			}
		}
		if (g_useHardware3D) {
			return;
		}
		if (playerIdx == g_localPlayer) {
			if (!g_filmPlaybackMode) {
				Hud_UpdateHUDMask(5, 0);
			} else if (!g_filmOverlayMfdVisible) {
				Hud_UpdateHUDMask(5, 0);
			}
		}
		break;
	} while (0);

	if (!g_useHardware3D && playerIdx == g_localPlayer) {
		g_hudElementEnabled[7].enabled = 1;
		g_hudElementEnabled[8].enabled = 1;
		Hud_ClearFlightSurface();
		if (g_players[g_localPlayer].hudEnabled || g_filmPlaybackMode) {
			RenderScene_Initialize(1);
			Hud_EnableHudDrawElements();
			Hud_RenderHud();
			FlightDisplay_BlitRenderSurface();
		} else {
			g_sceneBypassCockpitMask = 1;
			RenderScene_Initialize(1);
			g_sceneBypassCockpitMask = 0;
			FlightDisplay_BlitRenderSurface();
		}
	}
}

// FUNCTION: XWA 0x47D320
void Hud_EnterPlayerMapView(int playerIdx) {
	uint8_t savedLockBackBufferForHudDraw;

	g_players[playerIdx].hudEnabled = 1;

	if (!g_useHardware3D && playerIdx == g_localPlayer) {
		if (g_hudElementEnabled[1].enabled) {
			Hud_UpdateHUDMask(0, 0);
		}
		if (g_hudElementEnabled[2].enabled) {
			Hud_UpdateHUDMask(1, 0);
		}
		if (g_hudElementEnabled[3].enabled) {
			Hud_UpdateHUDMask(2, 0);
		}
		if (!g_hudElementEnabled[0].enabled) {
			g_hudElementEnabled[0].enabled = 1;
			Hud_UpdateHUDMask(3, 1);
		}

		savedLockBackBufferForHudDraw = (uint8_t)g_flightLockBackBufferForHudDraw;
		g_flightLockBackBufferForHudDraw = 1;
		FlightSurface_Lock();
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
		g_flightFillClipRectFn();
		FlightSurface_Unlock();
		g_flightLockBackBufferForHudDraw = savedLockBackBufferForHudDraw;
	}

	g_players[playerIdx].mapCameraState = 0xffu;
	if (!g_players[playerIdx].mfd.enabled[1]) {
		Hud_ToggleMfdSide(playerIdx, 1);
	}
	if (!g_players[playerIdx].mfd.enabled[2]) {
		Hud_ToggleMfdSide(playerIdx, 2);
	}

	if (!g_players[playerIdx].mapCameraState) {
		g_players[playerIdx].mfd.savedPage[1] = 8;
	}
	g_players[playerIdx].mfd.page[1] = 8;
	if (!g_players[playerIdx].mapCameraState) {
		g_players[playerIdx].mfd.savedPage[2] = 2;
	}
	g_players[playerIdx].mfd.page[2] = 2;

	g_players[playerIdx].mfd.activeIndex = 1;
	if (!g_players[playerIdx].mapCameraState && !g_inHangarReady) {
		g_players[playerIdx].mfd.savedActiveIndex = 1;
	}

	if (!g_useHardware3D && playerIdx == g_localPlayer) {
		g_hudElementEnabled[7].enabled = 1;
		g_hudElementEnabled[8].enabled = 1;

		savedLockBackBufferForHudDraw = (uint8_t)g_flightLockBackBufferForHudDraw;
		g_flightLockBackBufferForHudDraw = 1;
		FlightSurface_Lock();
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
		g_flightFillClipRectFn();
		FlightSurface_Unlock();
		g_flightLockBackBufferForHudDraw = savedLockBackBufferForHudDraw;

		if (g_players[g_localPlayer].hudEnabled || g_filmPlaybackMode) {
			RenderScene_Initialize(1);
			g_hudElementEnabled[4].enabled = 1;
			g_hudElementEnabled[5].enabled = 1;
			g_hudElementEnabled[6].enabled = 1;
			g_hudElementEnabled[7].enabled = 1;
			g_hudElementEnabled[8].enabled = 1;
			g_hudElementEnabled[9].enabled = 1;
			if (g_filmPlaybackMode && !g_useHardware3D) {
				g_hudElementEnabled[10].enabled = 1;
				g_hudElementEnabled[11].enabled = 1;
			}
			Hud_RenderHud();
		} else {
			g_sceneBypassCockpitMask = 1;
			RenderScene_Initialize(1);
			g_sceneBypassCockpitMask = 0;
		}
		FlightDisplay_BlitRenderSurface();
	}

	g_mfdLeftNeedsRedraw = 1;
	g_mfdRightNeedsRedraw = 1;

	if (playerIdx == g_localPlayer && !g_useHardware3D) {
		savedLockBackBufferForHudDraw = (uint8_t)g_flightLockBackBufferForHudDraw;
		g_flightLockBackBufferForHudDraw = 1;
		FlightSurface_Lock();
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
		g_flightFillClipRectFn();
		FlightSurface_Unlock();
		g_flightLockBackBufferForHudDraw = savedLockBackBufferForHudDraw;

		if (!g_players[g_localPlayer].hudEnabled && !g_filmPlaybackMode) {
			g_sceneBypassCockpitMask = 1;
			RenderScene_Initialize(1);
			g_sceneBypassCockpitMask = 0;
			FlightDisplay_BlitRenderSurface();
			return;
		}

		RenderScene_Initialize(1);
		g_hudElementEnabled[4].enabled = 1;
		g_hudElementEnabled[5].enabled = 1;
		g_hudElementEnabled[6].enabled = 1;
		g_hudElementEnabled[7].enabled = 1;
		g_hudElementEnabled[8].enabled = 1;
		g_hudElementEnabled[9].enabled = 1;
		if (g_filmPlaybackMode && !g_useHardware3D) {
			g_hudElementEnabled[10].enabled = 1;
			g_hudElementEnabled[11].enabled = 1;
		}
		Hud_RenderHud();
		FlightDisplay_BlitRenderSurface();
	}
}

// FUNCTION: XWA 0x47D6A0
void Hud_ResetHudRuntimeState(void) {
	int playerIdx;

#ifdef XWA_MODERN
	XwaSnapshotHud_Reset();
#endif

	g_hudElementEnabled[0].enabled = 1;
	g_hudElementEnabled[1].enabled = 1;
	g_hudElementEnabled[2].enabled = 1;
	g_hudElementEnabled[3].enabled = 1;
	playerIdx = g_localPlayer;
	g_reticleLaserHardpointCount = 0;
	g_hudLaserChargeDisplayDrawn = 0;
	g_hudShieldPercentLabelsInitialized = 0;
	g_systemMessagePaneVisible = 0;
	g_flightGroupMessagePaneVisible = 0;
	g_readyMessagePaneVisible = 0;
	g_players[playerIdx].padlockActive = 0;
}

static void Hud_FlushBoxOverlayBatchHW(void) {
	if (g_d3dVertexCount == 0 && g_d3dIndexCount == 0) {
		return;
	}

	std3D_LockExecuteBuffer();
	std3D_AddVertices(g_flightVertexBuffer, g_d3dVertexCount);
	std3D_BeginInstructions();
	std3D_AddTriangles(g_triBuffer, (unsigned int)g_d3dIndexCount);
	std3D_ExecuteBuffer();
	g_d3dIndexCount = 0;
	g_d3dVertexCount = 0;
}

static void Hud_FlushBoxOverlayBatchIfNeededHW(int neededVertices, int neededTriangles) {
	if (g_d3dVertexCount + neededVertices > g_maxBatchVerts ||
		g_d3dIndexCount + neededTriangles > g_maxBatchTris) {
		Hud_FlushBoxOverlayBatchHW();
	}
}

static __inline int Hud_EmitClippedBoxOverlayQuadHW(int left, int top, int right, int bottom, float depthZ,
													uint32_t color, int16_t* result) {
	if (left < 0) {
		left = 0;
	}
	if (top < 0) {
		top = 0;
	}
	if (right > g_flightVpWidth) {
		right = g_flightVpWidth;
	}
	if (bottom > g_flightVpHeight) {
		bottom = g_flightVpHeight;
	}
	if (left >= right || top >= bottom) {
		return 0;
	}

	*result = Hud_EmitBoxOverlayQuadHW((float)left, (float)top, (float)right, (float)bottom, depthZ, color);
	return 1;
}

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x48F350
void sw3d_BlitOccludedSpan(const uint16_t* srcRaster, int startX, int endX, int scanY, float depth) {
	SceneSpan* span;
	int drawX;

	srcRaster = (const uint16_t*)((const uint8_t*)srcRaster - g_flight16bppBytesPerPixel * startX);
	drawX = startX;
	g_sw3dScanlineByteOffset =
		g_flight16bppBytesPerPixel * g_flightVpX + g_surfacePitch * (scanY + g_flightVpY);

	span = ((SceneSpan**)g_scanlineSpanHeads)[scanY];
	while (span != NULL) {
		int spanEnd;

		spanEnd = span->endX;
		if (spanEnd <= drawX) {
			span = span->next;
			continue;
		}
		if (span->startX > drawX) {
			break;
		}

		if (depth <= span->pFace->minVertW) {
			drawX = spanEnd;
			startX = drawX;
			if (spanEnd >= endX) {
				return;
			}
		} else if (depth < span->pFace->maxVertW) {
			float spanDepth;

			spanDepth = (float)scanY * span->pFace->gradients[7] + span->pFace->gradients[8];
			spanDepth = (float)startX * span->pFace->gradients[6] + spanDepth;
			if (depth <= spanDepth) {
				if (span->pFace->gradients[6] >= 0.0f) {
					drawX = spanEnd;
					startX = drawX;
					if (spanEnd >= endX) {
						return;
					}
				} else {
					int limitX;
					int deltaX;
					float limitDepth;

					limitX = spanEnd;
					if (limitX >= endX) {
						limitX = endX;
					}
					deltaX = limitX - drawX;
					limitDepth = (float)deltaX * span->pFace->gradients[6] + spanDepth;
					if (depth <= limitDepth) {
						if (spanEnd >= endX) {
							return;
						}
						drawX = spanEnd;
						startX = drawX;
					} else {
						drawX += (int)((float)deltaX - (depth - limitDepth) / -span->pFace->gradients[6]);
						startX = drawX;
						if (drawX >= endX) {
							return;
						}
					}
				}
			} else if (span->pFace->gradients[6] > 0.0f) {
				int limitX;
				int deltaX;
				float limitDepth;

				limitX = spanEnd;
				if (limitX > endX) {
					limitX = endX;
				}
				deltaX = limitX - drawX;
				limitDepth = (float)deltaX * span->pFace->gradients[6] + spanDepth;
				if (depth < limitDepth) {
					endX = drawX + (int)((float)deltaX - (depth - limitDepth) / -span->pFace->gradients[6]);
					if (drawX >= endX) {
						return;
					}
				}
			}
		}

		span = span->next;
	}

	while (span != NULL) {
		int spanStart;

		spanStart = span->startX;
		if (spanStart >= endX) {
			break;
		}

		if (depth <= span->pFace->minVertW) {
			int copyCount;

			copyCount = spanStart - drawX;
			if (copyCount > 0) {
				const uint8_t* src;
				uint8_t* dst;

				src = (const uint8_t*)srcRaster + g_flight16bppBytesPerPixel * drawX;
				dst = (uint8_t*)g_surfacePixels + g_sw3dScanlineByteOffset + startX + startX;
				do {
					dst[0] = src[0];
					dst[1] = src[1];
					src += 2;
					dst += 2;
				} while (--copyCount != 0);
			}

			drawX = span->endX;
			startX = drawX;
			if (drawX >= endX) {
				return;
			}
		} else if (depth < span->pFace->maxVertW) {
			float spanDepth;

			spanDepth = (float)scanY * span->pFace->gradients[7] + span->pFace->gradients[8];
			spanDepth = (float)spanStart * span->pFace->gradients[6] + spanDepth;
			if (depth <= spanDepth) {
				int copyCount;

				copyCount = spanStart - drawX;
				if (copyCount > 0) {
					const uint8_t* src;
					uint8_t* dst;

					src = (const uint8_t*)srcRaster + g_flight16bppBytesPerPixel * drawX;
					dst = (uint8_t*)g_surfacePixels + g_sw3dScanlineByteOffset + startX + startX;
					do {
						dst[0] = src[0];
						dst[1] = src[1];
						src += 2;
						dst += 2;
					} while (--copyCount != 0);
				}

				drawX = span->endX;
				if (span->pFace->gradients[6] >= 0.0f) {
					startX = drawX;
					if (drawX >= endX) {
						return;
					}
				} else if (drawX >= endX) {
					float limitDepth;
					int skip;

					limitDepth = (float)(endX - spanStart) * span->pFace->gradients[6] + spanDepth;
					if (depth <= limitDepth) {
						return;
					}
					skip = (int)((depth - limitDepth) / -span->pFace->gradients[6]);
					drawX = endX - skip;
					startX = drawX;
					if (drawX >= endX) {
						return;
					}
				} else {
					float limitDepth;

					limitDepth = (float)(drawX - spanStart) * span->pFace->gradients[6] + spanDepth;
					if (depth > limitDepth) {
						drawX -= (int)((depth - limitDepth) / -span->pFace->gradients[6]);
					}
					startX = drawX;
				}
			} else if (span->pFace->gradients[6] > 0.0f) {
				int spanEnd;

				spanEnd = span->endX;
				if (spanEnd >= endX) {
					float limitDepth;

					limitDepth = (float)(endX - spanStart) * span->pFace->gradients[6] + spanDepth;
					if (depth < limitDepth) {
						int copyCount;

						copyCount = spanStart +
									(int)((float)(endX - spanStart) -
										  (depth - limitDepth) / -span->pFace->gradients[6]) -
									drawX;
						if (copyCount > 0) {
							const uint8_t* src;
							uint8_t* dst;

							src = (const uint8_t*)srcRaster + g_flight16bppBytesPerPixel * drawX;
							dst = (uint8_t*)g_surfacePixels + g_sw3dScanlineByteOffset + startX + startX;
							do {
								dst[0] = src[0];
								dst[1] = src[1];
								src += 2;
								dst += 2;
							} while (--copyCount != 0);
						}
						return;
					}
				} else {
					float limitDepth;

					limitDepth = (float)(spanEnd - spanStart) * span->pFace->gradients[6] + spanDepth;
					if (depth < limitDepth) {
						int copyCount;

						copyCount =
							spanEnd - (int)((depth - limitDepth) / -span->pFace->gradients[6]) - drawX;
						if (copyCount > 0) {
							const uint8_t* src;
							uint8_t* dst;

							src = (const uint8_t*)srcRaster + g_flight16bppBytesPerPixel * drawX;
							dst = (uint8_t*)g_surfacePixels + g_sw3dScanlineByteOffset + startX + startX;
							do {
								dst[0] = src[0];
								dst[1] = src[1];
								src += 2;
								dst += 2;
							} while (--copyCount != 0);
						}
						drawX = span->endX;
						startX = drawX;
					}
				}
			}
		}

		span = span->next;
	}

	{
		int copyCount;

		copyCount = endX - drawX;
		if (copyCount > 0) {
			const uint8_t* src;
			uint8_t* dst;

			src = (const uint8_t*)srcRaster + g_flight16bppBytesPerPixel * drawX;
			dst = (uint8_t*)g_surfacePixels + g_sw3dScanlineByteOffset + startX + startX;
			do {
				dst[0] = src[0];
				dst[1] = src[1];
				src += 2;
				dst += 2;
			} while (--copyCount != 0);
		}
	}
}
#ifndef XWA_MODERN
#pragma optimize("y", on)
#endif

// FUNCTION: XWA 0x48F850
void Hud_DrawBoxInXTrans(int x, int y, int width, int height, int colorIdx, int depth) {
	uint16_t* span;
	int right;
	int bottom;
	int cornerWidth;
	int cornerHeight;
	float spanDepth;
	uint16_t spanColor;
	int i;

	bottom = y + height;
	if (bottom <= 0) {
		return;
	}

	right = x + width;
	if (right <= 0) {
		return;
	}

	if (x >= g_flightVpWidth) {
		return;
	}

	if (y >= g_flightVpHeight || height <= 0 || width <= 0) {
		return;
	}

	if (g_useHardware3D) {
		Hud_DrawBoxOverlayHW(x, y, width, height, colorIdx, depth);
		return;
	}

	cornerWidth = width >> 3;
	cornerHeight = height >> 3;
	if (cornerWidth < 3) {
		cornerWidth = 3;
	}
	if (cornerHeight < 3) {
		cornerHeight = 3;
	}
	if (cornerWidth > width) {
		cornerWidth = width;
	}
	if (cornerHeight > height) {
		cornerHeight = height;
	}

	span = g_panelBoxSpanScratch;
	if (x > 0) {
		span = &g_panelBoxSpanScratch[x];
	}
	spanColor = g_flightTextPalette[colorIdx];
	{
		uint16_t* spanCursor;

		spanCursor = span;
		i = cornerWidth;
		while (i > 0) {
			*spanCursor++ = spanColor;
			--i;
		}
	}

	if (depth < 1) {
		depth = 1;
	}
	spanDepth = (float)((double)(uint32_t)g_projScaleInt / (double)depth);

	if (!g_flightSurfaceAlreadyLocked) {
		FlightSurface_Lock();
	}

	if (y >= 0) {
		int spanStart;
		int spanEnd;

		spanEnd = x + cornerWidth;
		spanStart = x;
		if (spanEnd > 0 && x < g_flightVpWidth) {
			if (spanStart < 0) {
				spanStart = 0;
			}
			if (spanEnd > g_flightVpWidth) {
				spanEnd = g_flightVpWidth;
			}
			sw3d_BlitOccludedSpan(span, spanStart, spanEnd, y, spanDepth);
		}

		spanEnd = x + width;
		spanStart = spanEnd - cornerWidth;
		if (spanEnd > 0 && spanStart < g_flightVpWidth) {
			if (spanStart < 0) {
				spanStart = 0;
			}
			if (spanEnd > g_flightVpWidth) {
				spanEnd = g_flightVpWidth;
			}
			sw3d_BlitOccludedSpan(span, spanStart, spanEnd, y, spanDepth);
		}
	}

	if (bottom <= g_flightVpHeight) {
		int spanStart;
		int spanEnd;

		spanEnd = x + cornerWidth;
		spanStart = x;
		if (spanEnd > 0 && x < g_flightVpWidth) {
			if (spanStart < 0) {
				spanStart = 0;
			}
			if (spanEnd > g_flightVpWidth) {
				spanEnd = g_flightVpWidth;
			}
			sw3d_BlitOccludedSpan(span, spanStart, spanEnd, bottom - 1, spanDepth);
		}

		spanEnd = x + width;
		spanStart = spanEnd - cornerWidth;
		if (spanEnd > 0 && spanStart < g_flightVpWidth) {
			if (spanStart < 0) {
				spanStart = 0;
			}
			if (spanEnd > g_flightVpWidth) {
				spanEnd = g_flightVpWidth;
			}
			sw3d_BlitOccludedSpan(span, spanStart, spanEnd, bottom - 1, spanDepth);
		}
	}

	if (cornerHeight > 1) {
		int scanY;
		int remaining;

		scanY = y + 1;
		remaining = cornerHeight - 1;
		do {
			if (scanY >= 0 && scanY < g_flightVpHeight) {
				if (x >= 0) {
					sw3d_BlitOccludedSpan(span, x, x + 1, scanY, spanDepth);
				}
				if (right <= g_flightVpWidth) {
					sw3d_BlitOccludedSpan(span, right - 1, right, scanY, spanDepth);
				}
			}
			++scanY;
			--remaining;
		} while (remaining != 0);
	}

	{
		int rowOffset;
		int lastRow;
		int scanY;

		rowOffset = height - cornerHeight;
		lastRow = height - 1;
		if (rowOffset < lastRow) {
			scanY = rowOffset + y;
			do {
				if (rowOffset >= cornerHeight && scanY >= 0 && scanY < g_flightVpHeight) {
					if (x >= 0) {
						sw3d_BlitOccludedSpan(span, x, x + 1, scanY, spanDepth);
					}
					if (right <= g_flightVpWidth) {
						sw3d_BlitOccludedSpan(span, right - 1, right, scanY, spanDepth);
					}
				}
				++rowOffset;
				++scanY;
			} while (rowOffset < lastRow);
		}
	}

	if ((g_players[g_localPlayer].mfd.enabled[0] || g_players[g_localPlayer].viewState.externalCameraActive ||
		 (g_filmPlaybackMode && g_filmOverlayActive == 1)) &&
		!g_inHangarReady && colorIdx == 59) {
		int centerX;
		int centerY;

		centerX = x + width / 2;
		if (centerX > 0 && centerX < (int16_t)g_screenWidth) {
			centerY = y + height / 2;
			if (centerY > 0 && centerY < (int16_t)g_screenHeight) {
				Hud_DrawTargetBoxReadout(x, y, width, height);
			}
		}
	}

	if (!g_flightSurfaceAlreadyLocked) {
		FlightSurface_Unlock();
	}
}

// FUNCTION: XWA 0x497AF0
void Hud_ResetFlightMessagePanes(int forceExpireActiveMessages) {
	if (g_flightMessagePaneLayoutInitSentinel == -1) {
		if (g_flightResolutionMode >= FLIGHT_RES_640x480 && g_flightResolutionMode <= FLIGHT_RES_1600x1200) {
			g_unusedFlightMessagePaneLayoutScalar0 = 6;
			g_flightMessagePaneLayoutInitSentinel = 85;
			g_unusedFlightMessagePaneLayout505 = 505;
			g_unusedFlightMessagePaneLayoutScalar1 = 53;
			g_unusedFlightMessagePaneLayoutScalar2 = 85;
			g_unusedFlightMessagePaneLayoutScalar3 = 64;
			g_unusedFlightMessagePaneLayoutScalar4 = 555;
			g_unusedFlightMessagePaneLayoutScalar5 = 75;
			g_unusedFlightMessagePaneLayout150 = 150;
			g_unusedFlightMessagePaneLayoutScalar6 = 89;
			g_unusedFlightMessagePaneLayoutScalar7 = 530;
			g_unusedFlightMessagePaneLayoutScalar8 = 100;
		}
	} else if (forceExpireActiveMessages) {
		++g_flightMessagePanesForceExpire;
	}

	FlightSurface_Lock();
	FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
	FlightText_SetWordWrap(0);
	FlightText_SetClearLineBackground(0);
	FlightText_SetFontTier(1);
	Hud_DrawNetworkStatusIndicators();
	FlightText_SetColor(0x43u);
	g_messageLogFileWriteRequested = 0;
	g_readyMessageQueueCount = 0;
	g_messageLogTotalCount = 0;
	g_messageLogWriteIndex = 0xffff;
	g_unusedReadyMessagePaneInitialState = g_readyMessagePaneQueue[0].stateOrMessageId;
	g_readyMessagePaneQueue[0].stateOrMessageId = 0xffff;
	g_systemMessagePane.stateOrMessageId = 0xffff;
	g_flightGroupMessagePane.stateOrMessageId = 0xffff;
	g_targetDescriptionMessageId = MSG_TARGET_RELS;
	Hud_ClearReadyMessageTextPane();
	Hud_ClearSystemTextPane();
	Hud_ClearFlightGroupTextPane();
	FlightSurface_Unlock();
}

// FUNCTION: XWA 0x497C50
void msg_writeMessageLogFile(void) {
#ifdef XWA_MODERN
	int logIndex;
	XwaFile* stream;
	char fileName[16];

	if (g_messageLogWriteIndex == 0xffffu) {
		return;
	}

	logIndex = 0;
	for (;;) {
		snprintf(fileName, sizeof(fileName), "msglog%d.txt", logIndex);
		stream = File_Open(AERON_VFS_ROOT_USER, fileName, "r");
		if (stream == NULL) {
			break;
		}

		if (logIndex == 99) {
			File_Close(stream);
			break;
		}

		File_Close(stream);
		++logIndex;
		if (logIndex >= 100) {
			stream = NULL;
			break;
		}
	}

	stream = File_Open(AERON_VFS_ROOT_USER, fileName, "a");
	if (stream != NULL) {
		uint16_t messageIndex;

		for (messageIndex = 0; messageIndex < g_messageLogWriteIndex; ++messageIndex) {
			HudInFlightMessageRecord* record;
			const char* text;
			int prefix;
			char line[128];
			int lineLength;

			record = &g_messageLogRecords[messageIndex];
			text = record->text;
			prefix = (int)(signed char)record->text[0];
			if (prefix < 9) {
				text = &record->text[1];
				if (prefix == 1 && text[0] >= '0' && text[0] <= '3') {
					text = &record->text[2];
				}
			}

			lineLength = snprintf(line, sizeof(line), "%s\t%d:%d:%d\n", text, record->clockHour,
								  record->clockMinute, record->clockTick);
			if (lineLength > 0) {
				if ((size_t)lineLength >= sizeof(line)) {
					lineLength = (int)sizeof(line) - 1;
				}
				File_WriteCount(stream, line, (size_t)lineLength);
			}
		}

		File_Close(stream);
	}
#else
	int logIndex;
	int messageIndex;
	FILE* stream;
	char fileName[16];
	int prefix;
	char* text;
	HudInFlightMessageRecord* record;

	if (g_messageLogWriteIndex == 0xffffu) {
		return;
	}

	for (logIndex = 0; logIndex < 100; ++logIndex) {
		sprintf(fileName, g_messageLogFileNameFormat, logIndex);
		stream = fopen(fileName, "r");
		if (stream == NULL || logIndex == 99) {
			if (stream != NULL) {
				fclose(stream);
			}
			stream = fopen(fileName, "a");
			break;
		}
	}

	if (stream != NULL) {
		for (messageIndex = 0; messageIndex < g_messageLogWriteIndex; ++messageIndex) {
			record = &g_messageLogRecords[messageIndex];
			text = record->text;
			prefix = (int)(signed char)record->text[0];
			if (prefix < 9) {
				++text;
				if (prefix == 1 && text[0] >= '0' && text[0] <= '3') {
					++text;
				}
			}

			fprintf(stream, g_messageLogLineFormat, text, record->clockHour, record->clockMinute,
					record->clockTick);
		}

		fclose(stream);
	}
#endif
}

// FUNCTION: XWA 0x497D40
void msg_emitInFlightMessage(int messageId, int playerIdx) {
	HudInFlightMessageRecord record;
	const char* src;
	uint16_t argIndex;
	uint16_t textIndex;
	uint8_t paneType;
	int16_t paneTypeSigned;

	if (g_flightSimSideEffectsSuppressed) {
		return;
	}
	if (playerIdx != g_localPlayer) {
		return;
	}

	record.stateOrMessageId = (uint16_t)messageId;
	if (g_missionTimeLimitActive) {
		record.clockWord = g_missionCountdownClock.subsecondTicks;
		record.clockTick = g_missionCountdownClock.seconds;
		record.clockMinute = g_missionCountdownClock.minutes;
		record.clockHour = g_missionCountdownClock.hours;
	} else {
		record.clockWord = (uint16_t)g_missionElapsedClock.subsecondTicks;
		record.clockTick = g_missionElapsedClock.seconds;
		record.clockMinute = g_missionElapsedClock.minutes;
		record.clockHour = g_missionElapsedClock.hours;
	}
	record.ageTicks = 0;
	record.showCount = 0;
	record.senderIff = g_msgSenderIff;
	record.voiceSfxId = g_pendingHudMessageVoiceSfxId;
	src = g_strInFlightMessages[messageId];
	argIndex = 0;
	textIndex = 0;
	while (*src != '\0' && textIndex < 70u) {
		if (*src == '*') {
			uint16_t arg;
			const char* argText;

			++src;
			arg = g_msgArgTable[argIndex++];
			if (arg >= 0x8000u) {
				argText = g_msgPtrs[arg & 0x7fffu];
			} else {
				argText = g_strInFlightMessages[arg];
			}

			while (*argText != '\0' && textIndex < 70u) {
				record.text[textIndex++] = *argText++;
			}
		} else if (*src == '&') {
			uint16_t width;
			uint16_t value;
			int16_t started;

			++src;
			width = (uint8_t)*src;
			++src;
			value = g_msgArgTable[argIndex++];
			started = 0;
			while (width > 0u) {
				uint16_t divisor;
				uint16_t digit;
				int16_t outChar;

				if (textIndex >= 70u) {
					break;
				}

				divisor = g_flightTextDecimalDivisors[width];
				digit = (uint16_t)(value / divisor);
				value = (uint16_t)(value - divisor * digit);
				if (started || width <= 1u || digit != 0) {
					started = 1;
					if (digit > 9u) {
						digit = 9;
					}
					outChar = (int16_t)(digit + '0');
				} else {
					outChar = ' ';
				}

				if (outChar != ' ') {
					record.text[textIndex++] = (char)outChar;
				}
				--width;
			}
		} else {
			record.text[textIndex++] = *src++;
		}
	}
	if (textIndex < 70u) {
		record.text[textIndex] = '\0';
	} else {
		record.text[69] = '\0';
	}

	src = g_strInFlightMessages[messageId];
	while (*src == '{' || *src == '}') {
		++src;
	}
	if ((uint8_t)*src < 9u) {
		paneType = (uint8_t)*src;
	} else {
		paneType = 6;
	}
	record.paneType = paneType;
	paneTypeSigned = (int16_t)paneType;

	if (!g_replayViewMode && (paneTypeSigned == 2 || paneTypeSigned == 1)) {
		++g_messageLogTotalCount;
		if (++g_messageLogWriteIndex == 300u) {
			if (g_messageLogFileWriteRequested) {
				msg_writeMessageLogFile();
			}
			g_messageLogWriteIndex = 0;
			g_messageLogWrapped = 1;
		}

		g_messageLogRecords = (HudInFlightMessageRecord*)Memory_LockHandle(g_messageLogHandle);
		Memory_UnlockHandle(g_messageLogHandle);
		memcpy(&g_messageLogRecords[g_messageLogWriteIndex], &record, sizeof(record));
	}

	if (paneTypeSigned != 3 && paneTypeSigned != 4 && paneTypeSigned != 7) {
		if (paneTypeSigned == 8) {
			memcpy(&g_flightGroupMessagePane, &record, sizeof(g_flightGroupMessagePane));
			Hud_ShowFlightMessagePane(8, 0);
			return;
		}

		if (g_readyMessagePaneQueue[0].stateOrMessageId != 0xffffu) {
			switch (g_readyMessagePaneQueue[0].paneType) {
				case 1:
					if (paneTypeSigned == 2 || paneTypeSigned == 1) {
						int oldCount;

						oldCount = g_readyMessageQueueCount++;
						memcpy(&g_readyMessagePaneQueue[oldCount + 1], &record, sizeof(record));
						if (g_readyMessageQueueCount >= 10u) {
							--g_readyMessageQueueCount;
						}
						paneTypeSigned = 0;
						break;
					}
					if (g_readyMessagePaneQueue[0].showCount < 2u &&
						g_readyMessagePaneQueue[0].ageTicks == 0) {
						uint8_t queueCount;
						uint16_t dstIndex;

						queueCount = g_readyMessageQueueCount;
						dstIndex = (uint16_t)queueCount + 1u;
						while (dstIndex > 0u) {
							memcpy(&g_readyMessagePaneQueue[dstIndex], &g_readyMessagePaneQueue[dstIndex - 1],
								   sizeof(g_readyMessagePaneQueue[0]));
							--dstIndex;
						}
						++queueCount;
						g_readyMessageQueueCount = queueCount;
						if (queueCount >= 10u) {
							g_readyMessageQueueCount = (uint8_t)(queueCount - 1u);
						}
					}
					break;
				case 2:
				case 5:
					if (paneTypeSigned == 2 || paneTypeSigned == 1) {
						int oldCount;

						oldCount = g_readyMessageQueueCount++;
						memcpy(&g_readyMessagePaneQueue[oldCount + 1], &record, sizeof(record));
						if (g_readyMessageQueueCount >= 10u) {
							--g_readyMessageQueueCount;
						}
						paneTypeSigned = 0;
						break;
					}
					if (g_readyMessagePaneQueue[0].showCount < 2u &&
						g_readyMessagePaneQueue[0].ageTicks == 0) {
						uint8_t queueCount;
						uint16_t dstIndex;

						queueCount = g_readyMessageQueueCount;
						dstIndex = (uint16_t)queueCount + 1u;
						while (dstIndex > 0u) {
							memcpy(&g_readyMessagePaneQueue[dstIndex], &g_readyMessagePaneQueue[dstIndex - 1],
								   sizeof(g_readyMessagePaneQueue[0]));
							--dstIndex;
						}
						++queueCount;
						g_readyMessageQueueCount = queueCount;
						if (queueCount >= 10u) {
							g_readyMessageQueueCount = (uint8_t)(queueCount - 1u);
						}
					}
					break;
				case 4:
					if (paneTypeSigned == 2 || paneTypeSigned == 1) {
						int oldCount;

						oldCount = g_readyMessageQueueCount++;
						memcpy(&g_readyMessagePaneQueue[oldCount + 1], &record, sizeof(record));
						if (g_readyMessageQueueCount >= 10u) {
							--g_readyMessageQueueCount;
						}
						paneTypeSigned = 0;
						break;
					}
					break;
				case 3:
				case 6:
				case 7:
				case 8:
					break;
				default:
					paneTypeSigned = 0;
					break;
			}
		}

		if (paneTypeSigned != 0) {
			memcpy(g_readyMessagePaneQueue, &record, sizeof(g_readyMessagePaneQueue[0]));
			Hud_ShowFlightMessagePane(paneTypeSigned, 1);
		}
		g_pendingHudMessageVoiceSfxId = 0;
		return;
	}

	if ((g_flightSystemMessagesEnabled || messageId == MSG_SYSTEMSGS_DISABLED) &&
		(g_systemMessagePane.stateOrMessageId == 0xffffu || g_systemMessagePane.paneType != 4u ||
		 paneTypeSigned == 7)) {
		memcpy(&g_systemMessagePane, &record, sizeof(g_systemMessagePane));
		Hud_ShowFlightMessagePane(paneTypeSigned, 0);
	}
}

// FUNCTION: XWA 0x4982C0
void Hud_ShowFlightMessagePane(int16_t paneType, char playVoice) {
	if (g_readyMessagePaneQueue[0].stateOrMessageId == 0xffffu && paneType != 3 && paneType != 8 &&
		paneType != 4 && paneType != 7) {
		return;
	}

	if (playVoice && g_readyMessagePaneQueue[0].voiceSfxId != 0 &&
		g_readyMessagePaneQueue[0].showCount == 0 && g_gameConfig.voiceSpecialEnabled) {
		fsfx_QueueVoiceSfx(g_readyMessagePaneQueue[0].voiceSfxId, 0, 0, 0, 0xffff, 0xffff);
	}

	if (paneType == 3 || paneType == 4 || paneType == 7) {
		g_systemMessagePane.stateOrMessageId = 1;
		Hud_SetFlightMessagePaneTimer(paneType);
		Hud_DrawSystemTextPane(g_systemMessagePane.text, paneType);
		g_systemMessagePaneVisible = 1;
	} else if (paneType == 8) {
		g_flightGroupMessagePane.stateOrMessageId = 1;
		Hud_SetFlightMessagePaneTimer(8);
		Hud_DrawFlightGroupTextPane(g_flightGroupMessagePane.text, 8);
		g_flightGroupMessagePaneVisible = 1;
	} else {
		if (g_readyMessagePaneQueue[0].stateOrMessageId == 468) {
			fsfx_PlaySound(126, 0xffffu, (unsigned int)g_localPlayer);
		}
		++g_readyMessagePaneQueue[0].showCount;
		Hud_SetFlightMessagePaneTimer(paneType);
		Hud_DrawReadyMessageTextPane(g_readyMessagePaneQueue[0].text, paneType);
		g_readyMessagePaneVisible = 1;
	}
}

// FUNCTION: XWA 0x498400
void Hud_EndHudMessageLine(int unused, char lastChar) {
	(void)unused;

	if (lastChar != '?' && lastChar != '!' && lastChar != ':' && lastChar != ' ' && lastChar != '.') {
		g_flightDrawCharFn('.');
	}

	FlightText_SetClearLineBackground(1);
	g_flightDrawCharFn('\n');
	FlightText_SetClearLineBackground(0);
	FlightText_SetFontTier(2);
}

// FUNCTION: XWA 0x498450
void Hud_SetFlightMessagePaneTimer(int16_t paneType) {
	if (paneType == 3 || paneType == 7) {
		g_playerFlightTransientTimers[g_localPlayer].systemMessagePaneTimer = 472;
		return;
	}

	if (paneType == 8) {
		g_playerFlightTransientTimers[g_localPlayer].flightGroupMessagePaneTimer = 1888;
		return;
	}

	if (paneType == 4) {
		g_playerFlightTransientTimers[g_localPlayer].systemMessagePaneTimer = 1888;
		return;
	}

	if (paneType == 2) {
		if (g_readyMessageQueueCount != 0) {
			g_playerFlightTransientTimers[g_localPlayer].readyMessagePaneTimer = 354;
		} else {
			g_playerFlightTransientTimers[g_localPlayer].readyMessagePaneTimer = 1180;
		}
		return;
	}

	if (paneType == 1) {
		if (g_readyMessageQueueCount != 0) {
			g_playerFlightTransientTimers[g_localPlayer].readyMessagePaneTimer = 590;
		} else {
			g_playerFlightTransientTimers[g_localPlayer].readyMessagePaneTimer = 944;
		}
		return;
	}

	g_playerFlightTransientTimers[g_localPlayer].readyMessagePaneTimer = 1652;
}

// FUNCTION: XWA 0x498530
void Hud_UpdateFlightMessagePanes(void) {
	int localPlayerIdx;

	localPlayerIdx = g_localPlayer;
	if ((g_playerFlightTransientTimers[localPlayerIdx].readyMessagePaneTimer == 0 &&
		 g_readyMessagePaneQueue[0].stateOrMessageId != 0xffff) ||
		g_flightMessagePanesForceExpire != 0) {
		uint8_t queuedCount;

		queuedCount = g_readyMessageQueueCount;
		if (queuedCount != 0) {
			uint16_t paneType;
			int moveCount;
			int queueIdx;

			moveCount = queuedCount;
			for (queueIdx = 0; queueIdx < moveCount; ++queueIdx) {
				memcpy(&g_readyMessagePaneQueue[queueIdx], &g_readyMessagePaneQueue[queueIdx + 1],
					   sizeof(g_readyMessagePaneQueue[0]));
			}
			paneType = g_readyMessagePaneQueue[0].paneType;
			g_readyMessageQueueCount = (uint8_t)(queuedCount - 1u);
			Hud_ShowFlightMessagePane((int16_t)paneType, 1);
		} else {
			g_readyMessagePaneVisible = 0;
			Hud_ClearReadyMessageTextPane();
			g_readyMessagePaneQueue[0].stateOrMessageId = 0xffff;
		}
		localPlayerIdx = g_localPlayer;
	}

	if ((g_playerFlightTransientTimers[localPlayerIdx].systemMessagePaneTimer == 0 &&
		 g_systemMessagePane.stateOrMessageId != 0xffff) ||
		g_flightMessagePanesForceExpire != 0) {
		g_systemMessagePaneVisible = 0;
		Hud_ClearSystemTextPane();
		localPlayerIdx = g_localPlayer;
		g_systemMessagePane.stateOrMessageId = 0xffff;
	}

	if ((g_playerFlightTransientTimers[localPlayerIdx].flightGroupMessagePaneTimer == 0 &&
		 g_flightGroupMessagePane.stateOrMessageId != 0xffff) ||
		g_flightMessagePanesForceExpire != 0) {
		g_flightGroupMessagePaneVisible = 0;
		Hud_ClearFlightGroupTextPane();
		localPlayerIdx = g_localPlayer;
		g_flightGroupMessagePane.stateOrMessageId = 0xffff;
	}

	if (g_playerFlightTransientTimers[localPlayerIdx].targetDescriptionRefreshTimer == 0) {
		int16_t currentTargetObjectIdx;

		currentTargetObjectIdx = g_players[localPlayerIdx].currentTargetObjectIdx;
		if (currentTargetObjectIdx != (int16_t)0xffff &&
			g_targetDescriptionMessageId !=
				msg_BuildTargetDescription(currentTargetObjectIdx, localPlayerIdx, 0, 0)) {
			if (msg_BuildTargetDescription(g_players[g_localPlayer].currentTargetObjectIdx, g_localPlayer, 0,
										   1)) {
				uint8_t hudStateLive;

				hudStateLive = g_players[g_localPlayer].viewState.hudStateLive;
				if (hudStateLive == 19 || hudStateLive == 0 || hudStateLive == 20) {
					g_targetDescriptionMessageId = msg_BuildTargetDescription(
						g_players[g_localPlayer].currentTargetObjectIdx, g_localPlayer, 1, 0);
					g_playerFlightTransientTimers[g_localPlayer].targetDescriptionRefreshTimer = 1180;
				}
			}
		}
	}

	for (localPlayerIdx = 0; localPlayerIdx < XWA_PLAYER_COUNT; ++localPlayerIdx) {
		if (g_players[localPlayerIdx].connectedFlag != 0 &&
			g_players[localPlayerIdx].pendingActionTimer == 0) {
			g_players[localPlayerIdx].pendingActionId = 0;
		}
	}

	if (g_flightMessagePanesForceExpire != 0) {
		g_flightMessagePanesForceExpire = 0;
	}
}

// FUNCTION: XWA 0x498740
void Hud_ClearReadyMessageQueue(void) {
	g_readyMessageQueueCount = 0;
	g_readyMessagePaneQueue[0].stateOrMessageId = 0xffff;
	g_playerFlightTransientTimers[g_localPlayer].systemMessagePaneTimer = 0;
}

// FUNCTION: XWA 0x498770
void Hud_AdvanceFlightMessagePaneTimers(void) {
	if (g_readyMessagePaneQueue[0].stateOrMessageId != 0xffff) {
		++g_readyMessagePaneQueue[0].ageTicks;
	}
	if (g_systemMessagePane.stateOrMessageId != 0xffff) {
		++g_systemMessagePane.ageTicks;
	}
	if (g_flightGroupMessagePane.stateOrMessageId != 0xffff) {
		++g_flightGroupMessagePane.ageTicks;
	}
}

// FUNCTION: XWA 0x4987B0
void Hud_DrawNetworkStatusIndicators(void) {
	int cursorLineHeight;
	int cursorYInt;
	float cursorY;

	if (g_flightPlayerCount <= 1 || (g_pingIndicator == 0 && g_lagIndicator == 0)) {
		return;
	}
	HUD_PANE_PUSH(XWA_HUD_PANE_NETWORK, 0, 0, g_screenWidth, g_screenHeight);

	FlightText_SetRenderOffset(0, 0);
	FlightText_SetShadowEnabled(1u);
	FlightText_SetFontTier(0);
	FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
	FlightText_SetClipRect(0, 0, (uint16_t)g_screenWidth, (uint16_t)g_screenHeight);
	cursorY = g_flightHudScaleFactor;
	cursorY *= flt_5A9AC4;
	cursorYInt = (int)cursorY;
	cursorY = g_flightHudScaleFactor;
	cursorY *= flt_5A9AC8;
	FlightText_SetCursor((int16_t)(int)cursorY, (int16_t)cursorYInt);

	if (g_pingIndicator != 0) {
		switch (g_pingIndicator) {
			case 1:
				FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
				FlightText_SetColor(0x4Eu);
				break;
			case 2:
				FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
				FlightText_SetColor(0x4Bu);
				break;
			case 3:
				FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
				FlightText_SetColor(0x4Au);
				break;
		}
		FlightText_SetShadowColor(g_flightColorEscapeBypassChar);
		FlightText_DrawString(g_strPanelStrings[PANEL_STRING_WARP_STRING]);
	}

	cursorLineHeight = (uint8_t)g_flightFontLineHeight;
	cursorY = g_flightHudScaleFactor;
	cursorY = cursorY * flt_5A9ACC + cursorLineHeight;
	cursorYInt = (int)cursorY;
	cursorY = g_flightHudScaleFactor;
	cursorY *= flt_5A9AC8;
	FlightText_SetCursor((int16_t)(int)cursorY, (int16_t)cursorYInt);
	if (g_lagIndicator != 0) {
		switch (g_lagIndicator) {
			case 1:
				FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
				FlightText_SetColor(0x4Eu);
				break;
			case 2:
				FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
				FlightText_SetColor(0x4Bu);
				break;
			case 3:
				FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
				FlightText_SetColor(0x4Au);
				break;
		}
		FlightText_SetShadowColor(g_flightColorEscapeBypassChar);
		FlightText_DrawString(g_strPanelStrings[PANEL_STRING_LATENCY_STRING]);
	}
	HUD_PANE_POP();
}

// FUNCTION: XWA 0x498980
uint16_t Hud_GetSystemMessagePaneState(void) { return g_systemMessagePane.stateOrMessageId; }

// FUNCTION: XWA 0x498990
void msg_reportfgcreation(uint16_t flightGroupIdx, uint16_t speciesIdx) {
	XwaFlightGroup* fg;
	int activeObjIdx;
	uint16_t clicks;
	ObjectTypeId objectType;
	uint16_t iff;
	unsigned int numberOfCraft;
	unsigned int missionDescriptionId;

	missionDescriptionId = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
	if (missionDescriptionId >= 49 && missionDescriptionId <= 52) {
		if (g_missionFlightGroups[flightGroupIdx].fg.iff == 1) {
			return;
		}
	}

	fg = &g_missionFlightGroups[flightGroupIdx].fg;
	if (g_missionFlightGroups[flightGroupIdx].fg.arrivalMethod == 0) {
		Mission_ResolveObjectOrMissionPointWorldLoc(0x8000u, flightGroupIdx, 0, 0);
	} else {
		activeObjIdx = g_activeRegionObjectSlotStart;
		while ((uint16_t)activeObjIdx < g_activeRegionCraftObjectSlotEnd) {
			ObjectRecord* object;

			object = &g_objectTable[(uint16_t)activeObjIdx];
			if (object->objectType != OBJ_None) {
				MobileObject* mobj;
				CraftData* craft;
				uint16_t objectFlightGroupIdx;

				mobj = object->mobj;
				objectFlightGroupIdx = object->flightGroupIdx;
				craft = mobj->pCraft;
				if (objectFlightGroupIdx == flightGroupIdx && craft->leader_obj_idx == -1) {
					Mission_ResolveObjectOrMissionPointWorldLoc((uint16_t)activeObjIdx, 0, 0, 0);
					break;
				}
			}
			++activeObjIdx;
		}
	}

	if (!g_players[g_localPlayer].mapCameraState) {
		ObjectRecord* playerObj;

		playerObj = &g_objectTable[g_players[g_localPlayer].objectIndex];
		trig2_ctop(worldlocx - playerObj->world_x, worldlocy - playerObj->world_y,
				   worldlocz - playerObj->world_z);
	} else {
		trig2_ctop(worldlocx - g_players[g_localPlayer].viewState.savedTargetX,
				   worldlocy - g_players[g_localPlayer].viewState.savedTargetY,
				   worldlocz - g_players[g_localPlayer].viewState.savedTargetZ);
	}

	trig2_polardistance *= 161;
	clicks = (uint16_t)((((trig2_polardistance >> 16) & 0xffff) + 50) / 100);
	if (clicks == 0) {
		clicks = 1;
	}

	numberOfCraft = fg->numberOfCraft;
	objectType = g_objectTypeTables.craftTypeToObjectType[fg->craftType];
	g_msgArgTable[0] = numberOfCraft;
	iff = fg->iff;
	g_msgSenderIff = iff;
	if ((g_modelTypeTable[objectType].flags & 4u) != 0) {
		int playerIdx;

		g_msgArgTable[2] = clicks;
		playerIdx = g_localPlayer;
		g_msgArgTable[0] = 0x8000u;
		g_msgArgTable[1] = 0x8001u;
		g_msgPtrs[1] = g_missionFlightGroups[flightGroupIdx].fg.name;
		g_msgPtrs[0] = g_modelDefs[speciesIdx].nameAlt;
		msg_emitInFlightMessage(MSG_BUOY_ACTIVATED, playerIdx);
		return;
	}

	{
		int playerIdx;

		playerIdx = g_localPlayer;
		if (iff != (uint16_t)g_players[playerIdx].iff) {
			if (numberOfCraft == 1) {
				g_msgPtrs[1] = g_modelDefs[speciesIdx].nameAlt;
			} else {
				g_msgPtrs[1] = g_strSpeciesNamesPlural[speciesIdx];
			}
			g_msgArgTable[1] = 0x8001u;
			g_msgArgTable[2] = clicks;
			msg_emitInFlightMessage(MSG_SENSOR_OBJ, playerIdx);
			return;
		}

		if (numberOfCraft == 1) {
			g_msgArgTable[0] = 0x8000u;
			g_msgArgTable[1] = 0x8001u;
			g_msgArgTable[2] = clicks;
			g_msgPtrs[0] = g_modelDefs[speciesIdx].nameAlt;
			g_msgPtrs[1] = g_missionFlightGroups[flightGroupIdx].fg.name;
			msg_emitInFlightMessage(MSG_ENTERING_OBJ, playerIdx);
			return;
		}

		g_msgArgTable[1] = 0x8001u;
		g_msgArgTable[2] = 0x8002u;
		g_msgArgTable[3] = clicks;
		g_msgPtrs[1] = g_modelDefs[speciesIdx].nameAlt;
		g_msgPtrs[2] = g_missionFlightGroups[flightGroupIdx].fg.name;
		msg_emitInFlightMessage(MSG_ENTERING_OBJS, playerIdx);
	}
}

// FUNCTION: XWA 0x498CE0
int msg_addMessagePtr(uint16_t slot, const char* text) {
	g_msgPtrs[slot] = text;
	g_msgArgTable[slot] = (uint16_t)(slot + 0x8000);
	return slot;
}

// FUNCTION: XWA 0x498D10
void msg_emitCraftMessage(uint16_t objIdx, CraftData* craft, int16_t msgTemplateId) {
	uint16_t flightGroupIdx;
	unsigned int modelIndex;
	int16_t craftNumber;

	if (g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] != 49 || msgTemplateId != 143 ||
		g_objectTable[objIdx].genusId != 0) {
		if (g_objectTable[objIdx].regionIdx == g_players[g_localPlayer].regionIndex) {
			flightGroupIdx = g_objectTable[objIdx].flightGroupIdx;
			g_msgSenderIff = (uint8_t)g_objectTable[objIdx].mobj->iff;
			modelIndex = craft->modelIndex;
			g_msgArgTable[0] = 0x8000u;
			g_msgPtrs[0] = g_modelDefs[modelIndex].nameLong;
			if ((int8_t)craft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] >= 0) {
				g_msgPtrs[1] = g_missionFlightGroups[flightGroupIdx].fg.name;
			} else {
				g_msgPtrs[1] = g_strInFlightMessages[535];
			}
			g_msgArgTable[1] = 0x8001u;

			craftNumber = (int16_t)Hud_MissionFG_GetCraftNumberIfShown(flightGroupIdx, craft);
			if (craftNumber != 0) {
				int playerIdx;

				playerIdx = g_localPlayer;
				g_msgArgTable[2] = (uint16_t)craftNumber;
				g_msgArgTable[3] = (uint16_t)msgTemplateId;
				msg_emitInFlightMessage(MSG_FRAME_MANY, playerIdx);
			} else {
				int playerIdx;

				playerIdx = g_localPlayer;
				g_msgArgTable[2] = (uint16_t)msgTemplateId;
				msg_emitInFlightMessage(MSG_FRAME_SINGLE, playerIdx);
			}
		}
	}
}

// FUNCTION: XWA 0x498E70
int msg_radioMessage(uint16_t objIdx, CraftData* craft, int16_t hudMessageId, uint16_t voiceVariant,
					 int16_t broadcast) {
	uint16_t flightGroupIdx;
	uint16_t craftNumber;

	flightGroupIdx = g_objectTable[objIdx].flightGroupIdx;
	g_msgSenderIff = g_missionFlightGroups[flightGroupIdx].fg.iff;
	if (g_msgSenderIff == (uint16_t)g_players[g_localPlayer].iff &&
		g_missionFlightGroups[flightGroupIdx].fg.team == (uint16_t)g_players[g_localPlayer].playerIff) {
		if (broadcast) {
			g_msgPtrs[0] = g_missionFlightGroups[flightGroupIdx].fg.name;
			g_msgArgTable[0] = 0x8000u;
			g_msgArgTable[1] = (uint16_t)hudMessageId;
			msg_emitInFlightMessage(MSG_ACK_WINGMAN, g_localPlayer);
		} else {
			const char* modelName;

			modelName = g_modelDefs[craft->modelIndex].nameLong;
			g_msgArgTable[0] = 0x8000u;
			g_msgPtrs[1] = g_missionFlightGroups[flightGroupIdx].fg.name;
			g_msgArgTable[1] = 0x8001u;
			g_msgPtrs[0] = modelName;
			craftNumber = (int16_t)Hud_MissionFG_GetCraftNumberIfShown(flightGroupIdx, craft);
			if (craftNumber != 0) {
				g_msgArgTable[2] = craftNumber;
				g_msgArgTable[3] = (uint16_t)hudMessageId;
				msg_emitInFlightMessage(MSG_ACK_MANY, g_localPlayer);
			} else {
				g_msgArgTable[2] = hudMessageId;
				msg_emitInFlightMessage(MSG_ACK_SINGLE, g_localPlayer);
			}
		}

		if (objIdx == g_players[g_localPlayer].objectIndex) {
			return fsfx_speakorderack(g_localPlayer, -2, 1, voiceVariant, 0xfffffffeu, 0xffffu);
		}
		return fsfx_speakorderack(g_localPlayer, objIdx, 1, voiceVariant, objIdx, 0xffffu);
	}

	return flightGroupIdx;
}

// FUNCTION: XWA 0x499000
void msg_reportmessage(uint16_t objIdx, CraftData* craft, int16_t msgTemplateId) {
	uint16_t flightGroupIdx;
	int16_t craftNumber;
	const char* modelName;

	if (g_objectTable[objIdx].regionIdx == g_players[g_localPlayer].regionIndex) {
		flightGroupIdx = g_objectTable[objIdx].flightGroupIdx;
		g_msgSenderIff = g_missionFlightGroups[flightGroupIdx].fg.iff;
		modelName = g_modelDefs[craft->modelIndex].nameLong;
		g_msgArgTable[0] = 0x8000u;
		g_msgPtrs[1] = g_missionFlightGroups[flightGroupIdx].fg.name;
		g_msgArgTable[1] = 0x8001u;
		g_msgPtrs[0] = modelName;

		craftNumber = (uint16_t)Hud_MissionFG_GetCraftNumberIfShown(flightGroupIdx, craft);
		if (craftNumber != 0) {
			g_msgArgTable[2] = craftNumber;
			g_msgArgTable[3] = (uint16_t)msgTemplateId;
			msg_emitInFlightMessage(MSG_REPORT_IN_MANY, g_localPlayer);
			return;
		}

		g_msgArgTable[2] = (uint16_t)msgTemplateId;
		msg_emitInFlightMessage(MSG_REPORT_IN_SINGLE, g_localPlayer);
	}
}

// FUNCTION: XWA 0x499100
int msg_BuildTargetDescription(ObjectIndex targetObjIdx, int playerIdx, int emitHudMessage,
							   int returnActionableOnly) {
	unsigned int targetIdx;
	CraftData* craft;
	unsigned int flightGroupIdx;
	unsigned int boundFlightGroupIdx;
	int team;
	int designation;
	int inspectFlag;
	int destroyFlag;
	int disableFlag;
	int captureFlag;
	int boardedFlag;
	int specialCargoRelevant;
	int actionable;
	uint32_t goalIdx;

	actionable = 0;
	g_msgArgTable[3] = MSG_TARGET_RELS;
	if (targetObjIdx == (ObjectIndex)-1) {
		return 0;
	}

	targetIdx = (uint16_t)targetObjIdx;
	if (g_objectTable[targetIdx].genusId == GENUS_SalvageJunk ||
		g_objectTable[targetIdx].genusId == GENUS_LargeScenery ||
		(targetIdx >= g_projectileObjectSlotStart && targetIdx < g_projectileObjectSlotEnd)) {
		return 0;
	}

	if (targetIdx >= g_activeRegionObjectSlotStart && targetIdx < g_activeRegionCraftObjectSlotEnd) {
		craft = g_objectTable[targetIdx].mobj->pCraft;
	} else {
		craft = NULL;
	}

	flightGroupIdx = g_objectTable[targetIdx].flightGroupIdx;
	if ((unsigned int)flightGroupIdx > (unsigned int)(int16_t)g_missionHeader.numFlightGroups) {
		return 0;
	}

	boundFlightGroupIdx = g_players[playerIdx].boundFlightGroupIdx;
	if (g_objectTable[targetIdx].mobj == NULL) {
		team = g_missionFlightGroups[flightGroupIdx].fg.team;
	} else {
		team = g_objectTable[targetIdx].mobj->team;
	}
	designation = g_missionFlightRuntimeState
					  .teamFgDesignationCode[(uint16_t)g_players[playerIdx].playerIff][flightGroupIdx];

	if (designation == 0) {
		if (craft == NULL) {
			if (g_objectTable[targetIdx].genusId == GENUS_Mine) {
				designation = 17;
			} else if (g_objectTable[targetIdx].objectType >= OBJ_CommSat1 &&
					   g_objectTable[targetIdx].objectType <= OBJ_CommSat3) {
				designation = 18;
			} else if (g_objectTable[targetIdx].objectType >= OBJ_Probe &&
					   g_objectTable[targetIdx].objectType <= OBJ_ProbeCapsule) {
				designation = 19;
			} else if (g_objectTable[targetIdx].objectType >= OBJ_NavBuoy1 &&
					   g_objectTable[targetIdx].objectType <= OBJ_NavBuoy2) {
				designation = 20;
			}
		} else {
			if (boundFlightGroupIdx == flightGroupIdx) {
				designation = (targetIdx - g_players[playerIdx].altViewObjectIdx) ? 14 : 22;
			} else if ((uint16_t)g_players[playerIdx].playerIff == team) {
				designation = 13;
			} else {
				designation = (g_objectTable[targetIdx].genusId == GENUS_Container) + 15;
			}
		}
	}

	msg_formatObjectName(targetObjIdx, 0, g_flightTextScratchBuffer);
	g_msgPtrs[0] = g_flightTextScratchBuffer;
	g_msgArgTable[0] = 0x8000u;
	g_msgArgTable[1] = MSG_TARGET_RELS;
	if (designation != 0) {
		if (g_targetDescDesignationUsesRelationText[designation] != 0) {
			if ((uint16_t)g_players[playerIdx].playerIff == team) {
				g_msgArgTable[1] = MSG_257_REL_OUR;
			} else if (Object_IsHostileToTeam(targetObjIdx, (uint16_t)g_players[playerIdx].playerIff) == 1) {
				++g_msgArgTable[1];
			} else if (Object_IsFriendlyToTeam(targetObjIdx, (uint16_t)g_players[playerIdx].playerIff) == 1) {
				g_msgArgTable[1] = (uint16_t)(g_msgArgTable[1] + 2u);
			} else {
				g_msgArgTable[1] = (uint16_t)(g_msgArgTable[1] + 4u);
			}
		}
		g_msgArgTable[2] = (uint16_t)(designation + MSG_TARGET_DESC_BLANK);
	} else {
		g_msgArgTable[2] = MSG_TARGET_RELS;
	}

	inspectFlag = 0;
	captureFlag = disableFlag = destroyFlag = 0;
	boardedFlag = 0;
	specialCargoRelevant = 0;

	for (goalIdx = 0; goalIdx < 8; ++goalIdx) {
		if (g_missionFlightGroups[flightGroupIdx]
					.fg.fgGoals[goalIdx]
					.payload.enabledForTeam[(uint16_t)g_players[playerIdx].playerIff] != 0 &&
			g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.argument == 0 &&
			g_missionFgStats[flightGroupIdx]
					.goalState[8 * (uint16_t)g_players[playerIdx].playerIff + goalIdx] == 4) {
			int condition;

			if (g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.amount == 6) {
				if (g_missionFgStats[flightGroupIdx].specialCargoOutcome[8] == 0) {
					inspectFlag = 1;
				}
				specialCargoRelevant = 1;
			}

			condition = g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.condition;
			if (condition == 2) {
				destroyFlag = 1;
			} else if (condition != 3) {
				if (condition == 8) {
					disableFlag = 1;
				} else if (condition == 5) {
					inspectFlag = 1;
				} else if (condition == 4 || condition == 44) {
					captureFlag = 1;
				} else if (condition == 6) {
					boardedFlag = 1;
				}
			}
		}
	}

	{
		uint32_t pairIdx;
		uint32_t triggerIdx;

		for (pairIdx = 0; pairIdx < 2; ++pairIdx) {
			for (triggerIdx = 0; triggerIdx < 2; ++triggerIdx) {
				XwaTrigger* trigger;
				int condition;

				trigger = &g_missionGlobalGoals[(uint16_t)g_players[playerIdx].playerIff][0]
							   .triggerPairs[pairIdx]
							   .triggers[triggerIdx];
				condition = trigger->condition;
				if (condition != 10 && (uint16_t)Mission_FlightGroupMatchesTriggerVariable(
										   flightGroupIdx, (MissionTriggerVariableType)trigger->variableType,
										   trigger->variable) != 0) {
					if (condition == 2) {
						destroyFlag = 1;
					} else if (condition != 3) {
						if (condition == 8) {
							disableFlag = 1;
						} else if (condition == 5) {
							inspectFlag = 1;
						} else if (condition == 4 || condition == 44) {
							captureFlag = 1;
						} else if (condition == 6) {
							boardedFlag = 1;
						}
					}
				}
			}
		}
	}

	if (targetIdx >= g_activeRegionObjectSlotStart && targetIdx < g_activeRegionCraftObjectSlotEnd) {
		if (inspectFlag != 0 && (int8_t)craft->iffVisibility[(uint16_t)g_players[playerIdx].playerIff] > 0) {
			inspectFlag = 0;
		}

		if (captureFlag != 0 || boardedFlag != 0) {
			if (g_objectTable[targetIdx].mobj->speed != 0) {
				disableFlag = 1;
			}
			if (specialCargoRelevant != 0 &&
				g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft != craft->waveNumber) {
				captureFlag = 0;
				boardedFlag = 0;
			}
			if (g_objectTable[targetIdx].mobj->team == (uint16_t)g_players[playerIdx].playerIff) {
				captureFlag = 0;
				boardedFlag = 0;
			}
		}

		if (disableFlag != 0 && specialCargoRelevant != 0 &&
			g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft != craft->waveNumber) {
			disableFlag = 0;
		}
		if (destroyFlag != 0 && specialCargoRelevant != 0 &&
			g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft != craft->waveNumber) {
			destroyFlag = 0;
		}
	}

	if (inspectFlag != 0) {
		g_msgArgTable[3] = MSG_TARGET_INSPECT;
		actionable = 1;
	} else if (disableFlag != 0) {
		if (captureFlag != 0) {
			g_msgArgTable[3] = MSG_TARGET_CAPTURE;
		} else if (boardedFlag != 0) {
			g_msgArgTable[3] = MSG_TARGET_BOARDED;
		} else {
			g_msgArgTable[3] = MSG_TARGET_DISABLE;
		}
		if (pai_OrderSlotMatchingObjectHasOrderClass(g_players[playerIdx].objectIndex, 19, targetIdx) == 1) {
			++g_msgArgTable[3];
			actionable = 1;
		}
	} else if (captureFlag != 0) {
		g_msgArgTable[3] = MSG_TARGET_CAPTURE;
		if (pai_OrderSlotMatchingObjectHasOrderClass(g_players[playerIdx].objectIndex, 31, targetIdx) == 1) {
			actionable = 1;
		}
	} else if (boardedFlag != 0) {
		g_msgArgTable[3] = MSG_TARGET_BOARDED;
		if (pai_OrderSlotMatchingObjectHasOrderClass(g_players[playerIdx].objectIndex, 31, targetIdx) == 1) {
			actionable = 1;
		}
	} else if (destroyFlag != 0) {
		g_msgArgTable[3] = MSG_TARGET_DESTROY;
		if (pai_OrderSlotMatchingObjectHasOrderClass(g_players[playerIdx].objectIndex, 69, targetIdx) == 1 ||
			pai_OrderSlotMatchingObjectHasOrderClass(g_players[playerIdx].objectIndex, 116, targetIdx) == 1) {
			++g_msgArgTable[3];
			actionable = 1;
		}
	}

	if (emitHudMessage != 0 && playerIdx == g_localPlayer) {
		msg_emitInFlightMessage(MSG_TARGET_DESC_BLANK, playerIdx);
		g_playerFlightTransientTimers[g_localPlayer].targetDescriptionRefreshTimer = 1180;
		g_targetDescriptionMessageId = g_msgArgTable[3];
	}

	if (returnActionableOnly != 0) {
		return actionable;
	}
	return g_msgArgTable[3];
}

static __inline char* msg_appendText(char* outName, const char* text) {
	char ch;

	ch = *outName;
	while (ch) {
		++outName;
		ch = *outName;
	}
	ch = *text;
	while (ch) {
		*outName = ch;
		++outName;
		++text;
		ch = *text;
	}
	*outName = 0;
	return outName;
}

static __inline char* msg_copyText(char* outName, const char* text) {
	char ch;

	ch = *text;
	while (ch) {
		*outName = ch;
		++outName;
		++text;
		ch = *text;
	}
	*outName = ch;
	return outName;
}

static __inline char* msg_appendChar(char* outName, char ch) {
	while (*outName) {
		++outName;
	}
	*outName++ = ch;
	*outName = 0;
	return outName;
}

// FUNCTION: XWA 0x4997B0
void msg_formatObjectName(uint16_t objIdx, uint16_t nameMode, char* outName) {
	MobileObject* mobj;
	uint16_t objectType;

	*outName = 0;
	mobj = g_objectTable[objIdx].mobj;
	objectType = g_objectTable[objIdx].objectType;

	if (mobj != NULL) {
		if (mobj->state == 0) {
			CraftData* craft;
			uint16_t flightGroupIdx;

			craft = mobj->pCraft;
			flightGroupIdx = g_objectTable[objIdx].flightGroupIdx;
			if (nameMode == 1) {
				char* outCursor;
				const char* text;
				char ch;

				outCursor = outName;
				text = g_modelDefs[craft->modelIndex].nameAlt;
				ch = *text;
				while (ch) {
					*outCursor = ch;
					++outCursor;
					++text;
					ch = *text;
				}
				*outCursor = 0;
			} else if (nameMode == 0) {
				char* outCursor;
				const char* text;
				char ch;

				outCursor = outName;
				text = g_modelDefs[craft->modelIndex].nameLong;
				ch = *text;
				while (ch) {
					*outCursor = ch;
					++outCursor;
					++text;
					ch = *text;
				}
				*outCursor = 0;
			}

			if ((int8_t)craft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] >= 0) {
				uint16_t craftNumber;

				if (g_missionFlightGroups[flightGroupIdx].fg.name[0] != 0) {
					msg_appendChar(outName, ' ');
					msg_appendText(outName, g_missionFlightGroups[flightGroupIdx].fg.name);
				}
				craftNumber = (uint16_t)Hud_MissionFG_GetCraftNumberIfShown(flightGroupIdx, craft);
				if (craftNumber != 0) {
					msg_appendChar(outName, ' ');
					if (craftNumber >= 10) {
						int tensDigit;
						int onesDigit;

						tensDigit = craftNumber / 10;
						onesDigit = craftNumber % 10;
						msg_appendChar(outName, (char)('0' + tensDigit));
						msg_appendChar(outName, (char)('0' + onesDigit));
					} else {
						msg_appendChar(outName, (char)('0' + craftNumber));
					}
				}
			}
			return;
		}

		if (objectType >= OBJ_WarheadTorpedo && objectType <= OBJ_WarheadFlare) {
			msg_copyText(outName, g_strWarheadNames[objectType - OBJ_WarheadTorpedo]);
			return;
		}
		if (objectType >= OBJ_CommSat1 && objectType <= OBJ_NavBuoy2) {
			msg_copyText(outName, g_strBuoyNames[objectType - OBJ_CommSat1]);
			return;
		}
		return;
	}

	if ((nameMode == 1 || nameMode == 0) && objectType - OBJ_CommSat1 < 13) {
		const char* buoyName;
		char* outCursor;
		char ch;

		buoyName = g_strBuoyNames[objectType - OBJ_CommSat1];
		outCursor = outName;
		ch = *buoyName;
		while (ch) {
			*outCursor = ch;
			++outCursor;
			++buoyName;
			ch = *buoyName;
		}
		*outCursor = 0;
		if (g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.name[0] != 0) {
			ObjectRecord* objectTable;
			char* appendCursor;
			const char* text;
			char appendCh;

			msg_appendChar(outName, ' ');
			objectTable = g_objectTable;
			appendCursor = outName;
			text = g_missionFlightGroups[objectTable[objIdx].flightGroupIdx].fg.name;
			appendCh = *appendCursor;
			while (appendCh) {
				++appendCursor;
				appendCh = *appendCursor;
			}
			appendCh = *text;
			while (appendCh) {
				*appendCursor = appendCh;
				++appendCursor;
				++text;
				appendCh = *text;
			}
			*appendCursor = 0;
		}
		return;
	}

	msg_copyText(outName, g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.name);
}

// FUNCTION: XWA 0x499B00
void msg_emitLocalPlayerCraftMessage(uint16_t messageId) {
	int objectIdx;

	objectIdx = g_players[g_localPlayer].objectIndex;
	g_msgSenderIff = (uint8_t)g_objectTable[objectIdx].mobj->iff;
	g_msgPtrs[0] = g_modelDefs[g_objectTable[objectIdx].mobj->pCraft->modelIndex].nameLong;
	g_msgArgTable[0] = 0x8000u;
	g_msgPtrs[1] = g_missionFlightGroups[g_objectTable[objectIdx].flightGroupIdx].fg.name;
	g_msgArgTable[1] = 0x8001u;
	g_msgArgTable[2] = Hud_MissionFG_GetCraftNumberIfShown(g_objectTable[objectIdx].flightGroupIdx,
														   g_objectTable[objectIdx].mobj->pCraft);

	msg_emitInFlightMessage(messageId, g_localPlayer);
}

// FUNCTION: XWA 0x499BC0
uint16_t Hud_MeasureFlightMessagePaneText(int16_t paneType) {
	const char* text;
	char str[80];
	uint16_t sourceCount;
	uint16_t outCount;
	char ch;

	if (paneType == 3 || paneType == 4 || paneType == 7) {
		text = g_systemMessagePane.text;
	} else if (paneType == 8) {
		text = g_flightGroupMessagePane.text;
	} else {
		text = g_readyMessagePaneQueue[0].text;
	}

	if ((uint8_t)*text < 9u) {
		++text;
	}

	sourceCount = 0;
	outCount = 0;
	ch = *text;
	while (ch != '\0' && sourceCount < 0x46u) {
		if (ch == '[' || ch == ']') {
			if ((uint8_t)ch == 0xfeu) {
				text += 2;
			}
		} else {
			str[outCount++] = ch;
		}
		++text;
		++sourceCount;
		ch = *text;
	}
	str[outCount] = '\0';
	return FlightText_MeasureStringWidth(str);
}

// FUNCTION: XWA 0x499C50
void Mfd_DrawMessageLogPage(int mfdSide, void* mfdSurface) {
	enum {
		MFD_MESSAGE_LOG_SCROLL_UP_KEY = 0x00a6,
		MFD_MESSAGE_LOG_SCROLL_DOWN_KEY = 0x00a7,
		MFD_MESSAGE_LOG_MAX_RECORDS = 300
	};

	uint16_t paneWidth;
	int16_t paneHeight;
	uint16_t drawCount;
	int16_t lineStep;
	int scrollOffset;
	int wrappedLineCount;
	int row;
	int cursorY;
	int recordRow;
	int16_t stopRow;
	int totalWrappedLines;
	int16_t totalRows;
	char measureText[80];

	paneHeight = (uint16_t)g_hudMfdPaneHeight;
	paneWidth = (uint16_t)g_hudMfdPaneWidth;
	if (g_useHardware3D) {
		if (mfdSide == 1) {
			FlightText_SetRenderOffset(0, (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		} else {
			FlightText_SetRenderOffset((int16_t)(g_screenWidth - g_hudMfdPaneWidth),
									   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		}
	} else {
		FlightSw_SetRenderTarget(mfdSurface, (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
								 (uint16_t)g_hudMfdPaneWidth * g_flight16bppBytesPerPixel);
	}

	FlightText_SetClipRect(0, 0, paneWidth, paneHeight);
	FlightText_SetWordWrap(1u);
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	lineStep = (uint8_t)g_flightFontLineHeight + 2;
	FlightText_SetColor(0x43u);

	drawCount = g_messageLogTotalCount;
	g_messageLogDrawTotalCount = drawCount;
	if (g_missionHeader.body.hangar != XWA_HANGAR_SKIRMISH &&
		g_missionHeader.body.hangar != XWA_HANGAR_QUICKSTART && g_missionHeader.body.hangar != 0 &&
		drawCount != 0 && g_readyMessagePaneQueue[0].stateOrMessageId != 0xffffu) {
		uint8_t readyClockTick;
		uint8_t readyClockMinute;
		uint8_t readyClockHour;

		readyClockTick = g_readyMessagePaneQueue[0].clockTick;
		readyClockMinute = g_readyMessagePaneQueue[0].clockMinute;
		readyClockHour = g_readyMessagePaneQueue[0].clockHour;
		do {
			if (g_missionTimeLimitActive) {
				if (g_messageLogRecords[drawCount - 1].clockHour > readyClockHour ||
					(g_messageLogRecords[drawCount - 1].clockHour == readyClockHour &&
					 (g_messageLogRecords[drawCount - 1].clockMinute > readyClockMinute ||
					  (g_messageLogRecords[drawCount - 1].clockMinute == readyClockMinute &&
					   g_messageLogRecords[drawCount - 1].clockTick >= readyClockTick)))) {
					break;
				}
			} else {
				if (g_messageLogRecords[drawCount - 1].clockHour < readyClockHour ||
					(g_messageLogRecords[drawCount - 1].clockHour == readyClockHour &&
					 (g_messageLogRecords[drawCount - 1].clockMinute < readyClockMinute ||
					  (g_messageLogRecords[drawCount - 1].clockMinute == readyClockMinute &&
					   g_messageLogRecords[drawCount - 1].clockTick <= readyClockTick)))) {
					break;
				}
			}
			g_messageLogDrawTotalCount = --drawCount;
		} while (drawCount != 0);
	}
	if (drawCount == 0) {
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_SetCursor(0, 0);
		FlightText_DrawStringCentered(g_strMfdStrings[MFD_STRING_NO_RADIO_MSG]);
		FlightText_SetWordWrap(0);
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
			return;
		}
	} else {
		if (mfdSide == 1) {
			scrollOffset = g_mfdMessageLogLeftScrollOffset;
			wrappedLineCount = g_mfdMessageLogLeftWrappedLineCount;
		} else {
			scrollOffset = g_mfdMessageLogRightScrollOffset;
			wrappedLineCount = g_mfdMessageLogRightWrappedLineCount;
		}

		g_messageLogRecords = (HudInFlightMessageRecord*)Memory_LockHandle(g_messageLogHandle);
		Memory_UnlockHandle(g_messageLogHandle);
		cursorY = 0;
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}

		if (mfdSide == g_players[g_localPlayer].mfd.activeIndex) {
			switch (g_currentActionKey) {
				case MFD_MESSAGE_LOG_SCROLL_UP_KEY:
					if (scrollOffset > 0) {
						if (mfdSide == 1) {
							scrollOffset = --g_mfdMessageLogLeftScrollOffset;
						} else if (mfdSide == 2) {
							g_mfdMessageLogRightScrollOffset--;
							scrollOffset = g_mfdMessageLogRightScrollOffset;
						}
					}
					break;

				case MFD_MESSAGE_LOG_SCROLL_DOWN_KEY:
					if (g_messageLogWrapped) {
						totalRows = MFD_MESSAGE_LOG_MAX_RECORDS;
					} else {
						totalRows = g_messageLogWriteIndex;
					}
					if (scrollOffset <= wrappedLineCount + totalRows - (int16_t)paneHeight / lineStep) {
						if (mfdSide == 1) {
							scrollOffset = ++g_mfdMessageLogLeftScrollOffset;
						} else if (mfdSide == 2) {
							g_mfdMessageLogRightScrollOffset++;
							scrollOffset = g_mfdMessageLogRightScrollOffset;
						}
					}
					break;

				default:
					break;
			}
		}

		totalWrappedLines = 0;
		stopRow = (int16_t)(scrollOffset + (int16_t)paneHeight / lineStep);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}

		recordRow = 0;
		for (row = 0; row < stopRow; ++row, ++recordRow) {
			if (recordRow >= scrollOffset && row < MFD_MESSAGE_LOG_MAX_RECORDS) {
				int recordIndex;
				int messageIndex;
				int rowY;
				const char* text;
				uint8_t prefix;
				uint8_t lastChar;
				uint16_t timeWidth;
				char wrappedLine;
				char wrappedLines;

				rowY = (int16_t)cursorY;
				FlightText_SetCursor(0, (uint16_t)rowY);
				if (g_messageLogDrawTotalCount <= MFD_MESSAGE_LOG_MAX_RECORDS) {
					recordIndex = (int)g_messageLogDrawTotalCount - recordRow - 1;
				} else {
					recordIndex = (int)(uint16_t)g_messageLogWriteIndex - recordRow - 1;
					if (recordIndex < 0) {
						recordIndex += MFD_MESSAGE_LOG_MAX_RECORDS;
					}
				}
				if (recordIndex >= 0) {
					FlightText_SetColor(0x42u);
					FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
					messageIndex = recordIndex;
					if (g_messageLogRecords[messageIndex].clockHour != 0) {
						FlightText_DrawDecimalNumber(g_messageLogRecords[messageIndex].clockHour, 2u, 1u);
						g_flightDrawCharFn(':');
						FlightText_DrawDecimalNumber(g_messageLogRecords[messageIndex].clockMinute, 2u, 2u);
					} else {
						FlightText_DrawDecimalNumber(g_messageLogRecords[messageIndex].clockMinute, 2u, 1u);
					}
					g_flightDrawCharFn(':');
					FlightText_DrawDecimalNumber(g_messageLogRecords[messageIndex].clockTick, 2u, 2u);
					timeWidth = g_messageLogRecords[messageIndex].clockHour != 0
									? FlightText_MeasureStringWidth("00:00:00 ")
									: FlightText_MeasureStringWidth("00:00 ");
					FlightText_SetCursor((int16_t)timeWidth, (int16_t)rowY);

					text = g_messageLogRecords[messageIndex].text;
					prefix = (uint8_t)*text;
					if (prefix < 9u) {
						FlightText_SetColor(g_messageTextPrefixColorCodes[prefix]);
						++text;
						if (prefix == 2u || prefix == 1u) {
							FlightText_SetColor(
								g_messageSenderIffColorCodes[g_messageLogRecords[messageIndex].senderIff]);
						}
					} else {
						FlightText_SetColor(0x42u);
					}

					measureText[0] = g_emptyString[0];
					memset(&measureText[1], 0, sizeof(g_messageLogRecords[0].text) - 1);
					wrappedLine = 0;
					wrappedLines = 0;
					lastChar = 0;
					while (*text != '\0') {
						uint8_t ch;

						ch = (uint8_t)*text;
						if (ch == '[') {
							if (g_flightTextColorIndex == 0xd4u) {
								g_flightTextColorIndex = 0xd3u;
							} else {
								++g_flightTextColorIndex;
							}
						} else if (ch == ']') {
							if (g_flightTextColorIndex == 0xd4u) {
								g_flightTextColorIndex = 0xd5u;
							} else {
								--g_flightTextColorIndex;
							}
						} else if (ch == ' ') {
							const char* nextChar;
							int length;

							nextChar = text + 1;
							length = 0;
							if (*nextChar != ' ') {
								while (*nextChar != '\0') {
									if (*nextChar == '[' || *nextChar == ']') {
										++nextChar;
									}
									if (*nextChar == '\0') {
										break;
									}
									measureText[length++] = *nextChar;
									++nextChar;
									if (*nextChar == ' ') {
										break;
									}
								}
							}
							measureText[length] = ' ';
							measureText[length + 1] = '\0';
							if (g_flightCursorX + FlightText_MeasureStringWidth(measureText) >=
								g_flightClipRight - (uint16_t)g_hudMfdTextInsetX) {
								g_flightDrawCharFn('\n');
								wrappedLine = 1;
								++wrappedLines;
							} else {
								g_flightDrawCharFn(*text);
								lastChar = (uint8_t)*text;
							}
						} else {
							if (wrappedLine) {
								g_flightCursorX += FlightText_MeasureStringWidth("00:00 ");
							}
							g_flightDrawCharFn(*text);
							lastChar = (uint8_t)*text;
							wrappedLine = 0;
						}
						++text;
					}

					if (lastChar != '?' && lastChar != '!' && lastChar != ':' && lastChar != '.') {
						g_flightDrawCharFn('.');
					}
					if (wrappedLines != 0) {
						totalWrappedLines += wrappedLines;
						cursorY += lineStep * wrappedLines;
					}
					cursorY += lineStep;
				}
			}
		}

		if (mfdSide == 1) {
			g_mfdMessageLogLeftWrappedLineCount = totalWrappedLines;
		} else {
			g_mfdMessageLogRightWrappedLineCount = totalWrappedLines;
		}
		if (mfdSide == 1 && g_players[g_localPlayer].mfd.page[1] == g_players[g_localPlayer].mfd.page[2]) {
			g_mfdMessageLogSharedPageLeftDrawn = 1;
		}
		if (mfdSide == 2 && g_mfdMessageLogSharedPageLeftDrawn) {
			g_mfdMessageLogSharedPageLeftDrawn = 0;
		}
		g_mfdMessageLogLastDrawTotalCount = g_messageLogDrawTotalCount;
		FlightText_SetWordWrap(0);

		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
			return;
		}
	}

	FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
	if (mfdSide == 1) {
		Blit16ToFlightSurface(mfdSurface, g_flightColorEscapeBypassChar, 0, 0, 5u,
							  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight), (uint16_t)g_hudMfdPaneWidth,
							  (uint16_t)g_hudMfdPaneHeight,
							  (uint16_t)(g_flight16bppBytesPerPixel * g_hudMfdPaneWidth));
	} else {
		Blit16ToFlightSurface(
			mfdSurface, g_flightColorEscapeBypassChar, 0, 0, (uint16_t)(g_screenWidth - g_hudMfdPaneWidth),
			(uint16_t)(g_screenHeight - g_hudMfdPaneHeight), (uint16_t)g_hudMfdPaneWidth,
			(uint16_t)g_hudMfdPaneHeight, (uint16_t)(g_flight16bppBytesPerPixel * g_hudMfdPaneWidth));
	}
}

// FUNCTION: XWA 0x4BEE80
void Mfd_DrawMissionScoreboardPage(int mfdSide, void* mfdSurface) {
	int16_t maxNameWidth;
	uint16_t paneWidth;
	uint16_t paneHeight;
	int16_t lineStep;
	int16_t scoreColumnX;
	uint16_t spaceWidth;
	int rowCount;
	int bodyY;
	int lastVisibleExclusive;
	int order[10];
	int playerFgCountByTeam[10];
	int ownedFgCountByTeam[10];
	int teamMarkers[10];
	int entryCount;
	int activePlayerCount;
	int playerIdx;
	int16_t row;
	uint16_t rowY;
	int16_t sharedKillCount;
	char text[80];

	entryCount = 0;
	maxNameWidth = (int16_t)entryCount;

	if (g_useHardware3D) {
		if (mfdSide == 1) {
			FlightText_SetRenderOffset((int16_t)g_hudMfdTextInsetX,
									   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		} else {
			int16_t rightX;

			rightX = (int16_t)(g_screenWidth - g_hudMfdTextInsetX);
			rightX = (int16_t)(rightX - g_hudMfdPaneWidth);
			FlightText_SetRenderOffset(rightX, (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		}
	} else {
		FlightSw_SetRenderTarget(mfdSurface, (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
								 (uint16_t)g_hudMfdPaneWidth * g_flight16bppBytesPerPixel);
	}

	FlightText_SetFontTier(0);
	if (mfdSide == 1 && g_mfdLeftNeedsRedraw != 0) {
		g_mfdLeftNeedsRedraw = 0;
	}
	if (mfdSide == 2 && g_mfdRightNeedsRedraw != 0) {
		g_mfdRightNeedsRedraw = 0;
	}

	for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
		if (g_players[playerIdx].connectedFlag != 0) {
			int16_t nameWidth;

			FlightText_SetScratch(NetSession_GetPlayerName(playerIdx));
			nameWidth = (int16_t)FlightText_MeasureStringWidth(g_flightTextScratchBuffer);
			if (maxNameWidth < nameWidth) {
				maxNameWidth = nameWidth;
			}
		}
	}

	{
		int16_t headerNameWidth;

		headerNameWidth = (int16_t)FlightText_MeasureStringWidth(g_strOverlayStrings[9]);
		if (maxNameWidth < headerNameWidth) {
			maxNameWidth = headerNameWidth;
		}
	}

	FlightText_SetScratch(g_strOverlayStrings[11]);
	FlightText_AppendScratchString("          ");
	FlightText_AppendScratchString(g_strOverlayStrings[12]);
	spaceWidth = FlightText_MeasureStringWidth("  ");
	scoreColumnX = maxNameWidth + spaceWidth;
	paneWidth = (uint16_t)(maxNameWidth + FlightText_MeasureStringWidth(g_flightTextScratchBuffer));
	activePlayerCount = g_activeFlightPlayerCount;

	if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
		g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) {
		int fgIdx;

		memset(teamMarkers, 0, sizeof(teamMarkers));
		memset(ownedFgCountByTeam, 0, sizeof(ownedFgCountByTeam));
		memset(playerFgCountByTeam, 0, sizeof(playerFgCountByTeam));

		for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
			if (g_missionFlightGroups[fgIdx].fg.playerNumber != 0 && g_missionFgStats[fgIdx].hasArrived) {
				int team;

				team = g_missionFlightGroups[fgIdx].fg.team;
				++playerFgCountByTeam[team];
			}
			if (g_missionFlightGroups[fgIdx].playerOwnerIdx != -1 && g_missionFgStats[fgIdx].hasArrived) {
				int team;

				team = g_missionFlightGroups[fgIdx].fg.team;
				++ownedFgCountByTeam[team];
			}
		}

		for (row = 0; row < 10; ++row) {
			if (playerFgCountByTeam[row] != 0) {
				order[entryCount++] = row;
			}
		}
#ifdef XWA_MODERN
		if (entryCount > 0)
#endif
			for (row = (int16_t)entryCount; row < 10; ++row) {
				order[row] = order[entryCount - 1];
			}
	} else {
		entryCount = activePlayerCount;
	}

	if (entryCount == 0) {
		entryCount = 1;
	}

	rowCount = entryCount + 1;
	lineStep = (uint8_t)g_flightFontLineHeight + 1;
	paneHeight = (uint16_t)(lineStep * rowCount + 2);

	if (paneWidth != (uint16_t)g_mfdMissionScoreboardLastWidth ||
		activePlayerCount != (int16_t)g_mfdMissionScoreboardLastPlayerCount) {
		uint16_t clearWidth;
		uint16_t clearHeight;

		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		clearWidth = paneWidth;
		clearHeight = paneHeight;
		if ((uint16_t)g_mfdMissionScoreboardLastPlayerCount != 0) {
			clearHeight = (uint16_t)(((uint8_t)g_flightFontLineHeight + 1) *
										 ((int16_t)g_mfdMissionScoreboardLastPlayerCount + 1) +
									 2);
		}
		if ((uint16_t)g_mfdMissionScoreboardLastWidth != 0) {
			clearWidth = (uint16_t)g_mfdMissionScoreboardLastWidth;
		}
		FlightText_SetClipRect(0, 0, clearWidth, clearHeight);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}

		lineStep = (uint8_t)g_flightFontLineHeight + 1;
		paneHeight = (uint16_t)(lineStep * rowCount + 2);
	}

	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetColor(0x46u);
	FlightText_SetClipRect(0, 0, paneWidth, paneHeight);
	if (!g_useHardware3D) {
		g_flightFillClipRectFn();
	}

	FlightText_SetCursor(0, 0);
	if ((g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
		 g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) &&
		playerFgCountByTeam[(uint16_t)g_players[g_localPlayer].playerIff] > 1) {
		FlightText_DrawString(g_strOverlayStrings[10]);
	} else {
		FlightText_DrawString(g_strOverlayStrings[9]);
	}

	FlightText_SetScratch(g_strOverlayStrings[11]);
	FlightText_AppendScratchString("     ");
	FlightText_AppendScratchString(g_strOverlayStrings[12]);
	FlightText_DrawStringRightAligned(g_flightTextScratchBuffer);

	*(uint16_t*)&g_mfdMissionScoreboardLastWidth = paneWidth;
	*(uint16_t*)&g_mfdMissionScoreboardLastPlayerCount = (uint16_t)g_activeFlightPlayerCount;
	*(uint16_t*)&g_mfdMissionScoreboardFirstVisibleRow = 0;

	bodyY = lineStep + 3;
	FlightText_SetClipRect(0, (int16_t)bodyY, paneWidth, paneHeight);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetColor(0x4Eu);
	lastVisibleExclusive = paneHeight / (uint16_t)lineStep + g_mfdMissionScoreboardFirstVisibleRow - 1;

	rowY = bodyY;
	if (g_missionHeader.body.hangar != XWA_HANGAR_QUICKSTART &&
		g_missionHeader.body.hangar != XWA_HANGAR_SKIRMISH) {
		int16_t connectedCount;

		connectedCount = 0;
		for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
			if (g_players[playerIdx].connectedFlag == 1 || g_players[playerIdx].connectedFlag == 2) {
				order[connectedCount++] = playerIdx;
			}
		}
#ifdef XWA_MODERN
		if (connectedCount > 0 && connectedCount < XWA_PLAYER_COUNT)
#endif
			for (row = connectedCount; row < XWA_PLAYER_COUNT; ++row) {
				order[row] = order[connectedCount - 1];
			}

		if (connectedCount > 1) {
			int16_t swapped;

			do {
				swapped = 0;
				for (row = 0; row < connectedCount - 1; ++row) {
					int player;
					int nextPlayer;

					player = order[row];
					nextPlayer = order[row + 1];
					if (player < XWA_PLAYER_COUNT && g_players[nextPlayer].missionStats.missionScore >
														 g_players[player].missionStats.missionScore) {
						order[row] = nextPlayer;
						order[row + 1] = player;
						swapped = 1;
						break;
					}
				}
			} while (swapped);
		}

		{
			int rowBottomY;

			rowBottomY = bodyY + lineStep;
			for (row = 0; row < connectedCount; ++row) {
				int player;

				player = order[row];
				if (row >= (int16_t)g_mfdMissionScoreboardFirstVisibleRow && row < lastVisibleExclusive &&
					player < XWA_PLAYER_COUNT) {
					int16_t totalFullKills;
					int16_t fgIdx;
					int16_t digits;
					int16_t xOffset;

					FlightText_SetScratch(NetSession_GetPlayerName(player));
					FlightText_SetClipRect(0, rowY, paneWidth, (uint16_t)rowBottomY);
					if (!g_useHardware3D) {
						g_flightFillClipRectFn();
					}

					if (g_localPlayer == player) {
						FlightText_SetColor(0x4Eu);
					} else {
						FlightText_SetColor(0x43u);
					}
					FlightText_SetCursor(0, rowY);
					FlightText_DrawString(g_flightTextScratchBuffer);
					g_flightDrawCharFn('\n');

					digits =
						(int16_t)FlightText_FormatScratchInt(g_players[player].missionStats.missionScore);
					if (digits < 6) {
						xOffset = (int16_t)(spaceWidth * (6 - digits));
					} else {
						xOffset = 0;
					}
					FlightText_SetCursor((int16_t)(scoreColumnX + xOffset), rowY);
					strcpy(text, g_flightTextScratchBuffer);
					strcat(text, "    ");

					totalFullKills = 0;
					sharedKillCount = 0;
					for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
						totalFullKills =
							(int16_t)(totalFullKills +
									  g_players[player].perMissionKills.killsFullOnFlightGroup[fgIdx]);
						sharedKillCount =
							(int16_t)(sharedKillCount +
									  g_players[player].perMissionKills.killsSharedOnFlightGroup[fgIdx]);
					}

					FlightText_FormatScratchInt(totalFullKills);
					strcat(text, g_flightTextScratchBuffer);
					strcat(text, "(");
					FlightText_FormatScratchInt(sharedKillCount);
					strcat(text, g_flightTextScratchBuffer);
					strcat(text, ")");
					FlightText_SetCursor(0, rowY);
					FlightText_DrawStringRightAligned(text);

					rowY += lineStep;
					rowBottomY += lineStep;
				}
			}
		}

		if (connectedCount == 1) {
			{
				struct SinglePlayerKillScratch {
					uint16_t killsByFlightGroup[192];
					uint16_t killBreakdown[3][192];
				} scratch;

				memset(scratch.killsByFlightGroup, 0, sizeof(scratch.killsByFlightGroup));
				memset(scratch.killsByFlightGroup, 0, sizeof(scratch.killsByFlightGroup));
				memset(scratch.killBreakdown, 0, sizeof(scratch.killBreakdown));
				memset(scratch.killBreakdown, 0, sizeof(scratch.killBreakdown));
				memset(scratch.killBreakdown, 0, sizeof(scratch.killBreakdown));
			}
		}
	} else {
		if (entryCount > 1) {
			int16_t swapped;

			do {
				swapped = 0;
				for (row = 0; row < entryCount - 1; ++row) {
					int team;
					int nextTeam;

					team = order[row];
					nextTeam = order[row + 1];
					if (team < 10 &&
						g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][nextTeam] +
								g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION][nextTeam] >
							g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][team] +
								g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION][team]) {
						order[row] = nextTeam;
						order[row + 1] = team;
						swapped = 1;
						break;
					}
				}
			} while (swapped);
		}

		for (row = 0; row < entryCount; ++row) {
			int team;

			team = order[row];
			if (row >= (int16_t)g_mfdMissionScoreboardFirstVisibleRow && row < lastVisibleExclusive &&
				team < 10) {
				int marker;

				if (playerFgCountByTeam[team] == 1 && ownedFgCountByTeam[team] == 1) {
					int fgIdx;

					for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
						if (g_missionFlightGroups[fgIdx].playerOwnerIdx != -1 &&
							g_missionFlightGroups[fgIdx].fg.team == team) {
							FlightText_SetScratch(
								NetSession_GetPlayerName(g_missionFlightGroups[fgIdx].playerOwnerIdx));
							break;
						}
					}
					if (fgIdx == (int16_t)g_missionHeader.numFlightGroups) {
						FlightText_SetScratch(g_missionTeams[team].name);
					}
				} else {
					FlightText_SetScratch(g_missionTeams[team].name);
				}

				marker = teamMarkers[team];
				if (marker != 0 && (marker <= 3 || team == (uint16_t)g_players[g_localPlayer].playerIff)) {
					FlightText_AppendScratchChar(' ');
					FlightText_AppendScratchChar('(');
					FlightText_AppendScratchChar((char)('0' + teamMarkers[team]));
					FlightText_AppendScratchChar(')');
				}

				FlightText_SetClipRect(0, rowY, paneWidth, (uint16_t)(rowY + lineStep));
				if (!g_useHardware3D) {
					g_flightFillClipRectFn();
				}

				if (team == (uint16_t)g_players[g_localPlayer].playerIff) {
					FlightText_SetColor(0x4Eu);
				} else if (marker == 1) {
					FlightText_SetColor(0x52u);
				} else {
					FlightText_SetColor(0x43u);
				}

				FlightText_SetCursor(0, rowY);
				FlightText_DrawString(g_flightTextScratchBuffer);
				g_flightDrawCharFn('\n');

				sharedKillCount = g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_SHARED][team];
				FlightText_FormatScratchInt(
					g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][team] +
					g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION][team]);
				strcpy(text, g_flightTextScratchBuffer);
				strcat(text, "    ");
				FlightText_FormatScratchInt(
					(int16_t)g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_FULL][team]);
				strcat(text, g_flightTextScratchBuffer);
				strcat(text, "(");
				FlightText_FormatScratchInt(sharedKillCount);
				strcat(text, g_flightTextScratchBuffer);
				strcat(text, ")");
				FlightText_SetCursor(0, rowY);
				FlightText_DrawStringRightAligned(text);

				rowY += lineStep;
			}
		}
	}

	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		uint16_t blitWidth;

		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		blitWidth = (uint16_t)g_hudMfdPaneWidth;
		if (mfdSide == 1) {
			Blit16ToFlightSurface(g_hudMfdLeftTexPixels, g_flightColorEscapeBypassChar, 0, 0, 30u,
								  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight + 15), blitWidth,
								  (uint16_t)g_hudMfdPaneHeight,
								  (uint16_t)(blitWidth * g_flight16bppBytesPerPixel));
		} else {
			Blit16ToFlightSurface(g_hudMfdRightTexPixels, g_flightColorEscapeBypassChar, 0, 0,
								  (uint16_t)(g_screenWidth - g_hudMfdPaneWidth + 25),
								  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight + 15), blitWidth,
								  (uint16_t)g_hudMfdPaneHeight,
								  (uint16_t)(blitWidth * g_flight16bppBytesPerPixel));
		}
	}
}

// FUNCTION: XWA 0x4BFB80
void Mfd_DrawRaceScoreboardPage(int mfdSide, void* mfdSurface) {
	int side;
	int16_t playerSlot;
	uint16_t paneWidth;
	int16_t paneHeight;
	int lineStep;
	int16_t rowCount;
	uint16_t doubleSpaceWidth;
	int16_t scoreColumnX;
	int bodyY;
	int playerOrder[10];
	int16_t connectedCount;
	int16_t playerIdx;
	int16_t row;
	int rowY;
	uint16_t rowBottomY;
	char text[80];

#ifdef XWA_MODERN
	text[0] = '\0';
#endif

	if (g_useHardware3D) {
		side = mfdSide;
		if (side == 1) {
			FlightText_SetRenderOffset((int16_t)g_hudMfdTextInsetX,
									   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		} else {
			int16_t rightX;

			rightX = (int16_t)(g_screenWidth - g_hudMfdTextInsetX);
			rightX = (int16_t)(rightX - g_hudMfdPaneWidth);
			FlightText_SetRenderOffset(rightX, (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		}
	} else {
		FlightSw_SetRenderTarget(mfdSurface, (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
								 (uint16_t)g_hudMfdPaneWidth * g_flight16bppBytesPerPixel);
		side = mfdSide;
	}

	FlightText_SetFontTier(0);

	if (side == 1 && g_mfdLeftNeedsRedraw != 0) {
		g_mfdLeftNeedsRedraw = 0;
	}
	if (side == 2 && g_mfdRightNeedsRedraw != 0) {
		g_mfdRightNeedsRedraw = 0;
	}

	playerSlot = 0;
	paneWidth = 0;
	for (; playerSlot < g_flightPlayerCount; ++playerSlot) {
		if (g_players[playerSlot].connectedFlag != 0) {
			int16_t nameWidth;

			FlightText_SetScratch(NetSession_GetPlayerName(playerSlot));
			nameWidth = (int16_t)FlightText_MeasureStringWidth(g_flightTextScratchBuffer);
			if ((int16_t)paneWidth < nameWidth) {
				paneWidth = (uint16_t)nameWidth;
			}
		}
	}

	{
		int16_t headerNameWidth;

		headerNameWidth = (int16_t)FlightText_MeasureStringWidth(g_strOverlayStrings[9]);
		if ((int16_t)paneWidth < headerNameWidth) {
			paneWidth = (uint16_t)headerNameWidth;
		}
	}

	FlightText_SetScratch(g_strHangarMiscStrings[HANGAR_MISC_RINGS_AND_LAPS_LEFT]);
	doubleSpaceWidth = (uint16_t)FlightText_MeasureStringWidth("  ");
	scoreColumnX = (int16_t)(paneWidth + doubleSpaceWidth);
	paneWidth = (uint16_t)(paneWidth + FlightText_MeasureStringWidth(g_flightTextScratchBuffer));
	lineStep = (uint8_t)g_flightFontLineHeight + 1;
	rowCount = g_activeFlightPlayerCount + 1;
	paneHeight = (uint16_t)(lineStep * rowCount + 2);

	if ((uint16_t)g_mfdRaceScoreboardLastWidth != paneWidth ||
		(int16_t)g_mfdRaceScoreboardLastPlayerCount != g_activeFlightPlayerCount) {
		uint16_t clearWidth;
		uint16_t clearHeight;

		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		clearWidth = paneWidth;
		clearHeight = paneHeight;
		if ((uint16_t)g_mfdRaceScoreboardLastPlayerCount != 0) {
			clearHeight = (uint16_t)(((uint8_t)g_flightFontLineHeight + 1) *
										 ((int16_t)g_mfdRaceScoreboardLastPlayerCount + 1) +
									 2);
		}
		if ((uint16_t)g_mfdRaceScoreboardLastWidth != 0) {
			clearWidth = (uint16_t)g_mfdRaceScoreboardLastWidth;
		}
		FlightText_SetClipRect(0, 0, clearWidth, clearHeight);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}

		lineStep = (uint8_t)g_flightFontLineHeight + 1;
		paneHeight = (uint16_t)(lineStep * rowCount + 2);
	}

	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetColor(0x46u);
	FlightText_SetClipRect(0, 0, paneWidth, paneHeight);
	if (!g_useHardware3D) {
		g_flightFillClipRectFn();
	}

	FlightText_SetCursor(0, 0);
	FlightText_DrawString(g_strOverlayStrings[9]);
	FlightText_SetScratch(g_strHangarMiscStrings[HANGAR_MISC_RINGS_AND_LAPS_LEFT]);
	FlightText_DrawStringRightAligned(g_flightTextScratchBuffer);

	*(uint16_t*)&g_mfdRaceScoreboardLastWidth = paneWidth;
	*(uint16_t*)&g_mfdRaceScoreboardLastPlayerCount = (uint16_t)g_activeFlightPlayerCount;
	*(uint16_t*)&g_mfdRaceScoreboardFirstVisibleRow = 0;
	bodyY = lineStep + 3;
	FlightText_SetClipRect(0, (int16_t)bodyY, paneWidth, paneHeight);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetColor(0x4Eu);
	{
		int lastVisibleExclusive;

		lastVisibleExclusive = paneHeight / (uint16_t)lineStep + g_mfdRaceScoreboardFirstVisibleRow - 1;

		connectedCount = 0;
#ifdef XWA_MODERN
		for (playerIdx = 0; playerIdx < g_flightPlayerCount && playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
#else
		for (playerIdx = 0; playerIdx < g_flightPlayerCount; ++playerIdx) {
#endif
			uint8_t connectedFlag;

			connectedFlag = g_players[playerIdx].connectedFlag;
			if (connectedFlag == 1 || connectedFlag == 2) {
				playerOrder[connectedCount++] = playerIdx;
			}
		}

#ifdef XWA_MODERN
		if (connectedCount > 0) {
			for (playerIdx = connectedCount; playerIdx < g_flightPlayerCount && playerIdx < XWA_PLAYER_COUNT;
				 ++playerIdx) {
				playerOrder[playerIdx] = playerOrder[playerIdx - 1];
			}
		}
#else
		for (playerIdx = connectedCount; playerIdx < g_flightPlayerCount; ++playerIdx) {
			playerOrder[playerIdx] = playerOrder[connectedCount - 1];
		}
#endif

		if (connectedCount > 1) {
			int16_t swapped;

			do {
				swapped = 0;
				for (playerIdx = 0; playerIdx < connectedCount - 1; ++playerIdx) {
					int currentPlayer;
					int nextPlayer;
					int16_t swappedPlayer;

					currentPlayer = playerOrder[playerIdx];
					nextPlayer = playerOrder[playerIdx + 1];
					if (currentPlayer < g_flightPlayerCount &&
						g_yardContext.playerChallengeStates[nextPlayer].score >
							g_yardContext.playerChallengeStates[currentPlayer].score) {
						swappedPlayer = (int16_t)playerOrder[playerIdx];
						playerOrder[playerIdx] = playerOrder[playerIdx + 1];
						playerOrder[playerIdx + 1] = swappedPlayer;
						swapped = 1;
						break;
					}
				}
			} while (swapped);
		}

		rowY = bodyY;
		rowBottomY = (uint16_t)(bodyY + lineStep);
		for (row = 0; row < connectedCount; ++row) {
			if (row >= (int16_t)g_mfdRaceScoreboardFirstVisibleRow && row < lastVisibleExclusive) {
				int player;

				player = playerOrder[row];
				if (player < g_flightPlayerCount) {
					FlightText_SetScratch(NetSession_GetPlayerName(player));
					FlightText_SetClipRect(0, (int16_t)rowY, paneWidth, (uint16_t)rowBottomY);
					if (!g_useHardware3D) {
						g_flightFillClipRectFn();
					}

					if (player == g_localPlayer) {
						FlightText_SetColor(0x4Eu);
					} else {
						FlightText_SetColor(0x43u);
					}
					FlightText_SetCursor(0, rowY);
					FlightText_DrawString(g_flightTextScratchBuffer);
					g_flightDrawCharFn('\n');

					if (g_yardChallengeMode >= 6u) {
						if (g_yardContext.playerChallengeStates[player].carriedObjectPickedUp) {
							FlightText_SetColor(0x4Au);
							FlightText_SetCursor(0, rowY);
							FlightText_DrawStringRightAligned(text);
							FlightText_DrawStringRightAligned(
								g_strHangarMiscStrings[HANGAR_MISC_YARD_HUD_GOT_R2]);
						}
					} else {
						if (g_yardContext.playerChallengeStates[player].finished) {
							uint16_t minutes;
							uint16_t seconds;

							minutes =
								(uint16_t)(g_yardContext.playerChallengeStates[player].finishTimeSeconds /
										   60);
							seconds =
								(uint16_t)(g_yardContext.playerChallengeStates[player].finishTimeSeconds %
										   60);
							FlightText_FormatScratchInt(minutes);
							strcpy(text, g_flightTextScratchBuffer);
							strcat(text, ":");
							if (seconds < 10u) {
								strcat(text, "0");
							}
							FlightText_FormatScratchInt(seconds);
						} else {
							int16_t digits;
							int16_t xOffset;

							digits = (int16_t)FlightText_FormatScratchInt(
								g_yardContext.playerChallengeStates[player].remainingCheckpointCount);
							if (digits < 6) {
								xOffset = (int16_t)(doubleSpaceWidth * (6 - digits));
							} else {
								xOffset = 0;
							}
							FlightText_SetCursor(scoreColumnX + xOffset, rowY);
							strcpy(text, g_flightTextScratchBuffer);
							strcat(text, "          ");
							FlightText_FormatScratchInt(
								(int16_t)g_yardContext.playerChallengeStates[player].lapsRemaining);
						}

						strcat(text, g_flightTextScratchBuffer);
						FlightText_SetCursor(0, rowY);
						FlightText_DrawStringRightAligned(text);
					}

					rowY += lineStep;
					rowBottomY = (uint16_t)(rowBottomY + lineStep);
				}
			}
		}
	}

	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		if (mfdSide == 1) {
			Blit16ToFlightSurface(g_hudMfdLeftTexPixels, g_flightColorEscapeBypassChar, 0, 0, 30u,
								  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight + 15),
								  (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
								  (uint16_t)(g_hudMfdPaneWidth * g_flight16bppBytesPerPixel));
		} else {
			Blit16ToFlightSurface(g_hudMfdRightTexPixels, g_flightColorEscapeBypassChar, 0, 0,
								  (uint16_t)(g_screenWidth - g_hudMfdPaneWidth + 25),
								  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight + 15),
								  (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
								  (uint16_t)(g_hudMfdPaneWidth * g_flight16bppBytesPerPixel));
		}
	}
}

static __inline void Mfd_SetupPaneRenderTarget(int mfdSide, void* mfdSurface) {
	if (g_useHardware3D) {
		if (mfdSide == 1) {
			FlightText_SetRenderOffset((int16_t)g_hudMfdTextInsetX,
									   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		} else {
			FlightText_SetRenderOffset((int16_t)(g_screenWidth - g_hudMfdTextInsetX - g_hudMfdPaneWidth),
									   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		}
	} else {
		FlightSw_SetRenderTarget(mfdSurface, (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
								 (uint16_t)g_hudMfdPaneWidth * g_flight16bppBytesPerPixel);
	}
}

static __inline void Mfd_FinishFriendlyCraftPage(int mfdSide, void* mfdSurface, int useTextInsetLeft) {
	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
		return;
	}

	FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
	if (mfdSide == 1) {
		Blit16ToFlightSurface(mfdSurface, g_flightColorEscapeBypassChar, 0, 0,
							  (uint16_t)(useTextInsetLeft ? g_hudMfdTextInsetX : 5),
							  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight), (uint16_t)g_hudMfdPaneWidth,
							  (uint16_t)g_hudMfdPaneHeight,
							  (uint16_t)g_hudMfdPaneWidth * g_flight16bppBytesPerPixel);
	} else {
		Blit16ToFlightSurface(
			mfdSurface, g_flightColorEscapeBypassChar, 0, 0, (uint16_t)(g_screenWidth - g_hudMfdPaneWidth),
			(uint16_t)(g_screenHeight - g_hudMfdPaneHeight), (uint16_t)g_hudMfdPaneWidth,
			(uint16_t)g_hudMfdPaneHeight, (uint16_t)g_hudMfdPaneWidth * g_flight16bppBytesPerPixel);
	}
}

static __inline void Mfd_DrawFriendlyCraftCenteredMessage(int mfdSide, void* mfdSurface, int stringIndex,
														  unsigned int color, int useTextInsetLeft) {
	int centerY;
	int lineHeight;

	if (!g_useHardware3D) {
		g_flightFillClipRectFn();
	}
	centerY = (uint16_t)g_hudMfdPaneHeight;
	lineHeight = (uint8_t)g_flightFontLineHeight;
	FlightText_SetCursor(0, (centerY >> 1) - lineHeight);
	FlightText_SetColor(color);
	FlightText_DrawStringCentered(g_strMfdStrings[stringIndex]);
	Mfd_FinishFriendlyCraftPage(mfdSide, mfdSurface, useTextInsetLeft);
}

static __inline unsigned int Mfd_FriendlyCraftScaledPercent(uint16_t q16Percent) {
	return (unsigned int)(q16Percent / 655);
}

static __inline int Mfd_FriendlyCraftIsHiddenFromPlayer(CraftData* craft, uint16_t playerIff) {
	return (int8_t)craft->iffVisibility[playerIff] < 0;
}

static __inline void Mfd_FriendlyCraftSetPercentColor(int percent) {
	if (percent <= 20) {
		FlightText_SetColor(0x4au);
	} else if (percent <= 50) {
		FlightText_SetColor(0x4eu);
	} else {
		FlightText_SetColor(0x46u);
	}
}

// FUNCTION: XWA 0x4C0360
void Mfd_DrawFriendlyCraftPage(int mfdSide, void* mfdSurface) {
	enum {
		MFD_FRIENDLY_CRAFT_SCROLL_UP_KEY = 0x00a6,
		MFD_FRIENDLY_CRAFT_SCROLL_DOWN_KEY = 0x00a7,
		MFD_FRIENDLY_CRAFT_MAX_ROWS = 192
	};
	uint16_t playerIff;
	uint32_t rows[MFD_FRIENDLY_CRAFT_MAX_ROWS];
	uint16_t rowCount;
	int rowTotal;
	int lineStep;
	uint16_t bodyBottomY;
	uint16_t paneWidth;
	int paneHeight;
	int16_t needsRedraw;
	int visibleRows;
	uint16_t lastVisibleRow;
	unsigned int shieldPct;
	int i;
	int j;
	int rowIndex;

	needsRedraw = 0;
	rowCount = 0;
	shieldPct = 0;

	Mfd_SetupPaneRenderTarget(mfdSide, mfdSurface);
	FlightText_SetClipRect(0, 0, (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight);
	FlightText_SetCursor(0, 0);
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetClearLineBackground(1);

	playerIff = (uint16_t)g_players[g_localPlayer].playerIff;

	if (mfdSide == 2 && g_mfdFriendlyCraftPageState.mirrorRedrawPending != 0) {
		needsRedraw = 1;
	}
	if (mfdSide == 1) {
		g_mfdFriendlyCraftPageState.currentTextTopY = (uint16_t)g_mfdFriendlyCraftPageState.leftTextTopY;
	} else {
		g_mfdFriendlyCraftPageState.currentTextTopY = (uint16_t)g_mfdFriendlyCraftPageState.rightTextTopY;
	}
	if (mfdSide == 1) {
		g_mfdFriendlyCraftPageState.topRowIndex = (uint16_t)g_mfdFriendlyCraftPageState.leftTopRowIndex;
	} else {
		g_mfdFriendlyCraftPageState.topRowIndex = (uint16_t)g_mfdFriendlyCraftPageState.rightTopRowIndex;
	}

	if (g_players[g_localPlayer].objectIndex != 0xffff) {
		ObjectRecord* playerObj;
		MobileObject* playerMobj;
		CraftData* playerCraft;

		playerObj = &g_objectTable[g_players[g_localPlayer].objectIndex];
		playerMobj = playerObj->mobj;
		if (playerMobj != NULL) {
			playerCraft = playerMobj->pCraft;
			if (playerCraft == NULL) {
				XWA_HUD_OUTPUT_DEBUG_STRING("NULL craft data pointer in DisplayFriendlyCraft()!\n");
				return;
			}
			if (g_players[g_localPlayer].hyperspacePhase != PLAYER_HYPERSPACE_PHASE_NONE) {
				Mfd_DrawFriendlyCraftCenteredMessage(mfdSide, mfdSurface, MFD_STRING_NOT_AVAILABLE, 0x43u, 0);
				return;
			}
			if (playerCraft->systemHealth[4] == 0) {
				g_msgArgTable[0] = 103;
				g_msgArgTable[1] = MSG_DAMAGED;
				msg_emitInFlightMessage(MSG_SYSTEMCOND, g_localPlayer);
				Mfd_SetupPaneRenderTarget(mfdSide, mfdSurface);
				FlightText_SetClipRect(0, 0, (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight);
				FlightText_SetFontTier(0);
				FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
				FlightText_SetShadowEnabled(0);
				Mfd_DrawFriendlyCraftCenteredMessage(mfdSide, mfdSurface, MFD_STRING_TARGETING_DAMAGED, 0x4au,
													 0);
				return;
			}
		} else {
			XWA_HUD_OUTPUT_DEBUG_STRING("NULL mobileobjectptr in DisplayFriendlyCraft()!\n");
			return;
		}
	}

	{
		ObjectRecord* objects;
		uint32_t objIdx;
		uint32_t tableIdx;

		objects = g_objectTable;
		objIdx = g_activeRegionObjectSlotStart;
		if (objIdx < g_activeRegionCraftObjectSlotEnd) {
			tableIdx = objIdx;
			do {
				MobileObject* mobj;
				CraftData* craft;

				if (objects[tableIdx].objectType != OBJ_None) {
					mobj = objects[tableIdx].mobj;
					if (mobj != NULL && mobj->pCraft != NULL && mobj->state == 0 &&
						g_missionFlightGroups[objects[tableIdx].flightGroupIdx].fg.name[0] != '\0' &&
						objIdx != (uint32_t)g_players[g_localPlayer].objectIndex &&
						Object_IsFriendlyToTeam((uint16_t)objIdx, playerIff)) {
						objects = g_objectTable;
						if (objects[tableIdx].genusId != GENUS_SatelliteBuoy &&
							objects[tableIdx].objectType != OBJ_RebelPilot &&
							objects[tableIdx].objectType != OBJ_ImperialPilot &&
							objects[tableIdx].objectType != OBJ_CivilianPilot) {
							craft = objects[tableIdx].mobj->pCraft;
							if (craft->objectKind != 3 && craft->objectKind != 4) {
								rows[rowCount++] =
									((uint32_t)objects[tableIdx].mobj->team << 24) | (uint16_t)objIdx;
							}
						}
					}
				}

				++objIdx;
				++tableIdx;
			} while (objIdx < g_activeRegionCraftObjectSlotEnd);
		}
	}
	rowTotal = rowCount;

	i = 0;
	do {
		j = i + 1;
		if (j < rowTotal) {
			do {
				uint32_t rowI;
				uint32_t rowJ;

				rowI = rows[i];
				rowJ = rows[j];
				if ((rowJ & 0xff000000u) < (rowI & 0xff000000u)) {
					rows[i] = rowJ;
					rows[j] = rowI & 0xffffu;
				}
				++j;
			} while (j < rowTotal);
			j = i + 1;
		}
		i = j;
	} while (j < rowTotal);

	if (rowCount == 0) {
		Mfd_DrawFriendlyCraftCenteredMessage(mfdSide, mfdSurface, MFD_STRING_NO_FRIENDLY_FGS, 0x43u, 0);
		return;
	}

	lineStep = (uint8_t)g_flightFontLineHeight - (int)(g_flightHudScaleFactor * -2.0f);
	paneWidth = (uint16_t)g_hudMfdPaneWidth;
	paneHeight = (uint16_t)g_hudMfdPaneHeight;
	bodyBottomY = paneHeight;
	if (g_useHardware3D) {
		bodyBottomY -= lineStep;
	}

	if ((mfdSide == 1 && g_mfdLeftNeedsRedraw != 0) || (mfdSide == 2 && g_mfdRightNeedsRedraw != 0)) {
		if (mfdSide == 1) {
			g_mfdFriendlyCraftPageState.leftTopRowIndex = 0;
			g_mfdLeftNeedsRedraw = 0;
		}
		if (mfdSide == 2) {
			g_mfdFriendlyCraftPageState.rightTopRowIndex = 0;
			g_mfdRightNeedsRedraw = 0;
		}
		g_mfdFriendlyCraftPageState.topRowIndex = 0;
		needsRedraw = 1;
		if (!g_useHardware3D) {
			FlightText_SetClipRect(0, 0, (uint16_t)paneWidth, (uint16_t)bodyBottomY);
			g_flightFillClipRectFn();
		}
	} else if ((uint16_t)g_players[g_localPlayer].regionIndex !=
			   (uint16_t)g_mfdFriendlyCraftSelectedRowCache) {
		if (mfdSide == 1) {
			g_mfdFriendlyCraftPageState.leftTopRowIndex = 0;
		}
		if (mfdSide == 2) {
			g_mfdFriendlyCraftPageState.rightTopRowIndex = 0;
		}
		g_mfdFriendlyCraftPageState.topRowIndex = 0;
		needsRedraw = 1;
		g_mfdFriendlyCraftSelectedRowCache = (int16_t)g_players[g_localPlayer].regionIndex;
	}

	FlightText_SetColor(0x43u);
	FlightText_SetClipRect(0, 0, (uint16_t)paneWidth, (uint16_t)bodyBottomY);
	FlightText_SetCursor(0, 0);
	FlightText_DrawString(g_strOverlayStrings[13]);
	FlightText_SetCursor((int16_t)g_mfdFriendlyCraftShieldHullHeaderX, 0);
	FlightText_SetScratch(g_strPanelStrings[PANEL_STRING_HUD_S]);
	FlightText_AppendScratchString("/");
	FlightText_AppendScratchString(g_strPanelStrings[PANEL_STRING_HUD_H]);
	FlightText_DrawString(g_flightTextScratchBuffer);
	FlightText_DrawStringRightAligned(g_strOverlayStrings[14]);
	FlightText_SetClipRect(0, (int16_t)lineStep, (uint16_t)paneWidth, (uint16_t)bodyBottomY);

	if ((uint16_t)g_mfdFriendlyCraftPageState.cachedRowCount != (uint16_t)rowCount) {
		if ((uint16_t)g_mfdFriendlyCraftPageState.topRowIndex != 0) {
			if ((uint16_t)rowCount < (uint16_t)g_mfdFriendlyCraftPageState.cachedRowCount) {
				int diff;

				diff = rowCount - (uint16_t)g_mfdFriendlyCraftPageState.cachedRowCount;
				if (mfdSide == 1) {
					g_mfdFriendlyCraftPageState.leftTopRowIndex =
						(uint16_t)(g_mfdFriendlyCraftPageState.leftTopRowIndex + diff);
					g_mfdFriendlyCraftPageState.topRowIndex =
						(uint16_t)g_mfdFriendlyCraftPageState.leftTopRowIndex;
				} else if (mfdSide == 2) {
					g_mfdFriendlyCraftPageState.rightTopRowIndex =
						(uint16_t)(g_mfdFriendlyCraftPageState.rightTopRowIndex + diff);
					g_mfdFriendlyCraftPageState.topRowIndex =
						(uint16_t)g_mfdFriendlyCraftPageState.rightTopRowIndex;
				}
			} else if ((uint16_t)rowCount > (uint16_t)g_mfdFriendlyCraftPageState.cachedRowCount) {
				int diff;

				diff = rowCount - (uint16_t)g_mfdFriendlyCraftPageState.cachedRowCount;
				if (mfdSide == 1) {
					g_mfdFriendlyCraftPageState.leftTopRowIndex =
						(uint16_t)(g_mfdFriendlyCraftPageState.leftTopRowIndex + diff);
					g_mfdFriendlyCraftPageState.topRowIndex =
						(uint16_t)g_mfdFriendlyCraftPageState.leftTopRowIndex;
				} else if (mfdSide == 2) {
					g_mfdFriendlyCraftPageState.rightTopRowIndex =
						(uint16_t)(g_mfdFriendlyCraftPageState.rightTopRowIndex + diff);
					g_mfdFriendlyCraftPageState.topRowIndex =
						(uint16_t)g_mfdFriendlyCraftPageState.rightTopRowIndex;
				}
			}
		}
		if (!g_useHardware3D) {
			FlightText_SetClipRect(0, (int16_t)lineStep, (uint16_t)paneWidth, (uint16_t)bodyBottomY);
			g_flightFillClipRectFn();
		}
		g_mfdFriendlyCraftPageState.cachedRowCount = (uint16_t)rowCount;
		needsRedraw = 1;
	}

	if (mfdSide == g_players[g_localPlayer].mfd.activeIndex) {
		if (g_currentActionKey == MFD_FRIENDLY_CRAFT_SCROLL_UP_KEY) {
			if ((uint16_t)g_mfdFriendlyCraftPageState.topRowIndex != 0) {
				if (mfdSide == 1) {
					--g_mfdFriendlyCraftPageState.leftTopRowIndex;
					g_mfdFriendlyCraftPageState.topRowIndex =
						(uint16_t)g_mfdFriendlyCraftPageState.leftTopRowIndex;
				} else if (mfdSide == 2) {
					--g_mfdFriendlyCraftPageState.rightTopRowIndex;
					g_mfdFriendlyCraftPageState.topRowIndex =
						(uint16_t)g_mfdFriendlyCraftPageState.rightTopRowIndex;
				}
			}
			needsRedraw = 1;
		} else if (g_currentActionKey == MFD_FRIENDLY_CRAFT_SCROLL_DOWN_KEY) {
			if (rowCount > 1 && (uint16_t)g_mfdFriendlyCraftPageState.topRowIndex <
									(uint16_t)(rowTotal - ((bodyBottomY - lineStep) / lineStep))) {
				if (mfdSide == 1) {
					++g_mfdFriendlyCraftPageState.leftTopRowIndex;
					g_mfdFriendlyCraftPageState.topRowIndex =
						(uint16_t)g_mfdFriendlyCraftPageState.leftTopRowIndex;
				} else if (mfdSide == 2) {
					++g_mfdFriendlyCraftPageState.rightTopRowIndex;
					g_mfdFriendlyCraftPageState.topRowIndex =
						(uint16_t)g_mfdFriendlyCraftPageState.rightTopRowIndex;
				}
			}
			needsRedraw = 1;
		}
	}

	if ((int16_t)g_playerFlightTransientTimers[g_localPlayer].field_0C <= 0) {
		g_playerFlightTransientTimers[g_localPlayer].field_0C = 472;
		needsRedraw = 1;
	}

	visibleRows = (bodyBottomY - lineStep) / lineStep;
	lastVisibleRow = g_mfdFriendlyCraftPageState.topRowIndex + visibleRows;
	if ((uint16_t)lastVisibleRow > (uint16_t)rowCount) {
		if ((uint16_t)g_mfdFriendlyCraftPageState.topRowIndex != 0) {
			--g_mfdFriendlyCraftPageState.topRowIndex;
		}
		lastVisibleRow = g_mfdFriendlyCraftPageState.topRowIndex + visibleRows;
	}

	if (needsRedraw || g_useHardware3D) {
		int16_t y;

		y = lineStep;
		FlightText_SetClipRect(0, (int16_t)lineStep, (uint16_t)paneWidth, (uint16_t)bodyBottomY);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}

		for (rowIndex = 0; rowIndex < rowTotal; ++rowIndex) {
			uint32_t row;
			uint16_t objectIdx;
			int objectTableIdx;
			const char* rowColorPtr;
			MobileObject* mobj;
			CraftData* craft;
			int16_t shColumnX;
			unsigned int hullPct;
			int playerOwnerIdx;

			FlightText_SetColor(0x43u);
			if (rowIndex < (uint16_t)g_mfdFriendlyCraftPageState.topRowIndex ||
				rowIndex > (uint16_t)lastVisibleRow) {
				continue;
			}

			row = rows[rowIndex];
			objectIdx = (uint16_t)row;
			rowColorPtr = &g_mfdCraftListStatusLetters[row >> 24];
			hullPct = 0;

			FlightText_SetColor((uint8_t)*rowColorPtr);
			Mfd_BuildScratchCraftListName(objectIdx, g_localPlayer, 1);
			FlightText_SetCursor(0, (int16_t)y);
			FlightText_TruncateStringToWidth(NULL, (unsigned int)g_mfdFriendlyCraftNameColumnWidth);
			FlightText_DrawString(g_flightTextScratchBuffer);

			shColumnX = g_mfdFriendlyCraftNameColumnWidth;
			objectTableIdx = objectIdx;
			mobj = g_objectTable[objectTableIdx].mobj;
			if (mobj != NULL) {
				craft = mobj->pCraft;
#ifdef XWA_MODERN
				if (craft == NULL) {
					y += lineStep;
					continue;
				}
#endif
				if (craft != NULL && craft->objectKind != 3 && craft->objectKind != 4) {
					int avgShield;
					int maxShield;

					avgShield = (craft->shieldRear + craft->shieldFront) / 2;
					maxShield = Craft_GetObjectMaxShield(objectIdx);
					if (avgShield != 0 && maxShield != 0) {
						shieldPct = 2u * Mfd_FriendlyCraftScaledPercent((uint16_t)MATH2_percentage(
											 (uint32_t)avgShield, (uint32_t)maxShield));
					} else {
						shieldPct = 0;
					}
					Mfd_FriendlyCraftSetPercentColor(shieldPct);
					if (!Mfd_FriendlyCraftIsHiddenFromPlayer(craft,
															 (uint16_t)g_players[g_localPlayer].playerIff)) {
						FlightText_SetCursor((int16_t)shColumnX, (int16_t)y);
						FlightText_DrawDecimalNumber((uint16_t)shieldPct, 3u, 1u);
					} else {
						FlightText_SetColor(0x46u);
						FlightText_SetCursor((int16_t)shColumnX, (int16_t)y);
						FlightText_DrawString("???");
					}
					FlightText_SetColor((uint8_t)*rowColorPtr);
					shColumnX += FlightText_MeasureStringWidth("200");
					FlightText_SetCursor((int16_t)shColumnX, (int16_t)y);
					g_flightDrawCharFn('/');
				}

				if (craft->objectKind != 3 && craft->objectKind != 4) {
					if ((uint32_t)craft->hullDamage > (uint32_t)craft->hullMax) {
						hullPct = 1;
					} else {
						hullPct = Mfd_FriendlyCraftScaledPercent((uint16_t)MATH2_percentage(
							(uint32_t)(craft->hullMax - craft->hullDamage), (uint32_t)craft->hullMax));
						if (hullPct == 0) {
							hullPct = 100;
						}
					}
				}

				if (!Mfd_FriendlyCraftIsHiddenFromPlayer(craft,
														 (uint16_t)g_players[g_localPlayer].playerIff)) {
					if (hullPct != 0) {
						FlightText_FormatScratchInt((int)hullPct);
						FlightText_SetCursor((int16_t)(shColumnX + FlightText_MeasureStringWidth("/")),
											 (int16_t)y);
						if (hullPct <= 20u) {
							FlightText_SetColor(0x4au);
						} else if (shieldPct <= 50) {
							FlightText_SetColor(0x4eu);
						} else {
							FlightText_SetColor(0x46u);
						}
						FlightText_DrawString(g_flightTextScratchBuffer);
					}
				} else {
					FlightText_SetCursor((int16_t)(shColumnX + FlightText_MeasureStringWidth("/")),
										 (int16_t)y);
					FlightText_SetColor(0x46u);
					FlightText_DrawString("???");
				}

				FlightText_SetColor((uint8_t)*rowColorPtr);
				playerOwnerIdx = g_objectTable[objectTableIdx].playerOwnerIdx;
				if (playerOwnerIdx != -1) {
					if (!Mfd_FriendlyCraftIsHiddenFromPlayer(craft,
															 (uint16_t)g_players[g_localPlayer].playerIff)) {
						uint16_t targetObjIdx;

						targetObjIdx = (uint16_t)g_players[playerOwnerIdx].currentTargetObjectIdx;
						if (targetObjIdx != 0xffffu && targetObjIdx >= g_activeRegionObjectSlotStart &&
							targetObjIdx < g_activeRegionCraftObjectSlotEnd &&
							g_objectTable[targetObjIdx].mobj != NULL) {
							Hud_AppendObjectDisplayName(targetObjIdx, 7);
						} else {
							FlightText_SetScratch(g_strComponentStrings[32]);
						}
					} else {
						FlightText_SetScratch("?????");
					}
				} else if (!Mfd_FriendlyCraftIsHiddenFromPlayer(
							   craft, (uint16_t)g_players[g_localPlayer].playerIff)) {
					AiController* ai;
					uint16_t targetObjIdx;

					ai = pai_GetEffectiveAIController(craft);
					targetObjIdx = ai->targetObjIdx;
					if (targetObjIdx != 255u && targetObjIdx != 0xffffu && targetObjIdx < 0x8000u) {
						Hud_AppendObjectDisplayName(targetObjIdx, 7);
					} else {
						FlightText_SetScratch(g_strComponentStrings[32]);
					}
				} else {
					FlightText_SetScratch("?????");
				}

				FlightText_TruncateStringToWidth(NULL, (unsigned int)g_mfdFriendlyCraftTargetColumnWidth);
				FlightText_DrawStringRightAligned(g_flightTextScratchBuffer);
			}
			y += lineStep;
		}

		if (mfdSide == 1) {
			g_mfdFriendlyCraftPageState.leftTextTopY = (uint16_t)y;
		} else {
			g_mfdFriendlyCraftPageState.rightTextTopY = (uint16_t)y;
		}
	}

	if (mfdSide == 1 && g_players[g_localPlayer].mfd.page[1] == g_players[g_localPlayer].mfd.page[2]) {
		g_mfdFriendlyCraftPageState.mirrorRedrawPending = 1;
	}
	FlightText_SetClearLineBackground(0);
	Mfd_FinishFriendlyCraftPage(mfdSide, mfdSurface, 1);
}

// FUNCTION: XWA 0x4C1400
void Mfd_DrawFlightGroupsPage(int mfdSide, void* mfdSurface) {
	enum {
		MFD_FLIGHT_GROUPS_SCROLL_UP_KEY = 0x00a6,
		MFD_FLIGHT_GROUPS_SCROLL_DOWN_KEY = 0x00a7,
		MFD_FLIGHT_GROUPS_MAX_ROWS = 192
	};
	CraftData* craft;
	uint16_t playerIff;
	uint32_t rows[MFD_FLIGHT_GROUPS_MAX_ROWS];
	uint16_t rowCount;
	int rowTotal;
	uint16_t lineStep;
	uint16_t bodyBottomY;
	uint16_t paneWidth;
	int16_t needsRedraw;
	uint16_t lastVisibleRow;
	int shieldPct;

	needsRedraw = 0;
	craft = NULL;
	shieldPct = 0;

	Mfd_SetupPaneRenderTarget(mfdSide, mfdSurface);
	FlightText_SetClipRect(0, 0, (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight);
	FlightText_SetCursor(0, 0);
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetClearLineBackground(1);
	playerIff = (uint16_t)g_players[g_localPlayer].playerIff;

	if (mfdSide == 2 && g_mfdFlightGroupsPageState.mirrorRedrawPending != 0) {
		needsRedraw = 1;
	}
	if (mfdSide != 1) {
		g_mfdFlightGroupsPageState.currentTextTopY = (uint16_t)g_mfdFlightGroupsPageState.rightTextTopY;
	} else {
		g_mfdFlightGroupsPageState.currentTextTopY = (uint16_t)g_mfdFlightGroupsPageState.leftTextTopY;
	}
	if (mfdSide != 1) {
		g_mfdFlightGroupsPageState.topRowIndex = (uint16_t)g_mfdFlightGroupsPageState.rightTopRowIndex;
	} else {
		g_mfdFlightGroupsPageState.topRowIndex = (uint16_t)g_mfdFlightGroupsPageState.leftTopRowIndex;
	}

	if (g_players[g_localPlayer].objectIndex != 0xffff) {
		MobileObject* playerMobj;

		playerMobj = g_objectTable[g_players[g_localPlayer].objectIndex].mobj;
		if (playerMobj == NULL) {
			return;
		}
		craft = playerMobj->pCraft;
		if (craft == NULL) {
			XWA_HUD_OUTPUT_DEBUG_STRING("NULL craft data pointer in DisplayFlightGroups()!\n");
			return;
		}
		if (g_players[g_localPlayer].hyperspacePhase != PLAYER_HYPERSPACE_PHASE_NONE) {
			Mfd_DrawFriendlyCraftCenteredMessage(mfdSide, mfdSurface, MFD_STRING_NOT_AVAILABLE, 0x43u, 0);
			return;
		}
		if (craft->systemHealth[4] == 0) {
			g_msgArgTable[0] = 103;
			g_msgArgTable[1] = MSG_DAMAGED;
			msg_emitInFlightMessage(MSG_SYSTEMCOND, g_localPlayer);
			Mfd_SetupPaneRenderTarget(mfdSide, mfdSurface);
			FlightText_SetClipRect(0, 0, (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight);
			FlightText_SetFontTier(0);
			FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
			FlightText_SetShadowEnabled(0);
			Mfd_DrawFriendlyCraftCenteredMessage(mfdSide, mfdSurface, MFD_STRING_TARGETING_DAMAGED, 0x4au, 0);
			return;
		}
	}

	rowCount = 0;
	{
		uint32_t objIdx;
		uint32_t tableIdx;

		for (objIdx = g_activeRegionObjectSlotStart, tableIdx = objIdx;
			 objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx, ++tableIdx) {
			MobileObject* mobj;
			CraftData* rowCraft;
			uint16_t objectTeam;

			if (g_objectTable[tableIdx].objectType == OBJ_None) {
				continue;
			}
			mobj = g_objectTable[tableIdx].mobj;
			if (mobj == NULL || mobj->pCraft == NULL || mobj->state != 0) {
				continue;
			}
			if (g_missionFlightGroups[g_objectTable[tableIdx].flightGroupIdx].fg.name[0] == '\0') {
				continue;
			}
			objectTeam = mobj->team;
			if (objectTeam == playerIff) {
				continue;
			}
			if (Team_IsHostileToTeam(objectTeam, playerIff) != 1) {
				continue;
			}
			if (g_objectTable[tableIdx].genusId == GENUS_SatelliteBuoy ||
				g_objectTable[tableIdx].objectType == OBJ_RebelPilot ||
				g_objectTable[tableIdx].objectType == OBJ_ImperialPilot ||
				g_objectTable[tableIdx].objectType == OBJ_CivilianPilot) {
				continue;
			}
			rowCraft = g_objectTable[tableIdx].mobj->pCraft;
			if (rowCraft->objectKind == 3 || rowCraft->objectKind == 4) {
				continue;
			}
			rows[rowCount++] = ((uint32_t)objectTeam << 24) | (uint16_t)objIdx;
		}
	}

	rowTotal = rowCount;
	{
		int i;
		int j;

		i = 0;
		do {
			j = i + 1;
			if (j < rowTotal) {
				do {
					uint32_t rowI;
					uint32_t rowJ;

					rowI = rows[i];
					rowJ = rows[j];
					if ((rowJ & 0xff000000u) < (rowI & 0xff000000u)) {
						rows[i] = rowJ;
						rows[j] = rowI & 0xffffu;
					}
					++j;
				} while (j < rowTotal);
				j = i + 1;
			}
			i = j;
		} while (j < rowTotal);
	}

	if (rowCount == 0) {
		Mfd_DrawFriendlyCraftCenteredMessage(mfdSide, mfdSurface, MFD_STRING_NO_ENEMY_FGS, 0x43u, 0);
		return;
	}

	lineStep = (uint8_t)g_flightFontLineHeight - (int)(g_flightHudScaleFactor * -2.0f);
	paneWidth = (uint16_t)g_hudMfdPaneWidth;
	bodyBottomY = (uint16_t)g_hudMfdPaneHeight;
	if (g_useHardware3D) {
		bodyBottomY -= lineStep;
	}

	if ((mfdSide == 1 && g_mfdLeftNeedsRedraw != 0) || (mfdSide == 2 && g_mfdRightNeedsRedraw != 0)) {
		if (mfdSide == 1) {
			g_mfdFlightGroupsPageState.leftTopRowIndex = 0;
			g_mfdLeftNeedsRedraw = 0;
		}
		if (mfdSide == 2) {
			g_mfdFlightGroupsPageState.rightTopRowIndex = 0;
			g_mfdRightNeedsRedraw = 0;
		}
		g_mfdFlightGroupsPageState.topRowIndex = 0;
		needsRedraw = 1;
		if (!g_useHardware3D) {
			FlightText_SetClipRect(0, 0, (uint16_t)paneWidth, (uint16_t)bodyBottomY);
			g_flightFillClipRectFn();
		}
	} else if ((uint16_t)g_players[g_localPlayer].regionIndex !=
			   (uint16_t)g_mfdFlightGroupsSelectedRowCache) {
		if (mfdSide == 1) {
			g_mfdFlightGroupsPageState.leftTopRowIndex = 0;
		}
		if (mfdSide == 2) {
			g_mfdFlightGroupsPageState.rightTopRowIndex = 0;
		}
		g_mfdFlightGroupsPageState.topRowIndex = 0;
		needsRedraw = 1;
		g_mfdFlightGroupsSelectedRowCache = (int16_t)g_players[g_localPlayer].regionIndex;
	}

	FlightText_SetColor(0x43u);
	FlightText_SetClipRect(0, 0, (uint16_t)paneWidth, (uint16_t)bodyBottomY);
	FlightText_SetCursor(0, 0);
	FlightText_DrawString(g_strOverlayStrings[13]);
	FlightText_SetCursor((int16_t)g_mfdFlightGroupsShieldHullHeaderX, 0);
	FlightText_SetScratch(g_strPanelStrings[PANEL_STRING_HUD_S]);
	FlightText_AppendScratchString("/");
	FlightText_AppendScratchString(g_strPanelStrings[PANEL_STRING_HUD_H]);
	FlightText_DrawString(g_flightTextScratchBuffer);
	FlightText_SetCursor((int16_t)g_mfdFlightGroupsTargetColumnX, 0);
	FlightText_DrawString(g_strOverlayStrings[14]);
	FlightText_SetCursor((int16_t)g_mfdFlightGroupsStatusColumnX, 0);
	FlightText_DrawString(g_strOverlayStrings[15]);
	FlightText_SetClipRect(0, (int16_t)lineStep, (uint16_t)paneWidth, (uint16_t)bodyBottomY);

	if ((uint16_t)g_mfdFlightGroupsPageState.cachedRowCount != (uint16_t)rowCount) {
		if ((uint16_t)g_mfdFlightGroupsPageState.topRowIndex != 0) {
			if ((uint16_t)rowCount < (uint16_t)g_mfdFlightGroupsPageState.cachedRowCount) {
				int diff;

				diff = rowCount - (uint16_t)g_mfdFlightGroupsPageState.cachedRowCount;
				if (mfdSide == 1) {
					g_mfdFlightGroupsPageState.leftTopRowIndex =
						(uint16_t)(g_mfdFlightGroupsPageState.leftTopRowIndex + diff);
					g_mfdFlightGroupsPageState.topRowIndex =
						(uint16_t)g_mfdFlightGroupsPageState.leftTopRowIndex;
				} else if (mfdSide == 2) {
					g_mfdFlightGroupsPageState.rightTopRowIndex =
						(uint16_t)(g_mfdFlightGroupsPageState.rightTopRowIndex + diff);
					g_mfdFlightGroupsPageState.topRowIndex =
						(uint16_t)g_mfdFlightGroupsPageState.leftTopRowIndex;
				}
			} else if ((uint16_t)rowCount > (uint16_t)g_mfdFlightGroupsPageState.cachedRowCount) {
				int diff;

				diff = rowCount - (uint16_t)g_mfdFlightGroupsPageState.cachedRowCount;
				if (mfdSide == 1) {
					g_mfdFlightGroupsPageState.leftTopRowIndex =
						(uint16_t)(g_mfdFlightGroupsPageState.leftTopRowIndex + diff);
					g_mfdFlightGroupsPageState.topRowIndex =
						(uint16_t)g_mfdFlightGroupsPageState.leftTopRowIndex;
				} else if (mfdSide == 2) {
					g_mfdFlightGroupsPageState.rightTopRowIndex =
						(uint16_t)(g_mfdFlightGroupsPageState.rightTopRowIndex + diff);
					g_mfdFlightGroupsPageState.topRowIndex =
						(uint16_t)g_mfdFlightGroupsPageState.leftTopRowIndex;
				}
			}
		}
		if (!g_useHardware3D) {
			FlightText_SetClipRect(0, (int16_t)lineStep, (uint16_t)paneWidth, (uint16_t)bodyBottomY);
			g_flightFillClipRectFn();
		}
		g_mfdFlightGroupsPageState.cachedRowCount = (uint16_t)rowCount;
		needsRedraw = 1;
	}

	if (mfdSide == g_players[g_localPlayer].mfd.activeIndex) {
		unsigned int actionKey;

		actionKey = (uint16_t)g_currentActionKey - MFD_FLIGHT_GROUPS_SCROLL_UP_KEY;
		if (actionKey == 0) {
			if ((uint16_t)g_mfdFlightGroupsPageState.topRowIndex != 0) {
				if (mfdSide == 1) {
					--g_mfdFlightGroupsPageState.leftTopRowIndex;
					g_mfdFlightGroupsPageState.topRowIndex =
						(uint16_t)g_mfdFlightGroupsPageState.leftTopRowIndex;
				} else if (mfdSide == 2) {
					--g_mfdFlightGroupsPageState.rightTopRowIndex;
					g_mfdFlightGroupsPageState.topRowIndex =
						(uint16_t)g_mfdFlightGroupsPageState.rightTopRowIndex;
				}
			}
			needsRedraw = 1;
		} else if (actionKey == MFD_FLIGHT_GROUPS_SCROLL_DOWN_KEY - MFD_FLIGHT_GROUPS_SCROLL_UP_KEY) {
			if (rowTotal > 1 && (uint16_t)g_mfdFlightGroupsPageState.topRowIndex <
									(uint16_t)(rowTotal - ((bodyBottomY - lineStep) / lineStep))) {
				if (mfdSide == 1) {
					++g_mfdFlightGroupsPageState.leftTopRowIndex;
					g_mfdFlightGroupsPageState.topRowIndex =
						(uint16_t)g_mfdFlightGroupsPageState.leftTopRowIndex;
				} else if (mfdSide == 2) {
					++g_mfdFlightGroupsPageState.rightTopRowIndex;
					g_mfdFlightGroupsPageState.topRowIndex =
						(uint16_t)g_mfdFlightGroupsPageState.rightTopRowIndex;
				}
			}
			needsRedraw = 1;
		}
	}

	if ((int16_t)g_playerFlightTransientTimers[g_localPlayer].field_0C <= 0) {
		g_playerFlightTransientTimers[g_localPlayer].field_0C = 472;
		needsRedraw = 1;
	}

	{
		lastVisibleRow =
			(uint16_t)g_mfdFlightGroupsPageState.topRowIndex + ((bodyBottomY - lineStep) / lineStep);
		if (lastVisibleRow > rowCount) {
			if ((uint16_t)g_mfdFlightGroupsPageState.topRowIndex != 0) {
				--g_mfdFlightGroupsPageState.topRowIndex;
			}
			lastVisibleRow =
				(uint16_t)g_mfdFlightGroupsPageState.topRowIndex + ((bodyBottomY - lineStep) / lineStep);
		}
	}

	if (needsRedraw || g_useHardware3D) {
		int16_t y;

		y = lineStep;
		FlightText_SetClipRect(0, (int16_t)lineStep, (uint16_t)paneWidth, (uint16_t)bodyBottomY);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}

		{
			int rowIndex;

			for (rowIndex = 0; rowIndex < rowTotal; ++rowIndex) {
				uint32_t row;
				uint16_t objectIdx;
				const char* rowColor;
				MobileObject* mobj;
				unsigned int hullPct;

				FlightText_SetColor(0x43u);
				if (rowIndex < (uint16_t)g_mfdFlightGroupsPageState.topRowIndex ||
					rowIndex > (uint16_t)lastVisibleRow) {
					continue;
				}

				row = rows[rowIndex];
				objectIdx = (uint16_t)row;
				rowColor = &g_mfdCraftListStatusLetters[row >> 24];

				FlightText_SetColor((uint8_t)*rowColor);
				Mfd_BuildScratchCraftListName(objectIdx, g_localPlayer, 1);
				FlightText_SetCursor(0, (int16_t)y);
				FlightText_TruncateStringToWidth(NULL, (unsigned int)g_mfdFlightGroupsNameColumnWidth);
				FlightText_DrawString(g_flightTextScratchBuffer);

				mobj = g_objectTable[objectIdx].mobj;
				hullPct = 0;

				if (mobj != NULL) {
					craft = mobj->pCraft;
#ifdef XWA_MODERN
					if (craft == NULL) {
						y += lineStep;
						continue;
					}
#endif

					FlightText_SetCursor((int16_t)g_mfdFlightGroupsShieldHullValueX, (int16_t)y);
					if (craft->objectKind != 3 && craft->objectKind != 4) {
						int avgShield;
						int maxShield;

						avgShield = (craft->shieldRear + craft->shieldFront) / 2;
						maxShield = Craft_GetObjectMaxShield(objectIdx);
						if (avgShield != 0 && maxShield != 0) {
							shieldPct = 2u * Mfd_FriendlyCraftScaledPercent((uint16_t)MATH2_percentage(
												 (uint32_t)avgShield, (uint32_t)maxShield));
						} else {
							shieldPct = 0;
						}
						Mfd_FriendlyCraftSetPercentColor(shieldPct);
						if ((int8_t)craft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] < 0) {
							FlightText_SetColor(0x46u);
							FlightText_DrawString("???");
						} else {
							FlightText_DrawDecimalNumber((uint16_t)shieldPct, 3u, 1u);
						}
					}

					FlightText_SetColor((uint8_t)*rowColor);
					g_flightDrawCharFn('/');
					FlightText_SetCursor(
						(int16_t)(g_mfdFlightGroupsShieldHullValueX + FlightText_MeasureStringWidth("200/")),
						(int16_t)y);
					if (craft->objectKind != 3 && craft->objectKind != 4) {
						if ((uint32_t)craft->hullDamage <= (uint32_t)craft->hullMax) {
							hullPct = Mfd_FriendlyCraftScaledPercent((uint16_t)MATH2_percentage(
								(uint32_t)(craft->hullMax - craft->hullDamage), (uint32_t)craft->hullMax));
							if (hullPct == 0) {
								hullPct = 100;
							}
						} else {
							hullPct = 1;
						}
					}

					if ((int8_t)craft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] < 0) {
						FlightText_SetColor(0x46u);
						FlightText_DrawString("???");
					} else if (hullPct != 0) {
						FlightText_FormatScratchInt((int)hullPct);
						if (hullPct <= 20u) {
							FlightText_SetColor(0x4au);
						} else if (shieldPct <= 50u) {
							FlightText_SetColor(0x4eu);
						} else {
							FlightText_SetColor(0x46u);
						}
						FlightText_DrawString(g_flightTextScratchBuffer);
					}

					FlightText_SetColor((uint8_t)*rowColor);
					FlightText_SetCursor((int16_t)g_mfdFlightGroupsTargetColumnX, (int16_t)y);
					if (g_objectTable[objectIdx].playerOwnerIdx == -1) {
						if ((int8_t)craft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] < 0) {
							FlightText_SetScratch("?????");
						} else {
							AiController* ai;
							uint16_t targetObjIdx;

							ai = pai_GetEffectiveAIController(craft);
							targetObjIdx = ai->targetObjIdx;
							if (targetObjIdx != 255u && targetObjIdx != 0xffffu && targetObjIdx < 0x8000u) {
								Hud_AppendObjectDisplayName(targetObjIdx, 7);
							} else {
								FlightText_SetScratch(g_strComponentStrings[32]);
							}
						}
					} else if ((int8_t)craft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] <
							   0) {
						FlightText_SetScratch("?????");
					} else {
						uint16_t targetObjIdx;

						targetObjIdx = (uint16_t)g_players[g_objectTable[objectIdx].playerOwnerIdx]
										   .currentTargetObjectIdx;
						if (targetObjIdx != 0xffffu && targetObjIdx >= g_activeRegionObjectSlotStart &&
							targetObjIdx < g_activeRegionCraftObjectSlotEnd &&
							g_objectTable[targetObjIdx].mobj != NULL) {
							Hud_AppendObjectDisplayName(targetObjIdx, 7);
						} else {
							FlightText_SetScratch(g_strComponentStrings[32]);
						}
					}
					FlightText_TruncateStringToWidth(NULL, (unsigned int)g_mfdFlightGroupsTargetColumnWidth);
					FlightText_DrawString(g_flightTextScratchBuffer);
				}

				FlightText_SetCursor((int16_t)g_mfdFlightGroupsStatusColumnX, (int16_t)y);
				{
					int inspectUnknown;
					int destroy;
					int disable;
					int inspect;
					int board;
					int capture;
					int specialGoal;
					unsigned int flightGroupIdx;
					int statusStringIdx;
					CraftData* statusCraft;
					const PlayerData* statusPlayer;

					statusStringIdx = 0;
					statusCraft = craft;
					statusPlayer = &g_players[g_localPlayer];
					if (objectIdx < (uint16_t)g_projectileObjectSlotStart ||
						objectIdx >= (uint16_t)g_projectileObjectSlotEnd) {
						if (objectIdx < (uint16_t)g_activeRegionObjectSlotStart ||
							objectIdx >= (uint16_t)g_activeRegionCraftObjectSlotEnd) {
							statusCraft = NULL;
						} else {
							statusCraft = g_objectTable[objectIdx].mobj->pCraft;
						}

						flightGroupIdx = g_objectTable[objectIdx].flightGroupIdx;
						if (flightGroupIdx <= (uint16_t)(int16_t)g_missionHeader.numFlightGroups) {
							inspectUnknown = 0;
							destroy = 0;
							disable = 0;
							inspect = 0;
							board = 0;
							capture = 0;
							specialGoal = 0;

							{
								uint16_t goalIdx;

								for (goalIdx = 0; goalIdx < 8u; ++goalIdx) {
									const XwaFlightGroupGoalPayload* goal;

									goal = &g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload;
									if (goal->enabledForTeam[(uint16_t)statusPlayer->playerIff] != 0 &&
										goal->argument == 0 &&
										g_missionFgStats[flightGroupIdx]
												.goalState[8u * (uint16_t)statusPlayer->playerIff +
														   goalIdx] == 4) {
										if (goal->amount == 6) {
											if (g_missionFgStats[flightGroupIdx].specialCargoOutcome[8] ==
												0) {
												inspectUnknown = 1;
											}
											specialGoal = 1;
										}

										if (goal->condition == 2) {
											destroy = 1;
										} else if (goal->condition == 3) {
											disable = 1;
										} else if (goal->condition == 8) {
											inspect = 1;
										} else if (goal->condition == 5) {
											inspectUnknown = 1;
										} else if (goal->condition == 4) {
											board = 1;
										} else if (goal->condition == 6) {
											capture = 1;
										}
									}
								}
							}

							{
								uint16_t pairIdx;

								for (pairIdx = 0; pairIdx < 2u; ++pairIdx) {
									uint16_t triggerIdx;

									for (triggerIdx = 0; triggerIdx < 2u; ++triggerIdx) {
										const XwaTrigger* trigger;

										trigger = &g_missionGlobalGoals[(uint16_t)statusPlayer->playerIff][0]
													   .triggerPairs[pairIdx]
													   .triggers[triggerIdx];
										if (trigger->condition != 10 &&
											Mission_FlightGroupMatchesTriggerVariable(
												flightGroupIdx,
												(MissionTriggerVariableType)trigger->variableType,
												trigger->variable)) {
											if (trigger->condition == 2) {
												destroy = 1;
											} else if (trigger->condition == 3) {
												disable = 1;
											} else if (trigger->condition == 8) {
												inspect = 1;
											} else if (trigger->condition == 5) {
												inspectUnknown = 1;
											} else if (trigger->condition == 4) {
												board = 1;
											} else if (trigger->condition == 6) {
												capture = 1;
											}
										}
									}
								}
							}

							if (objectIdx >= (uint16_t)g_activeRegionObjectSlotStart &&
								objectIdx < (uint16_t)g_activeRegionCraftObjectSlotEnd) {
								int activeInspect;

								if (inspectUnknown &&
									(int8_t)statusCraft->iffVisibility[(uint16_t)statusPlayer->playerIff] >
										0) {
									inspectUnknown = 0;
								}

								if (board || capture) {
									activeInspect = 1;
									if (g_objectTable[objectIdx].mobj->speed == 0) {
										activeInspect = inspect;
									}
									if (specialGoal &&
										g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft !=
											statusCraft->waveNumber) {
										board = 0;
										capture = 0;
									}
								} else {
									activeInspect = inspect;
								}

								if (activeInspect && specialGoal &&
									g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft !=
										statusCraft->waveNumber) {
									activeInspect = 0;
								}
								if (destroy && specialGoal &&
									g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft !=
										statusCraft->waveNumber) {
									destroy = 0;
								}
								inspect = activeInspect;
							}

							if (inspectUnknown) {
								statusStringIdx = 16;
							} else if (inspect) {
								if (board) {
									statusStringIdx = 20;
								} else {
									statusStringIdx = capture ? 21 : 18;
								}
							} else if (board) {
								statusStringIdx = 20;
							} else if (capture) {
								statusStringIdx = 21;
							} else if (destroy) {
								statusStringIdx = 17;
							} else if (disable) {
								statusStringIdx = 19;
							}
						}
					}

					if ((int8_t)craft->iffVisibility[(uint16_t)g_players[g_localPlayer].playerIff] < 0) {
						FlightText_DrawString("??????");
					} else if (statusStringIdx != 0) {
						FlightText_SetScratch(g_strOverlayStrings[statusStringIdx]);
						FlightText_TruncateStringToWidth(NULL,
														 (unsigned int)g_mfdFlightGroupsStatusColumnWidth);
						FlightText_DrawString(g_flightTextScratchBuffer);
					} else {
						FlightText_DrawString(g_strComponentStrings[32]);
					}
				}

				y += lineStep;
			}
		}

		if (mfdSide == 1) {
			g_mfdFlightGroupsPageState.leftTextTopY = (uint16_t)y;
		} else {
			g_mfdFlightGroupsPageState.rightTextTopY = (uint16_t)y;
		}
	}

	if (mfdSide == 1 && g_players[g_localPlayer].mfd.page[1] == g_players[g_localPlayer].mfd.page[2]) {
		g_mfdFlightGroupsPageState.mirrorRedrawPending = 1;
	}
	FlightText_SetClearLineBackground(0);
	Mfd_FinishFriendlyCraftPage(mfdSide, mfdSurface, 1);
}

// FUNCTION: XWA 0x4C28C0
void Mfd_BuildScratchCraftListName(uint16_t objectIdx, int viewerPlayerIdx, char includeIffColor) {
	int objectIndex;
	MobileObject* mobj;
	uint16_t craftNumber;
	CraftData* craft;
	uint16_t flightGroupIdx;
	uint16_t groupNumber;
	int8_t iff;
	char numberSeparator;

	g_flightTextScratchBuffer[0] = '\0';

	objectIndex = objectIdx;
	mobj = g_objectTable[objectIndex].mobj;
	if (mobj != NULL && mobj->state == 0) {

		craft = mobj->pCraft;
		flightGroupIdx = g_objectTable[objectIndex].flightGroupIdx;
		if (includeIffColor) {
			FlightText_AppendScratchChar((char)0xfe);
			iff = g_objectTable[objectIndex].mobj->iff;
			if (iff == 0) {
				FlightText_AppendScratchChar('R');
			} else if (iff == 1 || iff == 4) {
				FlightText_AppendScratchChar('J');
			} else if (iff == 2) {
				FlightText_AppendScratchChar('F');
			} else if (iff == 5) {
				FlightText_AppendScratchChar('V');
			} else {
				FlightText_AppendScratchChar('N');
			}
		}

		if ((int8_t)craft->iffVisibility[(uint16_t)g_players[viewerPlayerIdx].playerIff] >= 0) {
			FlightText_AppendScratchString(g_missionFlightGroups[flightGroupIdx].fg.name);
			craftNumber = (uint16_t)Hud_MissionFG_GetCraftNumberIfShown(flightGroupIdx, craft);
			numberSeparator = ' ';
		} else {
			FlightText_AppendScratchString(g_strPanelStrings[PANEL_STRING_NAME_G]);
			groupNumber = (uint16_t)(flightGroupIdx + 1);
			if (groupNumber >= 10) {
				int tensDigit;
				int onesDigit;

				tensDigit = groupNumber / 10;
				onesDigit = groupNumber % 10;
				FlightText_AppendScratchChar((char)('0' + tensDigit));
				FlightText_AppendScratchChar((char)('0' + onesDigit));
			} else {
				FlightText_AppendScratchChar((char)('0' + groupNumber));
			}
			numberSeparator = '-';
			craftNumber = (uint16_t)(craft->waveNumber + 1);
		}

		if (craftNumber != 0) {
			FlightText_AppendScratchChar(numberSeparator);
			if (craftNumber >= 10) {
				int tensDigit;
				int onesDigit;

				tensDigit = craftNumber / 10;
				onesDigit = craftNumber % 10;
				FlightText_AppendScratchChar((char)('0' + tensDigit));
				FlightText_AppendScratchChar((char)('0' + onesDigit));
			} else {
				FlightText_AppendScratchChar((char)('0' + craftNumber));
			}
		}
	}
}

// FUNCTION: XWA 0x4C2A90
void Mfd_DrawMapLegendPage(int mfdSide, void* mfdSurface) {
	uint16_t paneWidth;
	uint16_t paneHeight;
	int shouldRedraw;
	int lineStep;
	int y;
	int row;

	shouldRedraw = 0;
	paneWidth = (uint16_t)g_hudMfdPaneWidth;
	paneHeight = (uint16_t)g_hudMfdPaneHeight;

	if (g_useHardware3D) {
		if (mfdSide == 1) {
			FlightText_SetRenderOffset((int16_t)g_hudMfdTextInsetX,
									   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		} else {
			FlightText_SetRenderOffset((int16_t)(g_screenWidth - g_hudMfdTextInsetX - g_hudMfdPaneWidth),
									   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		}
	} else {
		FlightSw_SetRenderTarget(mfdSurface, paneWidth, paneHeight, paneWidth * g_flight16bppBytesPerPixel);
	}

	FlightText_SetFontTier(0);
	lineStep = (uint8_t)g_flightFontLineHeight + 2;
	FlightText_SetColor(0x43u);
	FlightText_SetFontTier(0);
	FlightText_SetClipRect(0, 0, paneWidth, paneHeight);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetClearLineBackground(1u);

	if (mfdSide == 2 && g_unusedMfdMapLegendRightRedrawFlag != 0) {
		shouldRedraw = 1;
	}
	if ((mfdSide == 1 && g_mfdLeftNeedsRedraw != 0) || (mfdSide == 2 && g_mfdRightNeedsRedraw != 0)) {
		if (mfdSide == 1) {
			g_mfdLeftNeedsRedraw = 0;
			shouldRedraw = 1;
			if (!g_useHardware3D) {
				g_flightFillClipRectFn();
			}
		}
		if (mfdSide == 2) {
			g_mfdRightNeedsRedraw = 0;
			shouldRedraw = 1;
			if (!g_useHardware3D) {
				g_flightFillClipRectFn();
			}
		}
	}

	if (shouldRedraw || g_useHardware3D) {
		FlightText_SetWordWrap(0);
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}

		y = 10;
		for (row = 0; row < 7; ++row) {
			uint16_t rightColumnX;

			FlightText_SetCursor(0, (int16_t)y);
			FlightText_SetScratch(g_strMapStrings[row]);
			FlightText_DrawString(g_strMapStrings[row]);
			rightColumnX = (uint16_t)(FlightText_MeasureStringWidth(g_strMapStrings[6]) + 5u);
			FlightText_SetCursor((int16_t)rightColumnX, (int16_t)y);
			FlightText_DrawString(g_strMapStrings[row + 7]);
			y += lineStep;
		}
	}

	g_unusedMfdMapLegendRightRedrawFlag = 0;
	FlightText_SetClearLineBackground(0);
	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		uint16_t srcPitchBytes;
		uint16_t dstY;

		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		srcPitchBytes = (uint16_t)(g_flight16bppBytesPerPixel * g_hudMfdPaneWidth);
		dstY = (uint16_t)(g_screenHeight - g_hudMfdPaneHeight);
		if (mfdSide == 1) {
			Blit16ToFlightSurface(mfdSurface, g_flightColorEscapeBypassChar, 0, 0, 5u, dstY, paneWidth,
								  paneHeight, srcPitchBytes);
		} else {
			Blit16ToFlightSurface(mfdSurface, g_flightColorEscapeBypassChar, 0, 0,
								  (uint16_t)(g_screenWidth - g_hudMfdPaneWidth), dstY, paneWidth, paneHeight,
								  srcPitchBytes);
		}
	}
}

// FUNCTION: XWA 0x4C2D70
void Mfd_DrawFilmLeftStatusPage(void) {
	char useElapsedTime;
	int16_t inset;
	uint16_t paneWidth;
	uint16_t paneHeight;
	int16_t lineStep;
	int16_t cursorY;
	uint16_t labelWidth;
	int rowY;
	char** commandText;

	useElapsedTime = 0;
	inset = (int16_t)(int)(g_flightHudScaleFactor * 10.0f);
	paneWidth = (uint16_t)g_hudMfdPaneWidth;
	paneHeight = (uint16_t)g_hudMfdPaneHeight;
	HUD_PANE_PUSH(XWA_HUD_PANE_MFD_LEFT_BODY, 0, g_screenHeight - g_hudMfdPaneHeight, paneWidth, paneHeight);

	if (g_useHardware3D) {
		FlightText_SetRenderOffset((int16_t)g_hudMfdTextInsetX,
								   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
	} else {
		FlightSw_SetRenderTarget(g_hudMfdLeftTexPixels, paneWidth, paneHeight,
								 paneWidth * g_flight16bppBytesPerPixel);
	}

	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetFontTier(0);
	FlightText_SetColor(0x43u);
	FlightText_SetWordWrap(0);
	FlightText_SetShadowEnabled(0);
	lineStep = (uint8_t)g_flightFontLineHeight - (uint16_t)(int)(g_flightHudScaleFactor * -3.0f);

	if (!g_useHardware3D) {
		FlightText_SetClipRect(0, 0, paneWidth, paneHeight);
		g_flightFillClipRectFn();
	}

	FlightText_SetClipRect(inset, inset, paneWidth, paneHeight);
	cursorY = (int16_t)(int)(g_flightHudScaleFactor * 10.0f);
	FlightText_SetCursor(inset, cursorY);
	FlightText_DrawString(g_strPanelStrings[PANEL_STRING_HUD_FILM_MISSION_TIME]);
	labelWidth = FlightText_MeasureStringWidth(g_strPanelStrings[PANEL_STRING_HUD_FILM_MISSION_TIME]);
	FlightText_SetCursor((int16_t)(inset + labelWidth), cursorY);

	if (g_provingGroundsModeActive && !g_yardContext.playerChallengeStates[g_localPlayer].finished) {
		useElapsedTime = 1;
	}
	if (!g_missionTimeLimitActive || useElapsedTime) {
		FlightText_DrawDecimalNumber(g_missionElapsedClock.minutes, 2u, 1u);
	} else {
		FlightText_DrawDecimalNumber(g_missionCountdownClock.minutes, 2u, 2u);
	}
	g_flightDrawCharFn(':');
	if (!g_missionTimeLimitActive || useElapsedTime) {
		FlightText_DrawDecimalNumber(g_missionElapsedClock.seconds, 2u, 2u);
	} else {
		FlightText_DrawDecimalNumber(g_missionCountdownClock.seconds, 2u, 2u);
	}

	if (g_pauseState == 3) {
		uint16_t timeWidth;

		timeWidth = FlightText_MeasureStringWidth(" 00:00 ");
		FlightText_SetCursor((int16_t)(inset + labelWidth + timeWidth), cursorY);
		FlightText_SetColor(0x4Au);
		FlightText_DrawString(g_strPanelStrings[PANEL_STRING_HUD_FILM_FF]);
	}

	rowY = (int)(g_flightHudScaleFactor * 30.0f);
	commandText = g_strFilmCommands;
	do {
		uint16_t keyWidth;

		FlightText_SetCursor(inset, (int16_t)rowY);
		FlightText_SetColor(0x4Au);
		FlightText_DrawString(commandText[0]);
		FlightText_SetColor(0x43u);
		keyWidth = FlightText_MeasureStringWidth("X");
		FlightText_SetCursor((int16_t)(inset - (int)(g_flightHudScaleFactor * -30.0f) + keyWidth),
							 (int16_t)rowY);
		FlightText_DrawString(commandText[1]);
		commandText += 2;
		rowY = (int16_t)(rowY + lineStep);
	} while (commandText < &g_strFilmCommands[10]);

	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		Blit16ToFlightSurface(g_hudMfdLeftTexPixels, g_flightColorEscapeBypassChar, 0, 0, 5u,
							  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight), (uint16_t)g_hudMfdPaneWidth,
							  (uint16_t)g_hudMfdPaneHeight, g_flight16bppBytesPerPixel * g_hudMfdPaneWidth);
	}
	HUD_PANE_POP();
}

// FUNCTION: XWA 0x4C30D0
void Mfd_DrawFilmRightOptionsPage(void) {
	char redraw;
	int16_t inset;
	uint16_t paneWidth;
	uint16_t paneHeight;
	int lineStep;
	uint16_t cursorY;
	int rowY;
	int optionIndex;

	redraw = 0;
	inset = (int16_t)(g_flightHudScaleFactor * 10.0f);
	paneWidth = (uint16_t)g_hudMfdPaneWidth;
	paneHeight = (uint16_t)g_hudMfdPaneHeight;
	HUD_PANE_PUSH(XWA_HUD_PANE_MFD_RIGHT_BODY, g_screenWidth - g_hudMfdPaneWidth,
				  g_screenHeight - g_hudMfdPaneHeight, paneWidth, paneHeight);

	if (g_useHardware3D) {
		FlightText_SetRenderOffset((int16_t)(g_screenWidth - g_hudMfdTextInsetX - g_hudMfdPaneWidth),
								   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
	} else {
		FlightSw_SetRenderTarget(g_hudMfdRightTexPixels, (uint16_t)g_hudMfdPaneWidth,
								 (uint16_t)g_hudMfdPaneHeight,
								 g_hudMfdPaneWidth * g_flight16bppBytesPerPixel);
	}

	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetFontTier(0);
	FlightText_SetColor(0x43u);
	FlightText_SetWordWrap(0);
	FlightText_SetShadowEnabled(0);
	lineStep = (uint16_t)g_flightFontLineHeight - (int)(g_flightHudScaleFactor * -3.0f);

	if (g_hudElementEnabled[11].enabled) {
		g_hudElementEnabled[11].enabled = 0;
		redraw = 1;
	}

	if (redraw || g_useHardware3D) {
		if (!g_useHardware3D) {
			FlightText_SetClipRect(0, 0, paneWidth, paneHeight);
			g_flightFillClipRectFn();
		}

		FlightText_SetClipRect(inset, inset, paneWidth, paneHeight);

		if (!g_inHangarReady && !g_players[g_localPlayer].mapCameraState &&
			g_players[g_localPlayer].hyperspacePhase == PLAYER_HYPERSPACE_PHASE_NONE) {
			if (g_filmOverlayActive == 1) {
				cursorY = (uint16_t)(int)(g_flightHudScaleFactor * 10.0f);

				if (g_filmOverlayViewState.cameraFocusObjIdx != 0xffff) {
					FlightText_SetCursor(inset, cursorY);
					FlightText_DrawString(g_strPanelStrings[PANEL_STRING_HUD_FILM_FOLLOWING]);
					Hud_AppendObjectDisplayName((uint16_t)g_filmOverlayViewState.cameraFocusObjIdx, 3);
					FlightText_DrawString(g_flightTextScratchBuffer);
					cursorY += (uint8_t)g_flightFontLineHeight;
				}

				if (g_filmOverlayViewState.aimTargetIdx != 0xffff) {
					FlightText_SetColor(0x43u);
					FlightText_SetCursor(inset, cursorY);
					FlightText_DrawString(g_strPanelStrings[PANEL_STRING_HUD_FILM_TRACKING]);
					Hud_AppendObjectDisplayName((uint16_t)g_filmOverlayViewState.aimTargetIdx, 3);
					FlightText_DrawString(g_flightTextScratchBuffer);
				}
			}

			rowY = (int)(g_flightHudScaleFactor * 40.0f);
			optionIndex = 0;
			do {
				FlightText_SetCursor(inset, (uint16_t)rowY);
				FlightText_SetColor(0x4Au);
				if (optionIndex == 0) {
					if (!g_players[g_localPlayer].mapCameraState &&
						g_players[g_localPlayer].hyperspacePhase == PLAYER_HYPERSPACE_PHASE_NONE &&
						!g_inHangarReady) {
						FlightText_DrawString(g_strFilmOptions[0]);
					}
				} else {
					if ((g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR || g_provingGroundsModeActive) &&
						g_filmOverlayViewState.cameraFocusObjIdx != 0xffff && optionIndex > 1 &&
						optionIndex < 8) {
						FlightText_SetColor(0x41u);
					}
					FlightText_DrawString(g_strFilmOptions[optionIndex]);
				}

				FlightText_SetColor(0x43u);
				FlightText_SetCursor(inset - (int)(g_flightHudScaleFactor * -15.0f) +
										 FlightText_MeasureStringWidth("X"),
									 (uint16_t)rowY);

				++optionIndex;
				if (optionIndex == 1) {
					if (!g_players[g_localPlayer].mapCameraState &&
						g_players[g_localPlayer].hyperspacePhase == PLAYER_HYPERSPACE_PHASE_NONE &&
						!g_inHangarReady) {
						FlightText_DrawString(g_strFilmOptions[1]);
					}
				} else {
					if ((g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR || g_provingGroundsModeActive) &&
						g_filmOverlayViewState.cameraFocusObjIdx != 0xffff && optionIndex > 1 &&
						optionIndex < 8) {
						FlightText_SetColor(0x41u);
					}
					FlightText_DrawString(g_strFilmOptions[optionIndex]);
				}

				if (!g_filmOverlayActive ||
					(g_filmOverlayViewState.cameraFocusObjIdx == 0xffff && optionIndex == 3)) {
					break;
				}

				rowY += lineStep;
				++optionIndex;
			} while (optionIndex < 14);

			if (g_useHardware3D) {
				FlightText_SetRenderOffset(0, 0);
				HUD_PANE_POP();
				return;
			}
		} else {
			FlightText_SetCursor(0, ((uint16_t)g_hudMfdPaneHeight >> 1) - (uint8_t)g_flightFontLineHeight);
			FlightText_DrawStringCentered(g_strMfdStrings[MFD_STRING_NOT_AVAILABLE]);
			HUD_PANE_POP();
			return;
		}
	}

	{
		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		{
			uint16_t srcPitchBytes;

			srcPitchBytes = g_hudMfdPaneWidth;
			srcPitchBytes *= g_flight16bppBytesPerPixel;
			Blit16ToFlightSurface(g_hudMfdRightTexPixels, g_flightColorEscapeBypassChar, 0, 0,
								  (uint16_t)(g_screenWidth - g_hudMfdPaneWidth),
								  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight),
								  (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight, srcPitchBytes);
		}
	}
	HUD_PANE_POP();
}

// FUNCTION: XWA 0x4C3550
void Mfd_DrawCommandMenuPage(int mfdSide, void* mfdSurface) {
	int altViewObjectIdx;
	int menuRow;
	int menuItemBase;
	int headerLineY;
	int numberValue;
	char isMainMenu;
	unsigned int itemCount;
	int targetSlot;
	int cursorY;
	char* primaryLabel;
	unsigned int drawIndex;

	menuItemBase = 0;
	isMainMenu = 0;
	headerLineY = 0;
	numberValue = 0;

	if (g_players[g_localPlayer].mapCameraState) {
		altViewObjectIdx = g_players[g_localPlayer].altViewObjectIdx;
	} else {
		altViewObjectIdx = g_players[g_localPlayer].objectIndex;
	}

	if (g_useHardware3D) {
		if (mfdSide == 1) {
			FlightText_SetRenderOffset((int16_t)g_hudMfdTextInsetX,
									   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		} else {
			FlightText_SetRenderOffset((int16_t)(g_screenWidth - g_hudMfdTextInsetX - g_hudMfdPaneWidth),
									   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		}
	} else {
		FlightSw_SetRenderTarget(mfdSurface, (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
								 (uint16_t)(g_hudMfdPaneWidth * g_flight16bppBytesPerPixel));
	}

	if (g_players[g_localPlayer].hyperspacePhase != PLAYER_HYPERSPACE_PHASE_NONE) {
		if (!g_useHardware3D) {
			g_flightFillClipRectFn();
		}
		FlightText_SetClipRect(0, 0, (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight);
		FlightText_SetFontTier(0);
		FlightText_SetColor(0x43u);
		FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
		cursorY = (uint16_t)g_hudMfdPaneHeight;
		cursorY >>= 1;
		cursorY -= g_flightFontLineHeight;
		FlightText_SetCursor(0, (int16_t)cursorY);
		FlightText_DrawStringCentered(g_strMfdStrings[MFD_STRING_NOT_AVAILABLE]);
		if (g_useHardware3D) {
			FlightText_SetRenderOffset(0, 0);
		} else {
			FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
			if (mfdSide == 1) {
				Blit16ToFlightSurface(mfdSurface, g_flightColorEscapeBypassChar, 0, 0, 5u,
									  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight),
									  (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
									  (uint16_t)(g_flight16bppBytesPerPixel * g_hudMfdPaneWidth));
			} else {
				Blit16ToFlightSurface(mfdSurface, g_flightColorEscapeBypassChar, 0, 0,
									  (uint16_t)(g_screenWidth - g_hudMfdPaneWidth),
									  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight),
									  (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
									  (uint16_t)(g_flight16bppBytesPerPixel * g_hudMfdPaneWidth));
			}
		}
		return;
	}

	menuRow = g_players[g_localPlayer].mfd.menuRow;
	if (menuRow == 0) {
		isMainMenu = 1;
		numberValue = 1;
		itemCount = g_players[g_localPlayer].mfdCommandMenuItemCount[0];
	} else {
		if (menuRow <= 8) {
			itemCount = (unsigned int)g_players[g_localPlayer].mfdCommandMenuItemCount[menuRow] + 1u;
		} else {
			itemCount = (unsigned int)g_mfdCommandSubMenuItemCount[menuRow / 10] + 1u;
		}
	}

	for (primaryLabel = &g_mfdCommandPrimaryTargetLabels[1][0], targetSlot = 1;
		 primaryLabel <= &g_mfdCommandPrimaryTargetLabels[6][0]; primaryLabel += 30, ++targetSlot) {
		if (g_players[g_localPlayer].mfd.commandMenu.primaryTargetObjIdx[targetSlot] != 0xffff) {
			Mfd_BuildScratchCraftListName(
				(uint16_t)g_players[g_localPlayer].mfd.commandMenu.primaryTargetObjIdx[targetSlot],
				g_localPlayer, 0);
			strcpy(primaryLabel, g_flightTextScratchBuffer);
		}
	}

	FlightText_SetClipRect(0, 0, (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight);
	FlightText_SetCursor(0, 0);
	FlightText_SetFontTier(0);
	FlightText_SetColor(0x43u);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	if (!g_useHardware3D) {
		g_flightFillClipRectFn();
	}

	if (menuRow <= 8) {
		const char* title;

		if (menuRow == 0) {
			title = g_strFlightCmdMainMenu[0];
		} else {
			if (menuRow <= 6) {
				title = g_mfdCommandPrimaryTargetLabels[menuRow];
			} else {
				title = g_strFlightCmdMainMenu[menuRow];
			}
		}

		FlightText_DrawStringCentered(title);
		if (menuRow != 0 && menuRow <= 6) {
			unsigned int objectIdx;

			objectIdx = (unsigned int)g_players[g_localPlayer].mfd.commandMenu.primaryTargetObjIdx[menuRow];
			if ((int)objectIdx == altViewObjectIdx) {
				headerLineY = g_flightFontLineHeight;
				FlightText_SetCursor(0, (int16_t)g_flightFontLineHeight);
				FlightText_DrawStringCentered(g_strMfdStrings[MFD_STRING_PLAYER_LABEL]);
			} else {
				headerLineY = g_flightFontLineHeight;
				Mfd_DrawCommandObjectOrderLine(objectIdx, 0, (int16_t)g_flightFontLineHeight);
			}
		}
	} else {
		if (g_players[g_localPlayer].mfd.menuRow <= 40) {
			if (g_players[g_localPlayer].mfd.commandMenu.selectedTargetSlot == 7) {
				FlightText_SetScratch(g_strFlightCmdMainMenu[7]);
				FlightText_AppendScratchString(" : ");
				FlightText_AppendScratchString(g_strFlightCmdSubMenu[(menuRow / 10) - 1]);
				FlightText_DrawStringCentered(g_flightTextScratchBuffer);
			} else {
				FlightText_SetScratch(
					g_mfdCommandPrimaryTargetLabels[g_players[g_localPlayer]
														.mfd.commandMenu.selectedTargetSlot]);
				FlightText_AppendScratchString(" : ");
				FlightText_AppendScratchString(g_strFlightCmdSubMenu[(menuRow / 10) - 1]);
				FlightText_DrawStringCentered(g_flightTextScratchBuffer);
				headerLineY = g_flightFontLineHeight;
				Mfd_DrawCommandObjectOrderLine(
					(unsigned int)g_players[g_localPlayer]
						.mfd.commandMenu
						.primaryTargetObjIdx[g_players[g_localPlayer].mfd.commandMenu.selectedTargetSlot],
					0, (int16_t)g_flightFontLineHeight);
			}
		} else {
			FlightText_SetScratch(g_mfdCommandSecondaryTargetLabels[g_players[g_localPlayer]
																		.mfd.commandMenu.selectedTargetSlot]);
			FlightText_DrawStringCentered(g_flightTextScratchBuffer);
			headerLineY = g_flightFontLineHeight;
			Mfd_DrawCommandObjectOrderLine(
				(unsigned int)g_players[g_localPlayer]
					.mfd.commandMenu
					.secondaryTargetObjIdx[g_players[g_localPlayer].mfd.commandMenu.selectedTargetSlot],
				0, (int16_t)g_flightFontLineHeight);
		}
	}

	if (isMainMenu) {
		menuItemBase = 1;
	}

	if (menuRow == 8) {
		unsigned int targetSlot;

		menuItemBase = g_players[g_localPlayer].mfdCommandMenuItemCount[8] - 1;
		for (targetSlot = 0; targetSlot < g_players[g_localPlayer].mfd.commandMenu.secondaryTargetCount;
			 ++targetSlot) {
			Hud_AppendObjectDisplayName(
				(uint16_t)g_players[g_localPlayer].mfd.commandMenu.secondaryTargetObjIdx[targetSlot], 3);
			strcpy(g_mfdCommandSecondaryTargetLabels[targetSlot], g_flightTextScratchBuffer);
		}
	}

	if (menuRow > 10) {
		menuItemBase = g_mfdCommandSubMenuFirstItemIndex[(menuRow - 10) / 10];
	}

	cursorY = headerLineY + 2 * g_flightFontLineHeight;
	primaryLabel = &g_mfdCommandPrimaryTargetLabels[menuItemBase][0];
	for (drawIndex = 0; drawIndex < itemCount; ++drawIndex) {
		if (drawIndex == g_players[g_localPlayer].mfd.menuItem) {
			FlightText_SetColor(0x46u);
		} else if (menuRow == 0) {
			if (drawIndex < 6u) {
				if (g_players[g_localPlayer].mfd.commandMenu.targetSlotValid[drawIndex + 1u] == 1) {
					FlightText_SetColor(g_mfdCommandNodeSwitchColorChar);
				} else {
					FlightText_SetColor(0x41u);
				}
			} else {
				if (drawIndex == 6u && g_players[g_localPlayer].mfd.commandMenu.commandableTargetCount < 2u) {
					FlightText_SetColor(0x41u);
				} else {
					FlightText_SetColor(g_mfdCommandNodeSwitchColorChar);
				}
				if (drawIndex == 7u) {
					FlightText_SetColor(0x41u);
				}
				if (drawIndex == 8u) {
					if (g_players[g_localPlayer].mfd.reinforcementCommandAvailable) {
						FlightText_SetColor(g_mfdCommandNodeSwitchColorChar);
					} else {
						FlightText_SetColor(0x41u);
					}
				}
			}
		} else if (menuRow < 7 &&
				   g_players[g_localPlayer].mfd.commandMenu.primaryTargetObjIdx[menuRow] ==
					   altViewObjectIdx &&
				   numberValue != 0 &&
				   !Mfd_IsCommandMenuItemAvailable((uint16_t)g_localPlayer, (uint16_t)menuRow,
												   (int16_t)(menuItemBase + 1))) {
			FlightText_SetColor(0x41u);
		} else {
			FlightText_SetColor(g_mfdCommandNodeSwitchColorChar);
		}

		{
			int numberX;

			numberX = g_mfdCommandMenuNumberX;
			FlightText_SetCursor((int16_t)numberX, (int16_t)cursorY);
			FlightText_DrawDecimalNumber((uint16_t)numberValue++, 1u, 1u);
			numberX += FlightText_MeasureStringWidth("0 ");
			FlightText_SetCursor((int16_t)numberX, (int16_t)cursorY);
			FlightText_DrawString("--");
			FlightText_SetCursor((int16_t)g_mfdCommandMenuTextX, (int16_t)cursorY);
		}

		if (numberValue == 1) {
			FlightText_DrawString(g_strMfdStrings[MFD_STRING_BACK_TO_MAIN]);
		} else if (isMainMenu) {
			const char* text;
			int drawnMenuItem;

			if (primaryLabel <= &g_mfdCommandPrimaryTargetLabels[6][0]) {
				text = primaryLabel;
			} else {
				text = g_strFlightCmdMainMenu[menuItemBase];
			}
			++menuItemBase;
			primaryLabel += 30;
			FlightText_DrawString(text);
			drawnMenuItem = menuItemBase - 1;
			if (drawnMenuItem <= 6 &&
				g_players[g_localPlayer].mfd.commandMenu.primaryTargetObjIdx[drawnMenuItem] ==
					altViewObjectIdx) {
				FlightText_DrawString(g_strMfdStrings[MFD_STRING_PLAYER_LABEL]);
			}
		} else if (menuRow <= 0 || menuRow > 7) {
			const char* text;

			if (menuRow == 8) {
				if (drawIndex != 0) {
					text = g_mfdCommandSecondaryTargetLabels[drawIndex - 1u];
				} else {
					text = g_strFlightCmdMenuItems[0];
				}
				FlightText_DrawString(text);
			} else {
				text = g_strFlightCmdSubMenuItems[menuItemBase];
				++menuItemBase;
				primaryLabel += 30;
				FlightText_DrawString(text);
			}
		} else {
			const char* text;

			if (menuRow < 7 &&
				g_players[g_localPlayer].mfd.commandMenu.primaryTargetObjIdx[menuRow] == altViewObjectIdx) {
				text = g_strFlightCmdMenuItems[menuItemBase + 1];
			} else {
				text = g_strFlightCmdSubMenu[menuItemBase];
			}
			++menuItemBase;
			primaryLabel += 30;
			FlightText_DrawString(text);
		}

		cursorY += g_flightFontLineHeight + g_mfdCommandMenuLineExtraSpacingY;
	}

	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
	} else {
		FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
		if (mfdSide == 1) {
			Blit16ToFlightSurface(mfdSurface, g_flightColorEscapeBypassChar, 0, 0, 5u,
								  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight),
								  (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
								  (uint16_t)(g_flight16bppBytesPerPixel * g_hudMfdPaneWidth));
		} else {
			Blit16ToFlightSurface(mfdSurface, g_flightColorEscapeBypassChar, 0, 0,
								  (uint16_t)(g_screenWidth - g_hudMfdPaneWidth),
								  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight),
								  (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
								  (uint16_t)(g_flight16bppBytesPerPixel * g_hudMfdPaneWidth));
		}
	}
}

// FUNCTION: XWA 0x4C3EF0
char Mfd_IsCommandMenuItemAvailable(uint16_t playerIdx, uint16_t menuRow, int16_t menuItem) {
	int viewedObjectIdx;
	uint16_t invalidObjectIdx;

	invalidObjectIdx = 0xffffu;

	viewedObjectIdx = g_players[playerIdx].mapCameraState ? g_players[playerIdx].altViewObjectIdx
														  : g_players[playerIdx].objectIndex;

	if (viewedObjectIdx == invalidObjectIdx) {
		return 0;
	}

	if (menuRow == 0) {
		++menuItem;
		switch ((uint16_t)menuItem) {
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
				if (g_players[playerIdx].mfd.commandMenu.targetSlotValid[(uint16_t)menuItem] != 0) {
					return 1;
				}
				return 0;
			case 7:
				if (g_players[playerIdx].mfd.commandMenu.commandableTargetCount != 0) {
					return 1;
				}
				return 0;
			case 8:
				return 0;
			case 9:
				if (g_players[playerIdx].mfd.reinforcementCommandAvailable != 0) {
					return 1;
				}
				return 0;
			default:
				return 1;
		}
	}

	if (menuRow >= 6u ||
		g_players[playerIdx].mfd.commandMenu.primaryTargetObjIdx[menuRow] != viewedObjectIdx) {
		return 1;
	}

	{
		ObjectRecord* object;
		CraftData* craft;
		int16_t modelIndex;
		uint16_t objectType;

		object = &g_objectTable[viewedObjectIdx];
		craft = NULL;
		if (object->mobj != NULL) {
			craft = object->mobj->pCraft;
		}

		if (craft == NULL) {
			XWA_HUD_OUTPUT_DEBUG_STRING("NULL Craft data pointer in CheckPlayerMenuItem()!\n");
			return 0;
		}

		objectType = object->objectType;
		if (objectType == OBJ_None) {
			return 0;
		}

		modelIndex = GetModelIndexFromType(objectType);
		if ((uint16_t)modelIndex == invalidObjectIdx) {
			return 0;
		}

		switch ((uint16_t)menuItem) {
			case 1:
			case 2:
				if (g_players[playerIdx].currentTargetObjectIdx != invalidObjectIdx &&
					craft->carriedObjectIndex == invalidObjectIdx) {
					return 1;
				}
				return 0;
			case 3:
				if (craft->carriedObjectIndex != invalidObjectIdx) {
					return 1;
				}
				return 0;
			case 4:
				if (g_modelDefs[(uint16_t)modelIndex].turretModelIndex[0] == 0) {
					return 0;
				}
				if (g_modelDefs[(uint16_t)modelIndex].turretModelIndex[1] == 0) {
					return 0;
				}
				break;
			case 5:
				if (g_modelDefs[(uint16_t)modelIndex].turretModelIndex[0] == 0 ||
					g_modelDefs[(uint16_t)modelIndex].turretModelIndex[1] == 0) {
					return 0;
				}
				break;
			case 6:
				return craft != NULL;
			default:
				break;
		}

		return 1;
	}
}

// FUNCTION: XWA 0x4C40B0
void Mfd_InitCommandMenuRuntimeState(void) {
	int localObjectIdx;
	uint8_t nodeSwitchIndex;
	unsigned playerIdx;
	int16_t numFlightGroups;
	char (*label)[30];
	char** menuText;

	memset(g_mfdCommandPrimaryTargetLabels, 0, sizeof(g_mfdCommandPrimaryTargetLabels));
	memset(g_mfdCommandSecondaryTargetLabels, 0, sizeof(g_mfdCommandSecondaryTargetLabels));

	label = &g_mfdCommandPrimaryTargetLabels[1];
	for (menuText = &g_strFlightCmdMainMenu[1]; menuText <= &g_strFlightCmdMainMenu[6]; ++menuText) {
		strcpy(*label, *menuText);
		++label;
	}

	localObjectIdx = g_players[g_localPlayer].objectIndex;
	nodeSwitchIndex = g_objectTable[localObjectIdx].mobj->nodeSwitchIndex;
	if (nodeSwitchIndex == 0) {
		g_mfdCommandNodeSwitchColorChar = 'J';
	} else if (nodeSwitchIndex == 1) {
		g_mfdCommandNodeSwitchColorChar = 'M';
	} else if (nodeSwitchIndex == 2) {
		g_mfdCommandNodeSwitchColorChar = 'F';
	} else if (nodeSwitchIndex == 3) {
		g_mfdCommandNodeSwitchColorChar = 'R';
	} else {
		g_mfdCommandNodeSwitchColorChar = 'J';
	}

	for (playerIdx = 0; playerIdx < g_activeFlightPlayerCount; ++playerIdx) {
		int16_t fgIdx;

		for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
			XwaFlightGroup* flightGroup;

			flightGroup = &g_missionFlightGroups[fgIdx].fg;
			if ((flightGroup->arrival[0].triggers[0].condition == 20 &&
				 flightGroup->arrival[0].triggers[0].variable == g_players[playerIdx].playerIff) ||
				(flightGroup->arrival[0].triggers[1].condition == 20 &&
				 flightGroup->arrival[0].triggers[1].variable == g_players[playerIdx].playerIff)) {
				g_players[playerIdx].mfd.reinforcementCommandAvailable = 1;
			}
			if ((flightGroup->arrival[1].triggers[0].condition == 20 &&
				 flightGroup->arrival[1].triggers[0].variable == g_players[playerIdx].playerIff) ||
				(flightGroup->arrival[1].triggers[1].condition == 20 &&
				 flightGroup->arrival[1].triggers[1].variable == g_players[playerIdx].playerIff)) {
				g_players[playerIdx].mfd.reinforcementCommandAvailable = 1;
			}
		}
	}
}

// FUNCTION: XWA 0x4C4200
void Mfd_UpdateCommandMenuTargets(void) {
	unsigned int playerIdx;

	for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
		PlayerData* player;
		MfdCommandMenuState* commandMenu;
		int viewedObjectIdx;
		uint32_t objIdx;

		player = &g_players[playerIdx];
		commandMenu = &player->mfd.commandMenu;

		for (objIdx = 0; objIdx < 7; ++objIdx) {
			commandMenu->primaryTargetObjIdx[objIdx] = 0xffff;
		}
		memset(commandMenu->primaryTargetSignature, 0, sizeof(commandMenu->primaryTargetSignature));
		memset(commandMenu->targetSlotValid, 0, sizeof(commandMenu->targetSlotValid));
		commandMenu->commandableTargetCount = 0;
		commandMenu->secondaryTargetCount = 0;
		memset(&player->mfdCommandMenuItemCount[1], 4, 6);

		if (player->connectedFlag != 1) {
			continue;
		}

		Mission_SetActiveRegionObjectRanges(player->regionIndex);
		viewedObjectIdx = player->mapCameraState ? player->altViewObjectIdx : player->objectIndex;
		if (viewedObjectIdx == 0xffff) {
			continue;
		}

		for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
			uint8_t objGlobalUnit;
			CraftData* craft;
			unsigned int targetSlot;

			if (g_objectTable[objIdx].objectType == OBJ_None) {
				continue;
			}

			objGlobalUnit = g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.globalUnit;
			if (objGlobalUnit == 0) {
				continue;
			}

			if (g_objectTable[objIdx].playerOwnerIdx != -1 && (int)objIdx != viewedObjectIdx) {
				continue;
			}

			if (objGlobalUnit != g_missionFlightGroups[player->boundFlightGroupIdx].fg.globalUnit ||
				g_missionFlightGroups[player->boundFlightGroupIdx].fg.globalUnit <= 0u) {
				continue;
			}

			Player_CanRadioCommandCraft((uint16_t)objIdx, playerIdx);

			if (g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.numberOfWaves != 0 ||
				(craft = g_objectTable[objIdx].mobj->pCraft, targetSlot = craft->craftIndexInGroup,
				 targetSlot > 6)) {
				craft = g_objectTable[objIdx].mobj->pCraft;
				targetSlot = craft->craftIndexInGroup;
				if (targetSlot >
					g_missionGlobalUnitCraftCount[g_missionFlightGroups[player->boundFlightGroupIdx]
													  .fg.globalUnit]) {
					continue;
				}
			}

			if (targetSlot == 0) {
				targetSlot = 1;
			}

			if (craft->objectKind != 0) {
				continue;
			}

			if (g_missionFlightGroups[g_objectTable[viewedObjectIdx].flightGroupIdx].fg.numberOfWaves != 0) {
				XwaFlightGroup* playerFg;

				playerFg = &g_missionFlightGroups[player->boundFlightGroupIdx].fg;
				if (g_missionGlobalUnitCraftCount[g_missionFlightGroups[player->boundFlightGroupIdx]
													  .fg.globalUnit] /
						(unsigned int)playerFg->numberOfCraft >
					1u) {
					targetSlot +=
						playerFg->numberOfCraft -
						g_missionGlobalUnitCraftCount[g_missionFlightGroups[player->boundFlightGroupIdx]
														  .fg.globalUnit];
				}
			}

			if ((unsigned int)commandMenu->targetSlotValid[targetSlot] != 0xffffu) {
				while (g_players[playerIdx].mfd.commandMenu.primaryTargetObjIdx[targetSlot] != 0xffff) {
					if (targetSlot >= 6) {
						break;
					}
					++targetSlot;
				}
			}

			if (g_players[playerIdx].mfd.commandMenu.primaryTargetObjIdx[targetSlot] == 0xffff) {
				commandMenu->targetSlotValid[targetSlot] = 1;
				g_players[playerIdx].mfd.commandMenu.primaryTargetObjIdx[targetSlot] = (int)objIdx;
				commandMenu->primaryTargetSignature[targetSlot] = g_objectTable[objIdx].objectSignature;
				if ((int)objIdx == viewedObjectIdx) {
					player->mfdCommandMenuItemCount[targetSlot] = 6;
				}

				++commandMenu->commandableTargetCount;
				if (commandMenu->commandableTargetCount == 6) {
					break;
				}
			}
		}

		g_players[playerIdx].mfdCommandMenuItemCount[8] =
			g_players[playerIdx].mfd.commandMenu.secondaryTargetCount;

		if (player->mfd.menuRow == 0) {
			int nextSlot;

			nextSlot = (uint8_t)g_players[playerIdx].mfd.menuItem;
			if (nextSlot + 1 <= 6 && !g_players[playerIdx].mfd.commandMenu.targetSlotValid[nextSlot + 1]) {
				do {
					g_players[playerIdx].mfd.menuItem = (uint8_t)(g_players[playerIdx].mfd.menuItem + 1u);
					nextSlot = (uint8_t)g_players[playerIdx].mfd.menuItem;
				} while (nextSlot + 1 <= 6 &&
						 !g_players[playerIdx].mfd.commandMenu.targetSlotValid[nextSlot + 1]);
			}

			if ((unsigned int)(uint8_t)g_players[playerIdx].mfd.menuItem + 1u == 7u &&
				g_players[playerIdx].mfd.commandMenu.commandableTargetCount < 2u) {
				uint8_t menuItem;

				menuItem = g_players[playerIdx].mfd.menuItem;
				menuItem = (uint8_t)(menuItem + 2u);
				g_players[playerIdx].mfd.menuItem = menuItem;
				if (!g_players[playerIdx].mfd.reinforcementCommandAvailable) {
					g_players[playerIdx].mfd.menuItem = 0;
				}
			}
		} else if (g_players[playerIdx].mfd.menuRow <= 7u || g_players[playerIdx].mfd.menuRow >= 10u) {
			if (g_players[playerIdx].mfd.menuRow <= 7u) {
				if (!g_players[playerIdx].mfd.commandMenu.targetSlotValid[g_players[playerIdx].mfd.menuRow]) {
					g_players[playerIdx].mfd.menuRow = 0;
					g_players[playerIdx].mfd.menuItem = 0;
					g_players[playerIdx].mfd.commandMenu.selectedTargetSlot = 0;
				}

				if (g_players[playerIdx]
						.mfd.commandMenu.primaryTargetObjIdx[g_players[playerIdx].mfd.menuRow] ==
					viewedObjectIdx) {
					uint8_t menuItem;

					menuItem = g_players[playerIdx].mfd.menuItem;
					if (menuItem != 0 &&
						!Mfd_IsCommandMenuItemAvailable((uint16_t)playerIdx, g_players[playerIdx].mfd.menuRow,
														menuItem)) {
						unsigned int scanCount;

						menuItem = (uint8_t)(g_players[playerIdx].mfd.menuItem + 1u);
						g_players[playerIdx].mfd.menuItem = menuItem;
						scanCount = 0;
						if (!Mfd_IsCommandMenuItemAvailable((uint16_t)playerIdx,
															g_players[playerIdx].mfd.menuRow, menuItem)) {
							for (;;) {
								if (scanCount >= 6u) {
									break;
								}
								menuItem = (uint8_t)(g_players[playerIdx].mfd.menuItem + 1u);
								++scanCount;
								g_players[playerIdx].mfd.menuItem = menuItem;
								if (Mfd_IsCommandMenuItemAvailable(
										(uint16_t)playerIdx, g_players[playerIdx].mfd.menuRow, menuItem)) {
									break;
								}
							}
						}
					}
				}
			} else if (g_players[playerIdx].mfd.menuRow <= 40u &&
					   !g_players[playerIdx]
							.mfd.commandMenu
							.targetSlotValid[g_players[playerIdx].mfd.commandMenu.selectedTargetSlot]) {
				g_players[playerIdx].mfd.menuRow = 0;
				g_players[playerIdx].mfd.menuItem = 0;
				g_players[playerIdx].mfd.commandMenu.selectedTargetSlot = 0;
			}
		}

		if ((player->mfd.menuRow == 7 || commandMenu->selectedTargetSlot == 7) &&
			commandMenu->commandableTargetCount == 0) {
			player->mfd.menuRow = 0;
			player->mfd.menuItem = 0;
			commandMenu->selectedTargetSlot = 0;
		}
	}

	{
		int activeRegionIdx;

		activeRegionIdx = regionIdx;
		Mission_SetActiveRegionObjectRanges(activeRegionIdx);
	}
}

// FUNCTION: XWA 0x4C4690
void Mfd_DrawConsolePage(int mfdSide, void* mfdSurface) {
	if (g_useHardware3D) {
		if (mfdSide == 1) {
			FlightText_SetRenderOffset((int16_t)g_hudMfdTextInsetX,
									   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		} else {
			FlightText_SetRenderOffset((int16_t)(g_screenWidth - g_hudMfdTextInsetX - g_hudMfdPaneWidth),
									   (int16_t)(g_screenHeight - g_hudMfdPaneHeight));
		}
	} else {
		FlightSw_SetRenderTarget(mfdSurface, (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight,
								 g_flight16bppBytesPerPixel * (uint16_t)g_hudMfdPaneWidth);
	}

	FlightText_SetClipRect(0, 0, (uint16_t)g_hudMfdPaneWidth, (uint16_t)g_hudMfdPaneHeight);
	FlightText_SetCursor(0, 0);
	FlightText_SetFontTier(0);
	FlightText_SetBackgroundColor(g_flightColorEscapeBypassChar);
	FlightText_SetColor(0x4au);
	if (!g_useHardware3D) {
		g_flightFillClipRectFn();
	}
	FlightConsole_DrawHistory();
	FlightConsole_DrawPrompt(g_localPlayer);

	if (g_useHardware3D) {
		FlightText_SetRenderOffset(0, 0);
		return;
	}

	FlightSw_SetRenderTarget(NULL, 320, 200u, 0);
	if (mfdSide == 1) {
		Blit16ToFlightSurface(mfdSurface, g_flightColorEscapeBypassChar, 0, 0, 5u,
							  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight), (uint16_t)g_hudMfdPaneWidth,
							  (uint16_t)g_hudMfdPaneHeight,
							  (uint16_t)((uint16_t)g_hudMfdPaneWidth * g_flight16bppBytesPerPixel));
	} else {
		Blit16ToFlightSurface(mfdSurface, g_flightColorEscapeBypassChar, 0, 0,
							  (uint16_t)(g_screenWidth - g_hudMfdPaneWidth),
							  (uint16_t)(g_screenHeight - g_hudMfdPaneHeight), (uint16_t)g_hudMfdPaneWidth,
							  (uint16_t)g_hudMfdPaneHeight,
							  (uint16_t)((uint16_t)g_hudMfdPaneWidth * g_flight16bppBytesPerPixel));
	}
}

// FUNCTION: XWA 0x4C4850
// Handles confirming the current MFD command-menu item for one player: enters a
// submenu, picks a target row, returns from object rows, or dispatches the
// chosen command through Mfd_ExecuteCommandMenuSelection.
void Mfd_SelectCommandMenuItem(int playerIdx) {
	int altViewObjectIdx = g_players[playerIdx].mapCameraState ? g_players[playerIdx].altViewObjectIdx
															   : g_players[playerIdx].objectIndex;
	uint8_t menuRow;
	if (altViewObjectIdx == 0xFFFF || g_players[playerIdx].connectedFlag != 1)
		return;

	menuRow = g_players[playerIdx].mfd.menuRow;
	if (!menuRow) {
		// Top-level menu: open a submenu, or execute item 9 (reinforcements).
		if (g_players[playerIdx].mfd.menuItem <= 8) {
			g_players[playerIdx].mfd.menuRow =
				(uint8_t)g_mfdCommandMainMenuRowByItem[g_players[playerIdx].mfd.menuItem];
			g_players[playerIdx].mfd.menuItem = 0;
			return;
		}
	} else if (menuRow <= 8) {
		if (menuRow == 8) {
			// "All craft" row: select the wingman slot and open the resupply menu.
			uint8_t menuItem = g_players[playerIdx].mfd.menuItem;
			if (menuItem) {
				g_players[playerIdx].mfd.commandMenu.selectedTargetSlot = menuItem - 1;
				g_players[playerIdx].mfd.menuRow = 50;
				g_players[playerIdx].mfd.menuItem = 0;
				return;
			}
		} else if (g_players[playerIdx].mfd.menuItem) {
			// Object target row (1..7): select the target, open its command submenu,
			// or execute immediately when the row points at the player's own craft.
			g_players[playerIdx].mfd.commandMenu.selectedTargetSlot = menuRow;
			if (g_players[playerIdx].mfd.menuRow >= 7 ||
				g_players[playerIdx].mfd.commandMenu.primaryTargetObjIdx[g_players[playerIdx].mfd.menuRow] !=
					altViewObjectIdx) {
				g_players[playerIdx].mfd.menuRow =
					(uint8_t)g_mfdCommandSubMenuRowByItem[g_players[playerIdx].mfd.menuItem];
				g_players[playerIdx].mfd.menuItem = 0;
				return;
			}
		} else {
			g_players[playerIdx].mfd.menuRow = 0;
			g_players[playerIdx].mfd.menuItem = 0;
			g_players[playerIdx].mfd.commandMenu.selectedTargetSlot = 0;
			return;
		}
	} else {
		// In a command submenu: execute the chosen action (item index - 1).
		uint8_t menuItem = g_players[playerIdx].mfd.menuItem;
		if (!menuItem) {
			g_players[playerIdx].mfd.menuRow = 0;
			g_players[playerIdx].mfd.menuItem = 0;
			g_players[playerIdx].mfd.commandMenu.selectedTargetSlot = 0;
			return;
		}
		g_players[playerIdx].mfd.menuItem = menuItem - 1;
	}

	Mfd_ExecuteCommandMenuSelection(playerIdx);
}

// FUNCTION: XWA 0x4C4970
// Executes the selected MFD command-menu action for one player: reinforcement
// and system-condition reports, player self-commands (dock/board, pickup,
// release, auto-gunner, eject), and the wingmate commands attack, evade,
// cover-me, form-up, report-in, and resupply/protect.
void Mfd_ExecuteCommandMenuSelection(int playerIdx) {
	uint8_t mapCameraState = g_players[playerIdx].mapCameraState;
	int targetObjIdx = 0xFFFF;
	CraftData* pCraft = NULL;
	CraftData* playerCraft = NULL;
	CraftData* wingmanCraft = NULL;
	uint8_t attackApplied = 0;
	unsigned int menuRow = g_players[playerIdx].mfd.menuRow;
	int menuItem = g_players[playerIdx].mfd.menuItem;
	uint16_t menuTargetSig = 0;
	int srcObjIdx = mapCameraState ? g_players[playerIdx].altViewObjectIdx : g_players[playerIdx].objectIndex;
	int slotIdx;
	int i;
	uint8_t selSlot;
	uint16_t targetSig;
	uint16_t curTarget;
	uint8_t formationSlots[7];

	if (srcObjIdx != 0xFFFF) {
		MobileObject* mobj = g_objectTable[srcObjIdx].mobj;
		if (mobj) {
			pCraft = mobj->pCraft;
			playerCraft = pCraft;
		} else {
			pCraft = NULL;
		}
	}

	// Reinforcement request / system-condition report (menu row 0, item 9).
	if (g_players[playerIdx].mfd.menuRow == 0 && menuItem == 9) {
		if (mapCameraState || (pCraft && (pCraft->workingSubsystems & 0x200) != 0)) {
			if (!g_missionFlightRuntimeState
					 .teamReinforcementCalled[(uint16_t)g_players[playerIdx].playerIff]) {
				if (g_players[playerIdx].iff == g_players[g_localPlayer].iff)
					msg_emitInFlightMessage(MSG_REINFORCE_CONFIRM, playerIdx);
				g_players[playerIdx].pendingActionId = 3;
				g_players[playerIdx].pendingActionTimer = 1888;
			}
		} else {
			g_msgArgTable[0] = 108;
			g_msgArgTable[1] = 94;
			msg_emitInFlightMessage(MSG_SYSTEMCOND, playerIdx);
		}
	}

	// Wingmate command rows resolve a primary target from the command menu.
	if (menuRow >= 0xA && menuRow <= 0x28) {
		selSlot = g_players[playerIdx].mfd.commandMenu.selectedTargetSlot;
		targetObjIdx = g_players[playerIdx].mfd.commandMenu.primaryTargetObjIdx[selSlot];
		menuTargetSig = g_players[playerIdx].mfd.commandMenu.primaryTargetSignature[selSlot];
	}

	switch (menuRow) {
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
			// Player self-commands.
			switch (menuItem) {
				case 1:
					Player_HandleDockBoardCommand(playerIdx);
					break;
				case 2:
					Player_HandlePickupCommand(playerIdx);
					break;
				case 3:
					Player_ReleaseCarriedObject(playerIdx);
					break;
				case 4:
					Player_AutoGunnerToggle(playerIdx);
					break;
				case 5:
					Player_AutoGunnerToggle(playerIdx);
					break;
				case 6:
					Player_HandleCraftDestruction(playerIdx);
					break;
				default:
					break;
			}
			break;

		case 0xA: {
			// Attack commands. First pass handles target-following actions.
			if (g_players[playerIdx].currentTargetObjectIdx != 0xffffu) {
				for (slotIdx = 0; slotIdx < 7; ++slotIdx) {
					selSlot = g_players[playerIdx].mfd.commandMenu.selectedTargetSlot;
					if (selSlot != 7 && selSlot != slotIdx)
						continue;
					targetObjIdx = g_players[playerIdx].mfd.commandMenu.primaryTargetObjIdx[slotIdx];
					if (targetObjIdx == 0xFFFF)
						continue;
					targetSig = g_players[playerIdx].mfd.commandMenu.primaryTargetSignature[slotIdx];
					curTarget = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
					switch (menuItem) {
						case 0:
						case 3:
							if (targetObjIdx == curTarget)
								break;
							if (Object_IsFriendlyToTeam(g_players[playerIdx].currentTargetObjectIdx,
														(uint16_t)g_players[playerIdx].playerIff))
								break;
							if (g_objectTable[targetObjIdx].playerOwnerIdx == -1 &&
								g_objectTable[targetObjIdx].objectType &&
								g_objectTable[targetObjIdx].objectSignature == targetSig) {
								AiController* ctrl;

								wingmanCraft = g_objectTable[targetObjIdx].mobj->pCraft;
								ctrl = pai_GetEffectiveAIController(wingmanCraft);
								if (!wingmanCraft->followPlayerMode) {
									wingmanCraft->savedPendingPlan = ctrl->pendingPlanId;
									wingmanCraft->savedCurrentPlan = ctrl->currentPlanId;
								}
								wingmanCraft->followPlayerMode = 2;
								wingmanCraft->followPlayerIdx = (uint8_t)playerIdx;
								wingmanCraft = g_objectTable[targetObjIdx].mobj->pCraft;
								wingmanCraft->followTimer = (uint16_t)(menuRow + menuItem);
								wingmanCraft->playerCommandCraftTypeFilter = 0;
								wingmanCraft->playerCommandTeamFilter = 0;
								pai_GetEffectiveAIController(wingmanCraft)->candidateTargetIdx =
									g_players[playerIdx].currentTargetObjectIdx;
								attackApplied = 1;
							}
							break;
						case 1:
							if (targetObjIdx == curTarget ||
								g_players[playerIdx].selectedTargetComponent == -1)
								break;
							if (Object_IsFriendlyToTeam(g_players[playerIdx].currentTargetObjectIdx,
														(uint16_t)g_players[playerIdx].playerIff))
								break;
							if (g_objectTable[targetObjIdx].objectType &&
								g_objectTable[targetObjIdx].objectSignature == targetSig) {
								AiController* ctrl;

								wingmanCraft = g_objectTable[targetObjIdx].mobj->pCraft;
								ctrl = pai_GetEffectiveAIController(wingmanCraft);
								if (!wingmanCraft->followPlayerMode) {
									wingmanCraft->savedPendingPlan = ctrl->pendingPlanId;
									wingmanCraft->savedCurrentPlan = ctrl->currentPlanId;
								}
								wingmanCraft->followPlayerMode = 2;
								wingmanCraft->followPlayerIdx = (uint8_t)playerIdx;
								wingmanCraft = g_objectTable[targetObjIdx].mobj->pCraft;
								wingmanCraft->followTimer = (uint16_t)(menuRow + 1);
								pai_GetEffectiveAIController(wingmanCraft)->candidateTargetIdx =
									g_players[playerIdx].currentTargetObjectIdx;
								wingmanCraft->playerCommandCraftTypeFilter =
									(uint16_t)(ModelMesh_GetObjectTypeMeshType(
												   g_objectTable[curTarget].objectType,
												   (uint16_t)g_players[playerIdx].selectedTargetComponent) +
											   1);
								wingmanCraft->playerCommandTeamFilter = (uint8_t)-1;
								attackApplied = 1;
							}
							break;
						case 2:
						case 4:
							if (targetObjIdx == curTarget)
								break;
							if (Object_IsFriendlyToTeam(g_players[playerIdx].currentTargetObjectIdx,
														(uint16_t)g_players[playerIdx].playerIff))
								break;
							if (g_objectTable[targetObjIdx].objectType &&
								g_objectTable[targetObjIdx].objectSignature == targetSig) {
								AiController* ctrl;
								uint16_t targetType;
								uint16_t ti;
								ObjectRecord* tgt;

								wingmanCraft = g_objectTable[targetObjIdx].mobj->pCraft;
								ctrl = pai_GetEffectiveAIController(wingmanCraft);
								if (!wingmanCraft->followPlayerMode) {
									wingmanCraft->savedPendingPlan = ctrl->pendingPlanId;
									wingmanCraft->savedCurrentPlan = ctrl->currentPlanId;
								}
								wingmanCraft->followPlayerMode = 2;
								wingmanCraft->followPlayerIdx = (uint8_t)playerIdx;
								wingmanCraft = g_objectTable[targetObjIdx].mobj->pCraft;
								wingmanCraft->followTimer = (uint16_t)(menuRow + menuItem);
								// Find the craft-type index for the target's object type.
								// The original scans a fixed 0x22D limit that over-reads
								// past the 404-entry table; a valid target always matches
								// within the table.
								targetType = g_objectTable[curTarget].objectType;
								for (ti = 0; ti < 0x22Du; ++ti) {
									if (g_objectTypeTables.craftTypeToObjectType[ti] == targetType)
										break;
								}
								wingmanCraft->playerCommandCraftTypeFilter = ti;
								tgt = &g_objectTable[curTarget];
								if (tgt->mobj)
									wingmanCraft->playerCommandTeamFilter = tgt->mobj->team;
								else
									wingmanCraft->playerCommandTeamFilter =
										g_missionFlightGroups[tgt->flightGroupIdx].fg.team;
								pai_GetEffectiveAIController(wingmanCraft)->candidateTargetIdx = (uint16_t)-1;
								attackApplied = 1;
							}
							break;
						default:
							break;
					}
				}
			}
			// Second pass handles attack-by-type / regroup actions (items 5..7).
			for (slotIdx = 0; slotIdx < 7; ++slotIdx) {
				selSlot = g_players[playerIdx].mfd.commandMenu.selectedTargetSlot;
				if (selSlot != 7 && selSlot != slotIdx)
					continue;
				targetObjIdx = g_players[playerIdx].mfd.commandMenu.primaryTargetObjIdx[slotIdx];
				if (targetObjIdx != 0xFFFF && menuItem >= 5 && menuItem <= 7) {
					if (g_objectTable[targetObjIdx].objectType &&
						g_objectTable[targetObjIdx].objectSignature ==
							g_players[playerIdx].mfd.commandMenu.primaryTargetSignature[slotIdx]) {
						AiController* ctrl;

						wingmanCraft = g_objectTable[targetObjIdx].mobj->pCraft;
						ctrl = pai_GetEffectiveAIController(wingmanCraft);
						if (!wingmanCraft->followPlayerMode) {
							wingmanCraft->savedPendingPlan = ctrl->pendingPlanId;
							wingmanCraft->savedCurrentPlan = ctrl->currentPlanId;
						}
						wingmanCraft->followPlayerMode = 2;
						wingmanCraft->followPlayerIdx = (uint8_t)playerIdx;
						attackApplied = 1;
						wingmanCraft = g_objectTable[targetObjIdx].mobj->pCraft;
						wingmanCraft->followTimer = (uint16_t)(menuRow + menuItem);
					}
				}
			}
			if (attackApplied) {
				if (g_players[playerIdx].mfd.commandMenu.selectedTargetSlot <= 6)
					msg_radioMessage((uint16_t)targetObjIdx, wingmanCraft,
									 (int16_t)g_mfdAttackRadioMsgId[menuItem],
									 g_mfdAttackRadioMsgArg[menuItem], 0);
				else
					msg_radioMessage((uint16_t)srcObjIdx, wingmanCraft,
									 (int16_t)g_mfdAttackRadioMsgId[menuItem],
									 g_mfdAttackRadioMsgArg[menuItem], 1);
			}
			break;
		}

		case 0x14: {
			// Evade / cover-me.
			CraftData* targetCraft = g_objectTable[targetObjIdx].mobj->pCraft;
			if (menuItem) {
				if (menuItem == 1) {
					Player_HandleEvadeCommand(playerIdx, targetObjIdx);
					if (g_players[playerIdx].mfd.commandMenu.selectedTargetSlot <= 6)
						msg_radioMessage((uint16_t)targetObjIdx, targetCraft, 159, 1, 0);
					else
						msg_radioMessage((uint16_t)srcObjIdx, targetCraft, 159, 1, 1);
				}
			} else if (targetObjIdx <= 6) {
				Player_HandleCoverMeCommand(playerIdx, targetObjIdx);
			} else {
				Player_HandleCoverMeCommand(playerIdx, 0xFFFF);
			}
			break;
		}

		case 0x1E: {
			// Form-up / formation adjustments.
			uint8_t formationApplied = 0;
			if (srcObjIdx != 0xFFFF) {
				// The original leaves this slot table uninitialized; every slot read
				// below is written first for a valid wingman.
				uint8_t nextSlot = 1;
				for (i = 0; i < 7; ++i) {
					int objIdx = g_players[playerIdx].mfd.commandMenu.primaryTargetObjIdx[i];
					if (objIdx != 0xFFFF && objIdx != srcObjIdx)
						formationSlots[i] = nextSlot++;
				}
				selSlot = g_players[playerIdx].mfd.commandMenu.selectedTargetSlot;
				if (selSlot == 7) {
					for (i = 0; i < 7; ++i) {
						targetObjIdx = g_players[playerIdx].mfd.commandMenu.primaryTargetObjIdx[i];
						if (targetObjIdx != 0xFFFF && g_objectTable[targetObjIdx].objectType &&
							g_objectTable[targetObjIdx].objectSignature ==
								g_players[playerIdx].mfd.commandMenu.primaryTargetSignature[i]) {
							AiController* ctrl;

							wingmanCraft = g_objectTable[targetObjIdx].mobj->pCraft;
							ctrl = pai_GetEffectiveAIController(wingmanCraft);
							if (!wingmanCraft->followPlayerMode) {
								wingmanCraft->savedPendingPlan = ctrl->pendingPlanId;
								wingmanCraft->savedCurrentPlan = ctrl->currentPlanId;
							}
							wingmanCraft->followPlayerMode = 2;
							wingmanCraft->followPlayerIdx = (uint8_t)playerIdx;
							formationApplied = 1;
							wingmanCraft = g_objectTable[targetObjIdx].mobj->pCraft;
							wingmanCraft->followFormationSlot = formationSlots[i];
							wingmanCraft->followTimer = (uint16_t)(menuRow + menuItem);
						}
					}
				} else if (selSlot < 7) {
					if (g_objectTable[targetObjIdx].objectType &&
						g_objectTable[targetObjIdx].objectSignature == menuTargetSig) {
						AiController* ctrl;

						wingmanCraft = g_objectTable[targetObjIdx].mobj->pCraft;
						ctrl = pai_GetEffectiveAIController(wingmanCraft);
						if (!wingmanCraft->followPlayerMode) {
							wingmanCraft->savedPendingPlan = ctrl->pendingPlanId;
							wingmanCraft->savedCurrentPlan = ctrl->currentPlanId;
						}
						wingmanCraft->followPlayerMode = 2;
						wingmanCraft->followPlayerIdx = (uint8_t)playerIdx;
						formationApplied = 1;
						wingmanCraft = g_objectTable[targetObjIdx].mobj->pCraft;
						wingmanCraft->followFormationSlot = formationSlots[selSlot];
						wingmanCraft->followTimer = (uint16_t)(menuRow + menuItem);
					}
				}
			}
			if (formationApplied) {
				switch (menuItem) {
					case 0:
						if (g_players[playerIdx].mfd.commandMenu.selectedTargetSlot <= 6)
							msg_radioMessage((uint16_t)targetObjIdx, wingmanCraft, 168, 16, 0);
						else
							msg_radioMessage((uint16_t)srcObjIdx, wingmanCraft, 168, 16, 1);
						g_players[playerIdx].mfd.menuItem = 0;
						break;
					case 1:
						playerCraft->aiFlight.formationType = 3;
						if (g_players[playerIdx].mfd.commandMenu.selectedTargetSlot <= 6)
							msg_radioMessage((uint16_t)targetObjIdx, wingmanCraft, 169, 16, 0);
						else
							msg_radioMessage((uint16_t)srcObjIdx, wingmanCraft, 169, 16, 1);
						g_players[playerIdx].mfd.menuItem = 0;
						break;
					case 2:
						playerCraft->aiFlight.formationType = 8;
						if (g_players[playerIdx].mfd.commandMenu.selectedTargetSlot <= 6)
							msg_radioMessage((uint16_t)targetObjIdx, wingmanCraft, 170, 16, 0);
						else
							msg_radioMessage((uint16_t)srcObjIdx, wingmanCraft, 170, 16, 1);
						g_players[playerIdx].mfd.menuItem = 0;
						break;
					case 3:
						playerCraft->aiFlight.formationType = 0;
						if (g_players[playerIdx].mfd.commandMenu.selectedTargetSlot <= 6)
							msg_radioMessage((uint16_t)targetObjIdx, wingmanCraft, 171, 16, 0);
						else
							msg_radioMessage((uint16_t)srcObjIdx, wingmanCraft, 171, 16, 1);
						g_players[playerIdx].mfd.menuItem = 0;
						break;
					case 4:
						playerCraft->aiFlight.formationType = 1;
						if (g_players[playerIdx].mfd.commandMenu.selectedTargetSlot <= 6)
							msg_radioMessage((uint16_t)targetObjIdx, wingmanCraft, 172, 16, 0);
						else
							msg_radioMessage((uint16_t)srcObjIdx, wingmanCraft, 172, 16, 1);
						g_players[playerIdx].mfd.menuItem = 0;
						break;
					case 5:
						playerCraft->aiFlight.formationType = 30;
						if (g_players[playerIdx].mfd.commandMenu.selectedTargetSlot <= 6)
							msg_radioMessage((uint16_t)targetObjIdx, wingmanCraft, 173, 16, 0);
						else
							msg_radioMessage((uint16_t)srcObjIdx, wingmanCraft, 173, 16, 1);
						g_players[playerIdx].mfd.menuItem = 0;
						break;
					case 6:
						if (playerCraft->aiFlight.separation)
							--playerCraft->aiFlight.separation;
						if (g_players[playerIdx].mfd.commandMenu.selectedTargetSlot <= 6)
							msg_radioMessage((uint16_t)targetObjIdx, wingmanCraft, 174, 17, 0);
						else
							msg_radioMessage((uint16_t)srcObjIdx, wingmanCraft, 174, 17, 1);
						g_players[playerIdx].mfd.menuItem = 0;
						break;
					case 7:
						if ((uint8_t)playerCraft->aiFlight.separation < 0xA)
							++playerCraft->aiFlight.separation;
						if (g_players[playerIdx].mfd.commandMenu.selectedTargetSlot <= 6)
							msg_radioMessage((uint16_t)targetObjIdx, wingmanCraft, 175, 18, 0);
						else
							msg_radioMessage((uint16_t)srcObjIdx, wingmanCraft, 175, 18, 1);
						g_players[playerIdx].mfd.menuItem = 0;
						break;
					default:
						g_players[playerIdx].mfd.menuItem = 0;
						break;
				}
			}
			break;
		}

		case 0x28:
			// Report in.
			if (!menuItem)
				Player_HandleReportInCommand(playerIdx, targetObjIdx);
			break;

		case 0x32: {
			// Resupply / protect.
			int selSlot;
			int secObjIdx;
			int16_t secSig;

			if (menuItem ||
				(selSlot = g_players[playerIdx].mfd.commandMenu.selectedTargetSlot,
				 secObjIdx = g_players[playerIdx].mfd.commandMenu.secondaryTargetObjIdx[selSlot],
				 secObjIdx == 0xFFFF) ||
				(secSig = g_players[playerIdx].mfd.commandMenu.secondaryTargetSignature[selSlot],
				 g_objectTable[secObjIdx].objectType == OBJ_None) ||
				g_objectTable[secObjIdx].objectSignature != (uint16_t)secSig) {
				if (g_players[playerIdx].currentTargetObjectIdx != 0xffffu) {
					// Protect / cover-from commands directed at the current target.
					uint16_t curTarget = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
					if (curTarget >= g_activeRegionObjectSlotStart &&
						curTarget < g_activeRegionCraftObjectSlotEnd &&
						!g_objectTable[curTarget].mobj->pCraft->workingSubsystems) {
						int selSlot2 = g_players[playerIdx].mfd.commandMenu.selectedTargetSlot;
						int cmdObj = g_players[playerIdx].mfd.commandMenu.secondaryTargetObjIdx[selSlot2];
						if (cmdObj != 0xFFFF) {
							int16_t cmdSig =
								g_players[playerIdx].mfd.commandMenu.secondaryTargetSignature[selSlot2];
							if (g_objectTable[cmdObj].objectType &&
								g_objectTable[cmdObj].objectSignature == (uint16_t)cmdSig) {
								AiController* ctrl;

								wingmanCraft = g_objectTable[cmdObj].mobj->pCraft;
								ctrl = pai_GetEffectiveAIController(wingmanCraft);
								if (!wingmanCraft->followPlayerMode) {
									wingmanCraft->savedPendingPlan = ctrl->pendingPlanId;
									wingmanCraft->savedCurrentPlan = ctrl->currentPlanId;
								}
								wingmanCraft->followPlayerMode = 2;
								wingmanCraft->followPlayerIdx = (uint8_t)playerIdx;
								if (menuItem >= 1 && menuItem <= 4) {
									wingmanCraft = g_objectTable[cmdObj].mobj->pCraft;
									wingmanCraft->followTimer = (uint16_t)(menuItem + 50);
									pai_GetEffectiveAIController(wingmanCraft)->candidateTargetIdx =
										g_players[playerIdx].currentTargetObjectIdx;
								}
								switch (menuItem) {
									case 1:
										msg_radioMessage((uint16_t)cmdObj, wingmanCraft, 176, 1, 0);
										break;
									case 2:
										msg_radioMessage((uint16_t)cmdObj, wingmanCraft, 177, 1, 0);
										break;
									case 3:
										msg_radioMessage((uint16_t)cmdObj, wingmanCraft, 178, 1, 0);
										break;
									case 4:
										msg_radioMessage((uint16_t)cmdObj, wingmanCraft, 179, 1, 0);
										break;
									default:
										break;
								}
							}
						}
					}
				}
			} else {
				// Resupply the valid secondary target.
				AiController* ctrl;

				wingmanCraft = g_objectTable[secObjIdx].mobj->pCraft;
				ctrl = pai_GetEffectiveAIController(wingmanCraft);
				if (!wingmanCraft->followPlayerMode) {
					wingmanCraft->savedPendingPlan = ctrl->pendingPlanId;
					wingmanCraft->savedCurrentPlan = ctrl->currentPlanId;
				}
				wingmanCraft->followPlayerMode = 2;
				wingmanCraft->followPlayerIdx = (uint8_t)playerIdx;
				Player_HandleResupplyCommand(playerIdx, secObjIdx);
			}
			break;
		}

		default:
			break;
	}

	if (menuRow != 30) {
		g_players[playerIdx].mfd.menuRow = 0;
		g_players[playerIdx].mfd.menuItem = 0;
		g_players[playerIdx].mfd.commandMenu.selectedTargetSlot = 0;
	}
}

// FUNCTION: XWA 0x4C58E0
void Mfd_DrawCommandObjectOrderLine(unsigned int objectIdx, int unused, int16_t y) {
	CraftData* craft;
	AiController* ai;
	int displayPlanId;
	uint16_t timerMinutes;
	uint16_t timerSeconds;
	uint16_t rangeWhole;
	uint16_t rangeFrac;
	char useTimerSuffix;
	const char* orderText;
	uint16_t targetObjIdx;
	MemoryHandle orderStringHandle;

	(void)unused;

	useTimerSuffix = 0;
	rangeWhole = 0;
	rangeFrac = 0;
	timerMinutes = 0;
	timerSeconds = 0;

	craft = g_objectTable[objectIdx].mobj->pCraft;
	ai = pai_GetEffectiveAIController(craft);

	if ((g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
		 g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) &&
		g_flightPlayerCount > 1) {
		uint16_t currentTargetObjectIdx;

		currentTargetObjectIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
		if (currentTargetObjectIdx == 0xffffu) {
			return;
		}
		if (g_objectTable[currentTargetObjectIdx].mobj != NULL && craft != NULL &&
			!g_flightLocatePlayersEnabled) {
			int playerIff;

			playerIff = 0;
			playerIff = (uint16_t)g_players[g_localPlayer].playerIff;
			if ((int8_t)craft->iffVisibility[playerIff] < 1 &&
				Object_IsHostileToTeam(currentTargetObjectIdx, playerIff) == 1) {
				return;
			}
		}
	}

	displayPlanId = ai->pendingPlanId;
	if (craft->workingSubsystems == 0) {
		displayPlanId = pai_findplanbyname("disabledpln");
	} else {
		if (g_objectTable[objectIdx].mobj->speed == 0) {
			if (strcmp(g_planTable[ai->pendingPlanId].name, "flyhomepln") == 0 ||
				strcmp(g_planTable[ai->pendingPlanId].name, "followhomepln") == 0 ||
				strcmp(g_planTable[ai->pendingPlanId].name, "flyhomeevadepln") == 0 ||
				strcmp(g_planTable[ai->pendingPlanId].name, "followhomeevadepln") == 0 ||
				strcmp(g_planTable[ai->pendingPlanId].name, "enterhangarpln") == 0 ||
				strcmp(g_planTable[ai->pendingPlanId].name, "exithangarpln") == 0 ||
				strcmp(g_planTable[ai->pendingPlanId].name, "intohyperspacepln") == 0 ||
				strcmp(g_planTable[ai->pendingPlanId].name, "outofhyperspacepln") == 0 ||
				strcmp(g_planTable[ai->pendingPlanId].name, "starshipintohyperpln") == 0 ||
				strcmp(g_planTable[ai->pendingPlanId].name, "starshipfollowhomepln") == 0) {
				displayPlanId = pai_findplanbyname("waitpln");
			}
		}
	}

	if (g_objectTable[objectIdx].playerOwnerIdx != -1) {
		pai_ObjectRefDirectionToObjectRef(objectIdx, ai->targetObjIdx);
	} else {
		trig2_ctop(ai->aimPointX - g_objectTable[objectIdx].world_x,
				   ai->aimPointY - g_objectTable[objectIdx].world_y,
				   ai->aimPointZ - g_objectTable[objectIdx].world_z);
	}

	{
		unsigned int rawDistance;

		rawDistance = (unsigned int)trig2_polardistance;
		if (g_objectTable[objectIdx].mobj->speed == 0) {
			useTimerSuffix = 1;
			if (strcmp(g_planTable[ai->pendingPlanId].name, "board2pln") == 0 ||
				strcmp(g_planTable[ai->pendingPlanId].name, "waitpln") == 0) {
				int seconds;

				seconds = ai->maneuverTimer / 236;
				timerMinutes = (uint16_t)((uint16_t)seconds / 60);
				timerSeconds = (uint16_t)(seconds - timerMinutes * 60);
			} else if ((uint16_t)rawDistance == 0) {
				timerMinutes = 0;
				timerSeconds = 0;
			}
		} else {
			uint16_t displayDistance;

			trig2_polardistance = (int)(rawDistance * 161u);
			displayDistance = (uint16_t)(((int)(rawDistance * 161u)) >> 16);
			if (displayDistance >= 10000u) {
				displayDistance = 9999u;
			}
			rangeWhole = (uint16_t)(displayDistance / 100);
			rangeFrac = (uint16_t)(displayDistance - rangeWhole * 100);
		}
	}

	orderStringHandle =
		g_missionOrderStringHandles[g_objectTable[objectIdx].flightGroupIdx]
								   [g_objectTable[objectIdx].regionIdx][((uint8_t)ai->currentOrderSlot)];
	if (orderStringHandle != 0) {
		orderText = (const char*)Memory_LockHandle(orderStringHandle);
		Memory_UnlockHandle(orderStringHandle);
	} else {
		orderText = g_strInFlightMessages[g_planReportMessageIdByPlanId[displayPlanId]];
	}

	if (g_objectTable[objectIdx].playerOwnerIdx != -1) {
		targetObjIdx = (uint16_t)g_players[g_objectTable[objectIdx].playerOwnerIdx].currentTargetObjectIdx;
	} else {
		targetObjIdx = ai->targetObjIdx;
	}
	if (craft->workingSubsystems == 0) {
		targetObjIdx = 0xffffu;
	}
	if (strcmp(g_planTable[displayPlanId].name, "waitpln") == 0) {
		targetObjIdx = 0xffffu;
	}

	if (targetObjIdx == 0xffffu || targetObjIdx == 255u || targetObjIdx >= 0x8000u) {
		FlightText_SetCursor(
			(int16_t)((((uint16_t)g_hudMfdPaneWidth >> 1) - (FlightText_MeasureStringWidth(orderText) >> 1)) -
					  (FlightText_MeasureStringWidth(" - 0:00") >> 1)),
			y);
	} else {
		Hud_AppendObjectDisplayName(targetObjIdx, 3);
		FlightText_SetCursor((int16_t)((((((uint16_t)g_hudMfdPaneWidth >> 1) -
										  (FlightText_MeasureStringWidth(orderText) >> 1)) -
										 (FlightText_MeasureStringWidth(" ") >> 1)) -
										(FlightText_MeasureStringWidth(g_flightTextScratchBuffer) >> 1)) -
									   (FlightText_MeasureStringWidth(" - 0:00") >> 1)),
							 y);
	}

	FlightText_SetColor(0x43u);
	FlightText_DrawString(orderText);
	if (targetObjIdx != 0xffffu && targetObjIdx != 255u && targetObjIdx < 0x8000u) {
		FlightText_DrawString(" ");
		FlightText_DrawString(g_flightTextScratchBuffer);
	}
	FlightText_SetColor(0x43u);
	FlightText_DrawString(" -");
	if (useTimerSuffix) {
		FlightText_DrawDecimalNumber(timerMinutes, 2u, 1u);
		g_flightDrawCharFn(':');
		FlightText_DrawDecimalNumber(timerSeconds, 2u, 2u);
	} else {
		FlightText_DrawDecimalNumber(rangeWhole, 2u, 1u);
		g_flightDrawCharFn('.');
		FlightText_DrawDecimalNumber(rangeFrac, 2u, 2u);
	}
}
