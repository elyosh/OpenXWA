#include "xwa_runtime/runtime/flight_task.h"
#include "xwa/flight/hangar.h"

#include "aeron/aeron.h"
#include "xwa/flight/ai/pai.h"
#include "xwa/flight/ai/pai_plan.h"
#include "xwa/flight/ai/paifight.h"
#include "xwa/flight/ai/paiman.h"
#include "xwa/flight/ai/paiorder.h"
#include "xwa/assets/file_io.h"
#include "xwa/assets/flight_model.h"
#include "xwa/assets/string_table.h"
#include "xwa/audio/music.h"
#include "xwa/audio/sound.h"
#include "xwa/config/game_config.h"
#include "xwa/config/pilot.h"
#include "xwa/console/console.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/death_star.h"
#include "xwa/flight/object/debris.h"
#include "xwa/flight/fediskio.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/flight_light.h"
#include "xwa/flight/flight_text.h"
#include "xwa/audio/fsfx.h"
#include "xwa/flight/starfield.h"
#include "xwa/frontend/flight_loading.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/input/dinput.h"
#include "xwa/input/forcefeedback.h"
#include "xwa/math/scalar.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/yard.h"
#include "xwa/flight/film.h"
#include "xwa/flight/flight_net.h"
#include "xwa/flight/flight_sync.h"
#include "xwa/flight/net_session.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/effects.h"
#include "xwa/render/renderer.h"
#include "xwa/render/renderer_internal.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"
#include "xwa/util/random.h"
#include "xwa_runtime/timing/modern_flight_timing.h"
#include "xwa/util/time.h"
#include "xwa_runtime/timing/host_clock.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	XWA_FLIGHT_FRAME_US = 4000u,
	XWA_FLIGHT_LOADING_MIN_VISIBLE_MS = 3000u,
	XWA_FLIGHT_ARG_COUNT = 7,
	XWA_FILM_PLAYBACK_ABORTED = 3,
};

typedef enum XwaFlightTaskPhase {
	XWA_FLIGHT_TASK_PHASE_INACTIVE = 0,
	XWA_FLIGHT_TASK_PHASE_MAIN_LOOP_INIT,
	XWA_FLIGHT_TASK_PHASE_LOADING,
	XWA_FLIGHT_TASK_PHASE_MISSION_INSTANCE_INIT,
	XWA_FLIGHT_TASK_PHASE_HANGAR_READY,
	XWA_FLIGHT_TASK_PHASE_MISSION_START,
	XWA_FLIGHT_TASK_PHASE_FRAME,
	XWA_FLIGHT_TASK_PHASE_INSTANCE_CLEANUP,
	XWA_FLIGHT_TASK_PHASE_FINAL_CLEANUP,
	XWA_FLIGHT_TASK_PHASE_DONE,
} XwaFlightTaskPhase;

typedef enum XwaFlightLoadingStage {
	XWA_FLIGHT_LOADING_STAGE_STEP0 = 0,
	XWA_FLIGHT_LOADING_STAGE_MISSION_INIT,
	XWA_FLIGHT_LOADING_STAGE_VOICE,
	XWA_FLIGHT_LOADING_STAGE_RESOURCES,
	XWA_FLIGHT_LOADING_STAGE_RUNTIME_INIT,
	XWA_FLIGHT_LOADING_STAGE_DEBRIS,
	XWA_FLIGHT_LOADING_STAGE_DETACH,
} XwaFlightLoadingStage;

typedef enum XwaFlightTaskHangarContinuation {
	XWA_FLIGHT_TASK_HANGAR_CONTINUE_MISSION_START = 0,
	XWA_FLIGHT_TASK_HANGAR_CONTINUE_FLIGHT,
} XwaFlightTaskHangarContinuation;

static int g_xwaFlightTaskActive;
static int g_xwaFlightTaskComplete;
static int g_xwaFlightTaskResult;
static int g_xwaFlightTaskFilmPathWasSet;
static XwaFlightTaskPhase g_xwaFlightTaskPhase;
static XwaFlightLoadingStage g_xwaFlightTaskLoadingStage;
static uint32_t g_xwaFlightTaskLoadingStartTick;
static uint64_t g_xwaFlightTaskNextWakeUs;
static XwaFlightTaskPhase g_xwaFlightTaskLastLoggedPhase = XWA_FLIGHT_TASK_PHASE_INACTIVE;
static uint32_t g_xwaFlightTaskTickLogCount;
static int g_xwaFlightTaskLastStepTargetTimestamp;
static int g_xwaFlightTaskOverlayTargetTimestamp;
static int g_xwaFlightTaskPredictedFrameDelta;
static int g_xwaFlightTaskLastSoundUpdateTimestamp;
static int g_xwaFlightTaskRestartMission;
static int g_xwaFlightTaskMissionLoaded;
static int g_xwaFlightTaskPingPrevHostDropCount;
static int g_xwaFlightTaskPingDropScore;
static int g_xwaFlightTaskRecoveryCountdownState;
static int g_xwaFlightTaskDirectWorldCleanup;
static XwaFlightTaskHangarContinuation g_xwaFlightTaskHangarContinuation;
static int g_xwaFlightTaskPendingHangarTicks;

// GLOBAL: XWA 0x78287C
int g_flightTimestampScaleOverride;
// GLOBAL: XWA 0x7CA3A0
int g_flightTickOverlayWindowTicks;
// GLOBAL: XWA 0x7D4BC4
int g_flightTickOverlayLastLoopTicks;
// GLOBAL: XWA 0x91ACA8
int g_flightTickOverlaySampleCount;

extern uint8_t g_flightStepRanThisFrame;
extern int g_filmPlaybackTimestampOverrideApplied;
extern int g_filmPlaybackStepCatchupTicks;
extern int g_lastLocalReplayInputTimestamp;
extern uint8_t g_unusedFlightStepResetFlag;
extern int dtMs;
extern PilotData g_pilotDataSnapshot;

typedef struct XwaFlightTaskLoopTiming {
	int frameStartTimestamp;
	int preStepTimestamp;
	int networkProcessTicks;
} XwaFlightTaskLoopTiming;

static void XwaFlightTask_RequestInstanceCleanup(int restartMission);

#if !defined(NDEBUG)
static const char* XwaFlightTask_PhaseName(XwaFlightTaskPhase phase) {
	switch (phase) {
		case XWA_FLIGHT_TASK_PHASE_INACTIVE:
			return "inactive";
		case XWA_FLIGHT_TASK_PHASE_MAIN_LOOP_INIT:
			return "main-loop-init";
		case XWA_FLIGHT_TASK_PHASE_LOADING:
			return "loading";
		case XWA_FLIGHT_TASK_PHASE_MISSION_INSTANCE_INIT:
			return "mission-instance-init";
		case XWA_FLIGHT_TASK_PHASE_HANGAR_READY:
			return "hangar-ready";
		case XWA_FLIGHT_TASK_PHASE_MISSION_START:
			return "mission-start";
		case XWA_FLIGHT_TASK_PHASE_FRAME:
			return "frame";
		case XWA_FLIGHT_TASK_PHASE_INSTANCE_CLEANUP:
			return "instance-cleanup";
		case XWA_FLIGHT_TASK_PHASE_FINAL_CLEANUP:
			return "final-cleanup";
		case XWA_FLIGHT_TASK_PHASE_DONE:
			return "done";
		default:
			return "unknown";
	}
}

static void XwaFlightTask_Log(const char* event) {
	Aeron_LogTrace("xwa.flight.task", "%s: phase=%s active=%d complete=%d now=%llu next=%llu input=%d game=%d",
			  event, XwaFlightTask_PhaseName(g_xwaFlightTaskPhase), g_xwaFlightTaskActive,
			  g_xwaFlightTaskComplete, (unsigned long long)Aeron_NowUs(),
			  (unsigned long long)g_xwaFlightTaskNextWakeUs, g_inputTimestamp, g_gameTime);
}
#else
static void XwaFlightTask_Log(const char* event) {
	(void)event;
}
#endif

static void XwaFlightTask_LogPhaseEntry(void) {
	if (g_xwaFlightTaskPhase != g_xwaFlightTaskLastLoggedPhase) {
		g_xwaFlightTaskLastLoggedPhase = g_xwaFlightTaskPhase;
		XwaFlightTask_Log("phase-enter");
	}
}

static void XwaFlightTask_AdvanceLoadingStage(XwaFlightLoadingStage nextStage) {
	g_xwaFlightTaskLoadingStage = nextStage;
	g_xwaFlightTaskNextWakeUs = Aeron_NowUs() + XWA_FLIGHT_FRAME_US;
}

static int XwaFlightTask_FileExists(AeronVfsRoot root, const char* path) {
	XwaFile* file;

	file = File_Open(root, path, "rb");
	if (file == 0) {
		return 0;
	}

	File_Close(file);
	return 1;
}

static int XwaFlightTask_ParseCommandLine(char* missionCmdLine) {
	char** args[XWA_FLIGHT_ARG_COUNT];
	int cursor;
	int quoted;
	int argCount;
	int argIndex;
	char c;

	args[0] = &g_argMissionPath;
	args[1] = &sessionName;
	args[2] = &g_argPilotName;
	args[3] = &g_argLocalIdStr;
	args[4] = &g_argMpGameName;
	args[5] = &g_argUnusedZeroStr;
	args[6] = &g_argNumPlayersStr;

	g_argProgramName = "xtie";
	g_argSentinel = "/trebla";
	cursor = 0;
	quoted = 0;
	argCount = 2;
	argIndex = 0;
	if (missionCmdLine[0] != '\0') {
		while (argIndex < XWA_FLIGHT_ARG_COUNT) {
			*args[argIndex] = &missionCmdLine[cursor];
			while (1) {
				c = missionCmdLine[cursor];
				if (c == ' ' && quoted != 1) {
					break;
				}
				if (c == '\0') {
					goto token_done;
				}
				if (c == '~') {
					if (quoted) {
						missionCmdLine[cursor] = '\0';
						quoted = 0;
						++cursor;
					} else {
						quoted = 1;
						*args[argIndex] = &missionCmdLine[++cursor];
					}
				} else {
					++cursor;
				}
			}

		token_done:
			++argCount;
			++argIndex;
			if (missionCmdLine[cursor] != '\0') {
				missionCmdLine[cursor] = '\0';
				if (missionCmdLine[++cursor] != '\0') {
					continue;
				}
			}
			break;
		}
	}

	return argCount >= 9;
}

