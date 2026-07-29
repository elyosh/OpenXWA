#include "xwa/flight/flight.h"
#include "xwa/flight/fediskio.h"
#include "xwa/flight/flight_debug.h"
#include "xwa/flight/flight_map.h"
#include "xwa/flight/hangar.h"

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
#include "xwa_runtime/compat/winmm/joystick.h"
#include "xwa/config/game_config.h"
#include "xwa/console/console.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/object/damage.h"
#include "xwa/flight/death_star.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/flight_light.h"
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
#include "xwa/net/net.h"
#include "xwa/flight/object/laser.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/effects.h"
#include "xwa/render/renderer.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"
#include "xwa/util/random.h"
#include "xwa/util/time.h"
#ifdef XWA_MODERN
#include "xwa_runtime/input/mouse_flight.h"
#include "xwa_runtime/timing/modern_flight_timing.h"
#endif

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef XWA_MODERN
extern void(__stdcall* g_OutputDebugStringA)(const char* outputString);
#endif

#ifdef XWA_MODERN
typedef struct ModernTurretAngleRemainder {
	uint16_t objectIdx;
	uint16_t objectSignature;
	uint8_t initialized;
	double axisA;
	double axisB;
} ModernTurretAngleRemainder;

typedef struct ModernMapCameraStepRemainder {
	int numerator;
	int direction;
} ModernMapCameraStepRemainder;

typedef struct ModernJoystickInputSample {
	int timestamp;
	int captured;
	int joystickValid;
	WinmmJoystickTraceSample bridge;
	int16_t prePackX;
	int16_t prePackY;
	int16_t prePackR;
	int8_t packedX;
	int8_t packedY;
	int8_t packedR;
} ModernJoystickInputSample;

typedef struct ModernJoystickIntegrationTrace {
	int timestamp;
	uint16_t elapsedTicks;
	int16_t scaledYaw;
	int16_t scaledPitch;
	int16_t scaledRoll;
	int targetYaw;
	int targetPitch;
	int targetRoll;
	int16_t smoothedYaw;
	int16_t smoothedPitch;
	int16_t smoothedRoll;
	int stepYaw;
	int stepPitch;
	int stepRoll;
	Q16Angle oldPitch;
	Q16Angle oldYaw;
	Q16Angle oldRoll;
	Q16Angle newPitch;
	Q16Angle newYaw;
	Q16Angle newRoll;
} ModernJoystickIntegrationTrace;

#define MODERN_JOYSTICK_TRACE_HISTORY_COUNT 16

static ModernTurretAngleRemainder g_modernTurretAngleRemainders[XWA_PLAYER_COUNT][2];
static ModernMapCameraStepRemainder g_modernMapCameraStepRemainders[XWA_PLAYER_COUNT];
static ModernJoystickInputSample g_modernJoystickInputSamples[MODERN_JOYSTICK_TRACE_HISTORY_COUNT];
static unsigned int g_modernJoystickInputSampleCursor;
static int g_modernJoystickTraceEnabled;

int FlightDebug_JoystickTraceEnabled(void) { return g_modernJoystickTraceEnabled; }

void FlightDebug_SetJoystickTraceEnabled(int enabled) {
	enabled = enabled != 0;
	if (enabled == g_modernJoystickTraceEnabled) {
		return;
	}
	g_modernJoystickTraceEnabled = enabled;
	memset(g_modernJoystickInputSamples, 0, sizeof(g_modernJoystickInputSamples));
	g_modernJoystickInputSampleCursor = 0;
	Aeron_LogInfo("xwa.input.trace", "JOYTRACE state=%s", enabled ? "on" : "off");
}

static void FlightDebug_CaptureJoystickInputSample(int timestamp) {
	ModernJoystickInputSample* sample;

	if (!g_modernJoystickTraceEnabled) {
		return;
	}

	sample = &g_modernJoystickInputSamples[g_modernJoystickInputSampleCursor];
	g_modernJoystickInputSampleCursor =
		(g_modernJoystickInputSampleCursor + 1) % MODERN_JOYSTICK_TRACE_HISTORY_COUNT;
	memset(sample, 0, sizeof(*sample));
	sample->timestamp = timestamp;
	sample->captured = 1;
	sample->joystickValid =
		g_joystickEnabled != 0 && g_joystickActive && WinmmJoystick_GetLastTraceSample(&sample->bridge);
	sample->prePackX = g_ctrlAxisX;
	sample->prePackY = g_ctrlAxisY;
	sample->prePackR = g_ctrlAxisR;
	sample->packedX = g_currentInputFrame.axisX;
	sample->packedY = g_currentInputFrame.axisY;
	sample->packedR = g_currentInputFrame.axisR;
}

static const ModernJoystickInputSample* FlightDebug_FindJoystickInputSample(int timestamp) {
	unsigned int i;

	for (i = 0; i < MODERN_JOYSTICK_TRACE_HISTORY_COUNT; ++i) {
		const ModernJoystickInputSample* sample =
			&g_modernJoystickInputSamples[(g_modernJoystickInputSampleCursor +
										   MODERN_JOYSTICK_TRACE_HISTORY_COUNT - 1 - i) %
										  MODERN_JOYSTICK_TRACE_HISTORY_COUNT];
		if (sample->captured && sample->timestamp == timestamp) {
			return sample;
		}
	}
	return NULL;
}

static void FlightDebug_LogJoystickIntegration(const ModernJoystickIntegrationTrace* trace) {
	const ModernJoystickInputSample* sample = FlightDebug_FindJoystickInputSample(trace->timestamp);
	ModernJoystickInputSample fallback;

	if (!sample) {
		memset(&fallback, 0, sizeof(fallback));
		fallback.bridge.sourceAxisX = -1;
		fallback.bridge.sourceAxisY = -1;
		fallback.bridge.sourceAxisR = -1;
		fallback.prePackX = g_ctrlAxisX;
		fallback.prePackY = g_ctrlAxisY;
		fallback.prePackR = g_ctrlAxisR;
		fallback.packedX = (int8_t)g_ctrlAxisX;
		fallback.packedY = (int8_t)g_ctrlAxisY;
		fallback.packedR = (int8_t)g_ctrlAxisR;
		sample = &fallback;
	}

	Aeron_LogInfo("xwa.input.trace",
				  "JOYTRACE ts=%d dt=%u joy=%d sdl=%d:%d,%d:%d,%d:%d wm=%u,%u,%u xwa=%d,%d,%d "
				  "pk=%d,%d,%d sc_ypr=%d,%d,%d tgt_ypr=%d,%d,%d sm_ypr=%d,%d,%d step_ypr=%d,%d,%d "
				  "ang_pyr=%u,%u,%u>%u,%u,%u",
				  trace->timestamp, (unsigned int)trace->elapsedTicks, sample->joystickValid,
				  sample->bridge.sourceAxisX, sample->bridge.sourceValueX, sample->bridge.sourceAxisY,
				  sample->bridge.sourceValueY, sample->bridge.sourceAxisR, sample->bridge.sourceValueR,
				  sample->bridge.winmmX, sample->bridge.winmmY, sample->bridge.winmmR, sample->prePackX,
				  sample->prePackY, sample->prePackR, sample->packedX, sample->packedY, sample->packedR,
				  trace->scaledYaw, trace->scaledPitch, trace->scaledRoll, trace->targetYaw,
				  trace->targetPitch, trace->targetRoll, trace->smoothedYaw, trace->smoothedPitch,
				  trace->smoothedRoll, trace->stepYaw, trace->stepPitch, trace->stepRoll,
				  (unsigned int)trace->oldPitch, (unsigned int)trace->oldYaw, (unsigned int)trace->oldRoll,
				  (unsigned int)trace->newPitch, (unsigned int)trace->newYaw, (unsigned int)trace->newRoll);
}

void Flight_ModernResetHighRateIntegration(void) {
	memset(g_modernTurretAngleRemainders, 0, sizeof(g_modernTurretAngleRemainders));
	memset(g_modernMapCameraStepRemainders, 0, sizeof(g_modernMapCameraStepRemainders));
	memset(g_modernJoystickInputSamples, 0, sizeof(g_modernJoystickInputSamples));
	g_modernJoystickInputSampleCursor = 0;
}
#endif

// GLOBAL: XWA 0x7827F4
int g_asyncFlag;
// GLOBAL: XWA 0x773334
int g_flightConfFlicker;
// GLOBAL: XWA 0x7B1CFC
char g_FlightConfRivaTxt;
// GLOBAL: XWA 0x773320
int g_flightConfTrainCourse;
// GLOBAL: XWA 0x7F5260
char g_flightConfNoPilot;
// GLOBAL: XWA 0x5FFDAC
int g_flightConfDirectInput = 1;
// GLOBAL: XWA 0x91AD30
char g_flightInputNonBlockingMsgPump;
// GLOBAL: XWA 0x91AD48
signed char g_lastKeyCode;
// GLOBAL: XWA 0x91AE60
int g_keyReady;
// GLOBAL: XWA 0x80B615
unsigned char g_flightConfSfxEnabled = 1;
// GLOBAL: XWA 0x80DC64
int g_flightConfMusicEnabled = 1;
// GLOBAL: XWA 0x7D4B68
unsigned char g_flightConfVoiceEnabled = 1;
// GLOBAL: XWA 0x7827E4
int g_flightSimSideEffectsSuppressed;
// GLOBAL: XWA 0x80B616
int16_t g_localBeamTargetObjIdx;
// GLOBAL: XWA 0x7827E8
int g_flightSfxSideEffectGate;
// GLOBAL: XWA 0x782830
int g_flightConfTickCounter;
// GLOBAL: XWA 0x782878
int inProgressLaunch;
// GLOBAL: XWA 0x917E4C
int g_flightConfNewNet;
// GLOBAL: XWA 0x773338
int g_flightConfNoLauncher;
// GLOBAL: XWA 0x5FFDA4
int g_flightFullscreen = 1;
// GLOBAL: XWA 0x5FFDA8
int g_flightPageFlip = 1;
// GLOBAL: XWA 0x91AD40
int g_flightStartedWithDashArg;
// GLOBAL: XWA 0x773328
int g_unusedFlightCmdLinePlusSwitchFlag;
// GLOBAL: XWA 0x7732E8
char* g_argProgramName;
// GLOBAL: XWA 0x7732EC
char* g_argSentinel;
// GLOBAL: XWA 0x7732F0
char* g_argMissionPath;
// GLOBAL: XWA 0x7732F4
char* sessionName;
// GLOBAL: XWA 0x7732F8
char* g_argPilotName;
// GLOBAL: XWA 0x7732FC
char* g_argLocalIdStr;
// GLOBAL: XWA 0x773300
char* g_argMpGameName;
char* g_argUnusedZeroStr;
// GLOBAL: XWA 0x773308
char* g_argNumPlayersStr;
// GLOBAL: XWA 0x6002C8
int g_flightBrightnessScaleQ8;
// GLOBAL: XWA 0x7D4B84
int g_flightSideEffectsEnabled;
// GLOBAL: XWA 0x771264
int g_fighterWarningLastScanTime;
// GLOBAL: XWA 0x771268
int g_fighterWarningTailCooldownUntil;
// GLOBAL: XWA 0x77126C
int g_fighterWarningForwardCooldownUntil;
// GLOBAL: XWA 0x771270
int g_fighterWarningUnusedState;
// GLOBAL: XWA 0x771274
int g_fighterWarningPrevNearbyHostileCount;
// GLOBAL: XWA 0x771278
int g_fighterWarningPrevForwardThreatCount;
// GLOBAL: XWA 0x77127C
int g_fighterWarningPrevTailAttackerCount;
// GLOBAL: XWA 0x910DF4
uint8_t g_backdropsEnabled;
// GLOBAL: XWA 0x7CA1E8
uint8_t g_debrisEnabled;
// GLOBAL: XWA 0x6002C4
uint16_t g_starDensity;
// GLOBAL: XWA 0x7827D4
int g_flightResolutionMode;
// GLOBAL: XWA 0x5FFDB4
int g_renderTargetWidth;
// GLOBAL: XWA 0x5FFDB8
int g_unusedFlightDisplayBytesPerPixelMirror;
// GLOBAL: XWA 0x5FFDBC
int g_unusedFlightDisplayHardware3DMirror;
// GLOBAL: XWA 0x5AA0AC
int g_flight16bppBytesPerPixel = 2;
// GLOBAL: XWA 0x91AD4C
int g_flightSoundInitStartTimeMs;
// GLOBAL: XWA 0x6002E8
char g_currentMissionFile[128];
// GLOBAL: XWA 0x8BF388
XwaFile* g_filmFile;
// GLOBAL: XWA 0x7B329C
int16_t g_filmVersion;
// GLOBAL: XWA 0x7D4C4D
uint8_t g_filmRecording;
// GLOBAL: XWA 0x7FFD6C
int g_cockpitObjectTypeForFilmHeader;
// GLOBAL: XWA 0x8C1608
uint8_t g_filmHeaderDifficulty;
// GLOBAL: XWA 0x917E44
uint8_t g_filmHeaderCollisionsEnabled;
// GLOBAL: XWA 0x80540A
uint8_t g_flightDifficulty;
// GLOBAL: XWA 0x80540B
uint8_t g_flightCollisionsEnabled;
// GLOBAL: XWA 0x7827DC
int g_sw3dSkipOddScanlines;
// GLOBAL: XWA 0x8C28E0
uint8_t g_flightReturnToFrontendRequested;
// GLOBAL: XWA 0x9C6E2C
int g_flightReturnToMissionSetupRequested;
// GLOBAL: XWA 0x8053E5
uint8_t g_provingGroundsModeActive;
// GLOBAL: XWA 0x80B604
int g_flightExitRequest;
// GLOBAL: XWA 0x80540C
uint8_t g_flightCraftJumpingEnabled;
// GLOBAL: XWA 0x80B60C
uint8_t g_pauseState;
// GLOBAL: XWA 0x910DEC
int g_flightPlayerCount;
// GLOBAL: XWA 0x8D4240
int g_activeFlightPlayerCount;
// GLOBAL: XWA 0x9C6954
int g_launchTriggered;
// GLOBAL: XWA 0x8D9740
int g_lastLocalReplayInputTimestamp;
// GLOBAL: XWA 0x8D93EC
int g_filmStepInputTimestamp;
// GLOBAL: XWA 0x6002D0
int dtMs;
// GLOBAL: XWA 0xABC96C
int g_currentCdDisk;
// GLOBAL: XWA 0x782810
int g_fpsSampleRingIndex;
// GLOBAL: XWA 0x8D9620
uint8_t g_unusedFlightStepResetFlag;
// GLOBAL: XWA 0x910DE0
int g_filmPlaybackTimestampOverrideApplied;
// GLOBAL: XWA 0x7CA174
uint8_t g_flightStepRanThisFrame;
// GLOBAL: XWA 0x808114
int g_filmPlaybackStepCatchupTicks;
// GLOBAL: XWA 0x8053E7
uint8_t g_yardChallengeMode;
// GLOBAL: XWA 0x5BA87C
int g_unusedFlightResumeResetSlot0;
// GLOBAL: XWA 0x5BA880
int g_unusedFlightResumeResetSlot1;
// GLOBAL: XWA 0x771218
uint8_t* g_worldMessageBuffer;
// GLOBAL: XWA 0x771280
int g_flightNetDirtyAllObjectTransformsAfterRestore;
// GLOBAL: XWA 0x771288
int g_worldMessageBufferCapacity;
// GLOBAL: XWA 0x60E760
uint16_t g_flightObjectAnimFrameScratch;
// GLOBAL: XWA 0x771290
int g_worldMessageBufferedCount;
// GLOBAL: XWA 0x77128C
int g_worldMessageBufferBytesFree;
// GLOBAL: XWA 0x771294
MemoryHandle g_worldMessageBufferHandle;
// GLOBAL: XWA 0x76E5E0
int g_flightNetWorldStateChunkAcked[16];
// GLOBAL: XWA 0x80AD00
int g_flightNetWorldChecksumPeerStatus[8];
// GLOBAL: XWA 0x7827EC
int g_flightNetBufferWorldMessagesUntilChecksum;
// GLOBAL: XWA 0x7827F0
uint32_t g_flightNetWorldChecksumEpoch;
// GLOBAL: XWA 0x8C1620
// FlightGlobalCountdownTimers: 12 uint16 timers (24 bytes), serialized whole in the
// world-state snapshot (Flight_SaveWorldState copies sizeof(FlightGlobalCountdownTimers)).
uint16_t g_flightGlobalCountdownTimers[12];
// GLOBAL: XWA 0x5AE038
// Last hyperspace FX phase that played one-shot sound/light/FF side effects, so
// each phase's intro effects fire only once across the per-player update sweep.
int g_hyperspaceFxPhaseLatch;
// GLOBAL: XWA 0x7CA184
uint32_t g_unusedWorldStateSerializedDword;
// GLOBAL: XWA 0x80B61C
uint16_t g_simStepScale;
// GLOBAL: XWA 0x600368
uint8_t g_flightRegionSessionGateMode;
// GLOBAL: XWA 0x91093C
uint8_t g_dormantFlightRegionSessionEarlyReturnFlag;
// GLOBAL: XWA 0x7D4B64
uint16_t g_unusedFlightSimStepScaleWordMirror;
// GLOBAL: XWA 0x6343E8
FlightCraftModelIndex g_curCraftModelIndex;
// GLOBAL: XWA 0x76E5A8
int g_flightNetClockProbeTimestamp;
// GLOBAL: XWA 0x76E5B0
int g_flightNetPeerSilenceTicks[8];
// GLOBAL: XWA 0x76E5D4
int g_flightNetRecoveryUiBlinkTime;
// GLOBAL: XWA 0x76E5D8
int g_flightNetWorldStateAckReceivedFlag;
// GLOBAL: XWA 0x8C1604
int g_singleObjectUpdateOverrideIdx = -1;
// GLOBAL: XWA 0x7CAB50
uint32_t g_unusedForceFeedbackPrevSpeedSnapshotLo;
// GLOBAL: XWA 0x7CAB40
FlightForceFeedbackSpeedSnapshot g_forceFeedbackLocalSpeedSnapshot;
// GLOBAL: XWA 0x7CAB44
uint32_t g_forceFeedbackLocalSpeedSnapshotHigh;
// GLOBAL: XWA 0x91094E
uint8_t g_unusedFlightAction140ToggleFlag;
// GLOBAL: XWA 0x7CAB54
uint32_t g_unusedForceFeedbackPrevSpeedSnapshotHigh;
// GLOBAL: XWA 0x771220
uint32_t g_worldChecksum[16];
// GLOBAL: XWA 0x770ED8
uint32_t peerChecksum[16];
// GLOBAL: XWA 0x76E620
int g_flightNetLocalResyncChecksums[128];
// GLOBAL: XWA 0x76EC60
int g_flightNetRemoteResyncChecksums[125];
// GLOBAL: XWA 0x76EE60
FlightNetWorldStateChunkPacket g_flightNetWorldStateChunkPackets[16];
// GLOBAL: XWA 0x76EA24
int g_flightNetRecoverySavedInputTimestamp;
// GLOBAL: XWA 0x76EA28
int g_flightNetWorldChecksumResetAccumMs;
// GLOBAL: XWA 0x76EA30
int g_flightNetRecoveryUiActive;
// GLOBAL: XWA 0x76EC38
int g_flightNetLastInputDeltaCodeByPlayer[8];
// GLOBAL: XWA 0x770E64
int g_flightNetRemoteResyncChecksumsReceivedFlag;
// GLOBAL: XWA 0x770E68
int g_flightNetSentWorldMessageCount;
// GLOBAL: XWA 0x770E6C
int g_flightNetReceivedWorldMessageCount;
// GLOBAL: XWA 0x770E78
FILE* g_flightNetServerLogFile;
// GLOBAL: XWA 0x770E80
int g_flightNetPendingAckCount;
// GLOBAL: XWA 0x770E84
int g_flightNetNextClientInputSendTimestamp;
// GLOBAL: XWA 0x770E88
int g_flightNetLastSentWorldMessageTimestamp;
// GLOBAL: XWA 0x770E8C
int g_unusedFlightNetMissionStartAckInitFlag;
// GLOBAL: XWA 0x910948
int g_serverTickTime;
// GLOBAL: XWA 0x7827E0
int g_flightNetClockAdjustAccumTicks;
// GLOBAL: XWA 0x7827F8
int g_flightNetHostAbortReceived;
// GLOBAL: XWA 0x7D4B88
int g_flightNetHostTimeoutElapsedMs;
// GLOBAL: XWA 0x8C28DC
int g_flightNetClockLeadAllowanceMs;
// GLOBAL: XWA 0x7D4B6A
uint16_t g_actionKey;
// GLOBAL: XWA 0x8053C0
uint16_t g_currentActionKey;
// GLOBAL: XWA 0x7CA3A4
int g_flightHudUpdateElapsedTicks;
// GLOBAL: XWA 0x8B94DC
uint16_t g_unusedFlightViewRenderHudWord;
// GLOBAL: XWA 0x7FBB68
uint16_t g_flightInitialTextureCacheFlushPending;
// GLOBAL: XWA 0x770ECC
uint8_t g_unusedLocalPlayerHitGlowMarksPending;
// GLOBAL: XWA 0x7D4B8C
int g_inputTimestamp;
// GLOBAL: XWA 0x91AB84
uint16_t g_joystickEnabled;

// GLOBAL: XWA 0x5AE1F0
const uint8_t g_subsystemMessageArgById[10] = {
	MSG_ENGINE,   MSG_FLIGHTCONTROL, MSG_LASER,          MSG_SHIELDS,        MSG_TARGCOMPUTER,
	MSG_LAUNCHER, MSG_BEAM,          MSG_COMMUNICATIONS, MSG_COUNTERMEASURE, MSG_HYPERDRIVE,
};

// GLOBAL: XWA 0x5B12A8
const uint16_t g_warheadTypeIds[11] = {
	0u,
	OBJ_WarheadSpaceBomb,
	OBJ_WarheadRocket,
	OBJ_WarheadMissile,
	OBJ_WarheadTorpedo,
	OBJ_WarheadAdvancedMissile,
	OBJ_WarheadAdvancedTorpedo,
	OBJ_WarheadMagPulse,
	OBJ_WarheadIonPulse,
	OBJ_WarheadMissile,
	OBJ_WarheadMissile,
};

// GLOBAL: XWA 0x5B12C0
// Q16 ammo-fraction scalers indexed by warhead type (FG.warhead), applied to a
// launcher's per-slot value via MATH2_fraction.
const uint16_t g_warheadAmmoCounts[12] = {
	0, 0x4000, 0x8000, 0xFFFF, 0xC000, 0xFFFF, 0xC000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0,
};

// GLOBAL: XWA 0x5B12F8
// Per-platform (OBJ_Platform1..5) component ids disabled when a beam weapon is mounted;
// 0xFF terminates / marks an empty slot. Beam type 1 disables the first 6, others all 12.
const uint8_t g_platformBeamDisabledComponentIds[60] = {
	22,   23,   21,   20,   19,   5,    15, 16, 17,   18,   24,   6,    3,    5,    27,
	9,    10,   0xFF, 1,    4,    26,   7,  8,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 5,  13, 15,   19,   20,   24,   11,   14,   6,
	17,   18,   23,   25,   26,   27,   28, 29, 0xFF, 21,   22,   23,   24,   30,   0xFF,
};

// GLOBAL: XWA 0x5AE1D8
const uint16_t g_subsystemIdToFlag[10] = {
	0x0040u, 0x0020u, 0x0010u, 0x0001u, 0x0004u, 0x0008u, 0x0100u, 0x0200u, 0x0002u, 0x0080u,
};

static const uint8_t g_throttleKeyByBucket[17] = {
	0x08u, 0xe7u, 0xe6u, 0xe5u, 0xe4u, 0x5du, 0xe3u, 0xe2u, 0xe1u,
	0xe0u, 0xdfu, 0x5bu, 0xdeu, 0xddu, 0xdcu, 0xdbu, 0x5cu,
};

// GLOBAL: XWA 0x91AB86
uint16_t g_keyMods;
// GLOBAL: XWA 0x8BF390
uint16_t g_flightKeyMods;
// GLOBAL: XWA 0x63CF6C
int g_controlMask;
// GLOBAL: XWA 0x63CF74
int g_injectedKeyCount;
// GLOBAL: XWA 0x9E9000
uint16_t g_injectedKeyStack[64];
// GLOBAL: XWA 0x80540F
uint8_t g_flightLocatePlayersEnabled;
// GLOBAL: XWA 0x910E20
uint16_t g_joystickDetectResultWord;
// GLOBAL: XWA 0x63CF70
int g_throttleSmoothed;
// GLOBAL: XWA 0x8D93D4
int16_t g_ctrlAxisX;
// GLOBAL: XWA 0x8D93DC
int16_t g_ctrlAxisY;
// GLOBAL: XWA 0x8D6BAE
int16_t g_ctrlAxisR;
// GLOBAL: XWA 0x8C1CC0
int16_t g_scaledInputPitch;
// GLOBAL: XWA 0x8C1CC2
int16_t g_scaledInputYaw;
// GLOBAL: XWA 0x8C1CC4
int16_t g_scaledInputRoll;
// GLOBAL: XWA 0x8C1640
// 16-bit per-frame tick delta. The binary only ever accesses it as a word; the
// adjacent 2 bytes at 0x8C1642 are unreferenced padding.
uint16_t g_elapsedTicks;
// GLOBAL: XWA 0x6002D4
int g_remotePlayerRenderSmoothingEnabled;
// GLOBAL: XWA 0x770F18
RemotePlayerRenderSample g_remotePlayerRenderSamples[8];
// GLOBAL: XWA 0x771098
RemotePlayerRenderSample g_remotePlayerSavedRenderPoses[8];
// GLOBAL: XWA 0x7CA360
FlightInputFrameRecord g_replayInputs[8];
// GLOBAL: XWA 0x7D4BE0
int g_inputFrameCount[XWA_INPUT_HISTORY_PLAYER_COUNT];
// GLOBAL: XWA 0x8C2900
InputFrame g_inputHistory[XWA_INPUT_HISTORY_PLAYER_COUNT][XWA_INPUT_HISTORY_FRAME_COUNT];
// GLOBAL: XWA 0x91ACB0
FlightInputFrameRecord g_currentInputFrame;
// GLOBAL: XWA 0x77121C
MemoryHandle g_worldStateDupHandle;
// GLOBAL: XWA 0x771260
MemoryHandle g_worldStateHandle;
// GLOBAL: XWA 0x91094C
MemoryHandle g_mapRoomIconsHandle;
// GLOBAL: XWA 0x910940
int g_unusedFlightResourceInitZero;
// GLOBAL: XWA 0x8C1CF0
uint8_t* g_mapRoomIconsBuffer;
// GLOBAL: XWA 0x8BF394
const char* g_mapRoomIconsResourcePath;
// GLOBAL: XWA 0x7D4F90
int g_unusedMapRoomIconCount;
// GLOBAL: XWA 0x91AE70
uint8_t* g_worldStateBuffer;
// GLOBAL: XWA 0x91AE6C
uint8_t* g_worldStateDupBuffer;
// GLOBAL: XWA 0x91AE74
int worldStateSize;
// GLOBAL: XWA 0x91AE78
int g_worldStateSize;

// GLOBAL: XWA 0x770E60
int g_lastFrameTime;
// GLOBAL: XWA 0x76EA20
int g_lastKeyframeTime;
// GLOBAL: XWA 0x770E70
int g_inputLogEnabled;
// GLOBAL: XWA 0x770E74
FILE* g_inputLogFile;

// FUNCTION: XWA 0x509040
int Flight_ApplyConfigToRuntime(GameConfig* oldConfig, GameConfig* newConfig) {
	unsigned int ffStrength;
	unsigned int ffCenterStrength;
	int musicWasStarted;
	int boundFlightGroupIdx;
	uint8_t oldFfEnabled;
	uint8_t ffEnabled;

	if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH || !g_pilotData.campaignMode) {
		g_flightCollisionsEnabled = newConfig->collisions;
	} else {
		g_flightCollisionsEnabled = newConfig->tourCollisions;
	}

	if (g_flightPlayerCount == 1 && g_pilotData.missionDirectoryId != MISSION_DIRECTORY_SKIRMISH) {
		if (g_pilotData.campaignMode) {
			if (newConfig->tourInvulnerable) {
				boundFlightGroupIdx = g_players[g_localPlayer].boundFlightGroupIdx;
				g_missionFlightGroups[boundFlightGroupIdx].fg.status1 = 20;
			}
			if (newConfig->tourUnlimitedAmmo) {
				boundFlightGroupIdx = g_players[g_localPlayer].boundFlightGroupIdx;
				g_missionFlightGroups[boundFlightGroupIdx].fg.status2 = 21;
			}
		} else {
			if (newConfig->invulnerable) {
				boundFlightGroupIdx = g_players[g_localPlayer].boundFlightGroupIdx;
				g_missionFlightGroups[boundFlightGroupIdx].fg.status1 = 20;
			}
			if (newConfig->unlimitedAmmo) {
				boundFlightGroupIdx = g_players[g_localPlayer].boundFlightGroupIdx;
				g_missionFlightGroups[boundFlightGroupIdx].fg.status2 = 21;
			}
		}
	}

	g_players[0].throttlePreset[0] = (int16_t)(0xffffu * (unsigned int)newConfig->presetThrottle[0] / 100u);
	g_players[0].laserPreset[0] = newConfig->presetLaser[0];
	g_players[0].shieldPreset[0] = newConfig->presetShield[0];
	g_players[0].beamPreset[0] = newConfig->presetBeam[0];
	g_players[0].throttlePreset[1] = (int16_t)(0xffffu * (unsigned int)newConfig->presetThrottle[1] / 100u);
	g_players[0].laserPreset[1] = newConfig->presetLaser[1];
	g_players[0].shieldPreset[1] = newConfig->presetShield[1];
	g_players[0].beamPreset[1] = newConfig->presetBeam[1];

	musicWasStarted = 0;
	if (oldConfig->musicEnabled) {
		if (!newConfig->musicEnabled) {
			Music_Stop();
		}
	} else if (newConfig->musicEnabled) {
		musicWasStarted = 1;
	}

	if (newConfig->musicEnabled) {
		Music_SetState(g_selectedMusicState);
		Music_Update();
		if (musicWasStarted == 1 || oldConfig->musicVolume != newConfig->musicVolume) {
			if (newConfig->musicVolume == 10) {
				Music_SetVolume(127);
			} else {
				Music_SetVolume(13 * newConfig->musicVolume);
			}
		}
	} else {
		Music_Stop();
	}

	g_hudColors[0] = (uint32_t)FrontendColor_GetIndexed(newConfig->hudColor[g_flightPlayerCount > 1]);
	g_hudColors[1] = g_hudColors[newConfig->hudColor[g_flightPlayerCount > 1] + 2];

	g_backdropsEnabled = newConfig->backdrop[g_flightPlayerCount > 1];
	g_debrisEnabled = newConfig->debris[g_flightPlayerCount > 1];

	switch (newConfig->starDensity[g_flightPlayerCount > 1]) {
		case 0:
			g_starDensity = 4;
			break;
		case 1:
			g_starDensity = 2;
			break;
		case 2:
			g_starDensity = 1;
			break;
	}

	switch (newConfig->localLights[g_flightPlayerCount > 1]) {
		case 0:
			g_localLightsLevel = 0;
			break;
		case 1:
			g_localLightsLevel = 1;
			break;
		case 2:
			g_localLightsLevel = 2;
			break;
	}

	if (!newConfig->use3dHardware[g_flightPlayerCount > 1]) {
		g_specularEnabled = newConfig->specular[g_flightPlayerCount > 1] != 0;
	} else {
		g_specularEnabled = 0;
	}
	g_dirLightingEnabled = newConfig->diffuse[g_flightPlayerCount > 1] != 0;

	if (!(uint16_t)Renderer_IsTextureClampSupported() || !g_gameConfig.hitEffects[g_flightPlayerCount > 1]) {
		g_hitEffectsEnabled = 0;
	} else {
		g_hitEffectsEnabled = 1;
	}
	g_particleEffectsEnabled = g_gameConfig.particleEffects[g_flightPlayerCount > 1];
	g_trailsEnabled = g_gameConfig.trails[g_flightPlayerCount > 1];

	oldFfEnabled = oldConfig->ffEnabled;
	ffEnabled = newConfig->ffEnabled;
	if (oldFfEnabled && !ffEnabled) {
		ForceFeedback_ShutdownDevice();
	} else if (ffEnabled && !oldFfEnabled) {
		ForceFeedback_Init();
	}

	ffStrength = 1250u * (unsigned int)newConfig->ffStrength;
	if (ffStrength > 10000u) {
		ffStrength = 10000u;
	}
	ffCenterStrength = 1250u * (unsigned int)newConfig->ffCenter;
	if (ffCenterStrength > 10000u) {
		ffCenterStrength = 10000u;
	}
	ForceFeedback_SetCenteringStrength(ffCenterStrength);
	ForceFeedback_SetStrength(ffStrength);
	return 1;
}

// FUNCTION: XWA 0x4F4480
int Flight_UpdateFighterWarnings(char resetState) {
	int result;
	int playerObjIdx;

	if (resetState) {
		g_fighterWarningLastScanTime = 0;
		g_fighterWarningTailCooldownUntil = 0;
		g_fighterWarningForwardCooldownUntil = 0;
		g_fighterWarningUnusedState = 0;
		g_fighterWarningPrevNearbyHostileCount = 0;
		g_fighterWarningPrevForwardThreatCount = 0;
		g_fighterWarningPrevTailAttackerCount = 0;
		return 0;
	}

	result = g_flightSideEffectsEnabled;
	if (!g_flightSideEffectsEnabled) {
		return result;
	}

	result = g_fighterWarningLastScanTime;
	if (g_fighterWarningLastScanTime <= g_gameTime && g_gameTime - g_fighterWarningLastScanTime < 118) {
		return result;
	}

	playerObjIdx = g_players[g_localPlayer].objectIndex;
	if (playerObjIdx == 0xffff) {
		return result;
	}

	if (g_fighterWarningLastScanTime < 236 && g_gameTime > 236) {
		uint32_t scanObjIdx;

		for (scanObjIdx = g_activeRegionObjectSlotStart; scanObjIdx < g_activeRegionCraftObjectSlotEnd;
			 ++scanObjIdx) {
			if ((g_objectTable[scanObjIdx].genusId == GENUS_Fighter ||
				 g_objectTable[scanObjIdx].genusId == GENUS_Transport ||
				 g_objectTable[scanObjIdx].genusId == GENUS_Freighter ||
				 g_objectTable[scanObjIdx].genusId == GENUS_Container ||
				 g_objectTable[scanObjIdx].genusId == GENUS_Starship ||
				 g_objectTable[scanObjIdx].genusId == GENUS_Platform) &&
				Object_IsHostileToTeam(scanObjIdx, (uint16_t)g_players[g_localPlayer].playerIff)) {
				fsfx_speakorderack(g_localPlayer, -1, 30, -1, 0xffffu, 0x8000u);
				break;
			}
		}
	}

	g_fighterWarningLastScanTime = g_gameTime;
	{
		int pendingWarningKind;
		uint32_t candidateObjIdx;
		int nearbyHostileCount;
		int tailAttackerCount;
		int forwardThreatCount;

		pendingWarningKind = -1;
		nearbyHostileCount = 0;
		forwardThreatCount = 0;
		tailAttackerCount = 0;
		candidateObjIdx = g_activeRegionObjectSlotStart;
		if (candidateObjIdx < g_activeRegionCraftObjectSlotEnd) {
			do {
				if (g_objectTable[candidateObjIdx].playerOwnerIdx != g_localPlayer &&
					g_objectTable[candidateObjIdx].objectType != OBJ_None &&
					g_objectTable[candidateObjIdx].mobj->state == 0) {
					if (g_missionFlightGroups[g_objectTable[candidateObjIdx].flightGroupIdx].fg.globalUnit ==
						g_missionFlightGroups[g_objectTable[playerObjIdx].flightGroupIdx].fg.globalUnit) {
						uint32_t turretScanObjIdx;

						for (turretScanObjIdx = g_activeRegionObjectSlotStart;
							 turretScanObjIdx < g_activeRegionCraftObjectSlotEnd; ++turretScanObjIdx) {
							ObjectRecord* turretObj;

							turretObj = &g_objectTable[turretScanObjIdx];
							if (turretObj->objectType != OBJ_None && turretObj->mobj->state == 0 &&
								turretObj->genusId == GENUS_Starship) {
								CraftData* turretCraft;

								turretCraft = turretObj->mobj->pCraft;
								if (turretCraft->workingSubsystems != 0 && turretCraft->objectKind == 0 &&
									g_missionFlightGroups[turretObj->flightGroupIdx].fg.status1 != 5) {
									int laserSlotIdx;
									WarheadInventoryEntry* weapon;

									weapon = turretCraft->warheadData;
									for (laserSlotIdx = 0; laserSlotIdx < turretCraft->laserSlotCount;
										 ++laserSlotIdx, ++weapon) {
										if (weapon->weaponType >= 4u &&
											(uint16_t)weapon->turretTargetObjIdx == candidateObjIdx &&
											turretCraft->componentHp[g_modelDefs[turretCraft->modelIndex]
																		 .weaponHardpoints[laserSlotIdx]
																		 .meshIdx] != 0) {
											fsfx_speakorderack(g_localPlayer, (int)candidateObjIdx, 4, -1,
															   0xffffu, 0xccccu);
										}
									}
								}
							}
						}
					} else if (g_objectTable[candidateObjIdx].genusId == GENUS_Fighter &&
							   Object_IsHostileToTeam(candidateObjIdx,
													  (uint16_t)g_players[g_localPlayer].playerIff)) {
						if (g_objectTable[candidateObjIdx].mobj != NULL &&
							g_objectTable[candidateObjIdx].mobj->pCraft != NULL &&
							g_objectTable[candidateObjIdx].mobj->pCraft->workingSubsystems != 0 &&
							g_objectTable[candidateObjIdx].mobj->pCraft->objectKind == 0) {
							int dx;
							int dy;
							int dz;
							unsigned int distance;

							dx = g_objectTable[candidateObjIdx].world_x - g_objectTable[playerObjIdx].world_x;
							dy = g_objectTable[candidateObjIdx].world_y - g_objectTable[playerObjIdx].world_y;
							dz = g_objectTable[candidateObjIdx].world_z - g_objectTable[playerObjIdx].world_z;
							distance = (unsigned int)collide_roughdistance3d(dx, dy, dz);
							if (distance < 0x18000u) {
								++nearbyHostileCount;
								if (distance > 0x200u && g_objectTable[playerObjIdx].mobj != NULL) {
									int dx256;
									int dy256;
									int dz256;
									int dotPlayerForward;
									int threshold;

									dx256 = dx >> 8;
									dy256 = dy >> 8;
									dz256 = dz >> 8;
									trig2_ctop(dx256, dy256, dz256);
									dotPlayerForward = dx256 * g_objectTable[playerObjIdx].mobj->cachedFwdX +
													   dy256 * g_objectTable[playerObjIdx].mobj->cachedFwdY +
													   dz256 * g_objectTable[playerObjIdx].mobj->cachedFwdZ;
									threshold = 27852 * trig2_polardistance;
									if (dotPlayerForward > threshold) {
										++forwardThreatCount;
										if (g_fighterWarningForwardCooldownUntil < g_gameTime &&
											pendingWarningKind != 1) {
											pendingWarningKind = 2;
											g_fighterWarningForwardCooldownUntil = g_gameTime + 14160;
										}
									} else if (dotPlayerForward < -threshold && distance < 0x10000u) {
										MobileObject* candidateMobj;
										CraftData* candidateCraft;
										int dotCandidateForward;

										candidateMobj = g_objectTable[candidateObjIdx].mobj;
										if (candidateMobj != NULL) {
											candidateCraft = candidateMobj->pCraft;
											if (candidateCraft != NULL &&
												candidateCraft->aiController.targetObjIdx ==
													(uint16_t)g_players[g_localPlayer].objectIndex) {
												dotCandidateForward = dx256 * candidateMobj->cachedFwdX +
																	  dy256 * candidateMobj->cachedFwdY +
																	  dz256 * candidateMobj->cachedFwdZ;
												if (-dotCandidateForward > threshold) {
													++tailAttackerCount;
													if (g_fighterWarningTailCooldownUntil < g_gameTime) {
														pendingWarningKind = 1;
														g_fighterWarningTailCooldownUntil = g_gameTime + 7080;
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
				++candidateObjIdx;
			} while (candidateObjIdx < g_activeRegionCraftObjectSlotEnd);
		}

		result = nearbyHostileCount;
		if ((result <= 0 || g_fighterWarningPrevNearbyHostileCount != 0) &&
			(pendingWarningKind != 1 || g_fighterWarningPrevTailAttackerCount >= tailAttackerCount)) {
			pendingWarningKind = -1;
		}

		g_fighterWarningPrevTailAttackerCount = tailAttackerCount;
		g_fighterWarningPrevNearbyHostileCount = result;
		g_fighterWarningPrevForwardThreatCount = forwardThreatCount;
		if (pendingWarningKind != -1) {
			DebugPrintfChannel(0x4000, "Triggering fighter warning %d.\n", pendingWarningKind);
			return fsfx_speakorderack(g_localPlayer, -1, 25, pendingWarningKind,
									  (unsigned int)g_players[g_localPlayer].objectIndex, 0xffffu);
		}
	}

	return result;
}

// FUNCTION: XWA 0x4F4A70
void Flight_UpdateDynamicMusicState(void) {
	int localPlayerIdx;
	int selectedState;
	uint32_t nearestHostileRangeScore;
	uint32_t nearestHeavyHostileRangeScore;
	int nearestHostileObjIdx;
	uint16_t sideStrength[4];
	uint32_t objIdx;
	int localPlayerIff;
	uint32_t engagementRangeThreshold;
	uint32_t warheadObjIdx;
	int playerObjIdx;
	int completedGoalCount;
	int failedGoalCount;
	CraftData* playerCraft;
	int flightGroupIdx;
	int goalIdx;

	if (!g_musicInitialized) {
		return;
	}
	if (!g_gameConfig.musicEnabled) {
		return;
	}

	localPlayerIdx = g_localPlayer;
	if (g_playerFlightTransientTimers[localPlayerIdx].dynamicMusicCooldown) {
		return;
	}

	g_playerFlightTransientTimers[localPlayerIdx].dynamicMusicCooldown = 59;
	if (g_provingGroundsModeActive) {
		YardPlayerChallengeState savedWorldState;

		savedWorldState = g_yardContext.playerChallengeStates[localPlayerIdx];
		if (g_inHangarReady) {
			selectedState = MUSIC_STATE_NO_ENEMIES_CALM;
		} else if (savedWorldState.finished) {
			selectedState = MUSIC_STATE_MISSION_SUCCESS;
		} else if (savedWorldState.remainingCheckpointCount < 30) {
			selectedState = MUSIC_STATE_1145;
		} else if (!savedWorldState.courseState) {
			selectedState = MUSIC_STATE_NO_ENEMIES_CALM;
		} else if (savedWorldState.courseState == 2) {
			selectedState = MUSIC_STATE_PANIC;
		} else if (savedWorldState.courseState == 5) {
			selectedState = MUSIC_STATE_PANIC;
		} else if (savedWorldState.ringCheckpointHit || savedWorldState.carriedObjectPickedUp) {
			selectedState = MUSIC_STATE_COMBAT_ACTIVE;
		} else {
			selectedState = MUSIC_STATE_COMBAT_STEADY;
		}
	} else if (g_playerFlightTransientTimers[localPlayerIdx].missionLossMusicTimer) {
		selectedState = g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT
							? MUSIC_STATE_MISSION_LOSS_ALT
							: MUSIC_STATE_1155;
		if (g_currentMusicState != selectedState) {
			DebugPrintfChannel(256, "Music state set to %d, Mission Loss.\n", selectedState);
		}
	} else if (g_playerFlightTransientTimers[localPlayerIdx].missionSuccessMusicTimer) {
		selectedState = g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT
							? MUSIC_STATE_MISSION_SUCCESS_ALT
							: MUSIC_STATE_MISSION_SUCCESS;
		if (g_currentMusicState != selectedState) {
			DebugPrintfChannel(256, "Music state set to %d, Mission Success.\n", selectedState);
		}
	} else {
		nearestHeavyHostileRangeScore = 0xffffffffu;
		nearestHostileRangeScore = 0xffffffffu;
		nearestHostileObjIdx = 0xffff;
		memset(sideStrength, 0, sizeof(sideStrength));

		for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
			MobileObject* mobj;
			CraftData* craft;
			int isHostile;
			ModelGenusId genusId;

			if (g_objectTable[objIdx].objectType == OBJ_None) {
				continue;
			}

			mobj = g_objectTable[objIdx].mobj;
			if (mobj == NULL) {
				continue;
			}

			craft = mobj->pCraft;
			if (craft == NULL || craft->objectKind) {
				continue;
			}

			isHostile =
				Object_IsHostileToTeam((uint16_t)objIdx, (uint16_t)g_players[localPlayerIdx].playerIff) != 0;
			genusId = g_objectTable[objIdx].genusId;
			if (genusId == GENUS_Starship || genusId == GENUS_Platform) {
				sideStrength[isHostile] += 4;
			} else if (genusId == GENUS_Transport || genusId == GENUS_Container ||
					   genusId == GENUS_Freighter) {
				sideStrength[isHostile] += 2;
			} else {
				++sideStrength[isHostile];
			}

			if (!isHostile || !craft->workingSubsystems) {
				continue;
			}

			localPlayerIdx = g_localPlayer;
			if (g_players[localPlayerIdx].objectIndex == 0xffff) {
				continue;
			}

			pai_ObjectRefUpdateApproxRangeScore(objIdx, g_players[localPlayerIdx].objectIndex);
			if ((uint32_t)g_targetRangeScore < nearestHostileRangeScore) {
				nearestHostileRangeScore = (uint32_t)g_targetRangeScore;
				nearestHostileObjIdx = (int)objIdx;
			}

			if (genusId == GENUS_Starship || genusId == GENUS_Platform || genusId == GENUS_Container ||
				genusId == GENUS_Freighter) {
				nearestHeavyHostileRangeScore = (uint32_t)g_targetRangeScore;
			}
		}

		localPlayerIff = (uint16_t)g_players[localPlayerIdx].playerIff;
		if (nearestHostileObjIdx == 0xffff) {
			if (g_missionFlightRuntimeState.teamGoalStatus[localPlayerIff][TEAM_GOAL_PRIMARY] == 1) {
				selectedState = g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT
									? MUSIC_STATE_NO_ENEMIES_CALM_ALT
									: MUSIC_STATE_NO_ENEMIES_CALM;
				if (g_currentMusicState != selectedState) {
					DebugPrintfChannel(256, "Music state set to %d, Mission Completed, no enemies.\n",
									   selectedState);
				}
			} else if (!g_musicCombatSeen) {
				selectedState = g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT
									? MUSIC_STATE_NO_ENEMIES_INTRO_ALT
									: MUSIC_STATE_NO_ENEMIES_INTRO;
				if (g_currentMusicState != selectedState) {
					DebugPrintfChannel(256, "Music state set to %d, Intro, no enemies.\n", selectedState);
				}
			} else {
				selectedState = g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT
									? MUSIC_STATE_NO_ENEMIES_CALM_ALT
									: MUSIC_STATE_NO_ENEMIES_CALM;
				if (g_currentMusicState != selectedState) {
					DebugPrintfChannel(256, "Music state set to %d, Waiting, no enemies.\n", selectedState);
				}
			}
		} else {
			engagementRangeThreshold = g_musicCombatSeen ? 0x100000u : 0x80000u;
			if (nearestHostileRangeScore > engagementRangeThreshold &&
				nearestHeavyHostileRangeScore > engagementRangeThreshold) {
				selectedState = g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT
									? MUSIC_STATE_NO_ENEMIES_INTRO_ALT
									: MUSIC_STATE_NO_ENEMIES_INTRO;
				if (g_currentMusicState != selectedState) {
					DebugPrintfChannel(256, "Music state set to %d, Intro.\n", selectedState);
				}
			} else {
				g_musicCombatSeen = 1;
				for (warheadObjIdx = g_projectileObjectSlotStart; warheadObjIdx < g_projectileObjectSlotEnd;
					 ++warheadObjIdx) {
					MobileObject* mobj;

					mobj = g_objectTable[warheadObjIdx].mobj;
					if (mobj != NULL && mobj->pWarheadGuidance->homingTier != 0 &&
						mobj->pWarheadGuidance->targetObjIdx ==
							(uint16_t)g_players[localPlayerIdx].objectIndex) {
						break;
					}
				}

				if (warheadObjIdx != g_projectileObjectSlotEnd) {
					selectedState = g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT
										? MUSIC_STATE_PANIC_ALT
										: MUSIC_STATE_PANIC;
					if (g_currentMusicState != selectedState) {
						DebugPrintfChannel(256, "Music state set to %d, Targeted by missile.\n",
										   selectedState);
					}
				} else if (nearestHostileRangeScore > 0x18000u && nearestHeavyHostileRangeScore > 0x18000u) {
					selectedState = MUSIC_STATE_CONFLICT;
					if (g_currentMusicState != selectedState) {
						DebugPrintfChannel(256, "Music state set to %d, Conflict.\n", selectedState);
					}
				} else {
					int numFlightGroups;

					numFlightGroups = (int16_t)g_missionHeader.numFlightGroups;
					completedGoalCount = 0;
					failedGoalCount = 0;
					flightGroupIdx = 0;
					if (numFlightGroups > 0) {
						localPlayerIff = (uint16_t)g_players[g_localPlayer].playerIff;
						do {
							XwaGoalFG* goal;
							uint8_t* enabledForTeam;
							int goalSlotsRemaining;

							goal = g_missionFlightGroups[flightGroupIdx].fg.fgGoals;
							enabledForTeam = &goal->payload.enabledForTeam[localPlayerIff];
							goalIdx = 0;
							goalSlotsRemaining = 8;
							do {
								if (*enabledForTeam != 0 && goal->payload.argument != 1) {
									uint8_t goalState;

									goalState = g_missionFgStats[flightGroupIdx]
													.goalState[8 * localPlayerIff + goalIdx];
									if (goalState == 1) {
										++completedGoalCount;
									} else if (goalState == 4) {
										++failedGoalCount;
									}
								}
								++goalIdx;
								enabledForTeam += sizeof(*goal);
								++goal;
								--goalSlotsRemaining;
							} while (goalSlotsRemaining);
							flightGroupIdx = (uint16_t)(flightGroupIdx + 1);
						} while ((uint16_t)flightGroupIdx < numFlightGroups);
					}
					(void)completedGoalCount;

					playerCraft = NULL;
					playerObjIdx = g_players[g_localPlayer].objectIndex;
					if (playerObjIdx != 0xffff) {
						MobileObject* playerMobj;

						playerMobj = g_objectTable[playerObjIdx].mobj;
						if (playerMobj != NULL) {
							playerCraft = playerMobj->pCraft;
						}
					}

					{
						int shieldFront;
						int shieldRear;
						int shieldTotal;

						shieldFront = playerCraft->shieldFront;
						shieldRear = playerCraft->shieldRear;
						shieldTotal = shieldFront + shieldRear;
						if (shieldTotal < 1000 &&
							(uint32_t)playerCraft->hullDamage <
								MATH2_longfraction((uint32_t)playerCraft->hullMax, 0x8000u)) {
							selectedState = g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT
												? MUSIC_STATE_PANIC_ALT
												: MUSIC_STATE_PANIC;
							if (g_currentMusicState != selectedState) {
								DebugPrintfChannel(256, "Music state set to %d, Almost dead: panic!\n",
												   selectedState);
							}
						} else if (nearestHeavyHostileRangeScore < 0x8000u) {
							selectedState = g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT
												? MUSIC_STATE_PANIC_ALT
												: MUSIC_STATE_PANIC;
							if (g_currentMusicState != selectedState) {
								DebugPrintfChannel(256, "Music state set to %d, Starship: panic!\n",
												   selectedState);
							}
						} else {
							if (failedGoalCount == 1 &&
								(localPlayerIff = (uint16_t)g_players[g_localPlayer].playerIff,
								 g_missionFlightRuntimeState
										 .teamGoalStatus[localPlayerIff][TEAM_GOAL_PRIMARY] != 2) &&
								g_missionFlightRuntimeState
										.teamGoalStatus[localPlayerIff][TEAM_GOAL_SECONDARY] != 1) {
								selectedState = g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT
													? MUSIC_STATE_CLIMAX
													: MUSIC_STATE_1145;
								if (g_currentMusicState != selectedState) {
									DebugPrintfChannel(256, "Music state set to %d, Climax.\n",
													   selectedState);
								}
							} else if (sideStrength[0] >= sideStrength[1]) {
								selectedState = g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT
													? MUSIC_STATE_COMBAT_ACTIVE_ALT
													: MUSIC_STATE_COMBAT_ACTIVE;
								if (g_currentMusicState != selectedState) {
									DebugPrintfChannel(256,
													   "Music state set to %d, outnumbering, confident.\n",
													   selectedState);
								}
							} else {
								uint16_t friendlyHostileRatioQ16;

								friendlyHostileRatioQ16 =
									(uint16_t)MATH2_divide(sideStrength[0], sideStrength[1]);
								if (friendlyHostileRatioQ16 >= 0xe000u) {
									selectedState = g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT
														? MUSIC_STATE_COMBAT_ACTIVE_ALT
														: MUSIC_STATE_COMBAT_ACTIVE;
									if (g_currentMusicState != selectedState) {
										DebugPrintfChannel(
											256, "Music state set to %d, not good, but confident.\n",
											selectedState);
									}
								} else if (friendlyHostileRatioQ16 >= 0x8000u) {
									selectedState = g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT
														? MUSIC_STATE_COMBAT_STEADY_ALT
														: MUSIC_STATE_COMBAT_STEADY;
									if (g_currentMusicState != selectedState) {
										DebugPrintfChannel(
											256, "Music state set to %d, outnumbered, challenged.\n",
											selectedState);
									}
								} else {
									selectedState = g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT
														? MUSIC_STATE_PANIC_ALT
														: MUSIC_STATE_PANIC;
									if (g_currentMusicState != selectedState) {
										DebugPrintfChannel(
											256, "Music state set to %d, way outnumbered.  Aaaaa!\n",
											selectedState);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (g_setMusicState) {
		selectedState = g_setMusicState;
	}

	g_selectedMusicState = selectedState;
	if (g_currentMusicState != selectedState) {
		Music_SetState(selectedState);
		if ((g_currentMusicState == MUSIC_STATE_NO_ENEMIES_INTRO ||
			 g_currentMusicState == MUSIC_STATE_NO_ENEMIES_INTRO_ALT) &&
			(g_selectedMusicState == MUSIC_STATE_CONFLICT || g_selectedMusicState == MUSIC_STATE_1115 ||
			 g_selectedMusicState == MUSIC_STATE_1120)) {
			if (g_missionHeader.body.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
				Music_TriggerSequence(2205, g_players[g_localPlayer].regionIndex, 2);
			} else {
				Music_TriggerSequence(2200, g_players[g_localPlayer].regionIndex, 2);
			}
		}
		g_currentMusicState = g_selectedMusicState;
	}

	Music_Update();
}

// FUNCTION: XWA 0x434520
void FlightInput_ResetRuntimeState(void) {
	uint16_t joystickActive;

	g_joystickEnabled = 0;
	g_keyMods = 0;
	joystickActive = Input_DetectActiveJoystick();
	g_controlMask = 0;
	g_injectedKeyCount = 0;
	g_joystickDetectResultWord = joystickActive;
	g_joystickEnabled = joystickActive != 0;
	g_throttleSmoothed = -1;

	return;
}

// FUNCTION: XWA 0x50B680
int FlightInput_HasKeyReady(void) {
	if (g_flightConfDirectInput) {
		return DInput_HasKeyReady();
	}

	return g_keyReady;
}

// FUNCTION: XWA 0x50A2B0
ObjectIndex FilmOverlay_FindNextSelectableObject(ObjectIndex currentObjIdx, int16_t step, int unusedPlayerIdx,
												 int excludedObjIdx) {
	ObjectRecord* objects;
	uint32_t endObjSlot;
	uint32_t startObjSlot;
	uint32_t remainingChecks;
	uint16_t candidate;

	(void)unusedPlayerIdx;

	if (g_provingGroundsModeActive) {
		return currentObjIdx;
	}

	endObjSlot = g_regionStaticObjectSlotEnd;
	startObjSlot = g_activeRegionObjectSlotStart;
	remainingChecks = endObjSlot;
	remainingChecks -= startObjSlot;
	candidate = (uint16_t)remainingChecks;
	remainingChecks += 0xffffu;
	if (candidate != 0) {
		objects = g_objectTable;
		candidate = (uint16_t)currentObjIdx;
		do {
			ObjectRecord* obj;
			MobileObject* mobj;
			uint16_t objectType;

			candidate = (uint16_t)(candidate + (uint16_t)step);
			if (candidate >= 0x8000u || candidate < startObjSlot) {
				candidate = (uint16_t)(endObjSlot - 1u);
			} else if (candidate >= endObjSlot) {
				candidate = (uint16_t)startObjSlot;
			}

			if (excludedObjIdx != candidate) {
				obj = &objects[candidate];
				objectType = objects[candidate].objectType;
				if (objectType != OBJ_None &&
					(g_modelTypeTable[objectType].flags & MODEL_TYPE_FLAG_FILM_OVERLAY_SELECTABLE) != 0) {
					mobj = obj->mobj;
					if (mobj == NULL) {
						return (ObjectIndex)candidate;
					}

					if (obj->genusId != GENUS_Explosion) {
						CraftData* beamCraft;
						uint8_t hasActiveBeam;

						hasActiveBeam =
							(uint8_t)(candidate != 0xffffu && candidate < g_activeRegionCraftObjectSlotEnd &&
									  obj->playerOwnerIdx != -1 && (beamCraft = mobj->pCraft) != NULL &&
									  (beamCraft->workingSubsystems & 0x100u) != 0 &&
									  beamCraft->beamActive != 0 && beamCraft->beamTypeId == 3 &&
									  beamCraft->beamTimer != 0);
						if (!hasActiveBeam && g_missionFlightGroups[obj->flightGroupIdx].fg.status1 != 27) {
							if (mobj->state != 0) {
								return (ObjectIndex)candidate;
							}

							g_curCraft = mobj->pCraft;
							if (g_curCraft->objectKind != GENUS_Freighter &&
								g_curCraft->objectKind != GENUS_Starship &&
								g_curCraft->objectKind != GENUS_NpcProjectile) {
								return (ObjectIndex)candidate;
							}
						}
					}
				}

				endObjSlot = g_regionStaticObjectSlotEnd;
			}

			{
				uint16_t prevRemainingChecks;

				prevRemainingChecks = (uint16_t)remainingChecks;
				remainingChecks += 0xffffu;
				if (prevRemainingChecks == 0) {
					break;
				}
			}
		} while (1);
	}

	return (ObjectIndex)candidate;
}

// FUNCTION: XWA 0x4031C0
void FlightObject_AnimateCrewMeshRotations(int objectIndex, int resetToNeutral) {
	short sideMeshIdx;
	short forwardMeshIdx;
	CraftData* craft;
	uint8_t* rotation;
	uint8_t value;

	sideMeshIdx = -1;
	forwardMeshIdx = -1;

	if (g_flightPlayerCount > 1 || objectIndex == 0xffff) {
		return;
	}

	switch (g_objectTable[objectIndex].objectType) {
		case OBJ_XWing:
			sideMeshIdx = 6;
			forwardMeshIdx = 9;
			break;

		case OBJ_AWing:
			forwardMeshIdx = 4;
			break;

		case OBJ_YWing:
			sideMeshIdx = 8;
			forwardMeshIdx = 13;
			break;

		case OBJ_BWing:
			break;

		case OBJ_CorellianTransport2:
			sideMeshIdx = 19;
			forwardMeshIdx = 17;
			break;

		case OBJ_FamilyTransport:
			sideMeshIdx = 13;
			forwardMeshIdx = 9;
			break;

		case OBJ_MilleniumFalcon2:
			sideMeshIdx = 21;
			forwardMeshIdx = 19;
			break;

		case OBJ_RebelPilot:
		case OBJ_ImperialPilot:
			forwardMeshIdx = 1;
			break;

		default:
			return;
	}

	craft = g_objectTable[objectIndex].mobj->pCraft;
	if (sideMeshIdx >= 0) {
		if (resetToNeutral) {
			craft->meshRotation[sideMeshIdx] = 0;
		} else {
			rotation = &craft->meshRotation[sideMeshIdx];
			value = *rotation;
			if ((value & 1u) != 0) {
				value += 2;
				*rotation = value;
				if (value == 33u || (uint16_t)GameRand() < 0x300u) {
					*rotation ^= 1u;
				}
			} else {
				value -= 2;
				*rotation = value;
				if (value == 0xe0u || (uint16_t)GameRand() < 0x300u) {
					*rotation ^= 1u;
				}
			}
		}
	}

	if (forwardMeshIdx >= 0) {
		if (resetToNeutral) {
			craft->meshRotation[forwardMeshIdx] = 0;
		} else {
			rotation = &craft->meshRotation[forwardMeshIdx];
			value = *rotation;
			if ((value & 1u) != 0) {
				value -= 2;
				*rotation = value;
				if (value == 0xf1u && (uint16_t)GameRand() > 0x300u) {
					*rotation = (uint8_t)(*rotation + 2u);
				}
				if (*rotation == 0xe1u || (uint16_t)GameRand() < 0x300u) {
					*rotation ^= 1u;
				}
			} else {
				value += 2;
				*rotation = value;
				if (value == 16u && (uint16_t)GameRand() > 0x300u) {
					*rotation = (uint8_t)(*rotation - 2u);
				}
				if (*rotation == 32u || (uint16_t)GameRand() < 0x300u) {
					*rotation ^= 1u;
				}
			}
		}
	}

	if (g_objectTable[objectIndex].objectType == OBJ_RebelPilot ||
		g_objectTable[objectIndex].objectType == OBJ_ImperialPilot) {
		int count;

		rotation = &craft->meshRotation[2];
		count = 4;
		do {
			if (resetToNeutral) {
				*rotation = 0;
			} else {
				value = *rotation;
				if ((value & 1u) != 0) {
					value -= 2;
					*rotation = value;
					if (value == 0xf1u && (uint16_t)GameRand() > 0x300u) {
						*rotation = (uint8_t)(*rotation + 2u);
					}
					if (*rotation == 0xe1u || (uint16_t)GameRand() < 0x300u) {
						*rotation ^= 1u;
					}
				} else {
					value += 2;
					*rotation = value;
					if (value == 16u && (uint16_t)GameRand() > 0x300u) {
						*rotation = (uint8_t)(*rotation - 2u);
					}
					if (*rotation == 32u || (uint16_t)GameRand() < 0x300u) {
						*rotation ^= 1u;
					}
				}
			}
			++rotation;
			--count;
		} while (count != 0);
	}
}

// FUNCTION: XWA 0x4016B0
void FlightObject_InitMeshAnimationDefaults(int objIdx) {
	ObjectRecord* obj;
	MobileObject* mobj;
	uint16_t objectType;
	CraftData* pCraft;

	mobj = g_objectTable[objIdx].mobj;
	obj = &g_objectTable[objIdx];
	if (mobj == NULL) {
		return;
	}

	objectType = obj->objectType;
	if (objectType == OBJ_None) {
		return;
	}

	pCraft = mobj->pCraft;
	g_curCraft = pCraft;

	switch (obj->objectType) {
		case OBJ_XWing:
		case OBJ_BWing:
		case OBJ_MissileBoat:
		case OBJ_AssaultGunboat:
		case OBJ_SkiprayBlastBoat:
		case OBJ_Shuttle: {
			int meshCount;
			int meshIdx;
			uint8_t openRotation;

			meshCount = ModelMesh_GetObjectTypeMeshCount(objectType);
			if ((uint16_t)meshCount > 0) {
				meshIdx = 0;
				meshCount &= 0xffff;
				openRotation = 64;
				do {
					MeshType meshType;

					meshType = ModelMesh_GetObjectTypeMeshType(objectType, meshIdx);
					if (meshType == MESH_Bridge && objectType == OBJ_BWing) {
						g_curCraft->meshRotation[meshIdx] = openRotation;
					}
					if (meshType == MESH_Launcher && objectType == OBJ_MissileBoat) {
						g_curCraft->meshRotation[meshIdx] = 32;
					}
					if (meshType == MESH_RotaryWing) {
						if (objectType == OBJ_XWing) {
							if (ModelMesh_GetCenterZ(OBJ_XWing, meshIdx) < 0) {
								g_curCraft->meshRotation[meshIdx] = 12;
							} else {
								g_curCraft->meshRotation[meshIdx] = 8;
							}
						} else if (objectType == OBJ_BWing) {
							g_curCraft->meshRotation[meshIdx] = openRotation;
						} else if (objectType == OBJ_Shuttle) {
							g_curCraft->meshRotation[meshIdx] = 96;
						} else if (objectType == OBJ_SkiprayBlastBoat) {
							g_curCraft->meshRotation[meshIdx] = openRotation;
						} else if (objectType == OBJ_MissileBoat) {
							g_curCraft->meshRotation[meshIdx] = 66;
						}
					}
					++meshIdx;
					--meshCount;
				} while (meshCount != 0);
			}

			g_curCraft->sFoilState = 2;
			break;
		}

		default:
			pCraft->sFoilState = 0;
			break;
	}
}

// FUNCTION: XWA 0x402D20
void FlightObject_UpdateDebrisAndTransientAnimations(void) {
	if (g_debrisEnabled && !g_provingGroundsModeActive &&
		g_playerFlightTransientTimers[g_localPlayer].debrisRecycleCooldown == 0) {
		int localPlayerObjIdx;

		localPlayerObjIdx = g_players[g_localPlayer].objectIndex;
		if (localPlayerObjIdx != 0xffff) {
			uint16_t debrisObjIdx;
			int debrisWorldX;
			int debrisWorldY;
			int debrisWorldZ;
			int playerWorldX;
			int playerWorldY;
			int playerWorldZ;
			unsigned int absDx;
			unsigned int absDy;
			unsigned int absDz;

			debrisObjIdx = g_localDebrisRecycleSlotCursor++;
			if (g_localDebrisRecycleSlotCursor == g_localDebrisSlotEnd) {
				g_localDebrisRecycleSlotCursor = (uint16_t)g_localTransientSlotStart;
			}

			debrisWorldX = g_objectTable[debrisObjIdx].world_x;
			debrisWorldY = g_objectTable[debrisObjIdx].world_y;
			debrisWorldZ = g_objectTable[debrisObjIdx].world_z;
			playerWorldX = g_objectTable[localPlayerObjIdx].world_x;
			playerWorldY = g_objectTable[localPlayerObjIdx].world_y;
			playerWorldZ = g_objectTable[localPlayerObjIdx].world_z;

			if ((!g_filmPlaybackMode || g_filmOverlayActive != 1) &&
				!g_players[g_localPlayer].viewState.externalCameraActive) {
				playerWorldX += (int)g_players[g_localPlayer].hardpointWorldX;
				playerWorldY += (int)g_players[g_localPlayer].hardpointWorldY;
				playerWorldZ += (int)g_players[g_localPlayer].hardpointWorldZ;
			}

			debrisWorldX -= playerWorldX;
			if (debrisWorldX < 0) {
				debrisWorldX = -debrisWorldX;
			}
			debrisWorldY -= playerWorldY;
			if (debrisWorldY < 0) {
				debrisWorldY = -debrisWorldY;
			}
			debrisWorldZ -= playerWorldZ;
			if (debrisWorldZ < 0) {
				debrisWorldZ = -debrisWorldZ;
			}
			absDx = (unsigned int)debrisWorldX;
			absDy = (unsigned int)debrisWorldY;
			absDz = (unsigned int)debrisWorldZ;

			if (collide_roughdistance3du(absDx, absDy, absDz) > 0xc00u) {
				MobileObject* playerMobj;
				int sideX;
				int sideY;
				int sideZ;
				int upX;
				int upY;
				int upZ;
				int16_t sideRandom;
				int16_t upRandom;

				g_objectTable[debrisObjIdx].objectType =
					(uint16_t)(OBJ_DebrisTextureGroup4000 + (GameRand2() & 3u));
				g_objectTable[debrisObjIdx].mobj->instanceExtent =
					g_modelTypeTable[g_objectTable[debrisObjIdx].objectType].maxBoundsExtent;

				if (g_objectTable[localPlayerObjIdx].mobj->orientMatrixDirty) {
					FVIEW_calcrotatemove(g_objectTable[localPlayerObjIdx].pitch,
										 g_objectTable[localPlayerObjIdx].yaw,
										 &g_objectTable[localPlayerObjIdx]);
					FVIEW_calcrotateorient(g_objectTable[localPlayerObjIdx].roll,
										   g_objectTable[localPlayerObjIdx].angleD,
										   &g_objectTable[localPlayerObjIdx]);
				}

				sideRandom = (int16_t)((GameRand2() & 0x3ffu) - 0x200);
				sideX =
					Xwa_Q15MulReuseFirstSlot(sideRandom, g_objectTable[localPlayerObjIdx].mobj->cachedSideX);
				sideY =
					Xwa_Q15MulReuseFirstSlot(sideRandom, g_objectTable[localPlayerObjIdx].mobj->cachedSideY);
				sideZ =
					Xwa_Q15MulReuseFirstSlot(sideRandom, g_objectTable[localPlayerObjIdx].mobj->cachedSideZ);

				upRandom = (int16_t)((GameRand2() & 0x3ffu) - 0x200);
				upX = Xwa_Q15MulReuseFirstSlot(upRandom, g_objectTable[localPlayerObjIdx].mobj->cachedUpX);
				upY = Xwa_Q15MulReuseFirstSlot(upRandom, g_objectTable[localPlayerObjIdx].mobj->cachedUpY);
				upZ = Xwa_Q15MulReuseFirstSlot(upRandom, g_objectTable[localPlayerObjIdx].mobj->cachedUpZ);

				playerMobj = g_objectTable[localPlayerObjIdx].mobj;
				playerWorldY += (int16_t)(upY + sideY + (playerMobj->cachedFwdY >> 4));
				playerWorldZ += (int16_t)(upZ + sideZ + (playerMobj->cachedFwdZ >> 4));
				playerWorldX += (int16_t)(upX + sideX + (playerMobj->cachedFwdX >> 4));
				g_objectTable[debrisObjIdx].world_x = playerWorldX;
				g_objectTable[debrisObjIdx].world_y = playerWorldY;
				g_objectTable[debrisObjIdx].world_z = playerWorldZ;
				g_objectTable[debrisObjIdx].typeSpecificByte[0] = 1;
			}
		}

		g_playerFlightTransientTimers[g_localPlayer].debrisRecycleCooldown = 59;
	}

#ifdef XWA_MODERN
	if (XwaModernFlightTiming_AdvanceTransientAnimation((uint16_t)g_elapsedTicks)) {
#else
	{
#endif
		uint32_t transientObjIdx;

		transientObjIdx = g_localTransientSlotStart;
		if ((uint16_t)transientObjIdx < g_localTransientSlotEnd) {
			do {
				uint16_t animFrame;
				uint32_t currentObjIdx;

				currentObjIdx = (uint16_t)transientObjIdx;

				if (g_objectTable[currentObjIdx].mobj != NULL &&
					g_objectTable[currentObjIdx].objectType != OBJ_None) {
					ModelTypeInfo* modelInfo;

					animFrame = g_objectTable[currentObjIdx].typeSpecificByte[0];
					g_flightObjectAnimFrameScratch = animFrame;
					modelInfo = &g_modelTypeTable[g_objectTable[currentObjIdx].objectType];
					if (g_flightObjectAnimFrameScratch != 0 && modelInfo->texLevels != NULL) {
						if (g_flightObjectAnimFrameScratch == 0xffffu) {
							animFrame = 0;
						}
						g_flightObjectAnimFrameScratch = (uint16_t)(animFrame + 1u);
						if (g_flightObjectAnimFrameScratch > modelInfo->frameCount) {
							if ((modelInfo->flags & MODEL_TYPE_FLAG_ANIMATION_LOOPS) != 0) {
								g_flightObjectAnimFrameScratch = 1;
							} else {
								g_flightObjectAnimFrameScratch = 0;
								g_objectTable[currentObjIdx].objectType = OBJ_None;
								if (currentObjIdx >= g_activeRegionObjectSlotStart &&
									currentObjIdx < g_activeRegionCraftObjectSlotEnd &&
									g_objectTable[currentObjIdx].mobj->pCraft != NULL) {
									Craft_ClearEffectiveAiObjectLink(
										g_objectTable[currentObjIdx].mobj->pCraft);
								}
							}
						}
					}

					g_objectTable[currentObjIdx].typeSpecificByte[0] =
						(uint8_t)g_flightObjectAnimFrameScratch;
				}
				++transientObjIdx;
			} while ((uint16_t)transientObjIdx < g_localTransientSlotEnd);
		}
	}
}

// FUNCTION: XWA 0x401880
// Per-region per-step object special-behavior pass, run once per fixed step. Three
// timer-gated phases over the current region's object ranges:
//  1. Craft S-foil / folding-wing / launcher / bridge mesh rotation toward open/closed,
//     emitting the open/closed HUD message on completion (objectSpecialBehaviorUpdateTimer).
//  2. Loose crew/pilot tumble, damaged-craft escape-pod ejection, hull explosion fragments,
//     debris/animated-texture frame advance, rotary gun-turret idle-spin and target tracking,
//     and active chaff cloud effects (crewMeshRotationUpdateTimer).
//  3. Death Star tunnel super-laser warm-up / travel / hold sequencing and beam sweep damage
//     for this region.
void FlightObject_UpdateSpecialBehavior(void) {
	uint32_t objIdx;
	int16_t objectType;
	int modelType;

	// ---- Phase 1: S-foil / folding mesh rotation ----
	if (g_flightGlobalCountdownTimers[11] == 0) {
		if (regionIdx == g_activeMissionRegionCount - 1) {
			g_flightGlobalCountdownTimers[11] = 14;
		}
		objIdx = g_activeRegionObjectSlotStart;
		if ((uint16_t)objIdx < g_activeRegionCraftObjectSlotEnd) {
			do {
				MobileObject* mobj = g_objectTable[(uint16_t)objIdx].mobj;

				if (mobj != NULL) {
					objectType = g_objectTable[(uint16_t)objIdx].objectType;
					if ((uint16_t)objectType != 0) {
						g_curCraft = mobj->pCraft;
						modelType = (uint16_t)objectType;

						switch ((uint16_t)objectType) {
							case 1:
							case 4:
							case 12:
							case 16:
							case 17:
							case 50: {
								uint16_t meshCount = (uint16_t)ModelMesh_GetObjectTypeMeshCount(modelType);
								int16_t anyRotated = 0;
								int m;
								int remainingMeshes;

								if (meshCount > 0) {
									m = 0;
									remainingMeshes = meshCount;
									do {
										MeshType meshType = ModelMesh_GetObjectTypeMeshType(modelType, m);

										// Falcon bridge fold (no completion accounting).
										if (meshType == MESH_Bridge && objectType == 4 &&
											(g_curCraft->sFoilState & 1)) {
											uint8_t mr = g_curCraft->meshRotation[m];
											if (g_curCraft->sFoilState & 2) {
												if (mr < 0x40) {
													g_curCraft->meshRotation[m] = (uint8_t)(mr + 2);
												}
											} else if (mr > 0) {
												g_curCraft->meshRotation[m] = (uint8_t)(mr - 2);
												if (g_curCraft->meshRotation[m] > 0x80) {
													g_curCraft->meshRotation[m] = 0;
												}
											}
										}
										// Launcher arms (type 12).
										if (meshType == MESH_Launcher && objectType == 12 &&
											(g_curCraft->sFoilState & 1)) {
											uint8_t mr = g_curCraft->meshRotation[m];
											if (g_curCraft->sFoilState & 2) {
												if (mr < 0x20) {
													g_curCraft->meshRotation[m] = (uint8_t)(mr + 1);
													anyRotated = 1;
												}
											} else if (mr > 0) {
												g_curCraft->meshRotation[m] = (uint8_t)(mr - 1);
												anyRotated = 1;
											}
										}
										// Folding wings / rotary panels.
										if (meshType == MESH_RotaryWing && (g_curCraft->sFoilState & 1)) {
											if (g_curCraft->sFoilState & 2) {
												if ((uint16_t)objectType == 1) {
													int maxRotation = ModelMesh_GetCenterZ(1, m) < 0 ? 12 : 8;
													if (g_curCraft->meshRotation[m] < maxRotation) {
														++g_curCraft->meshRotation[m];
														anyRotated = 1;
													}
												} else if ((uint16_t)objectType == 4) {
													if (g_curCraft->meshRotation[m] < 0x40u) {
														g_curCraft->meshRotation[m] += 2;
														anyRotated = 1;
													}
												} else if ((uint16_t)objectType == 50) {
													if (g_curCraft->meshRotation[m] < 0x60u) {
														g_curCraft->meshRotation[m] += 2;
														anyRotated = 1;
													}
												} else if ((uint16_t)objectType == 17) {
													if (g_curCraft->meshRotation[m] < 0x40u) {
														g_curCraft->meshRotation[m] += 2;
														anyRotated = 1;
													}
												} else if ((uint16_t)objectType == 12) {
													if (g_curCraft->meshRotation[m] < 0x42u) {
														g_curCraft->meshRotation[m] += 2;
														anyRotated = 1;
													}
												}
											} else {
												if ((uint16_t)objectType == 1) {
													if (g_curCraft->meshRotation[m] > 0) {
														--g_curCraft->meshRotation[m];
														anyRotated = 1;
													}
												} else if ((uint16_t)objectType == 4 ||
														   (uint16_t)objectType == 50 ||
														   (uint16_t)objectType == 17 ||
														   (uint16_t)objectType == 12) {
													if (g_curCraft->meshRotation[m] > 0) {
														g_curCraft->meshRotation[m] -= 2;
														anyRotated = 1;
													}
												}
											}
										}
										++m;
									} while (--remainingMeshes != 0);
								}

								if (g_curCraft->sFoilState & 1) {
									if (g_curCraft->sFoilState & 2) {
										if (!anyRotated) {
											g_curCraft->sFoilState = 2;
											msg_emitInFlightMessage(
												MSG_SFOILS_CLOSED,
												g_objectTable[(uint16_t)objIdx].playerOwnerIdx);
										}
									} else if (!anyRotated) {
										g_curCraft->sFoilState = 0;
										msg_emitInFlightMessage(
											MSG_SFOILS_OPEN, g_objectTable[(uint16_t)objIdx].playerOwnerIdx);
									}
								}
								break;
							}
							default:
								g_curCraft->sFoilState = 0;
								break;
						}
					}
				}
				++objIdx;
			} while ((uint16_t)objIdx < g_activeRegionCraftObjectSlotEnd);
		}
	}

	// ---- Phase 2: crew tumble, damage effects, turret aim, animated textures ----
	if (g_flightGlobalCountdownTimers[2] == 0) {
		if (regionIdx == g_activeMissionRegionCount - 1) {
			g_flightGlobalCountdownTimers[2] = 15;
		}
		for (objIdx = g_activeRegionObjectSlotStart; (uint16_t)objIdx < g_regionStaticObjectSlotEnd;
			 ++objIdx) {
			ModelGenusId genusId;
			AiController* ai;
			int doRotate;
			int meshCount;
			int m;
			int remainingMeshes;

			objectType = g_objectTable[(uint16_t)objIdx].objectType;
			if ((uint16_t)objectType >= 0x178u && (uint16_t)objectType <= 0x195u) {
				continue;
			}
			// Slowly tumble loose crew/pilot objects (object types 223..228).
			if ((uint16_t)objectType >= 0xDFu && (uint16_t)objectType <= 0xE4u) {
				g_objectTable[(uint16_t)objIdx].roll +=
					(Q16Angle)((15u * ((objIdx - g_regionStaticObjectSlotEnd) >> 4)) >> 4);
				g_objectTable[(uint16_t)objIdx].pitch +=
					(Q16Angle)((15u * ((objIdx - g_regionStaticObjectSlotEnd) >> 3)) >> 5);
				g_objectTable[(uint16_t)objIdx].yaw +=
					(Q16Angle)((60u - 15u * ((objIdx - g_regionStaticObjectSlotEnd) >> 4)) >> 4);
			}

			if (g_objectTable[(uint16_t)objIdx].mobj == NULL) {
				if (objectType != 0 && g_modelTypeTable[objectType].texLevels != NULL) {
					uint16_t frame;
					ModelTypeInfo* modelInfo;

					frame = g_objectTable[(uint16_t)objIdx].typeSpecificByte[0];
					g_flightObjectAnimFrameScratch = frame;
					modelInfo = &g_modelTypeTable[g_objectTable[(uint16_t)objIdx].objectType];
					if (g_flightObjectAnimFrameScratch != 0 && modelInfo->texLevels != NULL) {
						if (g_flightObjectAnimFrameScratch == 0xffffu) {
							frame = 0;
						}
						g_flightObjectAnimFrameScratch = (uint16_t)(frame + 1u);
						if (g_flightObjectAnimFrameScratch > modelInfo->frameCount) {
							if ((modelInfo->flags & MODEL_TYPE_FLAG_ANIMATION_LOOPS) != 0) {
								g_flightObjectAnimFrameScratch = 1;
							} else {
								g_flightObjectAnimFrameScratch = 0;
								g_objectTable[(uint16_t)objIdx].objectType = OBJ_None;
								if (objIdx >= g_activeRegionObjectSlotStart &&
									objIdx < g_activeRegionCraftObjectSlotEnd &&
									g_objectTable[(uint16_t)objIdx].mobj != NULL &&
									g_objectTable[(uint16_t)objIdx].mobj->pCraft != NULL) {
									Craft_ClearEffectiveAiObjectLink(
										g_objectTable[(uint16_t)objIdx].mobj->pCraft);
								}
							}
						}
					}
					g_objectTable[(uint16_t)objIdx].typeSpecificByte[0] =
						(uint8_t)g_flightObjectAnimFrameScratch;
				}
				continue;
			}
			if (objectType == 0) {
				continue;
			}
			genusId = g_objectTable[(uint16_t)objIdx].genusId;

			switch (genusId) {
				case GENUS_Fighter:
				case GENUS_Transport:
				case GENUS_Utility:
				case GENUS_Freighter:
				case GENUS_Starship:
				case GENUS_Platform:
				case GENUS_Container:
				case GENUS_PilotDroid:
				case GENUS_WeaponEmplacement:
					break;
				case GENUS_Debris:
				case GENUS_Explosion:
					if (objectType != OBJ_NoAsset_222) {
						uint16_t frame;
						ModelTypeInfo* modelInfo;

						frame = g_objectTable[(uint16_t)objIdx].typeSpecificByte[0];
						g_flightObjectAnimFrameScratch = frame;
						modelInfo = &g_modelTypeTable[g_objectTable[(uint16_t)objIdx].objectType];
						if (g_flightObjectAnimFrameScratch != 0 && modelInfo->texLevels != NULL) {
							if (g_flightObjectAnimFrameScratch == 0xffffu) {
								frame = 0;
							}
							g_flightObjectAnimFrameScratch = (uint16_t)(frame + 1u);
							if (g_flightObjectAnimFrameScratch > modelInfo->frameCount) {
								if ((modelInfo->flags & MODEL_TYPE_FLAG_ANIMATION_LOOPS) != 0) {
									g_flightObjectAnimFrameScratch = 1;
								} else {
									g_flightObjectAnimFrameScratch = 0;
									g_objectTable[(uint16_t)objIdx].objectType = OBJ_None;
									if (objIdx >= g_activeRegionObjectSlotStart &&
										objIdx < g_activeRegionCraftObjectSlotEnd &&
										g_objectTable[(uint16_t)objIdx].mobj != NULL &&
										g_objectTable[(uint16_t)objIdx].mobj->pCraft != NULL) {
										Craft_ClearEffectiveAiObjectLink(
											g_objectTable[(uint16_t)objIdx].mobj->pCraft);
									}
								}
							}
						}
						g_objectTable[(uint16_t)objIdx].typeSpecificByte[0] =
							(uint8_t)g_flightObjectAnimFrameScratch;
					} else {
						g_objectTable[(uint16_t)objIdx].typeSpecificByte[1] = 1;
						if ((uint16_t)GameRand() < 0x800u) {
							Object_SpawnEffectFragment((uint16_t)objIdx);
						}
					}
					continue;
				default:
					continue;
			}

			// --- craft genus block ---
			modelType = (uint16_t)objectType;
			meshCount = ModelMesh_GetObjectTypeMeshCount(modelType);
			g_curCraft = g_objectTable[(uint16_t)objIdx].mobj->pCraft;
			ai = pai_GetEffectiveAIController(g_curCraft);
			doRotate = 0;
			if (g_curCraft->workingSubsystems != 0) {
				const char* planName = g_planTable[ai->pendingPlanId].name;
				if (strcmp(planName, "nullpln") != 0 && strcmp(planName, "stationaryldrpln") != 0 &&
					strcmp(planName, "stationaryflwpln") != 0) {
					if (g_objectTable[(uint16_t)objIdx].playerOwnerIdx == -1) {
						doRotate = 1;
					}
				}
			}

			// Damaged craft eject crew/escape pods over time.
			if (g_objectTable[(uint16_t)objIdx].mobj->ejectionSpawnCount != 0) {
				if (g_curCraft->objectKind != 3 || objectType == OBJ_NoAsset_222 ||
					ModelMesh_HasFuselage(modelType) || g_objectTable[(uint16_t)objIdx].genusId == 0) {
					if (g_curCraft->systemHitFlag && (uint16_t)GameRand() < 0x800u) {
						FlightObject_SpawnEscapePodOrPilot((uint16_t)objIdx);
						--g_objectTable[(uint16_t)objIdx].mobj->ejectionSpawnCount;
					}
				} else if ((uint16_t)GameRand() < 0x800u) {
					FlightObject_SpawnEscapePodOrPilot((uint16_t)objIdx);
					--g_objectTable[(uint16_t)objIdx].mobj->ejectionSpawnCount;
				}
			}

			// Idle-spin or aim rotary gun turrets at their targets.
			if (doRotate && g_curCraft->laserSlotCount) {
				int slot;
				for (slot = 0; slot < g_curCraft->laserSlotCount; ++slot) {
					int meshIdx;

					if (g_curCraft->warheadData[slot].weaponType < 4u) {
						continue;
					}
					meshIdx = g_modelDefs[g_curCraft->modelIndex].weaponHardpoints[slot].meshIdx;
					if (!g_curCraft->componentHp[meshIdx] ||
						ModelMesh_GetObjectTypeMeshType(modelType, meshIdx) != MESH_RotaryGunTurret) {
						continue;
					}
					if (g_curCraft->warheadData[slot].turretTargetObjIdx == -1) {
						uint8_t mr = g_curCraft->meshRotation[meshIdx];
						g_curCraft->meshRotation[meshIdx] = (mr & 1) ? (uint8_t)(mr + 4) : (uint8_t)(mr - 4);
						if ((uint16_t)GameRand() < 0x600u) {
							g_curCraft->meshRotation[meshIdx] ^= 1u;
						}
					} else {
						OptRotationScale* rs = ModelMesh_GetRotScaleData(modelType, meshIdx);
						ObjectRecord* to = &g_objectTable[objIdx];
						MobileObject* tm;
						int side, fwd, up, rotationProj, dirProj, upProj;

						Mission_ResolveObjectOrMissionPointWorldLoc(
							(uint16_t)g_curCraft->warheadData[slot].turretTargetObjIdx, 0, 0, 0);
						worldlocx -= to->world_x;
						worldlocy -= to->world_y;
						worldlocz -= to->world_z;
						if (to->mobj->orientMatrixDirty) {
							FVIEW_calcrotatemove(to->pitch, to->yaw, to);
							FVIEW_calcrotateorient(to->roll, to->angleD, to);
						}
						tm = to->mobj;
						side = Xwa_Dot3Q15Inline(tm->cachedSideX, tm->cachedSideY, tm->cachedSideZ, worldlocx,
												 worldlocy, worldlocz);
						fwd = Xwa_Dot3Q15Inline(tm->cachedFwdX, tm->cachedFwdY, tm->cachedFwdZ, worldlocx,
												worldlocy, worldlocz);
						up = Xwa_Dot3Q15Inline(tm->cachedUpX, tm->cachedUpY, tm->cachedUpZ, worldlocx,
											   worldlocy, worldlocz);
						worldlocx = side - (int)rs->pivot.x;
						worldlocy = -(fwd + (int)rs->pivot.y);
						worldlocz = up - (int)rs->pivot.z;
						rotationProj =
							Xwa_Dot3Q15Inline((int)rs->rotationAxis.x, (int)rs->rotationAxis.y,
											  (int)rs->rotationAxis.z, worldlocx, worldlocy, worldlocz);
						dirProj =
							Xwa_Dot3Q15Inline((int)rs->directionAxis.x, (int)rs->directionAxis.y,
											  (int)rs->directionAxis.z, worldlocx, worldlocy, worldlocz);
						upProj = Xwa_Dot3Q15Inline((int)rs->upAxis.x, (int)rs->upAxis.y, (int)rs->upAxis.z,
												   worldlocx, worldlocy, worldlocz);
						(void)rotationProj;
						g_curCraft->meshRotation[meshIdx] = (uint8_t)(trig2_arctan(upProj, dirProj) >> 8);
					}
				}
			}

			// Per-mesh: animated textures, hull-damage effects, crew/gun mesh idle spin.
			remainingMeshes = meshCount;
			for (m = 0; remainingMeshes != 0; ++m, --remainingMeshes) {
				MeshType meshType = ModelMesh_GetObjectTypeMeshType(modelType, m);

				if (meshType == 3) {
					// Animated-texture mesh: advance the frame stored in componentState[49],
					// temporarily reusing OBJ_AnimationTextureGroup2008's model info.
					ObjectTypeId savedType = g_objectTable[(uint16_t)objIdx].objectType;
					uint16_t frame = g_curCraft->componentState[49];

					g_flightObjectAnimFrameScratch = frame;
					g_objectTable[(uint16_t)objIdx].objectType = OBJ_AnimationTextureGroup2008;
					if (frame != 0 &&
						g_modelTypeTable[g_objectTable[(uint16_t)objIdx].objectType].texLevels != NULL) {
						ModelTypeInfo* mi = &g_modelTypeTable[g_objectTable[(uint16_t)objIdx].objectType];
						g_flightObjectAnimFrameScratch = (uint16_t)(frame + 1u);
						if (g_flightObjectAnimFrameScratch > mi->frameCount) {
							if ((mi->flags & MODEL_TYPE_FLAG_ANIMATION_LOOPS) != 0) {
								g_flightObjectAnimFrameScratch = 1;
							} else {
								g_flightObjectAnimFrameScratch = 0;
								g_objectTable[(uint16_t)objIdx].objectType = OBJ_None;
								if (objIdx >= g_activeRegionObjectSlotStart &&
									objIdx < g_activeRegionCraftObjectSlotEnd) {
									CraftData* c2 = g_objectTable[(uint16_t)objIdx].mobj->pCraft;
									if (c2 != NULL) {
										Craft_ClearEffectiveAiObjectLink(c2);
									}
								}
							}
						}
					}
					g_objectTable[(uint16_t)objIdx].objectType = savedType;
					g_curCraft->componentState[49] = (uint8_t)g_flightObjectAnimFrameScratch;
				}

				// Cosmetic hull breakup effects on a dying craft.
				if (g_curCraft->objectKind == 3) {
					if (objectType != OBJ_NoAsset_222) {
						if (ModelMesh_HasFuselage(modelType) ||
							g_objectTable[(uint16_t)objIdx].genusId == 0) {
							if ((uint16_t)GameRand() < 0x1800u) {
								Object_SpawnEffectFragment((uint16_t)objIdx);
							}
							if (g_objectTable[(uint16_t)objIdx].mobj->ejectionSpawnCount) {
								FlightObject_SpawnEscapePodOrPilot((uint16_t)objIdx);
								--g_objectTable[(uint16_t)objIdx].mobj->ejectionSpawnCount;
							}
						} else if (g_objectTable[(uint16_t)objIdx].mobj->pCraft->componentState[m] != 4) {
							if (m % 3 == 0) {
								Craft_SpawnMainHullExplosionEffects((uint16_t)objIdx, 0);
							}
						}
					}
				}

				// Gun/crew mesh idle spin.
				if (doRotate) {
					if (meshType == 11 || meshType == 23 || meshType == 12 || meshType == 24 ||
						meshType == 13 || meshType == 25) {
						uint8_t mr = g_curCraft->meshRotation[m];
						g_curCraft->meshRotation[m] = (mr & 1) ? (uint8_t)(mr + 4) : (uint8_t)(mr - 4);
						if ((uint16_t)GameRand() < 0x200u) {
							g_curCraft->meshRotation[m] ^= 1u;
						}
					}
				}
			}

			// Chaff cloud particles while chaff is active.
			if (!g_curCraft->objectKind && g_curCraft->cmTypeId == 1 && g_curCraft->chaffActiveTimer) {
				if (g_useHardware3D && g_objRenderState[(uint16_t)objIdx].drawnThisFrame) {
					if ((g_gameTime & 0xFFF) != 0) {
						Vec3f localOffset;
						Vec3f direction = { 0.0f, 1.0f, 0.0f };
						int minY = -ModelBounds_GetMinY(modelType);
						int sizeX = ModelBounds_GetSizeX(modelType);
						int sizeZ = ModelBounds_GetSizeZ(modelType);

						localOffset.x = (float)(Particle_RandSignedUnitFloat() * (double)sizeX);
						localOffset.y = (float)((double)minY * 1.25);
						localOffset.z = (float)(Particle_RandSignedUnitFloat() * (double)sizeZ);
						Particle_AttachEffectToObject(10, (uint16_t)objIdx, &localOffset, &direction);
					}
				} else {
					Object_SpawnLocalEffectFragment((uint16_t)objIdx);
					Object_SpawnLocalEffectFragment((uint16_t)objIdx);
					Object_SpawnLocalEffectFragment((uint16_t)objIdx);
				}
			}

			if (g_objectTable[(uint16_t)objIdx].playerOwnerIdx != -1 && g_flightPlayerCount == 1 &&
				g_players[g_localPlayer].viewState.externalCameraActive) {
				FlightObject_AnimateCrewMeshRotations((uint16_t)objIdx, 0);
			}
			if (objectType == OBJ_RebelPilot || objectType == OBJ_ImperialPilot) {
				FlightObject_AnimateCrewMeshRotations((uint16_t)objIdx, 0);
			}
		}
	}

	// ---- Phase 3: Death Star tunnel super-laser sequencing ----
	if (g_deathStarTunnelLaserRegions[regionIdx].enabled) {
		if (!g_deathStarTunnelLaserRegions[regionIdx].shotActive) {
			int shotStartGameTime = g_deathStarTunnelLaserRegions[regionIdx].shotStartGameTime;

			if (g_deathStarTunnelLaserRegions[regionIdx].alternateDelayPhase) {
				if (g_gameTime - shotStartGameTime >
					g_deathStarTunnelLaserRegions[regionIdx].repeatShotDelayTicks) {
					DeathStar_SelectLaserTarget(0xFFFFu);
					g_deathStarTunnelLaserRegions[regionIdx].shotActive = 1;
					g_deathStarTunnelLaserRegions[regionIdx].shotStartGameTime = g_gameTime;
					g_deathStarTunnelLaserRegions[regionIdx].beamLightActive = 0;
				}
			} else if (g_gameTime - shotStartGameTime >
					   g_deathStarTunnelLaserRegions[regionIdx].firstShotDelayTicks) {
				DeathStar_SelectLaserTarget(0xFFFFu);
				g_deathStarTunnelLaserRegions[regionIdx].shotActive = 1;
				g_deathStarTunnelLaserRegions[regionIdx].shotStartGameTime = g_gameTime;
				g_deathStarTunnelLaserRegions[regionIdx].alternateDelayPhase = 1;
				g_deathStarTunnelLaserRegions[regionIdx].beamLightActive = 0;
			}
			return;
		}

		{
			int elapsed = g_gameTime - g_deathStarTunnelLaserRegions[regionIdx].shotStartGameTime;
			int warmupTicks = g_deathStarTunnelLaserRegions[regionIdx].warmupTicks;

			if (elapsed < warmupTicks) {
				// Warm-up: ramp the beam emitter sprite frame.
				double t = (double)elapsed / (double)warmupTicks;
				uint16_t frameCount = g_modelTypeTable[488].frameCount;

				if (t >= 0.5) {
					WorldRectRecord* spr = g_deathStarTunnelLaserRegions[regionIdx].beamSpriteRect;
					spr->frame =
						(spr->frame == frameCount - 1) ? (uint8_t)frameCount : (uint8_t)(frameCount - 1);
				} else {
					int span = (uint16_t)(frameCount - 2);
					g_deathStarTunnelLaserRegions[regionIdx].beamSpriteRect->frame =
						(uint8_t)((int64_t)((t + t) * (double)span) + 1);
				}
				return;
			}

			if (!g_deathStarTunnelLaserRegions[regionIdx].beamLightActive) {
				DeathStar_FireLaserAtTarget();
			}
			if (elapsed > g_deathStarTunnelLaserRegions[regionIdx].warmupTicks +
							  g_deathStarTunnelLaserRegions[regionIdx].travelTicks +
							  g_deathStarTunnelLaserRegions[regionIdx].holdTicks) {
				g_deathStarTunnelLaserRegions[regionIdx].shotActive = 0;
				g_deathStarTunnelLaserRegions[regionIdx].beamLightActive = 0;
				collide_damagecraft(g_deathStarTunnelLaserRegions[regionIdx].targetObjIdx, 0xFFFFu,
									g_deathStarTunnelLaserRegions[regionIdx].laserObjIdx, 0, 0);
				g_objectTable[g_deathStarTunnelLaserRegions[regionIdx].laserObjIdx].objectType = OBJ_None;
			}

			{
				int afterWarmup = elapsed - g_deathStarTunnelLaserRegions[regionIdx].warmupTicks;
				if (afterWarmup >= g_deathStarTunnelLaserRegions[regionIdx].holdTicks) {
					// Travel: drive the laser object from beam start toward the target.
					int travel = afterWarmup - g_deathStarTunnelLaserRegions[regionIdx].holdTicks;
					double frac =
						(double)travel / (double)g_deathStarTunnelLaserRegions[regionIdx].travelTicks;
					double dx =
						(double)g_objectTable[(uint16_t)g_deathStarTunnelLaserRegions[regionIdx].targetObjIdx]
							.world_x -
						(double)g_deathStarTunnelLaserRegions[regionIdx].beamStartX;
					float dy =
						(float)((double)g_objectTable[(uint16_t)g_deathStarTunnelLaserRegions[regionIdx]
														  .targetObjIdx]
									.world_y -
								(double)g_deathStarTunnelLaserRegions[regionIdx].beamStartY);
					float dz =
						(float)((double)g_objectTable[(uint16_t)g_deathStarTunnelLaserRegions[regionIdx]
														  .targetObjIdx]
									.world_z -
								(double)g_deathStarTunnelLaserRegions[regionIdx].beamStartZ);
					double dist = sqrt(dx * dx + (double)(dy * dy) + (double)(dz * dz));
					double inv = 1.0 / dist;

					g_deathStarTunnelLaserRegions[regionIdx].remainingDistance = (float)dist;
					g_objectTable[g_deathStarTunnelLaserRegions[regionIdx].laserObjIdx].world_x =
						(int)(int64_t)(inv * dx * frac *
										   g_deathStarTunnelLaserRegions[regionIdx].remainingDistance +
									   (double)g_deathStarTunnelLaserRegions[regionIdx].beamStartX);
					g_objectTable[g_deathStarTunnelLaserRegions[regionIdx].laserObjIdx].world_y =
						(int)(int64_t)(inv * dy * g_deathStarTunnelLaserRegions[regionIdx].remainingDistance *
										   frac +
									   (double)g_deathStarTunnelLaserRegions[regionIdx].beamStartY);
					g_objectTable[g_deathStarTunnelLaserRegions[regionIdx].laserObjIdx].world_z =
						(int)(int64_t)(inv * dz * g_deathStarTunnelLaserRegions[regionIdx].remainingDistance *
										   frac +
									   (double)g_deathStarTunnelLaserRegions[regionIdx].beamStartZ);
					g_deathStarTunnelLaserRegions[regionIdx].remainingDistance =
						(float)((1.0 - frac) * g_deathStarTunnelLaserRegions[regionIdx].remainingDistance);
					g_deathStarTunnelLaserRegions[regionIdx].beamSpriteRect->frame = 0;
				} else {
					uint16_t frameCount = g_modelTypeTable[488].frameCount;
					WorldRectRecord* spr = g_deathStarTunnelLaserRegions[regionIdx].beamSpriteRect;
					spr->frame =
						(spr->frame == frameCount - 1) ? (uint8_t)frameCount : (uint8_t)(frameCount - 1);
				}
			}

			// Sweep the beam segment against active craft for collateral damage.
			g_collisionSegmentStartWorldX =
				g_objectTable[g_deathStarTunnelLaserRegions[regionIdx].laserObjIdx].world_x;
			g_collisionSegmentStartWorldY =
				g_objectTable[g_deathStarTunnelLaserRegions[regionIdx].laserObjIdx].world_y;
			g_collisionSegmentStartWorldZ =
				g_objectTable[g_deathStarTunnelLaserRegions[regionIdx].laserObjIdx].world_z;
			g_collisionProbeWorldX =
				g_objectTable[g_deathStarTunnelLaserRegions[regionIdx].targetObjIdx].world_x;
			g_collisionProbeWorldY =
				g_objectTable[g_deathStarTunnelLaserRegions[regionIdx].targetObjIdx].world_y;
			g_collisionProbeWorldZ =
				g_objectTable[g_deathStarTunnelLaserRegions[regionIdx].targetObjIdx].world_z;
			objIdx = g_activeRegionObjectSlotStart;
			if ((uint16_t)objIdx < g_activeRegionCraftObjectSlotEnd) {
				do {
					if ((uint16_t)objIdx != g_deathStarTunnelLaserRegions[regionIdx].targetObjIdx &&
						collide_CheckSweptModelCollision(g_deathStarTunnelLaserRegions[regionIdx].laserObjIdx,
														 objIdx) > 0) {
						collide_damagecraft((uint16_t)objIdx, 0xFFFFu,
											g_deathStarTunnelLaserRegions[regionIdx].laserObjIdx, 0, 0);
					}
					++objIdx;
				} while ((uint16_t)objIdx < g_activeRegionCraftObjectSlotEnd);
			}
		}
	}
}

// FUNCTION: XWA 0x50B6F0
int FlightInput_GetNextKey(void) {
	if (g_flightConfDirectInput) {
		return (int16_t)DInput_GetKey();
	}

	if (!g_keyReady) {
		return 0;
	}

	g_keyReady = 0;
	return g_lastKeyCode;
}

// FUNCTION: XWA 0x434570
uint16_t FlightInput_Read(int playerIdxOrSentinel) {
	uint16_t key;
	int axisX;
	int axisY;
	int throttleRaw;
	int axisR;
	int keyMods;
	volatile int joystickButtons;

	if ((playerIdxOrSentinel == g_localPlayer || playerIdxOrSentinel == -2) && g_inHangarReady != 0 &&
		g_filmPlaybackMode == 0) {
		return 0;
	}

	if (playerIdxOrSentinel < 0) {
		axisX = 0;
		axisY = 0;
		throttleRaw = 0;
		axisR = 0;
		joystickButtons = 0;
		keyMods = 0;
		if (g_joystickEnabled != 0) {
			joystickButtons = Joystick_PollRawAxesIfEnabled(&axisX, &axisY, &throttleRaw, &axisR, 0);
		}

#ifdef XWA_MODERN
		/* Modern mouse flight control, injected before the input frame is
		 * packed so films, replays, and net lockstep stay source-agnostic.
		 * The mouse overrides any axis it deflects; the joystick keeps the
		 * rest. Cockpit mouse look owns the pointer while enabled. */
		if (g_padlockMouseLookEnabled) {
			XwaMouseFlight_Suspend();
		} else if (XwaMouseFlight_Sample()) {
			int mouseAxisX;
			int mouseAxisY;
			int mouseAxisR;

			XwaMouseFlight_GetAxes(&mouseAxisX, &mouseAxisY, &mouseAxisR);
			if (mouseAxisX != 0) {
				axisX = mouseAxisX;
			}
			if (mouseAxisY != 0) {
				axisY = mouseAxisY;
			}
			if (mouseAxisR != 0) {
				axisR = mouseAxisR;
			}
			joystickButtons |= XwaMouseFlight_ButtonsMask();
		}
#endif

		key = 0;
		if (g_injectedKeyCount != 0) {
			key = g_injectedKeyStack[g_injectedKeyCount - 1];
			--g_injectedKeyCount;
		} else if (FlightInput_HasKeyReady()) {
			key = (uint16_t)FlightInput_GetNextKey();
			if (g_consoleEnabled == 0 || g_players[g_localPlayer].hudEnabled == 0 ||
				g_players[g_localPlayer].viewState.externalCameraActive != 0 ||
				g_players[g_localPlayer].mfd.consolePageAvailable == 0 ||
				g_players[g_localPlayer].mfd.page[g_players[g_localPlayer].mfd.activeIndex] != 7) {
				key = Console_ApplyKeyMacro(key, g_localPlayer);
			}
		}

#ifdef XWA_MODERN
		/* A short right-button tap emits the target-in-sight action, like the
		 * original joystick button 2 tap. Taps made while typing a message are
		 * consumed and discarded — KEY_ALT_1 would insert a taunt there. */
		if (key == 0 && !g_padlockMouseLookEnabled && XwaMouseFlight_TakeTargetTap() &&
			g_players[g_localPlayer].msgTypeId == 0) {
			key = KEY_ALT_1;
		}
#endif

		{
			int buttonBit;
			unsigned int buttonIdx;
			int keyModAlt2;
			int keyModAlt3;
			int hatMasks[4];
			int filteredMask;
			unsigned int i;

			keyModAlt2 = 0;
			keyModAlt3 = 0;
			hatMasks[0] = 0;
			hatMasks[1] = 0;
			hatMasks[2] = 0;
			hatMasks[3] = 0;
			buttonBit = 1;

			for (buttonIdx = 0; buttonIdx < 20u; ++buttonIdx) {
				uint16_t buttonKey;

				buttonKey = g_gameConfig.joyButtons[buttonIdx];
				if (buttonKey != 0 && (buttonBit & joystickButtons) != 0) {
					switch (buttonKey) {
						case 156:
							keyModAlt2 = 1;
							break;
						case 157:
							keyModAlt3 = 1;
							break;
						case 180:
							hatMasks[0] = buttonBit;
							break;
						case 182:
							hatMasks[1] = buttonBit;
							break;
						case 184:
							hatMasks[2] = buttonBit;
							break;
						case 186:
							hatMasks[3] = buttonBit;
							break;
						default:
							break;
					}

					if ((g_controlMask & buttonBit) == 0) {
						if (key != 0) {
							joystickButtons &= ~buttonBit;
						} else {
							key = buttonKey;
						}
					}
				}

				buttonBit <<= 1;
			}

			filteredMask = joystickButtons;
			g_controlMask = joystickButtons;
			for (i = 0; i < 4u; ++i) {
				if (hatMasks[i] != 0) {
					filteredMask &= ~hatMasks[i];
				}
			}
			g_controlMask = filteredMask;
			keyMods = keyModAlt2 + 2 * keyModAlt3;
		}

		if (key == 0) {
			int targetThrottle;

			targetThrottle = (int)(int8_t)throttleRaw + 128;
			if (g_throttleSmoothed == -1) {
				g_throttleSmoothed = targetThrottle;
			} else {
				int previousThrottle;
				int previousBucket;
				int throttleBucket;

				previousThrottle = g_throttleSmoothed;
				g_throttleSmoothed += (targetThrottle - g_throttleSmoothed) / 4;
				previousBucket = (previousThrottle + 8) / 16;
				throttleBucket = (g_throttleSmoothed + 8) / 16;
				if (previousBucket != throttleBucket) {
					if (throttleBucket < 0) {
						throttleBucket = 0;
					}
					if (throttleBucket > 16) {
						throttleBucket = 16;
					}
					key = g_throttleKeyByBucket[16 - throttleBucket];
				}
			}
		}

		g_ctrlAxisR = (int16_t)axisR;
		g_ctrlAxisX = (int16_t)axisX;
		g_actionKey = key;
		g_ctrlAxisY = (int16_t)axisY;
		g_keyMods = (uint16_t)keyMods;

		if (g_dinputKeyboardState[0x4f] != 0) {
			if (g_dinputKeyboardState[0x51] == 0) {
				g_ctrlAxisR = -127;
			}
		}
		if (g_dinputKeyboardState[0x51] != 0 && g_dinputKeyboardState[0x4f] == 0) {
			g_ctrlAxisR = 127;
		}

		return key;
	} else {
		FlightInputFrameRecord* replayInput;

		replayInput = &g_replayInputs[playerIdxOrSentinel];
		key = replayInput->key;
		g_ctrlAxisX = replayInput->axisX;
		g_ctrlAxisY = replayInput->axisY;
		g_ctrlAxisR = replayInput->axisR;
		g_actionKey = key;
		g_keyMods = (uint16_t)(replayInput->keyMods & 3u);
		return key;
	}
}

#pragma pack(push, 1)
typedef struct FlightSerializedObjectRecord {
	uint16_t yaw;
	uint16_t pitch;
	uint16_t roll;
	uint16_t angleD;
	uint16_t typeSpecificWord;
	uint8_t typeSpecificByte0;
	uint8_t typeSpecificByte1;
	uint32_t mobj;
	uint16_t objectSignature;
	uint16_t objectType;
	uint8_t genusId;
	uint8_t flightGroupIdx;
	uint8_t regionIdx;
	int32_t world_x;
	int32_t world_y;
	int32_t world_z;
	int32_t playerOwnerIdx;
} FlightSerializedObjectRecord;

typedef struct FlightSerializedMobileObject {
	uint8_t proximityList[101];
	uint16_t spinRateFrac;
	uint16_t spinDecelRate;
	uint16_t speed;
	uint16_t speedRemainder;
	uint16_t framesAlive;
	uint16_t sourceObjectType;
	uint8_t team;
	uint8_t nodeSwitchIndex;
	uint16_t ejectionSpawnCount;
	uint8_t velocityOverrideActive;
	uint16_t velocityOverrideSpeed;
	uint16_t velocityOverrideElapsed;
	uint16_t velocityOverrideDuration;
	float spinAxisX;
	float spinAxisY;
	float spinAxisZ;
	char moveVectorDirty;
	uint8_t orientMatrixDirty;
	uint32_t pWarheadGuidance;
	uint32_t pCraft;
	uint32_t pCharData;
	uint8_t state;
	uint8_t motionFlags;
	int32_t instanceExtent;
	int32_t simStateTimestamp;
	int32_t prevWorldX;
	int32_t prevWorldY;
	int32_t prevWorldZ;
	int16_t rollImpulseRate;
	int16_t spinRate;
	int16_t spinAngleQ16;
	int32_t damageAmount;
	int32_t lifetimeTimer;
	int16_t sourceObjIdx;
	int8_t iff;
	int32_t collisionObjIdx;
	int16_t velocityOverrideDirX;
	int16_t velocityOverrideDirY;
	int16_t velocityOverrideDirZ;
	int16_t renderOffsetX;
	int16_t renderOffsetY;
	int16_t renderOffsetZ;
	int16_t moveX;
	int16_t moveY;
	int16_t moveZ;
	int16_t cachedFwdX;
	int16_t cachedFwdY;
	int16_t cachedFwdZ;
	int16_t cachedSideX;
	int16_t cachedSideY;
	int16_t cachedSideZ;
	int16_t cachedUpX;
	int16_t cachedUpY;
	int16_t cachedUpZ;
} FlightSerializedMobileObject;

typedef struct FlightSerializedCraftData {
	uint8_t bytes[1017];
} FlightSerializedCraftData;

typedef struct FlightSerializedWarheadGuidanceState {
	uint8_t bytes[10];
} FlightSerializedWarheadGuidanceState;

typedef struct FlightSerializedMobileObjectCharData {
	uint8_t bytes[114];
} FlightSerializedMobileObjectCharData;

typedef struct FlightSerializedProximityList {
	uint16_t objIdx[16];
	uint8_t count;
	int32_t score[16];
	int32_t overflowScore;
} FlightSerializedProximityList;

typedef struct FlightSerializedMissionStateBlock {
	uint8_t flightMissionEndPending;
	uint8_t provingGroundsModeActive;
	uint8_t missionStateByte8053E6;
	uint8_t yardChallengeMode;
	uint16_t unusedMissionInitStateWord[6];
	MissionClock missionElapsedClock;
	MissionClock missionCountdownClock;
	uint16_t nextObjectSignature;
	uint32_t unusedMissionInitStateDword805406;
	uint8_t flightDifficulty;
	uint8_t flightCollisionsEnabled;
	uint8_t flightCraftJumpingEnabled;
	uint8_t missionRandomVariationEnabled;
	uint8_t unusedMissionOptionByte80540E;
	uint8_t flightLocatePlayersEnabled;
	uint8_t aiOpponentsEnabled;
	uint8_t playerFlightGroupWaveMode;
	uint8_t missionTimeLimitActive;
	uint8_t teamVictoryTimeLimitMinutes;
	uint8_t teamVictoryTimeLimitStarted;
	uint8_t craftImpactBounceEnabled;
	uint16_t teamFullKillCount[10];
	uint16_t teamSharedKillCount[10];
	uint16_t teamAssistKillCount[10];
	uint16_t teamCraftLossCount[10];
	uint16_t teamFgInspectedCraftCount[10][192];
	uint16_t teamFgTransferCounter[10][192];
	uint8_t teamFgDesignationCode[10][192];
	uint8_t globalPrimaryGoalStatus;
	uint8_t globalGoalStatusUnusedBytes[2];
	uint8_t globalBonusGoalStatus;
	uint8_t teamGlobalGoalState[10][3];
	uint8_t teamGoalStatus[10][3];
	uint16_t globalGoalTriggerCurrentCount[10][4][3];
	uint16_t globalGoalTriggerTotalCount[10][4][3];
	uint8_t teamHasCountableCraft[10];
	uint8_t teamActiveGoalSequence[10];
	uint8_t teamReinforcementCalled[10];
	uint8_t missionMessageTriggered[64];
	int32_t missionGlobalUnitCraftCount[41];
	MissionRegionHyperPointTables missionRegionHyperPoints;
	int32_t missionFormatVersion;
	int32_t connectedPlayerCount;
	int32_t maxConnectedPlayerCountThisMission;
	int32_t teamMissionBonusScoreTenths[10];
	int32_t teamMissionScore[10];
	int32_t teamMissionCompletionTimeSeconds[10];
	int32_t missionMessageDelayCountdown[63];
	int32_t activeMissionRegionCount;
} FlightSerializedMissionStateBlock;
#pragma pack(pop)

typedef char xwa_flight_serialized_object_record_size[(sizeof(FlightSerializedObjectRecord) == 39) ? 1 : -1];
typedef char xwa_flight_serialized_mobile_object_size[(sizeof(FlightSerializedMobileObject) == 229) ? 1 : -1];
typedef char xwa_flight_serialized_craft_data_size[(sizeof(FlightSerializedCraftData) == 1017) ? 1 : -1];
typedef char xwa_flight_serialized_warhead_guidance_state_size
	[(sizeof(FlightSerializedWarheadGuidanceState) == 10) ? 1 : -1];
typedef char xwa_flight_serialized_mobile_object_char_data_size
	[(sizeof(FlightSerializedMobileObjectCharData) == 114) ? 1 : -1];
typedef char
	xwa_flight_serialized_proximity_list_size[(sizeof(FlightSerializedProximityList) == 101) ? 1 : -1];
typedef char xwa_flight_serialized_mission_state_block_size
	[(sizeof(FlightSerializedMissionStateBlock) == 0x2D32) ? 1 : -1];

static FlightSerializedMissionStateBlock g_flightSerializedMissionStateBlock;
static uint8_t g_flightSerializedYardContextBlock[sizeof(YardContext)];

static void* FlightWorldState_DecodeOffset(uint32_t encodedOffset, void* base) {
	if (encodedOffset == 0) {
		return NULL;
	}

	return (uint8_t*)base + encodedOffset - 1u;
}

static uint32_t FlightWorldState_EncodeOffset(const void* ptr, const void* base) {
	if (ptr == NULL) {
		return 0;
	}

	return (uint32_t)((const uint8_t*)ptr - (const uint8_t*)base + 1u);
}

static void FlightWorldState_WriteU32(uint8_t* dst, uint32_t value) { memcpy(dst, &value, sizeof(value)); }

static void FlightWorldState_WriteBlock(uint8_t** cursor, const void* src, size_t size) {
	memcpy(*cursor, src, size);
	*cursor += size;
}

static void FlightWorldState_SerializeObjectRecord(FlightSerializedObjectRecord* dst,
												   const ObjectRecord* src) {
	dst->objectSignature = src->objectSignature;
	dst->objectType = (uint16_t)src->objectType;
	dst->genusId = (uint8_t)src->genusId;
	dst->flightGroupIdx = src->flightGroupIdx;
	dst->regionIdx = src->regionIdx;
	dst->world_x = src->world_x;
	dst->world_y = src->world_y;
	dst->world_z = src->world_z;
	dst->yaw = src->yaw;
	dst->pitch = src->pitch;
	dst->roll = src->roll;
	dst->angleD = src->angleD;
	dst->typeSpecificWord = src->typeSpecificWord;
	dst->typeSpecificByte0 = src->typeSpecificByte[0];
	dst->typeSpecificByte1 = src->typeSpecificByte[1];
	dst->playerOwnerIdx = src->playerOwnerIdx;
	dst->mobj = FlightWorldState_EncodeOffset(src->mobj, g_mobileObjectPoolBase);
}

static void FlightWorldState_SerializeProximityList(uint8_t dstBytes[101],
													const MobileObjectProximityList* src) {
	FlightSerializedProximityList* dst;

	dst = (FlightSerializedProximityList*)dstBytes;
	dst->count = src->count;
	memcpy(dst->score, src->score, sizeof(dst->score));
	memcpy(dst->objIdx, src->objIdx, sizeof(dst->objIdx));
	dst->overflowScore = src->overflowScore;
}

static void FlightWorldState_SerializeMobileObject(FlightSerializedMobileObject* dst,
												   const MobileObject* src) {
	dst->state = src->state;
	dst->motionFlags = src->motionFlags;
	dst->instanceExtent = src->instanceExtent;
	dst->simStateTimestamp = src->simStateTimestamp;
	dst->prevWorldX = src->prevWorldX;
	dst->prevWorldY = src->prevWorldY;
	dst->prevWorldZ = src->prevWorldZ;
	FlightWorldState_SerializeProximityList(dst->proximityList, &src->proximityList);
	dst->rollImpulseRate = src->rollImpulseRate;
	dst->spinRate = src->spinRate;
	dst->spinRateFrac = src->spinRateFrac;
	dst->spinDecelRate = src->spinDecelRate;
	dst->spinAngleQ16 = src->spinAngleQ16;
	dst->speed = src->speed;
	dst->speedRemainder = src->speedRemainder;
	dst->damageAmount = src->damageAmount;
	dst->lifetimeTimer = src->lifetimeTimer;
	dst->framesAlive = src->framesAlive;
	dst->sourceObjIdx = src->sourceObjIdx;
	dst->sourceObjectType = src->sourceObjectType;
	dst->iff = src->iff;
	dst->team = src->team;
	dst->nodeSwitchIndex = src->nodeSwitchIndex;
	dst->ejectionSpawnCount = src->ejectionSpawnCount;
	dst->collisionObjIdx = src->collisionObjIdx;
	dst->velocityOverrideActive = src->velocityOverrideActive;
	dst->velocityOverrideSpeed = src->velocityOverrideSpeed;
	dst->velocityOverrideElapsed = src->velocityOverrideElapsed;
	dst->velocityOverrideDuration = src->velocityOverrideDuration;
	dst->velocityOverrideDirX = src->velocityOverrideDirX;
	dst->velocityOverrideDirY = src->velocityOverrideDirY;
	dst->velocityOverrideDirZ = src->velocityOverrideDirZ;
	dst->renderOffsetX = src->renderOffsetX;
	dst->renderOffsetY = src->renderOffsetY;
	dst->renderOffsetZ = src->renderOffsetZ;
	dst->spinAxisX = src->spinAxisX;
	dst->spinAxisY = src->spinAxisY;
	dst->spinAxisZ = src->spinAxisZ;
	dst->moveVectorDirty = src->moveVectorDirty;
	dst->moveX = src->moveX;
	dst->moveY = src->moveY;
	dst->moveZ = src->moveZ;
	dst->orientMatrixDirty = src->orientMatrixDirty;
	dst->cachedFwdX = src->cachedFwdX;
	dst->cachedFwdY = src->cachedFwdY;
	dst->cachedFwdZ = src->cachedFwdZ;
	dst->cachedSideX = src->cachedSideX;
	dst->cachedSideY = src->cachedSideY;
	dst->cachedSideZ = src->cachedSideZ;
	dst->cachedUpX = src->cachedUpX;
	dst->cachedUpY = src->cachedUpY;
	dst->cachedUpZ = src->cachedUpZ;
	dst->pWarheadGuidance = FlightWorldState_EncodeOffset(src->pWarheadGuidance, g_warheadGuidancePoolBase);
	dst->pCraft = FlightWorldState_EncodeOffset(src->pCraft, g_craftDataPoolBase);
	dst->pCharData = FlightWorldState_EncodeOffset(src->pCharData, g_mobileObjectCharDataPool);
}

static void FlightWorldState_SerializeCraftData(FlightSerializedCraftData* dst, const CraftData* src) {
	uint32_t encodedLink;
	enum { FLIGHT_SERIALIZED_CRAFT_EFFECTIVE_LINK_OFFSET = 0x3F5 };

	memset(dst, 0, sizeof(*dst));
	memcpy(dst->bytes, src, offsetof(CraftData, effectiveAiObjectLink));
	encodedLink = FlightWorldState_EncodeOffset(src->effectiveAiObjectLink, g_objectTable);
	FlightWorldState_WriteU32(&dst->bytes[FLIGHT_SERIALIZED_CRAFT_EFFECTIVE_LINK_OFFSET], encodedLink);
}

static void FlightWorldState_SerializeWarheadGuidance(FlightSerializedWarheadGuidanceState* dst,
													  const WarheadGuidanceState* src) {
	memcpy(&dst->bytes[0], &src->sourcePlayerIdx, 4);
	memcpy(&dst->bytes[4], &src->targetObjIdx, 4);
	memcpy(&dst->bytes[8], &src->minSpeed, 2);
}

static void FlightWorldState_BuildMissionStateBlock(FlightSerializedMissionStateBlock* block) {
	memset(block, 0, sizeof(*block));
	block->missionFormatVersion = g_missionFormatVersion;
	block->provingGroundsModeActive = g_provingGroundsModeActive;
	block->yardChallengeMode = g_yardChallengeMode;
	block->missionElapsedClock = g_missionElapsedClock;
	block->missionCountdownClock = g_missionCountdownClock;
	block->missionTimeLimitActive = g_missionTimeLimitActive;
	block->flightMissionEndPending = g_flightMissionEndPending;
	block->teamVictoryTimeLimitMinutes = g_teamVictoryTimeLimitMinutes;
	block->teamVictoryTimeLimitStarted = g_teamVictoryTimeLimitStarted;
	block->flightDifficulty = g_flightDifficulty;
	block->flightCollisionsEnabled = g_flightCollisionsEnabled;
	block->nextObjectSignature = g_nextObjectSignature;
	memcpy(block->teamMissionBonusScoreTenths,
		   g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS],
		   sizeof(g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS]));
	memcpy(block->teamMissionScore, g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION],
		   sizeof(g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION]));
	memcpy(block->teamFullKillCount, g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_FULL],
		   sizeof(g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_FULL]));
	memcpy(block->teamSharedKillCount, g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_SHARED],
		   sizeof(g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_SHARED]));
	memcpy(block->teamAssistKillCount, g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_ASSIST],
		   sizeof(g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_ASSIST]));
	memcpy(block->teamCraftLossCount, g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_LOSS],
		   sizeof(g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_LOSS]));
	memcpy(block->teamFgInspectedCraftCount,
		   g_missionFlightRuntimeState.teamFgCounters[TEAM_FG_COUNTER_INSPECTED],
		   sizeof(g_missionFlightRuntimeState.teamFgCounters[TEAM_FG_COUNTER_INSPECTED]));
	memcpy(block->teamFgTransferCounter, g_missionFlightRuntimeState.teamFgCounters[TEAM_FG_COUNTER_TRANSFER],
		   sizeof(g_missionFlightRuntimeState.teamFgCounters[TEAM_FG_COUNTER_TRANSFER]));
	memcpy(block->teamFgDesignationCode, g_missionFlightRuntimeState.teamFgDesignationCode,
		   sizeof(g_missionFlightRuntimeState.teamFgDesignationCode));
	memcpy(block->missionGlobalUnitCraftCount, g_missionGlobalUnitCraftCount,
		   sizeof(g_missionGlobalUnitCraftCount));
	block->activeMissionRegionCount = g_activeMissionRegionCount;
	memcpy(&block->missionRegionHyperPoints, &g_missionRegionHyperPoints, sizeof(g_missionRegionHyperPoints));
}

static void FlightWorldState_RestoreObjectRecord(ObjectRecord* dst, const FlightSerializedObjectRecord* src) {
	dst->objectSignature = src->objectSignature;
	dst->objectType = (ObjectTypeId)src->objectType;
	dst->genusId = src->genusId;
	dst->flightGroupIdx = src->flightGroupIdx;
	dst->regionIdx = src->regionIdx;
	dst->world_x = src->world_x;
	dst->world_y = src->world_y;
	dst->world_z = src->world_z;
	dst->yaw = src->yaw;
	dst->pitch = src->pitch;
	dst->roll = src->roll;
	dst->angleD = src->angleD;
	dst->typeSpecificWord = src->typeSpecificWord;
	dst->typeSpecificByte[0] = src->typeSpecificByte0;
	dst->typeSpecificByte[1] = src->typeSpecificByte1;
	dst->playerOwnerIdx = src->playerOwnerIdx;
	dst->mobj = (MobileObject*)FlightWorldState_DecodeOffset(src->mobj, g_mobileObjectPoolBase);
}

static void FlightWorldState_RestoreProximityList(MobileObjectProximityList* dst,
												  const uint8_t srcBytes[101]) {
	const FlightSerializedProximityList* src;

	src = (const FlightSerializedProximityList*)srcBytes;
	dst->count = src->count;
	memcpy(dst->score, src->score, sizeof(dst->score));
	memcpy(dst->objIdx, src->objIdx, sizeof(dst->objIdx));
	dst->overflowScore = src->overflowScore;
}

static void FlightWorldState_RestoreMobileObject(MobileObject* dst, const FlightSerializedMobileObject* src) {
	dst->state = src->state;
	dst->motionFlags = src->motionFlags;
	dst->instanceExtent = src->instanceExtent;
	dst->simStateTimestamp = src->simStateTimestamp;
	dst->prevWorldX = src->prevWorldX;
	dst->prevWorldY = src->prevWorldY;
	dst->prevWorldZ = src->prevWorldZ;
	FlightWorldState_RestoreProximityList(&dst->proximityList, src->proximityList);
	dst->rollImpulseRate = src->rollImpulseRate;
	dst->spinRate = src->spinRate;
	dst->spinRateFrac = src->spinRateFrac;
	dst->spinDecelRate = src->spinDecelRate;
	dst->spinAngleQ16 = src->spinAngleQ16;
	dst->speed = src->speed;
	dst->speedRemainder = src->speedRemainder;
	dst->damageAmount = src->damageAmount;
	dst->lifetimeTimer = src->lifetimeTimer;
	dst->framesAlive = src->framesAlive;
	dst->sourceObjIdx = src->sourceObjIdx;
	dst->sourceObjectType = src->sourceObjectType;
	dst->iff = src->iff;
	dst->team = src->team;
	dst->nodeSwitchIndex = src->nodeSwitchIndex;
	dst->ejectionSpawnCount = src->ejectionSpawnCount;
	dst->collisionObjIdx = src->collisionObjIdx;
	dst->velocityOverrideActive = src->velocityOverrideActive;
	dst->velocityOverrideSpeed = src->velocityOverrideSpeed;
	dst->velocityOverrideElapsed = src->velocityOverrideElapsed;
	dst->velocityOverrideDuration = src->velocityOverrideDuration;
	dst->velocityOverrideDirX = src->velocityOverrideDirX;
	dst->velocityOverrideDirY = src->velocityOverrideDirY;
	dst->velocityOverrideDirZ = src->velocityOverrideDirZ;
	dst->renderOffsetX = src->renderOffsetX;
	dst->renderOffsetY = src->renderOffsetY;
	dst->renderOffsetZ = src->renderOffsetZ;
	dst->spinAxisX = src->spinAxisX;
	dst->spinAxisY = src->spinAxisY;
	dst->spinAxisZ = src->spinAxisZ;
	dst->moveVectorDirty = src->moveVectorDirty;
	dst->moveX = src->moveX;
	dst->moveY = src->moveY;
	dst->moveZ = src->moveZ;
	dst->orientMatrixDirty = src->orientMatrixDirty;
	dst->cachedFwdX = src->cachedFwdX;
	dst->cachedFwdY = src->cachedFwdY;
	dst->cachedFwdZ = src->cachedFwdZ;
	dst->cachedSideX = src->cachedSideX;
	dst->cachedSideY = src->cachedSideY;
	dst->cachedSideZ = src->cachedSideZ;
	dst->cachedUpX = src->cachedUpX;
	dst->cachedUpY = src->cachedUpY;
	dst->cachedUpZ = src->cachedUpZ;
	dst->pWarheadGuidance = (WarheadGuidanceState*)FlightWorldState_DecodeOffset(src->pWarheadGuidance,
																				 g_warheadGuidancePoolBase);
	dst->pCraft = (CraftData*)FlightWorldState_DecodeOffset(src->pCraft, g_craftDataPoolBase);
	dst->pCharData =
		(MobileObjectCharData*)FlightWorldState_DecodeOffset(src->pCharData, g_mobileObjectCharDataPool);
}

static uint32_t FlightWorldState_ReadU32(const uint8_t* src) {
	uint32_t value;

	memcpy(&value, src, sizeof(value));
	return value;
}

static void FlightWorldState_RestoreCraftData(CraftData* dst, const FlightSerializedCraftData* src) {
	uint32_t encodedLink;
	enum { FLIGHT_SERIALIZED_CRAFT_EFFECTIVE_LINK_OFFSET = 0x3F5 };

	memcpy(dst, src->bytes, offsetof(CraftData, effectiveAiObjectLink));
	encodedLink = FlightWorldState_ReadU32(&src->bytes[FLIGHT_SERIALIZED_CRAFT_EFFECTIVE_LINK_OFFSET]);
	dst->effectiveAiObjectLink = (ObjectRecord*)FlightWorldState_DecodeOffset(encodedLink, g_objectTable);
}

static void FlightWorldState_RestoreWarheadGuidance(WarheadGuidanceState* dst,
													const FlightSerializedWarheadGuidanceState* src) {
	memcpy(&dst->sourcePlayerIdx, &src->bytes[0], 4);
	memcpy(&dst->targetObjIdx, &src->bytes[4], 4);
	memcpy(&dst->minSpeed, &src->bytes[8], 2);
}

static void FlightWorldState_ApplyMissionStateBlock(const FlightSerializedMissionStateBlock* block) {
	g_missionFormatVersion = block->missionFormatVersion;
	g_provingGroundsModeActive = block->provingGroundsModeActive;
	g_yardChallengeMode = block->yardChallengeMode;
	g_missionElapsedClock = block->missionElapsedClock;
	g_missionCountdownClock = block->missionCountdownClock;
	g_missionTimeLimitActive = block->missionTimeLimitActive;
	g_flightMissionEndPending = block->flightMissionEndPending;
	g_teamVictoryTimeLimitMinutes = block->teamVictoryTimeLimitMinutes;
	g_teamVictoryTimeLimitStarted = block->teamVictoryTimeLimitStarted;
	g_flightDifficulty = block->flightDifficulty;
	g_flightCollisionsEnabled = block->flightCollisionsEnabled;
	g_nextObjectSignature = block->nextObjectSignature;
	memcpy(g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS],
		   block->teamMissionBonusScoreTenths,
		   sizeof(g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS]));
	memcpy(g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION], block->teamMissionScore,
		   sizeof(g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION]));
	memcpy(g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_FULL], block->teamFullKillCount,
		   sizeof(g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_FULL]));
	memcpy(g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_SHARED], block->teamSharedKillCount,
		   sizeof(g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_SHARED]));
	memcpy(g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_ASSIST], block->teamAssistKillCount,
		   sizeof(g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_ASSIST]));
	memcpy(g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_LOSS], block->teamCraftLossCount,
		   sizeof(g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_LOSS]));
	memcpy(g_missionFlightRuntimeState.teamFgCounters[TEAM_FG_COUNTER_INSPECTED],
		   block->teamFgInspectedCraftCount,
		   sizeof(g_missionFlightRuntimeState.teamFgCounters[TEAM_FG_COUNTER_INSPECTED]));
	memcpy(g_missionFlightRuntimeState.teamFgCounters[TEAM_FG_COUNTER_TRANSFER], block->teamFgTransferCounter,
		   sizeof(g_missionFlightRuntimeState.teamFgCounters[TEAM_FG_COUNTER_TRANSFER]));
	memcpy(g_missionFlightRuntimeState.teamFgDesignationCode, block->teamFgDesignationCode,
		   sizeof(g_missionFlightRuntimeState.teamFgDesignationCode));
	g_activeMissionRegionCount = block->activeMissionRegionCount;
	memcpy(&g_missionRegionHyperPoints, &block->missionRegionHyperPoints, sizeof(g_missionRegionHyperPoints));
}

// FUNCTION: XWA 0x4F53A0
void Flight_FreeWorldStateBuffers(void) {
	Memory_FreeHandle("WORLDSTATEDATA", g_worldStateHandle);
	g_worldStateHandle = 0;
	g_worldStateBuffer = NULL;
	Memory_FreeHandle("DUPWORLDSTATEDATA", g_worldStateDupHandle);
	g_worldStateDupHandle = 0;
	g_worldStateDupBuffer = NULL;
}

// FUNCTION: XWA 0x4F52A0
void Flight_AllocWorldStateBuffers(void) {
	size_t bufferSize;
	size_t craftDataSize;
	unsigned int baseSize;

	baseSize =
		41u * g_regionObjectSlotEnd + 229u * g_regionMainObjectSlotsTotal +
		10u * (g_projectileObjectSlotsTotal + 402u * (uint32_t)(int16_t)g_missionHeader.numFlightGroups) +
		114u * g_mobileObjectCharDataCount;
	craftDataSize = 1017u * g_craftObjectSlotsTotal;
	bufferSize = baseSize + craftDataSize + 20778u;
	if (g_provingGroundsModeActive != 0) {
		bufferSize += 3964u;
	} else {
		bufferSize = craftDataSize + baseSize + 20778u;
	}
	GameRand_GetPrimarySeed();
	bufferSize += 3023u * (uint32_t)g_flightPlayerCount + 22086u;

	g_worldStateHandle = Memory_AllocHandle("WORLDSTATEDATA", bufferSize);
	if (g_worldStateHandle == 0) {
		FeDiskIo_FatalError(0);
	}
	g_worldStateBuffer = (uint8_t*)Memory_LockHandle(g_worldStateHandle);

	g_worldStateDupHandle = Memory_AllocHandle("DUPWORLDSTATEDATA", bufferSize);
	if (g_worldStateDupHandle == 0) {
		FeDiskIo_FatalError(0);
	}
	g_worldStateDupBuffer = (uint8_t*)Memory_LockHandle(g_worldStateDupHandle);
}

// FUNCTION: XWA 0x4F5280
uint8_t* Flight_GetDuplicateWorldStateBuffer(void) { return g_worldStateDupBuffer; }

// FUNCTION: XWA 0x4F5290
int Flight_GetSerializedWorldStateSize(void) { return worldStateSize; }

// FUNCTION: XWA 0x4F60E0
int Flight_ComputeWorldStateResyncSegmentSize(int size) { return size / 124; }

// FUNCTION: XWA 0x4F9190
void FlightNet_CopyWorldStateResyncChunk(const void* chunkData, int dstOffset, uint32_t chunkSize) {
	memcpy(&g_worldStateDupBuffer[dstOffset], chunkData, chunkSize);
}

// FUNCTION: XWA 0x4F6100
int Flight_BuildWorldStateResyncSegmentChecksums(int* outChecksums, uint8_t* worldState, int worldStateSize) {
	int segmentSize;
	int segmentCount;

	segmentSize = worldStateSize / 124;
	if (segmentSize == 0) {
		segmentSize = worldStateSize;
	}

	segmentCount = 125;
	do {
		uint32_t checksum;
		int bytesInSegment;

		checksum = 0;
		if (segmentSize > 0) {
			bytesInSegment = segmentSize;
			do {
				if (worldStateSize != 0) {
					checksum = (uint32_t)(checksum + *worldState++);
#ifdef XWA_MODERN
					checksum = (checksum << 1) + (checksum >> 31);
#else
					checksum = _rotl(checksum, 1);
#endif
					--worldStateSize;
				}
				--bytesInSegment;
			} while (bytesInSegment != 0);
		}

		*outChecksums++ = (int)checksum;
		--segmentCount;
	} while (segmentCount != 0);

	return 125;
}

#pragma pack(push, 1)
typedef struct FlightWorldStatePresenceObjectRecord {
	uint8_t bytesBeforeMobj[0x23];
	uint32_t mobj;
} FlightWorldStatePresenceObjectRecord;

typedef struct FlightWorldStatePresenceMobileObject {
	uint8_t bytesBeforeWarheadGuidance[0xd9];
	uint32_t pWarheadGuidance;
	uint32_t pCraft;
	uint32_t pCharData;
} FlightWorldStatePresenceMobileObject;
#pragma pack(pop)

typedef char xwa_flight_presence_object_mobj_offset
	[(offsetof(FlightWorldStatePresenceObjectRecord, mobj) == 0x23) ? 1 : -1];
typedef char xwa_flight_presence_mobile_warhead_offset
	[(offsetof(FlightWorldStatePresenceMobileObject, pWarheadGuidance) == 0xd9) ? 1 : -1];
typedef char xwa_flight_presence_mobile_craft_offset
	[(offsetof(FlightWorldStatePresenceMobileObject, pCraft) == 0xdd) ? 1 : -1];
typedef char xwa_flight_presence_mobile_char_data_offset
	[(offsetof(FlightWorldStatePresenceMobileObject, pCharData) == 0xe1) ? 1 : -1];

// FUNCTION: XWA 0x4F6170
int Flight_BuildWorldStateObjectPresenceMap(uint8_t* outMap, uint8_t* worldState) {
	uint8_t* outStart;
	uint32_t slot;
	int zeroRun;

	outStart = outMap;
	*(uint32_t*)outMap = g_regionObjectSlotEnd;
	outMap += sizeof(g_regionObjectSlotEnd);
	zeroRun = 0;

	for (slot = 0; slot < g_regionObjectSlotEnd; ++slot) {
		int objectSignature;
		uint8_t presence;

		presence = 0;
		objectSignature = *(uint16_t*)worldState;
		worldState += sizeof(uint16_t);

		if (objectSignature != 0) {
			FlightWorldStatePresenceObjectRecord* objectRecord;

			objectRecord = (FlightWorldStatePresenceObjectRecord*)worldState;
			worldState += sizeof(FlightSerializedObjectRecord);
			presence = 0x01;

			if (objectRecord->mobj != 0) {
				FlightWorldStatePresenceMobileObject* mobileObject;

				mobileObject = (FlightWorldStatePresenceMobileObject*)worldState;
				worldState += sizeof(FlightSerializedMobileObject);
				presence = 0x03;

				if (mobileObject->pCraft != 0) {
					presence |= 0x04;
					worldState += sizeof(FlightSerializedCraftData);
				}

				if (mobileObject->pWarheadGuidance != 0) {
					presence |= 0x08;
					worldState += sizeof(FlightSerializedWarheadGuidanceState);
				}

				if (mobileObject->pCharData != 0) {
					presence |= 0x10;
					worldState += sizeof(FlightSerializedMobileObjectCharData);
				}
			}
		}

		if (!presence) {
			++zeroRun;
			if (zeroRun >= 126) {
				*outMap++ = (uint8_t)(zeroRun | 0x80);
				zeroRun = 0;
			}
		} else {
			if (zeroRun != 0) {
				*outMap++ = (uint8_t)(zeroRun | 0x80);
				zeroRun = 0;
			}
			*outMap++ = presence;
		}
	}

	if (zeroRun != 0) {
		*outMap++ = (uint8_t)(zeroRun | 0x80);
	}

	return (int)(outMap - outStart);
}

#ifndef XWA_MODERN
#pragma function(memcpy)
#define FLIGHT_WORLD_STATE_MOVE memcpy
#else
#define FLIGHT_WORLD_STATE_MOVE memmove
#endif
// FUNCTION: XWA 0x4F6240
int Flight_ApplyWorldStateObjectPresenceMap(const uint8_t* presenceMap) {
	uint8_t* cursor;
	uint8_t* end;
	int mapSlotLimit;
	int zeroRunRemaining;
	uint32_t slot;
	enum {
		FLIGHT_WORLDSTATE_HAS_OBJECT = 0x01,
		FLIGHT_WORLDSTATE_HAS_MOBILE = 0x02,
		FLIGHT_WORLDSTATE_HAS_CRAFT = 0x04,
		FLIGHT_WORLDSTATE_HAS_WARHEAD = 0x08,
		FLIGHT_WORLDSTATE_HAS_CHAR_DATA = 0x10,
		FLIGHT_WORLDSTATE_OBJECT_MOBJ_OFFSET = 0x23,
		FLIGHT_WORLDSTATE_MOBILE_WARHEAD_OFFSET = 0xd9,
		FLIGHT_WORLDSTATE_MOBILE_CRAFT_OFFSET = 0xdd,
		FLIGHT_WORLDSTATE_MOBILE_CHAR_DATA_OFFSET = 0xe1
	};

	cursor = g_worldStateDupBuffer;
	end = cursor + worldStateSize;
	mapSlotLimit = *(const int*)presenceMap;
	presenceMap += sizeof(mapSlotLimit);
	zeroRunRemaining = 0;

	for (slot = 0; slot < g_regionObjectSlotEnd; ++slot) {
		uint8_t presence;
		int objectSignature;

		if (zeroRunRemaining != 0) {
			--zeroRunRemaining;
			presence = 0;
		} else {
			if ((int)slot >= mapSlotLimit) {
				break;
			}

			presence = *presenceMap++;
			if ((presence & 0x80u) != 0) {
				zeroRunRemaining = presence & 0x7f;
				presence = 0;
				--zeroRunRemaining;
			}
		}

		objectSignature = *(uint16_t*)cursor;
		cursor += sizeof(uint16_t);
		if (objectSignature != 0) {
			if ((presence & FLIGHT_WORLDSTATE_HAS_OBJECT) != 0) {
				uint8_t* objectRecord;

				objectRecord = cursor;
				cursor += sizeof(FlightSerializedObjectRecord);
				if (*(uint32_t*)&objectRecord[FLIGHT_WORLDSTATE_OBJECT_MOBJ_OFFSET] != 0) {
					if ((presence & FLIGHT_WORLDSTATE_HAS_MOBILE) != 0) {
						uint8_t* mobileObject;

						mobileObject = cursor;
						cursor += sizeof(FlightSerializedMobileObject);

						if (*(uint32_t*)&mobileObject[FLIGHT_WORLDSTATE_MOBILE_CRAFT_OFFSET] != 0) {
							if ((presence & FLIGHT_WORLDSTATE_HAS_CRAFT) != 0) {
								cursor += sizeof(FlightSerializedCraftData);
							} else {
								uint8_t* blockStart;

								blockStart = cursor;
								cursor += sizeof(FlightSerializedCraftData);
								FLIGHT_WORLD_STATE_MOVE(blockStart, cursor, (size_t)(end - cursor));
								cursor = blockStart;
								end -= sizeof(FlightSerializedCraftData);
							}
						} else if ((presence & FLIGHT_WORLDSTATE_HAS_CRAFT) != 0) {
							uint8_t* blockStart;

							blockStart = cursor;
							cursor += sizeof(FlightSerializedCraftData);
							FLIGHT_WORLD_STATE_MOVE(cursor, blockStart, (size_t)(end - blockStart));
							memset(blockStart, 0, (size_t)(cursor - blockStart));
							end += sizeof(FlightSerializedCraftData);
						}

						if (*(uint32_t*)&mobileObject[FLIGHT_WORLDSTATE_MOBILE_WARHEAD_OFFSET] != 0) {
							if ((presence & FLIGHT_WORLDSTATE_HAS_WARHEAD) != 0) {
								cursor += sizeof(FlightSerializedWarheadGuidanceState);
							} else {
								uint8_t* blockStart;

								blockStart = cursor;
								cursor += sizeof(FlightSerializedWarheadGuidanceState);
								FLIGHT_WORLD_STATE_MOVE(blockStart, cursor, (size_t)(end - cursor));
								cursor = blockStart;
								end -= sizeof(FlightSerializedWarheadGuidanceState);
							}
						} else if ((presence & FLIGHT_WORLDSTATE_HAS_WARHEAD) != 0) {
							uint8_t* blockStart;

							blockStart = cursor;
							cursor += sizeof(FlightSerializedWarheadGuidanceState);
							FLIGHT_WORLD_STATE_MOVE(cursor, blockStart, (size_t)(end - blockStart));
							memset(blockStart, 0, (size_t)(cursor - blockStart));
							end += sizeof(FlightSerializedWarheadGuidanceState);
						}

						if (*(uint32_t*)&mobileObject[FLIGHT_WORLDSTATE_MOBILE_CHAR_DATA_OFFSET] != 0) {
							if ((presence & FLIGHT_WORLDSTATE_HAS_CHAR_DATA) != 0) {
								cursor += sizeof(FlightSerializedMobileObjectCharData);
							} else {
								uint8_t* blockStart;

								blockStart = cursor;
								cursor += sizeof(FlightSerializedMobileObjectCharData);
								FLIGHT_WORLD_STATE_MOVE(blockStart, cursor, (size_t)(end - cursor));
								cursor = blockStart;
								end -= sizeof(FlightSerializedMobileObjectCharData);
							}
						} else if ((presence & FLIGHT_WORLDSTATE_HAS_CHAR_DATA) != 0) {
							uint8_t* blockStart;

							blockStart = cursor;
							cursor += sizeof(FlightSerializedMobileObjectCharData);
							FLIGHT_WORLD_STATE_MOVE(cursor, blockStart, (size_t)(end - blockStart));
							memset(blockStart, 0, (size_t)(cursor - blockStart));
							end += sizeof(FlightSerializedMobileObjectCharData);
						}
					} else {
						uint8_t* blockStart;

						blockStart = cursor;
						cursor += sizeof(FlightSerializedMobileObject);
						FLIGHT_WORLD_STATE_MOVE(blockStart, cursor, (size_t)(end - cursor));
						end -= sizeof(FlightSerializedMobileObject);
						cursor = blockStart;
					}
				} else if ((presence & FLIGHT_WORLDSTATE_HAS_MOBILE) != 0) {
					uint8_t* blockStart;

					blockStart = cursor;
					cursor += sizeof(FlightSerializedMobileObject);
					FLIGHT_WORLD_STATE_MOVE(cursor, blockStart, (size_t)(end - blockStart));
					memset(blockStart, 0, (size_t)(cursor - blockStart));
					end += sizeof(FlightSerializedMobileObject);
				}
			} else {
				uint8_t* blockStart;

				blockStart = cursor;
				cursor += sizeof(FlightSerializedObjectRecord);
				FLIGHT_WORLD_STATE_MOVE(blockStart, cursor, (size_t)(end - cursor));
				end -= sizeof(FlightSerializedObjectRecord);
				cursor = blockStart;
			}
		} else if ((presence & FLIGHT_WORLDSTATE_HAS_OBJECT) != 0) {
			uint8_t* blockStart;

			blockStart = cursor;
			cursor += sizeof(FlightSerializedObjectRecord);
			FLIGHT_WORLD_STATE_MOVE(cursor, blockStart, (size_t)(end - blockStart));
			memset(blockStart, 0, (size_t)(cursor - blockStart));
			end += sizeof(FlightSerializedObjectRecord);
		}
	}

	worldStateSize = (int)(end - g_worldStateDupBuffer);
	return (int)(intptr_t)g_worldStateDupBuffer;
}
#undef FLIGHT_WORLD_STATE_MOVE
#ifndef XWA_MODERN
#pragma intrinsic(memcpy)
#endif

// FUNCTION: XWA 0x4F53F0
int Flight_SaveWorldState(void) {
#ifndef XWA_MODERN
	uint8_t* cursor;
	uint8_t* tailCursor;
	uint32_t slot;
	uint16_t randSeed;

	cursor = g_worldStateBuffer;
	for (slot = 0; slot < g_regionObjectSlotEnd; ++slot) {
		*(uint16_t*)cursor = (uint16_t)g_objectTable[slot].objectType;
		cursor += sizeof(uint16_t);

		if (g_objectTable[slot].objectType != OBJ_None) {
			uint16_t recordFlags;

			recordFlags = g_modelTypeTable[(uint16_t)g_objectTable[slot].objectType].recordFlags;
			if (g_objectTable[slot].mobj != NULL) {
				g_objectTable[slot].mobj =
					(MobileObject*)((uint8_t*)g_objectTable[slot].mobj - (uint32_t)g_mobileObjectPoolBase);
				g_objectTable[slot].mobj = (MobileObject*)((uint8_t*)g_objectTable[slot].mobj + 1u);
			}

			memcpy(cursor, &g_objectTable[slot], sizeof(FlightSerializedObjectRecord));
			cursor += sizeof(FlightSerializedObjectRecord);

			if (g_objectTable[slot].mobj != NULL) {
				g_objectTable[slot].mobj = (MobileObject*)((uint8_t*)g_objectTable[slot].mobj - 1u);
				g_objectTable[slot].mobj =
					(MobileObject*)((uint8_t*)g_objectTable[slot].mobj + (uint32_t)g_mobileObjectPoolBase);
			}

			if (g_objectTable[slot].mobj != NULL) {
				if (g_objectTable[slot].mobj->pCraft != NULL) {
					g_objectTable[slot].mobj->pCraft =
						(CraftData*)((uint8_t*)g_objectTable[slot].mobj->pCraft -
									 (uint32_t)g_craftDataPoolBase);
					g_objectTable[slot].mobj->pCraft =
						(CraftData*)((uint8_t*)g_objectTable[slot].mobj->pCraft + 1u);
				}
				if (g_objectTable[slot].mobj->pWarheadGuidance != NULL) {
					g_objectTable[slot].mobj->pWarheadGuidance =
						(WarheadGuidanceState*)((uint8_t*)g_objectTable[slot].mobj->pWarheadGuidance -
												(uint32_t)g_warheadGuidancePoolBase);
					g_objectTable[slot].mobj->pWarheadGuidance =
						(WarheadGuidanceState*)((uint8_t*)g_objectTable[slot].mobj->pWarheadGuidance + 1u);
				}
				if (g_objectTable[slot].mobj->pCharData != NULL) {
					g_objectTable[slot].mobj->pCharData =
						(MobileObjectCharData*)((uint8_t*)g_objectTable[slot].mobj->pCharData -
												(uint32_t)g_mobileObjectCharDataPool);
					g_objectTable[slot].mobj->pCharData =
						(MobileObjectCharData*)((uint8_t*)g_objectTable[slot].mobj->pCharData + 1u);
				}

				memcpy(cursor, g_objectTable[slot].mobj, sizeof(FlightSerializedMobileObject));
				cursor += sizeof(FlightSerializedMobileObject);

				if (g_objectTable[slot].mobj->pCraft != NULL) {
					g_objectTable[slot].mobj->pCraft =
						(CraftData*)((uint8_t*)g_objectTable[slot].mobj->pCraft - 1u);
					g_objectTable[slot].mobj->pCraft =
						(CraftData*)((uint8_t*)g_objectTable[slot].mobj->pCraft +
									 (uint32_t)g_craftDataPoolBase);
				}
				if (g_objectTable[slot].mobj->pWarheadGuidance != NULL) {
					g_objectTable[slot].mobj->pWarheadGuidance =
						(WarheadGuidanceState*)((uint8_t*)g_objectTable[slot].mobj->pWarheadGuidance - 1u);
					g_objectTable[slot].mobj->pWarheadGuidance =
						(WarheadGuidanceState*)((uint8_t*)g_objectTable[slot].mobj->pWarheadGuidance +
												(uint32_t)g_warheadGuidancePoolBase);
				}
				if (g_objectTable[slot].mobj->pCharData != NULL) {
					g_objectTable[slot].mobj->pCharData =
						(MobileObjectCharData*)((uint8_t*)g_objectTable[slot].mobj->pCharData - 1u);
					g_objectTable[slot].mobj->pCharData =
						(MobileObjectCharData*)((uint8_t*)g_objectTable[slot].mobj->pCharData +
												(uint32_t)g_mobileObjectCharDataPool);
				}

				if ((recordFlags & 4u) == 0 && g_objectTable[slot].mobj->pCraft != NULL) {
					if (g_objectTable[slot].mobj->pCraft->effectiveAiObjectLink != NULL) {
						g_objectTable[slot].mobj->pCraft->effectiveAiObjectLink =
							(ObjectRecord*)((uint8_t*)g_objectTable[slot]
												.mobj->pCraft->effectiveAiObjectLink -
											(uint32_t)g_objectTable);
						g_objectTable[slot].mobj->pCraft->effectiveAiObjectLink =
							(ObjectRecord*)((uint8_t*)g_objectTable[slot]
												.mobj->pCraft->effectiveAiObjectLink +
											1u);
					}

					memcpy(cursor, g_objectTable[slot].mobj->pCraft, sizeof(FlightSerializedCraftData));
					cursor += sizeof(FlightSerializedCraftData);

					if (g_objectTable[slot].mobj->pCraft->effectiveAiObjectLink != NULL) {
						g_objectTable[slot].mobj->pCraft->effectiveAiObjectLink =
							(ObjectRecord*)((uint8_t*)g_objectTable[slot]
												.mobj->pCraft->effectiveAiObjectLink -
											1u);
						g_objectTable[slot].mobj->pCraft->effectiveAiObjectLink =
							(ObjectRecord*)((uint8_t*)g_objectTable[slot]
												.mobj->pCraft->effectiveAiObjectLink +
											(uint32_t)g_objectTable);
					}
				}

				if (g_objectTable[slot].mobj->pWarheadGuidance != NULL) {
					memcpy(cursor, g_objectTable[slot].mobj->pWarheadGuidance,
						   sizeof(FlightSerializedWarheadGuidanceState));
					cursor += sizeof(FlightSerializedWarheadGuidanceState);
				}

				if (g_objectTable[slot].mobj->pCharData != NULL) {
					memcpy(cursor, g_objectTable[slot].mobj->pCharData,
						   sizeof(FlightSerializedMobileObjectCharData));
					cursor += sizeof(FlightSerializedMobileObjectCharData);
				}
			}
		}
	}

	memcpy(cursor, g_missionFgStats, 370u * (uint32_t)(int16_t)g_missionHeader.numFlightGroups);
	tailCursor = cursor + 370u * (uint32_t)(int16_t)g_missionHeader.numFlightGroups;

	memcpy(tailCursor, &g_missionFormatVersion, sizeof(g_flightSerializedMissionStateBlock));
	tailCursor += sizeof(g_flightSerializedMissionStateBlock);

	if (g_provingGroundsModeActive != 0) {
		memcpy(tailCursor, &g_yardContext, sizeof(g_yardContext));
		tailCursor += sizeof(g_yardContext);
	}

	memcpy(tailCursor, g_flightGlobalCountdownTimers, sizeof(g_flightGlobalCountdownTimers));
	tailCursor += sizeof(g_flightGlobalCountdownTimers);

	memcpy(tailCursor, &g_flightPlayerCount, sizeof(g_flightPlayerCount));
	tailCursor += sizeof(g_flightPlayerCount);
	memcpy(tailCursor, &g_craftObjectSlotsTotal, sizeof(g_craftObjectSlotsTotal));
	tailCursor += sizeof(g_craftObjectSlotsTotal);
	memcpy(tailCursor, &g_mobileObjectCharDataCount, sizeof(g_mobileObjectCharDataCount));
	tailCursor += sizeof(g_mobileObjectCharDataCount);
	memcpy(tailCursor, &g_projectileObjectSlotsTotal, sizeof(g_projectileObjectSlotsTotal));
	tailCursor += sizeof(g_projectileObjectSlotsTotal);
	memcpy(tailCursor, &g_debrisObjectSlotsTotal, sizeof(g_debrisObjectSlotsTotal));
	tailCursor += sizeof(g_debrisObjectSlotsTotal);
	memcpy(tailCursor, &g_explosionObjectSlotsTotal, sizeof(g_explosionObjectSlotsTotal));
	tailCursor += sizeof(g_explosionObjectSlotsTotal);
	memcpy(tailCursor, &g_localDebrisObjectSlotsTotal, sizeof(g_localDebrisObjectSlotsTotal));
	tailCursor += sizeof(g_localDebrisObjectSlotsTotal);
	memcpy(tailCursor, &g_regionMainObjectSlotsTotal, sizeof(g_regionMainObjectSlotsTotal));
	tailCursor += sizeof(g_regionMainObjectSlotsTotal);
	memcpy(tailCursor, &g_regionStaticObjectSlotsTotal, sizeof(g_regionStaticObjectSlotsTotal));
	tailCursor += sizeof(g_regionStaticObjectSlotsTotal);

	memcpy(tailCursor, g_planTable, sizeof(g_planTable));
	tailCursor += sizeof(g_planTable);
	memcpy(tailCursor, &g_planCount, sizeof(g_planCount));
	tailCursor += sizeof(g_planCount);
	memcpy(tailCursor, &g_unusedWorldStateSerializedDword, sizeof(g_unusedWorldStateSerializedDword));
	tailCursor += sizeof(g_unusedWorldStateSerializedDword);
	memcpy(tailCursor, g_builtinPlanIdByNameIndex, PAI_BUILTIN_PLAN_ID_CACHE_COUNT);
	tailCursor += PAI_BUILTIN_PLAN_ID_CACHE_COUNT;

	randSeed = GameRand_GetPrimarySeed();
	*(uint16_t*)tailCursor = randSeed;
	tailCursor += sizeof(randSeed);
	memcpy(tailCursor, g_players, 3023u * (uint32_t)g_flightPlayerCount);

	{
		int* serializedSize;

		serializedSize = &g_worldStateSize;
		*serializedSize = 3023 * g_flightPlayerCount - (int)g_worldStateBuffer;
		*serializedSize += (int)tailCursor;
		return *serializedSize;
	}
#else
	uint8_t* cursor;
	uint32_t slot;
	uint32_t fgStatsSize;
	uint16_t randSeed;

	cursor = g_worldStateBuffer;
	for (slot = 0; slot < g_regionObjectSlotEnd; ++slot) {
		const ObjectRecord* obj;
		uint16_t objectType;

		obj = &g_objectTable[slot];
		objectType = (uint16_t)obj->objectType;
		FlightWorldState_WriteBlock(&cursor, &objectType, sizeof(objectType));

		if (objectType != OBJ_None) {
			FlightSerializedObjectRecord serializedObj;
			uint8_t recordFlags;
			const MobileObject* mobj;

			FlightWorldState_SerializeObjectRecord(&serializedObj, obj);
			FlightWorldState_WriteBlock(&cursor, &serializedObj, sizeof(serializedObj));

			recordFlags = g_modelTypeTable[objectType].recordFlags;
			mobj = obj->mobj;
			if (mobj != NULL) {
				FlightSerializedMobileObject serializedMobj;

				FlightWorldState_SerializeMobileObject(&serializedMobj, mobj);
				FlightWorldState_WriteBlock(&cursor, &serializedMobj, sizeof(serializedMobj));

				if ((recordFlags & 4u) == 0 && mobj->pCraft != NULL) {
					FlightSerializedCraftData serializedCraft;

					FlightWorldState_SerializeCraftData(&serializedCraft, mobj->pCraft);
					FlightWorldState_WriteBlock(&cursor, &serializedCraft, sizeof(serializedCraft));
				}

				if (mobj->pWarheadGuidance != NULL) {
					FlightSerializedWarheadGuidanceState serializedGuidance;

					FlightWorldState_SerializeWarheadGuidance(&serializedGuidance, mobj->pWarheadGuidance);
					FlightWorldState_WriteBlock(&cursor, &serializedGuidance, sizeof(serializedGuidance));
				}

				if (mobj->pCharData != NULL) {
					FlightWorldState_WriteBlock(&cursor, mobj->pCharData,
												sizeof(FlightSerializedMobileObjectCharData));
				}
			}
		}
	}

	fgStatsSize = 370u * (uint32_t)(int16_t)g_missionHeader.numFlightGroups;
	FlightWorldState_WriteBlock(&cursor, g_missionFgStats, fgStatsSize);

	FlightWorldState_BuildMissionStateBlock(&g_flightSerializedMissionStateBlock);
	FlightWorldState_WriteBlock(&cursor, &g_flightSerializedMissionStateBlock,
								sizeof(g_flightSerializedMissionStateBlock));

	if (g_provingGroundsModeActive != 0) {
		memset(g_flightSerializedYardContextBlock, 0, sizeof(g_flightSerializedYardContextBlock));
		memcpy(g_flightSerializedYardContextBlock, &g_yardContext, sizeof(g_yardContext));
		FlightWorldState_WriteBlock(&cursor, g_flightSerializedYardContextBlock,
									sizeof(g_flightSerializedYardContextBlock));
	}

	FlightWorldState_WriteBlock(&cursor, g_flightGlobalCountdownTimers,
								sizeof(g_flightGlobalCountdownTimers));

	FlightWorldState_WriteBlock(&cursor, &g_flightPlayerCount, sizeof(g_flightPlayerCount));
	FlightWorldState_WriteBlock(&cursor, &g_craftObjectSlotsTotal, sizeof(g_craftObjectSlotsTotal));
	FlightWorldState_WriteBlock(&cursor, &g_mobileObjectCharDataCount, sizeof(g_mobileObjectCharDataCount));
	FlightWorldState_WriteBlock(&cursor, &g_projectileObjectSlotsTotal, sizeof(g_projectileObjectSlotsTotal));
	FlightWorldState_WriteBlock(&cursor, &g_debrisObjectSlotsTotal, sizeof(g_debrisObjectSlotsTotal));
	FlightWorldState_WriteBlock(&cursor, &g_explosionObjectSlotsTotal, sizeof(g_explosionObjectSlotsTotal));
	FlightWorldState_WriteBlock(&cursor, &g_localDebrisObjectSlotsTotal,
								sizeof(g_localDebrisObjectSlotsTotal));
	FlightWorldState_WriteBlock(&cursor, &g_regionMainObjectSlotsTotal, sizeof(g_regionMainObjectSlotsTotal));
	FlightWorldState_WriteBlock(&cursor, &g_regionStaticObjectSlotsTotal,
								sizeof(g_regionStaticObjectSlotsTotal));

	FlightWorldState_WriteBlock(&cursor, g_planTable, sizeof(g_planTable));
	FlightWorldState_WriteBlock(&cursor, &g_planCount, sizeof(g_planCount));
	FlightWorldState_WriteBlock(&cursor, &g_unusedWorldStateSerializedDword,
								sizeof(g_unusedWorldStateSerializedDword));
	FlightWorldState_WriteBlock(&cursor, g_builtinPlanIdByNameIndex, PAI_BUILTIN_PLAN_ID_CACHE_COUNT);

	randSeed = GameRand_GetPrimarySeed();
	FlightWorldState_WriteBlock(&cursor, &randSeed, sizeof(randSeed));
	FlightWorldState_WriteBlock(&cursor, g_players, 3023u * (uint32_t)g_flightPlayerCount);

	g_worldStateSize = (int)(cursor - g_worldStateBuffer);
	worldStateSize = g_worldStateSize;
	return g_worldStateSize;
#endif
}

// FUNCTION: XWA 0x4F5C10
int Flight_ChecksumWorldState(int expectedChecksum, int serverTicks) {
	uint8_t* cursor;
	uint8_t* segmentStart;
	int segmentThreshold;
	int segmentIndex;
	uint32_t sectionChecksum;
	uint32_t totalChecksum;
	uint32_t calculatedChecksum;
	uint32_t slot;
	uint32_t byteCount;
	uint16_t randSeed;
	int finalSegmentSize;

	segmentThreshold = g_worldStateSize >> 4;
	cursor = g_worldStateBuffer;
	totalChecksum = 0;
	segmentStart = g_worldStateBuffer;
	segmentIndex = 0;
	sectionChecksum = 0;

	DebugPrintfChannel(0x20000, "ChecksumServerWorld at serverticks %ld.\n", (long)g_serverTickTime);

	for (slot = 0; slot < g_regionObjectSlotEnd; ++slot) {
		uint32_t objectType;

#ifndef XWA_MODERN
		objectType = *(uint16_t*)cursor;
#else
		{
			uint16_t serializedObjectType;

			memcpy(&serializedObjectType, cursor, sizeof(serializedObjectType));
			objectType = serializedObjectType;
		}
#endif
		cursor += sizeof(uint16_t);
		if (objectType != OBJ_None) {
			uint8_t* serializedObj;
			uint8_t* serializedMobj;
			uint32_t encodedMobj;
			uint16_t recordFlags;

			recordFlags = g_modelTypeTable[(uint16_t)g_objectTable[slot].objectType].recordFlags;
			serializedObj = cursor;
			byteCount = 35;
			do {
				sectionChecksum += *cursor++;
			} while (--byteCount != 0);
#ifndef XWA_MODERN
			encodedMobj = *(uint32_t*)&serializedObj[35];
#else
			encodedMobj = FlightWorldState_ReadU32(&serializedObj[35]);
#endif
			cursor += 4;

			if (encodedMobj != 0) {
				uint32_t encodedGuidance;
				uint32_t encodedCraft;
				uint32_t encodedCharData;

				serializedMobj = cursor;
				byteCount = 191;
				do {
					sectionChecksum += *cursor++;
				} while (--byteCount != 0);
				cursor += 38;

				if ((recordFlags & 4u) == 0) {
#ifndef XWA_MODERN
					encodedCraft = *(uint32_t*)&serializedMobj[221];
#else
					encodedCraft = FlightWorldState_ReadU32(&serializedMobj[221]);
#endif
					if (encodedCraft != 0) {
						byteCount = 985;
						do {
							sectionChecksum += *cursor++;
						} while (--byteCount != 0);
						cursor += 32;
					}
				}
#ifndef XWA_MODERN
				encodedGuidance = *(uint32_t*)&serializedMobj[217];
#else
				encodedGuidance = FlightWorldState_ReadU32(&serializedMobj[217]);
#endif
				if (encodedGuidance != 0) {
					byteCount = 10;
					do {
						sectionChecksum += *cursor++;
					} while (--byteCount != 0);
				}
#ifndef XWA_MODERN
				encodedCharData = *(uint32_t*)&serializedMobj[225];
#else
				encodedCharData = FlightWorldState_ReadU32(&serializedMobj[225]);
#endif
				if (encodedCharData != 0) {
					byteCount = 114;
					do {
						sectionChecksum += *cursor++;
					} while (--byteCount != 0);
				}
			}

			DebugPrintfChannel(0x20000, "Checksum %d summed object %d to %lx.\n", segmentIndex, (int)slot,
							   (unsigned long)sectionChecksum);
		}

		if ((int)(cursor - segmentStart) > segmentThreshold) {
			peerChecksum[segmentIndex] = (uint32_t)(cursor - segmentStart);
			g_worldChecksum[segmentIndex++] = sectionChecksum;
			totalChecksum += sectionChecksum;
			segmentStart = cursor;
			sectionChecksum = 0;
		}
	}

	byteCount = 370u * (uint32_t)(int16_t)g_missionHeader.numFlightGroups;
	if ((int)byteCount > 0) {
		do {
			sectionChecksum += *cursor++;
		} while (--byteCount != 0);
	}
	if ((int)(cursor - segmentStart) > segmentThreshold) {
		peerChecksum[segmentIndex] = (uint32_t)(cursor - segmentStart);
		DebugPrintfChannel(0x20000, "Checksum A %d summed to %lx.\n", segmentIndex,
						   (unsigned long)sectionChecksum);
		g_worldChecksum[segmentIndex++] = sectionChecksum;
		totalChecksum += sectionChecksum;
		segmentStart = cursor;
		sectionChecksum = 0;
	}

	byteCount = 0x2D32u;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	if ((int)(cursor - segmentStart) > segmentThreshold) {
		peerChecksum[segmentIndex] = (uint32_t)(cursor - segmentStart);
		DebugPrintfChannel(0x20000, "Checksum C %d summed to %lx.\n", segmentIndex,
						   (unsigned long)sectionChecksum);
		g_worldChecksum[segmentIndex++] = sectionChecksum;
		totalChecksum += sectionChecksum;
		segmentStart = cursor;
		sectionChecksum = 0;
	}

	if (g_provingGroundsModeActive != 0) {
		byteCount = sizeof(g_yardContext);
		do {
			sectionChecksum += *cursor++;
		} while (--byteCount != 0);
		if ((int)(cursor - segmentStart) > segmentThreshold) {
			peerChecksum[segmentIndex] = (uint32_t)(cursor - segmentStart);
			DebugPrintfChannel(0x20000, "Checksum Y %d summed to %lx.\n", segmentIndex,
							   (unsigned long)sectionChecksum);
			g_worldChecksum[segmentIndex++] = sectionChecksum;
			totalChecksum += sectionChecksum;
			segmentStart = cursor;
			sectionChecksum = 0;
		}
	}

	byteCount = 24;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	if ((int)(cursor - segmentStart) > segmentThreshold) {
		peerChecksum[segmentIndex] = (uint32_t)(cursor - segmentStart);
		DebugPrintfChannel(0x20000, "Checksum D %d summed to %lx.\n", segmentIndex,
						   (unsigned long)sectionChecksum);
		g_worldChecksum[segmentIndex++] = sectionChecksum;
		totalChecksum += sectionChecksum;
		segmentStart = cursor;
		sectionChecksum = 0;
	} else {
		DebugPrintfChannel(0x20000, "Checksum D %d didn't sum to %lx.\n", segmentIndex,
						   (unsigned long)sectionChecksum);
	}

	byteCount = 4;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	byteCount = 4;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	byteCount = 4;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	byteCount = 4;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	byteCount = 4;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	byteCount = 4;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	byteCount = 4;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	byteCount = 4;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	byteCount = 4;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	byteCount = 0x5500u;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	byteCount = 4;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	byteCount = 4;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	byteCount = 0x100u;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	if ((int)(cursor - segmentStart) > segmentThreshold) {
		peerChecksum[segmentIndex] = (uint32_t)(cursor - segmentStart);
		DebugPrintfChannel(0x20000, "Checksum E %d summed to %lx.\n", segmentIndex,
						   (unsigned long)sectionChecksum);
		g_worldChecksum[segmentIndex++] = sectionChecksum;
		totalChecksum += sectionChecksum;
		segmentStart = cursor;
		sectionChecksum = 0;
	}

	randSeed = GameRand_GetPrimarySeed();
	byteCount = 2;
	do {
		sectionChecksum += *cursor++;
	} while (--byteCount != 0);
	byteCount = 3023u * (uint32_t)g_flightPlayerCount;
	if ((int)byteCount > 0) {
		do {
			sectionChecksum += *cursor++;
		} while (--byteCount != 0);
	}

	DebugPrintfChannel(0x20000, "randomSeed set to %d.\n", randSeed);

	finalSegmentSize = (int)(cursor - segmentStart);
	if (finalSegmentSize > segmentThreshold) {
		peerChecksum[segmentIndex] = (uint32_t)finalSegmentSize;
		DebugPrintfChannel(0x20000, "Checksum F %d summed to %lx.\n", segmentIndex,
						   (unsigned long)sectionChecksum);
		g_worldChecksum[segmentIndex++] = sectionChecksum;
		calculatedChecksum = totalChecksum + sectionChecksum;
	} else {
		calculatedChecksum = totalChecksum;
	}

	if (segmentIndex < 16) {
		int clearIndex;
		uint32_t* peerChecksumClear;

		peerChecksumClear = &peerChecksum[segmentIndex];

		for (clearIndex = segmentIndex; clearIndex < 16; ++clearIndex) {
			g_worldChecksum[clearIndex] = 0;
		}
		for (clearIndex = 0; clearIndex < 16 - segmentIndex; ++clearIndex) {
			peerChecksumClear[clearIndex] = 0;
		}
	}

	DebugPrintfChannel(0x20000, "ChecksumServerWorld(%d,%lx) calculated %lx.\n", expectedChecksum,
					   (unsigned long)serverTicks, (unsigned long)calculatedChecksum);
	return (int)calculatedChecksum;
}

// FUNCTION: XWA 0x4F58A0
void Flight_RestoreWorldState(void) {
#ifndef XWA_MODERN
	uint8_t* cursor;
	uint32_t slot;
	uint32_t fgStatsSize;
	uint16_t randSeed;
	ObjectRecord* obj;

	cursor = g_worldStateBuffer;
	obj = g_objectTable;
	for (slot = 0; slot < g_regionObjectSlotEnd; ++slot, ++obj) {
		uint16_t objectType;

		obj->objectType = *(uint16_t*)cursor;
		cursor += sizeof(objectType);
		objectType = obj->objectType;

		if (objectType != OBJ_None) {
			uint16_t recordFlags;

			recordFlags = g_modelTypeTable[objectType].recordFlags;
			memcpy(obj, cursor, sizeof(FlightSerializedObjectRecord));
			cursor += sizeof(FlightSerializedObjectRecord);

			if (obj->mobj != NULL) {
				obj->mobj = (MobileObject*)((uint8_t*)obj->mobj - 1u);
				obj->mobj = (MobileObject*)((uint8_t*)obj->mobj + (uint32_t)g_mobileObjectPoolBase);
				if (obj->mobj != NULL) {
					memcpy(obj->mobj, cursor, sizeof(FlightSerializedMobileObject));
					cursor += sizeof(FlightSerializedMobileObject);

					if (obj->mobj->pCraft != NULL) {
						obj->mobj->pCraft = (CraftData*)((uint8_t*)obj->mobj->pCraft - 1u);
						obj->mobj->pCraft =
							(CraftData*)((uint8_t*)obj->mobj->pCraft + (uint32_t)g_craftDataPoolBase);
					}
					if (obj->mobj->pWarheadGuidance != NULL) {
						obj->mobj->pWarheadGuidance =
							(WarheadGuidanceState*)((uint8_t*)obj->mobj->pWarheadGuidance - 1u);
						obj->mobj->pWarheadGuidance =
							(WarheadGuidanceState*)((uint8_t*)obj->mobj->pWarheadGuidance +
													(uint32_t)g_warheadGuidancePoolBase);
					}
					if (obj->mobj->pCharData != NULL) {
						obj->mobj->pCharData = (MobileObjectCharData*)((uint8_t*)obj->mobj->pCharData - 1u);
						obj->mobj->pCharData = (MobileObjectCharData*)((uint8_t*)obj->mobj->pCharData +
																	   (uint32_t)g_mobileObjectCharDataPool);
					}

					if ((recordFlags & 4u) == 0 && obj->mobj->pCraft != NULL) {
						memcpy(obj->mobj->pCraft, cursor, sizeof(FlightSerializedCraftData));
						cursor += sizeof(FlightSerializedCraftData);
						if (obj->mobj->pCraft->effectiveAiObjectLink != NULL) {
							obj->mobj->pCraft->effectiveAiObjectLink =
								(ObjectRecord*)((uint8_t*)obj->mobj->pCraft->effectiveAiObjectLink - 1u);
							obj->mobj->pCraft->effectiveAiObjectLink =
								(ObjectRecord*)((uint8_t*)obj->mobj->pCraft->effectiveAiObjectLink +
												(uint32_t)g_objectTable);
						}
					}

					if (obj->mobj->pWarheadGuidance != NULL) {
						memcpy(obj->mobj->pWarheadGuidance, cursor,
							   sizeof(FlightSerializedWarheadGuidanceState));
						cursor += sizeof(FlightSerializedWarheadGuidanceState);
					}

					if (obj->mobj->pCharData != NULL) {
						memcpy(obj->mobj->pCharData, cursor, sizeof(FlightSerializedMobileObjectCharData));
						cursor += sizeof(FlightSerializedMobileObjectCharData);
					}
				}
			}
		} else if (obj->objectSignature != 0) {
			Object_ClearSlotState((uint16_t)slot);
			obj->objectSignature = 0;
		}
	}

	fgStatsSize = 370u * (uint32_t)(int16_t)g_missionHeader.numFlightGroups;
	memcpy(g_missionFgStats, cursor, fgStatsSize);
	cursor += fgStatsSize;

	memcpy(&g_missionFormatVersion, cursor, sizeof(g_flightSerializedMissionStateBlock));
	cursor += sizeof(g_flightSerializedMissionStateBlock);

	if (*(uint8_t*)((uint8_t*)&g_missionFormatVersion + 5) != 0) {
		memcpy(&g_yardContext, cursor, sizeof(g_flightSerializedYardContextBlock));
		cursor += sizeof(g_flightSerializedYardContextBlock);
	}

	memcpy(g_flightGlobalCountdownTimers, cursor, sizeof(g_flightGlobalCountdownTimers));
	cursor += sizeof(g_flightGlobalCountdownTimers);

	memcpy(&g_flightPlayerCount, cursor, sizeof(g_flightPlayerCount));
	cursor += sizeof(g_flightPlayerCount);
	memcpy(&g_craftObjectSlotsTotal, cursor, sizeof(g_craftObjectSlotsTotal));
	cursor += sizeof(g_craftObjectSlotsTotal);
	memcpy(&g_mobileObjectCharDataCount, cursor, sizeof(g_mobileObjectCharDataCount));
	cursor += sizeof(g_mobileObjectCharDataCount);
	memcpy(&g_projectileObjectSlotsTotal, cursor, sizeof(g_projectileObjectSlotsTotal));
	cursor += sizeof(g_projectileObjectSlotsTotal);
	memcpy(&g_debrisObjectSlotsTotal, cursor, sizeof(g_debrisObjectSlotsTotal));
	cursor += sizeof(g_debrisObjectSlotsTotal);
	memcpy(&g_explosionObjectSlotsTotal, cursor, sizeof(g_explosionObjectSlotsTotal));
	cursor += sizeof(g_explosionObjectSlotsTotal);
	memcpy(&g_localDebrisObjectSlotsTotal, cursor, sizeof(g_localDebrisObjectSlotsTotal));
	cursor += sizeof(g_localDebrisObjectSlotsTotal);
	memcpy(&g_regionMainObjectSlotsTotal, cursor, sizeof(g_regionMainObjectSlotsTotal));
	cursor += sizeof(g_regionMainObjectSlotsTotal);
	memcpy(&g_regionStaticObjectSlotsTotal, cursor, sizeof(g_regionStaticObjectSlotsTotal));
	cursor += sizeof(g_regionStaticObjectSlotsTotal);

	memcpy(g_planTable, cursor, sizeof(g_planTable));
	cursor += sizeof(g_planTable);
	memcpy(&g_planCount, cursor, sizeof(g_planCount));
	cursor += sizeof(g_planCount);
	memcpy(&g_unusedWorldStateSerializedDword, cursor, sizeof(g_unusedWorldStateSerializedDword));
	cursor += sizeof(g_unusedWorldStateSerializedDword);
	memcpy(g_builtinPlanIdByNameIndex, cursor, PAI_BUILTIN_PLAN_ID_CACHE_COUNT);
	cursor += PAI_BUILTIN_PLAN_ID_CACHE_COUNT;

	randSeed = *(uint16_t*)cursor;
	cursor += sizeof(randSeed);
	Math_SeedRandom(randSeed);
	memcpy(g_players, cursor, 3023u * (uint32_t)g_flightPlayerCount);
#else
	uint8_t* cursor;
	uint32_t slot;
	uint32_t fgStatsSize;
	uint16_t randSeed;

	cursor = g_worldStateBuffer;
	for (slot = 0; slot < g_regionObjectSlotEnd; ++slot) {
		ObjectRecord* obj;
		uint16_t objectType;

		obj = &g_objectTable[slot];
		memcpy(&objectType, cursor, sizeof(objectType));
		cursor += sizeof(objectType);
		obj->objectType = (ObjectTypeId)objectType;

		if (objectType != OBJ_None) {
			const FlightSerializedObjectRecord* serializedObj;
			uint8_t recordFlags;

			serializedObj = (const FlightSerializedObjectRecord*)cursor;
			cursor += sizeof(*serializedObj);
			recordFlags = g_modelTypeTable[objectType].recordFlags;
			FlightWorldState_RestoreObjectRecord(obj, serializedObj);

			if (obj->mobj != NULL) {
				const FlightSerializedMobileObject* serializedMobj;

				serializedMobj = (const FlightSerializedMobileObject*)cursor;
				cursor += sizeof(*serializedMobj);
				FlightWorldState_RestoreMobileObject(obj->mobj, serializedMobj);

				if ((recordFlags & 4u) == 0 && obj->mobj->pCraft != NULL) {
					const FlightSerializedCraftData* serializedCraft;

					serializedCraft = (const FlightSerializedCraftData*)cursor;
					cursor += sizeof(*serializedCraft);
					FlightWorldState_RestoreCraftData(obj->mobj->pCraft, serializedCraft);
				}

				if (obj->mobj->pWarheadGuidance != NULL) {
					const FlightSerializedWarheadGuidanceState* serializedGuidance;

					serializedGuidance = (const FlightSerializedWarheadGuidanceState*)cursor;
					cursor += sizeof(*serializedGuidance);
					FlightWorldState_RestoreWarheadGuidance(obj->mobj->pWarheadGuidance, serializedGuidance);
				}

				if (obj->mobj->pCharData != NULL) {
					memcpy(obj->mobj->pCharData, cursor, sizeof(FlightSerializedMobileObjectCharData));
					cursor += sizeof(FlightSerializedMobileObjectCharData);
				}
			}
		} else if (obj->objectSignature != 0) {
			Object_ClearSlotState((uint16_t)slot);
			obj->objectSignature = 0;
		}
	}

	fgStatsSize = 370u * (uint32_t)(int16_t)g_missionHeader.numFlightGroups;
	memcpy(g_missionFgStats, cursor, fgStatsSize);
	cursor += fgStatsSize;

	memcpy(&g_flightSerializedMissionStateBlock, cursor, sizeof(g_flightSerializedMissionStateBlock));
	cursor += sizeof(g_flightSerializedMissionStateBlock);
	FlightWorldState_ApplyMissionStateBlock(&g_flightSerializedMissionStateBlock);

	if (g_provingGroundsModeActive != 0) {
		memcpy(g_flightSerializedYardContextBlock, cursor, sizeof(g_flightSerializedYardContextBlock));
		memcpy(&g_yardContext, cursor, sizeof(g_yardContext));
		cursor += sizeof(g_yardContext);
	}

	memcpy(g_flightGlobalCountdownTimers, cursor, sizeof(g_flightGlobalCountdownTimers));
	cursor += sizeof(g_flightGlobalCountdownTimers);

	memcpy(&g_flightPlayerCount, cursor, sizeof(g_flightPlayerCount));
	cursor += sizeof(g_flightPlayerCount);
	memcpy(&g_craftObjectSlotsTotal, cursor, sizeof(g_craftObjectSlotsTotal));
	cursor += sizeof(g_craftObjectSlotsTotal);
	memcpy(&g_mobileObjectCharDataCount, cursor, sizeof(g_mobileObjectCharDataCount));
	cursor += sizeof(g_mobileObjectCharDataCount);
	memcpy(&g_projectileObjectSlotsTotal, cursor, sizeof(g_projectileObjectSlotsTotal));
	cursor += sizeof(g_projectileObjectSlotsTotal);
	memcpy(&g_debrisObjectSlotsTotal, cursor, sizeof(g_debrisObjectSlotsTotal));
	cursor += sizeof(g_debrisObjectSlotsTotal);
	memcpy(&g_explosionObjectSlotsTotal, cursor, sizeof(g_explosionObjectSlotsTotal));
	cursor += sizeof(g_explosionObjectSlotsTotal);
	memcpy(&g_localDebrisObjectSlotsTotal, cursor, sizeof(g_localDebrisObjectSlotsTotal));
	cursor += sizeof(g_localDebrisObjectSlotsTotal);
	memcpy(&g_regionMainObjectSlotsTotal, cursor, sizeof(g_regionMainObjectSlotsTotal));
	cursor += sizeof(g_regionMainObjectSlotsTotal);
	memcpy(&g_regionStaticObjectSlotsTotal, cursor, sizeof(g_regionStaticObjectSlotsTotal));
	cursor += sizeof(g_regionStaticObjectSlotsTotal);

	memcpy(g_planTable, cursor, sizeof(g_planTable));
	cursor += sizeof(g_planTable);
	memcpy(&g_planCount, cursor, sizeof(g_planCount));
	cursor += sizeof(g_planCount);
	memcpy(&g_unusedWorldStateSerializedDword, cursor, sizeof(g_unusedWorldStateSerializedDword));
	cursor += sizeof(g_unusedWorldStateSerializedDword);
	memcpy(g_builtinPlanIdByNameIndex, cursor, PAI_BUILTIN_PLAN_ID_CACHE_COUNT);
	cursor += PAI_BUILTIN_PLAN_ID_CACHE_COUNT;

	memcpy(&randSeed, cursor, sizeof(randSeed));
	cursor += sizeof(randSeed);
	Math_SeedRandom(randSeed);
	memcpy(g_players, cursor, 3023u * (uint32_t)g_flightPlayerCount);
#endif
}

// Craft steering embeds this arithmetic; other callers use the public wrapper.
static __inline void Flight_AccelerateHyperspaceSpeedInline(int objectIdx, int acceleration) {
	uint32_t product;
	uint32_t wholeQuotient;
	uint16_t wholeDelta;
	uint16_t fracDelta;
	MobileObject* mobj;
	uint16_t oldRemainder;

	product = (uint32_t)(uint16_t)g_elapsedTicks * (uint32_t)acceleration;
	wholeQuotient = product / 236u;
	wholeDelta = (uint16_t)wholeQuotient;
	fracDelta = (uint16_t)(((product - wholeQuotient * 236u) << 16) / 236u);

	mobj = g_objectTable[objectIdx].mobj;
	oldRemainder = mobj->speedRemainder;
	mobj->speedRemainder = (uint16_t)(oldRemainder + fracDelta);
	if (g_objectTable[objectIdx].mobj->speedRemainder < oldRemainder) {
		++g_objectTable[objectIdx].mobj->speed;
	}

	g_objectTable[objectIdx].mobj->speed += wholeDelta;
	if (g_objectTable[objectIdx].mobj->speed > 3600u) {
		g_objectTable[objectIdx].mobj->speed = 3600;
	}
}

// FUNCTION: XWA 0x42D220
void Flight_AccelerateHyperspaceSpeed(int objectIdx, int acceleration) {
	Flight_AccelerateHyperspaceSpeedInline(objectIdx, acceleration);
}

static __inline void Flight_DecelerateHyperspaceSpeedInline(int objectIdx, int deceleration) {
	uint32_t product;
	uint32_t wholeQuotient;
	uint16_t wholeDelta;
	uint16_t fracDelta;
	MobileObject* mobj;
	uint16_t oldRemainder;

	product = (uint32_t)(uint16_t)g_elapsedTicks * (uint32_t)deceleration;
	wholeQuotient = product / 236u;
	wholeDelta = (uint16_t)wholeQuotient;
	fracDelta = (uint16_t)(((product - wholeQuotient * 236u) << 16) / 236u);

	mobj = g_objectTable[objectIdx].mobj;
	oldRemainder = mobj->speedRemainder;
	mobj->speedRemainder = (uint16_t)(oldRemainder - fracDelta);
	if (g_objectTable[objectIdx].mobj->speedRemainder > oldRemainder) {
		--g_objectTable[objectIdx].mobj->speed;
	}

	g_objectTable[objectIdx].mobj->speed -= wholeDelta;
	if (g_objectTable[objectIdx].mobj->speed > 0x8000u) {
		g_objectTable[objectIdx].mobj->speed = 0;
	}
}

// FUNCTION: XWA 0x42D2E0
void Flight_DecelerateHyperspaceSpeed(int objectIdx, int deceleration) {
	Flight_DecelerateHyperspaceSpeedInline(objectIdx, deceleration);
}

// FUNCTION: XWA 0x4034D0
// Advances one player's hyperspace/region-transition state machine: outbound
// departure (craft removal / save / wave handling), inter-region transfer
// (copy/position the craft and any carried object into the target region),
// inbound arrival (camera/control/AI restoration), plus the associated sound,
// HUD, music, force-feedback, and directional-light side effects.
void FlightObject_UpdatePlayerHyperspaceTransition(unsigned int playerIdx) {
	int playerObjIdx = g_players[playerIdx].objectIndex;
	CraftData* craft = g_objectTable[playerObjIdx].mobj->pCraft;
	uint8_t objectKind = craft->objectKind;

	// A craft that is no longer a normal/escape-pod kind aborts hyperspace.
	if (objectKind != 0) {
		if (objectKind != 5) {
			g_players[playerIdx].hyperspacePhase = PLAYER_HYPERSPACE_PHASE_NONE;
			if (playerIdx == (unsigned int)g_localPlayer) {
				FlightLight_SetLocalPlayerPulseEnabled(4, 0);
				FlightLight_SetLocalPlayerPulseEnabled(5, 0);
				FlightLight_SetLocalPlayerPulseEnabled(3, 0);
			}
		}
	}
	g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks += (uint16_t)g_elapsedTicks;

	switch (g_players[playerIdx].hyperspacePhase) {
		case PLAYER_HYPERSPACE_OUTBOUND:
			if (g_hyperspaceFxPhaseLatch != 2) {
				g_hyperspaceFxPhaseLatch = 2;
				if (playerIdx == (unsigned int)g_localPlayer) {
					int li;
					if (g_players[playerIdx].iff == 1)
						fsfx_PlaySound(115, 0xFFFFu, g_localPlayer);
					else
						fsfx_PlaySound(118, 0xFFFFu, g_localPlayer);
					ForceFeedback_PlayHyperspaceOutboundEffect();
					Music_TriggerSequence(2190, g_players[playerIdx].regionIndex, 3);
					// Snapshot the live lights as fade base, then fade to black.
					for (li = 0; li < g_dirLightCount; ++li) {
						g_directionalLights[li].fade.baseIntensity = g_directionalLights[li].intensity;
						g_directionalLights[li].fade.baseField1C = g_directionalLights[li].field_1C;
						g_directionalLights[li].fade.baseColorR = g_directionalLights[li].colorR;
						g_directionalLights[li].fade.baseColorG = g_directionalLights[li].colorG;
						g_directionalLights[li].fade.baseColorB = g_directionalLights[li].colorB;
						g_directionalLights[li].fade.targetIntensity = 0.0f;
						g_directionalLights[li].fade.targetField1C = 0.0f;
						g_directionalLights[li].fade.targetColorR = 0.0f;
						g_directionalLights[li].fade.targetColorG = 0.0f;
						g_directionalLights[li].fade.targetColorB = 0.0f;
					}
				}
			}
			if (g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks < 0x24Eu) {
				Flight_AccelerateHyperspaceSpeed(g_players[playerIdx].objectIndex, 1500);
				if (playerIdx == (unsigned int)g_localPlayer) {
					float blend = g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks * 0.0016949152f;
					FlightLight_BlendDirectionalLightTargets(blend);
				}
				return;
			}
			if (playerIdx == (unsigned int)g_localPlayer) {
				FlightLight_BlendDirectionalLightTargets(1.0f);
				if (g_players[playerIdx].iff == 1 && !Sound_CountPlayingInstances(g_sfxIds[116]))
					fsfx_PlaySound(116, 0xFFFFu, g_localPlayer);
			}
			{
				int flightGroupIdx = g_objectTable[playerObjIdx].flightGroupIdx;
				if (g_players[playerIdx].hyperspaceRuntime.targetRegionOrMode == 4) {
					// targetRegionOrMode == 4: the craft departs the mission entirely.
					Mission_RecordCraftOutcome(playerObjIdx, flightGroupIdx, 0x11u);
					if (g_missionHeader.body.hangar != XWA_HANGAR_QUICKSTART &&
						g_playerFlightGroupWaveMode == 1 &&
						g_missionFlightGroups[flightGroupIdx].fg.numberOfWaves != 99 &&
						(uint16_t)GetModelIndexFromType(g_objectTable[playerObjIdx].objectType) != 0xFFFF) {
						g_players[playerIdx].missionStats.missionScore +=
							g_modelDefs[GetModelIndexFromType(g_objectTable[playerObjIdx].objectType)]
								.craftPointValue;
					}
					if (g_players[playerIdx].objectIndex != 0xFFFF) {
						fsfx_UpdateBeamSystemLoop(0, playerIdx);
						fsfx_UpdateIncomingMissileWarning(0);
						fsfx_StopHyperZoomImp(playerIdx);
					}
					g_objectTable[playerObjIdx].objectType = OBJ_None;
					Player_SaveCraftSettings(playerIdx);
					Craft_ClearEffectiveAiObjectLink(craft);
					g_players[playerIdx].hyperspacePhase = PLAYER_HYPERSPACE_PHASE_NONE;
					if (playerIdx == (unsigned int)g_localPlayer) {
						FlightLight_SetLocalPlayerPulseEnabled(4, 0);
						FlightLight_SetLocalPlayerPulseEnabled(5, 0);
						FlightLight_SetLocalPlayerPulseEnabled(3, 0);
					}
					Mission_ProcessFlightGroupWaveCompletion(flightGroupIdx);

					if (g_missionHeader.body.hangar != XWA_HANGAR_SKIRMISH ||
						Player_BindToAvailableCraft(playerIdx, 0xFFFFu, 0, 0) != 0) {
						// Non-skirmish, or skirmish with no craft left.
						Player_EndFlightParticipation(playerIdx);
						Player_EmitRemotePlayerDepartedMessages(playerIdx);
					} else {
						// Rebound to a fresh craft in skirmish.
						if (playerIdx == (unsigned int)g_localPlayer)
							msg_emitLocalPlayerCraftMessage(MSG_PREVIOUS_HYPERSPACED);
					}
					if (playerIdx == (unsigned int)g_localPlayer) {
						FlightLight_ClearDirectionalLights();
						FlightLight_AddCurrentRegionBackdropLights();
					}
				} else {
					// Transition into the inter-region transfer phase.
					if (playerIdx == (unsigned int)g_localPlayer) {
						FlightLight_SetLocalPlayerPulseEnabled(4, 1);
						FlightLight_SetLocalPlayerPulseEnabled(5, 0);
						FlightLight_SetLocalPlayerPulseEnabled(3, 0);
					}
					g_players[playerIdx].hyperspacePhase = PLAYER_HYPERSPACE_REGION_TRANSFER;
					g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks = 0;
				}

				// Record the departed/transferring craft's region tail events.
				++g_missionFgStats[flightGroupIdx]
					  .tailEventCounts[g_objectTable[playerObjIdx].regionIdx + 10];
				if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber)
					++g_missionFgStats[flightGroupIdx]
						  .tailEventCounts[g_objectTable[playerObjIdx].regionIdx + 15];
				if (craft->carriedObjectIndex != 0xFFFF) {
					CraftData* carriedCraft;
					int carriedFg = g_objectTable[craft->carriedObjectIndex].flightGroupIdx;
					++g_missionFgStats[carriedFg].tailEventCounts[g_objectTable[playerObjIdx].regionIdx + 10];
					carriedCraft = g_objectTable[craft->carriedObjectIndex].mobj->pCraft;
					if (carriedCraft) {
						int cfg;
						carriedCraft->objectKind = 0;
						cfg = g_objectTable[craft->carriedObjectIndex].flightGroupIdx;
						if (g_missionFlightGroups[cfg].fg.specialCargoCraft == carriedCraft->waveNumber)
							++g_missionFgStats[cfg]
								  .tailEventCounts[g_objectTable[playerObjIdx].regionIdx + 10];
					}
				}
			}
			return;

		case PLAYER_HYPERSPACE_INBOUND:
			if (g_hyperspaceFxPhaseLatch != 3) {
				g_hyperspaceFxPhaseLatch = 3;
				if (playerIdx == (unsigned int)g_localPlayer) {
					int li;
					fsfx_PlaySound(117, 0xFFFFu, g_localPlayer);
					ForceFeedback_PlayHyperspaceInboundEffect();
					FlightLight_ClearDirectionalLights();
					FlightLight_AddCurrentRegionBackdropLights();
					// Fade in from black to the new region's directional lights.
					for (li = 0; li < g_dirLightCount; ++li) {
						g_directionalLights[li].fade.targetIntensity = g_directionalLights[li].intensity;
						g_directionalLights[li].fade.targetField1C = g_directionalLights[li].field_1C;
						g_directionalLights[li].fade.targetColorR = g_directionalLights[li].colorR;
						g_directionalLights[li].fade.targetColorG = g_directionalLights[li].colorG;
						g_directionalLights[li].fade.targetColorB = g_directionalLights[li].colorB;
						g_directionalLights[li].fade.baseIntensity = 0.0f;
						g_directionalLights[li].fade.baseField1C = 0.0f;
						g_directionalLights[li].fade.baseColorR = 0.0f;
						g_directionalLights[li].fade.baseColorG = 0.0f;
						g_directionalLights[li].fade.baseColorB = 0.0f;
					}
					FlightLight_BlendDirectionalLightTargets(0.0f);
				}
			}
			if (g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks > 0xECu) {
				int objIdx;
				// Arrival complete: restore control and AI.
				g_players[playerIdx].hyperspacePhase = PLAYER_HYPERSPACE_PHASE_NONE;
				craft->objectKind = 0;
				objIdx = g_players[playerIdx].objectIndex;
				g_objectTable[objIdx].mobj->moveVectorDirty = 1;
				g_objectTable[objIdx].mobj->orientMatrixDirty = 1;
				if (playerIdx == (unsigned int)g_localPlayer) {
					FlightLight_SetLocalPlayerPulseEnabled(4, 0);
					FlightLight_SetLocalPlayerPulseEnabled(5, 0);
					FlightLight_SetLocalPlayerPulseEnabled(3, 0);
				}
				Flight_RecomputeCraftSpeedFromPowerSettings(objIdx);
				g_players[playerIdx].smoothedInputYaw = 0;
				g_players[playerIdx].smoothedInputPitch = 0;
				g_players[playerIdx].hyperspaceRuntime.hyperBuoyPromptCooldown = 25;
				if (playerIdx == (unsigned int)g_localPlayer) {
					FlightLight_BlendDirectionalLightTargets(1.0f);
					Hud_MarkFilmOverlayElementsVisible();
				}
				if (g_players[playerIdx].aiControlledFlag) {
					int slot;
					int aiRegion;
					uint8_t order;
					pai_setupcraftcontext(objIdx);
					g_curCraft = craft;
					aiRegion = g_paiContext.curOrderCoord.fields.regionIdx;
					slot = 0;
					while (slot < 4 &&
						   g_paiContext.aiController->orderScratch.completionState[aiRegion][slot])
						++slot;
					if (slot == 4) {
						int completedSlot = 0;
						while (completedSlot < 4 && g_paiContext.aiController->orderScratch
															.completionState[aiRegion][completedSlot] != 1)
							++completedSlot;
						slot = 0;
					}
					order = g_missionFlightGroups[g_paiContext.curOrderCoord.fields.flightGroupIdx]
								.fg.orders[slot + 4 * aiRegion]
								.order;
					g_paiContext.aiController->currentPlanId =
						g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
					g_paiContext.aiController->pendingPlanId = g_paiContext.aiController->currentPlanId;
					g_paiContext.aiController->currentOrderSlot = (char)slot;
					pai_ApplyPendingPlanTargetAndManeuver(objIdx);
				}
#ifdef XWA_MODERN
				DebugPrintf("Exit hypervector: %x headingxy, %x headingz.\n", g_objectTable[objIdx].yaw,
							g_objectTable[objIdx].pitch);
#else
				{
					char debugMessage[256];
					sprintf(debugMessage, "Exit hypervector: %x headingxy, %x headingz.\n",
							g_objectTable[objIdx].yaw, g_objectTable[objIdx].pitch);
					g_OutputDebugStringA(debugMessage);
				}
#endif
			} else {
				Flight_DecelerateHyperspaceSpeed(g_players[playerIdx].objectIndex, 1500);
				if (playerIdx == (unsigned int)g_localPlayer) {
					float blend = g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks * 0.0042372881f;
					FlightLight_BlendDirectionalLightTargets(blend);
				}
			}
			break;

		case PLAYER_HYPERSPACE_REGION_TRANSFER:
			if (g_hyperspaceFxPhaseLatch != 4) {
				g_hyperspaceFxPhaseLatch = 4;
				if (playerIdx == (unsigned int)g_localPlayer && g_players[g_localPlayer].iff == 1)
					Sound_StopOldestInstance(g_sfxIds[115]);
			}
			if (g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks > 0x512u) {
				unsigned int newSlot;
				unsigned int oldObjIdx = g_players[playerIdx].objectIndex;
				int targetRegion = g_players[playerIdx].hyperspaceRuntime.targetRegionOrMode;
				int fromRegion = g_objectTable[oldObjIdx].regionIdx;
				Mission_SetActiveRegionObjectRanges(targetRegion);

				newSlot = g_activeRegionObjectSlotStart;
				while (newSlot < g_activeRegionCraftObjectSlotEnd &&
					   g_objectTable[newSlot].objectType != OBJ_None)
					++newSlot;

				if (newSlot < g_activeRegionCraftObjectSlotEnd) {
					int x, y, z;
					int fgIdx2;
					int movementYaw;
					int movementPitch;
					Q16Angle objectYaw, objectPitch;
					ObjectRecord* po;
					ObjectTypeId arrivedType;
					Object_CopyStatePreservingStorage(newSlot, oldObjIdx);
					g_objectTable[newSlot].regionIdx = targetRegion;
					if (!g_players[playerIdx].hyperspaceRuntime.regionTransferArrivalCounted[targetRegion]) {
						int fgIdx = g_objectTable[oldObjIdx].flightGroupIdx;
						g_players[playerIdx].hyperspaceRuntime.regionTransferArrivalCounted[targetRegion] = 1;
						++g_missionFgStats[fgIdx].tailEventCounts[targetRegion];
					}

					// Resolve the arrival point coming from fromRegion.
					if (g_missionRegionHyperPoints.arrivalPointValid[targetRegion][fromRegion]) {
						x = g_missionRegionHyperPoints.arrivalPoint[targetRegion][fromRegion].x;
						y = g_missionRegionHyperPoints.arrivalPoint[targetRegion][fromRegion].y;
						z = g_missionRegionHyperPoints.arrivalPoint[targetRegion][fromRegion].z;
					} else {
						x = 0;
						y = 0;
						z = 0;
					}

					fgIdx2 = g_objectTable[oldObjIdx].flightGroupIdx;
					if (g_missionFlightGroups[fgIdx2].fg.orders[4 * targetRegion].waypoints[0].enabled) {
						Mission_ResolveObjectOrMissionPointWorldLoc(0x8004u, fgIdx2, targetRegion, 0);
						trig2_ctop(worldlocx - x, worldlocy - y, worldlocz - z);
						movementYaw = trig2_xyangle;
						movementPitch = targetPitch;
						objectYaw = movementYaw;
						objectPitch = movementPitch;
					} else {
						movementYaw = 0;
						movementPitch = 0x4000;
						objectYaw = 0;
						objectPitch = 0x4000;
					}
					if (!objectYaw) {
						movementYaw = 0;
						objectYaw = 0;
					}
					// Offset the craft behind the arrival point along the reversed heading.
					movementYaw += 0x8000;
					movementPitch = 0x8000 - movementPitch;
					trig2_xyangle = movementYaw;
					targetPitch = movementPitch;
					trig2_movexyz(0xFFFF, movementYaw, movementPitch);
					trig2_xmovedist += trig2_xmovedist >> 2;
					trig2_ymovedist += trig2_ymovedist >> 2;
					trig2_zmovedist += trig2_zmovedist >> 2;
					g_objectTable[newSlot].world_x = trig2_xmovedist + x;
					g_objectTable[newSlot].world_y = trig2_ymovedist + y;
					g_objectTable[newSlot].world_z = trig2_zmovedist + z;
					g_objectTable[newSlot].mobj->prevWorldX = g_objectTable[newSlot].world_x;
					g_objectTable[newSlot].mobj->prevWorldY = g_objectTable[newSlot].world_y;
					g_objectTable[newSlot].mobj->prevWorldZ = g_objectTable[newSlot].world_z;
					g_objectTable[newSlot].mobj->speed = 3600;
					g_objectTable[newSlot].yaw = objectYaw;
					g_objectTable[newSlot].pitch = objectPitch;
					g_objectTable[newSlot].roll = 0;
					g_objectTable[newSlot].angleD = 0;
					FVIEW_calcrotatemove(g_objectTable[newSlot].pitch, g_objectTable[newSlot].yaw,
										 &g_objectTable[newSlot]);
					FVIEW_calcrotateorient(g_objectTable[newSlot].roll, g_objectTable[newSlot].angleD,
										   &g_objectTable[newSlot]);
					g_objectTable[oldObjIdx].objectType = OBJ_None;

					g_players[playerIdx].objectIndex = newSlot;
					g_players[playerIdx].regionIndex = targetRegion;
					g_players[playerIdx].viewState.cameraFocusObjIdx = newSlot;
					g_players[playerIdx].targetCycleStart = g_players[playerIdx].currentTargetObjectIdx;
					g_players[playerIdx].currentTargetObjectIdx = 0xffffu;
					if ((unsigned int)g_localPlayer == playerIdx) {
						g_players[g_localPlayer].hyperspacePhase = PLAYER_HYPERSPACE_PHASE_NONE;
						FlightLight_SetLocalPlayerPulseEnabled(4, 0);
						FlightLight_SetLocalPlayerPulseEnabled(5, 1);
						g_localPlayerLightPulses[5].startTime = g_gameTime - 236;
						FlightLight_SetLocalPlayerPulseEnabled(3, 0);
						FlightView_UpdatePlayerCamera(g_localPlayer);
						Flight_InitInboundHyperspaceStreaks();
					}

					// Move any carried object into the target region alongside it.
					if (g_curCraft->carriedObjectIndex != 0xFFFF) {
						unsigned int carrySlot = g_activeRegionObjectSlotStart;
						while (carrySlot < g_activeRegionCraftObjectSlotEnd &&
							   g_objectTable[carrySlot].objectType != OBJ_None)
							++carrySlot;
						if (carrySlot < g_activeRegionCraftObjectSlotEnd) {
							MobileObject* cmobj;
							unsigned int oldCarried;
							Object_CopyStatePreservingStorage(carrySlot, g_curCraft->carriedObjectIndex);
							g_objectTable[carrySlot].regionIdx = targetRegion;
							++g_missionFgStats[g_objectTable[carrySlot].flightGroupIdx]
								  .tailEventCounts[targetRegion];
							cmobj = g_objectTable[carrySlot].mobj;
							if (cmobj) {
								CraftData* ccraft = cmobj->pCraft;
								if (ccraft) {
									int cfg;
									ccraft->objectKind = 0;
									cfg = g_objectTable[carrySlot].flightGroupIdx;
									if (g_missionFlightGroups[cfg].fg.specialCargoCraft == ccraft->waveNumber)
										++g_missionFgStats[cfg].tailEventCounts[targetRegion + 5];
								}
							}
							oldCarried = g_curCraft->carriedObjectIndex;
							g_objectTable[newSlot].mobj->pCraft->carriedObjectIndex = carrySlot;
							g_objectTable[oldCarried].objectType = OBJ_None;
						}
					}

					g_players[playerIdx].hyperspacePhase = PLAYER_HYPERSPACE_INBOUND;
					g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks = 0;

					// X-/B-wing s-foils reopen on arrival.
					po = &g_objectTable[g_players[playerIdx].objectIndex];
					arrivedType = (ObjectTypeId)po->objectType;
					if (arrivedType == OBJ_XWing || arrivedType == OBJ_BWing) {
						po->mobj->pCraft->sFoilState ^= 2u;
						g_objectTable[g_players[playerIdx].objectIndex].mobj->pCraft->sFoilState |= 1u;
						if (playerIdx == (unsigned int)g_localPlayer) {
							fsfx_PlaySound(120, 0xFFFFu, playerIdx);
							msg_emitInFlightMessage(MSG_SFOILS_OPENING, playerIdx);
							Mission_SetActiveRegionObjectRanges(fromRegion);
							return;
						}
					}
				} else {
					// No free slot in the target region: retry shortly.
					g_players[playerIdx].hyperspaceRuntime.phaseElapsedTicks = 236;
				}
				Mission_SetActiveRegionObjectRanges(fromRegion);
			}
			break;
	}
}

// FUNCTION: XWA 0x42CF90
void Flight_SlewObjectSpeedTowardTarget(int objectIdx, int targetSpeed, int allowDecel, int fracQ16) {
	uint32_t speedDelta;

	speedDelta = targetSpeed - g_objectTable[objectIdx].mobj->speed;
	if (speedDelta == 0) {
		return;
	}

	if (speedDelta < 0x8000u) {
		uint32_t step;
		MobileObject* mobj;
		uint32_t scaledDelta;
		uint32_t wholeDelta;
		uint16_t fracDelta;
		uint16_t oldRemainder;

		step =
			(uint16_t)MATH2_fraction(g_modelDefs[(uint16_t)g_curCraftModelIndex.packed].accelRate, 0x4000u);
		if (step == 0) {
			step = 1;
		}
		step += (uint16_t)MATH2_fraction(
			(uint16_t)(g_modelDefs[(uint16_t)g_curCraftModelIndex.packed].accelRate - step),
			(uint16_t)fracQ16);
		mobj = g_objectTable[objectIdx].mobj;
		if (mobj->pCraft->slamActive != 0) {
			step *= 3;
		}
		if (speedDelta >= step) {
			speedDelta = step;
		}

		scaledDelta = (uint32_t)(uint16_t)g_elapsedTicks * speedDelta;
		wholeDelta = scaledDelta / 236u;
		fracDelta = (uint16_t)(((scaledDelta - wholeDelta * 236u) << 16) / 236u);

		oldRemainder = mobj->speedRemainder;
		mobj->speedRemainder = (uint16_t)(oldRemainder + fracDelta);
		if (g_objectTable[objectIdx].mobj->speedRemainder < oldRemainder) {
			++g_objectTable[objectIdx].mobj->speed;
		}

		g_objectTable[objectIdx].mobj->speed = (uint16_t)(g_objectTable[objectIdx].mobj->speed + wholeDelta);
		if (g_objectTable[objectIdx].mobj->speed > 3600u) {
			g_objectTable[objectIdx].mobj->speed = 3600;
		}
	} else if (allowDecel == 1) {
		uint32_t step;
		uint32_t scaledDelta;
		uint32_t wholeDelta;
		uint16_t fracDelta;
		uint16_t oldRemainder;

		step =
			(uint16_t)MATH2_fraction(g_modelDefs[(uint16_t)g_curCraftModelIndex.packed].decelRate, 0x4000u);
		if (step == 0) {
			step = 1;
		}
		step += (uint16_t)MATH2_fraction(
			(uint16_t)(g_modelDefs[(uint16_t)g_curCraftModelIndex.packed].decelRate - step),
			(uint16_t)(0xffffu - fracQ16));

		speedDelta = -speedDelta;
		if (speedDelta >= step) {
			speedDelta = step;
		}

		scaledDelta = (uint32_t)(uint16_t)g_elapsedTicks * speedDelta;
		wholeDelta = scaledDelta / 236u;
		fracDelta = (uint16_t)(((scaledDelta - wholeDelta * 236u) << 16) / 236u);

		oldRemainder = g_objectTable[objectIdx].mobj->speedRemainder;
		g_objectTable[objectIdx].mobj->speedRemainder = (uint16_t)(oldRemainder - fracDelta);
		if (g_objectTable[objectIdx].mobj->speedRemainder > oldRemainder) {
			--g_objectTable[objectIdx].mobj->speed;
		}

		g_objectTable[objectIdx].mobj->speed = (uint16_t)(g_objectTable[objectIdx].mobj->speed - wholeDelta);
		if (g_objectTable[objectIdx].mobj->speed > 0x8000u) {
			g_objectTable[objectIdx].mobj->speed = 0;
		}
	}
}

// FUNCTION: XWA 0x42D4A0
void Flight_RecomputeCraftSpeedFromPowerSettings(int objectIdx) {
	MobileObject* mobj;

	mobj = g_objectTable[objectIdx].mobj;
	if (mobj == 0) {
		return;
	}

	{
		CraftData* craft;
		uint16_t commandedSpeed;
		uint16_t throttleSpeed;
		uint16_t recomputedSpeed;

		craft = mobj->pCraft;
		if (craft == 0) {
			return;
		}

		commandedSpeed = (uint16_t)craft->commandedSpeed;
		throttleSpeed = craft->throttleSpeed;
		if (commandedSpeed != 0 && throttleSpeed == 0xffffu) {
			recomputedSpeed = commandedSpeed;
		} else {
			uint16_t maxSpeed;
			uint16_t enginePowerMargin;

			enginePowerMargin = 6;
			enginePowerMargin = (uint16_t)(enginePowerMargin - craft->beamLevel);
			enginePowerMargin = (uint16_t)(enginePowerMargin - craft->shieldRedirect);
			enginePowerMargin = (uint16_t)(enginePowerMargin - craft->laserRedirect);
			maxSpeed = craft->aiFlight.maxSpeedCache;
			if (g_objectTable[objectIdx].objectType == OBJ_TIEBomber) {
				maxSpeed =
					(uint16_t)(maxSpeed + enginePowerMargin * (uint16_t)MATH2_fraction(maxSpeed, 0x1000u));
			} else if (g_objectTable[objectIdx].objectType == OBJ_TIEFighter && enginePowerMargin > 0) {
				maxSpeed =
					(uint16_t)(maxSpeed + enginePowerMargin * (uint16_t)MATH2_fraction(maxSpeed, 0x3000u));
			} else {
				maxSpeed =
					(uint16_t)(maxSpeed + enginePowerMargin * (uint16_t)MATH2_fraction(maxSpeed, 0x2000u));
			}

			recomputedSpeed = (uint16_t)MATH2_fraction(maxSpeed, throttleSpeed);
			if (craft->slamActive != 0) {
				recomputedSpeed = (uint16_t)(recomputedSpeed << 1);
			}
		}

		mobj->speed = recomputedSpeed;
	}
}

static __inline void Flight_SetElapsedTicksLow(uint16_t lowWord) { g_elapsedTicks = lowWord; }

static __inline uint16_t Flight_AngleMagnitude(uint16_t delta) {
	if (delta >= 0x8000u) {
		delta = (uint16_t)-delta;
	}

	return delta;
}

static __inline uint16_t Flight_ScaledElapsedAxisStep(uint16_t rate) {
	int32_t product;

	product = (int32_t)((uint32_t)(uint16_t)g_elapsedTicks * rate);
	return (uint16_t)(product / 236);
}

static __inline void Flight_DecelerateSpeedByStep(uint16_t objectIdx, uint32_t decelStep) {
	uint32_t product;
	uint32_t wholeDelta;
	uint16_t fracDelta;
	uint16_t oldRemainder;
	MobileObject* mobj;

	product = (uint32_t)(uint16_t)g_elapsedTicks * decelStep;
	wholeDelta = product / 236u;
	fracDelta = (uint16_t)(((product - wholeDelta * 236u) << 16) / 236u);
	mobj = g_objectTable[objectIdx].mobj;

	oldRemainder = mobj->speedRemainder;
	mobj->speedRemainder = (uint16_t)(oldRemainder - fracDelta);
	if (g_objectTable[objectIdx].mobj->speedRemainder > oldRemainder) {
		--g_objectTable[objectIdx].mobj->speed;
	}
	g_objectTable[objectIdx].mobj->speed -= (uint16_t)wholeDelta;
	if (g_objectTable[objectIdx].mobj->speed > 0x8000u) {
		g_objectTable[objectIdx].mobj->speed = 0;
	}
}

static __inline void Flight_ResetAiMotionStates(void) {
	g_curCraft->aiFlight.climbState = 0;
	g_curCraft->aiFlight.diveState = 0;
	g_curCraft->aiFlight.enterFlag = 0;
	g_curCraft->aiFlight.headingState = 0;
}

// Axis (roll/pitch/yaw) slews scale by aiFlight.motionScale, NOT by the
// throttle-scaled speed fraction the caller derives. Turn rate must not depend
// on throttle, otherwise craft under-turn at reduced throttle and cannot close
// a bearing (e.g. boarding approaches orbit instead of docking).
static __inline void Flight_UpdateAiCraftOrientation(uint16_t objectIdx, AiController* ai) {
	if (g_curCraft->workingSubsystems == 0 || g_curCraft->beamEffectAccum[1] != 0 ||
		g_curCraft->nextLinkObjectIdx != 0xffffu) {
		return;
	}

	if ((uint8_t)g_curCraft->aiFlight.enterFlag >= 1u && (uint8_t)g_curCraft->aiFlight.enterFlag <= 3u) {
		uint16_t rollDelta;
		uint16_t rollStep;

		rollDelta = (uint16_t)(ai->targetRoll - g_objectTable[objectIdx].roll);
		if ((uint16_t)g_curCraft->aiFlight.rollAccel != 0xffffu) {
			uint16_t oldAccel;
			uint16_t newAccel;

			oldAccel = (uint16_t)g_curCraft->aiFlight.rollAccel;
			newAccel = (uint16_t)(oldAccel + Flight_ScaledElapsedAxisStep(oldAccel));
			g_curCraft->aiFlight.rollAccel = (int16_t)newAccel;
			if (oldAccel > (uint16_t)g_curCraft->aiFlight.rollAccel) {
				g_curCraft->aiFlight.rollAccel = (int16_t)0xffffu;
			}
			if ((uint16_t)g_curCraft->aiFlight.rollAccel == 0) {
				g_curCraft->aiFlight.rollAccel = (int16_t)0xffffu;
			}
		}
		rollStep = Flight_ScaledElapsedAxisStep((uint16_t)g_curCraft->aiFlight.rollRate);
		rollStep = (uint16_t)MATH2_fraction(rollStep, (uint16_t)g_curCraft->aiFlight.rollAccel);
		rollStep = (uint16_t)MATH2_fraction(rollStep, (uint16_t)g_curCraft->aiFlight.rollStep);
		rollStep = (uint16_t)MATH2_fraction(rollStep, (uint16_t)g_curCraft->aiFlight.motionScale);
		rollStep = (uint16_t)(rollStep << 1);
		if (g_curCraft->aiFlight.enterFlag != 3) {
			if (rollDelta < 0x8000u) {
				if (rollDelta <= rollStep) {
					g_objectTable[objectIdx].roll = ai->targetRoll;
					g_curCraft->aiFlight.enterFlag = 4;
				} else {
					g_objectTable[objectIdx].roll = (uint16_t)(g_objectTable[objectIdx].roll + rollStep);
				}
			} else {
				rollDelta = (uint16_t)-rollDelta;
				if (rollDelta <= rollStep) {
					g_objectTable[objectIdx].roll = ai->targetRoll;
					g_curCraft->aiFlight.enterFlag = 4;
				} else {
					g_objectTable[objectIdx].roll = (uint16_t)(g_objectTable[objectIdx].roll - rollStep);
				}
			}
		} else {
			if (ai->targetRoll < 0x8000u) {
				g_objectTable[objectIdx].roll = (uint16_t)(g_objectTable[objectIdx].roll + rollStep);
			} else {
				g_objectTable[objectIdx].roll = (uint16_t)(g_objectTable[objectIdx].roll - rollStep);
			}
		}
	}

	if ((uint8_t)g_curCraft->aiFlight.headingState > 0u) {
		uint16_t pitchDelta;
		uint16_t pitchStep;

		pitchDelta = Flight_AngleMagnitude((uint16_t)(ai->targetZAngle - g_objectTable[objectIdx].pitch));
		pitchStep = Flight_ScaledElapsedAxisStep((uint16_t)g_curCraft->aiFlight.pitchRate);
		pitchStep = (uint16_t)MATH2_fraction(pitchStep, (uint16_t)g_curCraft->aiFlight.pitchAccel);
		pitchStep = (uint16_t)MATH2_fraction(pitchStep, (uint16_t)g_curCraft->aiFlight.headingStep);
		pitchStep = (uint16_t)MATH2_fraction(pitchStep, (uint16_t)g_curCraft->aiFlight.motionScale);
		if (g_curCraft->aiFlight.headingState == 1) {
			if (pitchDelta > pitchStep) {
				g_objectTable[objectIdx].pitch = (uint16_t)(g_objectTable[objectIdx].pitch - pitchStep);
				if (g_objectTable[objectIdx].pitch >= 0xe000u) {
					g_objectTable[objectIdx].pitch = (uint16_t)-g_objectTable[objectIdx].pitch;
					g_objectTable[objectIdx].yaw = (uint16_t)(g_objectTable[objectIdx].yaw + 0x8000u);
					g_objectTable[objectIdx].roll = (uint16_t)(g_objectTable[objectIdx].roll + 0x8000u);
					g_curCraft->aiFlight.headingForce = 0;
					g_curCraft->aiFlight.headingState = 2;
				}
			} else if (!g_curCraft->aiFlight.headingForce) {
				g_objectTable[objectIdx].pitch = ai->targetZAngle;
				g_curCraft->aiFlight.headingState = 3;
			} else {
				g_objectTable[objectIdx].pitch = (uint16_t)(g_objectTable[objectIdx].pitch - pitchStep);
				if (g_objectTable[objectIdx].pitch >= 0xe000u) {
					g_objectTable[objectIdx].pitch = (uint16_t)-g_objectTable[objectIdx].pitch;
					g_objectTable[objectIdx].yaw = (uint16_t)(g_objectTable[objectIdx].yaw + 0x8000u);
					g_objectTable[objectIdx].roll = (uint16_t)(g_objectTable[objectIdx].roll + 0x8000u);
					g_curCraft->aiFlight.headingForce = 0;
					g_curCraft->aiFlight.headingState = 2;
				}
			}
		} else {
			if (g_curCraft->aiFlight.headingState == 2) {
				if (pitchDelta <= pitchStep && !g_curCraft->aiFlight.headingForce) {
					g_objectTable[objectIdx].pitch = ai->targetZAngle;
					g_curCraft->aiFlight.headingState = 3;
				} else {
					g_objectTable[objectIdx].pitch = (uint16_t)(g_objectTable[objectIdx].pitch + pitchStep);
					if (g_objectTable[objectIdx].pitch >= 0x8000u) {
						g_objectTable[objectIdx].pitch = (uint16_t)-g_objectTable[objectIdx].pitch;
						g_objectTable[objectIdx].yaw = (uint16_t)(g_objectTable[objectIdx].yaw + 0x8000u);
						g_objectTable[objectIdx].roll = (uint16_t)(g_objectTable[objectIdx].roll + 0x8000u);
						g_curCraft->aiFlight.headingForce = 0;
						g_curCraft->aiFlight.headingState = 1;
					}
				}
			}
		}
	}

	if (g_curCraft->objectKind != 2) {
		uint8_t turnState;
		uint16_t targetDelta;

		turnState = g_curCraft->aiFlight.turnState;
		if (turnState >= 1) {
			uint16_t yawMagnitude;
			uint16_t turnStep;
			uint16_t oldYaw;
			uint16_t actualYawStep;

			targetDelta = (uint16_t)(ai->targetXYAngle - g_objectTable[objectIdx].yaw);
			if (targetDelta != 0) {
				yawMagnitude = targetDelta;
				if (yawMagnitude >= 0x8000u) {
					yawMagnitude = (uint16_t)-yawMagnitude;
				}
				if ((uint16_t)g_curCraft->aiFlight.turnAccel != 0xffffu) {
					uint16_t oldAccel;
					uint16_t newAccel;

					oldAccel = (uint16_t)g_curCraft->aiFlight.turnAccel;
					newAccel = (uint16_t)(oldAccel + Flight_ScaledElapsedAxisStep(oldAccel));
					g_curCraft->aiFlight.turnAccel = (int16_t)newAccel;
					if (oldAccel > (uint16_t)g_curCraft->aiFlight.turnAccel) {
						g_curCraft->aiFlight.turnAccel = (int16_t)0xffffu;
					}
					if ((uint16_t)g_curCraft->aiFlight.turnAccel == 0) {
						g_curCraft->aiFlight.turnAccel = (int16_t)0xffffu;
					}
				}
				turnStep = Flight_ScaledElapsedAxisStep((uint16_t)g_curCraft->aiFlight.turnRate);
				turnStep = (uint16_t)MATH2_fraction(turnStep, (uint16_t)g_curCraft->aiFlight.turnAccel);
				turnStep = (uint16_t)MATH2_fraction(turnStep, (uint16_t)g_curCraft->aiFlight.turnStep);
				turnStep = (uint16_t)MATH2_fraction(turnStep, (uint16_t)g_curCraft->aiFlight.motionScale);
				oldYaw = g_objectTable[objectIdx].yaw;
				actualYawStep = turnStep;
				if (yawMagnitude < turnStep) {
					actualYawStep = 0;
					g_objectTable[objectIdx].yaw = ai->targetXYAngle;
					g_curCraft->aiFlight.turnState = 3;
				} else {
					if (targetDelta < 0x8000u) {
						g_objectTable[objectIdx].yaw = (uint16_t)(oldYaw + turnStep);
					} else {
						g_objectTable[objectIdx].yaw = (uint16_t)(oldYaw - turnStep);
					}
				}

				if ((g_modelTypeTable[(uint16_t)g_objectTable[objectIdx].objectType].flags &
					 MODEL_TYPE_FLAG_YAW_UPDATES_ANGLE_D) != 0) {
					g_objectTable[objectIdx].angleD =
						(uint16_t)(g_objectTable[objectIdx].angleD +
								   (uint16_t)(g_objectTable[objectIdx].yaw - oldYaw));
				}

				if (g_curCraft->aiFlight.enterFlag == 0 || g_curCraft->aiFlight.enterFlag == 4) {
					int16_t bank;

					bank = (int16_t)MATH2_fraction(
						actualYawStep, g_modelDefs[(uint16_t)g_curCraftModelIndex.packed].autoBankFactor);
					if (targetDelta < 0x8000u) {
						g_objectTable[objectIdx].roll = (uint16_t)(g_objectTable[objectIdx].roll - bank);
						if ((g_objectTable[objectIdx].genusId == GENUS_Starship ||
							 g_objectTable[objectIdx].genusId == GENUS_Freighter) &&
							g_objectTable[objectIdx].roll < 0xe000u &&
							g_objectTable[objectIdx].roll > 0x8000u) {
							g_objectTable[objectIdx].roll = 0xe000u;
						}
					} else {
						g_objectTable[objectIdx].roll = (uint16_t)(g_objectTable[objectIdx].roll + bank);
						if ((g_objectTable[objectIdx].genusId == GENUS_Starship ||
							 g_objectTable[objectIdx].genusId == GENUS_Freighter) &&
							g_objectTable[objectIdx].roll > 0x2000u &&
							g_objectTable[objectIdx].roll < 0x4000u) {
							g_objectTable[objectIdx].roll = 0x2000u;
						}
					}
				}
			} else if (turnState == 2) {
				g_curCraft->aiFlight.turnState = 3;
			}
		}
	}
}

// FUNCTION: XWA 0x42BEA0
void Flight_UpdateCraftSteeringAndSpeed(void) {
	uint32_t savedSimStepScale;
	uint32_t savedElapsedTicksLow;
	uint32_t slot;
	int processedOverride;

	savedSimStepScale = (uint16_t)g_simStepScale;
	savedElapsedTicksLow = (uint16_t)g_elapsedTicks;
	slot = g_activeRegionObjectSlotStart;
	processedOverride = 0;

	if ((uint16_t)slot < g_activeRegionCraftObjectSlotEnd) {
		for (; (uint16_t)slot < g_activeRegionCraftObjectSlotEnd; ++slot) {
			MobileObject* mobj;
			AiController* ai;
			uint16_t oldPitch;
			uint16_t oldYaw;
			uint16_t oldRoll;
			int playerOwnerIdx;
			int aiDriven;
			uint16_t throttleSpeed;
			uint16_t motionFrac;

			if (g_objectTable[(uint16_t)slot].objectType == OBJ_None) {
				continue;
			}
			if (g_singleObjectUpdateOverrideIdx != -1) {
				if (processedOverride) {
					break;
				}
				processedOverride = 1;
				slot = g_singleObjectUpdateOverrideIdx;
			}

			g_simStepScale = (uint16_t)savedSimStepScale;
			Flight_SetElapsedTicksLow((uint16_t)savedElapsedTicksLow);

			mobj = g_objectTable[(uint16_t)slot].mobj;
			if (mobj != NULL) {
				uint32_t simStateTimestamp;

				simStateTimestamp = mobj->simStateTimestamp;
				if (simStateTimestamp != 0) {
					uint16_t deltaTicks;

					deltaTicks = (uint16_t)(g_gameTime + (uint16_t)savedElapsedTicksLow -
											(uint16_t)mobj->simStateTimestamp);
					Flight_SetElapsedTicksLow(deltaTicks);
					if (deltaTicks == 0) {
						continue;
					}
					g_simStepScale = (uint16_t)(236 / (int)deltaTicks);
					if (g_simStepScale == 0) {
						g_simStepScale = 1;
					}
				}
			}

			if (g_objectTable[(uint16_t)slot].objectType == OBJ_None) {
				continue;
			}
			mobj = g_objectTable[(uint16_t)slot].mobj;
			if (mobj->state != 0) {
				continue;
			}

			g_curCraft = mobj->pCraft;
			ai = pai_GetEffectiveAIController(g_curCraft);
			oldPitch = g_objectTable[(uint16_t)slot].pitch;
			oldYaw = g_objectTable[(uint16_t)slot].yaw;
			oldRoll = g_objectTable[(uint16_t)slot].roll;
			g_curCraftModelIndex.words[0] = g_curCraft->modelIndex;
			playerOwnerIdx = g_objectTable[(uint16_t)slot].playerOwnerIdx;
			aiDriven = playerOwnerIdx == -1 || g_players[playerOwnerIdx].aiControlledFlag;

			throttleSpeed = 0;
			if (playerOwnerIdx != -1) {
				if ((g_curCraft->workingSubsystems & 0x40u) != 0) {
					throttleSpeed = g_curCraft->throttleSpeed;
				}
			} else {
				if (g_curCraft->workingSubsystems != 0) {
					throttleSpeed =
						(uint16_t)MATH2_fraction(g_curCraft->throttleSpeed, g_curCraft->engineOutputScale);
				}
			}
			motionFrac = (uint16_t)MATH2_fraction(throttleSpeed, (uint16_t)g_curCraft->aiFlight.motionScale);

			if (aiDriven) {
				Flight_UpdateAiCraftOrientation((uint16_t)slot, ai);
			}

			if (aiDriven) {
				if (g_curCraft->aiFlight.climbState == 1 && g_curCraft->workingSubsystems != 0 &&
					g_curCraft->beamEffectAccum[1] == 0 &&
					g_objectTable[(uint16_t)slot].world_z >= ai->aimPointZ) {
					g_curCraft->aiFlight.climbState = 0;
					g_objectTable[(uint16_t)slot].pitch = 0x4000u;
				}
			}

			if (aiDriven) {
				if (g_curCraft->aiFlight.diveState == 1 && g_curCraft->workingSubsystems != 0 &&
					g_curCraft->beamEffectAccum[1] == 0) {
					Flight_UpdateDivePulloutPitchTarget((uint16_t)slot);
				}
			}

			switch (g_curCraft->objectKind) {
				case 0: {
					uint16_t targetSpeed;

					targetSpeed = (uint16_t)g_curCraft->commandedSpeed;
					if (targetSpeed == 0 || motionFrac != 0xffffu ||
						g_objectTable[(uint16_t)slot].playerOwnerIdx != -1) {
						uint16_t speedBonusStep;
						uint16_t maxSpeed;
						int enginePowerMargin;

						maxSpeed = (uint16_t)g_curCraft->aiFlight.maxSpeedCache;
						enginePowerMargin = 6 - g_curCraft->laserRedirect - g_curCraft->shieldRedirect -
											g_curCraft->beamLevel;
						if (g_objectTable[(uint16_t)slot].objectType == OBJ_TIEBomber) {
							speedBonusStep = (uint16_t)MATH2_fraction(maxSpeed, 0x1000u);
						} else if (g_objectTable[(uint16_t)slot].objectType == OBJ_TIEFighter &&
								   (int16_t)enginePowerMargin > 0) {
							speedBonusStep = (uint16_t)MATH2_fraction(maxSpeed, 0x3000u);
						} else {
							speedBonusStep = (uint16_t)MATH2_fraction(maxSpeed, 0x2000u);
						}
						targetSpeed = (uint16_t)MATH2_fraction(
							(uint16_t)(enginePowerMargin * speedBonusStep + maxSpeed), motionFrac);
						if (g_curCraft->slamActive != 0) {
							targetSpeed = (uint16_t)(targetSpeed << 1);
						}
					}

					if (g_players[g_localPlayer].objectIndex == (int)slot) {
						g_forceFeedbackLocalSpeedSnapshot.words[0] = targetSpeed;
					}
					if (targetSpeed < g_objectTable[(uint16_t)slot].mobj->speed) {
						uint32_t speedDelta;

						speedDelta = (uint32_t)(g_objectTable[(uint16_t)slot].mobj->speed - targetSpeed);
						if (speedDelta < 200u) {
							Flight_SlewObjectSpeedTowardTarget(slot, targetSpeed, 1, motionFrac);
						} else {
							Flight_DecelerateSpeedByStep((uint16_t)slot, speedDelta / 3u);
						}
					} else {
						Flight_SlewObjectSpeedTowardTarget(slot, targetSpeed, 1, motionFrac);
					}
					break;
				}

				case 2:
					if (g_objectTable[(uint16_t)slot].mobj->speed > 0u) {
						Flight_DecelerateHyperspaceSpeedInline((uint16_t)slot, 20);
					}
					break;

				case 1:
				case 3:
				case 4:
					if (g_objectTable[(uint16_t)slot].mobj->speed >
						(uint16_t)g_curCraft->aiFlight.maxSpeedCache) {
						Flight_DecelerateSpeedByStep((uint16_t)slot,
													 (g_objectTable[(uint16_t)slot].mobj->speed -
													  (uint16_t)g_curCraft->aiFlight.maxSpeedCache) /
														 3);
					}
					/* Fall through: these modes clear the same AI motion state as mode 6. */

				case 6:
					Flight_ResetAiMotionStates();
					break;

				case 5:
					if (g_curCraft->workingSubsystems != 0) {
						Flight_AccelerateHyperspaceSpeedInline((uint16_t)slot, 1500);
					} else {
						if (g_objectTable[(uint16_t)slot].mobj->speed > 0u) {
							Flight_DecelerateHyperspaceSpeedInline((uint16_t)slot, 20);
						}
						if (g_objectTable[(uint16_t)slot].mobj->speed == 0) {
							g_curCraft->objectKind = 0;
						}
					}
					break;

				default:
					break;
			}

			if (oldPitch != g_objectTable[(uint16_t)slot].pitch ||
				oldYaw != g_objectTable[(uint16_t)slot].yaw ||
				oldRoll != g_objectTable[(uint16_t)slot].roll) {
				g_objectTable[(uint16_t)slot].mobj->moveVectorDirty = 1;
				g_objectTable[(uint16_t)slot].mobj->orientMatrixDirty = 1;
			}

			if (g_curCraft->carriedObjectIndex != 0xffffu) {
				uint16_t carriedIdx;

				carriedIdx = g_curCraft->carriedObjectIndex;
				if (g_objectTable[carriedIdx].mobj != NULL) {
					g_objectTable[carriedIdx].pitch = g_objectTable[(uint16_t)slot].pitch;
					g_objectTable[carriedIdx].yaw = g_objectTable[(uint16_t)slot].yaw;
					g_objectTable[carriedIdx].roll = g_objectTable[(uint16_t)slot].roll;
					g_objectTable[carriedIdx].angleD = g_objectTable[(uint16_t)slot].angleD;
					g_objectTable[carriedIdx].mobj->moveVectorDirty = 1;
					g_objectTable[carriedIdx].mobj->orientMatrixDirty = 1;
				}
			}

			if (g_curCraft->nextLinkObjectIdx == 0xffffu) {
				CraftData* craft;
				uint16_t linkedObjIdx;
				uint16_t sourceObjIdx;
				uint16_t sourceShadowIdx;
				uint16_t previousSourceIdx;

				linkedObjIdx = g_curCraft->linkedPrevObjectIdx;
				sourceObjIdx = (uint16_t)slot;
				craft = g_curCraft;
				sourceShadowIdx = (uint16_t)slot;
				previousSourceIdx = (uint16_t)slot;
				while (linkedObjIdx != 0xffffu) {
					uint16_t linkedIdx;
					uint16_t yawDelta;
					uint16_t yawMagnitude;
					uint16_t desiredYaw;
					uint16_t copySourceIdx;

					copySourceIdx = sourceObjIdx;
					linkedIdx = linkedObjIdx;

					g_objectTable[linkedIdx].pitch = g_objectTable[copySourceIdx].pitch;
					desiredYaw = g_objectTable[copySourceIdx].yaw;
					yawDelta = (uint16_t)(desiredYaw - g_objectTable[linkedIdx].yaw);
					yawMagnitude = yawDelta;
					if (yawDelta != 0) {
						uint8_t allowFineSlew;

						allowFineSlew = 0;
						if (yawMagnitude >= 0x8000u) {
							yawMagnitude = (uint16_t)-yawMagnitude;
						}
						if (yawMagnitude > 0x800u) {
							if (yawDelta > 0x8000u) {
								g_objectTable[linkedIdx].yaw = (uint16_t)(desiredYaw + 0x800u);
							} else {
								g_objectTable[linkedIdx].yaw = (uint16_t)(desiredYaw - 0x800u);
							}
						} else {
							if (sourceObjIdx != (uint16_t)slot || craft->aiFlight.turnState == 2) {
								sourceObjIdx = sourceShadowIdx;
								allowFineSlew = g_objectTable[previousSourceIdx].yaw == desiredYaw &&
												previousSourceIdx != sourceShadowIdx;
							} else {
								allowFineSlew = 1;
							}
						}

						if (allowFineSlew) {
							uint16_t step;

							step = Flight_ScaledElapsedAxisStep((uint16_t)craft->aiFlight.turnRate);
							step = (uint16_t)MATH2_fraction(step, (uint16_t)craft->aiFlight.turnAccel);
							step = (uint16_t)MATH2_fraction(step, (uint16_t)craft->aiFlight.turnStep);
							if (yawMagnitude < step) {
								g_objectTable[linkedIdx].yaw = g_objectTable[copySourceIdx].yaw;
							} else {
								if (yawDelta < 0x8000u) {
									g_objectTable[linkedIdx].yaw =
										(uint16_t)(g_objectTable[linkedIdx].yaw + step);
								} else {
									g_objectTable[linkedIdx].yaw =
										(uint16_t)(g_objectTable[linkedIdx].yaw - step);
								}
							}
						}
					}

					previousSourceIdx = sourceObjIdx;
					g_objectTable[linkedIdx].roll = g_objectTable[copySourceIdx].roll;
					sourceObjIdx = linkedObjIdx;
					sourceShadowIdx = linkedObjIdx;
					g_objectTable[linkedIdx].angleD = g_objectTable[copySourceIdx].angleD;
					g_objectTable[linkedIdx].mobj->moveVectorDirty = 1;
					g_objectTable[linkedIdx].mobj->orientMatrixDirty = 1;
					linkedObjIdx = g_objectTable[linkedIdx].mobj->pCraft->linkedPrevObjectIdx;
				}
			}
		}
	}

	g_simStepScale = (uint16_t)savedSimStepScale;
	Flight_SetElapsedTicksLow((uint16_t)savedElapsedTicksLow);
}

// FUNCTION: XWA 0x42D3A0
void Flight_UpdateDivePulloutPitchTarget(int objectIdx) {
	AiController* ai;
	ObjectRecord* obj;
	int deltaZToAim;
	int pulloutThreshold;

	ai = pai_GetEffectiveAIController(g_curCraft);
	obj = &g_objectTable[objectIdx];
	deltaZToAim = obj->world_z - ai->aimPointZ;
	if (deltaZToAim >= 0 && deltaZToAim > 256) {
		if (obj->mobj->moveVectorDirty) {
			FVIEW_calcrotatemove(obj->pitch, obj->yaw, obj);
		}

		if (obj->genusId == 0) {
			pulloutThreshold = -3 * (int)g_simStepScale * (int)obj->mobj->moveZ;
		} else {
			pulloutThreshold = -2 * (int)g_simStepScale * (int)obj->mobj->moveZ;
		}

		if (deltaZToAim <= pulloutThreshold) {
			uint16_t currentPitch;

			currentPitch = obj->pitch;
			if (currentPitch > 0x4000u) {
				ai->targetZAngle = (uint16_t)((((int)currentPitch - 0x4000) >> 1) + 0x4000);
				g_curCraft->aiFlight.headingState = 1;
			}
		}
	} else {
		obj->pitch = 0x4000;
		g_curCraft->aiFlight.headingState = 0;
		g_curCraft->aiFlight.diveState = 2;
	}
}

// FUNCTION: XWA 0x4F8410
InputFrame* FlightNet_FindLastNonzeroInputFrame(int playerIdx) {
	int remaining;
	InputFrame* frame;
	InputFrame* result;

	remaining = g_inputFrameCount[playerIdx];
	frame = g_inputHistory[playerIdx];
	result = NULL;
	while (remaining > 0) {
		if (frame->applied != 0) {
			result = frame;
		}
		++frame;
		--remaining;
	}

	return result;
}

// FUNCTION: XWA 0x4F8350
InputFrame* FlightNet_InsertInputFrame(int playerIdx, int timestamp, const FlightInputFrameRecord* input) {
	int idx;
	int count;
	InputFrame* frame;
	InputFrame* arrayEnd;
	int existingTime;

	idx = 0;
	count = g_inputFrameCount[playerIdx];
	frame = g_inputHistory[playerIdx];
	arrayEnd = &frame[count];

	while (idx < count && frame->timestamp < timestamp) {
		++idx;
		++frame;
	}

	existingTime = frame->timestamp;
	if (existingTime > timestamp || idx == count) {
		if (count == XWA_INPUT_HISTORY_FRAME_COUNT) {
			return NULL;
		}
		g_inputFrameCount[playerIdx] = count + 1;
		if (arrayEnd > frame) {
			InputFrame* shiftDestination;

			shiftDestination = arrayEnd + 1;
			do {
				*--shiftDestination = *--arrayEnd;
			} while (arrayEnd > frame);
		}
	} else if (timestamp == existingTime) {
		if (!frame->valid) {
			return NULL;
		}
		if (frame->applied == 1) {
			return NULL;
		}
	}

	frame->timestamp = timestamp;
	frame->valid = 1;
	frame->applied = 0;
	frame->input = *input;
	return frame;
}

// FUNCTION: XWA 0x4F81B0
void FlightNet_QueuePredictedRemoteInputFrames(int predictedFrameDelta) {
	int playerIdx;

	if (g_asyncFlag) {
		return;
	}

	for (playerIdx = 0; playerIdx < XWA_INPUT_HISTORY_PLAYER_COUNT; ++playerIdx) {
		int count;
		InputFrame* lastFrame;
		InputFrame* predictedFrame;
		FlightInputFrameRecord input;

		if (!g_players[playerIdx].connectedFlag || playerIdx == g_localPlayer) {
			continue;
		}

		count = g_inputFrameCount[playerIdx];
		if (count == 0) {
			continue;
		}

		lastFrame = &g_inputHistory[playerIdx][count - 1];
		memset(&input, 0, sizeof(input));
		input.axisX = lastFrame->input.axisX;
		input.axisY = lastFrame->input.axisY;
		input.axisR = lastFrame->input.axisR;

		predictedFrame =
			FlightNet_InsertInputFrame(playerIdx, lastFrame->timestamp + predictedFrameDelta, &input);
		if (predictedFrame != NULL) {
			predictedFrame->applied = 0;
			predictedFrame->valid = 2;
		}
	}
}

// FUNCTION: XWA 0x4F8260
__inline void FlightNet_DiscardPredictedInputFrames(int playerIdx) {
	int frameIdx;
	InputFrame* frame;
	InputFrame* frameBase;

	if (g_asyncFlag) {
		return;
	}
	if (!g_players[playerIdx].connectedFlag) {
		return;
	}
	if (playerIdx == g_localPlayer) {
		return;
	}

	frame = g_inputHistory[playerIdx];
	frameBase = frame;
	frameIdx = 0;
	while (frameIdx < g_inputFrameCount[playerIdx]) {
		if (frame->applied != 0 || frame->valid != 2) {
			++frame;
			++frameIdx;
			continue;
		}

		if (g_inputFrameCount[playerIdx] != 0 && frame >= frameBase) {
			int copyIdx;

			--g_inputFrameCount[playerIdx];
			copyIdx = 0;
			while (copyIdx < g_inputFrameCount[playerIdx]) {
				if (frameBase >= frame) {
					*frameBase = frameBase[1];
				}
				++copyIdx;
				++frameBase;
			}
			frameBase = g_inputHistory[playerIdx];
		}
	}
}

static __inline uint16_t FlightNet_ReadPacketWord(const uint8_t* packet) {
	uint16_t value;

	memcpy(&value, packet, sizeof(value));
	return value;
}

static __inline uint32_t FlightNet_PeekPacketDword(const uint8_t* packet) {
	uint32_t value;

	memcpy(&value, packet, sizeof(value));
	return value;
}

// FUNCTION: XWA 0x4F8A40
void FlightNet_ApplyWorldMessagePacket(uint8_t* packet) {
	uint32_t rawPacketTick;
	int packetTick;
	int checksumRequested;
	FlightInputFrameRecord input;

	if (NetSession_GetLocalPlayerId() == 0 && g_flightNetBufferWorldMessagesUntilChecksum == 1) {
		uint8_t* cursor;
		int packetLen;
		int connectedSections;

		cursor = packet + 8;
		packetLen = 9;
		connectedSections = *cursor++;
		while (connectedSections > 0) {
			int frameCount;

			frameCount = *cursor++;
			++packetLen;
			while (frameCount > 0) {
				int code;

				code = *cursor++;
				++packetLen;
				if ((code & 0x7f) == 0x7f) {
					cursor += 4;
					packetLen += 4;
				}
				if ((code & 0x80) != 0) {
					++cursor;
					++packetLen;
				}
				cursor += 3;
				packetLen += 3;
				--frameCount;
			}
			--connectedSections;
		}

		if (g_worldMessageBufferBytesFree < packetLen) {
			int oldHandle;
			int oldCapacity;
			int growBytes;

			oldHandle = g_worldMessageBufferHandle;
			oldCapacity = g_worldMessageBufferCapacity;
			growBytes = packetLen * 100;
			g_worldMessageBufferBytesFree += growBytes;
			g_worldMessageBufferCapacity += growBytes;

			g_worldMessageBufferHandle =
				Memory_AllocHandle("WORLDMESSAGEBUF", (size_t)g_worldMessageBufferCapacity);
			if (g_worldMessageBufferHandle == 0) {
				FeDiskIo_FatalError(0);
			}
			g_worldMessageBuffer = (uint8_t*)Memory_LockHandle(g_worldMessageBufferHandle);

			if (oldHandle != 0) {
				uint8_t* oldBuffer;

				oldBuffer = (uint8_t*)Memory_LockHandle(oldHandle);
				memcpy(g_worldMessageBuffer, oldBuffer, (size_t)oldCapacity);
				Memory_UnlockHandle(oldHandle);
				Memory_FreeHandle("WORLDMESSAGEBUF", oldHandle);
			}
		}

		memcpy(&g_worldMessageBuffer[g_worldMessageBufferCapacity - g_worldMessageBufferBytesFree], packet,
			   (size_t)packetLen);
		g_worldMessageBufferBytesFree -= packetLen;
		++g_worldMessageBufferedCount;
	}

	rawPacketTick = FlightNet_PeekPacketDword(packet + 4);
	packet += 8;
	packetTick = (int)rawPacketTick;
	checksumRequested = (int)(rawPacketTick & 0x80000000u);
	packetTick &= 0x7fffffff;
	if (packetTick <= g_serverTickTime) {
		return;
	}

	if (packetTick - g_serverTickTime != dtMs) {
		NetReliable_ResetRecvQueueState();
	}

	if (!g_asyncFlag) {
		int playerIdx;

		for (playerIdx = 0; playerIdx < XWA_INPUT_HISTORY_PLAYER_COUNT; ++playerIdx) {
			FlightNet_DiscardPredictedInputFrames(playerIdx);
		}
	}

	Flight_RestoreWorldState();
	g_gameTime = g_serverTickTime;

	if (g_flightNetDirtyAllObjectTransformsAfterRestore != 0) {
		uint32_t slot;

		for (slot = 0; slot < g_regionObjectSlotEnd; ++slot) {
			if (g_objectTable[slot].objectType != OBJ_None && g_objectTable[slot].mobj != NULL) {
				g_objectTable[slot].mobj->moveVectorDirty = 1;
				g_objectTable[slot].mobj->orientMatrixDirty = 1;
			}
		}
		g_flightNetDirtyAllObjectTransformsAfterRestore = 0;
	}

	{
		uint8_t* cursor;
		int remainingSections;
		int playerIdx;

		cursor = packet;
		remainingSections = *cursor++;
		for (playerIdx = 0; playerIdx < XWA_INPUT_HISTORY_PLAYER_COUNT; ++playerIdx) {
			if (!g_players[playerIdx].connectedFlag) {
				continue;
			}
			if (remainingSections == 0) {
				break;
			}

			--remainingSections;
			{
				int frameCount;

				frameCount = *cursor++;
				while (frameCount > 0) {
					int code;
					int deltaCode;
					int timestamp;
					InputFrame* inserted;

					code = *cursor++;
					deltaCode = code & 0x7f;
					if (deltaCode == 0x7f) {
						timestamp = (int)FlightNet_PeekPacketDword(cursor);
						cursor += 4;
					} else if (deltaCode == 0x7e) {
						timestamp = packetTick - (int)FlightNet_ReadPacketWord(cursor);
						cursor += 2;
					} else if (deltaCode == 0x7d) {
						timestamp = packetTick - 0x7d;
						timestamp -= (int)*cursor;
						++cursor;
					} else {
						timestamp = packetTick - (int)deltaCode;
					}

#ifdef XWA_MODERN
					memset(&input, 0, sizeof(input));
#endif
					if ((code & 0x80) != 0) {
						input.key = *cursor++;
					} else {
						input.key = 0;
					}

					input.axisX = (int8_t)(cursor[0] & 0xfe);
					input.axisY = (int8_t)(cursor[1] & 0xfe);
					input.axisR = (int8_t)(cursor[2] & 0xfe);
					input.keyMods = (uint8_t)(cursor[1] & 1);
					input.keyMods <<= 1;
					input.keyMods |= (uint8_t)(cursor[0] & 1);
					input.key += (uint16_t)((cursor[2] & 1) << 8);
					cursor += 3;

					inserted = FlightNet_InsertInputFrame(playerIdx, timestamp, &input);
					if (inserted != NULL) {
						inserted->valid = 0;
						inserted->applied = 0;
					}
					--frameCount;
				}
			}
		}
	}

	g_flightSimSideEffectsSuppressed = 0;
	Flight_StepSimToTime(packetTick);
	if (g_provingGroundsModeActive) {
		Yard_UpdateChallengeTick(dtMs);
	}

	{
		int playerIdx;

		for (playerIdx = 0; playerIdx < XWA_INPUT_HISTORY_PLAYER_COUNT; ++playerIdx) {
			if (g_players[playerIdx].connectedFlag && playerIdx != g_localPlayer) {
				FlightView_UpdatePlayerCamera(playerIdx);
			}
		}
	}
	FlightView_UpdatePlayerCamera(g_localPlayer);

	g_gameTime = packetTick;
	g_serverTickTime = packetTick;
#ifndef XWA_MODERN
	Sound_FlushQueuedEffects();
#endif
	Flight_SaveWorldState();

	if (checksumRequested) {
		Flight_ChecksumWorldState(0, 0);
		g_flightNetWorldChecksumEpoch = (uint32_t)g_serverTickTime;
		if (NetSession_GetLocalPlayerId() != 0) {
			FlightNet_BroadcastWorldChecksum((const int*)g_worldChecksum, (const int*)peerChecksum, 16);
		}
		FlightNet_SendWorldChecksumToLocalPlayer((const int*)g_worldChecksum, (const int*)peerChecksum, 16);

		g_flightNetBufferWorldMessagesUntilChecksum = 1;
		memcpy(g_worldStateDupBuffer, g_worldStateBuffer, (size_t)g_worldStateSize);
		worldStateSize = g_worldStateSize;
		g_worldMessageBufferBytesFree = g_worldMessageBufferCapacity;
		g_worldMessageBufferedCount = 0;
	}
}

// FUNCTION: XWA 0x4F91C0
void FlightNet_ReplayResyncMessages(unsigned int savedWorldStateSize, int serverTickTime) {
	int packetOffset;

	worldStateSize = (int)savedWorldStateSize;
	g_flightNetDirtyAllObjectTransformsAfterRestore = 1;
	memcpy(g_worldStateBuffer, g_worldStateDupBuffer, (size_t)savedWorldStateSize);
	g_worldStateSize = worldStateSize;
	g_serverTickTime = serverTickTime;

	Flight_ChecksumWorldState(0, 0);
	FlightNet_SendWorldChecksumToLocalPlayer((const int*)g_worldChecksum, (const int*)peerChecksum, 16);
	memcpy(g_worldStateDupBuffer, g_worldStateBuffer, (size_t)g_worldStateSize);
	worldStateSize = g_worldStateSize;
	g_flightNetBufferWorldMessagesUntilChecksum = 0;

	packetOffset = 0;
	while (g_worldMessageBufferedCount != 0) {
		uint8_t* packet;
		uint8_t* cursor;
		int connectedSections;
		uint32_t packetTick;

		--g_worldMessageBufferedCount;
		packet = g_worldMessageBuffer + packetOffset;
		packetOffset += 9;
		cursor = packet + 9;

		connectedSections = packet[8];
		while (connectedSections > 0) {
			int frameCount;

			frameCount = *cursor++;
			++packetOffset;
			while (frameCount > 0) {
				uint8_t code;

				code = *cursor++;
				++packetOffset;
				if ((code & 0x7f) == 0x7f) {
					cursor += 4;
					packetOffset += 4;
				}
				if ((code & 0x80) != 0) {
					++cursor;
					++packetOffset;
				}
				cursor += 3;
				packetOffset += 3;
				--frameCount;
			}
			--connectedSections;
		}

		packetTick = FlightNet_PeekPacketDword(packet + 4) & 0x7fffffffu;
		memcpy(packet + 4, &packetTick, sizeof(packetTick));
		FlightNet_ApplyWorldMessagePacket(packet);
	}

	g_worldMessageBufferBytesFree = g_worldMessageBufferCapacity;
}

static void FlightNet_MarkLocalAbortAndPilotLeft(void) {
	int localPlayer;

	localPlayer = g_localPlayer;
	g_playerAbortFlags[localPlayer] = 1;
	g_flightMissionEndPending = 1;
	g_players[localPlayer].connectedFlag = 0;
	FlightNet_MarkPilotNetworkPlayerLeft(localPlayer);
}

static void FlightNet_SendPlayerAbortPacket(int directPlayId, int playerIdx, int broadcast) {
	g_flightNetScratchPacket[0] = 21;
	g_flightNetScratchPacket[1] = (uint32_t)playerIdx;
	if (broadcast) {
		NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);
	} else {
		NetSession_SendPacket(directPlayId, g_flightNetScratchPacket, 8);
	}
}

static const uint8_t* FlightNet_DecodeRemoteInputFrame(int playerIdx, const uint8_t* cursor,
													   int* outTimestamp, FlightInputFrameRecord* input) {
	uint8_t code;
	uint8_t deltaCode;
	int timestamp;
	uint8_t highByte;
	uint8_t b0;
	uint8_t b1;
	uint8_t b2;

	memset(input, 0, sizeof(*input));

	code = *cursor++;
	deltaCode = code & 0x7f;
	if (deltaCode == 0x7f) {
		timestamp = (int)FlightNet_PeekPacketDword(cursor);
		cursor += 4;
	} else {
		int lastCode;

		lastCode = g_flightNetLastInputDeltaCodeByPlayer[playerIdx];
		if ((lastCode & 0x7f) > deltaCode) {
			lastCode += 128;
		}
		timestamp = (lastCode & 0xffffff80) | deltaCode;
	}

	if ((code & 0x80) != 0) {
		highByte = *cursor++;
	} else {
		highByte = 0;
	}

	g_flightNetLastInputDeltaCodeByPlayer[playerIdx] = timestamp;

	b0 = cursor[0];
	b1 = cursor[1];
	b2 = cursor[2];
	cursor += 3;

	input->key = (uint16_t)(highByte | ((uint16_t)(b2 & 1) << 8));
	input->axisX = (int8_t)(b0 & 0xfe);
	input->axisY = (int8_t)(b1 & 0xfe);
	input->axisR = (int8_t)(b2 & 0xfe);
	input->keyMods = (uint8_t)((b0 & 1) | ((b1 & 1) << 1));
	*outTimestamp = timestamp;
	return cursor;
}

static void FlightNet_InsertDecodedRemoteInput(int playerIdx, int timestamp,
											   const FlightInputFrameRecord* input) {
	InputFrame* inserted;

	inserted = FlightNet_InsertInputFrame(playerIdx, timestamp, input);
	if (inserted != NULL) {
		inserted->valid = 1;
		inserted->applied = NetSession_GetLocalPlayerId() != 0;
	}
}

static void FlightNet_HandleRemoteInputPacket(int senderDpid, const uint8_t* packet, int batched) {
	int playerIdx;

	playerIdx = NetSession_FindPlayerSlotByDpid(senderDpid);
	if (!g_players[playerIdx].connectedFlag) {
		FlightNet_SendPlayerAbortPacket(senderDpid, playerIdx, 0);
		return;
	}

	if (g_flightNetPeerSilenceTicks[playerIdx] > 0) {
		g_flightNetPeerSilenceTicks[playerIdx] = 0;
	}
	FlightNet_DiscardPredictedInputFrames(playerIdx);

	if (!batched) {
		FlightInputFrameRecord input;
		int timestamp;

		FlightNet_DecodeRemoteInputFrame(playerIdx, packet + 4, &timestamp, &input);
		FlightNet_InsertDecodedRemoteInput(playerIdx, timestamp, &input);
		return;
	}

	{
		int frameCount;
		const uint8_t* cursor;

		frameCount = packet[4];
		cursor = packet + 5;
		while (frameCount > 0) {
			FlightInputFrameRecord input;
			int timestamp;

			cursor = FlightNet_DecodeRemoteInputFrame(playerIdx, cursor, &timestamp, &input);
			FlightNet_InsertDecodedRemoteInput(playerIdx, timestamp, &input);
			--frameCount;
		}
	}
}

static void FlightNet_ResetPeerSilenceForPacket(int senderDpid) {
	if (senderDpid == NetSession_GetHostDplayId()) {
		g_flightNetHostTimeoutElapsedMs = 0;
	} else {
		int playerIdx;

		playerIdx = NetSession_FindPlayerSlotByDpid(senderDpid);
		if (g_players[playerIdx].connectedFlag && g_flightNetPeerSilenceTicks[playerIdx] > 0) {
			g_flightNetPeerSilenceTicks[playerIdx] = 0;
		}
	}
}

static void FlightNet_HandleAbortPacket(const int* packet) {
	unsigned int playerIdx;

	playerIdx = (unsigned int)packet[1];
	if (playerIdx < XWA_PLAYER_COUNT) {
		g_playerAbortFlags[playerIdx] = 1;
	}
	if (playerIdx == (unsigned int)g_localPlayer) {
		FlightNet_MarkLocalAbortAndPilotLeft();
	}
}

static void FlightNet_HandleClockProbePacket(int senderDpid, const int* packet) {
	int sample;
	int currentLead;

	g_flightNetScratchPacket[0] = 31;
	g_flightNetScratchPacket[1] = (uint32_t)packet[1];
	NetSession_SendPacket(senderDpid, g_flightNetScratchPacket, 8);

	sample = packet[2];
	if (!g_asyncFlag || g_activeFlightPlayerCount < g_flightNetSmallSessionPlayerThreshold) {
		sample >>= 1;
	}

	currentLead = g_flightNetClockLeadAllowanceMs;
	if (sample > currentLead) {
		int delta;

		delta = (sample - currentLead) >> 1;
		if (delta == 0) {
			delta = 1;
		}
		currentLead += delta;
	} else if (sample < currentLead) {
		int delta;

		delta = (currentLead - sample) >> 1;
		if (delta == 0) {
			delta = 1;
		}
		currentLead -= delta;
	}

	g_flightNetClockLeadAllowanceMs = currentLead;
	if (g_asyncFlag && g_flightNetClockLeadAllowanceMs < 130) {
		g_flightNetClockLeadAllowanceMs = 130;
	}
}

static void FlightNet_HandleClockProbeAckPacket(const int* packet) {
	if (NetSession_GetLocalPlayerId() == 0 && g_flightNetClockProbeTimestamp == packet[1]) {
		int sample;

		sample = g_inputTimestamp + g_flightNetClockAdjustAccumTicks + 20 - packet[1];
		if (sample < 472) {
			if (sample > g_flightNetClockLeadAllowanceMs) {
				int delta;

				delta = (sample - g_flightNetClockLeadAllowanceMs) >> 1;
				if (delta == 0) {
					delta = 1;
				}
				g_flightNetClockLeadAllowanceMs += delta;
			} else if (sample < g_flightNetClockLeadAllowanceMs) {
				int delta;

				delta = (g_flightNetClockLeadAllowanceMs - sample) >> 1;
				if (delta == 0) {
					delta = 1;
				}
				g_flightNetClockLeadAllowanceMs -= delta;
			}
		}
	}
}

static void FlightNet_PrepareResyncChunkAck(const int* packet) {
	const uint8_t* record;
	int dstOffset;

	record = (const uint8_t*)&packet[3];
	dstOffset = packet[3];
	while (dstOffset != -1) {
		int chunkSize;

		memcpy(&chunkSize, record + 4, sizeof(chunkSize));
		FlightNet_CopyWorldStateResyncChunk(record + 8, dstOffset, (uint32_t)chunkSize);
		record += 8 + chunkSize;
		memcpy(&dstOffset, record, sizeof(dstOffset));
	}

	g_flightNetScratchPacket[0] = 16;
	g_flightNetScratchPacket[1] = (uint32_t)packet[2];
}

static void FlightNet_HandleResyncChunkPacket(const int* packet) {
	FlightNet_PrepareResyncChunkAck(packet);
	NetSession_SendPacket(NetSession_GetHostDplayId(), g_flightNetScratchPacket, 8);
}

static int FlightNet_RunClientWorldStateResyncWait(int packetSize) {
	int countdownState;

	countdownState = -1;
	while (1) {
		int hostDpid;

		hostDpid = NetSession_GetHostDplayId();
		NetSession_SendPacket(hostDpid, g_flightNetScratchPacket, packetSize);

		if (FlightInput_HasKeyReady() && FlightInput_GetNextKey() == 27) {
			FlightNet_SendPlayerAbortPacket(0, g_localPlayer, 1);
			FlightNet_MarkLocalAbortAndPilotLeft();
			return 0;
		}

		while (1) {
			int senderDpid;
			int outAux;
			int* packet;
			int savedInputTimestamp;
			int timeoutBucket;

			savedInputTimestamp = g_inputTimestamp;
			g_inputTimestamp += (int)Time_GetFrameDelta();
			g_flightNetHostTimeoutElapsedMs += g_inputTimestamp - savedInputTimestamp;
			g_inputTimestamp = savedInputTimestamp;

			if (g_flightNetHostTimeoutElapsedMs > 7080) {
				FlightAlert_RestoreBoxBackground();
				return 0;
			}

			timeoutBucket = (7080 - g_flightNetHostTimeoutElapsedMs) / 118;
			if (timeoutBucket != countdownState) {
				countdownState = timeoutBucket;
				if (timeoutBucket < 50) {
					char line[80];

					sprintf(line, g_strDiskIoMessages[31], timeoutBucket / 2, 5 * (timeoutBucket & 1));
					FlightAlert_DrawBox(3, line, NULL, 0x30u);
				} else if ((timeoutBucket & 1) != 0) {
					FlightAlert_DrawBox(3, g_strDiskIoMessages[30], NULL, 0x30u);
				} else {
					FlightAlert_DrawBox(3, g_strDiskIoMessages[26], NULL, 0x30u);
				}
			}

			packet = NetSession_ReceiveGamePacket(&senderDpid, &outAux);
			if (packet == NULL) {
				continue;
			}

			switch (packet[0]) {
				case 1:
					FlightNet_HandleRemoteInputPacket(senderDpid, (const uint8_t*)packet, 0);
					continue;
				case 2: {
					int savedTimestamp;

					savedTimestamp = g_inputTimestamp;
					FlightNet_ApplyWorldMessagePacket((uint8_t*)packet);
					Time_GetFrameDelta();
					g_inputTimestamp = savedTimestamp;
					if (g_flightMissionEndPending) {
						FlightAlert_RestoreBoxBackground();
						return 0;
					}
					continue;
				}
				case 8:
					g_flightMissionEndPending = 1;
					g_flightNetHostAbortReceived = 1;
					g_players[g_localPlayer].connectedFlag = 0;
					FlightAlert_RestoreBoxBackground();
					return 0;
				case 21:
					FlightNet_HandleAbortPacket(packet);
					if ((unsigned int)packet[1] == (unsigned int)g_localPlayer) {
						FlightAlert_RestoreBoxBackground();
						return 0;
					}
					continue;
				case 22:
					FlightNet_HandleRemoteInputPacket(senderDpid, (const uint8_t*)packet, 1);
					continue;
				case 23:
					dpid = packet[1];
					DebugPrintfChannel(
						0x20000, "HandleWorldPacket Received resync notification for playerid %d.\n", dpid);
					continue;
				case 29:
					FlightNet_ResetPeerSilenceForPacket(senderDpid);
					continue;
				case 61:
					FlightNet_SendPlayerAbortPacket(0, g_localPlayer, 1);
					FlightNet_MarkLocalAbortAndPilotLeft();
					FlightAlert_RestoreBoxBackground();
					return 0;
				case 62:
				case 63:
					if (packet[1] == (int)g_flightNetWorldChecksumEpoch) {
						if (packet[0] == 63) {
							FlightNet_PrepareResyncChunkAck(packet);
							packetSize = 8;
							break;
						}

						if (!g_playerAbortFlags[g_localPlayer]) {
							Time_GetFrameDelta();
							FlightNet_ReplayResyncMessages((unsigned int)packet[2], packet[1]);
							g_flightNetScratchPacket[0] = 27;
							NetSession_SendPacket(NetSession_GetHostDplayId(), g_flightNetScratchPacket, 4);
							g_inputTimestamp = g_serverTickTime + g_flightNetClockLeadAllowanceMs;
							FlightAlert_RestoreBoxBackground();
							return 1;
						}

						FlightNet_MarkLocalAbortAndPilotLeft();
						FlightAlert_RestoreBoxBackground();
						return 0;
					}
					continue;
				default:
					continue;
			}

			break;
		}
	}
}

static int FlightNet_HandleWorldStateResyncPacket(int senderDpid, const int* packet) {
	(void)senderDpid;

	if (packet[1] != (int)g_flightNetWorldChecksumEpoch) {
		return 1;
	}

	if (packet[0] == 61) {
		int serializedWorldStateSize;
		int checksumBytes;

		g_inputTimestamp += (int)Time_GetFrameDelta();
		FlightAlert_SaveBoxBackground();
		FlightAlert_DrawBox(1, g_strDiskIoMessages[22], NULL, 0x30u);
		Flight_ApplyWorldStateObjectPresenceMap((const uint8_t*)packet + 8);

		g_flightNetScratchPacket[0] = 60;
		serializedWorldStateSize = Flight_GetSerializedWorldStateSize();
		checksumBytes = 4 * Flight_BuildWorldStateResyncSegmentChecksums(
								(int*)(g_flightNetScratchPacket + 1), Flight_GetDuplicateWorldStateBuffer(),
								serializedWorldStateSize);
		return FlightNet_RunClientWorldStateResyncWait(checksumBytes + 4);
	}

	if (packet[0] == 63) {
		FlightNet_HandleResyncChunkPacket(packet);
		return 1;
	}

	if (packet[0] == 62) {
		if (!g_playerAbortFlags[g_localPlayer]) {
			Time_GetFrameDelta();
			FlightNet_ReplayResyncMessages((unsigned int)packet[2], packet[1]);
			g_flightNetScratchPacket[0] = 27;
			NetSession_SendPacket(NetSession_GetHostDplayId(), g_flightNetScratchPacket, 4);
			g_inputTimestamp = g_serverTickTime + g_flightNetClockLeadAllowanceMs;
			FlightAlert_RestoreBoxBackground();
		} else {
			FlightNet_MarkLocalAbortAndPilotLeft();
		}
	}
	return 1;
}

static int FlightNet_WriteWorldMessageFrame(uint8_t** cursor, InputFrame* frame, int packetTick) {
	uint8_t* out;
	int delta;
	uint8_t code;

	out = *cursor;
	delta = packetTick - frame->timestamp;
	if (delta < 0x10000) {
		if (delta < 368) {
			code = (uint8_t)(delta >= 125 ? 125 : delta);
		} else {
			code = 126;
		}
	} else {
		code = 127;
	}
	if (frame->input.key != 0) {
		code |= 0x80u;
	}
	*out++ = code;

	switch (code & 0x7f) {
		case 127:
			memcpy(out, &frame->timestamp, sizeof(frame->timestamp));
			out += 4;
			break;
		case 126: {
			uint16_t wordDelta;

			wordDelta = (uint16_t)(packetTick - (uint16_t)frame->timestamp);
			memcpy(out, &wordDelta, sizeof(wordDelta));
			out += 2;
			break;
		}
		case 125:
			*out++ = (uint8_t)(packetTick - (uint8_t)frame->timestamp - 125);
			break;
		default:
			break;
	}

	if ((code & 0x80u) != 0) {
		*out++ = (uint8_t)frame->input.key;
	}

	out[0] = (uint8_t)frame->input.axisX & 0xfeu;
	out[1] = (uint8_t)frame->input.axisY & 0xfeu;
	out[2] = (uint8_t)frame->input.axisR & 0xfeu;
	out[0] |= (uint8_t)(frame->input.keyMods & 1u);
	out[1] |= (uint8_t)((frame->input.keyMods >> 1) & 1u);
	out[2] |= (uint8_t)((frame->input.key >> 8) & 1u);
	out += 3;

	frame->applied = 0;
	delta = (int)(out - *cursor);
	*cursor = out;
	return delta;
}

static void FlightNet_MaybeSendWorldMessage(int currentTimestamp) {
	int adjustedTimestamp;
	int nextSendTimestamp;
	int elapsedSinceSend;
	int earliestAppliedFrame;
	int playerIdx;

	if (g_flightNetPendingAckCount || g_flightNetRecoveryUiActive) {
		return;
	}

	adjustedTimestamp = g_inputTimestamp + g_flightNetClockAdjustAccumTicks;
	nextSendTimestamp = g_flightNetNextClientInputSendTimestamp;
	if (nextSendTimestamp == 0) {
		nextSendTimestamp = adjustedTimestamp + (g_flightNetClockLeadAllowanceMs >> 3);
		g_flightNetNextClientInputSendTimestamp = nextSendTimestamp;
	}

	elapsedSinceSend = adjustedTimestamp - nextSendTimestamp;
	if (elapsedSinceSend < dtMs) {
		return;
	}

	if (elapsedSinceSend > 5 * dtMs) {
		g_flightNetNextClientInputSendTimestamp = nextSendTimestamp + dtMs;
	} else {
		earliestAppliedFrame = 0x7fffffff;
		for (playerIdx = 0; playerIdx < XWA_INPUT_HISTORY_PLAYER_COUNT; ++playerIdx) {
			if (g_players[playerIdx].connectedFlag) {
				InputFrame* lastFrame;

				lastFrame = FlightNet_FindLastNonzeroInputFrame(playerIdx);
				if (lastFrame == NULL) {
					earliestAppliedFrame = 0;
					break;
				}
				if (lastFrame->timestamp < earliestAppliedFrame) {
					earliestAppliedFrame = lastFrame->timestamp;
				}
			}
		}
		if (g_flightNetLastSentWorldMessageTimestamp + dtMs >= earliestAppliedFrame) {
			return;
		}
		g_flightNetNextClientInputSendTimestamp += dtMs;
	}

	++g_flightNetSentWorldMessageCount;
	if (g_flightPlayerCount > 1) {
		uint8_t* cursor;
		int packetLen;
		int packetTick;
		int* frameCountPtr;
		InputFrame* frameCursor;

		packetTick = g_flightNetLastSentWorldMessageTimestamp + dtMs;
		g_flightNetScratchPacket[0] = 2;
		g_flightNetScratchPacket[1] = (uint32_t)packetTick;
		g_flightNetLastSentWorldMessageTimestamp = packetTick;
		g_flightNetWorldChecksumResetAccumMs += packetTick - g_serverTickTime;
		if (g_flightNetWorldChecksumResetAccumMs > 472) {
			g_flightNetScratchPacket[1] = (uint32_t)packetTick | 0x80000000u;
			g_flightNetWorldChecksumResetAccumMs = 0;
			memset(g_flightNetWorldChecksumPeerStatus, 0, sizeof(g_flightNetWorldChecksumPeerStatus));
		}

		cursor = (uint8_t*)g_flightNetScratchPacket + 9;
		packetLen = 9;
		((uint8_t*)g_flightNetScratchPacket)[8] = 0;
		frameCursor = &g_inputHistory[0][0];
		for (playerIdx = 0; playerIdx < XWA_INPUT_HISTORY_PLAYER_COUNT; ++playerIdx) {
			if (g_players[playerIdx].connectedFlag) {
				int frameIdx;
				int count;

				++((uint8_t*)g_flightNetScratchPacket)[8];
				frameCountPtr = (int*)cursor;
				*cursor++ = 0;
				++packetLen;
				count = g_inputFrameCount[playerIdx];
				for (frameIdx = 0; frameIdx < count; ++frameIdx) {
					InputFrame* frame;

					frame = &g_inputHistory[playerIdx][frameIdx];
					if (frame->applied && frame->timestamp <= packetTick) {
						if ((int)(cursor - (uint8_t*)g_flightNetScratchPacket) >
							504 - g_activeFlightPlayerCount) {
							break;
						}
						++*(uint8_t*)frameCountPtr;
						packetLen += FlightNet_WriteWorldMessageFrame(&cursor, frame, packetTick);
					}
				}
			}
			frameCursor += XWA_INPUT_HISTORY_FRAME_COUNT;
			(void)frameCursor;
		}

		NetSession_SendPacket(0, g_flightNetScratchPacket, packetLen);
		if (g_inputLogEnabled == 1) {
			FILE* logFile;

			logFile = g_flightNetServerLogFile;
			if (logFile == NULL) {
				logFile = fopen("serverlog.txt", "w");
				g_flightNetServerLogFile = logFile;
			}
			if (logFile != NULL) {
				fprintf(logFile, "%8x\n", g_flightNetScratchPacket[1]);
				fflush(logFile);
			}
		}
	}

	(void)currentTimestamp;
}

// FUNCTION: XWA 0x4EBA20
int FlightNet_WaitForMissionStart(void) {
	int stillLoadingAltText;
	int activePlayerCount;
	int senderDpid;
	int recvAux;
	uint32_t lastStillLoadingUiTime;
	char line1[256];

	stillLoadingAltText = 1;
	memset(g_flightNetLastInputDeltaCodeByPlayer, 0, sizeof(g_flightNetLastInputDeltaCodeByPlayer));
	memset(g_flightNetPeerSilenceTicks, 0, sizeof(g_flightNetPeerSilenceTicks));
	lastStillLoadingUiTime = 0;
	g_lastFrameTime = 0;
	g_lastKeyframeTime = 0;
	dpid = 0;
	g_flightNetLastInputBatchSendTime = 0;
	g_flightNetInputDeltaBatchLen = 5;
	g_flightNetInputDeltaBatchPacket[0] = 22;
	g_flightNetInputDeltaBatchPacket[1] = 0;
	g_flightNetInputBatchIntervalTicks = 23;
	g_flightNetRecoveryUiActive = 0;
	g_flightNetPendingAckCount = 0;
	g_flightNetClockAdjustAccumTicks = 0;
	g_flightNetHostTimeoutElapsedMs = 0;

	if (g_activeFlightPlayerCount == 1) {
		g_serverTickTime = 0;
		g_flightNetClockLeadAllowanceMs = 30;
		g_gameTime = 0;
		g_inputTimestamp = 30;
		Time_GetFrameDelta();
		return 1;
	}

	activePlayerCount = NetSession_CountActivePlayers();
	{
		int hostDpid;

		hostDpid = NetSession_GetHostDplayId();
		g_flightNetScratchPacket[0] = 20;
		NetSession_SendPacket(hostDpid, g_flightNetScratchPacket, 4);
	}

	if (NetSession_GetLocalPlayerId() != 0) {
		int readyPacketCount;

		readyPacketCount = 0;
		if (activePlayerCount > 0) {
			while (1) {
				int* readyPacket;

				readyPacket = NetSession_WaitForGamePacket(&senderDpid, &recvAux, 60);
				if (!readyPacket) {
					return 0;
				}
				if (readyPacket[0] == 20) {
					++readyPacketCount;
					if (readyPacketCount >= activePlayerCount) {
						break;
					}
				}
			}
		}

		g_flightNetScratchPacket[0] = 19;
		NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 4);
	}

	FlightAlert_SaveBoxBackground();
	FlightAlert_DrawBox(1, g_strDiskIoMessages[32], NULL, 0x30u);
	while (1) {
		int* packet;

		packet = NetSession_WaitForGamePacket(&senderDpid, &recvAux, 60);
		if (!packet) {
			return 0;
		}

		if (packet[0] == 29) {
			uint32_t nowMs;

			nowMs = timeGetTime();
			if ((int)(nowMs - lastStillLoadingUiTime) > 200) {
				char* senderName;

				lastStillLoadingUiTime = nowMs;
				senderName = NetSession_GetPlayerName(NetSession_FindPlayerSlotByDpid(senderDpid));
				if (!senderName) {
					strcpy(line1, g_strDiskIoMessages[33]);
				} else {
					strcpy(line1, senderName);
					stillLoadingAltText = !stillLoadingAltText;
					strcat(line1, stillLoadingAltText ? g_strDiskIoMessages[34] : g_strDiskIoMessages[35]);
				}
				FlightAlert_DrawBox(3, line1, NULL, 0x30u);
			}
		}

		if (packet[0] == 19) {
			FlightAlert_RestoreBoxBackground();
			{
				int hostDpid;

				hostDpid = NetSession_GetHostDplayId();
				g_flightNetScratchPacket[0] = 27;
				NetSession_SendPacket(hostDpid, g_flightNetScratchPacket, 4);
			}
			Time_GetFrameDelta();
			g_serverTickTime = 0;
			g_gameTime = 0;
			g_inputTimestamp = 0;
			g_flightNetClockLeadAllowanceMs = g_asyncFlag != 0 ? 130 : 30;
			switch (g_gameConfig.serverUpdateRate) {
				case 4:
					dtMs = 59;
					break;
				case 6:
					dtMs = 39;
					break;
				case 8:
					dtMs = 29;
					break;
				default:
					dtMs = 29;
					break;
			}

			if (NetSession_GetLocalPlayerId() != 0) {
				int inputLead;

				g_flightNetPendingAckCount = (activePlayerCount != 1) + 1;
				g_flightNetNextClientInputSendTimestamp = 0;
				g_flightNetLastSentWorldMessageTimestamp = 0;
				g_unusedFlightNetMissionStartAckInitFlag = 1;
				while (g_flightNetPendingAckCount != 0 && (unsigned int)g_inputTimestamp < 100u) {
					FlightNet_ProcessIncomingPackets();
					g_inputTimestamp += (int)Time_GetFrameDelta();
				}
				g_flightNetPendingAckCount = 0;
				inputLead = g_inputTimestamp + (int)Time_GetFrameDelta();
				g_inputTimestamp = inputLead;
				g_flightNetClockLeadAllowanceMs = inputLead;
				if (inputLead < 35) {
					int adjustment;

					adjustment = 35 - inputLead;
					g_flightNetClockLeadAllowanceMs = 35;
					g_inputTimestamp += adjustment;
					g_flightNetClockAdjustAccumTicks -= adjustment;
				}
			}
			return 1;
		}
	}
}

// FUNCTION: XWA 0x4EBEE0
void FlightNet_ProcessIncomingPackets(void) {
	int startTimestamp;
	int currentTimestamp;
	int miscElapsed;
	int inputElapsed;
	int worldFrameElapsed;
	int worldMessageCount;

	if (!g_players[g_localPlayer].connectedFlag) {
		return;
	}

	inputElapsed = 0;
	miscElapsed = 0;
	worldFrameElapsed = 0;
	worldMessageCount = 0;

	if (g_flightPlayerCount == 1) {
		int senderDpid;
		int outAux;

		while (NetSession_ReceiveGamePacket(&senderDpid, &outAux) != NULL) {
		}
		return;
	}

	if (g_flightNetRecoveryUiActive) {
		currentTimestamp = g_flightNetRecoveryUiBlinkTime + 1 + (int)Time_GetFrameDelta();
		startTimestamp = g_flightNetRecoverySavedInputTimestamp - 826;
	} else {
		currentTimestamp = g_inputTimestamp + (int)Time_GetFrameDelta();
		startTimestamp = currentTimestamp;
	}

	while (1) {
		int* packet;
		int senderDpid;
		int outAux;

		if (currentTimestamp - startTimestamp > 826) {
			if (g_flightNetRecoveryUiActive) {
				if (FlightInput_HasKeyReady() && FlightInput_GetNextKey() == 27) {
					g_flightNetRecoveryUiActive = 0;
					FlightAlert_RestoreBoxBackground();
					g_serverTickTime = g_gameTime;
					g_inputTimestamp = g_flightNetRecoverySavedInputTimestamp;
					g_flightMissionEndPending = 1;
					g_players[g_localPlayer].connectedFlag = 0;
					FlightNet_SendPlayerAbortPacket(0, g_localPlayer, 1);
					if (NetSession_GetLocalPlayerId() != 0) {
						FlightNet_BroadcastLocalPlayerLeft();
					} else {
						FlightNet_MarkPilotNetworkPlayerLeft(g_localPlayer);
					}
					return;
				}
				if (currentTimestamp - g_flightNetRecoveryUiBlinkTime > 118) {
					int oldToggle;

					oldToggle = miscElapsed;
					g_flightNetRecoveryUiBlinkTime = currentTimestamp;
					miscElapsed = !miscElapsed;
					if (oldToggle) {
						FlightAlert_DrawBox(3, g_strDiskIoMessages[26], NULL, 0x34u);
					} else {
						FlightAlert_DrawBox(3, g_strDiskIoMessages[30], NULL, 0x34u);
					}
				}
			} else {
				int hostDpid;
				int playerSlot;
				char* playerName;
				char line1[80];

				miscElapsed = 1;
				FlightAlert_SaveBoxBackground();
				strcpy(line1, g_strDiskIoMessages[24]);
				hostDpid = dpid != 0 ? dpid : NetSession_GetHostDplayId();
				dpid = hostDpid;
				playerSlot = NetSession_FindPlayerSlotByDpid(hostDpid);
				if (g_playerAbortFlags[playerSlot] || !g_players[playerSlot].connectedFlag) {
					dpid = NetSession_GetHostDplayId();
					playerSlot = NetSession_FindPlayerSlotByDpid(NetSession_GetHostDplayId());
				}
				playerName = NetSession_GetPlayerName(playerSlot);
				if (playerName != NULL) {
					strcat(line1, playerName);
				}
				FlightAlert_DrawBox(1, line1, NULL, 0x34u);
				g_flightNetRecoveryUiActive = 1;
				g_flightNetRecoveryUiBlinkTime = currentTimestamp;
				g_flightNetRecoverySavedInputTimestamp = currentTimestamp;
				if (NetSession_GetLocalPlayerId() == 0) {
					NetReliable_CompactLocalReceiveQueue();
					g_flightNetScratchPacket[0] = 3;
					g_flightNetScratchPacket[1] = (uint32_t)g_localPlayer;
					NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);
					g_playerConnected[g_localPlayer] = 0;
				}
			}
		}

		currentTimestamp += (int)Time_GetFrameDelta();
		packet = NetSession_ReceiveGamePacket(&senderDpid, &outAux);
		{
			int frameDelta;

			frameDelta = (int)Time_GetFrameDelta();
			currentTimestamp += frameDelta;
			inputElapsed += frameDelta;
		}

		if (packet == NULL) {
			break;
		}

		switch (packet[0]) {
			case 1:
				FlightNet_HandleRemoteInputPacket(senderDpid, (const uint8_t*)packet, 0);
				worldFrameElapsed += (int)Time_GetFrameDelta();
				continue;
			case 2:
				g_flightNetHostTimeoutElapsedMs = 0;
				++g_flightNetReceivedWorldMessageCount;
				FlightNet_ApplyWorldMessagePacket((uint8_t*)packet);
				if (NetSession_GetLocalPlayerId() != 0) {
					int playerIdx;

					for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
						if (playerIdx != g_localPlayer && g_players[playerIdx].connectedFlag &&
							g_flightNetPeerSilenceTicks[playerIdx] != -1) {
							g_flightNetPeerSilenceTicks[playerIdx] += dtMs;
							if (g_flightNetPeerSilenceTicks[playerIdx] > 7080) {
								FlightNet_SendPlayerAbortPacket(0, playerIdx, 1);
								g_flightNetPeerSilenceTicks[playerIdx] = 0;
							}
						}
					}
				}
				if (g_flightMissionEndPending) {
					if (!g_flightNetRecoveryUiActive) {
						return;
					}
					goto restore_recovery_ui;
				}
				worldMessageCount++;
				worldFrameElapsed += (int)Time_GetFrameDelta();
				continue;
			case 3:
				if ((unsigned int)packet[1] < XWA_PLAYER_COUNT) {
					g_playerConnected[packet[1]] = 0;
				}
				continue;
			case 4:
				if (g_players[NetSession_FindPlayerSlotByDpid(senderDpid)].connectedFlag) {
					FlightNet_HandleWorldChecksumPacket(senderDpid, packet);
				}
				continue;
			case 8:
				g_flightMissionEndPending = 1;
				g_flightNetHostAbortReceived = 1;
				g_players[g_localPlayer].connectedFlag = 0;
				goto maybe_return_or_restore;
			case 16:
				g_flightNetWorldStateAckReceivedFlag = 1;
				if ((unsigned int)packet[1] < 16u) {
					g_flightNetWorldStateChunkAcked[packet[1]] = 1;
				}
				continue;
			case 21:
				FlightNet_HandleAbortPacket(packet);
				if ((unsigned int)packet[1] == (unsigned int)g_localPlayer) {
					goto maybe_return_or_restore;
				}
				continue;
			case 22:
				FlightNet_HandleRemoteInputPacket(senderDpid, (const uint8_t*)packet, 1);
				continue;
			case 23:
				dpid = packet[1];
				DebugPrintfChannel(
					0x20000, "ProcessIncomingNetworkPackets Received resync notification for playerid %d.\n",
					dpid);
				continue;
			case 24:
				FlightNet_HandleServerChecksumPacket((const uint32_t*)packet);
				g_flightNetClockProbeTimestamp = g_inputTimestamp + g_flightNetClockAdjustAccumTicks;
				g_flightNetScratchPacket[0] = 30;
				g_flightNetScratchPacket[1] = (uint32_t)g_flightNetClockProbeTimestamp;
				g_flightNetScratchPacket[2] = (uint32_t)g_flightNetClockLeadAllowanceMs;
				NetSession_SendPacket(NetSession_GetHostDplayId(), g_flightNetScratchPacket, 12);
				continue;
			case 27:
				if (g_flightNetPendingAckCount != 0 && --g_flightNetPendingAckCount == 0) {
					g_flightNetNextClientInputSendTimestamp = 0;
					goto maybe_return_or_restore;
				}
				continue;
			case 28:
				g_flightNetClockLeadAllowanceMs = packet[1];
				continue;
			case 29:
				FlightNet_ResetPeerSilenceForPacket(senderDpid);
				continue;
			case 30:
				FlightNet_HandleClockProbePacket(senderDpid, packet);
				continue;
			case 31:
				FlightNet_HandleClockProbeAckPacket(packet);
				continue;
			case 60:
				g_flightNetRemoteResyncChecksumsReceivedFlag = 1;
				memcpy(g_flightNetRemoteResyncChecksums, packet + 1,
					   sizeof(g_flightNetRemoteResyncChecksums));
				continue;
			case 61:
			case 62:
			case 63:
				if (!FlightNet_HandleWorldStateResyncPacket(senderDpid, packet)) {
					goto maybe_return_or_restore;
				}
				if (g_flightMissionEndPending || !g_players[g_localPlayer].connectedFlag) {
					goto maybe_return_or_restore;
				}
				continue;
			default:
				continue;
		}
	}

	if (NetSession_GetLocalPlayerId() != 0) {
		FlightNet_MaybeSendWorldMessage(currentTimestamp);
	}

	if (g_flightNetRecoveryUiActive) {
	restore_recovery_ui:
		g_flightNetRecoveryUiActive = 0;
		FlightAlert_RestoreBoxBackground();
		g_inputTimestamp = g_flightNetRecoverySavedInputTimestamp;
		return;
	}

	g_inputTimestamp = currentTimestamp + (int)Time_GetFrameDelta();
	{
		char line1[80];

		sprintf(line1, "RcvMsg:%-2d In2Svr:%-2d SvrSnd:%-2d SvrFrm:%-2d NumFrm:%-2d AllPIN:%-2d Misc:%-2d\n",
				inputElapsed, worldFrameElapsed, 0, worldFrameElapsed, worldMessageCount,
				g_inputTimestamp - startTimestamp,
				g_inputTimestamp - inputElapsed - startTimestamp - worldFrameElapsed);
	}
	return;

maybe_return_or_restore:
	if (!g_flightNetRecoveryUiActive) {
		return;
	}
	goto restore_recovery_ui;
}

// FUNCTION: XWA 0x4EDC70
int FlightNet_SendWorldStateResyncApplyRequest(int directPlayId, int worldStateSize) {
	int retryCount;
	int stillLoadingElapsed;

	g_flightNetScratchPacket[1] = g_flightNetWorldChecksumEpoch;
	g_flightNetScratchPacket[0] = 62;
	g_flightNetScratchPacket[2] = (uint32_t)worldStateSize;
	g_flightNetScratchPacket[3] = (uint32_t)g_inputTimestamp;
	NetSession_SendPacket(directPlayId, g_flightNetScratchPacket, 16);
	Time_GetFrameDelta();

	retryCount = 10;
	stillLoadingElapsed = 0;
	do {
		int savedInputTimestamp;

		savedInputTimestamp = g_inputTimestamp;
		g_flightNetPendingAckCount = 1;
		while (g_flightNetPendingAckCount != 0 &&
			   (unsigned int)(g_inputTimestamp - savedInputTimestamp) < 236u) {
			if (FlightInput_HasKeyReady()) {
				uint16_t key;

				key = (uint16_t)FlightInput_GetNextKey();
				if (key == 27) {
					retryCount = 1;
					g_inputTimestamp = 236 + g_inputTimestamp;
					break;
				}
			}
			FlightNet_ProcessIncomingPackets();
			g_inputTimestamp = Time_GetFrameDelta() + g_inputTimestamp;
			if (g_flightNetWorldStateAckReceivedFlag) {
				g_flightNetWorldStateAckReceivedFlag = 0;
				g_inputTimestamp = savedInputTimestamp;
			}
		}

		stillLoadingElapsed += g_inputTimestamp - savedInputTimestamp;
		if (stillLoadingElapsed >= 236) {
			stillLoadingElapsed = 0;
			g_flightNetScratchPacket[0] = 29;
			NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 4);
		}
		if (!g_flightNetPendingAckCount) {
			break;
		}
		--retryCount;
	} while (retryCount != 0);

	if (g_flightNetPendingAckCount == 1) {
		int playerSlot;

		playerSlot = NetSession_FindPlayerSlotByDpid(directPlayId);
		g_flightNetScratchPacket[0] = 21;
		g_flightNetScratchPacket[1] = (uint32_t)playerSlot;
		NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);
		g_inputTimestamp = Time_GetFrameDelta() + g_inputTimestamp;
		g_flightNetPendingAckCount = 0;
		g_inputTimestamp = g_serverTickTime + g_flightNetClockLeadAllowanceMs;
	} else {
		g_inputTimestamp = Time_GetFrameDelta() + g_inputTimestamp;
		g_inputTimestamp = g_serverTickTime + g_flightNetClockLeadAllowanceMs;
	}

	g_flightNetScratchPacket[0] = 23;
	g_flightNetScratchPacket[1] = 0;
	return NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);
}

// FUNCTION: XWA 0x4EDE30
int FlightNet_SendWorldStateResyncToPlayer(int directPlayId, uint8_t* worldState, int worldStateSize) {
	int alertToggle;
	int stillLoadingElapsed;
	int retryCount;
	char line1[256];
	int chunkSlot;
	int segmentIndex;
	int worldOffset;
	int packetFreeBytes;
	int segmentCount;
	int segmentSize;
	uint8_t* payload;
	int result;

	alertToggle = 0;
	stillLoadingElapsed = 0;
	g_inputTimestamp += (int)Time_GetFrameDelta();
	FlightAlert_SaveBoxBackground();

	strcpy(line1, g_strDiskIoMessages[23]);
	{
		char* playerName;

		playerName = NetSession_GetPlayerName(NetSession_FindPlayerSlotByDpid(directPlayId));
		if (playerName != NULL) {
			strcat(line1, playerName);
		}
	}
	FlightAlert_DrawBox(1, line1, NULL, 0x30u);

	g_flightNetScratchPacket[0] = 23;
	g_flightNetScratchPacket[1] = (uint32_t)directPlayId;
	NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);

	g_flightNetScratchPacket[0] = 61;
	g_flightNetScratchPacket[1] = g_flightNetWorldChecksumEpoch;
	result = Flight_BuildWorldStateObjectPresenceMap((uint8_t*)g_flightNetScratchPacket + 8, worldState);
	NetSession_SendPacket(directPlayId, g_flightNetScratchPacket, result + 8);

	g_flightNetPendingAckCount = 1;
	retryCount = 10;
	do {
		int elapsedThisPass;

		g_flightNetRemoteResyncChecksumsReceivedFlag = 0;
		elapsedThisPass = 0;
		while (elapsedThisPass < 236) {
			int savedInputTimestamp;
			int delta;

			if (FlightInput_HasKeyReady() && FlightInput_GetNextKey() == 27) {
				elapsedThisPass = 236;
				retryCount = 1;
				break;
			}

			savedInputTimestamp = g_inputTimestamp;
			FlightNet_ProcessIncomingPackets();
			g_inputTimestamp += (int)Time_GetFrameDelta();
			delta = g_inputTimestamp - savedInputTimestamp;
			g_inputTimestamp = savedInputTimestamp;
			elapsedThisPass += delta;
			if (g_flightNetRemoteResyncChecksumsReceivedFlag) {
				break;
			}
		}

		stillLoadingElapsed += elapsedThisPass;
		if (stillLoadingElapsed >= 236) {
			stillLoadingElapsed = 0;
			g_flightNetScratchPacket[0] = 29;
			NetSession_SendPacket(directPlayId, g_flightNetScratchPacket, 4);
			NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 4);
			alertToggle = !alertToggle;
			if (alertToggle) {
				FlightAlert_DrawBox(3, g_strDiskIoMessages[29], NULL, 0x30u);
			} else {
				FlightAlert_DrawBox(3, g_strDiskIoMessages[26], NULL, 0x30u);
			}
		}
		if (g_flightNetRemoteResyncChecksumsReceivedFlag) {
			break;
		}
		--retryCount;
	} while (retryCount != 0 && !g_flightNetRemoteResyncChecksumsReceivedFlag);

	if (!g_flightNetRemoteResyncChecksumsReceivedFlag) {
		g_flightNetScratchPacket[0] = 21;
		g_flightNetScratchPacket[1] = (uint32_t)NetSession_FindPlayerSlotByDpid(directPlayId);
		NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);
		FlightAlert_RestoreBoxBackground();
		Time_GetFrameDelta();
		g_flightNetPendingAckCount = 0;
		return 0;
	}

	segmentCount = Flight_BuildWorldStateResyncSegmentChecksums(g_flightNetLocalResyncChecksums, worldState,
																worldStateSize);
	memset(g_flightNetWorldStateChunkAcked, 0, sizeof(g_flightNetWorldStateChunkAcked));

	chunkSlot = 0;
	worldOffset = 0;
	segmentSize = Flight_ComputeWorldStateResyncSegmentSize(worldStateSize);
	packetFreeBytes = 492;
	g_flightNetWorldStateChunkPackets[0].packetType = 63;
	g_flightNetWorldStateChunkPackets[0].baseChecksum = g_flightNetWorldChecksumEpoch;
	g_flightNetWorldStateChunkPackets[0].chunkIndex = 0;
	payload = g_flightNetWorldStateChunkPackets[0].payload;

	for (segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
		if (g_flightNetLocalResyncChecksums[segmentIndex] == g_flightNetRemoteResyncChecksums[segmentIndex]) {
			worldOffset += segmentSize;
			continue;
		}

		{
			int remainingSegmentBytes;

			remainingSegmentBytes = segmentSize;
			if (worldOffset + remainingSegmentBytes > worldStateSize) {
				remainingSegmentBytes = worldStateSize - worldOffset;
				if (remainingSegmentBytes < 0) {
					remainingSegmentBytes = 0;
				}
			}

			while (remainingSegmentBytes != 0) {
				int recordBytes;
				int dataBytes;

				recordBytes = remainingSegmentBytes + 8;
				if (recordBytes > packetFreeBytes) {
					recordBytes = packetFreeBytes;
				}
				dataBytes = recordBytes - 8;
				memcpy(payload, &worldOffset, sizeof(worldOffset));
				memcpy(payload + 4, &dataBytes, sizeof(dataBytes));
				memcpy(payload + 8, &worldState[worldOffset], (size_t)dataBytes);
				payload += recordBytes;
				packetFreeBytes -= recordBytes;
				worldOffset += dataBytes;
				remainingSegmentBytes -= dataBytes;

				if (packetFreeBytes < 32) {
					int terminator;

					terminator = -1;
					memcpy(payload, &terminator, sizeof(terminator));
					NetSession_SendPacket(directPlayId,
										  (uint32_t*)&g_flightNetWorldStateChunkPackets[chunkSlot],
										  508 - packetFreeBytes);
					++chunkSlot;
					if (chunkSlot == 16) {
						if (!FlightNet_WaitForWorldStateChunkAcks(directPlayId, 16)) {
							FlightAlert_RestoreBoxBackground();
							Time_GetFrameDelta();
							g_flightNetPendingAckCount = 0;
							return 0;
						}
						g_flightNetScratchPacket[0] = 29;
						NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 4);
						if (alertToggle) {
							FlightAlert_DrawBox(3, g_strDiskIoMessages[26], NULL, 0x30u);
						} else {
							FlightAlert_DrawBox(3, g_strDiskIoMessages[29], NULL, 0x30u);
						}
						alertToggle = !alertToggle;
						chunkSlot = 0;
						memset(g_flightNetWorldStateChunkAcked, 0, sizeof(g_flightNetWorldStateChunkAcked));
					}

					packetFreeBytes = 492;
					g_flightNetWorldStateChunkPackets[chunkSlot].packetType = 63;
					g_flightNetWorldStateChunkPackets[chunkSlot].baseChecksum = g_flightNetWorldChecksumEpoch;
					g_flightNetWorldStateChunkPackets[chunkSlot].chunkIndex = (uint32_t)chunkSlot;
					payload = g_flightNetWorldStateChunkPackets[chunkSlot].payload;
				}
			}
		}
	}

	result = 1;
	if (packetFreeBytes < 496) {
		int terminator;

		terminator = -1;
		memcpy(payload, &terminator, sizeof(terminator));
		NetSession_SendPacket(directPlayId, (uint32_t*)&g_flightNetWorldStateChunkPackets[chunkSlot],
							  508 - packetFreeBytes);
		if (!FlightNet_WaitForWorldStateChunkAcks(directPlayId, chunkSlot + 1)) {
			result = 0;
		}
	}

	FlightAlert_RestoreBoxBackground();
	Time_GetFrameDelta();
	g_flightNetPendingAckCount = 0;
	return result;
}

// FUNCTION: XWA 0x4F8F90
void FlightNet_HandleWorldChecksumPacket(int senderDpid, const int* packet) {
	int playerSlot;
	int mismatch;

	if (packet[1] != (int)g_flightNetWorldChecksumEpoch) {
		return;
	}

	playerSlot = NetSession_FindPlayerSlotByDpid(senderDpid);
	if (g_playerAbortFlags[playerSlot]) {
		return;
	}
	if (!g_players[playerSlot].connectedFlag) {
		return;
	}

	mismatch = 0;
	if (playerSlot != g_localPlayer) {
		int checksumIndex;
		int packetPeerChecksumSum;
		int localPeerChecksumSum;

		packetPeerChecksumSum = 0;
		localPeerChecksumSum = 0;
		for (checksumIndex = 0; checksumIndex < 16; ++checksumIndex) {
			int packetChecksum;

			packetPeerChecksumSum += packet[18 + checksumIndex];
			localPeerChecksumSum += (int)peerChecksum[checksumIndex];
			packetChecksum = packet[2 + checksumIndex];
			if (packetChecksum != (int)g_worldChecksum[checksumIndex]) {
				DebugPrintfChannel(
					0x20000, "Player %d checksum[%d] (%lx) is different from world checksum[%d] (%lx).\n",
					playerSlot, checksumIndex, (unsigned long)packetChecksum, checksumIndex,
					(unsigned long)g_worldChecksum[checksumIndex]);
				mismatch = 1;
			}
		}
		(void)packetPeerChecksumSum;
		(void)localPeerChecksumSum;

		if (mismatch) {
			if (FlightNet_SendWorldStateResyncToPlayer(senderDpid, g_worldStateDupBuffer, worldStateSize)) {
				FlightNet_SendWorldStateResyncApplyRequest(senderDpid, worldStateSize);
			}
			mismatch = 1;
		}
	}

	if (NetSession_GetLocalPlayerId() != 0) {
		int playerIdx;
		int aggregateStatus;

		g_flightNetWorldChecksumPeerStatus[playerSlot] = mismatch == 1 ? 2 : 1;
		aggregateStatus = 3;
		for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
			if (g_players[playerIdx].connectedFlag) {
				aggregateStatus &= g_flightNetWorldChecksumPeerStatus[playerIdx];
			}
		}
		if ((aggregateStatus & 1) != 0) {
			g_flightNetBufferWorldMessagesUntilChecksum = 0;
		}
	}
}

// FUNCTION: XWA 0x4EE330
int FlightNet_WaitForWorldStateChunkAcks(int directPlayId, int chunkCount) {
	int alertToggle;
	int stillLoadingElapsed;
	int lastAckCount;
	int retryCountdown;

	alertToggle = 0;
	stillLoadingElapsed = 0;
	lastAckCount = 0;
	retryCountdown = 20;

	do {
		int elapsedThisPass;
		int ackCount;
		int escapePressed;

		elapsedThisPass = 0;
		ackCount = lastAckCount;
		escapePressed = 0;
		while (1) {
			int savedInputTimestamp;
			int delta;

			if (FlightInput_HasKeyReady() && FlightInput_GetNextKey() == 27) {
				escapePressed = 1;
				break;
			}

			savedInputTimestamp = g_inputTimestamp;
			FlightNet_ProcessIncomingPackets();
			g_inputTimestamp += (int)Time_GetFrameDelta();
			delta = g_inputTimestamp - savedInputTimestamp;
			g_inputTimestamp = savedInputTimestamp;
			elapsedThisPass += delta;

			ackCount = 0;
			while (ackCount < chunkCount && g_flightNetWorldStateChunkAcked[ackCount] != 0) {
				++ackCount;
			}
			if (ackCount == chunkCount) {
				return 1;
			}
			if (elapsedThisPass >= 236) {
				break;
			}
		}

		if (escapePressed) {
			ackCount = lastAckCount;
			elapsedThisPass = 236;
			retryCountdown = 1;
		}

		if (ackCount == chunkCount) {
			break;
		}

		stillLoadingElapsed += elapsedThisPass;
		if (stillLoadingElapsed >= 236) {
			stillLoadingElapsed = 0;
			g_flightNetScratchPacket[0] = 29;
			NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 4);
			alertToggle = !alertToggle;
			if (alertToggle) {
				FlightAlert_DrawBox(3, g_strDiskIoMessages[29], NULL, 0x30u);
			} else {
				FlightAlert_DrawBox(3, g_strDiskIoMessages[28], NULL, 0x30u);
			}
		}

		if (ackCount == lastAckCount) {
			--retryCountdown;
		} else {
			retryCountdown = 20;
		}
		lastAckCount = ackCount;
	} while (retryCountdown != 0);

	if (retryCountdown != 0) {
		return 1;
	}

	g_flightNetScratchPacket[0] = 21;
	g_flightNetScratchPacket[1] = (uint32_t)NetSession_FindPlayerSlotByDpid(directPlayId);
	NetSession_BroadcastPacketToPlayers(g_flightNetScratchPacket, 8);
	return 0;
}

static void Flight_SetStepScaleFromDelta(uint16_t deltaTicks) {
	uint16_t scale;

	scale = deltaTicks != 0 ? (uint16_t)(236u / deltaTicks) : 236u;
	if (scale == 0) {
		scale = 1;
	}
	g_simStepScale = scale;
}

#ifdef XWA_MODERN
static void Flight_ModernUpdateAllCraftAI(uint16_t elapsedTicks) {
	uint16_t savedElapsedTicks;
	uint16_t savedSimStepScale;

	savedElapsedTicks = g_elapsedTicks;
	savedSimStepScale = g_simStepScale;
	g_elapsedTicks = elapsedTicks;
	Flight_SetStepScaleFromDelta(elapsedTicks);
	pai_UpdateAllCraftAI();
	g_elapsedTicks = savedElapsedTicks;
	g_simStepScale = savedSimStepScale;
}
#endif

// FUNCTION: XWA 0x4F6510
void Flight_StepSimToTime(int targetTimestamp) {
	int currentGameTime;
	char result;

	g_unusedFlightStepResetFlag = 0;
	g_filmPlaybackTimestampOverrideApplied = 0;
	Math_SetFpuSinglePrecisionMode();

	currentGameTime = g_gameTime;
	while (1) {
		uint8_t pauseState;
		uint8_t recordingState;
		int playbackHeaderStep;
		int pauseStateBeforeStep;

		pauseState = g_pauseState;
		if (pauseState == 1) {
			break;
		}

		g_flightStepRanThisFrame = 1;
		if (pauseState == 2) {
			pauseState = 1;
			g_pauseState = 1;
		}

		recordingState = g_filmRecording;
		g_flightSideEffectsEnabled = 1;
		g_elapsedTicks = (uint16_t)(targetTimestamp - currentGameTime);
		if (g_elapsedTicks < 1) {
			if (g_filmPlaybackMode == 0 && recordingState == 0) {
#ifdef XWA_MODERN
				XwaModernFlightTiming_BeginAdvance((uint16_t)g_elapsedTicks);
#endif
				Flight_AdvanceOneStep((int)(uint16_t)g_elapsedTicks + currentGameTime);
				return;
			}
			g_elapsedTicks = 1;
		}

		if ((int)(uint16_t)g_elapsedTicks > dtMs) {
			g_elapsedTicks = (uint16_t)dtMs;
		}

		playbackHeaderStep = 0;
		pauseStateBeforeStep = g_pauseState;
		if (!g_flightSimSideEffectsSuppressed) {
			if (recordingState == 2) {
				Film_WriteBytesBuffered(&g_gameTime, sizeof(g_gameTime));
				Film_WriteBytesBuffered(&g_elapsedTicks, sizeof(g_elapsedTicks));
				Film_WriteBytesBuffered(&targetTimestamp, sizeof(targetTimestamp));
				Film_WriteBytesBuffered(&dtMs, sizeof(dtMs));
				pauseState = g_pauseState;
				currentGameTime = g_gameTime;
			} else if (g_filmPlaybackMode == 2) {
				Film_ReadBytes(&g_gameTime, sizeof(g_gameTime));
				Film_ReadBytes(&g_elapsedTicks, sizeof(g_elapsedTicks));
				Film_ReadBytes(&targetTimestamp, sizeof(targetTimestamp));
				Film_ReadBytes(&dtMs, sizeof(dtMs));
				g_filmStepInputTimestamp = targetTimestamp;
				if (g_elapsedTicks == 0) {
					g_filmPlaybackMode = 4;
					g_elapsedTicks = 1;
					g_flightMissionEndPending = 1;
					return;
				}
				pauseState = g_pauseState;
				currentGameTime = g_gameTime;
			} else if (g_filmPlaybackMode == 1) {
				playbackHeaderStep = 1;
			} else if (recordingState == 1) {
				g_filmStepInputTimestamp = targetTimestamp;
			}
		}

		g_simStepScale = (uint16_t)(236 / (int)(uint16_t)g_elapsedTicks);
		if (g_simStepScale == 0) {
			g_simStepScale = 1;
		}
#ifdef XWA_MODERN
		XwaModernFlightTiming_BeginAdvance((uint16_t)g_elapsedTicks);
#endif
		if (g_filmPlaybackMode == 0 || pauseState != 0 || playbackHeaderStep) {
			g_filmPlaybackStepCatchupTicks = 0;
		} else {
			g_filmPlaybackStepCatchupTicks += (int)Time_GetSimStepDelta();
			if ((uint32_t)(uint16_t)g_elapsedTicks - (uint32_t)g_filmPlaybackStepCatchupTicks < 0x1d8u &&
				(uint32_t)g_filmPlaybackStepCatchupTicks < (uint16_t)g_elapsedTicks) {
				do {
					g_filmPlaybackStepCatchupTicks += (int)Time_GetSimStepDelta();
				} while ((uint32_t)g_filmPlaybackStepCatchupTicks < (uint16_t)g_elapsedTicks);
			}
			currentGameTime = g_gameTime;
			g_filmPlaybackStepCatchupTicks = 0;
		}

		Flight_AdvanceOneStep((int)(uint16_t)g_elapsedTicks + currentGameTime);
		if (g_filmPlaybackMode == 2) {
			Pause_ProcessInput();
		}

		result = (char)g_filmStepInputTimestamp;
		if (g_filmPlaybackMode != 0 && g_pauseState == 0 && pauseStateBeforeStep != 0) {
			targetTimestamp = g_filmStepInputTimestamp;
			g_inputTimestamp = g_filmStepInputTimestamp;
			g_filmPlaybackTimestampOverrideApplied = 1;
			g_filmPlaybackStepCatchupTicks = 0;
		}

		if (g_filmPlaybackMode == 3) {
			g_flightMissionEndPending = 1;
		} else if (playbackHeaderStep == 1) {
			targetTimestamp = g_filmStepInputTimestamp;
			g_inputTimestamp = g_filmStepInputTimestamp;
		}

		if (g_flightSimSideEffectsSuppressed || g_flightMissionEndPending != 1) {
#ifdef XWA_MODERN
			XwaModernAiCadence modernAi;
#endif

			Mission_UpdateFlightGroupArrivals();
			Flight_UpdateTimers();
#ifdef XWA_MODERN
			modernAi = XwaModernFlightTiming_BeginAiAdvance((uint16_t)g_elapsedTicks);
#endif
			{
				int simStepScale;

				simStepScale = (uint16_t)g_simStepScale;
				++g_fpsSampleRingIndex;
				g_fpsSampleHistory[g_fpsSampleRingIndex] = (float)simStepScale;
				if (g_fpsSampleRingIndex == 4) {
					g_fpsSampleRingIndex = 0;
				}
			}

			for (regionIdx = 0; (unsigned int)regionIdx < (unsigned int)g_activeMissionRegionCount;
				 ++regionIdx) {
				uint32_t objectIdx;

				g_flightSideEffectsEnabled = regionIdx == g_players[g_localPlayer].regionIndex;
				Mission_SetActiveRegionObjectRanges(regionIdx);
#ifdef XWA_MODERN
				if (!XwaModernFlightTiming_IsHighRate()) {
					pai_UpdateAllCraftAI();
				} else if (modernAi.due) {
					Flight_ModernUpdateAllCraftAI(modernAi.elapsed_ticks);
				}
#else
				pai_UpdateAllCraftAI();
#endif
				Flight_UpdateFighterWarnings(0);
				fsfx_UpdateNearbyWeaponLoop();

				objectIdx = g_activeRegionObjectSlotStart;
				while (objectIdx < g_activeRegionCraftObjectSlotEnd) {
					if (g_objectTable[objectIdx].objectType != OBJ_None) {
						CraftData* craft;
						AiController* ai;

						craft = g_objectTable[objectIdx].mobj->pCraft;
						g_curCraft = craft;
						ai = &craft->aiController;
#ifdef XWA_MODERN
						if (modernAi.due) {
							uint16_t aiElapsedTicks;

							aiElapsedTicks = modernAi.elapsed_ticks;
							if (ai->thinkTimer != 0) {
								ai->thinkTimer -= aiElapsedTicks;
								craft = g_curCraft;
							}
							if (ai->maneuverTimer != 0) {
								ai->maneuverTimer -= aiElapsedTicks;
								if (ai->maneuverTimer < 0) {
									ai->maneuverTimer = 0;
								}
								craft = g_curCraft;
							}
							if (ai->aiPlanState != 0) {
								ai->aiPlanState -= aiElapsedTicks;
								if (ai->aiPlanState < 0) {
									ai->aiPlanState = 0;
								}
								craft = g_curCraft;
							}
						}
#else
						if (ai->thinkTimer != 0) {
							ai->thinkTimer -= (uint16_t)g_elapsedTicks;
							craft = g_curCraft;
						}
						if (ai->maneuverTimer != 0) {
							ai->maneuverTimer -= (uint16_t)g_elapsedTicks;
							if (ai->maneuverTimer < 0) {
								ai->maneuverTimer = 0;
							}
							craft = g_curCraft;
						}
						if (ai->aiPlanState != 0) {
							ai->aiPlanState -= (uint16_t)g_elapsedTicks;
							if (ai->aiPlanState < 0) {
								ai->aiPlanState = 0;
							}
							craft = g_curCraft;
						}
#endif
						if (craft->weaponFireInhibitTimer != 0) {
							uint16_t oldTimer;

							oldTimer = craft->weaponFireInhibitTimer;
							craft->weaponFireInhibitTimer = (uint16_t)(oldTimer - g_elapsedTicks);
							craft = g_curCraft;
							if (craft->weaponFireInhibitTimer > oldTimer) {
								craft->weaponFireInhibitTimer = 0;
								craft = g_curCraft;
							}
						}
						if (craft->cmFireCooldownTimer != 0) {
							craft->cmFireCooldownTimer =
								(uint16_t)(craft->cmFireCooldownTimer - g_elapsedTicks);
							craft = g_curCraft;
							if ((int16_t)craft->cmFireCooldownTimer < 0) {
								craft->cmFireCooldownTimer = 0;
								craft = g_curCraft;
							}
						}
						{
							unsigned int weaponIdx;

							for (weaponIdx = 0; weaponIdx < craft->laserSlotCount; ++weaponIdx) {
								if (craft->warheadData[weaponIdx].turretRetargetCooldownTimer > 0) {
									craft->warheadData[weaponIdx].turretRetargetCooldownTimer =
										(int16_t)(craft->warheadData[weaponIdx].turretRetargetCooldownTimer -
												  (uint16_t)g_elapsedTicks);
									craft = g_curCraft;
								}
								if (craft->warheadData[weaponIdx].turretRotBucket > 0) {
									craft->warheadData[weaponIdx].turretRotBucket =
										(int16_t)(craft->warheadData[weaponIdx].turretRotBucket -
												  (uint16_t)g_elapsedTicks);
									craft = g_curCraft;
								}
							}
						}
					}
					++objectIdx;
				}

				Flight_UpdateCraftSteeringAndSpeed();
				collide_collisions();
				if (!g_flightSimSideEffectsSuppressed && g_flightMissionEndPending == 1) {
					break;
				}
				Object_UpdateLifetimeAndMovement();
				laser_weaponsfire();
				FlightObject_UpdateSpecialBehavior();
				if (!g_flightSimSideEffectsSuppressed && g_flightMissionEndPending == 1) {
					break;
				}
			}

			g_flightSideEffectsEnabled = 1;
			FlightObject_UpdateDebrisAndTransientAnimations();
			if (g_useHardware3D) {
#ifdef XWA_MODERN
				if (XwaModernFlightTiming_AdvanceGlowMarkAnimation((uint16_t)g_elapsedTicks)) {
#endif
					GlowMark_UpdateActivePatches();
#ifdef XWA_MODERN
				}
#endif
			}
			Player_ValidateAllCurrentTargets();
			Player_UpdateParticipationState();
			if (g_flightSimSideEffectsSuppressed || g_flightMissionEndPending != 1) {
				Mission_UpdateLogic();
				if (!g_provingGroundsModeActive) {
					Mfd_UpdateCommandMenuTargets();
				}
				Mission_SetActiveRegionObjectRanges(g_players[g_localPlayer].regionIndex);
				Hud_UpdateFlightMessagePanes();
				Flight_UpdateDynamicMusicState();
				ForceFeedback_UpdateActiveEffects((uint16_t)g_elapsedTicks);
				fsfx_UpdateVoiceQueue();
				fsfx_UpdatePlayerEngineLoop();
				fsfx_UpdateFlightSfx();
				fsfx_UpdateMissileThreatWarning();
				result = (char)g_provingGroundsModeActive;
				if (g_provingGroundsModeActive) {
					result = (char)Yard_UpdateChallengeTick((uint16_t)g_elapsedTicks);
				}
				currentGameTime = (uint16_t)g_elapsedTicks + g_gameTime;
				g_gameTime = currentGameTime;
				if (currentGameTime < targetTimestamp) {
					continue;
				}
			}
		}

		return;
	}

	Pause_ProcessInput();
	if (g_filmPlaybackMode != 0 && g_pauseState == 0) {
		g_filmPlaybackTimestampOverrideApplied = 1;
		targetTimestamp = g_filmStepInputTimestamp;
		g_inputTimestamp = g_filmStepInputTimestamp;
		g_filmPlaybackStepCatchupTicks = 0;
		(void)targetTimestamp;
	}
	Music_Update();
}

// FUNCTION: XWA 0x4F6B70
void Flight_AdvanceOneStep(int targetTimestamp) {
	uint32_t loopIdx;
	int inputCursor[8];
	int sideEffectsAllowed;
	int headerValue;
	int frameCount;
	InputFrame* frame;
	char buffer[1024];

	if (!g_flightSimSideEffectsSuppressed) {
		if (g_filmPlaybackMode == 0) {
			if (g_filmRecording == 1) {
				int savedCraftId;
#ifdef XWA_MODERN
				XwaFile* missionFile;
#else
				FILE* missionFile;
#endif

				{
					uint32_t frameDelta;

					frameDelta = Time_GetFrameDelta();
					g_filmRecording = 2;
					g_inputTimestamp = (int)frameDelta + g_inputTimestamp;
				}

				if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_TOUR) {
					headerValue = g_currentCdDisk;
					Film_WriteBytesBuffered(&headerValue, 2u);
				} else {
					headerValue = 2;
					Film_WriteBytesBuffered(&headerValue, 2u);
				}
				headerValue = 5;
				Film_WriteBytesBuffered(&headerValue, 2u);
				Film_WriteBytesBuffered(g_currentMissionFile, sizeof(g_currentMissionFile));

				{
					int filmCraftId;

					filmCraftId =
						Craft_FindCraftTypeForObjectType((unsigned short)g_cockpitObjectTypeForFilmHeader);
					savedCraftId = g_pilotData.networkPlayers[0].craftId;
					g_pilotData.networkPlayers[0].craftId = filmCraftId;
				}
				Film_WriteBytesBuffered(&g_pilotData, sizeof(g_pilotData));
				g_pilotData.networkPlayers[0].craftId = savedCraftId;
				Film_WriteBytesBuffered(&g_flightDifficulty, sizeof(g_flightDifficulty));
				Film_WriteBytesBuffered(&g_flightCollisionsEnabled, sizeof(g_flightCollisionsEnabled));

#ifdef XWA_MODERN
				missionFile = File_Open(AERON_VFS_ROOT_ASSET, g_currentMissionFile, "rb");
#else
				missionFile = fopen(g_currentMissionFile, "rb");
				if (missionFile == NULL) {
					strcpy(buffer, "D:\\");
					strcat(buffer, g_currentMissionFile);
					buffer[0] = File_GetCdDriveLetter();
					missionFile = fopen(buffer, "rb");
				}
#endif
				if (missionFile == NULL) {
					g_filmRecording = 0;
#ifdef XWA_MODERN
					File_Close(g_filmFile);
#else
					fclose((FILE*)g_filmFile);
#endif
					g_filmFile = NULL;
				} else {
					int allocatedWorldState;
					size_t bytesRead;

#ifdef XWA_MODERN
					File_Seek(missionFile, 0, SEEK_END);
					loopIdx = (uint32_t)File_Tell(missionFile);
					File_Seek(missionFile, 0, SEEK_SET);
#else
					fseek(missionFile, 0, SEEK_END);
					loopIdx = (uint32_t)ftell(missionFile);
					fseek(missionFile, 0, SEEK_SET);
#endif
					Film_WriteBytesBuffered(&loopIdx, sizeof(loopIdx));
					while (1) {
#ifdef XWA_MODERN
						bytesRead = File_ReadPartial(missionFile, buffer, sizeof(buffer));
#else
						bytesRead = fread(buffer, 1u, sizeof(buffer), missionFile);
#endif
						loopIdx = (uint32_t)Film_WriteBytesBuffered(buffer, bytesRead);
						if (bytesRead != sizeof(buffer)) {
							break;
						}
						if (loopIdx == 0) {
							g_filmRecording = 0;
#ifdef XWA_MODERN
							File_Close(g_filmFile);
#else
							fclose((FILE*)g_filmFile);
#endif
							g_filmFile = NULL;
							break;
						}
					}
#ifdef XWA_MODERN
					File_Close(missionFile);
#else
					fclose(missionFile);
#endif

					allocatedWorldState = 0;
					if (g_worldStateBuffer == NULL) {
						size_t bufferSize;
						unsigned int baseSize;

						allocatedWorldState = 1;
						baseSize = 41u * g_regionObjectSlotEnd + 229u * g_regionMainObjectSlotsTotal +
								   10u * (g_projectileObjectSlotsTotal +
										  402u * (uint32_t)(int16_t)g_missionHeader.numFlightGroups) +
								   114u * g_mobileObjectCharDataCount;
						bufferSize = baseSize + 1017u * g_craftObjectSlotsTotal + 20778u;
						if (g_provingGroundsModeActive != 0) {
							bufferSize += 3964u;
						}
						GameRand_GetPrimarySeed();
						bufferSize += 3023u * (uint32_t)g_flightPlayerCount + 22086u;

						g_worldStateHandle = Memory_AllocHandle("WORLDSTATEDATA", bufferSize);
						if (g_worldStateHandle == 0) {
							FeDiskIo_FatalError(0);
						}
						g_worldStateBuffer = (uint8_t*)Memory_LockHandle(g_worldStateHandle);
						g_worldStateDupHandle = Memory_AllocHandle("DUPWORLDSTATEDATA", bufferSize);
						if (g_worldStateDupHandle == 0) {
							FeDiskIo_FatalError(0);
						}
						g_worldStateDupBuffer = (uint8_t*)Memory_LockHandle(g_worldStateDupHandle);
					}

					Flight_SaveWorldState();
					loopIdx = (uint32_t)g_worldStateSize;
					Film_WriteBytesBuffered(&loopIdx, sizeof(loopIdx));
					Film_WriteBytesBuffered(g_worldStateBuffer, loopIdx);
					if (allocatedWorldState) {
						Memory_FreeHandle("WORLDSTATEDATA", g_worldStateHandle);
						g_worldStateHandle = 0;
						g_worldStateBuffer = NULL;
						Memory_FreeHandle("DUPWORLDSTATEDATA", g_worldStateDupHandle);
						g_worldStateDupHandle = 0;
						g_worldStateDupBuffer = NULL;
					}

					if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR) {
						DeathStar_WriteFilmStateBlock();
					}
					Hangar_FilmWriteSceneObjectState();

					for (loopIdx = 0; (int)loopIdx < (int)g_activeMissionRegionCount; ++loopIdx) {
						if (g_deathStarTunnelLaserRegions[loopIdx].enabled) {
							Film_WriteBytesBuffered(&g_deathStarTunnelLaserRegions[loopIdx],
													sizeof(g_deathStarTunnelLaserRegions[loopIdx]));
						}
					}

					Film_WriteBytesBuffered(&g_modelTextureOverrideNextSlot,
											sizeof(g_modelTextureOverrideNextSlot));
					for (loopIdx = 0; (int)loopIdx < 32; ++loopIdx) {
						Film_WriteBytesBuffered(&g_modelTextureOverrideSlots[loopIdx].modelType,
												sizeof(g_modelTextureOverrideSlots[loopIdx].modelType));
					}

					Film_WriteBytesBuffered(&g_escapePodPilotFlightGroupIdx,
											sizeof(g_escapePodPilotFlightGroupIdx));
					if (g_escapePodPilotFlightGroupIdx != -1) {
						Film_WriteBytesBuffered(
							&g_missionFlightGroups[g_escapePodPilotFlightGroupIdx],
							sizeof(g_missionFlightGroups[g_escapePodPilotFlightGroupIdx]));
					}
					Film_WriteBytesBuffered(&g_debrisDensityLevel, sizeof(g_debrisDensityLevel));
					Film_WriteBytesBuffered(
						&g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status1,
						sizeof(
							g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status1));
					Film_WriteBytesBuffered(
						&g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status2,
						sizeof(
							g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status2));

					loopIdx = GameRand_GetSecondarySeed();
					Film_WriteBytesBuffered(&loopIdx, sizeof(loopIdx));
					Film_WriteBytesBuffered(g_playerFlightTransientTimers,
											sizeof(g_playerFlightTransientTimers));
					Film_WriteBytesBuffered(&g_selectedMusicState, sizeof(g_selectedMusicState));
					Film_WriteBytesBuffered(&g_currentMusicState, sizeof(g_currentMusicState));
					Film_WriteBytesBuffered(&g_musicCombatSeen, 1u);
					Film_WriteBytesBuffered(&g_setMusicState, sizeof(g_setMusicState));
					Film_WriteBytesBuffered(&g_hardpointOriginOffset[0], sizeof(g_hardpointOriginOffset[0]));
					Film_WriteBytesBuffered(&g_hardpointOriginOffset[1], sizeof(g_hardpointOriginOffset[1]));
					Film_WriteBytesBuffered(&g_hardpointOriginOffset[2], sizeof(g_hardpointOriginOffset[2]));
					Film_WriteBytesBuffered(&g_projOffsetY, sizeof(g_projOffsetY));
					Film_WriteBytesBuffered(&g_collisionSegmentStartWorldX,
											sizeof(g_collisionSegmentStartWorldX));
					Film_WriteBytesBuffered(&g_collisionSegmentStartWorldY,
											sizeof(g_collisionSegmentStartWorldY));
					Film_WriteBytesBuffered(&g_collisionSegmentStartWorldZ,
											sizeof(g_collisionSegmentStartWorldZ));
					Film_WriteBytesBuffered(&g_collisionProbeWorldX, sizeof(g_collisionProbeWorldX));
					Film_WriteBytesBuffered(&g_collisionProbeWorldY, sizeof(g_collisionProbeWorldY));
					Film_WriteBytesBuffered(&g_collisionProbeWorldZ, sizeof(g_collisionProbeWorldZ));
					Film_WriteBytesBuffered(&g_collisionSweepStartX, sizeof(g_collisionSweepStartX));
					Film_WriteBytesBuffered(&g_collisionSweepStartY, sizeof(g_collisionSweepStartY));
					Film_WriteBytesBuffered(&g_collisionSweepStartZ, sizeof(g_collisionSweepStartZ));
					Film_WriteBytesBuffered(&g_collisionSweepEndX, sizeof(g_collisionSweepEndX));
					Film_WriteBytesBuffered(&g_collisionSweepEndY, sizeof(g_collisionSweepEndY));
					Film_WriteBytesBuffered(&g_collisionSweepEndZ, sizeof(g_collisionSweepEndZ));
					Film_WriteBytesBuffered(&g_collisionHitOffsetX, sizeof(g_collisionHitOffsetX));
					Film_WriteBytesBuffered(&g_collisionHitOffsetY, sizeof(g_collisionHitOffsetY));
					Film_WriteBytesBuffered(&g_collisionHitOffsetZ, sizeof(g_collisionHitOffsetZ));
					Film_WriteBytesBuffered(&g_inHangarReady, sizeof(g_inHangarReady));
					Film_WriteBytesBuffered(&g_gameTime, sizeof(g_gameTime));
					Film_WriteBytesBuffered(&g_elapsedTicks, sizeof(g_elapsedTicks));
					Film_WriteBytesBuffered(&g_filmStepInputTimestamp, sizeof(g_filmStepInputTimestamp));
					Film_WriteBytesBuffered(&targetTimestamp, sizeof(targetTimestamp));
					Film_WriteBytesBuffered(&dtMs, sizeof(dtMs));
				}
				Time_GetFrameDelta();
			}
		} else if (g_filmPlaybackMode == 1) {
			int allocatedWorldState;

			g_inputTimestamp = (int)Time_GetFrameDelta() + g_inputTimestamp;
			Film_SeekPastEmbeddedMissionData();

			allocatedWorldState = 0;
			if (g_worldStateBuffer == NULL) {
				size_t bufferSize;
				unsigned int baseSize;

				allocatedWorldState = 1;
				baseSize = 41u * g_regionObjectSlotEnd + 229u * g_regionMainObjectSlotsTotal +
						   10u * (g_projectileObjectSlotsTotal +
								  402u * (uint32_t)(int16_t)g_missionHeader.numFlightGroups) +
						   114u * g_mobileObjectCharDataCount;
				bufferSize = baseSize + 1017u * g_craftObjectSlotsTotal + 20778u;
				if (g_provingGroundsModeActive != 0) {
					bufferSize += 3964u;
				}
				GameRand_GetPrimarySeed();
				bufferSize += 3023u * (uint32_t)g_flightPlayerCount + 22086u;

				g_worldStateHandle = Memory_AllocHandle("WORLDSTATEDATA", bufferSize);
				if (g_worldStateHandle == 0) {
					FeDiskIo_FatalError(0);
				}
				g_worldStateBuffer = (uint8_t*)Memory_LockHandle(g_worldStateHandle);
				g_worldStateDupHandle = Memory_AllocHandle("DUPWORLDSTATEDATA", bufferSize);
				if (g_worldStateDupHandle == 0) {
					FeDiskIo_FatalError(0);
				}
				g_worldStateDupBuffer = (uint8_t*)Memory_LockHandle(g_worldStateDupHandle);
			}

			Film_ReadBytes(&loopIdx, sizeof(loopIdx));
			Film_ReadBytes(g_worldStateBuffer, loopIdx);
			Flight_RestoreWorldState();
			if (allocatedWorldState) {
				Memory_FreeHandle("WORLDSTATEDATA", g_worldStateHandle);
				g_worldStateHandle = 0;
				g_worldStateBuffer = NULL;
				Memory_FreeHandle("DUPWORLDSTATEDATA", g_worldStateDupHandle);
				g_worldStateDupHandle = 0;
				g_worldStateDupBuffer = NULL;
			}

			if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR) {
				DeathStar_ReadFilmStateBlock();
			}
			Hangar_FilmReadSceneObjectState();

			for (loopIdx = 0; (int)loopIdx < (int)g_activeMissionRegionCount; ++loopIdx) {
				if (g_deathStarTunnelLaserRegions[loopIdx].enabled) {
					Film_ReadBytes(&g_deathStarTunnelLaserRegions[loopIdx],
								   sizeof(g_deathStarTunnelLaserRegions[loopIdx]));
				}
			}

			if (g_filmVersion != 0) {
				Film_ReadBytes(&g_modelTextureOverrideNextSlot, sizeof(g_modelTextureOverrideNextSlot));
				for (loopIdx = 0; (int)loopIdx < 32; ++loopIdx) {
					Film_ReadBytes(&g_modelTextureOverrideSlots[loopIdx].modelType,
								   sizeof(g_modelTextureOverrideSlots[loopIdx].modelType));
					if (g_modelTextureOverrideSlots[loopIdx].modelType != 0) {
						ModelMesh_AssignDebrisTexSlot(
							(ObjectTypeId)g_modelTextureOverrideSlots[loopIdx].modelType, (uint16_t)loopIdx);
					}
				}
			}

			if ((uint16_t)g_filmVersion > 1u) {
				Film_ReadBytes(&g_escapePodPilotFlightGroupIdx, sizeof(g_escapePodPilotFlightGroupIdx));
				if (g_escapePodPilotFlightGroupIdx != -1) {
					Film_ReadBytes(&g_missionFlightGroups[g_escapePodPilotFlightGroupIdx],
								   sizeof(g_missionFlightGroups[g_escapePodPilotFlightGroupIdx]));
				}
			}
			if ((uint16_t)g_filmVersion > 2u) {
				Film_ReadBytes(&g_debrisDensityLevel, sizeof(g_debrisDensityLevel));
			}
			if ((uint16_t)g_filmVersion > 3u) {
				Film_ReadBytes(
					&g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status1,
					sizeof(g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status1));
				Film_ReadBytes(
					&g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status2,
					sizeof(g_missionFlightGroups[g_players[g_localPlayer].boundFlightGroupIdx].fg.status2));
			}

			Film_ReadBytes(&loopIdx, sizeof(loopIdx));
			GameRand_SetSecondarySeed((int16_t)loopIdx);
			Film_ReadBytes(g_playerFlightTransientTimers, sizeof(g_playerFlightTransientTimers));
			Film_ReadBytes(&g_selectedMusicState, sizeof(g_selectedMusicState));
			Film_ReadBytes(&g_currentMusicState, sizeof(g_currentMusicState));
			g_currentMusicState = MUSIC_STATE_NONE;
			Film_ReadBytes(&g_musicCombatSeen, 1u);
			Film_ReadBytes(&g_setMusicState, sizeof(g_setMusicState));
			Film_ReadBytes(&g_hardpointOriginOffset[0], sizeof(g_hardpointOriginOffset[0]));
			Film_ReadBytes(&g_hardpointOriginOffset[1], sizeof(g_hardpointOriginOffset[1]));
			Film_ReadBytes(&g_hardpointOriginOffset[2], sizeof(g_hardpointOriginOffset[2]));
			Film_ReadBytes(&g_projOffsetY, sizeof(g_projOffsetY));
			Film_ReadBytes(&g_collisionSegmentStartWorldX, sizeof(g_collisionSegmentStartWorldX));
			Film_ReadBytes(&g_collisionSegmentStartWorldY, sizeof(g_collisionSegmentStartWorldY));
			Film_ReadBytes(&g_collisionSegmentStartWorldZ, sizeof(g_collisionSegmentStartWorldZ));
			Film_ReadBytes(&g_collisionProbeWorldX, sizeof(g_collisionProbeWorldX));
			Film_ReadBytes(&g_collisionProbeWorldY, sizeof(g_collisionProbeWorldY));
			Film_ReadBytes(&g_collisionProbeWorldZ, sizeof(g_collisionProbeWorldZ));
			Film_ReadBytes(&g_collisionSweepStartX, sizeof(g_collisionSweepStartX));
			Film_ReadBytes(&g_collisionSweepStartY, sizeof(g_collisionSweepStartY));
			Film_ReadBytes(&g_collisionSweepStartZ, sizeof(g_collisionSweepStartZ));
			Film_ReadBytes(&g_collisionSweepEndX, sizeof(g_collisionSweepEndX));
			Film_ReadBytes(&g_collisionSweepEndY, sizeof(g_collisionSweepEndY));
			Film_ReadBytes(&g_collisionSweepEndZ, sizeof(g_collisionSweepEndZ));
			Film_ReadBytes(&g_collisionHitOffsetX, sizeof(g_collisionHitOffsetX));
			Film_ReadBytes(&g_collisionHitOffsetY, sizeof(g_collisionHitOffsetY));
			Film_ReadBytes(&g_collisionHitOffsetZ, sizeof(g_collisionHitOffsetZ));
			Film_ReadBytes(&g_inHangarReady, sizeof(g_inHangarReady));
			Film_ReadBytes(&g_gameTime, sizeof(g_gameTime));
			Film_ReadBytes(&g_elapsedTicks, sizeof(g_elapsedTicks));
			Film_ReadBytes(&g_filmStepInputTimestamp, sizeof(g_filmStepInputTimestamp));
			Film_ReadBytes(&targetTimestamp, sizeof(targetTimestamp));
			Film_ReadBytes(&dtMs, sizeof(dtMs));

			g_inputTimestamp = g_filmStepInputTimestamp;
			if (g_elapsedTicks == 0) {
				g_elapsedTicks = 1;
				g_flightMissionEndPending = 1;
				g_filmPlaybackMode = 4;
				return;
			}

			g_simStepScale = (uint16_t)(236 / (int)g_elapsedTicks);
			if (g_simStepScale == 0) {
				g_simStepScale = 1;
			}
			g_mfdLeftNeedsRedraw = 1;
			g_mfdRightNeedsRedraw = 1;
			g_filmPlaybackMode = 2;
			Time_GetFrameDelta();
		}
	}

	if (g_flightPlayerCount == 1) {
		sideEffectsAllowed = 1;
	} else {
		sideEffectsAllowed = g_flightSimSideEffectsSuppressed;
	}
	for (loopIdx = 0; (int)loopIdx < 8; ++loopIdx) {
		inputCursor[loopIdx] = 0;
		if (g_flightSimSideEffectsSuppressed) {
			continue;
		}

		if (g_filmRecording == 2) {
			if (g_players[loopIdx].connectedFlag) {
				InputFrame* playerHistory;

				playerHistory = g_inputHistory[loopIdx];
				frameCount = g_inputFrameCount[loopIdx];
				Film_WriteBytesBuffered(&frameCount, sizeof(frameCount));
				Film_WriteBytesBuffered(playerHistory,
										(size_t)frameCount * sizeof(g_inputHistory[loopIdx][0]));
			}
		} else if (g_filmPlaybackMode == 2 && g_players[loopIdx].connectedFlag) {
			InputFrame* playerHistory;

			playerHistory = g_inputHistory[loopIdx];
			Film_ReadBytes(&frameCount, sizeof(frameCount));
			g_inputFrameCount[loopIdx] = frameCount;
			if (frameCount <= 8) {
				Film_ReadBytes(playerHistory, (size_t)frameCount * sizeof(g_inputHistory[loopIdx][0]));
			} else {
				g_flightMissionEndPending = 1;
				g_filmPlaybackMode = 4;
				return;
			}
		}
	}

	while (1) {
		int selectedPlayer;

		selectedPlayer = -1;
		for (loopIdx = 0; (int)loopIdx < 8; ++loopIdx) {
			if (g_players[loopIdx].connectedFlag && inputCursor[loopIdx] < g_inputFrameCount[loopIdx] &&
				(selectedPlayer == -1 ||
				 g_inputHistory[loopIdx][inputCursor[loopIdx]].timestamp <
					 g_inputHistory[selectedPlayer][inputCursor[selectedPlayer]].timestamp)) {
				selectedPlayer = loopIdx;
			}
		}

		if (selectedPlayer == -1) {
			return;
		}

		{
			uint16_t savedObjectSignature;
			uint16_t savedSpeed;
			uint16_t savedSpeedRemainder;
			uint16_t savedElapsedTicks;
			uint16_t savedSimStepScale;
			int lockstepTimestamp;
			int frameTimestamp;
			int savedGameTime;
			int savedHasCheckpointFlag;
			int savedObjectIndex;
			int savedSimStateTimestamp;
			int savedWorldX;
			int savedWorldY;
			int savedWorldZ;
			Q16Angle savedRoll;
			Q16Angle savedPitch;
			Q16Angle savedYaw;
			int savedLifetimeTimer;
			int16_t savedRollImpulseRate;

			frame = &g_inputHistory[selectedPlayer][inputCursor[selectedPlayer]];
			++inputCursor[selectedPlayer];
			Mission_SetActiveRegionObjectRanges(g_players[selectedPlayer].regionIndex);

			if ((!sideEffectsAllowed || g_flightPlayerCount == 1) &&
				frame->timestamp <= g_players[selectedPlayer].lockstepTimestamp && frame->applied == 0) {
				int frameIdx;
				int frameCount;

				frameCount = g_inputFrameCount[selectedPlayer];
				if (frameCount != 0 && frame >= g_inputHistory[selectedPlayer]) {
					--g_inputFrameCount[selectedPlayer];
					for (frameIdx = 0; frameIdx < g_inputFrameCount[selectedPlayer]; ++frameIdx) {
						if (&g_inputHistory[selectedPlayer][frameIdx] >= frame) {
							g_inputHistory[selectedPlayer][frameIdx] =
								g_inputHistory[selectedPlayer][frameIdx + 1];
						}
					}
				}
				--inputCursor[selectedPlayer];
				continue;
			}

			if (frame->timestamp > targetTimestamp || (!sideEffectsAllowed && frame->valid != 0) ||
				frame->timestamp <= g_players[selectedPlayer].lockstepTimestamp) {
				continue;
			}

			frameTimestamp = frame->timestamp;
			savedGameTime = g_gameTime;
			savedHasCheckpointFlag = g_players[selectedPlayer].hasCheckpointFlag;
			lockstepTimestamp = g_players[selectedPlayer].lockstepTimestamp;

			if (savedHasCheckpointFlag) {
				savedObjectIndex = g_players[selectedPlayer].objectIndex;
				if (savedObjectIndex != 0xffff) {
					ObjectRecord* obj;
					MobileObject* mobj;

					obj = &g_objectTable[savedObjectIndex];
					mobj = obj->mobj;
					savedSimStateTimestamp = mobj->simStateTimestamp;
					savedWorldX = obj->world_x;
					savedWorldY = obj->world_y;
					savedWorldZ = obj->world_z;
					savedRoll = obj->roll;
					savedPitch = obj->pitch;
					savedYaw = obj->yaw;
					savedObjectSignature = obj->objectSignature;
					savedLifetimeTimer = mobj->lifetimeTimer;
					savedRollImpulseRate = mobj->rollImpulseRate;
					savedSpeed = mobj->speed;
					savedSpeedRemainder = mobj->speedRemainder;
				}
			}

			if (frameTimestamp <= g_gameTime) {
				int playerObjectIndex;

				playerObjectIndex = g_players[selectedPlayer].objectIndex;
				if (playerObjectIndex != 0xffff &&
					g_players[selectedPlayer].regionIndex != g_hangarSceneRegionIdx) {
					ObjectRecord* playerObj;
					MobileObject* playerMobj;

					playerObj = &g_objectTable[playerObjectIndex];
					playerMobj = playerObj->mobj;
					if (playerMobj != NULL && playerMobj->pCraft != NULL) {
						if (playerObj->objectSignature == (uint16_t)g_players[selectedPlayer].savedFieldId &&
							g_players[selectedPlayer].regionSessionId ==
								g_players[selectedPlayer].savedRegion) {
							if (playerMobj->simStateTimestamp > lockstepTimestamp) {
								playerMobj->simStateTimestamp = lockstepTimestamp;
								g_objectTable[g_players[selectedPlayer].objectIndex].world_x =
									g_players[selectedPlayer].savedX;
								g_objectTable[g_players[selectedPlayer].objectIndex].world_y =
									g_players[selectedPlayer].savedY;
								g_objectTable[g_players[selectedPlayer].objectIndex].world_z =
									g_players[selectedPlayer].savedZ;
								g_objectTable[g_players[selectedPlayer].objectIndex].roll =
									(Q16Angle)g_players[selectedPlayer].savedRoll;
								g_objectTable[g_players[selectedPlayer].objectIndex].pitch =
									(Q16Angle)g_players[selectedPlayer].savedPitch;
								g_objectTable[g_players[selectedPlayer].objectIndex].yaw =
									(Q16Angle)g_players[selectedPlayer].savedYaw;
								g_objectTable[g_players[selectedPlayer].objectIndex].mobj->lifetimeTimer =
									g_players[selectedPlayer].savedLifetimeTimer;
								g_objectTable[g_players[selectedPlayer].objectIndex].mobj->rollImpulseRate =
									g_players[selectedPlayer].savedRollImpulseRate;
								g_objectTable[g_players[selectedPlayer].objectIndex].mobj->speed =
									(uint16_t)g_players[selectedPlayer].savedSpeed;
								g_objectTable[g_players[selectedPlayer].objectIndex].mobj->speedRemainder =
									(uint16_t)g_players[selectedPlayer].savedSpeedRemainder;
								g_objectTable[g_players[selectedPlayer].objectIndex].mobj->orientMatrixDirty =
									1;
								g_objectTable[g_players[selectedPlayer].objectIndex].mobj->moveVectorDirty =
									1;
							}
							if (g_gameTime > g_objectTable[g_players[selectedPlayer].objectIndex]
												 .mobj->simStateTimestamp) {
								g_gameTime = g_objectTable[g_players[selectedPlayer].objectIndex]
												 .mobj->simStateTimestamp;
							}
						} else if (!sideEffectsAllowed) {
							frame->timestamp = g_gameTime + 4;
							g_players[selectedPlayer].lockstepTimestamp = g_gameTime;
						} else {
							continue;
						}
					} else {
						continue;
					}
				}
			}

			savedElapsedTicks = g_elapsedTicks;
			savedSimStepScale = g_simStepScale;
			if (g_players[selectedPlayer].objectIndex != 0xffff &&
				g_players[selectedPlayer].regionIndex != g_hangarSceneRegionIdx) {
				int objectIndex;

				objectIndex = g_players[selectedPlayer].objectIndex;
				g_singleObjectUpdateOverrideIdx = objectIndex;
				if (g_objectTable[objectIndex].mobj != NULL) {
					ObjectRecord* obj;

					g_elapsedTicks = (uint16_t)(frame->timestamp - g_gameTime);
					if (g_elapsedTicks != 0) {
						g_simStepScale = (uint16_t)(236 / (int)g_elapsedTicks);
					} else {
						g_simStepScale = 236u;
					}
					if (g_simStepScale == 0) {
						g_simStepScale = 1;
					}
					Flight_UpdateCraftSteeringAndSpeed();
					Object_UpdateLifetimeAndMovement();
					g_objectTable[g_singleObjectUpdateOverrideIdx].mobj->simStateTimestamp = frame->timestamp;

					obj = &g_objectTable[g_singleObjectUpdateOverrideIdx];
					g_players[selectedPlayer].savedX = obj->world_x;
					g_players[selectedPlayer].savedY = obj->world_y;
					g_players[selectedPlayer].savedZ = obj->world_z;
					g_players[selectedPlayer].savedRoll = (int16_t)obj->roll;
					g_players[selectedPlayer].savedPitch = (int16_t)obj->pitch;
					g_players[selectedPlayer].savedYaw = (int16_t)obj->yaw;
					g_players[selectedPlayer].savedLifetimeTimer = obj->mobj->lifetimeTimer;
					g_players[selectedPlayer].savedRollImpulseRate = obj->mobj->rollImpulseRate;
					g_players[selectedPlayer].savedSpeed = (int16_t)obj->mobj->speed;
					g_players[selectedPlayer].savedSpeedRemainder = (int16_t)obj->mobj->speedRemainder;
					g_players[selectedPlayer].savedFieldId = (int16_t)obj->objectSignature;
					g_players[selectedPlayer].savedRegion = g_players[selectedPlayer].regionSessionId;
				}
				g_singleObjectUpdateOverrideIdx = -1;
			}

			g_elapsedTicks =
				(uint16_t)(frame->timestamp - (uint16_t)g_players[selectedPlayer].lockstepTimestamp);
#ifdef XWA_MODERN
			if (XwaModernFlightTiming_StepTicks() != 1 && g_elapsedTicks < 4u) {
				g_elapsedTicks = 4;
			}
#else
			if (g_elapsedTicks < 4u) {
				g_elapsedTicks = 4;
			}
#endif
			g_simStepScale = g_elapsedTicks != 0 ? (uint16_t)(236 / (int)g_elapsedTicks) : 236u;
			if (g_simStepScale == 0) {
				g_simStepScale = 1;
			}
			g_players[selectedPlayer].lockstepTimestamp = frame->timestamp;
			g_replayInputs[selectedPlayer] = frame->input;
			if (sideEffectsAllowed && frame->timestamp <= savedGameTime) {
				g_replayInputs[selectedPlayer].key = KEY_NONE;
				g_replayInputs[selectedPlayer].keyMods = 0;
			}
			if (g_flightPlayerCount > 1 && selectedPlayer == g_localPlayer) {
				g_flightSfxSideEffectGate = 2;
				if (frame->timestamp > g_lastLocalReplayInputTimestamp) {
					g_flightSfxSideEffectGate = 1;
					g_lastLocalReplayInputTimestamp = frame->timestamp;
				}
			}
			if (g_players[selectedPlayer].regionIndex != g_hangarSceneRegionIdx) {
				Flight_UpdateEntity((unsigned int)selectedPlayer);
				ComputeHardpointWorldPos(selectedPlayer);
			}

			g_gameTime = savedGameTime;
			g_flightSfxSideEffectGate = 0;
			g_elapsedTicks = savedElapsedTicks;
			g_simStepScale = savedSimStepScale;

			if (savedHasCheckpointFlag && savedObjectIndex != 0xffff) {
				g_objectTable[savedObjectIndex].mobj->simStateTimestamp = savedSimStateTimestamp;
				g_objectTable[savedObjectIndex].world_x = savedWorldX;
				g_objectTable[savedObjectIndex].world_y = savedWorldY;
				g_objectTable[savedObjectIndex].world_z = savedWorldZ;
				g_objectTable[savedObjectIndex].roll = savedRoll;
				g_objectTable[savedObjectIndex].pitch = savedPitch;
				g_objectTable[savedObjectIndex].yaw = savedYaw;
				g_objectTable[savedObjectIndex].mobj->lifetimeTimer = savedLifetimeTimer;
				g_objectTable[savedObjectIndex].mobj->rollImpulseRate = savedRollImpulseRate;
				g_objectTable[savedObjectIndex].mobj->speed = savedSpeed;
				g_objectTable[savedObjectIndex].mobj->speedRemainder = savedSpeedRemainder;
				g_objectTable[savedObjectIndex].objectSignature = savedObjectSignature;
			}
		}
	}
}

// FUNCTION: XWA 0x50B5C0
int Flight_PumpWindowMessages(void) {
	/* Port replacement: Aeron owns SDL event pumping and focus/cursor platform state. */
	return 0;
}

// FUNCTION: XWA 0x5097C0
int Pause_ProcessInput(void) {
	int key;

	key = g_currentInputFrame.key;
	if (key <= 0x2f) {
		if (key == 47) {
			goto toggle_film_overlay;
		}
		if (key == 42) {
			goto clear_film_aim_target;
		}
		goto process_film_overlay_input;
	}

	switch (key) {
		case 114:
			Music_SetState(MUSIC_STATE_NONE);
			Hud_MarkFilmOverlayElementsVisible();
			g_flightMissionEndPending = 1;
			g_filmPlaybackMode = 3;
			fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
			break;

		case 102:
			if (g_pauseState < 3u) {
				fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
				g_pauseState = 3;
			} else {
				fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
				g_pauseState = 0;
			}
			break;

		case 113:
			g_flightMissionEndPending = 1;
			g_filmPlaybackMode = 4;
			fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
			break;

		case 115:
			if (g_pauseState == 0) {
				g_pauseState = 1;
				fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
			} else {
				g_pauseState = 2;
				fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
			}
			break;

		case 164:
			if (g_pauseState == 1) {
				g_pauseState = 2;
				fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
			}
			break;

		case 112:
			if (g_pauseState) {
				fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
			}
			g_pauseState = 0;
			break;

		case 189:
			goto toggle_film_overlay;

		case 99:
			if (g_filmOverlayActive != 1) {
				return 1;
			}
			Hud_MarkFilmOverlayElementsVisible();
			if (g_missionHeader.body.hangar != XWA_HANGAR_DEATHSTAR &&
				g_missionHeader.body.hangar != XWA_HANGAR_SIMULATOR2 && g_missionHeader.body.hangar != 0) {
				if (g_filmOverlayViewState.cameraFocusObjIdx == 0xffff) {
					g_filmOverlayViewState.cameraFocusObjIdx = g_players[g_localPlayer].objectIndex;
					if (g_filmOverlayViewState.cameraFocusObjIdx != 0xffff) {
						g_filmOverlayViewState.cameraDistance =
							g_modelTypeTable[g_objectTable[g_filmOverlayViewState.cameraFocusObjIdx]
												 .objectType]
								.maxBoundsExtent +
							512;
					}
					g_filmOverlayViewState.hudAimX = (uint16_t)(int16_t)-2790;
					g_filmOverlayViewState.hudAimY = 0;
				} else {
					g_filmOverlayViewState.cameraFocusObjIdx = 0xffff;
					g_filmOverlayViewState.aimTargetIdx = 0xffff;
				}
			}
			break;

		case 116:
			if (g_filmOverlayActive != 1) {
				return 1;
			}
			if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR || g_provingGroundsModeActive) {
				break;
			}
			Hud_MarkFilmOverlayElementsVisible();
			{
				int previousFocusObjIdx;
				int nextFocusObjIdx;

				previousFocusObjIdx = g_filmOverlayViewState.cameraFocusObjIdx;
				if (g_filmOverlayViewState.cameraFocusObjIdx != 0xffff) {
					nextFocusObjIdx = (uint16_t)FilmOverlay_FindNextSelectableObject(
						(ObjectIndex)g_filmOverlayViewState.cameraFocusObjIdx, 1, g_localPlayer, 0xffff);
				} else {
					nextFocusObjIdx = g_players[g_localPlayer].objectIndex;
				}
				g_filmOverlayViewState.cameraFocusObjIdx = nextFocusObjIdx;
				fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
				if (g_filmOverlayViewState.cameraFocusObjIdx != 0xffff) {
					g_filmOverlayViewState.cameraDistance =
						g_modelTypeTable[g_objectTable[g_filmOverlayViewState.cameraFocusObjIdx].objectType]
							.maxBoundsExtent +
						512;
					g_filmOverlayViewState.hudAimX = (uint16_t)(int16_t)-2790;
					g_filmOverlayViewState.hudAimY = 0;
				}
				if (g_filmOverlayViewState.aimTargetIdx != 0xffff) {
					g_filmOverlayViewState.aimTargetIdx = previousFocusObjIdx;
				}
			}
			break;

		case 121:
			if (g_filmOverlayActive != 1) {
				return 1;
			}
			if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR || g_provingGroundsModeActive) {
				break;
			}
			Hud_MarkFilmOverlayElementsVisible();
			{
				int previousFocusObjIdx;
				int nextFocusObjIdx;

				previousFocusObjIdx = g_filmOverlayViewState.cameraFocusObjIdx;
				if (g_filmOverlayViewState.cameraFocusObjIdx != 0xffff) {
					nextFocusObjIdx = (uint16_t)FilmOverlay_FindNextSelectableObject(
						(ObjectIndex)g_filmOverlayViewState.cameraFocusObjIdx, -1, g_localPlayer, 0xffff);
				} else {
					nextFocusObjIdx = g_players[g_localPlayer].objectIndex;
				}
				g_filmOverlayViewState.cameraFocusObjIdx = nextFocusObjIdx;
				fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
				if (g_filmOverlayViewState.cameraFocusObjIdx != 0xffff) {
					g_filmOverlayViewState.cameraDistance =
						g_modelTypeTable[g_objectTable[g_filmOverlayViewState.cameraFocusObjIdx].objectType]
							.maxBoundsExtent +
						512;
					g_filmOverlayViewState.hudAimX = (uint16_t)(int16_t)-2790;
					g_filmOverlayViewState.hudAimY = 0;
				}
				if (g_filmOverlayViewState.aimTargetIdx != 0xffff) {
					g_filmOverlayViewState.aimTargetIdx = previousFocusObjIdx;
				}
			}
			break;

		case 103: {
			int objectIdx;

			if (g_filmOverlayActive != 1) {
				return 1;
			}
			Hud_MarkFilmOverlayElementsVisible();
			if (g_filmOverlayViewState.aimTargetIdx != 0xffff) {
				objectIdx = (uint16_t)FilmOverlay_FindNextSelectableObject(
					(ObjectIndex)g_filmOverlayViewState.aimTargetIdx, 1, g_localPlayer,
					g_filmOverlayViewState.cameraFocusObjIdx);
			} else {
				objectIdx = g_players[g_localPlayer].objectIndex;
				if (g_filmOverlayViewState.cameraFocusObjIdx == objectIdx) {
					objectIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
					if ((uint16_t)objectIdx == 0xffffu) {
						objectIdx = (uint16_t)FilmOverlay_FindNextSelectableObject(
							(ObjectIndex)g_filmOverlayViewState.cameraFocusObjIdx, 1, g_localPlayer,
							g_filmOverlayViewState.cameraFocusObjIdx);
					}
				}
			}
			g_filmOverlayViewState.aimTargetIdx = objectIdx;
			fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
			break;
		}

		case 104: {
			int objectIdx;

			if (g_filmOverlayActive != 1) {
				return 1;
			}
			Hud_MarkFilmOverlayElementsVisible();
			if (g_filmOverlayViewState.aimTargetIdx != 0xffff) {
				objectIdx = (uint16_t)FilmOverlay_FindNextSelectableObject(
					(ObjectIndex)g_filmOverlayViewState.aimTargetIdx, -1, g_localPlayer,
					g_filmOverlayViewState.cameraFocusObjIdx);
			} else {
				objectIdx = g_players[g_localPlayer].objectIndex;
				if (g_filmOverlayViewState.cameraFocusObjIdx == objectIdx) {
					objectIdx = (uint16_t)g_players[g_localPlayer].currentTargetObjectIdx;
					if ((uint16_t)objectIdx == 0xffffu) {
						objectIdx = (uint16_t)FilmOverlay_FindNextSelectableObject(
							(ObjectIndex)g_filmOverlayViewState.cameraFocusObjIdx, 1, g_localPlayer,
							g_filmOverlayViewState.cameraFocusObjIdx);
					}
				}
			}
			g_filmOverlayViewState.aimTargetIdx = objectIdx;
			fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
			break;
		}

		case 190:
			goto clear_film_aim_target;

		case 300:
			if (g_filmPlaybackMode) {
				Hud_SetFilmOverlayMfdVisible((char)!g_filmOverlayMfdVisible);
			}
			break;

		default:
			break;
	}

	goto process_film_overlay_input;

toggle_film_overlay:
	if (!g_players[g_localPlayer].mapCameraState &&
		g_players[g_localPlayer].hyperspacePhase == PLAYER_HYPERSPACE_PHASE_NONE && !g_inHangarReady) {
		if (!g_filmOverlayActive) {
			uint8_t hudStateLive;
			int defaultHudAimX;

			defaultHudAimX = -2790;
			memcpy(&g_filmOverlayViewState, &g_players[g_localPlayer].viewState,
				   sizeof(g_filmOverlayViewState));
			hudStateLive = g_filmOverlayViewState.hudStateLive;
			g_filmOverlayViewState.hudAimX = (uint16_t)defaultHudAimX;
			g_filmOverlayViewState.savedHudAimX = (uint16_t)defaultHudAimX;
			g_filmOverlayViewState.hudStateLive = 18;
			g_filmOverlayViewState.hudStateMirror = 18;
			g_filmOverlayViewState.externalCameraActive = 1;
			g_filmOverlayViewState.playerInputBlocked = 0;
			g_filmOverlayViewState.aimTargetIdx = 0xffff;
			g_filmOverlayViewState.transitionTimer = 0;
			g_filmOverlayViewState.hudAimY = 0;
			g_filmOverlayViewState.savedHudStateByte = hudStateLive;
			g_filmOverlayViewState.savedHudAimY = 0;
			if (g_filmOverlayViewState.cameraFocusObjIdx != 0xffff) {
				g_filmOverlayViewState.cameraDistance =
					g_modelTypeTable[g_objectTable[g_filmOverlayViewState.cameraFocusObjIdx].objectType]
						.maxBoundsExtent +
					512;
			} else {
				g_filmOverlayViewState.cameraDistance = 1024;
			}
			g_filmOverlayActive = 1;
			Hud_SyncLocalSoftwareHudMasks(0);
			fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
		} else {
			Hud_SetHudViewState(g_players[g_localPlayer].viewState.hudStateLive, g_localPlayer);
			g_filmOverlayActive = 0;
			Hud_SyncLocalSoftwareHudMasks(1);
			fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
		}
	}

	goto process_film_overlay_input;

clear_film_aim_target:
	if (g_filmOverlayActive != 1) {
		return 1;
	}
	Hud_MarkFilmOverlayElementsVisible();
	fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
	if (g_filmOverlayViewState.cameraFocusObjIdx != 0xffff) {
		g_filmOverlayViewState.cameraDistance =
			g_modelTypeTable[g_objectTable[g_filmOverlayViewState.cameraFocusObjIdx].objectType]
				.maxBoundsExtent +
			512;
		g_filmOverlayViewState.hudAimX = (uint16_t)(int16_t)-2790;
		g_filmOverlayViewState.hudAimY = 0;
	}
	g_filmOverlayViewState.aimTargetIdx = 0xffff;

process_film_overlay_input:
	if (g_filmOverlayActive == 1) {
		int hudAimXDelta;
		int hudAimYDelta;
		int16_t scaledPitch;
		int scaledRoll;
		int16_t scaledYaw;
		int16_t keyMods;
		int16_t distanceMode;
		int16_t distanceStep;

		scaledYaw = (int16_t)(120 * g_currentInputFrame.axisX);
		scaledPitch = (int16_t)(50 * g_currentInputFrame.axisY);
		scaledRoll = 120 * g_currentInputFrame.axisR;
		keyMods = (int8_t)g_currentInputFrame.keyMods;
		(void)scaledRoll;
		if ((int16_t)((uint16_t)scaledYaw >= 0x8000u ? -scaledYaw : scaledYaw) <= 64) {
			scaledYaw = 0;
		}
		if ((int16_t)((uint16_t)scaledPitch >= 0x8000u ? -scaledPitch : scaledPitch) <= 24) {
			scaledPitch = 0;
		}

		hudAimYDelta = MATH2_ABoverC32(scaledYaw, (uint16_t)g_elapsedTicks, 236);
		hudAimXDelta = MATH2_ABoverC32((int16_t)(2 * scaledPitch), (uint16_t)g_elapsedTicks, 236);
		if (g_filmOverlayViewState.cameraFocusObjIdx != 0xffff) {
			g_filmOverlayViewState.hudAimY += (uint16_t)hudAimYDelta;
			g_filmOverlayViewState.hudAimX += (uint16_t)hudAimXDelta;
		} else if ((int16_t)hudAimXDelta != 0 || (int16_t)hudAimYDelta != 0) {
			FlightView_RotateFilmOverlayFreeCameraByInput((int16_t)hudAimXDelta,
														  (int16_t)-(int16_t)hudAimYDelta, g_localPlayer);
		}

		distanceMode = keyMods & 0xf;
		if (distanceMode != 1 && distanceMode != 2) {
			g_filmOverlayViewState.cameraDistanceStep = 32;
			return 1;
		}

		distanceStep = (int16_t)(g_filmOverlayViewState.cameraDistanceStep + 32);
		g_filmOverlayViewState.cameraDistanceStep = (uint16_t)distanceStep;
		if (g_filmOverlayViewState.cameraFocusObjIdx == 0xffff) {
			int moveStep;

			if ((uint16_t)distanceStep > 0x2000u) {
				distanceStep = 0x2000;
				g_filmOverlayViewState.cameraDistanceStep = 0x2000;
			}
			if (distanceMode == 1) {
				moveStep = (int16_t)MATH2_ABoverC32(distanceStep, (uint16_t)g_elapsedTicks, 236);
				g_filmOverlayViewState.savedTargetX += Xwa_Q15MulReuseFirstSlot(moveStep, g_camMatR2_X);
				moveStep = (int16_t)MATH2_ABoverC32(distanceStep, (uint16_t)g_elapsedTicks, 236);
				g_filmOverlayViewState.savedTargetY += Xwa_Q15MulReuseFirstSlot(moveStep, g_camMatR2_Y);
				moveStep = (int16_t)MATH2_ABoverC32(distanceStep, (uint16_t)g_elapsedTicks, 236);
				g_filmOverlayViewState.savedTargetZ += Xwa_Q15MulReuseFirstSlot(moveStep, g_camMatR2_Z);
			} else {
				moveStep = (int16_t)MATH2_ABoverC32(distanceStep, (uint16_t)g_elapsedTicks, 236);
				g_filmOverlayViewState.savedTargetX -= Xwa_Q15MulReuseFirstSlot(moveStep, g_camMatR2_X);
				moveStep = (int16_t)MATH2_ABoverC32(distanceStep, (uint16_t)g_elapsedTicks, 236);
				g_filmOverlayViewState.savedTargetY -= Xwa_Q15MulReuseFirstSlot(moveStep, g_camMatR2_Y);
				moveStep = (int16_t)MATH2_ABoverC32(distanceStep, (uint16_t)g_elapsedTicks, 236);
				g_filmOverlayViewState.savedTargetZ -= Xwa_Q15MulReuseFirstSlot(moveStep, g_camMatR2_Z);
			}

			if (g_filmOverlayViewState.savedTargetX < -0x1000000) {
				g_filmOverlayViewState.savedTargetX = -0x1000000;
			}
			if (g_filmOverlayViewState.savedTargetX > 0x1000000) {
				g_filmOverlayViewState.savedTargetX = 0x1000000;
			}
			if (g_filmOverlayViewState.savedTargetY < -0x1000000) {
				g_filmOverlayViewState.savedTargetY = -0x1000000;
			}
			if (g_filmOverlayViewState.savedTargetY > 0x1000000) {
				g_filmOverlayViewState.savedTargetY = 0x1000000;
			}
			if (g_filmOverlayViewState.savedTargetZ < -0x1000000) {
				g_filmOverlayViewState.savedTargetZ = -0x1000000;
			}
			if (g_filmOverlayViewState.savedTargetZ > 0x1000000) {
				g_filmOverlayViewState.savedTargetZ = 0x1000000;
			}
			return 1;
		}

		if ((uint16_t)distanceStep > 0x400u) {
			distanceStep = 0x400;
			g_filmOverlayViewState.cameraDistanceStep = 0x400;
		}
		if (distanceMode == 1) {
			g_filmOverlayViewState.cameraDistance -=
				(int16_t)MATH2_ABoverC32(distanceStep, (uint16_t)g_elapsedTicks, 236);
			if (g_filmOverlayViewState.cameraDistance < 80) {
				g_filmOverlayViewState.cameraDistance = 80;
			}
		} else {
			g_filmOverlayViewState.cameraDistance +=
				(int16_t)MATH2_ABoverC32(distanceStep, (uint16_t)g_elapsedTicks, 236);
			if (g_filmOverlayViewState.cameraDistance > 0x2000) {
				g_filmOverlayViewState.cameraDistance = 0x2000;
			}
		}
	}

	return 1;
}

// FUNCTION: XWA 0x4ED7F0
int FlightNet_SampleAndSendInput(void) {
	int playerIdx;
	int packetLen;
	int deltaCode;
	int framePayloadLen;
	int validFrame;
	char axisRByte;
	FILE* logFile;
	uint8_t* packetBytes;
	uint8_t* batchBytes;
	InputFrame* inserted;

	FlightInput_Read(-2);

	g_currentInputFrame.key = g_actionKey;
	g_currentInputFrame.axisX = (int8_t)((uint8_t)g_ctrlAxisX & 0xfeu);
	g_currentInputFrame.axisY = (int8_t)((uint8_t)g_ctrlAxisY & 0xfeu);
	g_currentInputFrame.axisR = (int8_t)((uint8_t)g_ctrlAxisR & 0xfeu);
	g_currentInputFrame.keyMods = (uint8_t)g_keyMods & 3u;
	validFrame = 1;
	g_flightNetScratchPacket[0] = (uint32_t)validFrame;
	g_inputTimestamp += (int)Time_GetFrameDelta();
	deltaCode = g_inputTimestamp - g_lastFrameTime;
	if (deltaCode >= 127 || deltaCode < 0 || g_lastFrameTime == 0) {
		deltaCode = 127;
	} else {
		deltaCode = g_inputTimestamp & 0x7f;
	}
	if (g_lastFrameTime - g_lastKeyframeTime > 236) {
		deltaCode = 127;
	}

	packetBytes = (uint8_t*)g_flightNetScratchPacket;
	if (deltaCode == 127) {
		g_lastKeyframeTime = g_inputTimestamp;
		packetBytes[4] = (uint8_t)deltaCode;
		memcpy(packetBytes + 5, &g_inputTimestamp, sizeof(g_inputTimestamp));
		packetLen = 9;
	} else {
		packetBytes[4] = (uint8_t)deltaCode;
		packetLen = 5;
	}

	if (g_currentInputFrame.key != 0) {
		packetBytes[4] = (uint8_t)(deltaCode | 0x80);
		packetBytes[packetLen] = (uint8_t)g_currentInputFrame.key;
		++packetLen;
	}
	g_lastFrameTime = g_inputTimestamp;

	packetBytes[packetLen] = (uint8_t)g_currentInputFrame.axisX;
	packetBytes[packetLen + 1] = (uint8_t)g_currentInputFrame.axisY;
	packetBytes[packetLen + 2] = (uint8_t)g_currentInputFrame.axisR;
	if ((g_currentInputFrame.keyMods & 1u) != 0) {
		packetBytes[packetLen] |= 1u;
	}
	if ((g_currentInputFrame.keyMods & 2u) != 0) {
		packetBytes[packetLen + 1] |= 1u;
	}
	axisRByte = packetBytes[packetLen + 2];
	axisRByte = (uint8_t)(axisRByte | (uint8_t)(g_currentInputFrame.key > 0xffu));
	packetLen += 3;
	packetBytes[packetLen - 1] = axisRByte;

	if (g_flightPlayerCount > validFrame) {
		if (g_inputLogEnabled == validFrame) {
			logFile = g_inputLogFile;
			if (logFile != NULL ||
				(logFile = fopen("inputlog.txt", "w"), g_inputLogFile = logFile, logFile != NULL)) {
				fprintf(logFile, "%8x %4x %2x %2x %2x\n", g_flightNetScratchPacket[1],
						g_currentInputFrame.key, (uint8_t)g_currentInputFrame.axisX,
						(uint8_t)g_currentInputFrame.axisY, g_currentInputFrame.keyMods);
				fflush(g_inputLogFile);
			}
		}
		if (!g_asyncFlag) {
			for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
				if (g_players[playerIdx].connectedFlag &&
					(g_playerConnected[playerIdx] != 0 || playerIdx == g_localPlayer)) {
					NetSession_SendPacket(g_players[playerIdx].network.directPlayId, g_flightNetScratchPacket,
										  packetLen);
				}
			}
		} else {
			batchBytes = (uint8_t*)g_flightNetInputDeltaBatchPacket;
			++batchBytes[4];
			g_flightNetInputDeltaBatchPacket[0] = 22;
			framePayloadLen = packetLen - 4;
			memcpy(batchBytes + g_flightNetInputDeltaBatchLen, packetBytes + 4, (size_t)framePayloadLen);
			packetLen += g_flightNetInputDeltaBatchLen - 4;
			g_flightNetInputDeltaBatchLen = packetLen;
			if ((uint32_t)(g_inputTimestamp - g_flightNetLastInputBatchSendTime) >
				(uint32_t)g_flightNetInputBatchIntervalTicks) {
				g_flightNetLastInputBatchSendTime = g_inputTimestamp;
				NetSession_SendPacket(NetSession_GetHostDplayId(), g_flightNetInputDeltaBatchPacket,
									  packetLen);
				if (g_activeFlightPlayerCount < g_flightNetSmallSessionPlayerThreshold) {
					PlayerData* playerCursor;
					int* connectedCursor;

					playerIdx = 0;
					playerCursor = g_players;
					connectedCursor = g_playerConnected;
					do {
						int destPlayerId;

						if (playerCursor->connectedFlag && playerIdx != g_localPlayer) {
							destPlayerId = playerCursor->network.directPlayId;
							if (destPlayerId != NetSession_GetHostDplayId() && *connectedCursor != 0) {
								NetSession_SendPacket(destPlayerId, g_flightNetInputDeltaBatchPacket,
													  g_flightNetInputDeltaBatchLen);
							}
						}
						++connectedCursor;
						++playerIdx;
						++playerCursor;
					} while (connectedCursor < g_playerConnected + XWA_PLAYER_COUNT);
				}
				g_flightNetInputDeltaBatchLen = 5;
				g_flightNetInputDeltaBatchPacket[0] = 22;
				batchBytes[4] = 0;
			}
		}
	}

	inserted = FlightNet_InsertInputFrame(g_localPlayer, g_inputTimestamp, &g_currentInputFrame);
	if (inserted != NULL) {
		inserted->applied = 0;
		inserted->valid = validFrame;
#ifdef XWA_MODERN
		FlightDebug_CaptureJoystickInputSample(g_inputTimestamp);
#endif
	}

	return Flight_PumpWindowMessages();
}

// FUNCTION: XWA 0x5117F0
// Decide whether the flight loop should continue, return to the hangar, restart/debrief,
// or save a recorded film, based on film playback/record state, end-of-mission flags, and
// player connection counts. Updates g_flightExitRequest / g_flightMissionEndPending and
// returns the resulting g_flightMissionEndPending value.
int Flight_CheckMissionEndAndExitRequest(void) {
	int connectedOrPendingCount = 0;
	int activeConnectedCount;
	int missionEndPending;
	int p;

	g_activeFlightPlayerCount = connectedOrPendingCount;
	activeConnectedCount = 0;
	if (g_filmPlaybackMode == 4) {
		g_flightMissionEndPending = 1;
		missionEndPending = g_flightMissionEndPending;
		g_flightExitRequest = 1;
	} else if (g_filmPlaybackMode == 3) {
		g_flightMissionEndPending = 1;
		missionEndPending = g_flightMissionEndPending;
		g_flightExitRequest = 2;
	} else {
		for (p = 0; p < XWA_PLAYER_COUNT; ++p) {
			uint8_t state = g_players[p].connectedFlag;
			if (state == 1 || state == 2) {
				++connectedOrPendingCount;
			}
			if (state == 1) {
				++activeConnectedCount;
			}
		}
		g_activeFlightPlayerCount = connectedOrPendingCount;
		if (!activeConnectedCount) {
			g_flightMissionEndPending = 1;
		}
		if (!g_players[g_localPlayer].connectedFlag) {
			g_flightMissionEndPending = 1;
		}
		if (g_flightReturnToFrontendRequested || !g_flightMissionEndPending || g_flightPlayerCount != 1) {
			int result = g_flightMissionEndPending;

			return (uint8_t)result;
		}
		if (g_hangarBackdropModelType == 179) {
			uint8_t goalStatus =
				g_missionFlightRuntimeState
					.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff][TEAM_GOAL_PRIMARY];
			int result = g_flightMissionEndPending;

			g_flightExitRequest = (goalStatus != 1) + 1;
			return (uint8_t)result;
		}
		if (g_inHangarReady) {
			g_flightMissionEndPending = 0;
			return (uint8_t)g_flightMissionEndPending;
		}

		if (g_missionHeader.body.hangar != XWA_HANGAR_QUICKSTART &&
			g_missionHeader.body.hangar != XWA_HANGAR_SKIRMISH &&
			g_missionHeader.body.hangar != XWA_HANGAR_DEATHSTAR) {
			{
				unsigned missionDescriptionId = (unsigned)g_pilotData.missionDescriptionIds[4];
				int campaignMode = g_pilotData.campaignMode;

				if (campaignMode && missionDescriptionId >= 0x31u && missionDescriptionId <= 0x33u) {
					int result = g_flightMissionEndPending;

					g_flightExitRequest = 1;
					return (uint8_t)result;
				}
			}
			if (g_provingGroundsModeActive && g_pilotData.missionDirectoryId == 1 &&
				(unsigned)g_pilotData.missionDescriptionIds[1] >= 0x3Au &&
				(unsigned)g_pilotData.missionDescriptionIds[1] <= 0x41u) {
				int result = g_flightMissionEndPending;

				g_flightExitRequest = 1;
				return (uint8_t)result;
			}

			if (g_filmRecording) {
				Film_FlushWriteBuffer();
#ifdef XWA_MODERN
				File_Close(g_filmFile);
#else
				fclose((FILE*)g_filmFile);
#endif
				g_filmFile = NULL;
				g_filmRecording = 0;
				g_inputTimestamp = Time_GetFrameDelta() + g_inputTimestamp;
				Sound_StopAllInstances();
				Music_PauseIfInitialized();
				FlightFilm_SaveTempRecordingWithPrompt();
				Music_ResumeIfInitialized();
				Time_GetFrameDelta();
			}
			if (g_filmPlaybackMode == 2) {
				g_flightMissionEndPending = 1;
				g_flightExitRequest = 2;
				return (uint8_t)g_flightMissionEndPending;
			}
#ifdef XWA_MODERN
			Hangar_BeginEnterCraft(0xFFFFu);
#else
			Hangar_EnterCraft(0xFFFFu);
#endif
			return (uint8_t)g_flightMissionEndPending;
		}

		if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR &&
			g_missionFlightRuntimeState
					.teamGoalStatus[(uint16_t)g_players[g_localPlayer].playerIff][TEAM_GOAL_PRIMARY] != 1) {
			g_flightExitRequest = 2;
		}
		missionEndPending = g_flightMissionEndPending;
	}
	return (uint8_t)missionEndPending;
}

// FUNCTION: XWA 0x4F9110
void FlightNet_HandleServerChecksumPacket(const uint32_t* packet) {
	int i;
	int mismatch;

	if (NetSession_GetLocalPlayerId() != 0) {
		return;
	}
	if (packet[1] != g_flightNetWorldChecksumEpoch) {
		return;
	}

	packet += 2;
	mismatch = 0;
	for (i = 0; i < 16; ++i) {
		if (packet[i] != g_worldChecksum[i]) {
			DebugPrintfChannel(0x20000,
							   "Server checksum[%d] (%lx) is different from world checksum[%d] (%lx).\n", i,
							   (unsigned long)packet[i], i, (unsigned long)g_worldChecksum[i]);
			mismatch = 1;
		}
	}

	if (!mismatch) {
		g_flightNetBufferWorldMessagesUntilChecksum = 0;
		g_worldMessageBufferBytesFree = g_worldMessageBufferCapacity;
		g_worldMessageBufferedCount = 0;
	}
}

// FUNCTION: XWA 0x4D4640
int sub_4D4640(void) {
	g_unusedFlightResumeResetSlot0 = -1;
	g_unusedFlightResumeResetSlot1 = -1;
	return -1;
}

// FUNCTION: XWA 0x5117C0
void Flight_UpdateActivePlayerCount(void) {
	int i;

	g_activeFlightPlayerCount = 0;
	for (i = 0; i < XWA_PLAYER_COUNT; ++i) {
		if (g_players[i].connectedFlag == 1 || g_players[i].connectedFlag == 2) {
			++g_activeFlightPlayerCount;
		}
	}
}

// FUNCTION: XWA 0x4F9300
void FlightNet_ResetWorldMessageBufferCursor(void) {
	g_worldMessageBufferedCount = 0;
	g_worldMessageBufferBytesFree = g_worldMessageBufferCapacity;
}

// FUNCTION: XWA 0x4F8450
void FlightNet_ResetRemotePlayerRenderSmoothing(void) {
	int i;

	for (i = 0; i < 8; ++i) {
		g_remotePlayerRenderSamples[i].valid = 0;
		g_remotePlayerSavedRenderPoses[i].valid = 0;
	}
}

// FUNCTION: XWA 0x4F3D20
void Flight_UpdateTimers(void) {
	unsigned int timerIdx;
	unsigned int playerIdx;

	{
		uint16_t (*transientTimers)[sizeof(PlayerFlightTransientTimers) / sizeof(uint16_t)];

		transientTimers = (uint16_t (*)[sizeof(PlayerFlightTransientTimers) / sizeof(uint16_t)])
			g_playerFlightTransientTimers;

		for (timerIdx = 0; timerIdx < 12; ++timerIdx) {
			if (g_flightGlobalCountdownTimers[timerIdx]) {
				g_flightGlobalCountdownTimers[timerIdx] -= (uint16_t)g_elapsedTicks;
				if ((int16_t)g_flightGlobalCountdownTimers[timerIdx] < 0) {
					g_flightGlobalCountdownTimers[timerIdx] = 0;
				}
			}
		}

		if (!g_flightSimSideEffectsSuppressed) {
			for (timerIdx = 0; timerIdx < (int)(sizeof(PlayerFlightTransientTimers) / 2u); ++timerIdx) {
				for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
					if (g_players[playerIdx].connectedFlag) {
						if (transientTimers[playerIdx][timerIdx]) {
							transientTimers[playerIdx][timerIdx] -= (uint16_t)g_elapsedTicks;
							if ((int16_t)transientTimers[playerIdx][timerIdx] < 0) {
								transientTimers[playerIdx][timerIdx] = 0;
							}
						}
					}
				}
			}
		}

		for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
			if (g_players[playerIdx].pendingActionTimer) {
				g_players[playerIdx].pendingActionTimer -= (uint16_t)g_elapsedTicks;
				if (g_players[playerIdx].pendingActionTimer < 0) {
					g_players[playerIdx].pendingActionTimer = 0;
				}
			}
			if (g_players[playerIdx].beamFireCooldownTimer) {
				g_players[playerIdx].beamFireCooldownTimer -= (uint16_t)g_elapsedTicks;
				if (g_players[playerIdx].beamFireCooldownTimer < 0) {
					g_players[playerIdx].beamFireCooldownTimer = 0;
				}
			}
		}

		g_missionElapsedClock.subsecondTicks -= (uint16_t)g_elapsedTicks;
	}

	if (g_missionElapsedClock.subsecondTicks <= 0) {
		unsigned int localPlayerIdx;
		uint8_t hangar;
		int8_t countdownTick;
		int completedChallengeOrSkirmishGoal;
		int imposeTimeLimitMessage;

		g_missionElapsedClock.subsecondTicks += 236;

		if (++g_missionElapsedClock.seconds >= 60u) {
			g_missionElapsedClock.seconds = 0;
			if (++g_missionElapsedClock.minutes >= 60u) {
				g_missionElapsedClock.minutes = 0;
				++g_missionElapsedClock.hours;
				if (g_missionElapsedClock.hours >= 24u) {
					g_missionElapsedClock.hours = 0;
				}
			}
		}

		localPlayerIdx = (unsigned int)g_localPlayer;
		hangar = g_missionHeader.body.hangar;

		if (g_missionCountdownClock.minutes || g_missionCountdownClock.seconds) {
			countdownTick = (int8_t)(g_missionCountdownClock.seconds - 1u);
			g_missionCountdownClock.seconds = (uint8_t)countdownTick;
			if (countdownTick == -1) {
				--g_missionCountdownClock.minutes;
				countdownTick = 59;
				g_missionCountdownClock.seconds = 59;
				if ((int8_t)g_missionCountdownClock.minutes == -1) {
					countdownTick = 0;
					g_missionCountdownClock.seconds = 0;
					g_missionCountdownClock.minutes = 0;
				}
			}

			if (g_missionTimeLimitActive && !g_missionCountdownClock.minutes && !countdownTick &&
				!g_flightSimSideEffectsSuppressed) {
				for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
					if (g_players[playerIdx].connectedFlag) {
						g_players[playerIdx].connectedFlag = 2;
					}
				}

				g_flightMissionEndPending = 1;
				if (hangar != XWA_HANGAR_QUICKSTART && hangar != XWA_HANGAR_SKIRMISH &&
					hangar != XWA_HANGAR_DEATHSTAR &&
					g_missionFlightRuntimeState.teamGoalStatus[(uint16_t)g_players[localPlayerIdx].playerIff]
															  [TEAM_GOAL_PRIMARY] != 1) {
					g_missionFlightRuntimeState
						.teamGoalStatus[(uint16_t)g_players[localPlayerIdx].playerIff][TEAM_GOAL_SECONDARY] =
						1;
				}
			}
		}

		if (g_missionTimeLimitActive) {
			if (g_missionCountdownClock.minutes == 2 && g_missionCountdownClock.seconds == 0) {
				fsfx_PlaySound(59, 0xffffu, localPlayerIdx);
				msg_emitInFlightMessage(MSG_TWO_MIN_WARNING, g_localPlayer);
				localPlayerIdx = (unsigned int)g_localPlayer;
			}

			if (g_missionCountdownClock.minutes == 1 && g_missionCountdownClock.seconds == 0) {
				fsfx_PlaySound(128, 0xffffu, localPlayerIdx);
				msg_emitInFlightMessage(MSG_ONE_MIN_WARNING, g_localPlayer);
				localPlayerIdx = (unsigned int)g_localPlayer;
			}

			if (g_missionCountdownClock.minutes == 0) {
				if (g_missionCountdownClock.seconds == 15) {
					int playerObjectIdx;
					int isFighterCraft;

					playerObjectIdx = g_players[localPlayerIdx].objectIndex;
					if (playerObjectIdx != 0xffff) {
						uint16_t objectType;

						objectType = g_objectTable[playerObjectIdx].objectType;
						isFighterCraft = objectType == OBJ_XWing || objectType == OBJ_YWing ||
										 objectType == OBJ_AWing || objectType == OBJ_Z95 ||
										 objectType == OBJ_BWing;
					} else {
						isFighterCraft = 0;
					}
					if (isFighterCraft) {
						fsfx_PlaySound(122, 0xffffu, localPlayerIdx);
					} else {
						fsfx_PlaySound(128, 0xffffu, localPlayerIdx);
						fsfx_PlaySound(128, 0xffffu, g_localPlayer);
					}
					localPlayerIdx = (unsigned int)g_localPlayer;
				}

				if (g_missionCountdownClock.minutes == 0 && g_missionCountdownClock.seconds == 2) {
					msg_emitInFlightMessage(MSG_TIME_OUT, (int)localPlayerIdx);
				}
			}
		}

		if (g_missionHeader.body.hangar != XWA_HANGAR_QUICKSTART &&
			g_missionHeader.body.hangar != XWA_HANGAR_JUNKYARD &&
			g_missionHeader.body.hangar != XWA_HANGAR_SKIRMISH &&
			g_missionFlightRuntimeState.teamGoalStatus[0][TEAM_GOAL_PRIMARY] == 1 &&
			g_missionFlightRuntimeState.teamGoalStatus[0][TEAM_GOAL_SECONDARY] == 0 &&
			g_missionHeader.body.endMissionWhenComplete && !g_teamVictoryTimeLimitStarted) {
			if (g_missionCountdownClock.minutes || g_missionCountdownClock.seconds > 10u) {
				g_missionCountdownClock.minutes = 0;
				g_missionCountdownClock.seconds = 10;
			} else {
				g_flightMissionEndPending = 1;
			}
			g_missionTimeLimitActive = 1;
			g_teamVictoryTimeLimitStarted = 1;
		}

		completedChallengeOrSkirmishGoal = 0;
		if (g_provingGroundsModeActive) {
			int playerIdx;

			for (playerIdx = 0; playerIdx < g_flightPlayerCount; ++playerIdx) {
				if (g_yardContext.playerChallengeStates[playerIdx].finished) {
					completedChallengeOrSkirmishGoal = 1;
					break;
				}
			}
		} else if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
			int teamIdx;

			for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
				if (g_missionFlightRuntimeState.teamGoalStatus[teamIdx][TEAM_GOAL_PRIMARY] == 1) {
					completedChallengeOrSkirmishGoal = 1;
					break;
				}
			}
		}

		imposeTimeLimitMessage = 0;
		if (g_flightPlayerCount > 1 && g_teamVictoryTimeLimitMinutes && !g_teamVictoryTimeLimitStarted &&
			(g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH || g_provingGroundsModeActive)) {
			if ((uint8_t)g_missionCountdownClock.minutes > g_teamVictoryTimeLimitMinutes ||
				(g_missionCountdownClock.minutes == 0 && g_missionCountdownClock.seconds == 0)) {
				uint8_t presentTeams[10];
				int presentTeamCount;
				unsigned int presentTeamIdx;

				memset(presentTeams, 0, sizeof(presentTeams));
				for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
					if (g_players[playerIdx].connectedFlag == 1) {
						presentTeams[(uint16_t)g_players[playerIdx].playerIff] = 1;
					}
				}

				if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART) {
					uint32_t objectIdx;

					for (objectIdx = 0; objectIdx < g_objectTableSlotCount; ++objectIdx) {
						ObjectRecord* object;

						object = &g_objectTable[objectIdx];
						if (object->objectType != OBJ_None && object->mobj != NULL &&
							object->mobj->pCraft != NULL &&
							g_missionFlightGroups[object->flightGroupIdx].fg.playerNumber) {
							presentTeams[g_missionFlightGroups[object->flightGroupIdx].fg.team] = 1;
						}
					}
				}

				presentTeamCount = 0;
				for (playerIdx = 0; playerIdx < 10; ++playerIdx) {
					if (presentTeams[playerIdx]) {
						++presentTeamCount;
						presentTeamIdx = (unsigned int)playerIdx;
					}
				}

				if (presentTeamCount == 1 &&
					(g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
					 g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) &&
					(g_missionFlightRuntimeState.teamGoalStatus[presentTeamIdx][TEAM_GOAL_PRIMARY] == 1 ||
					 g_missionFlightRuntimeState.teamGoalStatus[presentTeamIdx][TEAM_GOAL_PRIMARY] == 2 ||
					 g_missionFlightRuntimeState.teamGoalStatus[presentTeamIdx][TEAM_GOAL_SECONDARY] == 1)) {
					g_missionCountdownClock.seconds = 0;
					g_missionCountdownClock.minutes = g_teamVictoryTimeLimitMinutes;
					g_missionTimeLimitActive = g_teamVictoryTimeLimitMinutes;
					g_teamVictoryTimeLimitStarted = 1;
					imposeTimeLimitMessage = 1;
				}
			}

			if (completedChallengeOrSkirmishGoal) {
				g_missionCountdownClock.seconds = 0;
				g_missionCountdownClock.minutes = g_teamVictoryTimeLimitMinutes;
				g_missionTimeLimitActive = g_teamVictoryTimeLimitMinutes;
				g_teamVictoryTimeLimitStarted = 1;
				imposeTimeLimitMessage = 1;
			}
		}

		if (imposeTimeLimitMessage) {
			g_msgArgTable[0] = (uint8_t)g_missionTimeLimitActive;
			if (g_missionTimeLimitActive == 1) {
				msg_emitInFlightMessage(MSG_TIME_LIMIT_IMPOSED_1, g_localPlayer);
			} else {
				msg_emitInFlightMessage(MSG_TIME_LIMIT_IMPOSED_2, g_localPlayer);
			}
			fsfx_PlaySound(128, 0xffffu, g_localPlayer);
		}

		for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
			PlayerData* player;

			player = &g_players[playerIdx];
			if (player->connectedFlag && player->objectIndex != 0xffff) {
				CraftData* craft;
				uint16_t selectedSystemIdx;
				uint16_t selectedDisplaySlot;
				unsigned int systemIdx;

				selectedSystemIdx = 0xffffu;
				selectedDisplaySlot = 0xffffu;
				craft = g_objectTable[player->objectIndex].mobj->pCraft;
				if (craft->workingSubsystems) {
					uint8_t* systemDisplaySlots;

					systemDisplaySlots = craft->systemDisplaySlotBySystem;
					for (systemIdx = 0; systemIdx < 10; ++systemIdx) {
						if (craft->systemHealth[systemIdx] == 0 &&
							systemDisplaySlots[systemIdx] < selectedDisplaySlot) {
							selectedDisplaySlot = systemDisplaySlots[systemIdx];
							selectedSystemIdx = (uint16_t)systemIdx;
						}
					}

					for (systemIdx = 0; systemIdx < 10; ++systemIdx) {
						if (craft->systemHealth[systemIdx] == 0 && systemIdx == selectedSystemIdx) {
							if (!craft->systemTimer[systemIdx]) {
								craft->systemHealth[systemIdx] = 100;
								craft->workingSubsystems |= g_subsystemIdToFlag[systemIdx];
								g_msgArgTable[0] = g_subsystemMessageArgById[systemIdx];
								g_msgArgTable[1] = MSG_REPAIRED;
								msg_emitInFlightMessage(MSG_SYSTEMCOND, playerIdx);
								fsfx_PlaySound(125, 0xffffu, (unsigned int)playerIdx);
							} else {
								--craft->systemTimer[systemIdx];
							}
						}
					}
				}

				if (player->currentTargetObjectIdx != 0xffffu) {
					++player->targetSubState;
				}

				if (player->hyperspaceRuntime.hyperBuoyPromptCooldown) {
					--player->hyperspaceRuntime.hyperBuoyPromptCooldown;
				}

				if (craft->lastReleasedObjectIdx != 0xffffu && craft->releaseClearTimer) {
					--craft->releaseClearTimer;
					if (craft->releaseClearTimer == 0) {
						craft->lastReleasedObjectIdx = 0xffffu;
					}
				}
			}
		}

		{
			uint32_t objectIdx;

			for (objectIdx = 0; objectIdx < g_objectTableSlotCount; ++objectIdx) {
				if (g_objectTable[objectIdx].objectType != OBJ_None &&
					g_objectTable[objectIdx].mobj != NULL) {
					++g_objectTable[objectIdx].mobj->framesAlive;
				}
			}
		}

		Hud_AdvanceFlightMessagePaneTimers();
	}
}

// FUNCTION: XWA 0x4F8470
void FlightNet_CaptureRemotePlayerRenderSamples(void) {
	int playerIdx;

	if (!g_remotePlayerRenderSmoothingEnabled) {
		return;
	}

	for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
		PlayerData* player;
		RemotePlayerRenderSample* sample;
		int previousValid;
		ObjectRecord* obj;
		MobileObject* mobj;

		player = &g_players[playerIdx];
		sample = &g_remotePlayerRenderSamples[playerIdx];
		previousValid = sample->valid;
		sample->valid = 0;

		if (!player->connectedFlag || playerIdx == g_localPlayer || player->objectIndex == 0xffff) {
			continue;
		}

		obj = &g_objectTable[player->objectIndex];
		if (obj->objectType == OBJ_None) {
			continue;
		}

		mobj = obj->mobj;
		if (mobj == NULL) {
			continue;
		}

		if (!previousValid) {
			sample->roll = (int16_t)obj->roll;
			sample->pitch = (int16_t)obj->pitch;
			sample->yaw = (int16_t)obj->yaw;
		}

		sample->valid = 1;
		sample->objectSignature = obj->objectSignature;
		sample->worldX = obj->world_x;
		sample->worldY = obj->world_y;
		sample->worldZ = obj->world_z;
		sample->rollDelta = (int)obj->roll - (uint16_t)sample->roll;
		sample->pitchDelta = (int)obj->pitch - (uint16_t)sample->pitch;
		sample->yawDelta = (int)obj->yaw - (uint16_t)sample->yaw;
		sample->roll = (int16_t)obj->roll;
		sample->pitch = (int16_t)obj->pitch;
		sample->yaw = (int16_t)obj->yaw;

		if (mobj->moveVectorDirty) {
			FVIEW_calcrotatemove(obj->pitch, obj->yaw, obj);
		}

		sample->moveX = mobj->moveX;
		sample->moveY = mobj->moveY;
		sample->moveZ = mobj->moveZ;
		sample->speedMagnitude = mobj->speed;
		sample->simStateTimestamp = mobj->simStateTimestamp;

		if (g_remotePlayerSavedRenderPoses[playerIdx].valid) {
			RemotePlayerRenderSample* saved;

			saved = &g_remotePlayerSavedRenderPoses[playerIdx];
			obj->roll = (uint16_t)saved->roll;
			obj->pitch = (uint16_t)saved->pitch;
			obj->yaw = (uint16_t)saved->yaw;
			obj->world_x = saved->worldX;
			obj->world_y = saved->worldY;
			obj->world_z = saved->worldZ;
		}
	}
}

// FUNCTION: XWA 0x4F8670
void FlightNet_ApplyRemotePlayerRenderSmoothing(void) {
	int playerIdx;
	PlayerData* player;
	int playerSlot;

	if (!g_remotePlayerRenderSmoothingEnabled) {
		return;
	}

	playerSlot = 0;
	player = g_players;
	playerIdx = 0;
	for (; (intptr_t)player < (intptr_t)&g_players[XWA_PLAYER_COUNT]; ++player, ++playerIdx, ++playerSlot) {
		ObjectRecord* obj;
		int sampleWorldX;
		int sampleWorldY;
		int sampleWorldZ;
		int elapsedTicks;
		int extrapolatedTicks;
		int deltaX;
		int deltaY;
		int deltaZ;
		int roughDistance;
		int blendFrac;
		int maxAngleStep;

		g_remotePlayerSavedRenderPoses[playerIdx].valid = 0;
		if (!player->connectedFlag || player->objectIndex == 0xffff || player->hasCheckpointFlag) {
			continue;
		}

		obj = &g_objectTable[player->objectIndex];
		if (obj->objectType == OBJ_None) {
			continue;
		}

		if (obj->mobj == NULL) {
			continue;
		}

		if (!g_remotePlayerRenderSamples[playerIdx].valid || playerSlot == g_localPlayer ||
			player->boundObjectSignature != g_remotePlayerRenderSamples[playerIdx].objectSignature) {
			continue;
		}

		sampleWorldX = g_remotePlayerRenderSamples[playerIdx].worldX;
		g_remotePlayerSavedRenderPoses[playerIdx].roll = (int16_t)obj->roll;
		g_remotePlayerSavedRenderPoses[playerIdx].pitch = (int16_t)obj->pitch;
		g_remotePlayerSavedRenderPoses[playerIdx].yaw = (int16_t)obj->yaw;
		g_remotePlayerSavedRenderPoses[playerIdx].worldX = obj->world_x;
		g_remotePlayerSavedRenderPoses[playerIdx].worldY = obj->world_y;
		sampleWorldZ = obj->world_z;
		sampleWorldY = g_remotePlayerRenderSamples[playerIdx].worldY;
		g_remotePlayerSavedRenderPoses[playerIdx].worldZ = sampleWorldZ;
		sampleWorldZ = g_remotePlayerRenderSamples[playerIdx].worldZ;
		g_remotePlayerSavedRenderPoses[playerIdx].valid = 1;

		elapsedTicks =
			obj->mobj->simStateTimestamp - g_remotePlayerRenderSamples[playerIdx].simStateTimestamp;
		if (elapsedTicks < 0) {
			continue;
		}

		extrapolatedTicks = 0;
		if (elapsedTicks > 0 && g_remotePlayerRenderSamples[playerIdx].speedMagnitude != 0) {
			int scaledSpeed;

			scaledSpeed = (4660 * (int)g_remotePlayerRenderSamples[playerIdx].speedMagnitude + 128) >> 8;
			extrapolatedTicks = (int)(int32_t)((uint32_t)elapsedTicks * (uint32_t)scaledSpeed) / 236;
			sampleWorldX += Xwa_Q15MulReuseFirstSlot((int)g_remotePlayerRenderSamples[playerIdx].moveX,
													 extrapolatedTicks);
			sampleWorldY += Xwa_Q15MulReuseFirstSlot((int)g_remotePlayerRenderSamples[playerIdx].moveY,
													 extrapolatedTicks);
			sampleWorldZ += Xwa_Q15MulReuseFirstSlot((int)g_remotePlayerRenderSamples[playerIdx].moveZ,
													 extrapolatedTicks);
		}

		deltaX = obj->world_x - sampleWorldX;
		deltaY = obj->world_y - sampleWorldY;
		deltaZ = obj->world_z - sampleWorldZ;
		roughDistance = collide_roughdistance3d(deltaX, deltaY, deltaZ);
		if (roughDistance <= 32 * extrapolatedTicks && roughDistance != 0) {
			blendFrac = (roughDistance << 14) / (32 * extrapolatedTicks);
		} else {
			blendFrac = 0x4000;
		}

		deltaX = Xwa_Q15MulReuseFirstSlot(blendFrac, deltaX);
		deltaY = Xwa_Q15MulReuseFirstSlot(blendFrac, deltaY);
		deltaZ = Xwa_Q15MulReuseFirstSlot(blendFrac, deltaZ);
		obj->world_x = sampleWorldX + deltaX;
		obj->world_y = sampleWorldY + deltaY;
		obj->world_z = sampleWorldZ + deltaZ;

		{
			int angleDiff;
			int signedAngleDiff;
			int angleCandidate;
			int angleCurrentDiff;
			int angleLimit;
			int16_t sampleAngle;

			sampleAngle = g_remotePlayerRenderSamples[playerIdx].roll;
			angleDiff = (int)(int16_t)(obj->roll - (uint16_t)sampleAngle);
			signedAngleDiff = angleDiff;
			if (g_remotePlayerRenderSamples[playerIdx].rollDelta > 0) {
				if (angleDiff < 0) {
					angleDiff = 0;
					obj->roll = (Q16Angle)sampleAngle;
					signedAngleDiff = 0;
				}
			} else if (g_remotePlayerRenderSamples[playerIdx].rollDelta < 0) {
				if (angleDiff > 0) {
					angleDiff = 0;
					obj->roll = (Q16Angle)sampleAngle;
					signedAngleDiff = 0;
				}
			}
			if (angleDiff < 0) {
				angleDiff = -angleDiff;
			}
			maxAngleStep = (6144 * elapsedTicks) / 236;
			if (angleDiff > maxAngleStep) {
				angleCandidate = (int)g_remotePlayerRenderSamples[playerIdx].roll + signedAngleDiff;
				angleLimit = maxAngleStep;
				angleLimit <<= 3;
				angleCurrentDiff = (int)(int16_t)obj->roll - angleCandidate;
				if (angleCurrentDiff < 0) {
					angleCurrentDiff = -angleCurrentDiff;
				}
				if (angleCurrentDiff < angleLimit) {
					obj->roll = (Q16Angle)angleCandidate;
				}
			}
		}

		{
			int angleDiff;
			int signedAngleDiff;
			int angleCandidate;
			int angleCurrentDiff;
			int angleLimit;
			int sampleDelta;
			int16_t sampleAngle;

			sampleAngle = g_remotePlayerRenderSamples[playerIdx].pitch;
			sampleDelta = g_remotePlayerRenderSamples[playerIdx].pitchDelta;
			angleDiff = (int)(int16_t)(obj->pitch - (uint16_t)sampleAngle);
			signedAngleDiff = angleDiff;
			if (sampleDelta > 0) {
				if (angleDiff < 0) {
					angleDiff = 0;
					obj->pitch = (Q16Angle)sampleAngle;
					signedAngleDiff = 0;
				}
			} else if (sampleDelta < 0) {
				if (angleDiff > 0) {
					angleDiff = 0;
					obj->pitch = (Q16Angle)sampleAngle;
					signedAngleDiff = 0;
				}
			}
			if (angleDiff < 0) {
				angleDiff = -angleDiff;
			}
			if (angleDiff > maxAngleStep) {
				angleCandidate = (int)g_remotePlayerRenderSamples[playerIdx].pitch + signedAngleDiff;
				angleLimit = maxAngleStep;
				angleLimit <<= 3;
				angleCurrentDiff = (int)(int16_t)obj->pitch - angleCandidate;
				if (angleCurrentDiff < 0) {
					angleCurrentDiff = -angleCurrentDiff;
				}
				if (angleCurrentDiff < angleLimit) {
					obj->pitch = (Q16Angle)angleCandidate;
				}
			}
		}

		{
			int angleDiff;
			int signedAngleDiff;
			int angleCandidate;
			int angleCurrentDiff;
			int angleLimit;
			int sampleDelta;
			int16_t sampleAngle;

			sampleAngle = g_remotePlayerRenderSamples[playerIdx].yaw;
			sampleDelta = g_remotePlayerRenderSamples[playerIdx].yawDelta;
			angleDiff = (int)(int16_t)(obj->yaw - (uint16_t)sampleAngle);
			signedAngleDiff = angleDiff;
			if (sampleDelta > 0) {
				if (angleDiff < 0) {
					angleDiff = 0;
					obj->yaw = (Q16Angle)sampleAngle;
					signedAngleDiff = 0;
				}
			} else if (sampleDelta < 0) {
				if (angleDiff > 0) {
					angleDiff = 0;
					obj->yaw = (Q16Angle)sampleAngle;
					signedAngleDiff = 0;
				}
			}
			if (angleDiff < 0) {
				angleDiff = -angleDiff;
			}
			if (angleDiff > maxAngleStep) {
				angleCandidate = (int)g_remotePlayerRenderSamples[playerIdx].yaw + signedAngleDiff;
				angleLimit = maxAngleStep;
				angleLimit <<= 3;
				angleCurrentDiff = (int)(int16_t)obj->yaw - angleCandidate;
				if (angleCurrentDiff < 0) {
					angleCurrentDiff = -angleCurrentDiff;
				}
				if (angleCurrentDiff < angleLimit) {
					obj->yaw = (Q16Angle)angleCandidate;
				}
			}
		}
	}
}

static __inline void FlightAction_PlayEngineClick(unsigned int playerIdx) {
	fsfx_PlaySound(68, 0xffffu, playerIdx);
}

static __inline void FlightAction_PlayAcceptedClick(unsigned int playerIdx) {
	fsfx_PlaySound(67, 0xffffu, playerIdx);
}

static __inline void FlightAction_EmitSystemCondition(uint16_t systemMsgId, uint16_t conditionMsgId,
													  unsigned int playerIdx) {
	g_msgArgTable[0] = systemMsgId;
	g_msgArgTable[1] = conditionMsgId;
	msg_emitInFlightMessage(MSG_SYSTEMCOND, (int)playerIdx);
}

static __inline int FlightAction_ShieldLaserTransferStep(ObjectRecord const* obj) {
	return GetModelIndexFromType(obj->objectType) == GetModelIndexFromType(OBJ_MissileBoat) ? 32 : 4;
}

static __inline int FlightAction_TransferShieldsToLasers(unsigned int playerIdx, ObjectRecord const* obj,
														 CraftData* craft) {
	int laserDeficit;
	uint8_t slot;
	int shieldStep;
	int shieldRequested;
	int shieldRemoved;
	int chargesToAdd;
	if (g_players[playerIdx].hasCheckpointFlag) {
		return 1;
	}
	if ((craft->systemFlags & 1) == 0) {
		FlightAction_PlayEngineClick(playerIdx);
		msg_emitInFlightMessage(MSG_NOT_EQUIPPED_SHIELDS, (int)playerIdx);
		return 1;
	}

	laserDeficit = 0;
	for (slot = 0; slot < craft->laserSlotCount; ++slot) {
		if (craft->warheadData[slot].weaponType < 4) {
			laserDeficit += 127 - craft->warheadData[slot].laserCharge;
		}
	}

	shieldStep = FlightAction_ShieldLaserTransferStep(obj);
	shieldRequested = (laserDeficit > 100 ? 100 : laserDeficit) * shieldStep;
	shieldRemoved = 0;

	if (craft->shieldDistribMode == 0) {
		shieldRemoved = craft->shieldFront < shieldRequested ? craft->shieldFront : shieldRequested;
		craft->shieldFront -= shieldRemoved;
	} else if (craft->shieldDistribMode == 2) {
		shieldRemoved = craft->shieldRear < shieldRequested ? craft->shieldRear : shieldRequested;
		craft->shieldRear -= shieldRemoved;
	} else {
		int halfRequested = shieldRequested / 2;
		int frontRemoved = craft->shieldFront < halfRequested ? craft->shieldFront : halfRequested;
		int rearRemoved = craft->shieldRear < shieldRequested ? craft->shieldRear : halfRequested;
		craft->shieldFront -= frontRemoved;
		craft->shieldRear -= rearRemoved;
		shieldRemoved = frontRemoved + rearRemoved;
	}

	chargesToAdd = shieldRemoved / shieldStep;
	if (chargesToAdd != 0) {
		uint16_t attempts;
		uint8_t slot = 0;
		for (attempts = 0; chargesToAdd > 0 && attempts < 100; ++attempts) {
			if (craft->warheadData[slot].laserCharge != 127 && craft->warheadData[slot].weaponType < 4) {
				++craft->warheadData[slot].laserCharge;
			}
			--chargesToAdd;
			++slot;
			if (slot >= craft->laserSlotCount) {
				slot = 0;
			}
		}

		FlightAction_PlayAcceptedClick(playerIdx);
		msg_emitInFlightMessage(MSG_TRANSFER_TO_LASER, (int)playerIdx);
		return 1;
	}
	return 0;
}

static __inline void FlightAction_TransferAllLasersToShields(unsigned int playerIdx, ObjectRecord const* obj,
															 CraftData* craft) {
	int maxShield;
	int shieldDeficit;
	int shieldStep;
	int laserCharge;
	uint8_t slot;
	if (g_players[playerIdx].hasCheckpointFlag) {
		return;
	}
	if ((craft->systemFlags & 1) == 0) {
		msg_emitInFlightMessage(MSG_NOT_EQUIPPED_SHIELDS, (int)playerIdx);
		return;
	}
	if ((craft->workingSubsystems & 1) == 0) {
		FlightAction_EmitSystemCondition(MSG_SHIELDS, MSG_DAMAGED, playerIdx);
		return;
	}

	maxShield = Craft_GetObjectMaxShield((unsigned short)g_players[playerIdx].objectIndex);
	shieldDeficit = 2 * maxShield - craft->shieldFront - craft->shieldRear;
	if (shieldDeficit < 0) {
		shieldDeficit = 0;
	}
	if (shieldDeficit == 0) {
		FlightAction_PlayEngineClick(playerIdx);
		return;
	}

	shieldStep = FlightAction_ShieldLaserTransferStep(obj);
	laserCharge = 0;
	for (slot = 0; slot < craft->laserSlotCount; ++slot) {
		if (craft->warheadData[slot].laserCharge > 0 && craft->warheadData[slot].weaponType < 4) {
			laserCharge += craft->warheadData[slot].laserCharge;
		}
	}

	if (laserCharge != 0) {
		uint8_t slot = 0;
		while (laserCharge != 0) {
			if (craft->warheadData[slot].laserCharge > 0 && craft->warheadData[slot].weaponType < 4) {
				--craft->warheadData[slot].laserCharge;
				--laserCharge;
				if (craft->shieldFront < maxShield) {
					craft->shieldFront += craft->shieldRear >= maxShield ? shieldStep : shieldStep / 2;
				}
				if (craft->shieldRear < maxShield) {
					craft->shieldRear += craft->shieldFront >= maxShield ? shieldStep : shieldStep / 2;
				}
				if (craft->shieldFront >= maxShield && craft->shieldRear >= maxShield) {
					break;
				}
			}

			++slot;
			if (slot >= craft->laserSlotCount) {
				slot = 0;
			}
		}
		FlightAction_PlayAcceptedClick(playerIdx);
		msg_emitInFlightMessage(MSG_TRANSFERRING_ALL, (int)playerIdx);
	} else {
		FlightAction_PlayEngineClick(playerIdx);
	}

	if (craft->shieldDistribMode == 0) {
		craft->shieldFront += shieldStep;
	} else if (craft->shieldDistribMode == 2) {
		craft->shieldRear += shieldStep;
	} else {
		craft->shieldFront += shieldStep / 2;
		craft->shieldRear += shieldStep / 2;
	}
}

static __inline void FlightAction_TransferLasersToShields(unsigned int playerIdx, ObjectRecord const* obj,
														  CraftData* craft) {
	ModelIndex modelIndex;
	int maxShield;
	int shieldDeficit;
	int shieldStep;
	int chargesToDrain;
	uint8_t slot;
	uint16_t attempts;
	if (g_players[playerIdx].hasCheckpointFlag) {
		return;
	}
	if ((craft->systemFlags & 1) == 0) {
		msg_emitInFlightMessage(MSG_NOT_EQUIPPED_SHIELDS, (int)playerIdx);
		return;
	}
	if ((craft->workingSubsystems & 1) == 0) {
		return;
	}

	modelIndex = (ModelIndex)GetModelIndexFromType(obj->objectType);
	if (modelIndex == (ModelIndex)0xffffu) {
		FlightAction_EmitSystemCondition(MSG_SHIELDS, MSG_DAMAGED, playerIdx);
		return;
	}

	maxShield = 2 * g_modelDefs[(uint16_t)modelIndex].shieldStrength;
	shieldDeficit = 2 * maxShield - craft->shieldFront - craft->shieldRear;
	if (shieldDeficit < 0) {
		shieldDeficit = 0;
	}
	if (shieldDeficit > 800) {
		shieldDeficit = 800;
	}

	shieldStep = FlightAction_ShieldLaserTransferStep(obj);
	chargesToDrain = shieldDeficit / shieldStep;
	if (chargesToDrain == 0) {
		FlightAction_PlayEngineClick(playerIdx);
		return;
	}

	slot = 0;
	for (attempts = 0; chargesToDrain > 0 && attempts < 100; ++attempts) {
		if (craft->warheadData[slot].laserCharge > 0 && craft->warheadData[slot].weaponType < 4) {
			--craft->warheadData[slot].laserCharge;
			--chargesToDrain;
			if (craft->shieldDistribMode == 0) {
				if (craft->shieldFront >= maxShield) {
					if (craft->shieldRear < maxShield) {
						craft->shieldRear += shieldStep;
					}
				} else {
					craft->shieldFront += shieldStep;
				}
			} else if (craft->shieldDistribMode == 2) {
				if (craft->shieldRear >= maxShield) {
					if (craft->shieldFront < maxShield) {
						craft->shieldFront += shieldStep;
					}
				} else {
					craft->shieldRear += shieldStep;
				}
			} else {
				craft->shieldFront += shieldStep / 2;
				craft->shieldRear += shieldStep / 2;
			}
		}
		++slot;
		if (slot >= craft->laserSlotCount) {
			slot = 0;
		}
	}

	if (craft->shieldFront > maxShield) {
		craft->shieldFront = maxShield;
	}
	if (craft->shieldRear > maxShield) {
		craft->shieldRear = maxShield;
	}
	FlightAction_PlayAcceptedClick(playerIdx);
	msg_emitInFlightMessage(MSG_TRANSFER_TO_SHIELDS, (int)playerIdx);
}

static __inline int FlightAction_WarheadMessageBase(uint16_t warheadTypeId, unsigned int playerIdx) {
	switch (warheadTypeId) {
		case OBJ_WarheadTorpedo:
			return 0;
		case OBJ_WarheadMissile:
			return 1;
		case OBJ_WarheadAdvancedTorpedo:
			return 2;
		case OBJ_WarheadAdvancedMissile:
			return 3;
		case OBJ_WarheadSpaceBomb:
			return 4;
		case OBJ_WarheadRocket:
			return 5;
		case OBJ_WarheadMagPulse:
			return 6;
		case OBJ_WarheadIonPulse:
			return 7;
		default:
			return (int)playerIdx;
	}
}

static __inline uint16_t FlightAction_FindIncomingWarheadTarget(unsigned int playerIdx) {
	uint32_t objIdx;
	uint32_t bestDistance = UINT32_MAX;
	uint16_t bestObjIdx = 0xffffu;

	for (objIdx = g_projectileObjectSlotStart; objIdx < g_projectileObjectSlotEnd; ++objIdx) {
		ObjectRecord* obj = &g_objectTable[objIdx];
		if (obj->objectType == OBJ_None ||
			(obj->genusId != GENUS_PlayerProjectile && obj->genusId != GENUS_NpcProjectile) ||
			obj->objectType < OBJ_LaserRebel || obj->objectType > OBJ_LaserImperialDS ||
			g_projectileDamageByType[obj->objectType - OBJ_LaserRebel] == 0 || obj->mobj == NULL ||
			obj->mobj->pWarheadGuidance == NULL) {
			continue;
		}

		if (obj->mobj->pWarheadGuidance->targetObjIdx == (uint16_t)g_players[playerIdx].objectIndex) {
			pai_ObjectRefDirectionToObjectRef((unsigned int)g_players[playerIdx].objectIndex, objIdx);
			if ((uint32_t)trig2_polardistance < bestDistance) {
				bestDistance = (uint32_t)trig2_polardistance;
				bestObjIdx = (uint16_t)objIdx;
			}
		}
	}

	if (bestObjIdx != 0xffffu) {
		return bestObjIdx;
	}

	for (objIdx = g_projectileObjectSlotStart; objIdx < g_projectileObjectSlotEnd; ++objIdx) {
		uint16_t targetObjIdx;
		ObjectRecord* obj = &g_objectTable[objIdx];
		if (obj->objectType == OBJ_None ||
			(obj->genusId != GENUS_PlayerProjectile && obj->genusId != GENUS_NpcProjectile) ||
			obj->objectType < OBJ_LaserRebel || obj->objectType > OBJ_LaserImperialDS ||
			g_projectileDamageByType[obj->objectType - OBJ_LaserRebel] == 0 || obj->mobj == NULL ||
			obj->mobj->pWarheadGuidance == NULL) {
			continue;
		}

		targetObjIdx = obj->mobj->pWarheadGuidance->targetObjIdx;
		if (targetObjIdx != 0xffffu && g_objectTable[targetObjIdx].mobj != NULL &&
			g_objectTable[targetObjIdx].mobj->state == 0) {
			int targetTeam = g_objectTable[targetObjIdx].mobj->team;
			int playerTeam = g_players[playerIdx].playerIff;
			if (targetTeam == playerTeam || g_missionTeams[targetTeam].allies[playerTeam] == 1) {
				pai_ObjectRefDirectionToObjectRef((unsigned int)g_players[playerIdx].objectIndex, objIdx);
				if ((uint32_t)trig2_polardistance < bestDistance) {
					bestDistance = (uint32_t)trig2_polardistance;
					bestObjIdx = (uint16_t)objIdx;
				}
			}
		}
	}

	return bestObjIdx;
}

static __inline uint16_t FlightAction_FindNextPlayerCraftTarget(unsigned int playerIdx) {
	uint32_t objIdx;
	uint32_t count;
	if (g_activeRegionCraftObjectSlotEnd == g_activeRegionObjectSlotStart) {
		return 0xffffu;
	}

	objIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
	count = g_activeRegionCraftObjectSlotEnd - g_activeRegionObjectSlotStart;
	while (count-- != 0) {
		ObjectRecord* obj;
		MobileObject* mobj;
		CraftData* craft;
		++objIdx;
		if (objIdx >= g_activeRegionCraftObjectSlotEnd || objIdx < g_activeRegionObjectSlotStart) {
			objIdx = g_activeRegionObjectSlotStart;
		}

		obj = &g_objectTable[objIdx];
		if (obj->objectType == OBJ_None || obj->playerOwnerIdx == -1 ||
			obj->playerOwnerIdx == (int)playerIdx || obj->genusId == GENUS_Explosion ||
			obj->regionIdx != g_players[playerIdx].regionIndex) {
			continue;
		}

		mobj = obj->mobj;
		craft = mobj != NULL ? mobj->pCraft : NULL;
		if (craft != NULL && (craft->workingSubsystems & 0x100) != 0 && craft->beamActive &&
			craft->beamTypeId == 3 && craft->beamTimer != 0) {
			continue;
		}

		if (!g_flightLocatePlayersEnabled) {
			int playerTeam = g_players[playerIdx].playerIff;
			int objTeam = mobj ? mobj->team : g_missionFlightGroups[obj->flightGroupIdx].fg.team;
			if (objTeam != playerTeam && !g_missionTeams[playerTeam].allies[objTeam]) {
				continue;
			}
		}

		if (mobj == NULL || craft == NULL ||
			(mobj->state == 0 &&
			 (craft->objectKind == 3 || craft->objectKind == 4 || craft->objectKind == 7))) {
			continue;
		}

		return (uint16_t)objIdx;
	}

	return 0xffffu;
}

static __inline int FlightAction_IsPlayerTractorBeamVictim(uint16_t objIdx, ObjectRecord const* obj,
														   CraftData const* craft) {
	return objIdx != 0xffffu && objIdx < g_activeRegionCraftObjectSlotEnd && obj->playerOwnerIdx != -1 &&
		   craft != NULL && (craft->workingSubsystems & 0x100) != 0 && craft->beamActive &&
		   craft->beamTypeId == 3 && craft->beamTimer != 0;
}

static __inline uint16_t FlightAction_FindNearestLocatedHostilePlayerTarget(unsigned int playerIdx) {
	uint16_t scanIdx;
	uint16_t bestObjIdx;
	uint32_t bestDistance;
	uint32_t remaining;
	PlayerData* player = &g_players[playerIdx];
	if (g_activeRegionCraftObjectSlotEnd == g_activeRegionObjectSlotStart) {
		return 0xffffu;
	}

	scanIdx = (uint16_t)player->currentTargetObjectIdx;
	bestObjIdx = 0xffffu;
	bestDistance = UINT32_MAX;
	remaining = g_activeRegionCraftObjectSlotEnd - g_activeRegionObjectSlotStart;

	while (remaining-- != 0) {
		ObjectRecord* obj;
		MobileObject* mobj;
		uint16_t playerTeam;
		uint16_t candidateTeam;
		CraftData* craft;
		++scanIdx;
		if (scanIdx >= g_activeRegionCraftObjectSlotEnd) {
			scanIdx = (uint16_t)g_activeRegionObjectSlotStart;
		}
		if (scanIdx < g_activeRegionObjectSlotStart) {
			scanIdx = (uint16_t)g_activeRegionObjectSlotStart;
		}

		obj = &g_objectTable[scanIdx];
		if (obj->objectType == OBJ_None || obj->playerOwnerIdx == -1 ||
			obj->playerOwnerIdx == (int)playerIdx || obj->genusId == GENUS_Explosion ||
			obj->regionIdx != player->regionIndex) {
			continue;
		}

		mobj = obj->mobj;
		playerTeam = (uint16_t)player->playerIff;
		candidateTeam =
			(uint16_t)(mobj != NULL ? mobj->team : g_missionFlightGroups[obj->flightGroupIdx].fg.team);
		if (candidateTeam == playerTeam || g_missionTeams[playerTeam].allies[candidateTeam]) {
			continue;
		}

		craft = mobj != NULL ? mobj->pCraft : NULL;
		if (mobj == NULL || craft == NULL || FlightAction_IsPlayerTractorBeamVictim(scanIdx, obj, craft)) {
			continue;
		}

		if (mobj->state == 0 &&
			(craft->objectKind == 3 || craft->objectKind == 4 || craft->objectKind == 7)) {
			continue;
		}

		if (player->objectIndex == -1) {
			Mission_ResolveObjectOrMissionPointWorldLoc(scanIdx, 0, 0, 0);
			trig2_ctop(worldlocx - player->viewState.savedTargetX, worldlocy - player->viewState.savedTargetY,
					   worldlocz - player->viewState.savedTargetZ);
		} else {
			pai_ObjectRefDirectionToObjectRef((unsigned int)player->objectIndex, scanIdx);
		}

		if ((uint32_t)trig2_polardistance < bestDistance) {
			bestDistance = (uint32_t)trig2_polardistance;
			bestObjIdx = scanIdx;
		}
	}

	return bestObjIdx;
}

static __inline uint16_t FlightAction_CycleAttackerTarget(unsigned int playerIdx,
														  CraftData const* playerCraft) {
	uint16_t candidateIdx;
	uint16_t remaining;
	PlayerData* player = &g_players[playerIdx];
	uint16_t playerObjIdx = (uint16_t)player->objectIndex;
	if (playerObjIdx == 0xffffu || g_activeRegionCraftObjectSlotEnd == g_activeRegionObjectSlotStart) {
		return 0xffffu;
	}

	candidateIdx = (uint16_t)player->currentTargetObjectIdx;
	remaining = (uint16_t)(g_activeRegionCraftObjectSlotEnd - g_activeRegionObjectSlotStart);
	while (remaining-- != 0) {
		ObjectRecord* candidate;
		CraftData* candidateCraft;
		++candidateIdx;
		if (candidateIdx >= g_activeRegionCraftObjectSlotEnd ||
			candidateIdx < g_activeRegionObjectSlotStart) {
			candidateIdx = g_activeRegionObjectSlotStart;
		}

		candidate = &g_objectTable[candidateIdx];
		if (candidate->objectType == OBJ_None || candidateIdx == playerObjIdx ||
			candidate->genusId == GENUS_Explosion || candidate->mobj == NULL ||
			candidate->mobj->pCraft == NULL) {
			continue;
		}

		candidateCraft = candidate->mobj->pCraft;
		if (candidateCraft->workingSubsystems == 0 || candidateCraft->objectKind != 0 ||
			FlightAction_IsPlayerTractorBeamVictim(candidateIdx, candidate, candidateCraft)) {
			continue;
		}

		if (candidate->playerOwnerIdx == -1) {
			uint8_t slot;
			AiController* ai = pai_GetEffectiveAIController(candidateCraft);
			if (ai->targetObjIdx == playerObjIdx && (ai->maneuverMode == 12 || ai->maneuverMode == 23)) {
				return candidateIdx;
			}
			for (slot = 0; slot < candidateCraft->laserSlotCount; ++slot) {
				if (candidateCraft->warheadData[slot].weaponType == 4 &&
					(uint16_t)candidateCraft->warheadData[slot].turretTargetObjIdx == playerObjIdx) {
					return candidateIdx;
				}
			}
			continue;
		}

		if (playerCraft != NULL && playerCraft->lastAttackerObjIdx == candidateIdx) {
			uint16_t nowSeconds = (uint16_t)Mission_GameTimeToSeconds(
				g_missionElapsedClock.hours, g_missionElapsedClock.minutes, g_missionElapsedClock.seconds);
			if ((uint16_t)(nowSeconds - playerCraft->lastHitTimestamp) < 5u) {
				return candidateIdx;
			}
		}

		if ((uint16_t)g_players[candidate->playerOwnerIdx].currentTargetObjectIdx == playerObjIdx) {
			int playerTeam = player->playerIff;
			int candidateTeam = candidate->mobj ? candidate->mobj->team
												: g_missionFlightGroups[candidate->flightGroupIdx].fg.team;
			if (candidateTeam != playerTeam && !g_missionTeams[playerTeam].allies[candidateTeam]) {
				return candidateIdx;
			}
		}
	}

	return 0xffffu;
}

static __inline uint16_t FlightAction_CycleFlaggedTarget(unsigned int playerIdx) {
	uint16_t candidateIdx;
	uint16_t remaining;
	PlayerData* player = &g_players[playerIdx];
	if (g_activeRegionCraftObjectSlotEnd == g_activeRegionObjectSlotStart) {
		return 0xffffu;
	}

	candidateIdx = (uint16_t)(player->currentTargetObjectIdx + 1);
	if (candidateIdx < g_activeRegionObjectSlotStart || candidateIdx >= g_activeRegionCraftObjectSlotEnd) {
		candidateIdx = g_activeRegionObjectSlotStart;
	}

	remaining = (uint16_t)(g_activeRegionCraftObjectSlotEnd - g_activeRegionObjectSlotStart);
	while (remaining-- != 0) {
		ObjectRecord* candidate = &g_objectTable[candidateIdx];
		if (candidate->objectType != OBJ_None &&
			(g_modelTypeTable[(uint16_t)candidate->objectType].flags & 4) != 0) {
			if (candidateIdx != (uint16_t)player->currentTargetObjectIdx) {
				return candidateIdx;
			}
			return 0xffffu;
		}

		++candidateIdx;
		if (candidateIdx < g_activeRegionObjectSlotStart ||
			candidateIdx >= g_activeRegionCraftObjectSlotEnd) {
			candidateIdx = g_activeRegionObjectSlotStart;
		}
	}

	return 0xffffu;
}

static __inline uint16_t FlightAction_FindNewestLeaderTarget(unsigned int playerIdx) {
	uint16_t objIdx;
	uint16_t bestObjIdx = 0xffffu;
	uint16_t bestFramesAlive = 0xffffu;
	uint16_t playerObjIdx = (uint16_t)g_players[playerIdx].objectIndex;

	for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
		CraftData* candidateCraft;
		ObjectRecord* obj = &g_objectTable[objIdx];
		if (obj->objectType == OBJ_None || objIdx == playerObjIdx || obj->genusId == GENUS_Explosion ||
			obj->mobj == NULL || obj->mobj->pCraft == NULL) {
			continue;
		}

		candidateCraft = obj->mobj->pCraft;
		if (candidateCraft->leader_obj_idx != -1 ||
			(candidateCraft->objectKind != 0 && candidateCraft->objectKind != 2 &&
			 candidateCraft->objectKind != 6) ||
			FlightAction_IsPlayerTractorBeamVictim(objIdx, obj, candidateCraft)) {
			continue;
		}

		if (obj->mobj->framesAlive < bestFramesAlive) {
			bestFramesAlive = obj->mobj->framesAlive;
			bestObjIdx = objIdx;
		}
	}

	return bestObjIdx;
}

static __inline int FlightAction_HasCheckpointPartner(unsigned int playerIdx) {
	unsigned int otherIdx;
	uint16_t boundFlightGroupIdx = g_players[playerIdx].boundFlightGroupIdx;
	for (otherIdx = 0; otherIdx < 8; ++otherIdx) {
		PlayerData const* other = &g_players[otherIdx];
		if (otherIdx != playerIdx && other->connectedFlag && other->hasCheckpointFlag &&
			other->boundFlightGroupIdx == boundFlightGroupIdx) {
			return 1;
		}
	}
	return 0;
}

static __inline void FlightAction_NotifySameTeamPlayersOfTarget(unsigned int playerIdx,
																uint8_t pendingActionId,
																InFlightMessageId localMessageId) {
	unsigned int otherIdx;
	PlayerData* issuer = &g_players[playerIdx];
	for (otherIdx = 0; otherIdx < 8; ++otherIdx) {
		CraftData* otherCraft;
		PlayerData* other = &g_players[otherIdx];
		if (otherIdx == playerIdx || other->connectedFlag != 1 || other->playerIff != issuer->playerIff ||
			other->objectIndex == -1 || other->objectIndex == issuer->currentTargetObjectIdx ||
			other->pendingActionId != 0) {
			continue;
		}

		otherCraft = g_objectTable[(uint16_t)other->objectIndex].mobj->pCraft;
		if (otherCraft != NULL && (otherCraft->workingSubsystems & 0x200) != 0) {
			other->pendingActionId = pendingActionId;
			other->pendingActionParam = issuer->currentTargetObjectIdx;
			other->pendingActionIssuerPlayerIdx = (int16_t)playerIdx;
			other->pendingActionTimer = 1416;
			if (otherIdx == (unsigned int)g_localPlayer) {
				fsfx_PlaySound(127, 0xffffu, (unsigned int)g_localPlayer);
				msg_addMessagePtr(0, NetSession_GetPlayerName((int)playerIdx));
				msg_emitInFlightMessage(localMessageId, g_localPlayer);
			}
		}
	}
}

static __inline void FlightAction_NotifySameTeamPlayersIgnoringTarget(unsigned int playerIdx) {
	unsigned int otherIdx;
	PlayerData* issuer = &g_players[playerIdx];
	for (otherIdx = 0; otherIdx < 8; ++otherIdx) {
		CraftData* otherCraft;
		PlayerData* other = &g_players[otherIdx];
		if (otherIdx == playerIdx || other->connectedFlag != 1 || other->playerIff != issuer->playerIff ||
			other->objectIndex == -1 || other->currentTargetObjectIdx != issuer->currentTargetObjectIdx ||
			(uint16_t)other->currentTargetObjectIdx == (uint16_t)other->objectIndex ||
			other->pendingActionId != 0) {
			continue;
		}

		otherCraft = g_objectTable[(uint16_t)other->objectIndex].mobj->pCraft;
		if (otherCraft != NULL && (otherCraft->workingSubsystems & 0x200) != 0) {
			other->pendingActionId = 4;
			other->pendingActionParam = issuer->currentTargetObjectIdx;
			other->pendingActionIssuerPlayerIdx = (int16_t)playerIdx;
			other->pendingActionTimer = 1416;
			if (otherIdx == (unsigned int)g_localPlayer) {
				fsfx_PlaySound(127, 0xffffu, (unsigned int)g_localPlayer);
				msg_addMessagePtr(0, NetSession_GetPlayerName((int)playerIdx));
				msg_emitInFlightMessage(MSG_IGNORE_MY_TARGET, g_localPlayer);
			}
		}
	}
}

static __inline void FlightAction_SendPendingCommandToTargetPlayer(unsigned int playerIdx,
																   uint8_t pendingActionId,
																   InFlightMessageId localMessageId) {
	unsigned int otherIdx;
	PlayerData* issuer = &g_players[playerIdx];
	for (otherIdx = 0; otherIdx < 8; ++otherIdx) {
		PlayerData* other = &g_players[otherIdx];
		if (other->objectIndex != issuer->currentTargetObjectIdx) {
			continue;
		}

		if (other->playerIff == issuer->playerIff) {
			other->pendingActionId = pendingActionId;
			other->pendingActionIssuerPlayerIdx = (int16_t)playerIdx;
			other->pendingActionTimer = 1416;
			if (otherIdx == (unsigned int)g_localPlayer) {
				fsfx_PlaySound(127, 0xffffu, (unsigned int)g_localPlayer);
				msg_addMessagePtr(0, NetSession_GetPlayerName((int)playerIdx));
				msg_emitInFlightMessage(localMessageId, g_localPlayer);
			}
		} else {
			g_msgSenderIff = issuer->iff;
			msg_emitInFlightMessage(MSG_NO_COMMLINK, (int)playerIdx);
		}
	}
}

static __inline void FlightAction_CommandTargetWait(unsigned int playerIdx) {
	if (Player_CanRadioCommandCraft((uint16_t)g_players[playerIdx].currentTargetObjectIdx, (int)playerIdx)) {
		uint16_t targetObjIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
		AiController* ai;
		g_curCraft = g_objectTable[targetObjIdx].mobj->pCraft;
		ai = pai_GetEffectiveAIController(g_curCraft);
		if (strcmp(g_planTable[ai->pendingPlanId].name, "craftwaitforgopln") &&
			strcmp(g_planTable[ai->pendingPlanId].name, "intohyperspacepln") &&
			strcmp(g_planTable[ai->pendingPlanId].name, "outofhyperspacepln")) {
			uint8_t slot;
			ai->savedPlanId = ai->pendingPlanId;
			ai->pendingPlanId = (uint8_t)pai_findplanbyname(
				g_objectTable[targetObjIdx].genusId == GENUS_Starship ? "starshipwaitforgopln"
																	  : "craftwaitforgopln");
			pai_setupcraftcontext(targetObjIdx);
			pai_ApplyPendingPlanTargetAndManeuver(targetObjIdx);
			for (slot = 0; slot < g_curCraft->laserSlotCount; ++slot) {
				if (g_curCraft->warheadData[slot].weaponType >= 4) {
					g_curCraft->warheadData[slot].turretTargetObjIdx = -1;
				}
			}
			msg_radioMessage((int16_t)targetObjIdx, g_curCraft, MSG_ACK_WAITING, 4, 0);
		}
	} else {
		FlightAction_SendPendingCommandToTargetPlayer(playerIdx, 7, MSG_WAIT_FOR_ORDERS);
	}
}

static __inline void FlightAction_CommandTargetGo(unsigned int playerIdx) {
	if (Player_CanRadioCommandCraft((uint16_t)g_players[playerIdx].currentTargetObjectIdx, (int)playerIdx)) {
		uint16_t targetObjIdx = (uint16_t)g_players[playerIdx].currentTargetObjectIdx;
		AiController* ai;
		g_curCraft = g_objectTable[targetObjIdx].mobj->pCraft;
		ai = pai_GetEffectiveAIController(g_curCraft);
		if (!strcmp(g_planTable[ai->pendingPlanId].name, "craftwaitforgopln")) {
			ai->pendingPlanId = ai->savedPlanId;
			pai_setupcraftcontext(targetObjIdx);
			pai_ApplyPendingPlanTargetAndManeuver(targetObjIdx);
			msg_radioMessage((int16_t)targetObjIdx, g_curCraft, MSG_ACK_GOING, 5, 0);
		}
	} else {
		FlightAction_SendPendingCommandToTargetPlayer(playerIdx, 8, MSG_GO_AHEAD);
	}
}

static __inline int FlightAction_TeamHasAvailableReinforcement(unsigned int playerIdx) {
	uint16_t fgIdx;
	int playerTeam = g_players[playerIdx].playerIff;
	for (fgIdx = 0; fgIdx < (uint16_t)(int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
		XwaFlightGroup* fg;
		if (!g_missionFgStats[fgIdx].arrivalEnabled) {
			continue;
		}
		fg = &g_missionFlightGroups[fgIdx].fg;
		if ((fg->arrival[0].triggers[0].condition == 20 &&
			 fg->arrival[0].triggers[0].variable == playerTeam) ||
			(fg->arrival[0].triggers[1].condition == 20 &&
			 fg->arrival[0].triggers[1].variable == playerTeam) ||
			(fg->arrival[1].triggers[0].condition == 20 &&
			 fg->arrival[1].triggers[0].variable == playerTeam) ||
			(fg->arrival[1].triggers[1].condition == 20 &&
			 fg->arrival[1].triggers[1].variable == playerTeam)) {
			return 1;
		}
	}
	return 0;
}

static __inline int FlightAction_TargetComponentSelectable(ObjectTypeId objectType,
														   CraftData const* targetCraft,
														   uint16_t componentIdx) {
	MeshType meshType;
	if (targetCraft->componentState[componentIdx] != 0) {
		return 0;
	}

	meshType = ModelMesh_GetObjectTypeMeshType((uint16_t)objectType, componentIdx);
	if (meshType != MESH_MiscHull && meshType != MESH_Antenna) {
		int meshCount;
		int meshIdx;
		int targetId = ModelMesh_GetTargetId((uint16_t)objectType, componentIdx);
		if (targetId == 0 || (targetId == 1 && meshType != 1 && meshType != 3)) {
			return targetCraft->componentHp[componentIdx] != 0;
		}

		meshCount = ModelMesh_GetObjectTypeMeshCount((uint16_t)objectType);
		for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
			if (ModelMesh_GetTargetId((uint16_t)objectType, meshIdx) == targetId &&
				ModelMesh_GetObjectTypeMeshType((uint16_t)objectType, meshIdx) == meshType) {
				if ((uint16_t)meshIdx != componentIdx) {
					return 0;
				}
				break;
			}
		}
	}

	return targetCraft->componentHp[componentIdx] != 0;
}

static __inline void FlightAction_CycleTargetComponent(unsigned int playerIdx, int direction) {
	uint16_t targetObjIdx;
	ObjectRecord* targetObj;
	uint16_t meshCount;
	PlayerData* player = &g_players[playerIdx];
	if (player->currentTargetObjectIdx == 0xffffu) {
		return;
	}

	targetObjIdx = (uint16_t)player->currentTargetObjectIdx;
	if (targetObjIdx < g_activeRegionObjectSlotStart || targetObjIdx >= g_activeRegionCraftObjectSlotEnd) {
		return;
	}

	targetObj = &g_objectTable[targetObjIdx];
	if (targetObj->mobj == NULL || targetObj->mobj->pCraft == NULL) {
		return;
	}

	meshCount = (uint16_t)ModelMesh_GetObjectTypeMeshCount((uint16_t)targetObj->objectType);
	if (meshCount != 0) {
		uint16_t attempts;
		for (attempts = 0; attempts < meshCount; ++attempts) {
			if (direction > 0) {
				++player->selectedTargetComponent;
				if ((uint16_t)player->selectedTargetComponent >= meshCount) {
					player->selectedTargetComponent = 0;
				}
			} else {
				if (--player->selectedTargetComponent == -1) {
					player->selectedTargetComponent = (int16_t)(meshCount - 1);
				}
			}

			if (FlightAction_TargetComponentSelectable(targetObj->objectType, targetObj->mobj->pCraft,
													   (uint16_t)player->selectedTargetComponent)) {
				break;
			}
		}
	}

	FlightAction_PlayAcceptedClick(playerIdx);
}

static __inline void FlightAction_ToggleFilmRecording(unsigned int playerIdx) {
	int playerObjIdx;
#ifndef XWA_MODERN
	char filmPath[256];
	char* filmName;
#endif
	if (g_flightPlayerCount != 1 || !g_filmFeatureEnabled) {
		return;
	}

	playerObjIdx = g_players[playerIdx].objectIndex;
	if (playerObjIdx == -1 || g_provingGroundsModeActive ||
		g_objectTable[(uint16_t)playerObjIdx].mobj->pCraft->objectKind == 9) {
		return;
	}

	if (g_filmPlaybackMode) {
		if (g_filmPlaybackMode == 2) {
			g_filmPlaybackMode = 3;
		}
		return;
	}

	if (g_filmRecording) {
		if (g_filmRecording == 2) {
			Film_FlushWriteBuffer();
#ifdef XWA_MODERN
			File_Close(g_filmFile);
			g_filmFile = NULL;
			g_filmRecording = 0;
			g_inputTimestamp += Time_GetFrameDelta();
			Sound_StopAllInstances();
			Music_PauseIfInitialized();
			FlightFilm_SaveTempRecordingWithPrompt();
#else
			fclose((FILE*)g_filmFile);
			g_filmFile = NULL;
			g_filmRecording = 0;
			g_inputTimestamp += (int)Time_GetFrameDelta();
			Sound_StopAllInstances();
			Music_PauseIfInitialized();
			filmName = FlightFilm_RunNamePrompt();
			if (filmName != NULL && filmName[0] != '\0') {
				sprintf(filmPath, "film\\%s.flm", filmName);
				remove(filmPath);
				rename("film\\tempfilm.tmp", filmPath);
			}
#endif
			Music_ResumeIfInitialized();
			Time_GetFrameDelta();
			g_actionKey = KEY_NONE;
			sub_4D4640();
		}
		return;
	}

	fsfx_PlaySound(67, 0xffffu, (unsigned int)g_localPlayer);
	g_filmRecording = 1;
#ifdef XWA_MODERN
	g_filmFile = File_Open(AERON_VFS_ROOT_USER, "film\\tempfilm.tmp", "wb");
#else
	g_filmFile = (AeronFile*)fopen("film\\tempfilm.tmp", "wb");
#endif
	if (g_filmFile == NULL) {
		g_filmRecording = 0;
	}
	g_filmWriteBufferedBytes = 0;
}

static __inline void FlightAction_UpdateHudForExternalCamera(unsigned int playerIdx) {
	PlayerData* player = &g_players[playerIdx];
	if (player->viewState.externalCameraActive) {
		Hud_SetHudEnabled((int)playerIdx, 0);
	} else if (player->savedHudEnabled) {
		Hud_SetHudEnabled((int)playerIdx, 1);
	}
}

static __inline void FlightAction_FocusNewestPlayerProjectile(unsigned int playerIdx) {
	PlayerData* player = &g_players[playerIdx];
	if (g_flightSimSideEffectsSuppressed || player->mapCameraState) {
		return;
	}

	if (player->viewState.cameraFocusObjIdx == player->objectIndex) {
		uint16_t bestFramesAlive = 0xffffu;
		uint32_t regionBase =
			(g_regionObjectSlotEnd / (uint32_t)g_missionRegionCount) * (uint32_t)player->regionIndex;
		uint32_t objIdx = regionBase + g_craftObjectSlotsTotal / (uint32_t)g_missionRegionCount;
		uint32_t objEnd = objIdx + g_projectileObjectSlotsTotal / (uint32_t)g_missionRegionCount;
		for (; objIdx < objEnd; ++objIdx) {
			ObjectRecord* projectile = &g_objectTable[objIdx];
			if (projectile->objectType != OBJ_None && projectile->mobj != NULL &&
				player->objectIndex == (uint16_t)projectile->mobj->sourceObjIdx &&
				projectile->mobj->framesAlive <= bestFramesAlive) {
				bestFramesAlive = projectile->mobj->framesAlive;
				player->viewState.cameraFocusObjIdx = (int)objIdx;
				player->viewState.externalCameraActive = 1;
				player->viewState.transitionTimer = 0;
				player->viewState.savedHudStateByte = player->viewState.hudStateLive;
				player->viewState.savedHudAimX = player->viewState.hudAimX;
				player->viewState.savedHudAimY = player->viewState.hudAimY;
				player->viewState.cameraDistance =
					4 * g_modelTypeTable[(uint16_t)projectile->objectType].maxBoundsExtent;
				if (player->viewState.cameraDistance > 0x2000) {
					player->viewState.cameraDistance = 0x2000;
				}
			}
		}
		Player_UpdateHudViewForCameraFocus((int)playerIdx);
	} else {
		player->viewState.cameraFocusObjIdx = player->objectIndex;
		Player_StepExtView((int)playerIdx);
	}

	FlightAction_UpdateHudForExternalCamera(playerIdx);
}

static __inline void FlightAction_CycleMfdCommandMenuItem(unsigned int playerIdx, int direction) {
	uint8_t menuRow;
	uint8_t maxItem;
	uint16_t attempts;
	PlayerData* player = &g_players[playerIdx];
	if (!player->hudEnabled || player->mfd.page[player->mfd.activeIndex] != 6) {
		return;
	}

	menuRow = player->mfd.menuRow;
	maxItem = menuRow == 0 ? (uint8_t)(player->mfdCommandMenuItemCount[0] - 1)
						   : (menuRow > 8 ? g_mfdCommandSubMenuItemCount[menuRow / 10]
										  : player->mfdCommandMenuItemCount[menuRow]);
	if (maxItem == 0) {
		return;
	}

	for (attempts = 0; attempts <= maxItem; ++attempts) {
		if (direction < 0) {
			--player->mfd.menuItem;
			if (player->mfd.menuItem > maxItem) {
				player->mfd.menuItem = maxItem;
			}
		} else {
			++player->mfd.menuItem;
			if (player->mfd.menuItem > maxItem) {
				player->mfd.menuItem = 0;
			}
		}
		if (Mfd_IsCommandMenuItemAvailable((uint16_t)playerIdx, player->mfd.menuRow, player->mfd.menuItem)) {
			break;
		}
	}
}

static __inline void FlightAction_ToggleConsoleMfd(unsigned int playerIdx) {
	PlayerData* player = &g_players[playerIdx];
	if (!g_consoleEnabled || g_flightPlayerCount > 1 || !player->hudEnabled) {
		return;
	}

	if (player->mfd.consolePageAvailable) {
		player->mfd.consolePageAvailable = 0;
		if (player->mfd.page[1] == 7) {
			Hud_SetMfdPage((int)playerIdx, 1, 1);
			Hud_ToggleMfdSide((int)playerIdx, 1);
			if (player->mfd.enabled[2]) {
				Hud_ForceHudRefresh((int)playerIdx, 2);
			}
		}
		if (player->mfd.page[2] == 7) {
			Hud_SetMfdPage((int)playerIdx, 2, 2);
			Hud_ToggleMfdSide((int)playerIdx, 2);
			if (player->mfd.enabled[1]) {
				Hud_ForceHudRefresh((int)playerIdx, 1);
			}
		}
	} else {
		player->mfd.consolePageAvailable = 1;
		if (!player->mfd.enabled[1]) {
			Hud_ToggleMfdSide((int)playerIdx, 1);
			Hud_ForceHudRefresh((int)playerIdx, 1);
		}
		Hud_SetMfdPage((int)playerIdx, 1, 7);
	}
}

static __inline void FlightAction_RequestAbortOrDisconnect(unsigned int playerIdx) {
	PlayerData* player = &g_players[playerIdx];
	if (player->connectedFlag == 1) {
		return;
	}

	FlightAction_PlayAcceptedClick(playerIdx);
	FlightAction_PlayAcceptedClick(playerIdx);
	fsfx_PlaySound(61, 0xffffu, playerIdx);
	msg_emitInFlightMessage(player->network.directPlayId == NetSession_GetHostDplayId()
								? MSG_END_MISSION_ABORT
								: MSG_END_MISSION_DISCONNECT,
							(int)playerIdx);
	player->pendingActionId = 2;
	player->pendingActionParam = -1;
	player->pendingActionTimer = 1888;
}

static __inline void FlightAction_ResetLook(PlayerData* player) {
	player->viewState.hudAimY = 0;
	player->viewState.hudAimX = 0;
	player->lookYawOffset = 0;
	player->lookPitchOffset = 0;
}

static __inline int FlightAction_HasCommandSystem(unsigned int playerIdx, CraftData* craft) {
	return g_players[playerIdx].mapCameraState || (craft != NULL && (craft->workingSubsystems & 0x200) != 0);
}

static __inline void FlightAction_SelectMfdCommandMenu(unsigned int playerIdx) {
	PlayerData* player = &g_players[playerIdx];

	if (!player->hudEnabled) {
		return;
	}

	if (!player->mfd.enabled[1] && player->mfd.enabled[2] == 1) {
		if (player->mfd.page[2] == 6) {
			Hud_ToggleMfdSide((int)playerIdx, 2);
		} else {
			Hud_ToggleMfdSide((int)playerIdx, 1);
			Hud_SetMfdPage((int)playerIdx, 1, 6);
		}
		return;
	}

	if (!player->mfd.enabled[1] && !player->mfd.enabled[2]) {
		Hud_ToggleMfdSide((int)playerIdx, 1);
		Hud_SetMfdPage((int)playerIdx, 1, 6);
		return;
	}

	if (player->mfd.enabled[1] == 1 && !player->mfd.enabled[2]) {
		if (player->mfd.page[1] == 6) {
			Hud_ToggleMfdSide((int)playerIdx, 1);
		} else {
			Hud_ToggleMfdSide((int)playerIdx, 2);
			Hud_SetMfdPage((int)playerIdx, 2, 6);
		}
		return;
	}

	if (player->mfd.page[1] != 6) {
		if (player->mfd.page[2] != 6) {
			Hud_SetMfdPage((int)playerIdx, 1, 6);
		} else {
			Hud_ToggleMfdSide((int)playerIdx, 2);
		}
	} else if (player->mfd.page[2] == 6) {
		Hud_ToggleMfdSide((int)playerIdx, 1);
		Hud_ToggleMfdSide((int)playerIdx, 2);
	} else {
		Hud_ToggleMfdSide((int)playerIdx, 1);
	}
}

static __inline void FlightAction_ConfirmPendingAction(unsigned int playerIdx, CraftData* craft) {
	PlayerData* player = &g_players[playerIdx];

	if (g_provingGroundsModeActive && g_yardChallengeMode >= 6u) {
		Yard_PickUpR2D2Objective(playerIdx);
	}

	switch (player->pendingActionId) {
		case 0:
			if (player->mapCameraState) {
				if (player->mapCameraState == 1) {
					int focusObjIdx = player->viewState.cameraFocusObjIdx;
					if (focusObjIdx == 0xffff) {
						focusObjIdx = player->viewState.aimTargetIdx;
						if (focusObjIdx == 0xffff) {
							if (player->currentTargetObjectIdx == 0xffffu) {
								player->viewState.cameraDistance = 0x40000;
								player->viewState.savedTargetZ = 0x40000;
							} else {
								ObjectRecord* focusObj =
									&g_objectTable[(uint16_t)player->currentTargetObjectIdx];
								player->viewState.cameraDistance = collide_roughdistance3d(
									player->viewState.savedTargetX - focusObj->world_x,
									player->viewState.savedTargetY - focusObj->world_y,
									player->viewState.savedTargetZ - focusObj->world_z);
								player->viewState.savedTargetX = focusObj->world_x;
								player->viewState.savedTargetY = focusObj->world_y;
								player->viewState.cameraFocusObjIdx = player->currentTargetObjectIdx;
							}
						} else {
							ObjectRecord* focusObj = &g_objectTable[(uint16_t)focusObjIdx];
							player->viewState.cameraDistance =
								collide_roughdistance3d(player->viewState.savedTargetX - focusObj->world_x,
														player->viewState.savedTargetY - focusObj->world_y,
														player->viewState.savedTargetZ - focusObj->world_z);
							player->viewState.savedTargetX = focusObj->world_x;
							player->viewState.savedTargetY = focusObj->world_y;
							player->viewState.cameraFocusObjIdx = focusObjIdx;
						}
					} else {
						ObjectRecord* focusObj = &g_objectTable[(uint16_t)focusObjIdx];
						player->viewState.cameraDistance =
							collide_roughdistance3d(player->viewState.savedTargetX - focusObj->world_x,
													player->viewState.savedTargetY - focusObj->world_y,
													player->viewState.savedTargetZ - focusObj->world_z);
						player->viewState.savedTargetX = focusObj->world_x;
						player->viewState.savedTargetY = focusObj->world_y;
					}
					player->viewState.aimTargetIdx = 0xffff;
				} else if (player->currentTargetObjectIdx != 0xffffu) {
					ObjectRecord* focusObj = &g_objectTable[(uint16_t)player->currentTargetObjectIdx];
					player->viewState.cameraFocusObjIdx = player->currentTargetObjectIdx;
					player->viewState.cameraDistance =
						collide_roughdistance3d(player->viewState.savedTargetX - focusObj->world_x,
												player->viewState.savedTargetY - focusObj->world_y,
												player->viewState.savedTargetZ - focusObj->world_z);
				}
				player->mapCameraState ^= 0x80u;
			}
			player->pendingActionId = 0;
			return;

		case 1:
		case 5:
			if (!player->mapCameraState && craft != NULL) {
				if ((craft->workingSubsystems & 4) == 0) {
					FlightAction_EmitSystemCondition(MSG_TARGCOMPUTER, MSG_DAMAGED, playerIdx);
				} else {
					player->currentTargetObjectIdx = player->pendingActionParam;
					player->missileLockState = 0;
					craft->warheadLockTicks = 0;
					msg_emitInFlightMessage(
						player->currentTargetObjectIdx == 0xffffu ||
								g_objectTable[(uint16_t)player->currentTargetObjectIdx].objectType == OBJ_None
							? MSG_OBJECT_DESTROYED
							: MSG_OBJECT_TARGETED,
						(int)playerIdx);
					if ((uint16_t)player->pendingActionIssuerPlayerIdx == (uint16_t)g_localPlayer) {
						msg_radioMessage(
							(uint16_t)player->objectIndex, g_objectTable[player->objectIndex].mobj->pCraft,
							player->pendingActionId == 1 ? MSG_ACK_USING_TARGET : MSG_ACK_COVER_ME,
							player->pendingActionId == 1 ? 6 : 9, 0);
					}
				}
			}
			player->pendingActionId = 0;
			return;

		case 2:
			if (!g_flightSimSideEffectsSuppressed) {
				if (player->pendingActionParam != -1) {
					if (!player->hasCheckpointFlag) {
						if (g_filmPlaybackMode && g_filmOverlayActive == 1) {
							Hud_SetHudViewState(g_players[g_localPlayer].viewState.hudStateLive,
												g_localPlayer);
							g_filmOverlayActive = 0;
							Hud_SyncLocalSoftwareHudMasks(1);
						}
#ifdef XWA_MODERN
						Hangar_BeginEnterCraft((uint16_t)player->pendingActionParam);
#else
						Hangar_EnterCraft((uint16_t)player->pendingActionParam);
#endif
						player->pendingActionId = 0;
					}
					return;
				}

				if (player->connectedFlag == 1) {
					int connectedCount;
					unsigned int scanPlayerIdx;
					if (g_flightPlayerCount == 1) {
						int team = (uint16_t)player->playerIff;
						if (g_missionFlightRuntimeState.teamGoalStatus[team][TEAM_GOAL_PRIMARY] == 1 &&
							Mission_ShouldApplyEndMissionPenalty(playerIdx)) {
							g_missionFlightRuntimeState.teamGoalStatus[team][TEAM_GOAL_PRIMARY] = 2;
						}
						if (g_missionFlightRuntimeState.teamGoalStatus[team][TEAM_GOAL_PRIMARY] != 1) {
							g_flightExitRequest = 1;
							if (g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH &&
								g_missionHeader.body.goalsUnimportant &&
								g_pilotData.numHumanPlayersLastMission == 1) {
								g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION][team] -= 2000;
								player->pendingActionId = 0;
								return;
							}
						} else {
							g_flightExitRequest = 1;
						}
					}
					connectedCount = 0;
					for (scanPlayerIdx = 0; scanPlayerIdx < 8; ++scanPlayerIdx) {
						if (g_players[scanPlayerIdx].connectedFlag == 1) {
							++connectedCount;
						}
					}
					if (connectedCount > 1) {
						if (player->objectIndex != 0xffff) {
							fsfx_UpdateBeamSystemLoop(0, playerIdx);
							fsfx_UpdateIncomingMissileWarning(0);
						}
						Player_UnbindFromCurrentCraft(playerIdx, 0, 1);
					}
					Player_EndFlightParticipation((int)playerIdx);
					if (playerIdx != (unsigned int)g_localPlayer) {
						msg_addMessagePtr(0, NetSession_GetPlayerName((int)playerIdx));
						msg_emitInFlightMessage(MSG_PLAYER_QUIT, g_localPlayer);
					}
					if (g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH &&
						g_missionHeader.body.goalsUnimportant &&
						g_pilotData.numHumanPlayersLastMission == 1) {
						int team = (uint16_t)player->playerIff;
						if (g_missionFlightRuntimeState.teamGoalStatus[team][TEAM_GOAL_PRIMARY] != 1) {
							g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION][team] -= 2000;
							player->pendingActionId = 0;
						}
					}
				} else {
					if (playerIdx == (unsigned int)g_localPlayer) {
						g_flightMissionEndPending = 1;
					}
					player->connectedFlag = 0;
					Flight_UpdateActivePlayerCount();
					if (NetSession_GetHostDplayId() != player->network.directPlayId) {
						FlightNet_MarkPilotNetworkPlayerLeft((int)playerIdx);
					}
					if (playerIdx == (unsigned int)g_localPlayer && NetSession_GetLocalPlayerId() != 0) {
						FlightNet_BroadcastLocalPlayerLeft();
					}
				}
			}
			player->pendingActionId = 0;
			return;

		case 3:
			if (!player->hasCheckpointFlag) {
				uint16_t team = (uint16_t)player->playerIff;
				player->mfd.reinforcementCommandAvailable = 0;
				g_missionFlightRuntimeState.teamReinforcementCalled[team] = 1;
				g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][team] -= 5000;
				if (player->iff == g_players[g_localPlayer].iff) {
					msg_emitInFlightMessage(MSG_REINFORCE, (int)playerIdx);
					fsfx_SpeakTacticalOfficerEvent(15, 176, 0xffffu, 0xffffu);
				}
			}
			player->pendingActionId = 0;
			return;

		case 4:
			if (!player->mapCameraState && craft != NULL) {
				if ((craft->workingSubsystems & 4) == 0) {
					FlightAction_EmitSystemCondition(MSG_TARGCOMPUTER, MSG_DAMAGED, playerIdx);
				} else if (player->currentTargetObjectIdx == player->pendingActionParam) {
					int target =
						Player_FindNearestEnemyFighter(playerIdx, (uint16_t)player->pendingActionParam);
					if (target == 0xffff) {
						player->currentTargetObjectIdx = 0xffffu;
					} else {
						Player_SetTarget(target, (int)playerIdx);
					}
					if (player->pendingActionParam != -1 &&
						g_objectTable[(uint16_t)player->pendingActionParam].objectType != OBJ_None) {
						msg_emitInFlightMessage(MSG_OBJECT_IGNORED, (int)playerIdx);
					} else {
						msg_emitInFlightMessage(MSG_OBJECT_DESTROYED, (int)playerIdx);
					}
					if ((uint16_t)player->pendingActionIssuerPlayerIdx == (uint16_t)g_localPlayer) {
						msg_radioMessage((uint16_t)player->objectIndex,
										 g_objectTable[player->objectIndex].mobj->pCraft,
										 MSG_ACK_IGNORE_TARGET, (GameRand2() & 1) + 7, 0);
					}
				}
			}
			player->pendingActionId = 0;
			return;

		case 6:
		case 10:
			if (!player->mapCameraState && !player->hasCheckpointFlag) {
				unsigned int otherPlayerIdx;
				if (player->pendingActionId == 6 &&
					(uint16_t)player->pendingActionIssuerPlayerIdx == (uint16_t)g_localPlayer) {
					msg_radioMessage((uint16_t)player->objectIndex,
									 g_objectTable[player->objectIndex].mobj->pCraft, MSG_ACK_HEAD_HOME, 3,
									 0);
				}
				if (player->pendingActionId == 10 && g_flightPlayerCount == 1) {
					Player_CycleGunnerSeat(playerIdx, (void*)1);
				}
				Player_HandleHyperspaceCommand(
					craft, playerIdx, player->pendingActionId == 6 ? 4 : (char)player->pendingActionParam);
				for (otherPlayerIdx = 0; otherPlayerIdx < 8; ++otherPlayerIdx) {
					PlayerData* other = &g_players[otherPlayerIdx];
					if (otherPlayerIdx != playerIdx && other->connectedFlag && other->hasCheckpointFlag &&
						other->boundFlightGroupIdx == player->boundFlightGroupIdx) {
						Player_HandleHyperspaceCommand(craft, otherPlayerIdx, 4);
						break;
					}
				}
			}
			player->pendingActionId = 0;
			return;

		case 7:
		case 8:
			if (!player->mapCameraState && !player->hasCheckpointFlag && craft != NULL) {
				craft->throttleSpeed = player->pendingActionId == 7 ? 0 : 0xffffu;
				FlightAction_PlayEngineClick(playerIdx);
				msg_emitInFlightMessage(player->pendingActionId == 7 ? MSG_WAITING_ENGINE_NO
																	 : MSG_GOING_ENGINE_FULL,
										(int)playerIdx);
				if ((uint16_t)player->pendingActionIssuerPlayerIdx == (uint16_t)g_localPlayer) {
					msg_radioMessage((uint16_t)player->objectIndex,
									 g_objectTable[player->objectIndex].mobj->pCraft,
									 player->pendingActionId == 7 ? MSG_ACK_WAITING : MSG_ACK_GOING,
									 player->pendingActionId == 7 ? 4 : 5, 0);
				}
			}
			player->pendingActionId = 0;
			return;

		case 9:
			if (!player->mapCameraState && !player->hasCheckpointFlag) {
				int target = Player_FindNearestEnemyFighter(playerIdx, (uint16_t)player->pendingActionParam);
				if (target == 0xffff) {
					player->currentTargetObjectIdx = 0xffffu;
				} else {
					Player_SetTarget(target, (int)playerIdx);
				}
				msg_emitInFlightMessage(MSG_OBJECT_TARGETED, (int)playerIdx);
			}
			player->pendingActionId = 0;
			return;

		case 11:
			if (!g_filmPlaybackMode) {
				g_flightExitRequest = 2;
				Hud_ClearFlightSurface();
			}
			player->pendingActionId = 0;
			return;

		default:
			player->pendingActionId = 0;
			return;
	}
}

// FUNCTION: XWA 0x4FBA80
void Flight_ProcessPlayerActions(unsigned int playerIdx) {
	int objIdx;
	CraftData* craft = NULL;
	unsigned int currentPlayerIdx = playerIdx;
	int actionIndex = 0;
	objIdx = g_players[currentPlayerIdx].objectIndex;
	if (objIdx != 0xffff) {
		craft = g_objectTable[(uint16_t)objIdx].mobj->pCraft;
	}

	if (g_players[currentPlayerIdx].hyperspacePhase) {
		FlightObject_UpdatePlayerHyperspaceTransition(currentPlayerIdx);
		return;
	}

	if (!g_players[currentPlayerIdx].mapCameraState) {
		switch (g_currentActionKey) {
			case KEY_BACKSPACE:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					craft->throttleSpeed = 0xffffu;
					FlightAction_PlayEngineClick(currentPlayerIdx);
					msg_emitInFlightMessage(MSG_ENGINE_FULL, (int)currentPlayerIdx);
				}
				break;
			case KEY_ENTER:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag &&
					g_players[currentPlayerIdx].currentTargetObjectIdx != 0xffffu) {
					MobileObject* targetMobj =
						g_objectTable[(uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx].mobj;
					if (targetMobj != NULL) {
						if (targetMobj->pCraft != NULL) {
							int16_t shieldRedirect = craft->shieldRedirect;
							uint16_t targetSpeed = targetMobj->speed;
							uint16_t powerMargin =
								(uint16_t)(6 - shieldRedirect - craft->laserRedirect - craft->beamLevel);
							uint16_t maxSpeed;
							if (powerMargin < 0x8000u) {
								maxSpeed = (uint16_t)(craft->aiFlight.maxSpeedCache +
													  MATH2_fraction((uint16_t)(powerMargin << 13),
																	 craft->aiFlight.maxSpeedCache));
							} else {
								maxSpeed = (uint16_t)(craft->aiFlight.maxSpeedCache -
													  MATH2_fraction((uint16_t)(-8192 * powerMargin),
																	 craft->aiFlight.maxSpeedCache));
							}
							if (targetSpeed < maxSpeed) {
								craft->throttleSpeed = (uint16_t)MATH2_divide(targetSpeed, maxSpeed);
								msg_emitInFlightMessage(MSG_MATCHING_SPEEDS, (int)currentPlayerIdx);
							} else {
								craft->throttleSpeed = 0xffffu;
								msg_emitInFlightMessage(MSG_TRY_MATCHING, (int)currentPlayerIdx);
							}
						} else {
							craft->throttleSpeed = 0xffffu;
						}
					} else {
						craft->throttleSpeed = 0;
					}
					FlightAction_PlayEngineClick(currentPlayerIdx);
				}
				break;
			case KEY_SEMICOLON:
				if (!FlightAction_TransferShieldsToLasers(currentPlayerIdx, &g_objectTable[(uint16_t)objIdx],
														  craft)) {
					FlightAction_PlayEngineClick(currentPlayerIdx);
				}
				break;
			case KEY_QUOTES:
				FlightAction_TransferAllLasersToShields(currentPlayerIdx, &g_objectTable[(uint16_t)objIdx],
														craft);
				break;
			case KEY_APOSTROPHE:
				FlightAction_TransferLasersToShields(currentPlayerIdx, &g_objectTable[(uint16_t)objIdx],
													 craft);
				break;
			case KEY_PLUS: {
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					uint16_t oldSpeed = craft->throttleSpeed;
					craft->throttleSpeed = (uint16_t)(craft->throttleSpeed + 2048);
					if (craft->throttleSpeed < oldSpeed) {
						craft->throttleSpeed = 0xffffu;
					}
					FlightAction_PlayEngineClick(currentPlayerIdx);
				}
				break;
			}
			case KEY_MINUS: {
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					uint16_t oldSpeed = craft->throttleSpeed;
					craft->throttleSpeed = (uint16_t)(craft->throttleSpeed - 2048);
					if (craft->throttleSpeed > oldSpeed) {
						craft->throttleSpeed = 0;
					}
					FlightAction_PlayEngineClick(currentPlayerIdx);
				}
				break;
			}
			case KEY_FOWARD_SLASH:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					craft->throttleSpeed = 0;
					FlightAction_PlayEngineClick(currentPlayerIdx);
					msg_emitInFlightMessage(MSG_ENGINE_NO, (int)currentPlayerIdx);
				}
				break;
			case KEY_LEFT_BRACKET:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					craft->throttleSpeed = 21845;
					FlightAction_PlayEngineClick(currentPlayerIdx);
					msg_emitInFlightMessage(MSG_ENGINE_13, (int)currentPlayerIdx);
				}
				break;
			case KEY_RIGHT_BRACKET:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					craft->throttleSpeed = 43690u;
					FlightAction_PlayEngineClick(currentPlayerIdx);
					msg_emitInFlightMessage(MSG_ENGINE_23, (int)currentPlayerIdx);
				}
				break;
			case KEY_PERIOD:
				if (!g_flightSimSideEffectsSuppressed) {
					if (!g_players[currentPlayerIdx].viewState.externalCameraActive &&
						(g_players[currentPlayerIdx].cockpitLookAvailable ||
						 g_players[currentPlayerIdx].cockpitToggleAvailable)) {
						g_players[currentPlayerIdx].padlockActive = 0;
						g_players[currentPlayerIdx].cockpitVisible =
							g_players[currentPlayerIdx].cockpitVisible == 0;
					}
					FlightAction_ResetLook(&g_players[currentPlayerIdx]);
				}
				break;
			case KEY_B:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					if ((craft->systemFlags & 0x100) == 0) {
						msg_emitInFlightMessage(MSG_NOT_EQUIPPED_BEAM, (int)currentPlayerIdx);
					} else if ((craft->workingSubsystems & 0x100) == 0) {
						FlightAction_PlayEngineClick(currentPlayerIdx);
						FlightAction_EmitSystemCondition(MSG_BEAM, MSG_DAMAGED, currentPlayerIdx);
					} else if (craft->beamActive) {
						craft->beamActive = 0;
						craft->beamTimer = 0;
						FlightAction_PlayEngineClick(currentPlayerIdx);
						msg_emitInFlightMessage((uint16_t)(craft->beamTypeId + 307), (int)currentPlayerIdx);
					} else if (craft->beamPresent) {
						craft->beamActive = 1;
						craft->beamTimer = -1;
						msg_emitInFlightMessage(
							(uint16_t)(craft->beamTypeId +
									   (g_players[currentPlayerIdx].currentTargetObjectIdx != 0xffffu ||
												craft->beamTypeId == 3
											? 301
											: 313)),
							(int)currentPlayerIdx);
						if (craft->beamTypeId == 1)
							fsfx_PlaySound(79, 0xffffu, currentPlayerIdx);
						if (craft->beamTypeId == 2)
							fsfx_PlaySound(82, 0xffffu, currentPlayerIdx);
						if (craft->beamTypeId == 3)
							fsfx_PlaySound(85, 0xffffu, currentPlayerIdx);
						if (craft->beamTypeId == 4)
							fsfx_PlaySound(87, 0xffffu, currentPlayerIdx);
					} else {
						FlightAction_PlayEngineClick(currentPlayerIdx);
						msg_emitInFlightMessage(MSG_BEAM_NO_ENERGY, (int)currentPlayerIdx);
					}
				}
				break;
			case KEY_C:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					if ((craft->workingSubsystems & 2) == 0) {
						FlightAction_PlayEngineClick(currentPlayerIdx);
						FlightAction_EmitSystemCondition(MSG_COUNTERMEASURE, MSG_UNAVAILABLE,
														 currentPlayerIdx);
					} else if (craft->cmFireCooldownTimer == 0) {
						if (craft->cmTypeId == 0) {
							msg_emitInFlightMessage(MSG_NO_DCM, (int)currentPlayerIdx);
							FlightAction_PlayEngineClick(currentPlayerIdx);
						} else if (craft->cmAmmoCount == 0) {
							msg_emitInFlightMessage((uint16_t)(craft->cmTypeId + 456), (int)currentPlayerIdx);
							FlightAction_PlayEngineClick(currentPlayerIdx);
						} else if (craft->cmTypeId == 1) {
							craft->chaffActiveTimer = (uint16_t)(craft->chaffActiveTimer + 10);
							msg_emitInFlightMessage(MSG_CHAFF_FIRED, (int)currentPlayerIdx);
							if (g_missionFlightGroups[g_objectTable[(uint16_t)objIdx].flightGroupIdx]
										.fg.status1 != 21 &&
								g_missionFlightGroups[g_objectTable[(uint16_t)objIdx].flightGroupIdx]
										.fg.status2 != 21) {
								--craft->cmAmmoCount;
							}
							fsfx_PlaySound(21, 0xffffu, currentPlayerIdx);
						} else if (laser_createcountermeasureprojectile(objIdx, OBJ_WarheadFlare) != 0xffff) {
							msg_emitInFlightMessage(MSG_FLARE_FIRED, (int)currentPlayerIdx);
							fsfx_PlaySound(23, 0xffffu, currentPlayerIdx);
						}
					}
				}
				break;
			case KEY_F:
				if (!g_provingGroundsModeActive && !g_players[currentPlayerIdx].hasCheckpointFlag &&
					!g_players[currentPlayerIdx].aiControlledFlag) {
					Player_AutoGunnerToggle(currentPlayerIdx);
				}
				break;
			case KEY_H:
				if (currentPlayerIdx == (unsigned int)g_localPlayer && !g_flightSimSideEffectsSuppressed &&
					g_flightPlayerCount == 1) {
					fsfx_PlaySound(61, 0xffffu, currentPlayerIdx);
					msg_emitInFlightMessage(MSG_HANGAR_RESTART, (int)currentPlayerIdx);
					g_players[currentPlayerIdx].pendingActionId = 11;
					g_players[currentPlayerIdx].pendingActionTimer = 1416;
				}
				break;
			case KEY_J:
				if (!g_flightSimSideEffectsSuppressed &&
					(g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
					 g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH)) {
					if (g_flightCraftJumpingEnabled) {
						uint16_t oldObjIdx = objIdx;
						if (Player_UnbindFromCurrentCraft((int)currentPlayerIdx, 1, 1)) {
							Player_BindToAvailableCraft(currentPlayerIdx, oldObjIdx, 0, 0);
							if (currentPlayerIdx == (unsigned int)g_localPlayer) {
								msg_emitLocalPlayerCraftMessage(MSG_JUMP_TO_NEW_CRAFT);
							}
						} else {
							msg_emitInFlightMessage(MSG_NO_WINGMEN, (int)currentPlayerIdx);
						}
					} else {
						msg_emitInFlightMessage(MSG_NO_JUMPING, (int)currentPlayerIdx);
					}
				}
				break;
			case KEY_L:
				if (!g_players[currentPlayerIdx].viewState.externalCameraActive &&
					!g_flightSimSideEffectsSuppressed && (g_flightPlayerCount == 1 || !g_asyncFlag) &&
					!g_players[currentPlayerIdx].currentSeatIdx &&
					!g_players[currentPlayerIdx].aiControlledFlag &&
					g_players[currentPlayerIdx].currentTargetObjectIdx != 0xffffu &&
					g_players[currentPlayerIdx].cockpitLookAvailable) {
					uint8_t wasPadlocked = g_players[currentPlayerIdx].padlockActive;
					g_players[currentPlayerIdx].cockpitVisible = 1;
					g_players[currentPlayerIdx].padlockActive = wasPadlocked == 0;
					if (wasPadlocked) {
						g_players[currentPlayerIdx].lookYawOffset = 0;
						g_players[currentPlayerIdx].lookPitchOffset = 0;
					}
				}
				break;
			case KEY_S:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					if ((craft->systemFlags & 1) == 0) {
						FlightAction_PlayEngineClick(currentPlayerIdx);
						msg_emitInFlightMessage(MSG_NOT_EQUIPPED_SHIELDS, (int)currentPlayerIdx);
					} else if ((craft->workingSubsystems & 1) == 0) {
						FlightAction_PlayEngineClick(currentPlayerIdx);
						FlightAction_EmitSystemCondition(MSG_SHIELDS, MSG_DAMAGED, currentPlayerIdx);
					} else {
						ModelIndex modelIndex =
							(ModelIndex)GetModelIndexFromType(g_objectTable[(uint16_t)objIdx].objectType);
						if (modelIndex == (ModelIndex)0xffffu) {
							break;
						}
						g_hudShieldPercentLabelsInitialized = 0;
						craft->shieldDistribMode = (uint8_t)(craft->shieldDistribMode + 1);
						if (craft->shieldDistribMode == 1) {
							int maxShield = Craft_GetObjectMaxShield(
								(unsigned short)g_players[currentPlayerIdx].objectIndex);
							uint16_t shieldScale = (uint16_t)MATH2_percentage(
								g_modelDefs[(uint16_t)modelIndex].shieldStrength, (uint32_t)maxShield);
							int totalShield =
								(int16_t)((uint16_t)craft->shieldFront + (uint16_t)craft->shieldRear);
							if (totalShield > 0) {
								uint16_t frontShield =
									(uint16_t)MATH2_fraction((uint16_t)totalShield, shieldScale);
								craft->shieldFront = frontShield;
								craft->shieldRear = totalShield - frontShield;
							}
						} else if (craft->shieldDistribMode == 2) {
							Player_TransferShieldBankEnergy(1, 0, (int)currentPlayerIdx);
						} else {
							craft->shieldDistribMode = 0;
							Player_TransferShieldBankEnergy(0, 1, (int)currentPlayerIdx);
						}
						msg_emitInFlightMessage((uint16_t)(craft->shieldDistribMode + MSG_SHIELD_FULL_FWD),
												(int)currentPlayerIdx);
						FlightAction_PlayAcceptedClick(currentPlayerIdx);
					}
				}
				break;
			case KEY_V:
				if (objIdx != 0xffff && (g_objectTable[(uint16_t)objIdx].objectType == OBJ_XWing ||
										 g_objectTable[(uint16_t)objIdx].objectType == OBJ_BWing)) {
					if ((craft->sFoilState & 1) == 0) {
						craft->sFoilState ^= 2u;
						craft->sFoilState |= 1u;
						if (currentPlayerIdx == (unsigned int)g_localPlayer) {
							fsfx_PlaySound(120, 0xffffu, currentPlayerIdx);
						}
						msg_emitInFlightMessage((craft->sFoilState & 2) ? MSG_SFOILS_CLOSING
																		: MSG_SFOILS_OPENING,
												(int)currentPlayerIdx);
					}
				} else {
					msg_emitInFlightMessage(MSG_NOT_EQUIPPED_SFOIL, (int)currentPlayerIdx);
				}
				break;
			case KEY_W:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag &&
					!g_players[currentPlayerIdx].currentSeatIdx) {
					g_players[currentPlayerIdx].selectedWarhead =
						(uint8_t)(g_players[currentPlayerIdx].selectedWarhead + 1);
					if (g_players[currentPlayerIdx].selectedWeaponMode) {
						if (g_players[currentPlayerIdx].selectedWarhead >= craft->warheadLauncherCount) {
							g_players[currentPlayerIdx].selectedWarhead = 0;
							if (craft->cannonClassCount) {
								g_players[currentPlayerIdx].selectedWeaponMode = 0;
							}
						}
					} else if (g_players[currentPlayerIdx].selectedWarhead >= craft->cannonClassCount) {
						g_players[currentPlayerIdx].selectedWarhead = 0;
						if (craft->warheadLauncherCount) {
							ModelIndex modelIndex =
								(ModelIndex)GetModelIndexFromType(g_objectTable[(uint16_t)objIdx].objectType);
							if (modelIndex != (ModelIndex)0xffffu) {
								uint8_t firstSlot =
									g_modelDefs[(uint16_t)modelIndex].warheadLauncherFirstSlot[0];
								uint8_t primaryCount = craft->warheadData[firstSlot].count;
								uint8_t secondaryCount = craft->warheadData[firstSlot + 1u].count;
								if ((primaryCount + secondaryCount) == 0 &&
									craft->warheadLauncherCount > 1u) {
									firstSlot = g_modelDefs[(uint16_t)modelIndex].warheadLauncherFirstSlot[1];
									primaryCount = craft->warheadData[firstSlot].count;
									secondaryCount = craft->warheadData[firstSlot + 1u].count;
								}
								if ((primaryCount + secondaryCount) != 0) {
									g_players[currentPlayerIdx].selectedWeaponMode = 1;
									g_players[currentPlayerIdx].missileLockState = 0;
									craft->warheadLockTicks = 0;
									if ((craft->warheadLauncherFlags[0] & 0x7f) != 3) {
										if ((craft->warheadLauncherFlags[0] & 0x80) == 0) {
											if (primaryCount < secondaryCount) {
												craft->warheadLauncherFlags[0] |= 0x80;
											}
										} else if (secondaryCount < primaryCount) {
											craft->warheadLauncherFlags[0] &= 0x7f;
										}
									}
								}
							}
						}
					}
					if (g_players[currentPlayerIdx].selectedWeaponMode) {
						if ((craft->workingSubsystems & 8) != 0) {
							msg_emitInFlightMessage(
								(uint16_t)(MSG_LAUNCHER_ARMED_PROTON +
										   FlightAction_WarheadMessageBase(
											   craft->warheadSlotTypeIds[g_players[currentPlayerIdx]
																			 .selectedWarhead],
											   currentPlayerIdx)),
								(int)currentPlayerIdx);
							fsfx_PlaySound(73, 0xffffu, currentPlayerIdx);
						} else {
							FlightAction_EmitSystemCondition(MSG_PROTON, MSG_DAMAGED, currentPlayerIdx);
							fsfx_PlaySound(70, 0xffffu, currentPlayerIdx);
						}
					} else if ((craft->workingSubsystems & 0x10) != 0) {
						msg_emitInFlightMessage(
							(uint16_t)(g_players[currentPlayerIdx].selectedWarhead + MSG_LASERS_ARMED),
							(int)currentPlayerIdx);
						fsfx_PlaySound(g_players[currentPlayerIdx].selectedWarhead ? 72 : 71, 0xffffu,
									   currentPlayerIdx);
					} else {
						FlightAction_EmitSystemCondition(
							(uint16_t)(MSG_LASER + g_players[currentPlayerIdx].selectedWarhead), MSG_DAMAGED,
							currentPlayerIdx);
						fsfx_PlaySound(70, 0xffffu, currentPlayerIdx);
					}
				}
				break;
			case KEY_X:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag &&
					!g_players[currentPlayerIdx].currentSeatIdx) {
					if (!g_players[currentPlayerIdx].selectedWeaponMode) {
						if (objIdx != 0xffff && g_objectTable[(uint16_t)objIdx].genusId != GENUS_Fighter) {
							if (g_players[currentPlayerIdx].turretAutoFireState) {
								uint8_t i;
								g_players[currentPlayerIdx].turretAutoFireState = 0;
								msg_emitInFlightMessage(MSG_GUNNER_LINKED, (int)currentPlayerIdx);
								for (i = 0; i < craft->laserSlotCount; ++i) {
									if (craft->warheadData[i].weaponType >= 4) {
										craft->warheadData[i].turretTargetObjIdx = -1;
									}
								}
							} else {
								g_players[currentPlayerIdx].turretAutoFireState = 1;
								msg_emitInFlightMessage(MSG_GUNNER_DEFENSIVE, (int)currentPlayerIdx);
							}
						} else {
							uint8_t mode;
							uint8_t group = g_players[currentPlayerIdx].selectedWarhead;
							ModelIndex modelIndex =
								(ModelIndex)GetModelIndexFromType(g_objectTable[(uint16_t)objIdx].objectType);
							if (modelIndex == (ModelIndex)0xffffu ||
								g_modelDefs[(uint16_t)modelIndex].laserGroupSlotCount[group] == 1) {
								fsfx_PlaySound(133, 0xffffu, currentPlayerIdx);
								break;
							}
							mode = (uint8_t)(craft->laserLinkMode[group] + 1);
							if (mode == 4 && craft->cannonClassCount == 2) {
								craft->laserLinkMode[0] = 4;
								craft->laserLinkMode[1] = 4;
							}
							if (mode > (craft->cannonClassCount == 2 ? 4 : 3)) {
								mode = 1;
								if (craft->cannonClassCount == 2) {
									if (group) {
										craft->laserLinkMode[0] = 1;
									} else {
										craft->laserLinkMode[1] = 1;
									}
								}
							}
							if (g_modelDefs[(uint16_t)modelIndex].laserGroupSlotCount[group] != 4 &&
								mode == 2) {
								mode = 3;
							}
							craft->laserLinkMode[group] = mode;
							craft->laserLinkNextSlot[group] =
								g_modelDefs[(uint16_t)modelIndex].laserGroupFirstSlot[group];
							if (currentPlayerIdx == (unsigned int)g_localPlayer) {
								msg_emitInFlightMessage((uint16_t)(mode + MSG_LASERS_ARMED + 1),
														(int)currentPlayerIdx);
							}
						}
					} else {
						uint8_t selected = g_players[currentPlayerIdx].selectedWarhead;
						int msgBase = FlightAction_WarheadMessageBase(craft->warheadSlotTypeIds[selected],
																	  currentPlayerIdx);
						craft->warheadLauncherFlags[selected] ^= 2u;
						msg_emitInFlightMessage(
							(uint16_t)(msgBase + ((craft->warheadLauncherFlags[selected] & 2)
													  ? MSG_LAUNCHER_DUAL_PROTON
													  : MSG_LAUNCHER_SINGLE_PROTON)),
							(int)currentPlayerIdx);
					}
					fsfx_PlaySound(133, 0xffffu, currentPlayerIdx);
				}
				break;
			case KEY_Z:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					uint8_t convergeMode = g_modelDefs[craft->modelIndex].laserConvergeMode;
					if (convergeMode == 1) {
						craft->laserConvergeLevel = (uint8_t)((craft->laserConvergeLevel + 1) % 4);
						msg_emitInFlightMessage(
							(uint16_t)(craft->laserConvergeLevel + MSG_LASERS_CONVERGE_OFF),
							(int)currentPlayerIdx);
						FlightAction_PlayEngineClick(currentPlayerIdx);
					} else if (convergeMode == 2) {
						craft->laserConvergeLevel ^= 4u;
						msg_emitInFlightMessage(
							(uint16_t)(craft->laserConvergeLevel + MSG_LASERS_CONVERGE_OFF),
							(int)currentPlayerIdx);
						FlightAction_PlayEngineClick(currentPlayerIdx);
					} else {
						msg_emitInFlightMessage(MSG_LASERS_CONVERGE_NA, (int)currentPlayerIdx);
						FlightAction_PlayAcceptedClick(currentPlayerIdx);
					}
				}
				break;
			case KEY_ALT_E:
				if (!g_flightSimSideEffectsSuppressed && !g_players[currentPlayerIdx].hasCheckpointFlag) {
					Player_ReleaseCarriedObject(currentPlayerIdx);
					Player_HandleCraftDestruction(currentPlayerIdx);
				}
				break;
			case KEY_ALT_1:
				if (!g_players[currentPlayerIdx].viewState.playerInputBlocked) {
					Player_SetTarget(Player_PickTargetInSight((int)currentPlayerIdx), (int)currentPlayerIdx);
				}
				break;
			case KEY_ALT_2:
				if (!g_players[currentPlayerIdx].viewState.playerInputBlocked) {
					laser_fireplayerweapon((int)currentPlayerIdx);
				}
				break;
			case KEY_PAD_0:
				if (!g_flightSimSideEffectsSuppressed &&
					(g_players[currentPlayerIdx].viewState.hudStateLive < 0x10u ||
					 g_players[currentPlayerIdx].viewState.externalCameraActive)) {
					g_players[currentPlayerIdx].viewState.hudAimXSnapState ^= 8u;
					g_players[currentPlayerIdx].viewState.hudAimX =
						(uint16_t)(g_players[currentPlayerIdx].viewState.hudAimXSnapState << 10);
					if (!g_players[currentPlayerIdx].viewState.externalCameraActive &&
						g_players[currentPlayerIdx].viewState.cameraFocusObjIdx ==
							g_players[currentPlayerIdx].objectIndex) {
						Hud_SetHudViewState(g_players[currentPlayerIdx].viewState.hudStateLive ^ 8,
											(int)currentPlayerIdx);
					}
				}
				break;
			case KEY_PAD_MINUS: {
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					uint16_t oldSpeed = craft->throttleSpeed;
					craft->throttleSpeed = (uint16_t)(craft->throttleSpeed - 2048);
					if (craft->throttleSpeed > oldSpeed) {
						craft->throttleSpeed = 0;
					}
					FlightAction_PlayEngineClick(currentPlayerIdx);
				}
				break;
			}
			case KEY_F8:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					if ((craft->systemFlags & 0x100) == 0) {
						msg_emitInFlightMessage(MSG_NOT_EQUIPPED_BEAM, (int)currentPlayerIdx);
						FlightAction_PlayEngineClick(currentPlayerIdx);
					} else if ((craft->workingSubsystems & 0x100) != 0) {
						craft->beamLevel = (uint8_t)((craft->beamLevel + 1) % 5);
						msg_emitInFlightMessage((uint16_t)(craft->beamLevel + MSG_BEAM_REDIRECT_OFF),
												(int)currentPlayerIdx);
						fsfx_PlaySound(craft->beamLevel + 70, 0xffffu, currentPlayerIdx);
					} else {
						FlightAction_EmitSystemCondition(MSG_BEAM, MSG_DAMAGED, currentPlayerIdx);
						FlightAction_PlayEngineClick(currentPlayerIdx);
					}
					if (!g_useHardware3D)
						g_hudElementEnabled[9].enabled = 1;
				}
				break;
			case KEY_F9:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					craft->laserRedirect = (uint8_t)((craft->laserRedirect + 1) % 5);
					if (currentPlayerIdx == (unsigned int)g_localPlayer) {
						if ((craft->workingSubsystems & 0x10) != 0) {
							msg_emitInFlightMessage((uint16_t)(craft->laserRedirect + MSG_LASER_REDIRECT_OFF),
													(int)currentPlayerIdx);
						} else {
							FlightAction_EmitSystemCondition(MSG_LASER, MSG_DAMAGED, currentPlayerIdx);
						}
						fsfx_PlaySound(craft->laserRedirect + 70, 0xffffu, currentPlayerIdx);
					}
					if (!g_useHardware3D)
						g_hudElementEnabled[9].enabled = 1;
				}
				break;
			case KEY_F10:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					if ((craft->systemFlags & 1) == 0) {
						msg_emitInFlightMessage(MSG_NOT_EQUIPPED_SHIELDS, (int)currentPlayerIdx);
						FlightAction_PlayEngineClick(currentPlayerIdx);
					} else if ((craft->workingSubsystems & 1) != 0) {
						craft->shieldRedirect = (uint8_t)((craft->shieldRedirect + 1) % 5);
						msg_emitInFlightMessage((uint16_t)(craft->shieldRedirect + MSG_SHIELD_REDIRECT_OFF),
												(int)currentPlayerIdx);
						fsfx_PlaySound(craft->shieldRedirect + 70, 0xffffu, currentPlayerIdx);
					} else {
						FlightAction_EmitSystemCondition(MSG_SHIELDS, MSG_DAMAGED, currentPlayerIdx);
						FlightAction_PlayEngineClick(currentPlayerIdx);
					}
					if (!g_useHardware3D)
						g_hudElementEnabled[9].enabled = 1;
				}
				break;
			case KEY_F11:
			case KEY_F12:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					int preset = g_currentActionKey != KEY_F11;
					craft->throttleSpeed = (uint16_t)g_players[currentPlayerIdx].throttlePreset[preset];
					craft->laserRedirect = g_players[currentPlayerIdx].laserPreset[preset];
					if ((craft->systemFlags & 1) != 0)
						craft->shieldRedirect = g_players[currentPlayerIdx].shieldPreset[preset];
					if ((craft->systemFlags & 0x100) != 0)
						craft->beamLevel = g_players[currentPlayerIdx].beamPreset[preset];
					FlightAction_PlayEngineClick(currentPlayerIdx);
				}
				break;
			case KEY_SHIFT_F9:
				if (!FlightAction_TransferShieldsToLasers(currentPlayerIdx, &g_objectTable[(uint16_t)objIdx],
														  craft)) {
					FlightAction_PlayEngineClick(currentPlayerIdx);
				}
				break;
			case KEY_SHIFT_F10:
				FlightAction_TransferLasersToShields(currentPlayerIdx, &g_objectTable[(uint16_t)objIdx],
													 craft);
				break;
			case KEY_SHIFT_F11:
			case KEY_SHIFT_F12:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					int preset = g_currentActionKey != KEY_SHIFT_F11;
					g_players[currentPlayerIdx].throttlePreset[preset] = (int16_t)craft->throttleSpeed;
					g_players[currentPlayerIdx].laserPreset[preset] = craft->laserRedirect;
					g_players[currentPlayerIdx].shieldPreset[preset] = craft->shieldRedirect;
					g_players[currentPlayerIdx].beamPreset[preset] = craft->beamLevel;
					if (currentPlayerIdx == (unsigned int)g_localPlayer) {
						g_gameConfig.presetThrottle[preset] = 100u * craft->throttleSpeed / 0xffffu;
						g_gameConfig.presetLaser[preset] = craft->laserRedirect;
						g_gameConfig.presetShield[preset] = craft->shieldRedirect;
						g_gameConfig.presetBeam[preset] = craft->beamLevel;
					}
					msg_emitInFlightMessage(MSG_CONFIGURATION_SAVED, (int)currentPlayerIdx);
					FlightAction_PlayEngineClick(currentPlayerIdx);
				}
				break;
			case KEY_THROTTLE_1:
			case KEY_THROTTLE_2:
			case KEY_THROTTLE_3:
			case KEY_THROTTLE_4:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					craft->throttleSpeed = (uint16_t)((g_currentActionKey + 6) << 12);
				}
				break;
			case KEY_THROTTLE_6:
			case KEY_THROTTLE_7:
			case KEY_THROTTLE_8:
			case KEY_THROTTLE_9:
			case KEY_THROTTLE_10:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					craft->throttleSpeed = (uint16_t)((g_currentActionKey + 7) << 12);
				}
				break;
			case KEY_THROTTLE_11:
			case KEY_THROTTLE_12:
			case KEY_THROTTLE_13:
			case KEY_THROTTLE_14:
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					craft->throttleSpeed = (uint16_t)((g_currentActionKey + 8) << 12);
				}
				break;
			case KEY_MFD_OVERLAY:
				if (!g_players[currentPlayerIdx].viewState.externalCameraActive) {
					Hud_ToggleMfdOverlay((int)currentPlayerIdx);
				}
				break;
			case KEY_EQUAL:
			case KEY_PAD_PLUS: {
				if (!g_players[currentPlayerIdx].hasCheckpointFlag) {
					uint16_t oldSpeed = craft->throttleSpeed;
					craft->throttleSpeed = (uint16_t)(craft->throttleSpeed + 2048);
					if (craft->throttleSpeed < oldSpeed) {
						craft->throttleSpeed = 0xffffu;
					}
					FlightAction_PlayEngineClick(currentPlayerIdx);
				}
				break;
			}
			case KEY_SHIFT_B:
				if (!g_players[currentPlayerIdx].aiControlledFlag)
					Player_HandleResupplyCommand(currentPlayerIdx, 0xffff);
				break;
			case KEY_SHIFT_C:
				Player_HandleCoverMeCommand((int)currentPlayerIdx, 0xffff);
				break;
			case KEY_SHIFT_D:
				if (!g_players[currentPlayerIdx].aiControlledFlag)
					Player_HandleDockBoardCommand(currentPlayerIdx);
				break;
			case KEY_SHIFT_P:
				if (!g_players[currentPlayerIdx].aiControlledFlag)
					Player_HandlePickupCommand(currentPlayerIdx);
				break;
			case KEY_SHIFT_R:
				if (!g_players[currentPlayerIdx].aiControlledFlag)
					Player_ReleaseCarriedObject(currentPlayerIdx);
				break;
			default:
				break;
		}
	}

	else {
		switch (g_currentActionKey) {
			case KEY_C:
				if (g_players[currentPlayerIdx].mapCameraState > 1 &&
					g_players[currentPlayerIdx].currentTargetObjectIdx != 0xffffu) {
					g_players[currentPlayerIdx].viewState.savedTargetX =
						g_objectTable[(uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx].world_x;
					g_players[currentPlayerIdx].viewState.savedTargetY =
						g_objectTable[(uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx].world_y;
				}
				break;
			case KEY_H: {
				uint8_t leftEnabled = g_players[currentPlayerIdx].mfd.enabled[1];
				uint8_t rightEnabled = g_players[currentPlayerIdx].mfd.enabled[2];
				if (leftEnabled != 1 || g_players[currentPlayerIdx].mfd.page[1] != 8) {
					if (rightEnabled != 1 || g_players[currentPlayerIdx].mfd.page[2] != 8) {
						if ((leftEnabled || rightEnabled) && leftEnabled == 1) {
							if (rightEnabled == 1) {
								Hud_SetMfdPage((int)currentPlayerIdx, 1, 8);
							} else {
								Hud_ToggleMfdSide((int)currentPlayerIdx, 2);
								Hud_SetMfdPage((int)currentPlayerIdx, 2, 8);
							}
						} else {
							Hud_ToggleMfdSide((int)currentPlayerIdx, 1);
							Hud_SetMfdPage((int)currentPlayerIdx, 1, 8);
						}
					}
				}
				break;
			}
			case KEY_Z:
				if (g_players[currentPlayerIdx].mapCameraState <= 1) {
					int focusObjIdx = g_players[currentPlayerIdx].viewState.cameraFocusObjIdx;
					if (focusObjIdx != 0xffff) {
						g_players[currentPlayerIdx].viewState.cameraDistance =
							g_modelTypeTable[(uint16_t)g_objectTable[focusObjIdx].objectType]
								.maxBoundsExtent +
							512;
					} else if (g_players[currentPlayerIdx].currentTargetObjectIdx != 0xffffu) {
						g_players[currentPlayerIdx].viewState.savedTargetX =
							g_objectTable[(uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx]
								.world_x;
						g_players[currentPlayerIdx].viewState.savedTargetY =
							g_objectTable[(uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx]
								.world_y;
						g_players[currentPlayerIdx].viewState.savedTargetZ =
							g_objectTable[(uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx]
								.world_z;
						g_players[currentPlayerIdx].viewState.cameraDistance =
							g_modelTypeTable[(uint16_t)g_objectTable[(uint16_t)g_players[currentPlayerIdx]
																		 .currentTargetObjectIdx]
												 .objectType]
								.maxBoundsExtent +
							512;
						FVIEW_BuildCameraOrient(0, (int16_t)g_players[currentPlayerIdx].viewState.viewPitch,
												(int16_t)g_players[currentPlayerIdx].viewState.viewYaw, 0, 0,
												0, NULL, -1);
						g_players[currentPlayerIdx].viewState.savedTargetX -=
							Xwa_Q15Mul(g_camMatR2_X, g_players[currentPlayerIdx].viewState.cameraDistance);
						g_players[currentPlayerIdx].viewState.savedTargetY -=
							Xwa_Q15Mul(g_camMatR2_Y, g_players[currentPlayerIdx].viewState.cameraDistance);
						g_players[currentPlayerIdx].viewState.savedTargetZ -=
							Xwa_Q15Mul(g_camMatR2_Z, g_players[currentPlayerIdx].viewState.cameraDistance);
					}
				} else if (g_players[currentPlayerIdx].currentTargetObjectIdx != 0xffffu) {
					g_players[currentPlayerIdx].viewState.savedTargetX =
						g_objectTable[(uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx].world_x;
					g_players[currentPlayerIdx].viewState.savedTargetY =
						g_objectTable[(uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx].world_y;
					g_players[currentPlayerIdx].viewState.savedTargetZ =
						g_objectTable[(uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx].world_z +
						16 * g_modelTypeTable[(uint16_t)g_objectTable[(uint16_t)g_players[currentPlayerIdx]
																		  .currentTargetObjectIdx]
												  .objectType]
								 .maxBoundsExtent;
				}
				break;
			case KEY_PAD_MINUS:
				if (g_players[currentPlayerIdx].viewState.aimTargetIdx == 0xffff) {
					if (g_players[currentPlayerIdx].currentTargetObjectIdx != 0xffffu) {
						g_players[currentPlayerIdx].viewState.aimTargetIdx =
							(uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx;
						if (g_players[currentPlayerIdx].mapCameraState > 1) {
							g_players[currentPlayerIdx].mapCameraState = 1;
							g_players[currentPlayerIdx].viewState.hudAimX = 0;
						}
					}
				} else {
					g_players[currentPlayerIdx].viewState.aimTargetIdx = 0xffff;
				}
				break;
			default:
				break;
		}
	}

	switch (g_currentActionKey) {
		case KEY_SHIFT_1:
		case KEY_SHIFT_2:
		case KEY_SHIFT_3:
		case KEY_SHIFT_4: {
			unsigned int dstPlayerIdx;
			switch (g_currentActionKey) {
				case KEY_SHIFT_1:
					actionIndex = 0;
					break;
				case KEY_SHIFT_2:
					actionIndex = 1;
					break;
				case KEY_SHIFT_3:
					actionIndex = 2;
					break;
				default:
					actionIndex = 3;
					break;
			}
			msg_addMessagePtr(0, NetSession_GetPlayerName((int)currentPlayerIdx));
			msg_addMessagePtr(1, &g_playerTauntText[currentPlayerIdx][actionIndex * 70u]);
			for (dstPlayerIdx = 0; dstPlayerIdx < 8; ++dstPlayerIdx) {
				if (g_players[dstPlayerIdx].connectedFlag) {
					g_msgSenderIff = 3;
					msg_emitInFlightMessage(MSG_PLAYER_MESSAGE, (int)dstPlayerIdx);
				}
			}
			msg_emitInFlightMessage(MSG_MESSAGE_SENT, (int)currentPlayerIdx);
			return;
		}
		case KEY_STAR:
		case KEY_PAD_STAR:
			if (!g_flightSimSideEffectsSuppressed && !g_replayViewMode) {
				if (!g_players[currentPlayerIdx].mapCameraState) {
					if (!g_players[currentPlayerIdx].viewState.externalCameraActive) {
						FlightAction_PlayEngineClick(currentPlayerIdx);
					} else {
						g_players[currentPlayerIdx].viewState.playerInputBlocked =
							g_players[currentPlayerIdx].viewState.playerInputBlocked == 0;
						FlightAction_PlayAcceptedClick(currentPlayerIdx);
					}
				} else if (g_players[currentPlayerIdx].viewState.cameraFocusObjIdx == 0xffff) {
					Player_StepExtView((int)currentPlayerIdx);
				} else {
					g_players[currentPlayerIdx].viewState.cameraFocusObjIdx = 0xffff;
				}
			}
			return;
		case KEY_COMMA:
			FlightAction_CycleTargetComponent(currentPlayerIdx, 1);
			return;
		case KEY_SLASH:
		case KEY_PAD_SLASH:
			if (!g_flightSimSideEffectsSuppressed && !g_players[currentPlayerIdx].mapCameraState) {
				if (g_players[currentPlayerIdx].viewState.cameraFocusObjIdx ==
					g_players[currentPlayerIdx].objectIndex) {
					Player_StepExtView((int)currentPlayerIdx);
				}
				if (g_players[currentPlayerIdx].mapCameraState ||
					g_players[currentPlayerIdx].viewState.cameraFocusObjIdx !=
						g_players[currentPlayerIdx].objectIndex) {
					return;
				}
				Hud_SetHudEnabled((int)currentPlayerIdx,
								  g_players[currentPlayerIdx].viewState.externalCameraActive
									  ? 0
									  : g_players[currentPlayerIdx].savedHudEnabled);
			}
			return;
		case KEY_0:
			if ((g_players[currentPlayerIdx].mfd.enabled[1] == 1 &&
				 g_players[currentPlayerIdx].mfd.page[1] == 6) ||
				(g_players[currentPlayerIdx].mfd.enabled[2] == 1 &&
				 g_players[currentPlayerIdx].mfd.page[2] == 6)) {
				g_players[currentPlayerIdx].mfd.menuRow = 0;
				g_players[currentPlayerIdx].mfd.menuItem = 0;
				g_players[currentPlayerIdx].mfd.commandMenu.selectedTargetSlot = 0;
			}
			return;
		case KEY_1:
		case KEY_2:
		case KEY_3:
		case KEY_4:
		case KEY_5:
		case KEY_6:
		case KEY_7:
		case KEY_9:
			if (g_players[currentPlayerIdx].hudEnabled && ((g_players[currentPlayerIdx].mfd.enabled[1] == 1 &&
															g_players[currentPlayerIdx].mfd.page[1] == 6) ||
														   (g_players[currentPlayerIdx].mfd.enabled[2] == 1 &&
															g_players[currentPlayerIdx].mfd.page[2] == 6))) {
				unsigned int item = g_currentActionKey - KEY_0;
				if (g_players[currentPlayerIdx].mfd.menuRow > 8) {
					if (item > (unsigned int)
								   g_mfdCommandSubMenuItemCount[g_players[currentPlayerIdx].mfd.menuRow / 10])
						return;
				} else if (item > (unsigned int)g_players[currentPlayerIdx]
									  .mfdCommandMenuItemCount[g_players[currentPlayerIdx].mfd.menuRow]) {
					return;
				}
				if (g_players[currentPlayerIdx].mfd.menuRow == 0) {
					if (item > 6) {
						if (g_currentActionKey == KEY_7) {
							if (g_players[currentPlayerIdx].mfd.commandMenu.commandableTargetCount < 2u) {
								return;
							}
						} else if (g_currentActionKey == KEY_9 &&
								   !g_players[currentPlayerIdx].mfd.reinforcementCommandAvailable) {
							return;
						}
					} else if (g_players[currentPlayerIdx].savedCraftSettingsRaw[item + 3u] != 1) {
						return;
					}
				}
				g_players[currentPlayerIdx].mfd.menuItem = (uint8_t)item;
				Mfd_SelectCommandMenuItem((int)currentPlayerIdx);
			}
			return;
		case KEY_LESS_THAN:
			FlightAction_CycleTargetComponent(currentPlayerIdx, -1);
			return;
		case KEY_SHIFT_A:
			if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR) {
				return;
			}
			if (!FlightAction_HasCommandSystem(currentPlayerIdx, craft)) {
				FlightAction_EmitSystemCondition(MSG_COMMUNICATIONS, MSG_DAMAGED, currentPlayerIdx);
				return;
			}
			if (g_players[currentPlayerIdx].currentTargetObjectIdx != 0xffffu) {
				if (craft != NULL && craft->playerCommandAvoidTargetObjIdx ==
										 g_players[currentPlayerIdx].currentTargetObjectIdx) {
					craft->playerCommandAvoidTargetObjIdx = -1;
				}
				Player_IssueAiWingmanTargetOrder((uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx,
												 0xffffu, MSG_ACK_USING_TARGET, 6, (int)currentPlayerIdx);
				FlightAction_NotifySameTeamPlayersOfTarget(currentPlayerIdx, 1, MSG_USE_MY_TARGET);
			}
			return;
		case KEY_SHIFT_E:
			if (g_missionHeader.body.hangar != XWA_HANGAR_DEATHSTAR) {
				Player_HandleEvadeCommand((int)currentPlayerIdx, 0xffff);
			}
			return;
		case KEY_SHIFT_F:
			FlightAction_ToggleFilmRecording(currentPlayerIdx);
			return;
		case KEY_SHIFT_G:
			if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR) {
				return;
			}
			if (!FlightAction_HasCommandSystem(currentPlayerIdx, craft)) {
				FlightAction_EmitSystemCondition(MSG_COMMUNICATIONS, MSG_DAMAGED, currentPlayerIdx);
				return;
			}
			FlightAction_CommandTargetGo(currentPlayerIdx);
			return;
		case KEY_SHIFT_H:
			if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR) {
				return;
			}
			if (!FlightAction_HasCommandSystem(currentPlayerIdx, craft)) {
				FlightAction_EmitSystemCondition(MSG_COMMUNICATIONS, MSG_DAMAGED, currentPlayerIdx);
				return;
			}
			if (Player_CanRadioCommandCraft((uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx,
											(int)currentPlayerIdx)) {
				uint16_t target = (uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx;
				AiController* ai;
				g_curCraft = g_objectTable[target].mobj->pCraft;
				ai = pai_GetEffectiveAIController(g_curCraft);
				if (strcmp(g_planTable[ai->pendingPlanId].name, "flyhomeevadepln") &&
					strcmp(g_planTable[ai->pendingPlanId].name, "starshipintohyperpln")) {
					if (!g_curCraft->aiFlight.missionAbortedFlag) {
						++g_missionFgStats[g_objectTable[target].flightGroupIdx].outcomeCount[21];
						if (g_missionFlightGroups[g_objectTable[target].flightGroupIdx]
								.fg.specialCargoCraft == g_curCraft->waveNumber) {
							g_missionFgStats[g_objectTable[target].flightGroupIdx].specialCargoOutcome[21] =
								1;
						}
					}
					g_curCraft->aiFlight.missionAbortedFlag = 1;
					ai->pendingPlanId = (uint8_t)pai_findplanbyname(
						g_objectTable[target].genusId == GENUS_Starship ? "starshipintohyperpln"
																		: "flyhomeevadepln");
					pai_setupcraftcontext(target);
					pai_ApplyPendingPlanTargetAndManeuver(target);
				}
				msg_radioMessage(target, g_curCraft, MSG_ACK_HEAD_HOME, 3, 0);
			}
			return;
		case KEY_SHIFT_I:
			if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR) {
				return;
			}
			if (!FlightAction_HasCommandSystem(currentPlayerIdx, craft)) {
				FlightAction_EmitSystemCondition(MSG_COMMUNICATIONS, MSG_DAMAGED, currentPlayerIdx);
				return;
			}
			if (g_players[currentPlayerIdx].currentTargetObjectIdx != 0xffffu) {
				if (craft != NULL) {
					craft->playerCommandAvoidTargetObjIdx =
						g_players[currentPlayerIdx].currentTargetObjectIdx;
				}
				Player_IssueAiWingmanTargetOrder((uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx,
												 0xffffu, MSG_ACK_IGNORE_TARGET, (GameRand2() & 1) + 7,
												 (int)currentPlayerIdx);
				FlightAction_NotifySameTeamPlayersIgnoringTarget(currentPlayerIdx);
			}
			return;
		case KEY_SHIFT_S:
			if (!FlightAction_HasCommandSystem(currentPlayerIdx, craft)) {
				FlightAction_EmitSystemCondition(MSG_COMMUNICATIONS, MSG_DAMAGED, currentPlayerIdx);
				return;
			}
			if (!g_players[currentPlayerIdx].pendingActionId) {
				g_msgSenderIff = g_players[currentPlayerIdx].iff;
				if (FlightAction_TeamHasAvailableReinforcement(currentPlayerIdx)) {
					if (g_missionFlightRuntimeState
							.teamReinforcementCalled[(uint16_t)g_players[currentPlayerIdx].playerIff]) {
						if (g_players[currentPlayerIdx].iff == g_players[g_localPlayer].iff) {
							msg_emitInFlightMessage(MSG_NOMORE_REINFORCE, (int)currentPlayerIdx);
							fsfx_SpeakTacticalOfficerEvent(15, 177, 0xffffu, 0xffffu);
						}
					} else {
						if (g_players[currentPlayerIdx].iff == g_players[g_localPlayer].iff) {
							msg_emitInFlightMessage(MSG_REINFORCE_CONFIRM, (int)currentPlayerIdx);
						}
						g_players[currentPlayerIdx].pendingActionId = 3;
						g_players[currentPlayerIdx].pendingActionTimer = 1888;
					}
				} else {
					if (g_players[currentPlayerIdx].iff == g_players[g_localPlayer].iff) {
						msg_emitInFlightMessage(MSG_NO_REINFORCE, (int)currentPlayerIdx);
						fsfx_SpeakTacticalOfficerEvent(15, 175, 0xffffu, 0xffffu);
					}
				}
			}
			return;
		case KEY_SHIFT_U:
			Player_HandleReportInCommand((int)currentPlayerIdx, 0xffff);
			return;
		case KEY_SHIFT_W:
			if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR) {
				return;
			}
			if (!FlightAction_HasCommandSystem(currentPlayerIdx, craft)) {
				FlightAction_EmitSystemCondition(MSG_COMMUNICATIONS, MSG_DAMAGED, currentPlayerIdx);
				return;
			}
			FlightAction_CommandTargetWait(currentPlayerIdx);
			return;
		case KEY_A:
			Player_SetTarget(
				Player_FindAttackerOfTarget((uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx,
											(int16_t)g_players[currentPlayerIdx].objectIndex),
				(int)currentPlayerIdx);
			return;
		case KEY_E:
			Player_SetTarget(FlightAction_CycleAttackerTarget(currentPlayerIdx, craft),
							 (int)currentPlayerIdx);
			return;
		case KEY_G:
		case KEY_ALT_G:
			if (g_players[currentPlayerIdx].viewState.externalCameraActive) {
				Player_StepExtView((int)currentPlayerIdx);
				Player_CycleGunnerSeat(currentPlayerIdx, NULL);
				Hud_SetHudEnabled((int)currentPlayerIdx, g_players[currentPlayerIdx].savedHudEnabled);
			} else {
				Player_CycleGunnerSeat(currentPlayerIdx, NULL);
			}
			return;
		case KEY_I:
			Player_SetTarget(FlightAction_FindIncomingWarheadTarget(currentPlayerIdx), (int)currentPlayerIdx);
			return;
		case KEY_M:
			if (!g_flightSimSideEffectsSuppressed && !g_provingGroundsModeActive &&
				g_missionHeader.body.hangar != XWA_HANGAR_DEATHSTAR &&
				!g_players[currentPlayerIdx].hasCheckpointFlag &&
				g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] != 1 &&
				!FlightAction_HasCheckpointPartner(currentPlayerIdx) &&
				g_players[currentPlayerIdx].hyperspacePhase == PLAYER_HYPERSPACE_PHASE_NONE) {
				if (g_players[currentPlayerIdx].mapCameraState) {
					if (g_players[currentPlayerIdx].connectedFlag != 2) {
						uint8_t oldMapCameraState;
						Mission_ProcessFlightGroupWaveCompletion(
							g_players[currentPlayerIdx].boundFlightGroupIdx);
						oldMapCameraState = g_players[currentPlayerIdx].mapCameraState;
						g_players[currentPlayerIdx].mapCameraState = 0;
						if (Player_BindToAvailableCraft(currentPlayerIdx, 0xffffu,
														g_players[currentPlayerIdx].boundObjectSignature,
														0)) {
							g_players[currentPlayerIdx].mapCameraState = oldMapCameraState;
						} else {
							Hud_RestorePlayerHudState((int)currentPlayerIdx);
							g_players[currentPlayerIdx].mapCameraState = 0;
							if (currentPlayerIdx == (unsigned int)g_localPlayer) {
								ForceFeedback_EnableEffects();
							}
						}
					}
				} else {
					int mapTargetObjIdx;
					if (g_filmPlaybackMode && g_filmOverlayActive == 1) {
						g_filmOverlayActive = 0;
						Hud_SyncLocalSoftwareHudMasks(1);
					}
					fsfx_UpdateBeamSystemLoop(0, currentPlayerIdx);
					fsfx_UpdateIncomingMissileWarning(0);
					fsfx_UpdateTargetingTone(0);
					mapTargetObjIdx = g_players[currentPlayerIdx].currentTargetObjectIdx;
					if (mapTargetObjIdx == -1) {
						mapTargetObjIdx = g_players[currentPlayerIdx].objectIndex;
					}
					g_players[currentPlayerIdx].altViewObjectIdx = g_players[currentPlayerIdx].objectIndex;
					Player_UnbindFromCurrentCraft(currentPlayerIdx, 0, 0);
					Hud_EnterPlayerMapView((int)currentPlayerIdx);
					Hud_SetHudViewState(21, (int)currentPlayerIdx);
					g_players[currentPlayerIdx].viewState.playerInputBlocked = 1;
					g_players[currentPlayerIdx].viewState.externalCameraActive = 1;
					g_players[currentPlayerIdx].viewState.transitionTimer = 0;
					g_players[currentPlayerIdx].viewState.cameraDistance = 0x40000;
					if (mapTargetObjIdx != -1) {
						Player_SetTarget((int16_t)mapTargetObjIdx, currentPlayerIdx);
					}
					g_players[currentPlayerIdx].viewState.cameraFocusObjIdx = 0xffff;
					g_players[currentPlayerIdx].viewState.aimTargetIdx = 0xffff;
					g_players[currentPlayerIdx].viewState.savedTargetZ = 0x40000;
					if (g_players[currentPlayerIdx].currentTargetObjectIdx != 0xffffu) {
						ObjectRecord* targetObj =
							&g_objectTable[(uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx];
						g_players[currentPlayerIdx].viewState.savedTargetX = targetObj->world_x;
						g_players[currentPlayerIdx].viewState.savedTargetY = targetObj->world_y;
						g_players[currentPlayerIdx].viewState.cameraDistance =
							16 * g_modelTypeTable[(uint16_t)targetObj->objectType].maxBoundsExtent;
					}
					g_players[currentPlayerIdx].pendingActionTimer = 0;
					g_players[currentPlayerIdx].pendingActionId = 0;
					fsfx_UpdatePlayerEngineLoop();
					fsfx_UpdateChaffLoop();
					fsfx_UpdateBeamEffectLoops();
					if (currentPlayerIdx == (unsigned int)g_localPlayer)
						ForceFeedback_StopAllEffects();
				}
			}
			return;
		case KEY_N:
			Player_SetTarget(FlightAction_CycleFlaggedTarget(currentPlayerIdx), (int)currentPlayerIdx);
			return;
		case KEY_O:
			if (g_provingGroundsModeActive) {
				Yard_TargetCurrentObjective(currentPlayerIdx);
			} else {
				int target = Player_FindNearestObjective(0, (int)currentPlayerIdx);
				if (target == 0xffff)
					target = Player_FindNearestObjective(2, (int)currentPlayerIdx);
				Player_SetTarget(target, (int)currentPlayerIdx);
			}
			return;
		case KEY_P:
			if (g_flightLocatePlayersEnabled) {
				Player_SetTarget(FlightAction_FindNearestLocatedHostilePlayerTarget(currentPlayerIdx),
								 (int)currentPlayerIdx);
			}
			return;
		case KEY_Q:
			if (g_players[currentPlayerIdx].connectedFlag == 1) {
				if ((g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH &&
					 g_missionHeader.body.goalsUnimportant && g_pilotData.numHumanPlayersLastMission == 1 &&
					 g_missionFlightRuntimeState.teamGoalStatus[(uint16_t)g_players[currentPlayerIdx]
																	.playerIff][TEAM_GOAL_PRIMARY] != 1) ||
					(g_flightPlayerCount == 1 &&
					 g_missionFlightRuntimeState.teamGoalStatus[(uint16_t)g_players[currentPlayerIdx]
																	.playerIff][TEAM_GOAL_PRIMARY] == 1 &&
					 Mission_ShouldApplyEndMissionPenalty(currentPlayerIdx))) {
					fsfx_PlaySound(61, 0xffffu, currentPlayerIdx);
					msg_emitInFlightMessage(MSG_END_MISSION_PENALTY, (int)currentPlayerIdx);
				} else {
					FlightAction_PlayAcceptedClick(currentPlayerIdx);
					msg_emitInFlightMessage(MSG_END_MISSION, (int)currentPlayerIdx);
				}
				g_players[currentPlayerIdx].pendingActionId = 2;
				g_players[currentPlayerIdx].pendingActionParam = -1;
				g_players[currentPlayerIdx].pendingActionTimer = 1888;
			} else {
				msg_emitInFlightMessage(MSG_YOU_MUST_WAIT, (int)currentPlayerIdx);
			}
			return;
		case KEY_R:
			Player_SetTarget(Player_FindNearestEnemyFighter(currentPlayerIdx, 0xffff), (int)currentPlayerIdx);
			return;
		case KEY_T:
			Player_SetTarget(
				Player_CycleTargetAnyIFF(g_players[currentPlayerIdx].currentTargetObjectIdx == 0xffffu
											 ? (uint16_t)g_players[currentPlayerIdx].targetCycleStart
											 : (uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx,
										 1, (int)currentPlayerIdx),
				(int)currentPlayerIdx);
			return;
		case KEY_U:
			Player_SetTarget(FlightAction_FindNewestLeaderTarget(currentPlayerIdx), (int)currentPlayerIdx);
			return;
		case KEY_Y:
			Player_SetTarget(
				Player_CycleTargetAnyIFF(g_players[currentPlayerIdx].currentTargetObjectIdx == 0xffffu
											 ? (uint16_t)g_players[currentPlayerIdx].targetCycleStart
											 : (uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx,
										 -1, (int)currentPlayerIdx),
				(int)currentPlayerIdx);
			return;
		case KEY_ALT_C:
			g_players[currentPlayerIdx].currentTargetObjectIdx = 0xffffu;
			return;
		case KEY_ALT_J:
			if (!g_flightSimSideEffectsSuppressed &&
				g_players[currentPlayerIdx].viewState.externalCameraActive) {
				if (g_players[currentPlayerIdx].viewState.transitionTimer) {
					g_players[currentPlayerIdx].viewState.transitionTimer = 0;
				} else {
					FlightView_FinishCameraFocusTransition((int)currentPlayerIdx, 5);
				}
			}
			return;
		case KEY_ALT_N:
			FlightAction_FocusNewestPlayerProjectile(currentPlayerIdx);
			return;
		case KEY_ABORT_MISSION:
			FlightAction_RequestAbortOrDisconnect(currentPlayerIdx);
			return;
		case KEY_ALT_U:
			if (!g_flightSimSideEffectsSuppressed && !g_players[currentPlayerIdx].mapCameraState) {
				if (g_players[currentPlayerIdx].viewState.cameraFocusObjIdx ==
					g_players[currentPlayerIdx].objectIndex) {
					if (g_players[currentPlayerIdx].currentTargetObjectIdx == 0xffffu) {
						return;
					}
					g_players[currentPlayerIdx].viewState.cameraFocusObjIdx =
						(uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx;
					g_players[currentPlayerIdx].viewState.cameraDistance =
						4 * g_modelTypeTable[(uint16_t)g_objectTable[(uint16_t)g_players[currentPlayerIdx]
																		 .currentTargetObjectIdx]
												 .objectType]
								.maxBoundsExtent;
					if (g_players[currentPlayerIdx].viewState.cameraDistance > 0x2000) {
						g_players[currentPlayerIdx].viewState.cameraDistance = 0x2000;
					}
				} else {
					g_players[currentPlayerIdx].viewState.cameraFocusObjIdx =
						g_players[currentPlayerIdx].objectIndex;
				}
				Player_StepExtView((int)currentPlayerIdx);
				FlightAction_UpdateHudForExternalCamera(currentPlayerIdx);
			}
			return;
		case KEY_SPACE | KEY_ALT_E:
			if (g_players[currentPlayerIdx].hudEnabled)
				Hud_CycleActiveMfdPage((uint16_t)currentPlayerIdx, 0);
			return;
		case (KEY_SPACE | KEY_ALT_E | 1):
			if (g_players[currentPlayerIdx].hudEnabled)
				Hud_CycleActiveMfdPage((uint16_t)currentPlayerIdx, 1);
			return;
		case (KEY_QUOTES | KEY_ALT_E):
			FlightAction_CycleMfdCommandMenuItem(currentPlayerIdx, -1);
			return;
		case (KEY_QUOTES | KEY_ALT_E | 1):
			FlightAction_CycleMfdCommandMenuItem(currentPlayerIdx, 1);
			return;
		case KEY_INSERT:
			if (!g_flightSimSideEffectsSuppressed && currentPlayerIdx == (unsigned int)g_localPlayer &&
				g_players[currentPlayerIdx].hudEnabled && !g_players[currentPlayerIdx].mfd.enabled[0] &&
				!g_players[currentPlayerIdx].mapCameraState) {
				int enabled = g_hudElementEnabled[1].enabled == 0;
				g_hudElementEnabled[1].enabled = (uint8_t)enabled;
				if (!g_useHardware3D) {
					g_hudElementEnabled[4].enabled = 1;
					if (!g_filmPlaybackMode || !g_filmOverlayActive) {
						Hud_UpdateHUDMask(0, enabled);
						Hud_RedrawSoftwareHudFrame();
					}
				}
			}
			return;
		case KEY_DELETE:
			if (!g_players[currentPlayerIdx].hyperspacePhase && g_players[currentPlayerIdx].hudEnabled &&
				(!g_players[currentPlayerIdx].mfd.enabled[0] || g_players[currentPlayerIdx].mapCameraState)) {
				if (g_players[currentPlayerIdx].mfd.enabled[1] == 1) {
					if (g_players[currentPlayerIdx].mfd.activeIndex == 1)
						Hud_ToggleMfdSide((int)currentPlayerIdx, 1);
					else
						Hud_ForceHudRefresh((int)currentPlayerIdx, 1);
				} else {
					Hud_ToggleMfdSide((int)currentPlayerIdx, 1);
				}
			}
			return;
		case KEY_HOME:
			if (!g_flightSimSideEffectsSuppressed && currentPlayerIdx == (unsigned int)g_localPlayer &&
				g_players[currentPlayerIdx].hudEnabled && !g_players[currentPlayerIdx].mfd.enabled[0] &&
				!g_players[currentPlayerIdx].mapCameraState) {
				int enabled = g_hudElementEnabled[3].enabled == 0;
				g_hudElementEnabled[3].enabled = (uint8_t)enabled;
				if (!g_useHardware3D) {
					g_hudElementEnabled[6].enabled = 1;
					if (!g_filmPlaybackMode || !g_filmOverlayActive) {
						Hud_UpdateHUDMask(2, enabled);
						Hud_RedrawSoftwareHudFrame();
					}
				}
			}
			return;
		case KEY_END:
			if (!g_flightSimSideEffectsSuppressed && currentPlayerIdx == (unsigned int)g_localPlayer &&
				g_players[currentPlayerIdx].hudEnabled &&
				(!g_players[currentPlayerIdx].mfd.enabled[0] || g_players[currentPlayerIdx].mapCameraState)) {
				int enabled = g_hudElementEnabled[0].enabled == 0;
				g_hudElementEnabled[0].enabled = (uint8_t)enabled;
				if (!g_players[currentPlayerIdx].mapCameraState)
					g_savedHudCmdPanelEnabled = (uint8_t)enabled;
				if (!g_useHardware3D && (!g_filmPlaybackMode || !g_filmOverlayActive)) {
					Hud_UpdateHUDMask(3, enabled);
					Hud_RedrawSoftwareHudFrame();
				}
			}
			return;
		case KEY_PAGEUP:
			if (!g_flightSimSideEffectsSuppressed && currentPlayerIdx == (unsigned int)g_localPlayer &&
				g_players[currentPlayerIdx].hudEnabled && !g_players[currentPlayerIdx].mfd.enabled[0] &&
				!g_players[currentPlayerIdx].mapCameraState) {
				int enabled = g_hudElementEnabled[2].enabled == 0;
				g_hudElementEnabled[2].enabled = (uint8_t)enabled;
				if (!g_useHardware3D) {
					g_hudElementEnabled[5].enabled = 1;
					if (!g_filmPlaybackMode || !g_filmOverlayActive) {
						Hud_UpdateHUDMask(1, enabled);
						Hud_RedrawSoftwareHudFrame();
					}
				}
			}
			return;
		case KEY_PAGEDOWN:
			if (!g_players[currentPlayerIdx].hyperspacePhase && g_players[currentPlayerIdx].hudEnabled &&
				(!g_players[currentPlayerIdx].mfd.enabled[0] || g_players[currentPlayerIdx].mapCameraState)) {
				if (g_players[currentPlayerIdx].mfd.enabled[2] == 1) {
					if (g_players[currentPlayerIdx].mfd.activeIndex == 2)
						Hud_ToggleMfdSide((int)currentPlayerIdx, 2);
					else
						Hud_ForceHudRefresh((int)currentPlayerIdx, 2);
				} else {
					Hud_ToggleMfdSide((int)currentPlayerIdx, 2);
				}
			}
			return;
		case (KEY_PAD_PLUS | 1):
			if (g_players[currentPlayerIdx].mfd.page[g_players[currentPlayerIdx].mfd.activeIndex] == 6) {
				if (g_players[currentPlayerIdx].mfd.menuRow != 0 ||
					!Mfd_IsCommandMenuItemAvailable((uint16_t)currentPlayerIdx, 0,
													g_players[currentPlayerIdx].mfd.menuItem)) {
					Mfd_SelectCommandMenuItem((int)currentPlayerIdx);
				} else {
					++g_players[currentPlayerIdx].mfd.menuItem;
					Mfd_SelectCommandMenuItem((int)currentPlayerIdx);
				}
			}
			return;
		case KEY_F1:
			Player_SetTarget(
				Player_CycleTarget(g_players[currentPlayerIdx].currentTargetObjectIdx == 0xffffu
									   ? (uint16_t)g_players[currentPlayerIdx].targetCycleStart
									   : (uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx,
								   1, (int)currentPlayerIdx, 2, 5),
				(int)currentPlayerIdx);
			return;
		case KEY_F2:
			Player_SetTarget(
				Player_CycleTarget(g_players[currentPlayerIdx].currentTargetObjectIdx == 0xffffu
									   ? (uint16_t)g_players[currentPlayerIdx].targetCycleStart
									   : (uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx,
								   1, (int)currentPlayerIdx, 5, 5),
				(int)currentPlayerIdx);
			return;
		case KEY_F3:
			Player_SetTarget(
				Player_CycleTarget(g_players[currentPlayerIdx].currentTargetObjectIdx == 0xffffu
									   ? (uint16_t)g_players[currentPlayerIdx].targetCycleStart
									   : (uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx,
								   1, (int)currentPlayerIdx, 3, 5),
				(int)currentPlayerIdx);
			return;
		case KEY_F4:
		case KEY_SHIFT_F3:
			Player_SetTarget(
				Player_CycleTarget(g_players[currentPlayerIdx].currentTargetObjectIdx == 0xffffu
									   ? (uint16_t)g_players[currentPlayerIdx].targetCycleStart
									   : (uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx,
								   -1, (int)currentPlayerIdx, 3, 5),
				(int)currentPlayerIdx);
			return;
		case KEY_F5:
		case KEY_F6:
		case KEY_F7: {
			uint16_t slot = (uint16_t)(g_currentActionKey - KEY_F5);
			if (g_players[currentPlayerIdx].targetPresetSlot[slot] != -1 &&
				g_objectTable[(uint16_t)g_players[currentPlayerIdx].targetPresetSlot[slot]].objectType !=
					OBJ_None) {
				ObjectRecord* presetObj =
					&g_objectTable[(uint16_t)g_players[currentPlayerIdx].targetPresetSlot[slot]];
				CraftData* presetCraft = presetObj->mobj != NULL ? presetObj->mobj->pCraft : NULL;
				if (!FlightAction_IsPlayerTractorBeamVictim(
						(uint16_t)g_players[currentPlayerIdx].targetPresetSlot[slot], presetObj,
						presetCraft)) {
					Player_SetTarget(g_players[currentPlayerIdx].targetPresetSlot[slot],
									 (int)currentPlayerIdx);
				}
			}
			return;
		}
		case KEY_SHIFT_F1:
			Player_SetTarget(
				Player_CycleTarget(g_players[currentPlayerIdx].currentTargetObjectIdx == 0xffffu
									   ? (uint16_t)g_players[currentPlayerIdx].targetCycleStart
									   : (uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx,
								   -1, (int)currentPlayerIdx, 2, 5),
				(int)currentPlayerIdx);
			return;
		case KEY_SHIFT_F2:
			Player_SetTarget(
				Player_CycleTarget(g_players[currentPlayerIdx].currentTargetObjectIdx == 0xffffu
									   ? (uint16_t)g_players[currentPlayerIdx].targetCycleStart
									   : (uint16_t)g_players[currentPlayerIdx].currentTargetObjectIdx,
								   -1, (int)currentPlayerIdx, 5, 5),
				(int)currentPlayerIdx);
			return;
		case KEY_SHIFT_F5:
		case KEY_SHIFT_F6:
		case KEY_SHIFT_F7: {
			uint16_t slot = (uint16_t)(g_currentActionKey - KEY_SHIFT_F5);
			if (g_players[currentPlayerIdx].currentTargetObjectIdx == 0xffffu) {
				FlightAction_PlayEngineClick(currentPlayerIdx);
			} else {
				g_players[currentPlayerIdx].targetPresetSlot[slot] =
					g_players[currentPlayerIdx].currentTargetObjectIdx;
				fsfx_PlaySound(75, 0xffffu, currentPlayerIdx);
			}
			return;
		}
		case KEY_CONSOLE_TOGGLE:
			FlightAction_ToggleConsoleMfd(currentPlayerIdx);
			return;
		case KEY_NEXT_PLAYER_CRAFT: {
			uint16_t target = FlightAction_FindNextPlayerCraftTarget(currentPlayerIdx);
			if (target == 0xffffu) {
				if (!g_flightLocatePlayersEnabled) {
					msg_emitInFlightMessage(MSG_NO_PLAYER_LOCATING, (int)currentPlayerIdx);
				}
			} else {
				Player_SetTarget(target, (int)currentPlayerIdx);
			}
		}
			return;
		case KEY_HUD_TOGGLE:
			if (!g_players[currentPlayerIdx].viewState.externalCameraActive &&
				g_players[currentPlayerIdx].hyperspacePhase == PLAYER_HYPERSPACE_PHASE_NONE) {
				Hud_SetHudEnabled((int)currentPlayerIdx, g_players[currentPlayerIdx].hudEnabled == 0);
				if (!g_players[currentPlayerIdx].mapCameraState && !g_inHangarReady) {
					g_players[currentPlayerIdx].savedHudEnabled = g_players[currentPlayerIdx].hudEnabled;
				}
			}
			return;
		default:
			break;
	}

	if (g_currentActionKey != KEY_SPACE) {
		if (g_currentActionKey != KEY_TAB) {
			if (g_currentActionKey == KEY_ENTER) {
				if (FlightAction_HasCommandSystem(currentPlayerIdx, craft)) {
					if (g_activeFlightPlayerCount != 1 && !g_players[currentPlayerIdx].msgTypeId) {
						g_players[currentPlayerIdx].msgTypeId = 1;
						g_players[currentPlayerIdx].msgLength = 0;
						g_players[currentPlayerIdx].msgText[0] = '_';
						g_players[currentPlayerIdx].msgText[1] = '\0';
						msg_addMessagePtr(0, g_players[currentPlayerIdx].msgText);
						msg_emitInFlightMessage(MSG_ENTER_MESSAGE_TEAM, (int)currentPlayerIdx);
					}
				} else {
					FlightAction_EmitSystemCondition(MSG_COMMUNICATIONS, MSG_DAMAGED, currentPlayerIdx);
				}
			}
			return;
		}
		FlightAction_SelectMfdCommandMenu(currentPlayerIdx);
		return;
	}

	FlightAction_ConfirmPendingAction(currentPlayerIdx, craft);
}

// FUNCTION: XWA 0x434420
void FlightInput_ScaleAxesForFlight(void) {
	g_currentActionKey = g_actionKey;
	g_flightKeyMods = 0;
	g_scaledInputRoll = 0;
	g_scaledInputPitch = 0;
	g_scaledInputYaw = 0;

	if ((uint16_t)(g_joystickEnabled | 1u) == 0) {
		return;
	}

	g_scaledInputYaw = (int16_t)(g_ctrlAxisX * 120);
	g_scaledInputPitch = (int16_t)(g_ctrlAxisY * 50);
	g_scaledInputRoll = (int16_t)(g_ctrlAxisR * 120);
	g_flightKeyMods = g_keyMods;
}

// FUNCTION: XWA 0x4344A0
void FlightInput_ApplyDeadzone(void) {
	int16_t yaw;
	int16_t pitch;
	int16_t roll;

	yaw = g_scaledInputYaw;
	if ((uint16_t)g_scaledInputYaw >= 0x8000u) {
		yaw = (int16_t)-g_scaledInputYaw;
	}
	if (yaw <= 64) {
		g_scaledInputYaw = 0;
	}

	pitch = g_scaledInputPitch;
	if ((uint16_t)g_scaledInputPitch >= 0x8000u) {
		pitch = (int16_t)-g_scaledInputPitch;
	}
	if (pitch <= 24) {
		g_scaledInputPitch = 0;
	}

	roll = g_scaledInputRoll;
	if ((uint16_t)g_scaledInputRoll >= 0x8000u) {
		roll = (int16_t)-g_scaledInputRoll;
	}
	if (roll <= 64) {
		g_scaledInputRoll = 0;
	}
}

#ifndef XWA_MODERN
int Console_HandleKey(uint16_t key, unsigned int playerIdx);
#endif

static __inline int Flight_UpdateEntity_IsDeadCameraObject(uint16_t objectType) {
	return objectType == OBJ_None ||
		   (objectType >= OBJ_ExplosionTextureGroup2000 && objectType <= OBJ_AnimationTextureGroup2008);
}

static __inline void Flight_UpdateEntity_NotifyPlayerNoMore(unsigned int playerIdx) {
	unsigned int i;
	int connectedCount = 0;

	msg_addMessagePtr(0, NetSession_GetPlayerName((int)playerIdx));
	msg_emitInFlightMessage(MSG_PLAYER_NO_MORE, g_localPlayer);

	for (i = 0; i < XWA_PLAYER_COUNT; ++i) {
		if (g_players[i].connectedFlag == 1) {
			++connectedCount;
		}
	}
	if (g_players[g_localPlayer].connectedFlag == 1) {
		if (connectedCount == 1) {
			msg_emitInFlightMessage(MSG_1_PLAYER, g_localPlayer);
		} else {
			g_msgArgTable[0] = (uint16_t)connectedCount;
			msg_emitInFlightMessage(MSG_MORE_PLAYERS, g_localPlayer);
		}
	}
}

static __inline int Flight_UpdateEntity_FindCheckpointPartner(unsigned int playerIdx) {
	int i;

	for (i = 0; i < XWA_PLAYER_COUNT; ++i) {
		if (i != playerIdx && g_players[i].connectedFlag && g_players[i].hasCheckpointFlag &&
			g_players[i].boundFlightGroupIdx == g_players[playerIdx].boundFlightGroupIdx) {
			return i;
		}
	}
	return -1;
}

static GameConfig g_flightOptionsModalOldConfig;
static unsigned int g_flightOptionsModalPlayerIdx;
static int g_flightOptionsModalPending;

static __inline int Flight_UpdateEntity_OpenOptions(unsigned int playerIdx) {
	int exitToFrontend;

	if (!g_flightOptionsModalPending) {
		if (g_flightPlayerCount != 1) {
			return 0;
		}
		if (g_filmPlaybackMode || g_filmRecording) {
			g_actionKey = KEY_NONE;
			if (g_filmPlaybackMode != 2 && g_filmRecording) {
				msg_emitInFlightMessage(MSG_FILM_NO_OPTIONS, (int)playerIdx);
				fsfx_PlaySound(63, 0xffffu, playerIdx);
			}
			return 0;
		}

		g_inputTimestamp += Time_GetFrameDelta();
		Sound_StopAllInstances();
		g_flightOptionsModalOldConfig = g_gameConfig;
		g_flightOptionsModalPlayerIdx = playerIdx;
		g_flightOptionsModalPending = 1;
		g_actionKey = KEY_NONE;
	}

	exitToFrontend = FlightDisplay_RunRestrictedOptionsModal();
	if (FlightDisplay_IsFrontendModalActive()) {
		g_actionKey = KEY_NONE;
		return 1;
	}

	g_flightOptionsModalPending = 0;
	Flight_ApplyConfigToRuntime(&g_flightOptionsModalOldConfig, &g_gameConfig);
	Time_GetFrameDelta();
	if (!exitToFrontend) {
		msg_emitInFlightMessage(MSG_MISSION_RESUMED, (int)g_flightOptionsModalPlayerIdx);
	}
	g_actionKey = KEY_NONE;
	sub_4D4640();

	if (exitToFrontend) {
		g_flightReturnToFrontendRequested = 1;
		Player_EndFlightParticipation((int)g_flightOptionsModalPlayerIdx);
		if (g_flightOptionsModalPlayerIdx != (unsigned int)g_localPlayer) {
			msg_addMessagePtr(0, NetSession_GetPlayerName((int)g_flightOptionsModalPlayerIdx));
			msg_emitInFlightMessage(MSG_PLAYER_QUIT, g_localPlayer);
		}
		if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART &&
			g_pilotData.numHumanPlayersLastMission == 1) {
			int playerIff = (uint16_t)g_players[g_flightOptionsModalPlayerIdx].playerIff;
			if (g_missionFlightRuntimeState.teamGoalStatus[playerIff][TEAM_GOAL_PRIMARY] != 1) {
				g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][playerIff] -= 2000;
			}
		}
	} else if (!g_useHardware3D) {
		Hud_RedrawSoftwareHudFrame();
	}

	return 0;
}

static __inline void Flight_UpdateEntity_PauseUntilKey(unsigned int playerIdx) {
	int16_t nextKey;

	if (g_flightPlayerCount != 1) {
		return;
	}
	if (g_filmPlaybackMode || g_filmRecording) {
		g_actionKey = KEY_NONE;
		if (g_filmRecording) {
			msg_emitInFlightMessage(MSG_FILM_NO_PAUSE, (int)playerIdx);
			fsfx_PlaySound(63, 0xffffu, playerIdx);
		}
		return;
	}

	g_inputTimestamp += Time_GetFrameDelta();
	fsfx_PlaySound(68, 0xffffu, playerIdx);
	msg_emitInFlightMessage(MSG_MISSION_PAUSED, (int)playerIdx);
	if (g_useHardware3D) {
		RenderScene_Initialize(1);
		FlightText_FlushQueue();
		RenderScene_DrawVisibleFaces();
	} else {
		FlightSw_SetRenderTarget(NULL, 0, 0, 0);
		FlightSurface_Lock();
		FlightText_SetClipRect(0, 0, g_screenWidth, g_screenHeight);
		g_flightFillClipRectFn();
		FlightSurface_Unlock();
		Hud_BlitSoftwareHudTextPanes();
	}
	FlightDisplay_Flip();
	Sound_StopAllInstances();
	Music_PauseIfInitialized();
	do {
		nextKey = 0;
		if (FlightInput_HasKeyReady()) {
			nextKey = FlightInput_GetNextKey();
		}
	} while (!nextKey);
	Music_ResumeIfInitialized();
	Time_GetFrameDelta();
	msg_emitInFlightMessage(MSG_MISSION_RESUMED, (int)playerIdx);
	g_actionKey = KEY_NONE;
	sub_4D4640();
}

static __inline int Flight_UpdateEntity_HandleLocalHotkeys(unsigned int playerIdx) {
	if (playerIdx != (unsigned int)g_localPlayer || g_flightSimSideEffectsSuppressed) {
		return 0;
	}

	if (g_flightOptionsModalPending || FlightDisplay_IsFrontendModalActive()) {
		return Flight_UpdateEntity_OpenOptions(playerIdx);
	}

	switch (g_currentActionKey) {
		case KEY_ESCAPE:
			return Flight_UpdateEntity_OpenOptions(playerIdx);
		case 131:
			if (!g_filmPlaybackMode && !g_filmRecording) {
				g_flightGraphicsDetailPreset = (uint8_t)(g_flightGraphicsDetailPreset + 1);
				if (g_flightGraphicsDetailPreset >= 4u) {
					g_flightGraphicsDetailPreset = 0;
				}
				Flight_ApplyGraphicsDetailPreset(g_flightGraphicsDetailPreset);
				msg_emitInFlightMessage((uint16_t)(g_flightGraphicsDetailPreset + 109), (int)playerIdx);
				fsfx_PlaySound(68, 0xffffu, playerIdx);
			}
			break;
		case 133: {
			unsigned int strength = ForceFeedback_GetStrength() + 1250;
			if (strength > 10000u) {
				strength = 0;
			}
			ForceFeedback_SetStrength(strength);
			fsfx_PlaySound(68, 0xffffu, playerIdx);
			g_msgArgTable[0] = (uint16_t)(strength / 1250u);
			g_msgArgTable[1] = 8;
			msg_emitInFlightMessage(MSG_FFEEDBACK, (int)playerIdx);
			break;
		}
		case 134: {
			unsigned int strength = ForceFeedback_GetCenteringStrength() + 1250;
			if (strength > 10000u) {
				strength = 0;
			}
			ForceFeedback_SetCenteringStrength(strength);
			fsfx_PlaySound(68, 0xffffu, playerIdx);
			g_msgArgTable[0] = (uint16_t)(strength / 1250u);
			g_msgArgTable[1] = 8;
			msg_emitInFlightMessage(MSG_FFEEDBACK_CENTERING, (int)playerIdx);
			break;
		}
		case 136:
			g_sw3dSkipOddScanlines = g_sw3dSkipOddScanlines == 0;
			break;
		case 140:
			g_unusedFlightAction140ToggleFlag = g_unusedFlightAction140ToggleFlag == 0;
			break;
		case 143:
			Flight_UpdateEntity_PauseUntilKey(playerIdx);
			break;
		case 146:
			g_flightSystemMessagesEnabled = g_flightSystemMessagesEnabled == 0;
			msg_emitInFlightMessage(g_flightSystemMessagesEnabled ? MSG_SYSTEMSGS_ENABLED
																  : MSG_SYSTEMSGS_DISABLED,
									(int)playerIdx);
			break;
		case 149:
			fsfx_PlaySound(68, 0xffffu, playerIdx);
			msg_emitInFlightMessage(MSG_VERSION, (int)playerIdx);
			break;
		case 152:
			g_flightRenderStatsDumpRequested = 1;
			break;
		case KEY_SCROLL_LOCK:
			if (!g_players[playerIdx].currentSeatIdx && g_flightPlayerCount == 1 && !g_filmPlaybackMode) {
				uint8_t oldEnabled = g_padlockMouseLookEnabled;
				g_padlockMouseLookEnabled = oldEnabled == 0;
				if (!oldEnabled) {
					g_padlockMouseLookIgnoreNextDelta = 1;
				}
			}
			break;
		case 232:
			if (!g_filmPlaybackMode) {
				fsfx_PlaySound(68, 0xffffu, playerIdx);
				FlightScreenshot_Capture();
			}
			break;
		default:
			break;
	}

	return 0;
}

#ifdef XWA_MODERN
static __inline int Flight_ModernScaleLegacyPerAdvanceValue(int value) {
	if (!XwaModernFlightTiming_IsHighRate()) {
		return value;
	}
	return value * (uint16_t)g_elapsedTicks / 8;
}

static __inline int Flight_ModernScaleMapCameraStep(int value, unsigned int playerIdx, int direction) {
	ModernMapCameraStepRemainder* state;
	int numerator;
	int scaled;

	if (!XwaModernFlightTiming_IsHighRate()) {
		g_modernMapCameraStepRemainders[playerIdx].numerator = 0;
		g_modernMapCameraStepRemainders[playerIdx].direction = 0;
		return value;
	}
	state = &g_modernMapCameraStepRemainders[playerIdx];
	if (state->direction != direction) {
		state->numerator = 0;
		state->direction = direction;
	}
	numerator = value * (uint16_t)g_elapsedTicks + state->numerator;
	scaled = numerator / 8;
	state->numerator = numerator % 8;
	return scaled;
}

static __inline void Flight_ModernResetMapCameraStep(unsigned int playerIdx) {
	g_modernMapCameraStepRemainders[playerIdx].numerator = 0;
	g_modernMapCameraStepRemainders[playerIdx].direction = 0;
}
#endif

static __inline void Flight_UpdateEntity_SendTypedMessage(unsigned int playerIdx, const char* text) {
	unsigned int dst;
	PlayerData* player = &g_players[playerIdx];

	msg_addMessagePtr(0, NetSession_GetPlayerName((int)playerIdx));
	msg_addMessagePtr(1, (void*)text);
	for (dst = 0; dst < XWA_PLAYER_COUNT; ++dst) {
		int emit = 0;
		if (!g_players[dst].connectedFlag) {
			continue;
		}
		g_msgSenderIff = 3;
		if (player->msgTypeId == 1) {
			if ((uint16_t)player->playerIff == (uint16_t)g_players[dst].playerIff) {
				emit = 1;
			}
		} else if (player->msgTypeId == 2) {
			int dstTeam = (uint16_t)g_players[dst].playerIff;
			if ((uint16_t)player->playerIff != dstTeam &&
				!g_missionTeams[dstTeam].allies[(uint16_t)player->playerIff]) {
				emit = 1;
			}
		} else {
			emit = 1;
		}
		if (emit) {
			msg_emitInFlightMessage(MSG_PLAYER_MESSAGE, (int)dst);
		}
	}
	player->msgTypeId = 0;
	msg_emitInFlightMessage(MSG_MESSAGE_SENT, (int)playerIdx);
}

static __inline int16_t Flight_UpdateEntity_SmoothAxis(int16_t current, int target) {
	int16_t delta = (int16_t)(target - current);
	int16_t step;

	if (!delta) {
		return current;
	}
	step = (int16_t)delta;
	if (delta < 0) {
		step = (int16_t)-step;
	}
	if (step < 8) {
		return (int16_t)(current + delta);
	}
	if (g_simStepScale > 4u) {
		step = (int16_t)(step / (int)g_simStepScale);
		if (!step) {
			step = 1;
		}
		step = (int16_t)(step * 4);
	}
	if (delta < 0) {
		return (int16_t)(current - step);
	}
	return (int16_t)(current + step);
}

static __inline void Flight_UpdateEntity_ClampMapTarget(PlayerViewState* view) {
	if (view->savedTargetX < -0x1000000)
		view->savedTargetX = -0x1000000;
	if (view->savedTargetX > 0x1000000)
		view->savedTargetX = 0x1000000;
	if (view->savedTargetY < -0x1000000)
		view->savedTargetY = -0x1000000;
	if (view->savedTargetY > 0x1000000)
		view->savedTargetY = 0x1000000;
	if (view->savedTargetZ < -0x1000000)
		view->savedTargetZ = -0x1000000;
	if (view->savedTargetZ > 0x1000000)
		view->savedTargetZ = 0x1000000;
}

// FUNCTION: XWA 0x4F9320
void Flight_UpdateEntity(unsigned int playerIdx) {
#ifndef XWA_MODERN
	GameConfig oldConfig;
#endif
	int seatIdx;

	if (g_flightMissionEndPending == 1) {
		return;
	}

	if (!g_flightSimSideEffectsSuppressed) {
		int focusObjIdx;

		focusObjIdx = g_players[playerIdx].viewState.cameraFocusObjIdx;
		if (focusObjIdx != 0xffff &&
			Flight_UpdateEntity_IsDeadCameraObject((uint16_t)g_objectTable[focusObjIdx].objectType)) {
			if (g_players[playerIdx].mapCameraState) {
				g_players[playerIdx].viewState.cameraFocusObjIdx = 0xffff;
			} else {
				g_players[playerIdx].viewState.transitionTimer = 0;
				g_players[playerIdx].viewState.cameraFocusObjIdx = g_players[playerIdx].objectIndex;
				if (g_filmPlaybackMode && g_filmOverlayActive == 1) {
					g_filmOverlayViewState.cameraFocusObjIdx = g_players[playerIdx].objectIndex;
				}
				Player_StepExtView((int)playerIdx);
				if (g_players[playerIdx].savedHudEnabled) {
					Hud_SetHudEnabled((int)playerIdx, 1);
				}
			}
		}

		if (g_players[playerIdx].mapCameraState && g_players[playerIdx].viewState.aimTargetIdx != 0xffff &&
			g_objectTable[g_players[playerIdx].viewState.aimTargetIdx].objectType == OBJ_None) {
			g_players[playerIdx].viewState.aimTargetIdx = 0xffff;
		}

		if (g_filmPlaybackMode && g_filmOverlayActive == 1 && playerIdx == (unsigned int)g_localPlayer) {
			if (g_filmOverlayViewState.cameraFocusObjIdx != 0xffff &&
				Flight_UpdateEntity_IsDeadCameraObject(
					(uint16_t)g_objectTable[g_filmOverlayViewState.cameraFocusObjIdx].objectType)) {
				int objectIndex = g_players[g_localPlayer].objectIndex;

				g_filmOverlayViewState.transitionTimer = 0;
				if (g_filmOverlayViewState.cameraFocusObjIdx == objectIndex) {
					g_filmOverlayActive = 0;
				} else {
					g_filmOverlayViewState.cameraFocusObjIdx = objectIndex;
					if (objectIndex == g_filmOverlayViewState.aimTargetIdx) {
						g_filmOverlayViewState.aimTargetIdx = 0xffff;
					}
				}
			}
			if (g_filmOverlayViewState.aimTargetIdx != 0xffff &&
				g_objectTable[g_filmOverlayViewState.aimTargetIdx].objectType == OBJ_None) {
				g_filmOverlayViewState.aimTargetIdx = 0xffff;
			}
		}
	}
	FlightInput_Read((int)playerIdx);
	FlightInput_ScaleAxesForFlight();

	if (g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] == 1 &&
		!g_missionElapsedClock.hours && !g_missionElapsedClock.minutes &&
		g_missionElapsedClock.seconds == 8u && !g_players[playerIdx].currentSeatIdx) {
		g_currentActionKey = KEY_G;
	}

	if (g_players[playerIdx].consoleKeyDelay) {
		--g_players[playerIdx].consoleKeyDelay;
	} else if (g_players[playerIdx].hudEnabled && !g_players[playerIdx].viewState.externalCameraActive &&
			   g_players[playerIdx].mfd.consolePageAvailable &&
			   g_players[playerIdx].mfd.page[g_players[playerIdx].mfd.activeIndex] == 7 &&
			   g_currentActionKey != KEY_CONSOLE_TOGGLE
#ifndef XWA_MODERN
			   && !Console_HandleKey(g_currentActionKey, playerIdx)
#endif
	) {
#ifdef XWA_MODERN
/* The legacy interactive console parser is not part of the port; keep its key-swallowing
   boundary so console-page input does not leak into flight actions. */
#endif
		g_currentActionKey = KEY_NONE;
	}

#ifdef XWA_MODERN
	if (Flight_UpdateEntity_HandleLocalHotkeys(playerIdx)) {
		return;
	}
	if (playerIdx == (unsigned int)g_localPlayer && !g_flightSimSideEffectsSuppressed) {
#else
	if (playerIdx == (unsigned int)g_localPlayer && !g_flightSimSideEffectsSuppressed) {
		switch (g_currentActionKey) {
			case KEY_ESCAPE:
				if (g_flightPlayerCount == 1) {
					if (g_filmPlaybackMode || g_filmRecording) {
						g_actionKey = KEY_NONE;
						if (g_filmPlaybackMode != 2 && g_filmRecording) {
							msg_emitInFlightMessage(MSG_FILM_NO_OPTIONS, (int)playerIdx);
							fsfx_PlaySound(63, 0xffffu, playerIdx);
						}
					} else {
						int exitToFrontend;

						g_inputTimestamp = Time_GetFrameDelta() + g_inputTimestamp;
						Sound_StopAllInstances();
						oldConfig = g_gameConfig;
						exitToFrontend = FlightDisplay_RunRestrictedOptionsModal();
						Flight_ApplyConfigToRuntime(&oldConfig, &g_gameConfig);
						Time_GetFrameDelta();
						if (!exitToFrontend) {
							msg_emitInFlightMessage(MSG_MISSION_RESUMED, (int)playerIdx);
						}
						g_actionKey = KEY_NONE;
						sub_4D4640();
						if (exitToFrontend) {
							g_flightReturnToFrontendRequested = 1;
							Player_EndFlightParticipation((int)playerIdx);
							if (playerIdx != (unsigned int)g_localPlayer) {
								msg_addMessagePtr(0, NetSession_GetPlayerName((int)playerIdx));
								msg_emitInFlightMessage(MSG_PLAYER_QUIT, g_localPlayer);
							}
							if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART &&
								g_pilotData.numHumanPlayersLastMission == 1) {
								int playerIff = (uint16_t)g_players[playerIdx].playerIff;
								if (g_missionFlightRuntimeState
										.teamGoalStatus[playerIff][TEAM_GOAL_PRIMARY] != 1) {
									g_missionFlightRuntimeState
										.teamScores[TEAM_SCORE_BONUS_TENTHS][playerIff] -= 2000;
								}
							}
						} else if (!g_useHardware3D) {
							Hud_RedrawSoftwareHudFrame();
						}
					}
				}
				break;
			case 152:
				g_flightRenderStatsDumpRequested = 1;
				break;
			case 133: {
				unsigned int strength = ForceFeedback_GetStrength() + 1250;
				if (strength > 10000u) {
					strength = 0;
				}
				ForceFeedback_SetStrength(strength);
				fsfx_PlaySound(68, 0xffffu, playerIdx);
				g_msgArgTable[0] = (uint16_t)(strength / 1250u);
				g_msgArgTable[1] = 8;
				msg_emitInFlightMessage(MSG_FFEEDBACK, (int)playerIdx);
				break;
			}
			case 134: {
				unsigned int strength = ForceFeedback_GetCenteringStrength() + 1250;
				if (strength > 10000u) {
					strength = 0;
				}
				ForceFeedback_SetCenteringStrength(strength);
				fsfx_PlaySound(68, 0xffffu, playerIdx);
				g_msgArgTable[0] = (uint16_t)(strength / 1250u);
				g_msgArgTable[1] = 8;
				msg_emitInFlightMessage(MSG_FFEEDBACK_CENTERING, (int)playerIdx);
				break;
			}
			case 136:
				g_sw3dSkipOddScanlines = g_sw3dSkipOddScanlines == 0;
				break;
			case 143:
				if (g_flightPlayerCount == 1) {
					if (g_filmPlaybackMode || g_filmRecording) {
						g_actionKey = KEY_NONE;
						if (g_filmRecording) {
							msg_emitInFlightMessage(MSG_FILM_NO_PAUSE, (int)playerIdx);
							fsfx_PlaySound(63, 0xffffu, playerIdx);
						}
					} else {
						int16_t nextKey;

						g_inputTimestamp = Time_GetFrameDelta() + g_inputTimestamp;
						fsfx_PlaySound(68, 0xffffu, playerIdx);
						msg_emitInFlightMessage(MSG_MISSION_PAUSED, (int)playerIdx);
						if (!g_useHardware3D) {
							FlightSw_SetRenderTarget(NULL, 0, 0, 0);
							FlightSurface_Lock();
							FlightText_SetClipRect(0, 0, g_screenWidth, g_screenHeight);
							g_flightFillClipRectFn();
							FlightSurface_Unlock();
							Hud_BlitSoftwareHudTextPanes();
						} else {
							RenderScene_Initialize(1);
							FlightText_FlushQueue();
							RenderScene_DrawVisibleFaces();
						}
						FlightDisplay_Flip();
						Sound_StopAllInstances();
						Music_PauseIfInitialized();
						do {
							nextKey = 0;
							if (FlightInput_HasKeyReady()) {
								nextKey = FlightInput_GetNextKey();
							}
						} while (!nextKey);
						Music_ResumeIfInitialized();
						Time_GetFrameDelta();
						msg_emitInFlightMessage(MSG_MISSION_RESUMED, (int)playerIdx);
						g_actionKey = KEY_NONE;
						sub_4D4640();
					}
				}
				break;
			case 149:
				fsfx_PlaySound(68, 0xffffu, playerIdx);
				msg_emitInFlightMessage(MSG_VERSION, (int)playerIdx);
				break;
			case 232:
				if (!g_filmPlaybackMode) {
					fsfx_PlaySound(68, 0xffffu, playerIdx);
					FlightScreenshot_Capture();
				}
				break;
			case 131:
				if (!g_filmPlaybackMode && !g_filmRecording) {
					g_flightGraphicsDetailPreset = (uint8_t)(g_flightGraphicsDetailPreset + 1);
					if (g_flightGraphicsDetailPreset >= 4u) {
						g_flightGraphicsDetailPreset = 0;
					}
					Flight_ApplyGraphicsDetailPreset(g_flightGraphicsDetailPreset);
					msg_emitInFlightMessage((uint16_t)(g_flightGraphicsDetailPreset + 109), (int)playerIdx);
					fsfx_PlaySound(68, 0xffffu, playerIdx);
				}
				break;
			case 140:
				g_unusedFlightAction140ToggleFlag = g_unusedFlightAction140ToggleFlag == 0;
				break;
			case 146:
				if (g_flightSystemMessagesEnabled) {
					g_flightSystemMessagesEnabled = 0;
					msg_emitInFlightMessage(MSG_SYSTEMSGS_DISABLED, (int)playerIdx);
				} else {
					g_flightSystemMessagesEnabled = 1;
					msg_emitInFlightMessage(MSG_SYSTEMSGS_ENABLED, (int)playerIdx);
				}
				break;
			case KEY_SCROLL_LOCK:
				if (!g_players[playerIdx].currentSeatIdx && g_flightPlayerCount == 1 && !g_filmPlaybackMode) {
					uint8_t oldEnabled = g_padlockMouseLookEnabled;
					g_padlockMouseLookEnabled = oldEnabled == 0;
					if (!oldEnabled) {
						g_padlockMouseLookIgnoreNextDelta = 1;
					}
				}
				break;
			default:
				break;
		}
#endif
		DInput_ReadKeyboardState();
		if (!g_players[playerIdx].viewState.externalCameraActive &&
			g_players[playerIdx].hyperspacePhase == PLAYER_HYPERSPACE_PHASE_NONE &&
			g_players[playerIdx].cockpitVisible && !g_players[playerIdx].currentSeatIdx &&
			g_players[playerIdx].cockpitLookAvailable && g_flightPlayerCount == 1) {
			if (g_dinputKeyboardState[0x4b] || g_currentActionKey == KEY_PAD_4) {
#ifdef XWA_MODERN
				g_players[playerIdx].lookYawOffset -= (int16_t)Flight_ModernScaleLegacyPerAdvanceValue(1200);
#else
				g_players[playerIdx].lookYawOffset -= 1200;
#endif
			}
			if (g_dinputKeyboardState[0x4d] || g_currentActionKey == KEY_PAD_6) {
#ifdef XWA_MODERN
				g_players[playerIdx].lookYawOffset += (int16_t)Flight_ModernScaleLegacyPerAdvanceValue(1200);
#else
				g_players[playerIdx].lookYawOffset += 1200;
#endif
			}
			if (g_dinputKeyboardState[0x48] || g_currentActionKey == KEY_PAD_8) {
#ifdef XWA_MODERN
				g_players[playerIdx].lookPitchOffset +=
					(int16_t)Flight_ModernScaleLegacyPerAdvanceValue(1200);
#else
				g_players[playerIdx].lookPitchOffset += 1200;
#endif
			}
			if (g_dinputKeyboardState[0x50] || g_currentActionKey == KEY_PAD_2) {
#ifdef XWA_MODERN
				g_players[playerIdx].lookPitchOffset -=
					(int16_t)Flight_ModernScaleLegacyPerAdvanceValue(1200);
#else
				g_players[playerIdx].lookPitchOffset -= 1200;
#endif
			}
			if (g_dinputKeyboardState[0x4c] || g_currentActionKey == KEY_PAD_5) {
				g_players[playerIdx].lookYawOffset = 0;
				g_players[playerIdx].lookPitchOffset = 0;
			}
			if (g_padlockMouseLookEnabled && !g_filmRecording && !g_filmPlaybackMode) {
				DInput_PollMouseState();
				if (!g_padlockMouseLookIgnoreNextDelta && (g_dinputMouseState.lX || g_dinputMouseState.lY)) {
#ifdef XWA_MODERN
					int scale =
						(Xwa_Abs32(g_dinputMouseState.lX) > 85 || Xwa_Abs32(g_dinputMouseState.lY) > 85) ? 40
																										 : 15;
					g_players[playerIdx].lookYawOffset = (int16_t)(g_players[playerIdx].lookYawOffset +
																   scale * (int16_t)g_dinputMouseState.lX);
					g_players[playerIdx].lookPitchOffset =
						(int16_t)(g_players[playerIdx].lookPitchOffset +
								  (g_padlockMouseLookInvertPitch ? scale : -scale) *
									  (int16_t)g_dinputMouseState.lY);
#else
					if (abs(g_dinputMouseState.lX) <= 85 && abs(g_dinputMouseState.lY) <= 85) {
						g_players[playerIdx].lookYawOffset += (int16_t)(15 * g_dinputMouseState.lX);
						if (g_padlockMouseLookInvertPitch) {
							g_players[playerIdx].lookPitchOffset += (int16_t)(15 * g_dinputMouseState.lY);
						} else {
							g_players[playerIdx].lookPitchOffset -= (int16_t)(15 * g_dinputMouseState.lY);
						}
					} else {
						g_players[playerIdx].lookYawOffset += (int16_t)(40 * g_dinputMouseState.lX);
						if (g_padlockMouseLookInvertPitch) {
							g_players[playerIdx].lookPitchOffset += (int16_t)(40 * g_dinputMouseState.lY);
						} else {
							g_players[playerIdx].lookPitchOffset -= (int16_t)(40 * g_dinputMouseState.lY);
						}
					}
#endif
				}
				if (g_dinputMouseState.rgbButtons[0]) {
					g_players[playerIdx].lookYawOffset = 0;
					g_players[playerIdx].lookPitchOffset = 0;
				}
				if (g_padlockMouseLookIgnoreNextDelta) {
					g_padlockMouseLookIgnoreNextDelta = 0;
				}
			}
		}
	}

	if (!g_players[playerIdx].viewState.externalCameraActive &&
		g_players[playerIdx].hyperspacePhase == PLAYER_HYPERSPACE_PHASE_NONE &&
		g_players[playerIdx].cockpitVisible &&
		((g_players[playerIdx].currentSeatIdx > 0 && g_players[playerIdx].cockpitToggleAvailable) ||
		 (!g_players[playerIdx].currentSeatIdx && g_players[playerIdx].cockpitLookAvailable))) {
		if (g_players[playerIdx].padlockActive) {
			uint16_t targetObjIdx = g_players[playerIdx].currentTargetObjectIdx;

			if (targetObjIdx == 0xffffu || g_players[playerIdx].viewState.cameraFocusObjIdx == 0xffff) {
				g_players[playerIdx].lookYawOffset = 0;
				g_players[playerIdx].lookPitchOffset = 0;
			} else {
				int dx = g_objectTable[targetObjIdx].world_x -
						 g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].world_x;
				int dy = g_objectTable[targetObjIdx].world_y -
						 g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].world_y;
				int dz = g_objectTable[targetObjIdx].world_z -
						 g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].world_z;
				int side;
				int fwd;
				int up;

				if (g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].mobj->orientMatrixDirty) {
					FVIEW_calcrotatemove(
						g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].pitch,
						g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].yaw,
						&g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx]);
					FVIEW_calcrotateorient(
						g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].roll,
						g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].angleD,
						&g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx]);
				}
				side = Xwa_Dot3Q15Inline(
					g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].mobj->cachedSideX,
					g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].mobj->cachedSideY,
					g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].mobj->cachedSideZ, dx, dy,
					dz);
				fwd = Xwa_Dot3Q15Inline(
					g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].mobj->cachedFwdX,
					g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].mobj->cachedFwdY,
					g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].mobj->cachedFwdZ, dx, dy,
					dz);
				up = Xwa_Dot3Q15Inline(
					g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].mobj->cachedUpX,
					g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].mobj->cachedUpY,
					g_objectTable[g_players[playerIdx].viewState.cameraFocusObjIdx].mobj->cachedUpZ, dx, dy,
					dz);
				trig2_ctop(side, -fwd, up);
				g_players[playerIdx].lookYawOffset = (int16_t)(32760 - trig2_xyangle);
				g_players[playerIdx].lookPitchOffset = (int16_t)(16380 - targetPitch);
			}
		}
	}

	if (g_flightRegionSessionGateMode > 1u && g_dormantFlightRegionSessionEarlyReturnFlag) {
		return;
	}

	if (g_players[playerIdx].regionSessionId) {
		if (!g_flightSimSideEffectsSuppressed && g_players[playerIdx].objectIndex != 0xffff &&
			!g_players[playerIdx].hasCheckpointFlag &&
			g_objectTable[g_players[playerIdx].objectIndex].objectType == OBJ_None) {
			int partner;

			Mission_ProcessFlightGroupWaveCompletion(g_players[playerIdx].boundFlightGroupIdx);
			if (g_missionHeader.body.hangar != XWA_HANGAR_SKIRMISH ||
				Player_BindToAvailableCraft(playerIdx, 0xffffu, 0, 0)) {
				Player_EndFlightParticipation((int)playerIdx);
				if (playerIdx != (unsigned int)g_localPlayer) {
					Flight_UpdateEntity_NotifyPlayerNoMore(playerIdx);
				}
				partner = Flight_UpdateEntity_FindCheckpointPartner(playerIdx);
				if (partner != -1) {
					Player_EndFlightParticipation(partner);
					if (partner != g_localPlayer) {
						Flight_UpdateEntity_NotifyPlayerNoMore((unsigned int)partner);
					}
				}
			} else {
				if (playerIdx == (unsigned int)g_localPlayer) {
					msg_emitLocalPlayerCraftMessage(MSG_PREVIOUS_DESTROYED);
				}
				partner = Flight_UpdateEntity_FindCheckpointPartner(playerIdx);
				if (partner == g_localPlayer) {
					msg_emitLocalPlayerCraftMessage(MSG_PREVIOUS_DESTROYED);
				}
			}
		}
		goto finish;
	}

	if (!g_players[playerIdx].hyperspacePhase) {
		if ((g_flightKeyMods & 0x0d) == 1 && !g_players[playerIdx].viewState.playerInputBlocked &&
			!g_players[playerIdx].mapCameraState) {
			laser_fireplayerweapon((int)playerIdx);
		}
		{
			uint16_t keyMods = g_flightKeyMods;
			uint16_t targetKeyMods = (uint16_t)(keyMods & 0x0e);
			uint16_t savedTargetKeyMods = (uint16_t)(g_players[playerIdx].savedKeyMods & 0x0e);

			if (targetKeyMods == 2) {
				if (savedTargetKeyMods == targetKeyMods) {
					g_players[playerIdx].keyModsHoldTimer =
						(uint16_t)(g_players[playerIdx].keyModsHoldTimer + (uint16_t)g_elapsedTicks);
				} else {
					g_players[playerIdx].keyModsHoldTimer = (uint16_t)g_elapsedTicks;
				}
				g_players[playerIdx].savedKeyMods = keyMods;
				if (g_players[playerIdx].keyModsHoldTimer < 59u) {
					keyMods &= 0xfffdu;
					g_flightKeyMods = keyMods;
				}
			} else {
				if (savedTargetKeyMods == 2 && g_players[playerIdx].keyModsHoldTimer < 59u) {
					int target = 0xffff;
					if (g_players[playerIdx].mapCameraState) {
						target = FlightMap_PickObjectNearestScreenCenter((int)playerIdx);
					} else if (!g_players[playerIdx].viewState.playerInputBlocked) {
						target = Player_PickTargetInSight((int)playerIdx);
					}
					if ((uint16_t)target != 0xffffu) {
						Player_SetTarget(target, (int)playerIdx);
					}
				}
				g_players[playerIdx].savedKeyMods = g_flightKeyMods;
				g_players[playerIdx].keyModsHoldTimer = 0;
			}
		}
	}

	if (!g_players[playerIdx].msgTypeId) {
		Flight_ProcessPlayerActions(playerIdx);
	} else {
		switch (g_currentActionKey) {
			case KEY_ESCAPE:
				g_players[playerIdx].msgTypeId = 0;
				msg_emitInFlightMessage(MSG_MESSAGE_ABORTED, (int)playerIdx);
				break;
			case KEY_TAB:
				++g_players[playerIdx].msgTypeId;
				if (g_players[playerIdx].msgTypeId > 3u) {
					g_players[playerIdx].msgTypeId = 1;
				}
				msg_addMessagePtr(0, g_players[playerIdx].msgText);
				msg_emitInFlightMessage((uint16_t)(g_players[playerIdx].msgTypeId + MSG_PLAYER_MESSAGE),
										(int)playerIdx);
				break;
			case KEY_BACKSPACE:
				if (g_players[playerIdx].msgLength) {
					--g_players[playerIdx].msgLength;
					g_players[playerIdx].msgText[g_players[playerIdx].msgLength] = '_';
					g_players[playerIdx].msgText[g_players[playerIdx].msgLength + 1] = '\0';
				}
				msg_addMessagePtr(0, g_players[playerIdx].msgText);
				msg_emitInFlightMessage((uint16_t)(g_players[playerIdx].msgTypeId + MSG_PLAYER_MESSAGE),
										(int)playerIdx);
				break;
			case KEY_ENTER:
				g_players[playerIdx].msgText[g_players[playerIdx].msgLength] = '\0';
				Flight_UpdateEntity_SendTypedMessage(playerIdx, g_players[playerIdx].msgText);
				break;
			case KEY_ALT_1:
			case KEY_ALT_2:
			case KEY_ALT_3:
			case 158: {
				unsigned int tauntIdx = (unsigned int)g_currentActionKey - 155u;
				g_players[playerIdx].msgText[g_players[playerIdx].msgLength] = '\0';
				Flight_UpdateEntity_SendTypedMessage(playerIdx,
													 &g_playerTauntText[playerIdx][tauntIdx * 70u]);
				break;
			}
			default:
				if (g_currentActionKey) {
					if (g_players[playerIdx].msgLength < 48u) {
						g_players[playerIdx].msgText[g_players[playerIdx].msgLength] =
							(char)g_currentActionKey;
						g_players[playerIdx].msgText[g_players[playerIdx].msgLength + 1] = '_';
						g_players[playerIdx].msgText[g_players[playerIdx].msgLength + 2] = '\0';
						++g_players[playerIdx].msgLength;
					}
				} else if (playerIdx != (unsigned int)g_localPlayer ||
						   (int16_t)g_playerFlightTransientTimers[playerIdx].flightGroupMessagePaneTimer >=
							   236) {
					break;
				}
				msg_addMessagePtr(0, g_players[playerIdx].msgText);
				msg_emitInFlightMessage((uint16_t)(g_players[playerIdx].msgTypeId + MSG_PLAYER_MESSAGE),
										(int)playerIdx);
				break;
		}
	}
	if (!g_players[playerIdx].connectedFlag) {
		goto finish;
	}
	seatIdx = g_players[playerIdx].currentSeatIdx - 1;
	if (g_players[playerIdx].objectIndex != 0xffff &&
		g_objectTable[g_players[playerIdx].objectIndex].mobj->pCraft->objectKind == 9) {
		goto finish;
	}

	FlightInput_ApplyDeadzone();
	g_scaledInputPitch = (int16_t)((uint16_t)g_scaledInputPitch << 1);

	if (g_players[playerIdx].viewState.playerInputBlocked || g_players[playerIdx].mapCameraState) {
		do {
			PlayerViewState* view = &g_players[playerIdx].viewState;
			uint8_t mapState = g_players[playerIdx].mapCameraState;
			int16_t yawDelta;
			int16_t pitchDelta;
			int16_t zoomMode;
			int16_t step;

			if (mapState) {
				if ((mapState & 0x80u) == 0) {
					uint8_t timer = (uint8_t)(mapState & 0x7fu);
					if (timer > 1u && !g_flightSimSideEffectsSuppressed) {
						if (timer <= (uint16_t)g_elapsedTicks) {
							timer = (uint8_t)((uint16_t)g_elapsedTicks + 1u);
						}
						g_players[playerIdx].mapCameraState = (uint8_t)(timer - (uint16_t)g_elapsedTicks);
					}
				} else {
					uint8_t timer = (uint8_t)(mapState & 0x7fu);
					if (timer >= 127u) {
						if (view->cameraFocusObjIdx != 0xffff) {
							view->savedTargetX = g_objectTable[view->cameraFocusObjIdx].world_x;
							view->savedTargetY = g_objectTable[view->cameraFocusObjIdx].world_y;
							view->savedTargetZ =
								g_objectTable[view->cameraFocusObjIdx].world_z + view->cameraDistance;
						}
						view->cameraFocusObjIdx = 0xffff;
					} else if (!g_flightSimSideEffectsSuppressed) {
						if ((uint16_t)g_elapsedTicks + timer > 127u) {
							timer = (uint8_t)(127u - (uint16_t)g_elapsedTicks);
						}
						g_players[playerIdx].mapCameraState =
							(uint8_t)(((uint16_t)g_elapsedTicks + timer) | 0x80u);
					}
				}
			}

			if (g_players[playerIdx].mapCameraState) {
#ifdef XWA_MODERN
				if (Xwa_Abs32((int16_t)g_scaledInputYaw) < 128) {
					g_scaledInputYaw = 0;
				}
				if (Xwa_Abs32((int16_t)g_scaledInputPitch) < 48) {
					g_scaledInputPitch = 0;
				}
#else
				int yawMagnitude = g_scaledInputYaw;
				int pitchMagnitude = g_scaledInputPitch;

				if ((uint16_t)g_scaledInputYaw >= 0x8000u) {
					yawMagnitude = (int16_t)-g_scaledInputYaw;
				}
				if ((uint16_t)g_scaledInputPitch >= 0x8000u) {
					pitchMagnitude = (int16_t)-g_scaledInputPitch;
				}
				if (yawMagnitude < 128) {
					g_scaledInputYaw = 0;
				}
				if (pitchMagnitude < 48) {
					g_scaledInputPitch = 0;
				}
#endif
			}
			yawDelta = (int16_t)MATH2_ABoverC32(g_scaledInputYaw, (uint16_t)g_elapsedTicks, 236);
			pitchDelta = (int16_t)MATH2_ABoverC32(g_scaledInputPitch, (uint16_t)g_elapsedTicks, 236);

			if (g_players[playerIdx].mapCameraState & 0x80u) {
				int panScale = (view->savedTargetZ >> 14) + 1;
				view->savedTargetX += yawDelta * panScale;
				view->savedTargetY += pitchDelta * panScale;
			} else {
				if (view->cameraFocusObjIdx == 0xffff) {
					if (pitchDelta || yawDelta) {
						FlightView_RotateViewByInput(pitchDelta, (int16_t)-yawDelta, (int)playerIdx);
					}
				} else {
					view->hudAimY = (uint16_t)(view->hudAimY + (uint16_t)yawDelta);
					view->hudAimX = (uint16_t)(view->hudAimX + (uint16_t)pitchDelta);
				}
			}

			zoomMode = (int16_t)(g_flightKeyMods & 0x0f);
			if (zoomMode != 1 && zoomMode != 2) {
				if (g_players[playerIdx].mapCameraState && view->cameraDistanceStep > 0x100u) {
#ifdef XWA_MODERN
					view->cameraDistanceStep =
						(uint16_t)(view->cameraDistanceStep -
								   Flight_ModernScaleMapCameraStep((view->cameraDistanceStep >> 3) + 32,
																   playerIdx, -1));
#else
					view->cameraDistanceStep =
						(uint16_t)(view->cameraDistanceStep - (view->cameraDistanceStep >> 3) - 32);
#endif
				} else {
					view->cameraDistanceStep = 32;
#ifdef XWA_MODERN
					Flight_ModernResetMapCameraStep(playerIdx);
#endif
				}
			} else {
				if (g_players[playerIdx].mapCameraState > 1u) {
#ifdef XWA_MODERN
					Flight_ModernResetMapCameraStep(playerIdx);
#endif
					if ((view->savedTargetZ & ~1) <= 57344) {
						view->cameraDistanceStep = (uint16_t)(view->savedTargetZ >> 1);
					} else {
						view->cameraDistanceStep = 28672;
					}
					if (view->cameraDistanceStep < 0x800u) {
						view->cameraDistanceStep = 0x800;
					}
				} else if (g_players[playerIdx].mapCameraState == 1) {
					if (view->cameraFocusObjIdx != 0xffff) {
#ifdef XWA_MODERN
						Flight_ModernResetMapCameraStep(playerIdx);
#endif
						if ((view->cameraDistance & ~1) <= 57344) {
							view->cameraDistanceStep = (uint16_t)(view->cameraDistance >> 1);
						} else {
							view->cameraDistanceStep = 28672;
						}
						if (view->cameraDistanceStep < 0x100u) {
							view->cameraDistanceStep = 0x100;
						}
					} else if (view->aimTargetIdx != 0xffff) {
#ifdef XWA_MODERN
						Flight_ModernResetMapCameraStep(playerIdx);
#endif
						int dist = collide_roughdistance3d(
							g_objectTable[view->aimTargetIdx].world_x - view->savedTargetX,
							g_objectTable[view->aimTargetIdx].world_y - view->savedTargetY,
							g_objectTable[view->aimTargetIdx].world_z - view->savedTargetZ);
						if (dist <= 28672) {
							view->cameraDistanceStep = (uint16_t)dist;
						} else {
							view->cameraDistanceStep = 28672;
						}
						if (view->cameraDistanceStep < 0x400u) {
							view->cameraDistanceStep = 0x400;
						}
					} else {
#ifdef XWA_MODERN
						view->cameraDistanceStep =
							(uint16_t)(view->cameraDistanceStep +
									   Flight_ModernScaleMapCameraStep(
										   ((uint16_t)(view->cameraDistanceStep + 32) >> 3) + 32, playerIdx,
										   1));
#else
						view->cameraDistanceStep =
							(uint16_t)(((uint16_t)(view->cameraDistanceStep + 32) >> 3) +
									   view->cameraDistanceStep + 32);
#endif
						if (view->cameraDistanceStep > 0x4000u) {
							view->cameraDistanceStep = 0x4000;
#ifdef XWA_MODERN
							Flight_ModernResetMapCameraStep(playerIdx);
#endif
						}
					}
				} else {
#ifdef XWA_MODERN
					view->cameraDistanceStep = (uint16_t)(view->cameraDistanceStep +
														  Flight_ModernScaleMapCameraStep(32, playerIdx, 1));
#else
					view->cameraDistanceStep = (uint16_t)(view->cameraDistanceStep + 32);
#endif
					if (view->cameraDistanceStep > 0x400u) {
						view->cameraDistanceStep = 0x400;
#ifdef XWA_MODERN
						Flight_ModernResetMapCameraStep(playerIdx);
#endif
					}
				}

				if (zoomMode == 1) {
					if (!g_players[playerIdx].mapCameraState) {
						step = (int16_t)MATH2_ABoverC32((int16_t)view->cameraDistanceStep,
														(uint16_t)g_elapsedTicks, 236);
						view->cameraDistance -= step;
						if (view->cameraDistance < 80) {
							view->cameraDistance = 80;
						}
					} else if (view->cameraFocusObjIdx != 0xffff) {
						int minDist;
						step = (int16_t)MATH2_ABoverC32((int16_t)view->cameraDistanceStep,
														(uint16_t)g_elapsedTicks, 236);
						view->cameraDistance -= step;
						minDist =
							g_modelTypeTable[(uint16_t)g_objectTable[view->cameraFocusObjIdx].objectType]
								.maxBoundsExtent +
							512;
						if (view->cameraDistance < minDist) {
							view->cameraDistance = minDist;
						}
					} else {
						FVIEW_BuildCameraOrient(0, (int16_t)view->viewPitch, (int16_t)view->viewYaw, 0,
												view->aimTargetIdx == 0xffff ? (int16_t)view->hudAimX : 0,
												view->aimTargetIdx == 0xffff ? (int16_t)view->hudAimY : 0,
												NULL, (int)playerIdx);
						view->savedTargetX += Xwa_Q15MulReuseFirstSlot(
							(int16_t)MATH2_ABoverC32((int16_t)view->cameraDistanceStep,
													 (uint16_t)g_elapsedTicks, 236),
							g_camMatR2_X);
						view->savedTargetY += Xwa_Q15MulReuseFirstSlot(
							(int16_t)MATH2_ABoverC32((int16_t)view->cameraDistanceStep,
													 (uint16_t)g_elapsedTicks, 236),
							g_camMatR2_Y);
						view->savedTargetZ += Xwa_Q15MulReuseFirstSlot(
							(int16_t)MATH2_ABoverC32((int16_t)view->cameraDistanceStep,
													 (uint16_t)g_elapsedTicks, 236),
							g_camMatR2_Z);
						if (view->aimTargetIdx != 0xffff) {
							int targetDistance = collide_roughdistance3d(
								g_objectTable[view->aimTargetIdx].world_x - view->savedTargetX,
								g_objectTable[view->aimTargetIdx].world_y - view->savedTargetY,
								g_objectTable[view->aimTargetIdx].world_z - view->savedTargetZ);
							if (targetDistance <
								g_modelTypeTable[(uint16_t)g_objectTable[view->aimTargetIdx].objectType]
										.maxBoundsExtent +
									512) {
#ifdef XWA_MODERN
								int correction =
									targetDistance -
									g_modelTypeTable[(uint16_t)g_objectTable[view->aimTargetIdx].objectType]
										.maxBoundsExtent -
									512;
#else
								int correction =
									targetDistance -
									g_modelTypeTable[(uint16_t)g_objectTable[view->cameraFocusObjIdx]
														 .objectType]
										.maxBoundsExtent -
									512;
#endif
								view->savedTargetX += Xwa_Q15MulReuseFirstSlot(correction, g_camMatR2_X);
								view->savedTargetY += Xwa_Q15MulReuseFirstSlot(correction, g_camMatR2_Y);
								view->savedTargetZ += Xwa_Q15MulReuseFirstSlot(correction, g_camMatR2_Z);
							}
						}
						if (g_players[playerIdx].mapCameraState > 1u && view->savedTargetZ < 80) {
							view->savedTargetZ = 80;
						}
						Flight_UpdateEntity_ClampMapTarget(view);
					}
				} else {
					if (!g_players[playerIdx].mapCameraState) {
						step = (int16_t)MATH2_ABoverC32((int16_t)view->cameraDistanceStep,
														(uint16_t)g_elapsedTicks, 236);
						view->cameraDistance += step;
						if (view->cameraDistance > 0x2000) {
							view->cameraDistance = 0x2000;
						}
					} else if (view->cameraFocusObjIdx != 0xffff) {
						step = (int16_t)MATH2_ABoverC32((int16_t)view->cameraDistanceStep,
														(uint16_t)g_elapsedTicks, 236);
						view->cameraDistance += step;
					} else {
						int16_t hudAimX = 0;
						int16_t hudAimY = 0;
						if (view->aimTargetIdx == 0xffff) {
							hudAimX = (int16_t)view->hudAimX;
							hudAimY = (int16_t)view->hudAimY;
						}
						FVIEW_BuildCameraOrient(0, (int16_t)view->viewPitch, (int16_t)view->viewYaw, 0,
												hudAimX, hudAimY, NULL, (int)playerIdx);
						view->savedTargetX -= Xwa_Q15MulReuseFirstSlot(
							(int16_t)MATH2_ABoverC32((int16_t)view->cameraDistanceStep,
													 (uint16_t)g_elapsedTicks, 236),
							g_camMatR2_X);
						view->savedTargetY -= Xwa_Q15MulReuseFirstSlot(
							(int16_t)MATH2_ABoverC32((int16_t)view->cameraDistanceStep,
													 (uint16_t)g_elapsedTicks, 236),
							g_camMatR2_Y);
						view->savedTargetZ -= Xwa_Q15MulReuseFirstSlot(
							(int16_t)MATH2_ABoverC32((int16_t)view->cameraDistanceStep,
													 (uint16_t)g_elapsedTicks, 236),
							g_camMatR2_Z);
						Flight_UpdateEntity_ClampMapTarget(view);
					}
				}
			}
		} while (0);
		goto finish;
	}

	if (g_players[playerIdx].hyperspacePhase == PLAYER_HYPERSPACE_PHASE_NONE) {
		CraftData* craft;
		uint16_t throttleScale;
		int swappedControls;
		int powerMargin;
		int powerScale;
		uint16_t rollRate;
		uint16_t pitchRate;
		int yawTarget;
		int rollTarget;
		int pitchTargetLocal;
		int rollWhole;
		uint16_t rollFrac;
		int pitchWhole;
		uint16_t pitchFrac;
#ifdef XWA_MODERN
		ModernJoystickIntegrationTrace joystickTrace;
		int joystickTraceActive;
#endif

		craft = g_objectTable[g_players[playerIdx].objectIndex].mobj->pCraft;
#ifdef XWA_MODERN
		joystickTraceActive = g_modernJoystickTraceEnabled && playerIdx == (unsigned int)g_localPlayer &&
							  !g_flightSimSideEffectsSuppressed && seatIdx == -1;
		if (joystickTraceActive) {
			memset(&joystickTrace, 0, sizeof(joystickTrace));
			joystickTrace.timestamp = g_players[playerIdx].lockstepTimestamp;
			joystickTrace.elapsedTicks = g_elapsedTicks;
		}
#endif

		if (seatIdx == -1) {
			throttleScale = craft->throttleSpeed;
			if ((craft->workingSubsystems & 0x40) == 0) {
				throttleScale = 0;
			}
			if (throttleScale < 0x5555u) {
				throttleScale = (uint16_t)(2 * throttleScale + 0x5555u);
			} else {
				throttleScale = (uint16_t)(0xffff - (throttleScale - 0x5555) / 2);
			}
			if (g_players[playerIdx].yawRollSwap) {
				int16_t tmp = g_scaledInputRoll;
				g_scaledInputRoll = g_scaledInputYaw;
				g_scaledInputYaw = tmp;
			}
		} else {
			throttleScale = 0xffffu;
		}

		swappedControls = 0;
		if ((g_flightKeyMods & 0x0e) == 2) {
			swappedControls = 1;
		}
		if (swappedControls) {
			int16_t tmp = g_scaledInputRoll;
			g_scaledInputRoll = g_scaledInputYaw;
			g_scaledInputYaw = tmp;
		}
#ifdef XWA_MODERN
		if (joystickTraceActive) {
			joystickTrace.scaledYaw = g_scaledInputYaw;
			joystickTrace.scaledPitch = g_scaledInputPitch;
			joystickTrace.scaledRoll = g_scaledInputRoll;
		}
#endif

		powerMargin = craft->laserRedirect;
		if (craft->systemFlags & 1) {
			powerMargin += craft->shieldRedirect;
		} else {
			powerMargin += craft->laserRedirect;
		}
		powerMargin = 4 - powerMargin;
		if (powerMargin < 0) {
			powerScale = powerMargin * 62464;
		} else {
			powerScale = powerMargin * 3072;
		}
		rollRate = (uint16_t)MATH2_fraction((uint16_t)craft->aiFlight.rollRate, throttleScale);
		if (seatIdx == -1) {
			rollRate = (uint16_t)MATH2_fraction(rollRate, (uint16_t)craft->aiFlight.motionScale);
		}
		if (powerMargin > 0) {
			rollRate = (uint16_t)(rollRate + MATH2_fraction(rollRate, (uint16_t)powerScale));
		} else {
			rollRate = (uint16_t)(rollRate - MATH2_fraction(rollRate, (uint16_t)powerScale));
		}
		rollWhole = rollRate / 14336;
		rollFrac = (uint16_t)MATH2_divide((uint16_t)(rollRate % 14336), 0x3800u);
		yawTarget = g_scaledInputYaw;
		if ((uint16_t)g_scaledInputYaw >= 0x8000u) {
			yawTarget = -yawTarget;
		}
		yawTarget = yawTarget * rollWhole + (int)MATH2_fraction((uint16_t)yawTarget, rollFrac);
		if ((uint16_t)g_scaledInputYaw >= 0x8000u) {
			yawTarget = -yawTarget;
		}
		rollTarget = g_scaledInputRoll;
		if ((uint16_t)g_scaledInputRoll >= 0x8000u) {
			rollTarget = -rollTarget;
		}
		rollTarget = rollTarget * rollWhole + (int)MATH2_fraction((uint16_t)rollTarget, rollFrac);
		if ((uint16_t)g_scaledInputRoll >= 0x8000u) {
			rollTarget = -rollTarget;
		}

		pitchRate = (uint16_t)MATH2_fraction((uint16_t)craft->aiFlight.pitchRate, throttleScale);
		if (seatIdx == -1) {
			pitchRate = (uint16_t)MATH2_fraction(pitchRate, (uint16_t)craft->aiFlight.motionScale);
		}
		if (powerMargin > 0) {
			pitchRate = (uint16_t)(pitchRate + MATH2_fraction(pitchRate, (uint16_t)powerScale));
		} else {
			pitchRate = (uint16_t)(pitchRate - MATH2_fraction(pitchRate, (uint16_t)powerScale));
		}
		pitchWhole = pitchRate / 5120;
		pitchFrac = (uint16_t)MATH2_divide((uint16_t)(pitchRate % 5120), 0x1400u);
		pitchTargetLocal = g_scaledInputPitch;
		if ((uint16_t)g_scaledInputPitch >= 0x8000u) {
			pitchTargetLocal = -pitchTargetLocal;
		}
		pitchTargetLocal =
			pitchTargetLocal * pitchWhole + (int)MATH2_fraction((uint16_t)pitchTargetLocal, pitchFrac);
		if ((uint16_t)g_scaledInputPitch >= 0x8000u) {
			pitchTargetLocal = -pitchTargetLocal;
		}

		g_forceFeedbackLocalSpeedSnapshot.words[1] = (uint16_t)rollTarget;
		if (seatIdx == -1 && ((craft->workingSubsystems & 0x20) == 0 ||
							  (craft->beamEffectAccum[1] && !craft->chaffActiveTimer))) {
			yawTarget = 0;
			pitchTargetLocal = 0;
			rollTarget = 0;
		}
#ifdef XWA_MODERN
		if (joystickTraceActive) {
			joystickTrace.targetYaw = yawTarget;
			joystickTrace.targetPitch = pitchTargetLocal;
			joystickTrace.targetRoll = rollTarget;
		}
#endif

		if (g_players[playerIdx].gap71_field16 == (int16_t)swappedControls) {
			g_players[playerIdx].smoothedInputYaw =
				Flight_UpdateEntity_SmoothAxis(g_players[playerIdx].smoothedInputYaw, yawTarget);
			g_players[playerIdx].smoothedInputPitch =
				Flight_UpdateEntity_SmoothAxis(g_players[playerIdx].smoothedInputPitch, pitchTargetLocal);
			g_players[playerIdx].smoothedInputRoll =
				Flight_UpdateEntity_SmoothAxis(g_players[playerIdx].smoothedInputRoll, rollTarget);
		} else {
			g_players[playerIdx].smoothedInputYaw = 0;
			g_players[playerIdx].smoothedInputRoll = 0;
		}
		g_players[playerIdx].gap71_field16 = (int16_t)swappedControls;
#ifdef XWA_MODERN
		if (joystickTraceActive) {
			joystickTrace.smoothedYaw = g_players[playerIdx].smoothedInputYaw;
			joystickTrace.smoothedPitch = g_players[playerIdx].smoothedInputPitch;
			joystickTrace.smoothedRoll = g_players[playerIdx].smoothedInputRoll;
		}
#endif

		yawTarget = MATH2_ABoverC32(g_players[playerIdx].smoothedInputYaw, (uint16_t)g_elapsedTicks, 236);
		pitchTargetLocal =
			MATH2_ABoverC32(g_players[playerIdx].smoothedInputPitch, (uint16_t)g_elapsedTicks, 236);
		rollTarget = MATH2_ABoverC32(g_players[playerIdx].smoothedInputRoll, (uint16_t)g_elapsedTicks, 236);
		if (((craft->workingSubsystems & 0x20) == 0 || g_players[playerIdx].inputDisabledFlag ||
			 (craft->beamEffectAccum[1] && !craft->chaffActiveTimer)) &&
			(seatIdx == -1 || swappedControls)) {
			yawTarget = 0;
			pitchTargetLocal = 0;
			rollTarget = 0;
		}
		if (g_players[playerIdx].inputDisabledFlag) {
			swappedControls = 0;
			rollTarget = 0;
		}
#ifdef XWA_MODERN
		if (joystickTraceActive) {
			uint16_t objectIndex = g_players[playerIdx].objectIndex;
			joystickTrace.stepYaw = yawTarget;
			joystickTrace.stepPitch = pitchTargetLocal;
			joystickTrace.stepRoll = rollTarget;
			joystickTrace.oldPitch = g_objectTable[objectIndex].pitch;
			joystickTrace.oldYaw = g_objectTable[objectIndex].yaw;
			joystickTrace.oldRoll = g_objectTable[objectIndex].roll;
		}
#endif

		if ((swappedControls || seatIdx == -1) && !g_players[playerIdx].aiControlledFlag) {
			if (pitchTargetLocal || yawTarget) {
				USER_calcdeltapitch(pitchTargetLocal, -yawTarget, (uint16_t)g_players[playerIdx].objectIndex);
				g_objectTable[g_players[playerIdx].objectIndex].mobj->orientMatrixDirty = 1;
				g_objectTable[g_players[playerIdx].objectIndex].mobj->moveVectorDirty = 1;
			}
			if (rollTarget) {
				g_objectTable[g_players[playerIdx].objectIndex].roll =
					(Q16Angle)(g_objectTable[g_players[playerIdx].objectIndex].roll -
							   2 * (int16_t)rollTarget);
				g_objectTable[g_players[playerIdx].objectIndex].mobj->orientMatrixDirty = 1;
				g_objectTable[g_players[playerIdx].objectIndex].mobj->moveVectorDirty = 1;
			}
			if (g_objectTable[g_players[playerIdx].objectIndex].genusId == GENUS_Fighter && yawTarget) {
				g_objectTable[g_players[playerIdx].objectIndex].roll =
					(Q16Angle)(g_objectTable[g_players[playerIdx].objectIndex].roll - (uint16_t)yawTarget);
			}
		}
#ifdef XWA_MODERN
		if (joystickTraceActive) {
			uint16_t objectIndex = g_players[playerIdx].objectIndex;
			joystickTrace.newPitch = g_objectTable[objectIndex].pitch;
			joystickTrace.newYaw = g_objectTable[objectIndex].yaw;
			joystickTrace.newRoll = g_objectTable[objectIndex].roll;
			FlightDebug_LogJoystickIntegration(&joystickTrace);
		}
#endif

		if (seatIdx >= 0 && !swappedControls && !g_players[playerIdx].viewState.externalCameraActive) {
			ModelIndex modelIndex = (ModelIndex)GetModelIndexFromType(
				(ObjectTypeId)g_objectTable[g_players[playerIdx].objectIndex].objectType);
			if (g_objectTable[g_players[playerIdx].objectIndex].objectType != OBJ_None &&
				modelIndex != (ModelIndex)0xffffu) {
				int16_t limitA;
				int16_t limitB;
#ifdef XWA_MODERN
				double accumA;
				double accumB;
				double integrationScale;
				ModernTurretAngleRemainder* angleRemainder;

				if (XwaModernFlightTiming_IsHighRate()) {
					integrationScale = (double)(uint16_t)g_elapsedTicks / 8.0;
					accumA = craft->turretAim.aimAccumA[seatIdx] +
							 ((double)g_players[playerIdx].smoothedInputPitch * 0.1 -
							  craft->turretAim.aimAccumA[seatIdx] * 0.25) *
								 integrationScale;
					accumB = craft->turretAim.aimAccumB[seatIdx] +
							 ((double)g_players[playerIdx].smoothedInputYaw * 0.1 -
							  craft->turretAim.aimAccumB[seatIdx] * 0.25) *
								 integrationScale;
				} else {
					integrationScale = 1.0;
					accumA = (double)g_players[playerIdx].smoothedInputPitch * 0.1 -
							 craft->turretAim.aimAccumA[seatIdx] * 0.25 + craft->turretAim.aimAccumA[seatIdx];
					accumB = (double)g_players[playerIdx].smoothedInputYaw * 0.1 -
							 craft->turretAim.aimAccumB[seatIdx] * 0.25 + craft->turretAim.aimAccumB[seatIdx];
				}
				angleRemainder = &g_modernTurretAngleRemainders[playerIdx][seatIdx];
				if (!angleRemainder->initialized ||
					angleRemainder->objectIdx != g_players[playerIdx].objectIndex ||
					angleRemainder->objectSignature !=
						g_objectTable[g_players[playerIdx].objectIndex].objectSignature) {
					angleRemainder->objectIdx = (uint16_t)g_players[playerIdx].objectIndex;
					angleRemainder->objectSignature =
						g_objectTable[g_players[playerIdx].objectIndex].objectSignature;
					angleRemainder->initialized = 1;
					angleRemainder->axisA = 0.0;
					angleRemainder->axisB = 0.0;
				}
				craft->turretAim.aimAccumA[seatIdx] = (float)accumA;
				{
					double angleStep = accumA * integrationScale;
					int angleDelta;
					if (XwaModernFlightTiming_IsHighRate()) {
						angleStep += angleRemainder->axisA;
					} else {
						angleRemainder->axisA = 0.0;
					}
					angleDelta = (int)angleStep;
					angleRemainder->axisA = angleStep - angleDelta;
					craft->turretAim.aimAngleA[seatIdx] =
						(Q16Angle)(craft->turretAim.aimAngleA[seatIdx] + angleDelta);
				}
				craft->turretAim.aimAccumB[seatIdx] = (float)accumB;
				{
					double angleStep = accumB * integrationScale;
					int angleDelta;
					if (XwaModernFlightTiming_IsHighRate()) {
						angleStep += angleRemainder->axisB;
					} else {
						angleRemainder->axisB = 0.0;
					}
					angleDelta = (int)angleStep;
					angleRemainder->axisB = angleStep - angleDelta;
					craft->turretAim.aimAngleB[seatIdx] =
						(Q16Angle)(craft->turretAim.aimAngleB[seatIdx] + angleDelta);
				}
#else
				{
					double accum = g_players[playerIdx].smoothedInputPitch * 0.1f -
								   craft->turretAim.aimAccumA[seatIdx] * 0.25f;
					accum += craft->turretAim.aimAccumA[seatIdx];
					craft->turretAim.aimAccumA[seatIdx] = (float)accum;
					craft->turretAim.aimAngleA[seatIdx] =
						(Q16Angle)(craft->turretAim.aimAngleA[seatIdx] + (int)accum);
				}
				{
					double accum = g_players[playerIdx].smoothedInputYaw * 0.1f -
								   craft->turretAim.aimAccumB[seatIdx] * 0.25f;
					accum += craft->turretAim.aimAccumB[seatIdx];
					craft->turretAim.aimAccumB[seatIdx] = (float)accum;
					craft->turretAim.aimAngleB[seatIdx] =
						(Q16Angle)(craft->turretAim.aimAngleB[seatIdx] + (int)accum);
				}
#endif
				limitA = g_modelDefs[(uint16_t)modelIndex].turretAimLimitA[seatIdx];
				limitB = g_modelDefs[(uint16_t)modelIndex].turretAimLimitB[seatIdx];
				if ((int16_t)craft->turretAim.aimAngleA[seatIdx] > limitA) {
					craft->turretAim.aimAngleA[seatIdx] = (uint16_t)limitA;
#ifdef XWA_MODERN
					angleRemainder->axisA = 0.0;
#endif
				}
				if ((int16_t)craft->turretAim.aimAngleA[seatIdx] < -limitA) {
					craft->turretAim.aimAngleA[seatIdx] = (uint16_t)-limitA;
#ifdef XWA_MODERN
					angleRemainder->axisA = 0.0;
#endif
				}
				if ((int16_t)craft->turretAim.aimAngleB[seatIdx] > limitB) {
					craft->turretAim.aimAngleB[seatIdx] = (uint16_t)limitB;
#ifdef XWA_MODERN
					angleRemainder->axisB = 0.0;
#endif
				}
				if ((int16_t)craft->turretAim.aimAngleB[seatIdx] < -limitB) {
					craft->turretAim.aimAngleB[seatIdx] = (uint16_t)-limitB;
#ifdef XWA_MODERN
					angleRemainder->axisB = 0.0;
#endif
				}
				g_players[playerIdx].smoothedInputYaw = 0;
				g_players[playerIdx].smoothedInputPitch = 0;
				g_players[playerIdx].smoothedInputRoll = 0;
			}
		}
	}

finish:
	if (g_players[playerIdx].inputDisabledFlag) {
		if (g_players[playerIdx].inputDisabledFlag == 4) {
			if (paiman_UpdatePlayerDeliveryAutopilot(playerIdx)) {
				g_players[playerIdx].inputDisabledFlag = 0;
			}
		} else if (g_players[playerIdx].inputDisabledFlag == 5) {
			if (paiman_UpdatePlayerTargetTrackingAutopilot(playerIdx)) {
				g_players[playerIdx].inputDisabledFlag = 0;
			}
		} else {
			if (paiman_UpdateBoardOrPickupAutopilot(playerIdx)) {
				g_players[playerIdx].inputDisabledFlag = 0;
			}
		}
	}

	if (g_players[playerIdx].objectIndex != 0xffff) {
		int objIdx = g_players[playerIdx].objectIndex;
		uint16_t carriedObjIdx = g_objectTable[objIdx].mobj->pCraft->carriedObjectIndex;

		if (carriedObjIdx != 0xffffu && g_objectTable[carriedObjIdx].mobj != NULL) {
			g_objectTable[carriedObjIdx].pitch = g_objectTable[objIdx].pitch;
			g_objectTable[carriedObjIdx].yaw = g_objectTable[objIdx].yaw;
			g_objectTable[carriedObjIdx].roll = g_objectTable[objIdx].roll;
			g_objectTable[carriedObjIdx].angleD = g_objectTable[objIdx].angleD;
			g_objectTable[carriedObjIdx].mobj->moveVectorDirty = 1;
			g_objectTable[carriedObjIdx].mobj->orientMatrixDirty = 1;
		}
	}
}
