#ifndef XWA_FLIGHT_FLIGHT_H
#define XWA_FLIGHT_FLIGHT_H

#include "xwa/assets/file_io.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/util/memory.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FlightResolutionMode {
	FLIGHT_RES_640x480 = 0,
	FLIGHT_RES_800x600 = 1,
	FLIGHT_RES_1024x768 = 2,
	FLIGHT_RES_1152x864 = 3,
	FLIGHT_RES_1280x1024 = 4,
	FLIGHT_RES_1600x1200 = 5
} FlightResolutionMode;

typedef union FlightForceFeedbackSpeedSnapshot {
	uint32_t packed;
	uint16_t words[2];
} FlightForceFeedbackSpeedSnapshot;

typedef union FlightCraftModelIndex {
	uint32_t packed;
	uint16_t words[2];
} FlightCraftModelIndex;

typedef enum GameActionKey {
	KEY_NONE = 0x000,
	KEY_BACKSPACE = 0x008,
	KEY_TAB = 0x009,
	KEY_ENTER = 0x00d,
	KEY_ESCAPE = 0x01b,
	KEY_SPACE = 0x020,
	KEY_SHIFT_1 = 0x021,
	KEY_QUOTES = 0x022,
	KEY_SHIFT_3 = 0x023,
	KEY_SHIFT_4 = 0x024,
	KEY_APOSTROPHE = 0x027,
	KEY_STAR = 0x02a,
	KEY_COMMA = 0x02c,
	KEY_MINUS = 0x02d,
	KEY_PERIOD = 0x02e,
	KEY_1 = 0x031,
	KEY_2 = 0x032,
	KEY_3 = 0x033,
	KEY_4 = 0x034,
	KEY_5 = 0x035,
	KEY_6 = 0x036,
	KEY_7 = 0x037,
	KEY_8 = 0x038,
	KEY_9 = 0x039,
	KEY_0 = 0x030,
	KEY_SEMICOLON = 0x03b,
	KEY_PLUS = 0x02b,
	KEY_SLASH = 0x02f,
	KEY_LESS_THAN = 0x03c,
	KEY_EQUAL = 0x03d,
	KEY_SHIFT_2 = 0x040,
	KEY_SHIFT_A = 0x041,
	KEY_SHIFT_B = 0x042,
	KEY_SHIFT_C = 0x043,
	KEY_SHIFT_D = 0x044,
	KEY_SHIFT_E = 0x045,
	KEY_SHIFT_F = 0x046,
	KEY_SHIFT_G = 0x047,
	KEY_SHIFT_H = 0x048,
	KEY_SHIFT_I = 0x049,
	KEY_SHIFT_P = 0x050,
	KEY_SHIFT_R = 0x052,
	KEY_SHIFT_S = 0x053,
	KEY_SHIFT_U = 0x055,
	KEY_SHIFT_W = 0x057,
	KEY_LEFT_BRACKET = 0x05b,
	KEY_FOWARD_SLASH = 0x05c,
	KEY_RIGHT_BRACKET = 0x05d,
	KEY_A = 0x061,
	KEY_B = 0x062,
	KEY_C = 0x063,
	KEY_E = 0x065,
	KEY_F = 0x066,
	KEY_G = 0x067,
	KEY_H = 0x068,
	KEY_I = 0x069,
	KEY_J = 0x06a,
	KEY_L = 0x06c,
	KEY_M = 0x06d,
	KEY_N = 0x06e,
	KEY_O = 0x06f,
	KEY_P = 0x070,
	KEY_Q = 0x071,
	KEY_R = 0x072,
	KEY_S = 0x073,
	KEY_T = 0x074,
	KEY_U = 0x075,
	KEY_V = 0x076,
	KEY_W = 0x077,
	KEY_X = 0x078,
	KEY_Y = 0x079,
	KEY_Z = 0x07a,
	KEY_ALT_B = 0x081,
	KEY_ALT_C = 0x082,
	KEY_ALT_D = 0x083,
	KEY_ALT_E = 0x084,
	KEY_ALT_J = 0x089,
	KEY_ALT_N = 0x08d,
	KEY_ALT_P = 0x08f,
	KEY_ABORT_MISSION = 0x090,
	KEY_ALT_S = 0x092,
	KEY_ALT_U = 0x094,
	KEY_ALT_V = 0x095,
	KEY_ALT_G = 0x096,
	KEY_ALT_1 = 0x09b,
	KEY_ALT_2 = 0x09c,
	KEY_ALT_3 = 0x09d,
	KEY_INSERT = 0x0a8,
	KEY_DELETE = 0x0a9,
	KEY_HOME = 0x0aa,
	KEY_END = 0x0ab,
	KEY_PAGEUP = 0x0ac,
	KEY_PAGEDOWN = 0x0ad,
	KEY_SCROLL_LOCK = 0x0af,
	KEY_PAD_0 = 0x0b2,
	KEY_PAD_1 = 0x0b3,
	KEY_PAD_2 = 0x0b4,
	KEY_PAD_3 = 0x0b5,
	KEY_PAD_4 = 0x0b6,
	KEY_PAD_5 = 0x0b7,
	KEY_PAD_6 = 0x0b8,
	KEY_PAD_7 = 0x0b9,
	KEY_PAD_8 = 0x0ba,
	KEY_PAD_9 = 0x0bb,
	KEY_PAD_SLASH = 0x0bd,
	KEY_PAD_STAR = 0x0be,
	KEY_PAD_MINUS = 0x0bf,
	KEY_PAD_PLUS = 0x0c0,
	KEY_PAD_DOT = 0x0c2,
	KEY_F1 = 0x0c3,
	KEY_F2 = 0x0c4,
	KEY_F3 = 0x0c5,
	KEY_F4 = 0x0c6,
	KEY_F5 = 0x0c7,
	KEY_F6 = 0x0c8,
	KEY_F7 = 0x0c9,
	KEY_F8 = 0x0ca,
	KEY_F9 = 0x0cb,
	KEY_F10 = 0x0cc,
	KEY_F11 = 0x0cd,
	KEY_F12 = 0x0ce,
	KEY_SHIFT_F1 = 0x0cf,
	KEY_SHIFT_F2 = 0x0d0,
	KEY_SHIFT_F3 = 0x0d1,
	KEY_SHIFT_F5 = 0x0d3,
	KEY_SHIFT_F6 = 0x0d4,
	KEY_SHIFT_F7 = 0x0d5,
	KEY_SHIFT_F9 = 0x0d7,
	KEY_SHIFT_F10 = 0x0d8,
	KEY_SHIFT_F11 = 0x0d9,
	KEY_SHIFT_F12 = 0x0da,
	KEY_THROTTLE_1 = 0x0db,
	KEY_THROTTLE_2 = 0x0dc,
	KEY_THROTTLE_3 = 0x0dd,
	KEY_THROTTLE_4 = 0x0de,
	KEY_THROTTLE_6 = 0x0df,
	KEY_THROTTLE_7 = 0x0e0,
	KEY_THROTTLE_8 = 0x0e1,
	KEY_THROTTLE_9 = 0x0e2,
	KEY_THROTTLE_10 = 0x0e3,
	KEY_THROTTLE_11 = 0x0e4,
	KEY_THROTTLE_12 = 0x0e5,
	KEY_THROTTLE_13 = 0x0e6,
	KEY_THROTTLE_14 = 0x0e7,
	KEY_MFD_OVERLAY = 0x12d,
	KEY_HUD_TOGGLE = 0x12c,
	KEY_CONSOLE_TOGGLE = 0x111,
	KEY_NEXT_PLAYER_CRAFT = 0x11e,
} GameActionKey;