static void XwaFlightTask_ApplyFlightConfig(void) {
	int configIndex;
	int starDensity;
	int textureRes;
	int explosionRes;
	int localLights;
	double lodScale;
	double mipScale;

	configIndex = NetSession_GetPlayerCount() > 1;
	g_flightBrightnessScaleQ8 = (g_gameConfig.brightness[configIndex] + 4) << 6;
	if (g_flightBrightnessScaleQ8 < 0x100) {
		g_flightBrightnessScaleQ8 = 0x100;
	} else if (g_flightBrightnessScaleQ8 > 0x2c0) {
		g_flightBrightnessScaleQ8 = 0x2c0;
	}

	g_backdropsEnabled = g_gameConfig.backdrop[configIndex];
	g_debrisEnabled = g_gameConfig.debris[configIndex];
	starDensity = g_gameConfig.starDensity[configIndex];
	if (starDensity == 0) {
		g_starDensity = 4;
	} else if (starDensity == 1) {
		g_starDensity = 2;
	} else if (starDensity == 2) {
		g_starDensity = 1;
	}

	/* Port policy: only the hardware renderer is supported. The original software
	   fallback branches remain outside the task split until they can be removed at
	   recovered call sites. */
	g_useHardware3D = 1;
	g_bilinearEnabled = g_gameConfig.bilinear[configIndex];
	g_usePalettizedTextures = g_gameConfig.palettizedTextures[configIndex];
	g_hwMipmapFilter = std3D_SetMipmapFilter(g_gameConfig.hardwareMipmap[configIndex]);
	g_hitEffectsEnabled = g_gameConfig.hitEffects[configIndex];
	g_particleEffectsEnabled = g_gameConfig.particleEffects[configIndex];
	g_trailsEnabled = g_gameConfig.trails[configIndex];

	lodScale = (double)(g_gameConfig.lod[configIndex] + 5);
	if (lodScale > 20.0) {
		lodScale = 20.0;
	}
	lodScale = lodScale * 0.039999999 + lodScale * 0.039999999;
	if (lodScale > 1.0) {
		lodScale = 1.0 / (2.0 - lodScale);
	}
	g_forcedLodLevel = 0;
	g_lodDistanceScale = (float)(1.0 / lodScale);

	mipScale = (double)g_gameConfig.mipmap[configIndex] * 0.052631579 +
			   (double)g_gameConfig.mipmap[configIndex] * 0.052631579;
	if (mipScale > 1.0) {
		mipScale = 1.0 / (2.0 - mipScale);
	}
	g_mipLodScale = (float)(1.0 / mipScale);

	textureRes = g_gameConfig.textureRes[configIndex];
	if (textureRes == 0) {
		g_keepFullResTextures = 0;
	} else if (textureRes == 1) {
		g_keepFullResTextures = 1;
	} else {
		g_keepFullResTextures = 2;
	}

	explosionRes = g_gameConfig.explosionRes[configIndex];
	if (explosionRes == 0) {
		g_explosionResLevel = 0;
	} else if (explosionRes == 1) {
		g_explosionResLevel = 1;
	} else {
		g_explosionResLevel = 2;
	}

	localLights = g_gameConfig.localLights[configIndex];
	if (localLights == 0) {
		g_localLightsLevel = 0;
	} else if (localLights == 1) {
		g_localLightsLevel = 1;
	} else {
		g_localLightsLevel = 2;
	}
	g_specularEnabled = g_gameConfig.specular[configIndex] != 0;
	g_dirLightingEnabled = g_gameConfig.diffuse[configIndex] != 0;
}

static void XwaFlightTask_ApplyFlightResolution(void) {
	g_flightResolutionMode = g_gameConfig.screenRes[NetSession_GetPlayerCount() > 1];
	switch (g_flightResolutionMode) {
		case FLIGHT_RES_800x600:
			width = 800;
			height = 600;
			break;
		case FLIGHT_RES_1024x768:
			width = 1024;
			height = 768;
			break;
		case FLIGHT_RES_1152x864:
			width = 1152;
			height = 864;
			break;
		case FLIGHT_RES_1280x1024:
			width = 1280;
			height = 1024;
			break;
		case FLIGHT_RES_1600x1200:
			width = 1600;
			height = 1200;
			break;
		default:
			width = 640;
			height = 480;
			g_flightResolutionMode = FLIGHT_RES_640x480;
			break;
	}

	g_surfaceWidth = width;
	g_surfaceHeight = height;
	g_screenWidth = width;
	g_screenHeight = height;
	g_renderTargetWidth = width;
	g_unusedFlightDisplayBytesPerPixelMirror = g_flight16bppBytesPerPixel;
	g_unusedFlightDisplayHardware3DMirror = g_useHardware3D;
}

static void XwaFlightTask_StoreActualConfigAfterDisplayInit(void) {
	int configIndex;

	configIndex = NetSession_GetPlayerCount() > 1;
	g_gameConfig.screenRes[configIndex] = (uint8_t)g_flightResolutionMode;
	g_gameConfig.use3dHardware[configIndex] = (uint8_t)g_useHardware3D;
	g_gameConfig.hitEffects[configIndex] = (uint8_t)g_hitEffectsEnabled;
}

static int XwaFlightTask_LoadFilmHeader(const char* filmFilePath) {
	int16_t ignored;

	if (filmFilePath == 0 || filmFilePath[0] == '\0') {
		return 1;
	}

	g_filmFile = File_Open(AERON_VFS_ROOT_USER, filmFilePath, "rb");
	if (g_filmFile == 0) {
		g_filmPlaybackMode = 0;
		return 0;
	}

	g_filmPlaybackMode = 1;
	if (!Film_ReadBytes(&ignored, sizeof(ignored)) || !Film_ReadBytes(&ignored, sizeof(ignored)) ||
		!Film_ReadBytes(&g_filmVersion, sizeof(g_filmVersion)) ||
		!Film_ReadBytes(g_currentMissionFile, sizeof(g_currentMissionFile))) {
		g_filmPlaybackMode = 0;
		File_Close(g_filmFile);
		g_filmFile = 0;
		return 0;
	}

	if (g_filmPlaybackMode == XWA_FILM_PLAYBACK_ABORTED) {
		g_filmPlaybackMode = 0;
		File_Close(g_filmFile);
		g_filmFile = 0;
		return 0;
	}

	Film_SeekPastHeaderAndMissionName();
	if (!Film_ReadBytes(&g_pilotData, sizeof(g_pilotData)) ||
		g_filmPlaybackMode == XWA_FILM_PLAYBACK_ABORTED) {
		g_filmPlaybackMode = 0;
		File_Close(g_filmFile);
		g_filmFile = 0;
		return 0;
	}

	g_cockpitObjectTypeForFilmHeader = g_pilotData.networkPlayers[0].craftId;
	if ((uint16_t)g_filmVersion <= 4u) {
		g_filmHeaderDifficulty = 1;
		g_filmHeaderCollisionsEnabled = 1;
	} else {
		Film_ReadBytes(&g_filmHeaderDifficulty, sizeof(g_filmHeaderDifficulty));
		Film_ReadBytes(&g_filmHeaderCollisionsEnabled, sizeof(g_filmHeaderCollisionsEnabled));
	}
	return 1;
}

static void XwaFlightTask_ResetPlayersForMissionStart(int playerCount) {
	memset(g_players, 0, sizeof(g_players));
	memset(g_flightNetWorldChecksumPeerStatus, 0, sizeof(g_flightNetWorldChecksumPeerStatus));
	for (int playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
		g_playerConnected[playerIdx] = 1;
		g_players[playerIdx].connectedFlag = playerIdx < playerCount ? 1 : 0;
		g_players[playerIdx].impactDamageCooldownTime = 0;
	}
	memset(g_inputFrameCount, 0, sizeof(g_inputFrameCount));
}

static void XwaFlightTask_ResetPlayersForMissionRestart(int playerCount) {
	for (int playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
		uint8_t cockpitLookAvailable;
		uint32_t networkTailWord;

		cockpitLookAvailable = g_players[playerIdx].cockpitLookAvailable;
		memcpy(&networkTailWord, ((uint8_t*)&g_players[playerIdx]) + 2967, sizeof(networkTailWord));
		memset(&g_players[playerIdx], 0, sizeof(g_players[playerIdx]));
		g_players[playerIdx].pendingActionTimer = 0;
		g_players[playerIdx].hasCheckpointFlag = 0;
		g_players[playerIdx].cockpitLookAvailable = cockpitLookAvailable;
		memcpy(((uint8_t*)&g_players[playerIdx]) + 2967, &networkTailWord, sizeof(networkTailWord));
		if (cockpitLookAvailable) {
			g_players[playerIdx].cockpitVisible = 1;
		}
		g_players[playerIdx].connectedFlag = playerIdx < playerCount ? 1 : 0;
		g_players[playerIdx].impactDamageCooldownTime = 0;
	}

	memset(g_flightNetWorldChecksumPeerStatus, 0, sizeof(g_flightNetWorldChecksumPeerStatus));
	for (int playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
		g_playerConnected[playerIdx] = 1;
	}
	memset(g_inputFrameCount, 0, sizeof(g_inputFrameCount));
}

static void XwaFlightTask_ResetLocalLightPulses(void) {
	for (int pulseIdx = 0; pulseIdx < 6; ++pulseIdx) {
		g_localPlayerLightPulses[pulseIdx].enabled = 0;
	}
}

static int XwaFlightTask_ShouldSkipHangarReady(void) {
	if (g_flightPlayerCount > 1) {
		return 1;
	}
	if (g_pilotData.campaignMode) {
		int missionDesc;

		missionDesc = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
		return missionDesc == 50 || missionDesc == 51;
	}
	return !g_provingGroundsModeActive && g_missionHeader.body.hangar != XWA_HANGAR_MONCALCRUISER &&
		   g_missionHeader.body.hangar != XWA_HANGAR_DEATHSTAR &&
		   g_missionHeader.body.hangar != XWA_HANGAR_FAMILYTRANSPORT;
}

static void XwaFlightTask_InitProjectionAndPalette(void) {
	RgbTriplet palette[256];

	g_screenHeight = height;
	g_screenWidth = width;
	g_surfacePitch = FlightDisplay_GetPrimarySurfacePitch();
	perspShift = 9;
	g_projAspectY = 0;
	switch (g_flightResolutionMode) {
		case FLIGHT_RES_800x600:
			g_flightHudScaleFactor = 1.0f;
			g_flightSwPointSpriteScale = 1.25f;
			g_projScaleInt = 640;
			g_projScaleHalfInt = 320;
			break;
		case FLIGHT_RES_1024x768:
			g_flightHudScaleFactor = 1.28f;
			g_flightSwPointSpriteScale = 1.6f;
			g_projScaleInt = 819;
			g_projScaleHalfInt = 409;
			break;
		case FLIGHT_RES_1152x864:
			g_flightHudScaleFactor = 1.4400001f;
			g_flightSwPointSpriteScale = 2.0f;
			g_projScaleInt = 922;
			g_projScaleHalfInt = 461;
			break;
		case FLIGHT_RES_1280x1024:
			g_flightHudScaleFactor = 1.8f;
			g_flightSwPointSpriteScale = 2.0f;
			g_projScaleInt = 960;
			g_projScaleHalfInt = 480;
			break;
		case FLIGHT_RES_1600x1200:
			g_flightHudScaleFactor = 2.0f;
			g_flightSwPointSpriteScale = 2.5f;
			g_projScaleInt = 1280;
			g_projScaleHalfInt = 640;
			break;
		case FLIGHT_RES_640x480:
		default:
			g_flightHudScaleFactor = 1.0f;
			g_flightSwPointSpriteScale = 1.0f;
			g_projScaleInt = 512;
			g_projScaleHalfInt = 256;
			break;
	}
	g_projScale = (float)(unsigned int)g_projScaleInt;
	g_projScaleDiv512 = g_projScale * (1.0f / 512.0f);

	FlightSurface_Lock();
	FlightSw_InitLineBuffer();
	FlightSurface_Unlock();
	DebugPrintf("");
	FlightDisplay_Flip();
	FlightRender_InstallCallbacks(3u);
	FeDiskIo_ReadAllBytesOrFatal("newpal.act", palette);
	for (int colorIdx = 0; colorIdx < 128; ++colorIdx) {
		RgbTriplet low;
		RgbTriplet high;
		int highIdx;

		highIdx = 255 - colorIdx;
		low = palette[colorIdx];
		high = palette[highIdx];
		palette[colorIdx].r = (uint8_t)(high.r >> 2);
		palette[colorIdx].g = (uint8_t)(high.g >> 2);
		palette[colorIdx].b = (uint8_t)(high.b >> 2);
		palette[highIdx].r = (uint8_t)(low.r >> 2);
		palette[highIdx].g = (uint8_t)(low.g >> 2);
		palette[highIdx].b = (uint8_t)(low.b >> 2);
	}
	g_flightSetPaletteRangeFn(palette, 0, 0x100u);
	FlightPalette_Reset();
}

static int XwaFlightTask_WaitForFrameStepDue(void) {
	if (g_filmPlaybackMode) {
		return 1;
	}
	g_inputTimestamp += (int)Time_GetFrameDelta();
	return g_inputTimestamp - g_gameTime >= XwaModernFlightTiming_StepTicks();
}

static int XwaFlightTask_SmoothedStepTarget(int desiredTimestamp, int currentGameTime) {
	int target;

	if (g_xwaFlightTaskLastStepTargetTimestamp != 0) {
		target = g_xwaFlightTaskLastStepTargetTimestamp + g_xwaFlightTaskPredictedFrameDelta;
		if (target < desiredTimestamp) {
			int delta;

			delta = desiredTimestamp - target;
			if (delta > (g_xwaFlightTaskPredictedFrameDelta >> 3)) {
				delta = g_xwaFlightTaskPredictedFrameDelta >> 3;
			}
			if (delta == 0) {
				delta = 1;
			}
			if (delta > 4) {
				delta = desiredTimestamp - target;
			}
			target += delta;
		} else if (target > desiredTimestamp) {
			int delta;

			delta = target - desiredTimestamp;
			if (delta > (g_xwaFlightTaskPredictedFrameDelta >> 3)) {
				delta = g_xwaFlightTaskPredictedFrameDelta >> 3;
			}
			if (delta == 0) {
				delta = 1;
			}
			if (delta > 4) {
				delta = target - desiredTimestamp;
			}
			target -= delta;
		}
	} else {
		target = desiredTimestamp;
	}

	if (XwaModernFlightTiming_StepTicks() != 1 && target - currentGameTime < 4) {
		target = currentGameTime + 4;
	}
	return target;
}

static void XwaFlightTask_UpdateNetworkIndicators(void) {
	int clockLead;

	clockLead = g_inputTimestamp - g_serverTickTime - g_flightNetClockLeadAllowanceMs;
	if (clockLead >= 472) {
		g_lagIndicator = clockLead >= 944 ? (clockLead >= 1416) + 2 : 1;
	} else {
		g_lagIndicator = 0;
	}

	if (g_xwaFlightTaskPingPrevHostDropCount != 0) {
		int hostDplayId;
		int hostDropCount;

		hostDplayId = NetSession_GetHostDplayId();
		hostDropCount = NetReliable_GetPeerPacketDropCountByDpid(hostDplayId);
		g_xwaFlightTaskPingDropScore += 10 * (hostDropCount - g_xwaFlightTaskPingPrevHostDropCount);
		if (g_xwaFlightTaskPingDropScore != 0) {
			g_pingIndicator =
				g_xwaFlightTaskPingDropScore >= 10 ? (g_xwaFlightTaskPingDropScore >= 20) + 2 : 1;
		} else {
			g_pingIndicator = 0;
		}
		g_xwaFlightTaskPingPrevHostDropCount = hostDropCount;
		if (g_xwaFlightTaskPingDropScore != 0) {
			--g_xwaFlightTaskPingDropScore;
		}
	} else {
		g_pingIndicator = 0;
	}
}

static void XwaFlightTask_RenderFrameAndAudio(const XwaFlightTaskLoopTiming* timing) {
	int renderStartTimestamp;
	int renderTicks;
	int loopTicks;
	int updateTicks;

	if (FlightDisplay_IsFrontendModalActive()) {
		return;
	}

	g_inputTimestamp += (int)Time_GetFrameDelta();
	updateTicks = g_inputTimestamp - timing->preStepTimestamp;
	g_inputTimestamp += (int)Time_GetFrameDelta();
	XwaFlightTask_UpdateNetworkIndicators();
	renderStartTimestamp = g_inputTimestamp;
	FlightSync_ApplyRemotePlayerRenderSmoothing();
	if (g_flightPlayerCount > 1) {
		g_flightSfxSideEffectGate = 1;
	}
	if (g_inHangarReady) {
		Hangar_RenderReadyScreen();
	} else {
		FlightView_RenderFrame();
	}
	g_flightSfxSideEffectGate = 0;
	Sound_FlushQueuedEffects();
	if (g_inputTimestamp - g_xwaFlightTaskLastSoundUpdateTimestamp > 29) {
		g_xwaFlightTaskLastSoundUpdateTimestamp = g_inputTimestamp;
		Sound_Update3DListenerAndSources();
	}
	FlightSync_CaptureRemotePlayerRenderSamples();
	g_inputTimestamp += (int)Time_GetFrameDelta();
	renderTicks = g_inputTimestamp - renderStartTimestamp;
	loopTicks = g_inputTimestamp - timing->frameStartTimestamp;
	if (loopTicks == 0) {
		loopTicks = 1;
	}
	if (g_flightTimestampScaleOverride) {
		g_inputTimestamp += g_flightTimestampScaleOverride * loopTicks;
	}
	if (g_flightConfTickCounter) {
		char overlay[160];

		if (g_flightTickOverlayWindowTicks > 944) {
			g_flightTickOverlaySampleCount = 0;
			g_flightTickOverlayWindowTicks = 0;
		}
		g_flightTickOverlayWindowTicks += loopTicks;
		g_flightTickOverlayLastLoopTicks = loopTicks;
		++g_flightTickOverlaySampleCount;
		sprintf(overlay, "R:%-2d U:%-2d N:%-2d O:%-2d T:%-2d FR:%-2d NOW:%-7dL:%-7dS:%-7dW:%-3dD:%-3dA%d\n",
				renderTicks, updateTicks, timing->networkProcessTicks,
				loopTicks - renderTicks - updateTicks - timing->networkProcessTicks, loopTicks,
				236 / loopTicks, g_inputTimestamp, g_gameTime, g_serverTickTime,
				g_inputTimestamp - g_serverTickTime, g_flightNetClockLeadAllowanceMs,
				g_flightNetClockAdjustAccumTicks);
		sprintf(overlay, "lT:%-6d LlT:%-6d LlD:%-6d Target:%-6d\n", g_gameTime,
				g_xwaFlightTaskLastStepTargetTimestamp, g_xwaFlightTaskPredictedFrameDelta,
				g_xwaFlightTaskOverlayTargetTimestamp);
		(void)overlay;
	} else {
		g_flightTickOverlaySampleCount = 0;
		g_flightTickOverlayWindowTicks = 0;
	}
}

static void XwaFlightTask_EnterHangarReady(XwaFlightTaskHangarContinuation continuation) {
	g_xwaFlightTaskHangarContinuation = continuation;
	g_xwaFlightTaskPendingHangarTicks = 0;
	g_xwaFlightTaskPhase = XWA_FLIGHT_TASK_PHASE_HANGAR_READY;
}

static int XwaFlightTask_EnterRequestedHangarReady(XwaFlightTaskHangarContinuation continuation) {
	if (!Hangar_TakeReadyLoopRequest()) {
		return 0;
	}

	XwaFlightTask_EnterHangarReady(continuation);
	return 1;
}

static int XwaFlightTask_UpdateHangarFromFlightFrame(int elapsedTicks) {
	int stepTicks;
	int advanceTicks;

	if (!g_inHangarReady) {
		g_xwaFlightTaskPendingHangarTicks = 0;
		return 0;
	}
	if (elapsedTicks > 0) {
		g_xwaFlightTaskPendingHangarTicks += elapsedTicks;
	}

	stepTicks = XwaModernFlightTiming_HangarStepTicks();
	if (g_xwaFlightTaskPendingHangarTicks < stepTicks) {
		return 0;
	}

	advanceTicks = g_xwaFlightTaskPendingHangarTicks - g_xwaFlightTaskPendingHangarTicks % stepTicks;
	g_xwaFlightTaskPendingHangarTicks -= advanceTicks;
	return Hangar_UpdateLaunch(advanceTicks);
}