#pragma pack(push, 1)
typedef struct RemotePlayerRenderSample {
	int valid;
	uint16_t objectSignature;
	int worldX;
	int worldY;
	int worldZ;
	int rollDelta;
	int pitchDelta;
	int yawDelta;
	int16_t roll;
	int16_t pitch;
	int16_t yaw;
	int16_t moveX;
	int16_t moveY;
	int16_t moveZ;
	uint16_t speedMagnitude;
	int simStateTimestamp;
} RemotePlayerRenderSample;
#pragma pack(pop)

typedef struct FlightNetWorldStateChunkPacket {
	uint32_t packetType;
	uint32_t baseChecksum;
	uint32_t chunkIndex;
	uint8_t payload[500];
} FlightNetWorldStateChunkPacket;

#pragma pack(push, 1)
typedef struct FlightInputFrameRecord {
	uint16_t reserved0;
	uint16_t key;
	int8_t axisX;
	int8_t axisY;
	int8_t axisR;
	uint8_t keyMods;
} FlightInputFrameRecord;
#pragma pack(pop)

typedef struct InputFrame {
	int applied;
	int valid;
	int timestamp;
	FlightInputFrameRecord input;
} InputFrame;

enum {
	XWA_INPUT_HISTORY_PLAYER_COUNT = 8,
	XWA_INPUT_HISTORY_FRAME_COUNT = 450,
	// Physical storage for the craft option list (XWA clears the whole 0x400-byte
};