static void XwaFlightTask_RunHangarReadyFrame(void) {
	int startGameTime;
	int elapsed;
	int off;
	int playerIdx;

	g_inputTimestamp += (int)Time_GetFrameDelta();
	if (g_inputTimestamp - g_gameTime < XwaModernFlightTiming_HangarStepTicks()) {
		return;
	}

	startGameTime = g_gameTime;
	elapsed = g_inputTimestamp - startGameTime;
	g_elapsedTicks = (uint16_t)elapsed;
	XwaModernFlightTiming_BeginAdvance((uint16_t)elapsed);

	for (off = 0; off < (int)sizeof(PlayerFlightTransientTimers); off += 2) {
		for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
			if (g_players[playerIdx].connectedFlag) {
				uint16_t* timer;
				int16_t nextValue;

				timer = (uint16_t*)((char*)&g_playerFlightTransientTimers[playerIdx] + off);
				nextValue = (int16_t)(*timer - elapsed);
				if (*timer) {
					*timer = (uint16_t)nextValue;
					if (nextValue < 0) {
						*timer = 0;
					}
				}
			}
		}
	}

	{
		int16_t clockWord;

		clockWord = g_missionElapsedClock.subsecondTicks;
		clockWord -= (int16_t)elapsed;
		g_missionElapsedClock.subsecondTicks = clockWord;
		if (clockWord <= 0) {
			clockWord += 236;
			g_missionElapsedClock.subsecondTicks = clockWord;
			Hud_AdvanceFlightMessagePaneTimers();
		}
	}

	{
		int fire;

		fire = (g_flightGlobalCountdownTimers[2] == 0);
		if (!fire) {
			int16_t nextValue;

			nextValue = (int16_t)(g_flightGlobalCountdownTimers[2] - elapsed);
			g_flightGlobalCountdownTimers[2] = (uint16_t)nextValue;
			if (nextValue < 0) {
				g_flightGlobalCountdownTimers[2] = 0;
			}
			fire = (g_flightGlobalCountdownTimers[2] == 0);
		}
		if (fire) {
			g_flightGlobalCountdownTimers[2] = 15;
			if (g_flightPlayerCount == 1 && g_players[g_localPlayer].viewState.externalCameraActive) {
				FlightObject_AnimateCrewMeshRotations((uint16_t)g_players[g_localPlayer].objectIndex, 0);
			}
		}
	}

	Hud_UpdateFlightMessagePanes();
	Flight_UpdateDynamicMusicState();
	Hangar_UpdateLaunch(elapsed);
	if (FlightDisplay_IsFrontendModalActive()) {
		return;
	}
	g_gameTime = g_inputTimestamp;
	Sound_FlushQueuedEffects();
	if (g_inputTimestamp - g_xwaFlightTaskLastSoundUpdateTimestamp > 29) {
		g_xwaFlightTaskLastSoundUpdateTimestamp = g_inputTimestamp;
		Sound_Update3DListenerAndSources();
	}
	if (g_launchAnimDone || g_flightExitRequest || !g_inHangarReady) {
		if (g_xwaFlightTaskHangarContinuation == XWA_FLIGHT_TASK_HANGAR_CONTINUE_MISSION_START) {
			g_xwaFlightTaskPhase = XWA_FLIGHT_TASK_PHASE_MISSION_START;
		} else if (g_flightExitRequest) {
			XwaFlightTask_RequestInstanceCleanup(0);
		} else {
			g_xwaFlightTaskPhase = XWA_FLIGHT_TASK_PHASE_FRAME;
		}
		return;
	}
	Hangar_RenderReadyScreen();
	g_gameTime = g_inputTimestamp;
}

static void XwaFlightTask_RequestInstanceCleanup(int restartMission) {
	g_xwaFlightTaskRestartMission = restartMission;
	g_xwaFlightTaskPhase = XWA_FLIGHT_TASK_PHASE_INSTANCE_CLEANUP;
}

static void XwaFlightTask_MarkComplete(void) {
	g_xwaFlightTaskPhase = XWA_FLIGHT_TASK_PHASE_DONE;
	g_xwaFlightTaskComplete = 1;
}

static void XwaFlightTask_ReturnFromMissionEntryFailure(int shutdownSoundEngine) {
	if (shutdownSoundEngine) {
		Sound_Shutdown_Sound_Engine();
	}
	FeDiskIo_FreeModelResources();
	memcpy(&g_localPlayerSnapshotOnFlightExit, &g_players[g_localPlayer],
		   sizeof(g_localPlayerSnapshotOnFlightExit));
	Backdrop_FreeCoordinateBuffers();
	Console_FreeMacros();
	FlightStarfield_Shutdown();
	XwaFlightTask_MarkComplete();
}

static int XwaFlightTask_RunSinglePlayerFrame(void) {
	int targetTimestamp;
	int realInputTimestamp;
	int missionEndPending;
	XwaFlightTaskLoopTiming timing;

	g_inputTimestamp += (int)Time_GetFrameDelta();
	timing.frameStartTimestamp = g_inputTimestamp;
	if (!XwaFlightTask_WaitForFrameStepDue()) {
		return 1;
	}

	timing.preStepTimestamp = g_inputTimestamp;
	FlightNet_ProcessIncomingPackets();
	if (!NetSession_GetLocalPlayerId() && g_flightNetHostTimeoutElapsedMs > 7080) {
		int localPlayer;

		localPlayer = g_localPlayer;
		g_players[localPlayer].connectedFlag = 0;
		FlightNet_MarkPilotNetworkPlayerLeft(localPlayer);
		XwaFlightTask_RequestInstanceCleanup(0);
		return 1;
	}
	g_inputTimestamp += (int)Time_GetFrameDelta();
	timing.networkProcessTicks = g_inputTimestamp - timing.preStepTimestamp;
	missionEndPending = Flight_CheckMissionEndAndExitRequest();
	if (XwaFlightTask_EnterRequestedHangarReady(XWA_FLIGHT_TASK_HANGAR_CONTINUE_FLIGHT)) {
		return 1;
	}
	if (missionEndPending) {
		if (g_gameTime == g_serverTickTime) {
			if (g_messageLogFileWriteRequested) {
				msg_writeMessageLogFile();
				g_messageLogFileWriteRequested = 0;
			}
			XwaFlightTask_RequestInstanceCleanup(0);
			return 1;
		}
		if (g_inputTimestamp - g_gameTime > 1180) {
			XwaFlightTask_RequestInstanceCleanup(0);
			return 1;
		}
		XwaFlightTask_WaitForFrameStepDue();
		return 1;
	}
	if (g_flightExitRequest) {
		XwaFlightTask_RequestInstanceCleanup(0);
		return 1;
	}

	timing.preStepTimestamp = g_inputTimestamp;
	g_inputTimestamp += (int)Time_GetFrameDelta();
	targetTimestamp = XwaFlightTask_SmoothedStepTarget(g_inputTimestamp, g_gameTime);
	g_xwaFlightTaskOverlayTargetTimestamp = targetTimestamp;
	realInputTimestamp = g_inputTimestamp;
	g_inputTimestamp = targetTimestamp;
	g_xwaFlightTaskPredictedFrameDelta = targetTimestamp - g_xwaFlightTaskLastStepTargetTimestamp;
	FlightNet_SampleAndSendInput();
	g_flightSimSideEffectsSuppressed = 0;
	dtMs = g_inputTimestamp - g_gameTime;
	Flight_StepSimToTime(g_inputTimestamp);
	XwaFlightTask_EnterRequestedHangarReady(XWA_FLIGHT_TASK_HANGAR_CONTINUE_FLIGHT);
	if (!g_filmPlaybackMode || g_flightStepRanThisFrame) {
		if (g_inHangarReady) {
			if (XwaFlightTask_UpdateHangarFromFlightFrame(dtMs)) {
				XwaFlightTask_RequestInstanceCleanup(0);
				return 1;
			}
		} else {
			g_xwaFlightTaskPendingHangarTicks = 0;
			if (g_provingGroundsModeActive && !g_flightMissionEndPending) {
				Yard_UpdateChallengeTick(dtMs);
			}
		}
		if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR) {
			DeathStarTunnel_Update();
		}
		g_flightStepRanThisFrame = 0;
	}
	if (g_filmPlaybackTimestampOverrideApplied) {
		targetTimestamp = g_inputTimestamp;
		realInputTimestamp = g_inputTimestamp;
		g_xwaFlightTaskOverlayTargetTimestamp = g_inputTimestamp;
		g_xwaFlightTaskPredictedFrameDelta = g_gameTime - g_xwaFlightTaskLastStepTargetTimestamp;
	}
	g_gameTime = g_inputTimestamp;
	g_serverTickTime = g_inputTimestamp;
	g_xwaFlightTaskLastStepTargetTimestamp = g_inputTimestamp;
	g_inputTimestamp = realInputTimestamp + g_inputTimestamp - targetTimestamp;
	XwaFlightTask_RenderFrameAndAudio(&timing);
	return 1;
}