typedef char xwa_flight_input_frame_record_size[(sizeof(FlightInputFrameRecord) == 0x08) ? 1 : -1];
typedef char xwa_input_frame_size[(sizeof(InputFrame) == 0x14) ? 1 : -1];
typedef char xwa_remote_player_render_sample_size[(sizeof(RemotePlayerRenderSample) == 0x30) ? 1 : -1];
typedef char
	xwa_remote_player_render_sample_world_x_offset[(offsetof(RemotePlayerRenderSample, worldX) == 0x6) ? 1
																									   : -1];

extern int g_asyncFlag;
extern int g_fpsSampleRingIndex;
extern int g_lastLocalReplayInputTimestamp;
extern int g_flightConfFlicker;
extern char g_FlightConfRivaTxt;
extern int g_flightConfTrainCourse;
extern char g_flightConfNoPilot;
extern int g_flightConfDirectInput;
extern char g_flightInputNonBlockingMsgPump;
extern signed char g_lastKeyCode;
extern int g_keyReady;
extern unsigned char g_flightConfSfxEnabled;
extern int g_flightConfMusicEnabled;
extern unsigned char g_flightConfVoiceEnabled;
extern int g_flightSimSideEffectsSuppressed;
extern int16_t g_localBeamTargetObjIdx;
extern int g_flightSfxSideEffectGate;
extern int g_flightConfTickCounter;
extern int inProgressLaunch;
extern int g_flightConfNewNet;
extern int g_flightConfNoLauncher;
extern int g_flightFullscreen;
extern int g_flightPageFlip;
extern int g_flightStartedWithDashArg;
extern int g_unusedFlightCmdLinePlusSwitchFlag;
extern char* g_argProgramName;
extern char* g_argSentinel;
extern char* g_argMissionPath;
extern char* sessionName;
extern char* g_argPilotName;
extern char* g_argLocalIdStr;
extern char* g_argMpGameName;
extern char* g_argUnusedZeroStr;
extern char* g_argNumPlayersStr;
extern int g_flightBrightnessScaleQ8;
extern int g_flightSideEffectsEnabled;
extern int g_fighterWarningLastScanTime;
extern int g_fighterWarningTailCooldownUntil;
extern int g_fighterWarningForwardCooldownUntil;
extern int g_fighterWarningUnusedState;
extern int g_fighterWarningPrevNearbyHostileCount;
extern int g_fighterWarningPrevForwardThreatCount;
extern int g_fighterWarningPrevTailAttackerCount;
extern uint8_t g_backdropsEnabled;
extern uint8_t g_debrisEnabled;
extern uint16_t g_starDensity;
extern int g_flightResolutionMode;
extern int g_renderTargetWidth;
extern int g_unusedFlightDisplayBytesPerPixelMirror;
extern int g_unusedFlightDisplayHardware3DMirror;
extern int g_flight16bppBytesPerPixel;
extern int g_flightSoundInitStartTimeMs;
extern char g_currentMissionFile[128];
extern XwaFile* g_filmFile;
extern int16_t g_filmVersion;
extern uint8_t g_filmRecording;
extern int g_cockpitObjectTypeForFilmHeader;
extern uint8_t g_filmHeaderDifficulty;
extern uint8_t g_filmHeaderCollisionsEnabled;
extern uint8_t g_flightDifficulty;
extern uint8_t g_flightCollisionsEnabled;
extern uint8_t g_flightLocatePlayersEnabled;
extern int g_sw3dSkipOddScanlines;
extern uint8_t g_flightReturnToFrontendRequested;
extern int g_flightReturnToMissionSetupRequested;
extern uint8_t g_provingGroundsModeActive;
extern int g_flightExitRequest;
extern uint8_t g_flightCraftJumpingEnabled;
extern uint8_t g_savedHudCmdPanelEnabled;
extern uint8_t g_pauseState;
extern int g_flightPlayerCount;
extern int g_activeFlightPlayerCount;
extern int dtMs;
extern FILE* g_inputLogFile;
extern int g_launchTriggered;
extern uint8_t g_yardChallengeMode;
extern const uint16_t g_warheadTypeIds[11];
extern const uint16_t g_warheadAmmoCounts[12];
extern const uint8_t g_platformBeamDisabledComponentIds[60];
extern int g_unusedFlightResumeResetSlot0;
extern int g_unusedFlightResumeResetSlot1;
extern uint8_t* g_worldMessageBuffer;
extern int g_flightNetDirtyAllObjectTransformsAfterRestore;
extern int g_worldMessageBufferCapacity;
extern int g_worldMessageBufferedCount;
extern int g_worldMessageBufferBytesFree;
extern MemoryHandle g_worldMessageBufferHandle;
extern int g_flightNetWorldStateChunkAcked[16];
extern int g_flightNetWorldChecksumPeerStatus[8];
extern int g_flightNetBufferWorldMessagesUntilChecksum;
extern uint32_t g_flightNetWorldChecksumEpoch;
// 12 uint16 timers (FlightGlobalCountdownTimers, 24 bytes). Serialized whole in the
// world-state snapshot. Index 2 = crewMeshRotation, 11 = objectSpecialBehaviorUpdate.
extern uint16_t g_flightGlobalCountdownTimers[12];
extern int g_hyperspaceFxPhaseLatch;
extern uint32_t g_unusedWorldStateSerializedDword;
extern uint16_t g_simStepScale;
extern uint8_t g_flightRegionSessionGateMode;
extern uint8_t g_dormantFlightRegionSessionEarlyReturnFlag;
extern uint16_t g_unusedFlightSimStepScaleWordMirror;
extern FlightCraftModelIndex g_curCraftModelIndex;
extern int g_flightNetClockProbeTimestamp;
extern int g_flightNetPeerSilenceTicks[8];
extern int g_flightNetRecoveryUiBlinkTime;
extern int g_flightNetWorldStateAckReceivedFlag;
extern int g_singleObjectUpdateOverrideIdx;
extern uint32_t g_unusedForceFeedbackPrevSpeedSnapshotLo;
extern FlightForceFeedbackSpeedSnapshot g_forceFeedbackLocalSpeedSnapshot;
extern uint32_t g_forceFeedbackLocalSpeedSnapshotHigh;
extern uint8_t g_unusedFlightAction140ToggleFlag;
extern uint32_t g_unusedForceFeedbackPrevSpeedSnapshotHigh;
extern uint32_t g_worldChecksum[16];
extern uint32_t peerChecksum[16];
extern int g_flightNetLocalResyncChecksums[128];
extern int g_flightNetRemoteResyncChecksums[125];
extern FlightNetWorldStateChunkPacket g_flightNetWorldStateChunkPackets[16];
extern int g_flightNetRecoverySavedInputTimestamp;
extern int g_flightNetWorldChecksumResetAccumMs;
extern int g_flightNetRecoveryUiActive;
extern int g_flightNetLastInputDeltaCodeByPlayer[8];
extern int g_flightNetRemoteResyncChecksumsReceivedFlag;
extern int g_flightNetSentWorldMessageCount;
extern int g_flightNetReceivedWorldMessageCount;
extern FILE* g_flightNetServerLogFile;
extern int g_flightNetPendingAckCount;
extern int g_flightNetNextClientInputSendTimestamp;
extern int g_flightNetLastSentWorldMessageTimestamp;
extern int g_unusedFlightNetMissionStartAckInitFlag;
extern int g_serverTickTime;
extern int g_flightNetClockAdjustAccumTicks;
extern int g_flightNetHostAbortReceived;
extern int g_flightNetHostTimeoutElapsedMs;
extern int g_flightNetClockLeadAllowanceMs;
extern uint16_t g_actionKey;
extern uint16_t g_currentActionKey;
extern int g_inputTimestamp;
extern int g_flightHudUpdateElapsedTicks;
extern uint16_t g_unusedFlightViewRenderHudWord;
extern uint16_t g_flightInitialTextureCacheFlushPending;
extern uint8_t g_unusedLocalPlayerHitGlowMarksPending;
extern int g_lastFrameTime;
extern int g_lastKeyframeTime;
extern int g_inputLogEnabled;
extern uint16_t g_joystickEnabled;
extern uint16_t g_keyMods;
extern uint16_t g_flightKeyMods;
extern int g_controlMask;
extern int g_injectedKeyCount;
extern uint16_t g_injectedKeyStack[64];
extern uint16_t g_joystickDetectResultWord;
extern int g_throttleSmoothed;
extern int16_t g_ctrlAxisX;
extern int16_t g_ctrlAxisY;
extern int16_t g_ctrlAxisR;
extern int16_t g_scaledInputPitch;
extern int16_t g_scaledInputYaw;
extern int16_t g_scaledInputRoll;
extern uint16_t g_elapsedTicks;
extern int g_remotePlayerRenderSmoothingEnabled;
extern RemotePlayerRenderSample g_remotePlayerRenderSamples[8];
extern RemotePlayerRenderSample g_remotePlayerSavedRenderPoses[8];
extern FlightInputFrameRecord g_replayInputs[8];
extern int g_inputFrameCount[XWA_INPUT_HISTORY_PLAYER_COUNT];
extern InputFrame g_inputHistory[XWA_INPUT_HISTORY_PLAYER_COUNT][XWA_INPUT_HISTORY_FRAME_COUNT];
extern FlightInputFrameRecord g_currentInputFrame;
extern MemoryHandle g_worldStateDupHandle;
extern MemoryHandle g_worldStateHandle;
extern MemoryHandle g_mapRoomIconsHandle;
extern int g_unusedFlightResourceInitZero;
extern uint8_t* g_mapRoomIconsBuffer;
extern const char* g_mapRoomIconsResourcePath;
extern int g_unusedMapRoomIconCount;
extern uint8_t* g_worldStateBuffer;
extern uint8_t* g_worldStateDupBuffer;
extern int worldStateSize;
extern int g_worldStateSize;
extern const uint16_t g_subsystemIdToFlag[10];

void Flight_FreeWorldStateBuffers(void);
void Flight_AllocWorldStateBuffers(void);
uint8_t* Flight_GetDuplicateWorldStateBuffer(void);
int Flight_GetSerializedWorldStateSize(void);
int Flight_ComputeWorldStateResyncSegmentSize(int size);
int Flight_BuildWorldStateResyncSegmentChecksums(int* outChecksums, uint8_t* worldState, int worldStateSize);
int Flight_BuildWorldStateObjectPresenceMap(uint8_t* outMap, uint8_t* worldState);
int Flight_ApplyWorldStateObjectPresenceMap(const uint8_t* presenceMap);
int Flight_SaveWorldState(void);
int Flight_ChecksumWorldState(int expectedChecksum, int serverTicks);
void Flight_RestoreWorldState(void);
int Flight_CheckMissionEndAndExitRequest(void);
int Flight_ApplyConfigToRuntime(GameConfig* oldConfig, GameConfig* newConfig);
void Flight_UpdateDynamicMusicState(void);
int Flight_UpdateFighterWarnings(char resetState);
void Flight_RecomputeCraftSpeedFromPowerSettings(int objectIdx);
void Flight_SlewObjectSpeedTowardTarget(int objectIdx, int targetSpeed, int allowDecel, int fracQ16);
void Flight_ProcessPlayerActions(unsigned int playerIdx);
void Flight_UpdateEntity(unsigned int playerIdx);
void Flight_AdvanceOneStep(int targetTimestamp);
void Flight_StepSimToTime(int targetTimestamp);
#ifdef XWA_MODERN
void Flight_ModernResetHighRateIntegration(void);
/* Complete a pending nonblocking options modal before flight timing resumes. */
int Flight_ContinueOptionsModal(void);
#endif
int Flight_PumpWindowMessages(void);
int Pause_ProcessInput(void);
int sub_4D4640(void);
void Flight_UpdateActivePlayerCount(void);
void Flight_UpdateTimers(void);
int FlightInput_HasKeyReady(void);
int FlightInput_GetNextKey(void);
uint16_t FlightInput_Read(int playerIdxOrSentinel);
void FlightInput_ResetRuntimeState(void);
ObjectIndex FilmOverlay_FindNextSelectableObject(ObjectIndex currentObjIdx, int16_t step, int unusedPlayerIdx,
												 int excludedObjIdx);
void FlightObject_InitMeshAnimationDefaults(int objIdx);
void FlightObject_AnimateCrewMeshRotations(int objectIndex, int resetToNeutral);
void FlightObject_UpdateDebrisAndTransientAnimations(void);
void FlightObject_UpdateSpecialBehavior(void);
void FlightInput_ScaleAxesForFlight(void);
void FlightInput_ApplyDeadzone(void);
void Flight_AccelerateHyperspaceSpeed(int objectIdx, int acceleration);
void Flight_DecelerateHyperspaceSpeed(int objectIdx, int deceleration);
void FlightObject_UpdatePlayerHyperspaceTransition(unsigned int playerIdx);
void Flight_UpdateDivePulloutPitchTarget(int objectIdx);
void Flight_UpdateCraftSteeringAndSpeed(void);

#ifdef __cplusplus
}
#endif

#endif