static int XwaFlightTask_RunMultiplayerAheadRecovery(void) {
	int stillLoadingPulseTicks;
	int entryFrameDelta;
	int recovered;
	char line[256];

	FlightNet_BroadcastPlayerDisconnected(g_localPlayer);
	entryFrameDelta = (int)Time_GetFrameDelta();
	g_flightNetHostTimeoutElapsedMs = 0;
	stillLoadingPulseTicks = 0;
	g_inputTimestamp += entryFrameDelta;
	FlightAlert_SaveBoxBackground();
	strcpy(line, g_strDiskIoMessages[24]);
	{
		char* statusPlayerName;

		statusPlayerName = FlightNet_GetStatusPlayerName();
		if (statusPlayerName != NULL) {
			strcat(line, statusPlayerName);
		}
	}
	FlightAlert_DrawBox(1, line, NULL, 0x34u);
	recovered = g_inputTimestamp - g_serverTickTime <= g_flightNetClockLeadAllowanceMs;

	while (g_inputTimestamp - g_serverTickTime > g_flightNetClockLeadAllowanceMs) {
		int savedTimestamp;
		int elapsed;
		int timeoutBucket;

		if (FlightInput_HasKeyReady() && FlightInput_GetNextKey() == 27) {
			break;
		}

		savedTimestamp = g_inputTimestamp;
		FlightNet_ProcessIncomingPackets();
		g_inputTimestamp += (int)Time_GetFrameDelta();
		elapsed = g_inputTimestamp - savedTimestamp;
		stillLoadingPulseTicks += elapsed;
		g_flightNetHostTimeoutElapsedMs += elapsed;
		g_inputTimestamp = savedTimestamp;

		if (g_flightNetHostTimeoutElapsedMs > 7080) {
			break;
		}
		if (stillLoadingPulseTicks > 236) {
			FlightNet_SendStillLoadingPulse();
			stillLoadingPulseTicks = 0;
		}

		timeoutBucket = (7080 - g_flightNetHostTimeoutElapsedMs) / 118;
		if (timeoutBucket != g_xwaFlightTaskRecoveryCountdownState) {
			g_xwaFlightTaskRecoveryCountdownState = timeoutBucket;
			if (timeoutBucket < 50) {
				sprintf(line, g_strDiskIoMessages[31], timeoutBucket / 2, 5 * (timeoutBucket & 1));
				FlightAlert_DrawBox(3, line, NULL, 0x34u);
			} else if ((timeoutBucket & 1) != 0) {
				FlightAlert_DrawBox(3, g_strDiskIoMessages[30], NULL, 0x34u);
			} else {
				FlightAlert_DrawBox(3, g_strDiskIoMessages[26], NULL, 0x34u);
			}
		}

		if ((Flight_CheckMissionEndAndExitRequest() && g_gameTime == g_serverTickTime) ||
			g_inputTimestamp - g_serverTickTime <= g_flightNetClockLeadAllowanceMs) {
			recovered = 1;
			break;
		}
	}

	if (recovered || g_inputTimestamp - g_serverTickTime <= g_flightNetClockLeadAllowanceMs) {
		FlightAlert_RestoreBoxBackground();
		Time_GetFrameDelta();
		if (Flight_CheckMissionEndAndExitRequest() && g_gameTime == g_serverTickTime) {
			XwaFlightTask_RequestInstanceCleanup(0);
			return 0;
		}
		g_inputTimestamp = g_serverTickTime + g_flightNetClockLeadAllowanceMs;
		return 1;
	}

	g_flightMissionEndPending = 1;
	FlightNet_BroadcastPlayerAbort(g_localPlayer);
	g_playerAbortFlags[g_localPlayer] = 1;
	g_players[g_localPlayer].connectedFlag = 0;
	FlightNet_MarkPilotNetworkPlayerLeft(g_localPlayer);
	XwaFlightTask_RequestInstanceCleanup(0);
	return 0;
}

static int XwaFlightTask_RunMultiplayerFrame(void) {
	int targetTimestamp;
	int realInputTimestamp;
	int serverLeadTarget;
	int clockDelta;
	int missionEndPending;
	XwaFlightTaskLoopTiming timing;

	g_inputTimestamp += (int)Time_GetFrameDelta();
	timing.frameStartTimestamp = g_inputTimestamp;
	if (!XwaFlightTask_WaitForFrameStepDue()) {
		return 1;
	}
	timing.preStepTimestamp = g_inputTimestamp;
	FlightNet_ProcessIncomingPackets();
	if (!NetSession_GetLocalPlayerId() && g_flightNetHostTimeoutElapsedMs > 7080) {
		int localPlayer;

		localPlayer = g_localPlayer;
		g_players[localPlayer].connectedFlag = 0;
		FlightNet_MarkPilotNetworkPlayerLeft(localPlayer);
		XwaFlightTask_RequestInstanceCleanup(0);
		return 1;
	}
	g_inputTimestamp += (int)Time_GetFrameDelta();
	timing.networkProcessTicks = g_inputTimestamp - timing.preStepTimestamp;
	missionEndPending = Flight_CheckMissionEndAndExitRequest();
	if (XwaFlightTask_EnterRequestedHangarReady(XWA_FLIGHT_TASK_HANGAR_CONTINUE_FLIGHT)) {
		return 1;
	}
	if (missionEndPending) {
		if (g_gameTime == g_serverTickTime) {
			if (g_messageLogFileWriteRequested) {
				msg_writeMessageLogFile();
				g_messageLogFileWriteRequested = 0;
			}
			XwaFlightTask_RequestInstanceCleanup(0);
			return 1;
		}
		if (g_inputTimestamp - g_gameTime > 1180) {
			XwaFlightTask_RequestInstanceCleanup(0);
			return 1;
		}
		XwaFlightTask_WaitForFrameStepDue();
		return 1;
	}
	if (g_flightExitRequest) {
		XwaFlightTask_RequestInstanceCleanup(0);
		return 1;
	}

	timing.preStepTimestamp = g_inputTimestamp;
	g_inputTimestamp += (int)Time_GetFrameDelta();
	serverLeadTarget = g_serverTickTime + g_flightNetClockLeadAllowanceMs;
	if (g_inputTimestamp > serverLeadTarget) {
		clockDelta = (g_inputTimestamp - serverLeadTarget) >> 4;
		if (clockDelta == 0) {
			clockDelta = 1;
		}
		if (clockDelta > (g_xwaFlightTaskPredictedFrameDelta >> 3)) {
			clockDelta = g_xwaFlightTaskPredictedFrameDelta >> 3;
		}
		g_inputTimestamp -= clockDelta;
		g_flightNetClockAdjustAccumTicks += clockDelta;
	} else if (g_inputTimestamp < serverLeadTarget) {
		clockDelta = (serverLeadTarget - g_inputTimestamp) >> 4;
		if (clockDelta == 0) {
			clockDelta = 1;
		}
		if (clockDelta > (g_xwaFlightTaskPredictedFrameDelta >> 3)) {
			clockDelta = g_xwaFlightTaskPredictedFrameDelta >> 3;
		}
		g_inputTimestamp += clockDelta;
		g_flightNetClockAdjustAccumTicks -= clockDelta;
	}

	if (g_inputTimestamp < g_serverTickTime) {
		clockDelta = serverLeadTarget - g_inputTimestamp;
		g_inputTimestamp = serverLeadTarget;
		g_flightNetClockAdjustAccumTicks -= clockDelta;
	}

	if (g_inputTimestamp - g_serverTickTime > g_flightNetClockLeadAllowanceMs + 1652 &&
		NetSession_GetLocalPlayerId() == 0) {
		if (!XwaFlightTask_RunMultiplayerAheadRecovery()) {
			return 1;
		}
	}

	if (g_inputTimestamp <= g_gameTime) {
		XwaFlightTask_RenderFrameAndAudio(&timing);
		return 1;
	}

	targetTimestamp = XwaFlightTask_SmoothedStepTarget(g_inputTimestamp, g_gameTime);
	g_xwaFlightTaskOverlayTargetTimestamp = targetTimestamp;
	realInputTimestamp = g_inputTimestamp;
	g_inputTimestamp = targetTimestamp;
	g_xwaFlightTaskPredictedFrameDelta = targetTimestamp - g_xwaFlightTaskLastStepTargetTimestamp;
	FlightNet_SampleAndSendInput();
	FlightSync_QueuePredictedRemoteInputFrames(g_xwaFlightTaskPredictedFrameDelta);
	g_flightSimSideEffectsSuppressed = 1;
	Flight_StepSimToTime(g_inputTimestamp);
	XwaFlightTask_EnterRequestedHangarReady(XWA_FLIGHT_TASK_HANGAR_CONTINUE_FLIGHT);
	if (g_inHangarReady) {
		XwaFlightTask_UpdateHangarFromFlightFrame(g_inputTimestamp - g_gameTime);
	} else {
		g_xwaFlightTaskPendingHangarTicks = 0;
	}
	g_gameTime = g_inputTimestamp;
	g_xwaFlightTaskLastStepTargetTimestamp = g_inputTimestamp;
	g_inputTimestamp = realInputTimestamp + g_inputTimestamp - targetTimestamp;
	XwaFlightTask_RenderFrameAndAudio(&timing);
	return 1;
}

int XwaFlightTask_Init(char* missionCmdLine, const char* filmFilePath) {
	int numHumanPlayers;
	int localId;
	int configIndex;

	g_xwaFlightTaskActive = 0;
	g_xwaFlightTaskComplete = 0;
	g_xwaFlightTaskResult = 0;
	g_xwaFlightTaskPhase = XWA_FLIGHT_TASK_PHASE_INACTIVE;
	g_xwaFlightTaskLoadingStage = XWA_FLIGHT_LOADING_STAGE_STEP0;
	g_xwaFlightTaskLoadingStartTick = 0;
	g_xwaFlightTaskLastLoggedPhase = XWA_FLIGHT_TASK_PHASE_INACTIVE;
	g_xwaFlightTaskTickLogCount = 0;
	g_xwaFlightTaskFilmPathWasSet = filmFilePath != 0 && filmFilePath[0] != '\0';
	g_xwaFlightTaskLastStepTargetTimestamp = 0;
	g_xwaFlightTaskOverlayTargetTimestamp = 0;
	g_xwaFlightTaskPredictedFrameDelta = 0;
	g_xwaFlightTaskLastSoundUpdateTimestamp = 0;
	g_xwaFlightTaskRestartMission = 0;
	g_xwaFlightTaskMissionLoaded = 0;
	g_xwaFlightTaskPingPrevHostDropCount = 0;
	g_xwaFlightTaskPingDropScore = 0;
	g_xwaFlightTaskRecoveryCountdownState = 0;
	g_xwaFlightTaskDirectWorldCleanup = 0;
	g_xwaFlightTaskHangarContinuation = XWA_FLIGHT_TASK_HANGAR_CONTINUE_MISSION_START;
	g_xwaFlightTaskPendingHangarTicks = 0;
	(void)Hangar_TakeReadyLoopRequest();

	DebugPrintf("Entering Flight_Main:");
	g_flightRenderToFrontend = 0;
	if (missionCmdLine == 0) {
		return 0;
	}

	g_filmPlaybackMode = 0;
	g_filmRecording = 0;
	Config_Load();
	g_flightConfFlicker = XwaFlightTask_FileExists(AERON_VFS_ROOT_ASSET, "flicker.txt") ? 0 : 1;
	g_FlightConfRivaTxt = XwaFlightTask_FileExists(AERON_VFS_ROOT_ASSET, "riva.txt") ? 1 : 0;
	g_flightConfPowerVr = XwaFlightTask_FileExists(AERON_VFS_ROOT_ASSET, "powervr.txt") ? 1 : 0;

	g_asyncFlag = g_gameConfig.asyncFlag;
	g_flightConfTrainCourse = strstr(missionCmdLine, "traincourse") != 0;
	g_flightConfNoPilot = strstr(missionCmdLine, "nopilot") != 0;
	g_flightConfDirectInput = strstr(missionCmdLine, "nodinput") != 0 ? 0 : 1;
	g_flightConfSfxEnabled = strstr(missionCmdLine, "nosfx") != 0 ? 0 : 1;
	g_flightConfMusicEnabled = strstr(missionCmdLine, "nomusic") != 0 ? 0 : 1;
	g_flightConfVoiceEnabled = strstr(missionCmdLine, "novoice") != 0 ? 0 : 1;
	g_flightConfTickCounter =
		strstr(missionCmdLine, "notickcounter") != 0 ? 0 : strstr(missionCmdLine, "tickcounter") != 0;
	inProgressLaunch = strstr(missionCmdLine, "inprogress") != 0;
	g_flightConfNewNet = strstr(missionCmdLine, "newnet") != 0;
	g_flightConfNoLauncher = strstr(missionCmdLine, "nolauncher") != 0;
	if (strstr(missionCmdLine, "nofullscreen") != 0) {
		g_flightFullscreen = 0;
	} else if (strstr(missionCmdLine, "fullscreen") != 0) {
		g_flightFullscreen = 1;
	}
	if (strstr(missionCmdLine, "nopageflip") != 0) {
		g_flightPageFlip = 0;
	} else if (strstr(missionCmdLine, "pageflip") != 0) {
		g_flightPageFlip = 1;
	}
	if (missionCmdLine[0] == '-') {
		g_flightStartedWithDashArg = 1;
	} else if (missionCmdLine[0] == '/' && missionCmdLine[1] == '+') {
		g_unusedFlightCmdLinePlusSwitchFlag = 1;
	}

	if (!XwaFlightTask_ParseCommandLine(missionCmdLine)) {
		return 0;
	}

	numHumanPlayers = atoi(g_argNumPlayersStr);
	localId = atoi(g_argLocalIdStr);
	if (!NetSession_InitGameSession(sessionName, g_argPilotName, localId, g_argMpGameName,
									(const char*)(uintptr_t)g_gameConfig.networkType, numHumanPlayers,
									inProgressLaunch)) {
		NetSession_Shutdown();
		return 0;
	}

	XwaFlightTask_ApplyFlightConfig();
	XwaFlightTask_ApplyFlightResolution();
	if (!FlightDisplay_Init()) {
		NetSession_Shutdown();
		return 0;
	}
	XwaFlightTask_StoreActualConfigAfterDisplayInit();
	if (g_flightConfPowerVr) {
		FlightText_SetHardwareGlyphDepth(0.000001f);
	} else {
		FlightText_SetHardwareGlyphDepth(1.0f);
	}
	if (g_useHardware3D) {
		g_bilinearEnabled = Renderer_CanUseBilinearFiltering() && g_bilinearEnabled;
		if (!g_bilinearEnabled && !g_explosionResLevel) {
			g_explosionResLevel = 1;
		}
	}
	Renderer_InitD3DRenderStatePresets();

	DebugPrintf("Init Dinput\n");
	if (g_flightConfDirectInput && !DInput_Init()) {
		g_flightConfDirectInput = 0;
	}
	DebugPrintf("Init Dsound\n");
	g_flightSoundInitStartTimeMs = (int)XwaTime_GetElapsedTicks();
	Sound_Init_Sound_Engine(0);
	strcpy(g_currentMissionFile, g_argMissionPath);
	g_filmPlaybackMode = 0;
	if (!XwaFlightTask_LoadFilmHeader(filmFilePath)) {
		return 0;
	}

	configIndex = NetSession_GetPlayerCount() > 1;
	(void)configIndex;
	g_xwaFlightTaskActive = 1;
	g_xwaFlightTaskPhase = XWA_FLIGHT_TASK_PHASE_MAIN_LOOP_INIT;
	g_xwaFlightTaskNextWakeUs = Aeron_NowUs();
	g_xwaFlightTaskResult = 1;
	XwaFlightTask_Log("init-complete");
	return 1;
}

void XwaFlightTask_Tick(void) {
	if (!g_xwaFlightTaskActive || g_xwaFlightTaskComplete) {
		if (g_xwaFlightTaskTickLogCount < 8u) {
			++g_xwaFlightTaskTickLogCount;
			XwaFlightTask_Log("tick-inactive-or-complete");
		}
		return;
	}
	/* Modal continuations discard their elapsed host time; do not simulate on the completion frame. */
	if (Flight_ContinueOptionsModal() || Hangar_ContinueOptionsModal()) {
		g_xwaFlightTaskNextWakeUs = Aeron_NowUs() + XWA_FLIGHT_FRAME_US;
		return;
	}

	if (Aeron_NowUs() < g_xwaFlightTaskNextWakeUs) {
		if (g_xwaFlightTaskTickLogCount < 8u) {
			++g_xwaFlightTaskTickLogCount;
			XwaFlightTask_Log("tick-before-wake");
		}
		return;
	}
	if (g_xwaFlightTaskTickLogCount < 8u) {
		++g_xwaFlightTaskTickLogCount;
		XwaFlightTask_Log("tick-run");
	}
	g_xwaFlightTaskNextWakeUs += XWA_FLIGHT_FRAME_US;

	while (!g_xwaFlightTaskComplete) {
		XwaFlightTask_LogPhaseEntry();
		switch (g_xwaFlightTaskPhase) {
			case XWA_FLIGHT_TASK_PHASE_MAIN_LOOP_INIT: {
				int playerCount;

				Math_SetFpuSinglePrecisionMode();
				Flight_PumpWindowMessages();
				memset(g_playerAbortFlags, 0, sizeof(g_playerAbortFlags));
				g_pingIndicator = 0;
				g_lagIndicator = 0;
				g_flightReturnToMissionSetupRequested = 1;
				g_pauseState = 0;
				g_filmOverlayActive = 0;
				g_unusedFlightStepResetFlag = 0;
				g_filmPlaybackTimestampOverrideApplied = 0;
				g_flightStepRanThisFrame = 0;
				g_sw3dSkipOddScanlines = 0;
				g_flightNetHostAbortReceived = 0;
				fsfx_ClearSfxNameTable();
				FlightSync_ResetRemotePlayerRenderSmoothing();
				Time_ResetFrameDeltaClocks();
				g_flightDisplaySurfacesActive = 1;
				g_flightSimSideEffectsSuppressed = 0;
				g_flightReturnToFrontendRequested = 0;
				g_collideUpdateCollisionObjLink = 0;
				g_localPlayer = NetSession_FindPlayerSlotByDpid(NetSession_GetLocalDplayId());
				playerCount = NetSession_GetPlayerCount();
				memset(g_replayInputs, 0, sizeof(g_replayInputs));
				memset(&g_currentInputFrame, 0, sizeof(g_currentInputFrame));
				g_activeFlightPlayerCount = playerCount;
				g_flightPlayerCount = playerCount;
				g_remotePlayerRenderSmoothingEnabled = g_asyncFlag;
				g_connectedPlayerCount = playerCount;
				g_maxConnectedPlayerCountThisMission = playerCount;
				if (g_filmPlaybackMode) {
					g_flightDifficulty = g_filmHeaderDifficulty;
					g_flightCollisionsEnabled = g_filmHeaderCollisionsEnabled;
				} else if (g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
					g_flightDifficulty = 1;
					g_flightCollisionsEnabled = g_gameConfig.collisions;
				} else if (!g_pilotData.campaignMode) {
					g_flightDifficulty = g_gameConfig.difficulty;
					g_flightCollisionsEnabled = g_gameConfig.collisions;
				} else {
					g_flightDifficulty = g_gameConfig.tourDifficulty;
					g_flightCollisionsEnabled = g_gameConfig.tourCollisions;
				}
				g_flightCraftJumpingEnabled = g_gameConfig.craftJumping;
				g_provingGroundsModeActive = g_pilotData.missionDirectoryId == MISSION_DIRECTORY_MELEE;
				g_missionRandomVariationEnabled = 0;
				g_flightLocatePlayersEnabled = g_gameConfig.locatePlayers;
				g_playerFlightGroupWaveMode = 1;
				g_missionTimeLimitActive = 0xffu;
				g_teamVictoryTimeLimitMinutes = (unsigned int)g_pilotData.numHumanPlayersLastMission <= 1u
													? 1
													: g_gameConfig.lastTeamTimeLimit;
				g_aiOpponentsEnabled = g_provingGroundsModeActive ? g_gameConfig.aiOpponents : 1;
				g_craftImpactBounceEnabled = 1;
				Math_SeedRandom(g_gameConfig.randomSeed);
				GameRand_SetSavedSeed(0xACEDu);
				GameRand_SetSecondarySeed((int16_t)(GameRand_GetPrimarySeed() + timeGetTime()));
				XwaFlightTask_ResetPlayersForMissionStart(playerCount);
				g_singleObjectUpdateOverrideIdx = -1;
				g_flightNetBufferWorldMessagesUntilChecksum = 0;
				g_flightNetWorldChecksumEpoch = 0;
				if (!g_flightConfNoPilot) {
					Mission_SyncPilotNetworkPlayersToSessionSlots();
				}
				pai_loadplans("paiplan");
				pai_cacheBuiltinPlanIds();
				XwaFlightTask_InitProjectionAndPalette();
				g_messageLogWriteIndex = 0xffffu;
				g_messageLogTotalCount = 0;
				g_flightSystemMessagesEnabled = 1;
				FeDiskIo_InitGlobalBuffers();
				FlightLoading_ShowInitialProgressScreen(1);
				g_xwaFlightTaskLoadingStartTick = XwaTime_GetElapsedTicks();
				g_xwaFlightTaskPhase = XWA_FLIGHT_TASK_PHASE_LOADING;
				XwaFlightTask_AdvanceLoadingStage(XWA_FLIGHT_LOADING_STAGE_STEP0);
				return;
			}

			case XWA_FLIGHT_TASK_PHASE_LOADING:
				switch (g_xwaFlightTaskLoadingStage) {
					case XWA_FLIGHT_LOADING_STAGE_STEP0:
						FlightInput_ResetRuntimeState();
						FlightLoading_PulseAndDrawProgressScreen(0);
						XwaFlightTask_AdvanceLoadingStage(XWA_FLIGHT_LOADING_STAGE_MISSION_INIT);
						return;

					case XWA_FLIGHT_LOADING_STAGE_MISSION_INIT:
						DebugPrintf("LOADING MISSION:%s", g_currentMissionFile);
						if ((uint16_t)Mission_Init(g_currentMissionFile) == 0) {
							XwaFlightTask_Log("mission-init-failed");
							XwaFlightTask_MarkComplete();
							return;
						}
						g_xwaFlightTaskMissionLoaded = 1;
						FeDiskIo_LoadFlightSfxBanks();
						FlightLoading_PulseAndDrawProgressScreen(1);
						XwaFlightTask_AdvanceLoadingStage(XWA_FLIGHT_LOADING_STAGE_VOICE);
						return;

					case XWA_FLIGHT_LOADING_STAGE_VOICE:
						fsfx_LoadMissionVoiceSfx();
						FlightLoading_PulseAndDrawProgressScreen(2);
						XwaFlightTask_AdvanceLoadingStage(XWA_FLIGHT_LOADING_STAGE_RESOURCES);
						return;

					case XWA_FLIGHT_LOADING_STAGE_RESOURCES:
						FeDiskIo_InitResources();
						XwaFlightTask_AdvanceLoadingStage(XWA_FLIGHT_LOADING_STAGE_RUNTIME_INIT);
						return;

					case XWA_FLIGHT_LOADING_STAGE_RUNTIME_INIT: {
						if (g_useHardware3D) {
							FlightText_BuildWidthTables();
							Math_SetFpuSinglePrecisionMode();
						}
						FlightLight_InitLocalPlayerPulses();
						Math_SetFpuSinglePrecisionMode();
						FlightSurface_Lock();
						Mission_InitFlightRuntimeState();
						FlightSurface_Unlock();
						FlightStarfield_Init();
						Backdrop_BuildCoordinateBuffers();
						Hud_InitHUD();
						Console_RunAutoexec();
						for (int playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
							if (g_players[playerIdx].cockpitLookAvailable) {
								g_players[playerIdx].cockpitVisible = 1;
							}
						}
						FlightLoading_ShowInitialProgressScreen(0);
						FlightLoading_PulseAndDrawProgressScreen(7);
						XwaFlightTask_AdvanceLoadingStage(XWA_FLIGHT_LOADING_STAGE_DEBRIS);
						return;
					}

					case XWA_FLIGHT_LOADING_STAGE_DEBRIS:
						if (!Debris_InitRubbleModelTables()) {
							DebugPrintf("  *** UNABLE TO SETUP  STARSHIP DEBRIS ***\n");
							DebugPrintf("*** A RUBBLE MODEL MAY BE BAD OR MISSING ***\n");
						}
						g_currentQuadTexCoords = g_defaultQuadTexCoords;
						g_glowMarkScratchNormalVec = &g_glowMarkPlaneScratch.normal;
						g_glowMarkScratchUAxisRefVec = &g_glowMarkPlaneScratch.uAxis;
						FlightLoading_PulseAndDrawProgressScreen(8);
						XwaFlightTask_AdvanceLoadingStage(XWA_FLIGHT_LOADING_STAGE_DETACH);
						return;

					case XWA_FLIGHT_LOADING_STAGE_DETACH:
					default:
						if (XwaTime_GetElapsedTicks() - g_xwaFlightTaskLoadingStartTick <
							XWA_FLIGHT_LOADING_MIN_VISIBLE_MS) {
							g_xwaFlightTaskNextWakeUs = Aeron_NowUs() + XWA_FLIGHT_FRAME_US;
							return;
						}
						FlightLoading_DetachFrontendSurfaces();
						XwaFlightTask_Log("loading-detached");
						FlightSurface_ClearToBlack();
						FlightDisplay_Flip();
						g_hangarInitialReadyEntryPending = 1;
						g_exteriorModel = 0;
						g_cockpitModel = 0;
						g_xwaFlightTaskPhase = XWA_FLIGHT_TASK_PHASE_MISSION_INSTANCE_INIT;
						continue;
				}

			case XWA_FLIGHT_TASK_PHASE_MISSION_INSTANCE_INIT: {
				int skipHangarReady;

				g_flightExitRequest = 0;
				g_collideSweepAllowUnownedTargets = 0;
				g_collideRicochetDamageScale = 1.0f;
				RenderScene_ResetDepthProjectionScale();
				fsfx_ResetFlightSfxState(0);
				g_loadingModel = 1;
				FeDiskIo_LoadCockpitModel();
				FeDiskIo_LoadExteriorModel();
				g_loadingModel = 0;
				g_inHangarReady = 0;
				skipHangarReady = XwaFlightTask_ShouldSkipHangarReady();
				if (g_useHardware3D) {
					if (g_objRenderState != NULL) {
						Memory_FreeTagged("OBJECT3D", g_objRenderState);
					}
					g_objRenderState = (ObjectRenderState*)Memory_AllocTagged(
						"OBJECT3D", sizeof(ObjectRenderState) * (size_t)g_regionObjectSlotEnd);
					if (g_objRenderState != NULL) {
						memset(g_objRenderState, 0,
							   sizeof(ObjectRenderState) * (size_t)g_regionObjectSlotEnd);
					}
					GlowMark_ShutdownFrameScalesAndPools();
					RenderBatch_FreeDataPoolsThunk();
					Particle_InitEffectTemplates();
					GlowMark_InitFrameScalesAndPools();
					RenderBatch_AllocMeshPassBatches();
					FlightText_DetectStretchBug();
					RenderScene_End3D();
				}
				XwaFlightTask_ResetLocalLightPulses();
				Math_SetFpuSinglePrecisionMode();
				if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR) {
					skipHangarReady = 1;
					g_debrisEnabled = 0;
					DeathStar_Init();
				}
				g_hangarBackdropModelType = 0;
				if (g_provingGroundsModeActive) {
					if (g_pilotData.meleeMissionIndex == -1) {
						g_yardChallengeMode = g_pilotData.provingGroundsMissionPerPlayer[0];
					} else {
						g_yardChallengeMode = (uint8_t)g_pilotData.meleeMissionIndex;
						skipHangarReady = 1;
					}
				}
				XwaModernFlightTiming_BeginSession(g_flightPlayerCount);
				Flight_ModernResetHighRateIntegration();
				/* Original flight starts from a zeroed mission clock before
				   hangar entry captures its camera delay timestamps. */
				g_gameTime = 0;
				g_serverTickTime = 0;
				g_inputTimestamp = 0;
				g_hangarSceneRegionIdx = g_missionRegionCount - 1;
				Time_GetFrameDelta();
				if (g_flightPlayerCount != 1 || g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR ||
					g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) {
					Hangar_ClearSceneObjectCount();
				} else {
					Hangar_SetupReadyScene();
				}
				if (skipHangarReady || g_filmPlaybackMode) {
					if (g_provingGroundsModeActive) {
						Yard_InitChallengeScene();
					}
				} else {
					if (Hangar_BeginEnterCraft(0xffffu)) {
						g_flightDisplaySurfacesActive = 0;
						Sound_FlushQueuedEffects();
						Sound_StopAllInstances();
						XwaFlightTask_Log("hangar-begin-enter-failed");
						XwaFlightTask_ReturnFromMissionEntryFailure(1);
						return;
					}
					(void)Hangar_TakeReadyLoopRequest();
					XwaFlightTask_EnterHangarReady(XWA_FLIGHT_TASK_HANGAR_CONTINUE_MISSION_START);
					XwaFlightTask_Log("enter-hangar-ready");
					return;
				}
				g_xwaFlightTaskPhase = XWA_FLIGHT_TASK_PHASE_MISSION_START;
				continue;
			}

			case XWA_FLIGHT_TASK_PHASE_HANGAR_READY:
				XwaFlightTask_RunHangarReadyFrame();
				return;

			case XWA_FLIGHT_TASK_PHASE_MISSION_START:
				if (g_inputTimestamp == 0) {
					g_inputTimestamp += (int)Time_GetFrameDelta();
					if (g_inputTimestamp == 0) {
						XwaFlightTask_Log("mission-start-wait-frame-delta");
						return;
					}
				}
				if (!FlightNet_SyncPlayerOptionsAndTaunts()) {
					g_flightDisplaySurfacesActive = 0;
					Sound_StopAllInstances();
					XwaFlightTask_Log("mission-start-sync-failed");
					XwaFlightTask_ReturnFromMissionEntryFailure(0);
					return;
				}
				/* Software rotated-sprite viewport reset omitted with the software renderer path. */
				if (g_useHardware3D) {
					Renderer_FlushTextureCacheAndReturnTrue();
				}
				for (uint32_t objectIdx = 0; objectIdx < g_regionObjectSlotEnd; ++objectIdx) {
					if (g_objectTable[objectIdx].mobj != NULL) {
						g_objectTable[objectIdx].mobj->simStateTimestamp = 0;
					}
				}
				if (g_flightPlayerCount <= 1) {
					FlightSync_ResetWorldMessageBufferCursor();
				} else {
					Flight_AllocWorldStateBuffers();
					FlightSync_ResetWorldMessageBufferCursor();
					Flight_SaveWorldState();
				}
				ForceFeedback_EnableEffects();
				Flight_UpdateFighterWarnings(1);
				g_renderFlags = 31;
				FlightView_RenderStartupFrame();
				XwaFlightTask_Log("startup-frame-rendered");
				NetSession_StubReturnTrue();
				if (!FlightNet_WaitForMissionStart()) {
					g_xwaFlightTaskDirectWorldCleanup = 1;
					XwaFlightTask_Log("wait-for-mission-start-failed");
					XwaFlightTask_RequestInstanceCleanup(0);
					continue;
				}
				g_flightReturnToMissionSetupRequested = 0;
				DebugPrintf("Entering FlyWarpingNetworkMission:");
				if (g_filmPlaybackMode) {
					g_filmPlaybackTimestampOverrideApplied = 1;
				}
				g_filmPlaybackStepCatchupTicks = 0;
				g_xwaFlightTaskLastSoundUpdateTimestamp = 0;
				g_xwaFlightTaskLastStepTargetTimestamp = 0;
				g_xwaFlightTaskOverlayTargetTimestamp = 0;
				g_lastLocalReplayInputTimestamp = 0;
				g_flightSfxSideEffectGate = 0;
				g_xwaFlightTaskPredictedFrameDelta = 0;
				g_xwaFlightTaskPingPrevHostDropCount = 0;
				g_xwaFlightTaskPingDropScore = 0;
				g_xwaFlightTaskRecoveryCountdownState = 0;
				g_xwaFlightTaskPhase = XWA_FLIGHT_TASK_PHASE_FRAME;
				return;

			case XWA_FLIGHT_TASK_PHASE_FRAME:
				if (g_flightPlayerCount == 1) {
					XwaFlightTask_RunSinglePlayerFrame();
					return;
				}
				XwaFlightTask_RunMultiplayerFrame();
				return;

			case XWA_FLIGHT_TASK_PHASE_INSTANCE_CLEANUP: {
				int restartMission;
				int directWorldCleanup;

				XwaModernFlightTiming_EndSession();
				directWorldCleanup = g_xwaFlightTaskDirectWorldCleanup;
				g_xwaFlightTaskDirectWorldCleanup = 0;
				restartMission = g_xwaFlightTaskRestartMission;
				if (!directWorldCleanup && g_flightExitRequest == 2) {
					restartMission = !g_provingGroundsModeActive || g_pilotData.meleeMissionIndex == -1;
				}
				if (!directWorldCleanup) {
					ForceFeedback_StopAllEffects();
					ForceFeedback_EnableEffects();
				}
				if (directWorldCleanup || g_flightPlayerCount > 1) {
					Flight_FreeWorldStateBuffers();
				}
				Sound_StopAllInstances();
				if (!g_filmPlaybackMode && g_filmRecording) {
					Music_PauseIfInitialized();
					Film_FlushWriteBuffer();
					if (g_filmFile != NULL) {
						File_Close(g_filmFile);
						g_filmFile = NULL;
					}
					g_filmRecording = 0;
					FlightFilm_SaveTempRecordingWithPrompt();
					Music_ResumeIfInitialized();
				}
				if (!g_flightReturnToMissionSetupRequested && !g_filmPlaybackMode) {
					FeDiskIo_CommitFlightResults();
				}
				g_flightDisplaySurfacesActive = 0;
				if (g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR) {
					DeathStar_Shutdown();
				}
				if (g_useHardware3D) {
					if (g_objRenderState != NULL) {
						Memory_FreeTagged("OBJECT3D", g_objRenderState);
						g_objRenderState = NULL;
					}
					GlowMark_ShutdownFrameScalesAndPools();
					RenderBatch_FreeDataPoolsThunk();
				}
				Backdrop_FreeCoordinateBuffers();
				FlightStarfield_Shutdown();
				if (restartMission || g_filmPlaybackMode == 3 || g_filmPlaybackMode == 2) {
					int playerCount;

					g_flightDisplaySurfacesActive = 1;
					memset(g_playerAbortFlags, 0, sizeof(g_playerAbortFlags));
					FlightSync_ResetRemotePlayerRenderSmoothing();
					Time_ResetFrameDeltaClocks();
					g_localPlayer = NetSession_FindPlayerSlotByDpid(NetSession_GetLocalDplayId());
					playerCount = NetSession_GetPlayerCount();
					g_activeFlightPlayerCount = playerCount;
					g_flightPlayerCount = playerCount;
					XwaFlightTask_ResetPlayersForMissionRestart(playerCount);
					g_missionTimeLimitActive = 0xffu;
					DebugPrintf("Reloading mission:%s", g_currentMissionFile);
					FlightSurface_Lock();
					Mission_Init(g_currentMissionFile);
					FlightSurface_Unlock();
					if (g_provingGroundsModeActive && g_yardSelectedCraftType) {
						g_missionFlightGroups[g_pilotData.networkPlayers[0].flightGroupId].fg.craftType =
							g_yardSelectedCraftType;
					}
					FlightSurface_Lock();
					Mission_InitFlightRuntimeState();
					FlightSurface_Unlock();
					XwaFlightTask_ResetLocalLightPulses();
					FlightStarfield_Init();
					Backdrop_BuildCoordinateBuffers();
					Hud_FreeHUDResources();
					Hud_ResetHudRuntimeState();
					Hud_InitHUD();
					Hud_ResetFlightMessagePanes(1);
					if (g_filmPlaybackMode) {
						Music_SetState(MUSIC_STATE_NONE);
						restartMission = 1;
						Film_SeekPastHeaderAndMissionName();
						Film_ReadBytes(&g_pilotData, sizeof(g_pilotData));
						g_filmOverlayActive = 0;
						g_pauseState = 0;
						g_filmPlaybackMode = 1;
					} else {
						int campaignPreservedKillStat;

						memset(g_pilotData.killsFullOnPlayer, 0, sizeof(g_pilotData.killsFullOnPlayer));
						memset(g_pilotData.killsSharedOnPlayer, 0, sizeof(g_pilotData.killsSharedOnPlayer));
						memset(g_pilotData.killsFullOnFlightGroup, 0,
							   sizeof(g_pilotData.killsFullOnFlightGroup));
						memset(g_pilotData.killsSharedOnFlightGroup, 0,
							   sizeof(g_pilotData.killsSharedOnFlightGroup));
						memset(g_pilotData.killsFullFromPlayer, 0, sizeof(g_pilotData.killsFullFromPlayer));
						memset(g_pilotData.killsSharedFromPlayer, 0,
							   sizeof(g_pilotData.killsSharedFromPlayer));
						memset(g_pilotData.killsFullFromFlightGroup, 0,
							   sizeof(g_pilotData.killsFullFromFlightGroup));
						memset(g_pilotData.killsSharedFromFlightGroup, 0,
							   sizeof(g_pilotData.killsSharedFromFlightGroup));
						memset(&g_pilotData.objectStats, 0, sizeof(g_pilotData.objectStats));
						memset(g_pilotData.teamsStatistics, 0, sizeof(g_pilotData.teamsStatistics));
						for (int playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
							g_pilotData.networkPlayers[playerIdx].totalScore = 0;
							g_pilotData.networkPlayers[playerIdx].kills = 0;
							g_pilotData.networkPlayers[playerIdx].killsShared = 0;
							g_pilotData.networkPlayers[playerIdx].m38 = 0;
							g_pilotData.networkPlayers[playerIdx].killsAssist = 0;
							g_pilotData.networkPlayers[playerIdx].totalLosses = 0;
							g_pilotData.networkPlayers[playerIdx].m60 = 0;
						}
						if (g_pilotData.campaignMode) {
							campaignPreservedKillStat =
								g_pilotData.factionStatistics[1]
									.stats
									.killsPerCraftPerMT[1][12 * g_pilotData.missionDescriptionIds[4] + 29];
							memcpy(&g_pilotData, &g_pilotDataSnapshot, sizeof(g_pilotData));
							g_pilotData.factionStatistics[1]
								.stats.killsPerCraftPerMT[1][12 * g_pilotData.missionDescriptionIds[4] + 29] =
								campaignPreservedKillStat;
						}
					}
				}
				if (restartMission) {
					g_xwaFlightTaskRestartMission = 0;
					g_xwaFlightTaskPhase = XWA_FLIGHT_TASK_PHASE_MISSION_INSTANCE_INIT;
					continue;
				}
				g_xwaFlightTaskPhase = XWA_FLIGHT_TASK_PHASE_FINAL_CLEANUP;
				continue;
			}

			case XWA_FLIGHT_TASK_PHASE_FINAL_CLEANUP:
				if (g_filmPlaybackMode && g_filmFile != NULL) {
					File_Close(g_filmFile);
					g_filmFile = NULL;
				}
				g_flightDebugPrintFn("");
				FeDiskIo_FreeModelResources();
				Hud_FreeHUDResources();
				Console_FreeMacros();
				Hud_ResetHudRuntimeState();
				Pilot_Save(0);
				ForceFeedback_ShutdownDevice();
				XwaFlightTask_MarkComplete();
				return;

			case XWA_FLIGHT_TASK_PHASE_DONE:
			case XWA_FLIGHT_TASK_PHASE_INACTIVE:
			default:
				XwaFlightTask_MarkComplete();
				return;
		}
	}
}

int XwaFlightTask_Shutdown(void) {
	XwaModernFlightTiming_EndSession();
	if (!g_xwaFlightTaskActive && !g_xwaFlightTaskComplete) {
		return g_xwaFlightTaskResult;
	}

	g_sw3dSkipOddScanlines = 0;
	Sound_Shutdown_Sound_Engine();
	if (g_flightConfDirectInput) {
		DInput_Shutdown();
	}
	NetSession_Shutdown();
	if (g_useHardware3D) {
		D3DInfo_ReleaseAll();
		std3D_Close(0);
		std3D_Shutdown();
	}
	FlightDisplay_FreeSurfaces();

	g_useHardware3D = 0;
	g_flightRenderToFrontend = 1;
	if (g_xwaFlightTaskFilmPathWasSet) {
		g_xwaFlightTaskResult = 0;
	} else if (g_flightReturnToFrontendRequested) {
		DebugPrintf("Exiting Flight_Main:");
		DebugPrintf("Exiting Flight_Main:");
		g_xwaFlightTaskResult = 2;
	} else if (g_flightReturnToMissionSetupRequested) {
		DebugPrintf("Exiting Flight_Main:");
		DebugPrintf("Exiting Flight_Main:");
		g_xwaFlightTaskResult = 3;
	} else if (g_provingGroundsModeActive && g_pilotData.missionDirectoryId == 1 &&
			   g_pilotData.missionDescriptionIds[1] == 66) {
		DebugPrintf("Exiting Flight_Main:");
		DebugPrintf("Exiting Flight_Main:");
		g_xwaFlightTaskResult = 0;
	} else {
		DebugPrintf("Exiting Flight_Main:");
		DebugPrintf("Exiting Flight_Main:");
		g_xwaFlightTaskResult = 1;
	}

	if (g_filmFile != 0) {
		File_Close(g_filmFile);
		g_filmFile = 0;
	}
	g_xwaFlightTaskActive = 0;
	g_xwaFlightTaskComplete = 0;
	return g_xwaFlightTaskResult;
}

int XwaFlightTask_IsActive(void) { return g_xwaFlightTaskActive != 0; }

int XwaFlightTask_IsComplete(void) { return g_xwaFlightTaskComplete != 0; }

int XwaFlightTask_GetResult(void) { return g_xwaFlightTaskResult; }

uint64_t XwaFlightTask_NextWakeDeadlineUs(void) { return g_xwaFlightTaskNextWakeUs; }
