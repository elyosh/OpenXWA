#include "xwa/audio/music.h"

#include "aeron/log.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/audio/imuse/imhost.h"
#include "xwa/audio/imuse/imuse.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/player/player.h"
#include "xwa/util/debug.h"
#include "xwa/util/time.h"

static int Music_NoOp(void) { return 0; }

// GLOBAL: XWA 0xAE2A44
int musicState;
// GLOBAL: XWA 0x694068
char g_musicInitialized;
// GLOBAL: XWA 0x9B6318
int g_selectedMusicState;
// GLOBAL: XWA 0x9B631C
int g_currentMusicState;
// GLOBAL: XWA 0x69406C
char g_musicCombatSeen;
// GLOBAL: XWA 0x693840
ImApiTable g_imApiTable = {
	0,          0, 0, 0, 0, 0, /* init, reserved, startup, shutdown, saveGame, restoreGame */
	Music_NoOp,                /* pause */
	Music_NoOp,                /* resume */
};
// GLOBAL: XWA 0x694060
int g_lastMusicSeq;
// GLOBAL: XWA 0x694064
int g_musicSeqCooldown;
// GLOBAL: XWA 0x694070
int g_setMusicState;

// GLOBAL: XWA 0x6937D0
ImHostServices g_imHostServices;

// FUNCTION: XWA 0x49A490
int Music_Init(void* directSound) {
	int result;

	if (g_musicInitialized) {
		DebugPrintf("InitMusic called when music already initialized");
	}
	g_musicInitialized = 0;
	if (directSound == NULL) {
		return 0;
	}

	/* iMUSE host-services callback table. Stream-based callbacks are cast to the
	   struct's void* stream signatures (the original casts identically). */
	g_imHostServices.version = 0x436C0000;
	g_imHostServices.printStatus = ImHost_PrintStatus;
	g_imHostServices.printMessage = ImHost_PrintMessage;
	g_imHostServices.printWarning = ImHost_PrintWarning;
	g_imHostServices.printError = ImHost_PrintError;
	g_imHostServices.printDebug = ImHost_PrintDebug;
	g_imHostServices.assertFail = ImHost_AssertFail;
	g_imHostServices.registerAtExit = ImHost_RegisterAtExit;
	g_imHostServices.allocMem = ImHost_AllocMem;
	g_imHostServices.freeMem = ImHost_FreeMem;
	g_imHostServices.reallocMem = ImHost_ReallocMem;
	g_imHostServices.getTime = ImHost_GetTime;
	g_imHostServices.openFile = (void* (*)(const char*, const char*))ImHost_OpenFile;
	g_imHostServices.closeFile = (int (*)(void*))ImHost_CloseFile;
	g_imHostServices.readFile = (unsigned int (*)(void*, void*, unsigned int))ImHost_ReadFile;
	g_imHostServices.readLine = (char* (*)(void*, char*, int))ImHost_ReadLine;
	g_imHostServices.writeFile = (int (*)(void*, void*, unsigned int))ImHost_WriteFile;
	g_imHostServices.atEof = (int (*)(void*))ImHost_AtEof;
	g_imHostServices.tellFile = (int (*)(void*))ImHost_TellFile;
	g_imHostServices.seekFile = (int (*)(void*, int, int))ImHost_SeekFile;
	g_imHostServices.getFileSize = ImHost_GetFileSize;
	g_imHostServices.filePrintf = (int (*)(void*, const char*, ...))ImHost_FilePrintf;
	g_imHostServices.allocHandle = ImHost_AllocHandle;
	g_imHostServices.freeHandle = ImHost_FreeHandle;
	g_imHostServices.reallocHandle = ImHost_ReallocHandle;
	g_imHostServices.lockHandle = ImHost_LockHandle;
	g_imHostServices.unlockHandle = ImHost_UnlockHandle;

	result = ImInit(&g_imHostServices, &g_imApiTable);
	if (result != 0) {
#ifdef XWA_MODERN
		Aeron_LogError("xwa.music", "Music_Init: ImInit failed");
#endif
		return 0;
	}
	ImSetDirectSoundDevice(directSound);
	result = g_imApiTable.startup();
	if (result != 0) {
#ifdef XWA_MODERN
		Aeron_LogError("xwa.music", "Music_Init: ImStartup failed");
#endif
		return 0;
	}

	g_musicCombatSeen = 0;
	g_currentMusicState = -1;
	g_selectedMusicState = -1;
	g_lastMusicSeq = 0;
	g_musicSeqCooldown = 0;
	g_setMusicState = MUSIC_STATE_NONE;
	g_musicInitialized = 1;
#ifdef XWA_MODERN
	Aeron_LogInfo("xwa.music", "Music_Init: OK");
#endif
	return 1;
}

// FUNCTION: XWA 0x49ADF0
void Music_Shutdown(void) {
	if (g_musicInitialized) {
		ImSetState(0);
		g_imApiTable.shutdown();
		g_musicInitialized = 0;
	}
}

// FUNCTION: XWA 0x49AA40
int Music_SetState(int state) {
	if (!g_musicInitialized) {
		return 0;
	}

	switch (state) {
		case MUSIC_STATE_NONE:
			return ImSetState(MUSIC_STATE_NONE);

		case MUSIC_STATE_DEFAULT_ALIAS:
			return ImSetState(MUSIC_STATE_NO_ENEMIES_CALM);

		case MUSIC_STATE_NO_ENEMIES_INTRO:
		case MUSIC_STATE_NO_ENEMIES_INTRO_ALT:
		case MUSIC_STATE_NO_ENEMIES_CALM:
		case MUSIC_STATE_NO_ENEMIES_CALM_ALT:
		case MUSIC_STATE_CONFLICT:
		case MUSIC_STATE_1115:
		case MUSIC_STATE_1120:
		case MUSIC_STATE_PANIC:
		case MUSIC_STATE_PANIC_ALT:
		case MUSIC_STATE_COMBAT_STEADY:
		case MUSIC_STATE_COMBAT_STEADY_ALT:
		case MUSIC_STATE_COMBAT_ACTIVE:
		case MUSIC_STATE_COMBAT_ACTIVE_ALT:
		case MUSIC_STATE_1145:
		case MUSIC_STATE_CLIMAX:
		case MUSIC_STATE_1155:
		case MUSIC_STATE_MISSION_LOSS_ALT:
		case MUSIC_STATE_MISSION_SUCCESS:
		case MUSIC_STATE_MISSION_SUCCESS_ALT:
		case MUSIC_STATE_FRONTEND_1200:
		case MUSIC_STATE_FRONTEND_1210:
		case MUSIC_STATE_HANGAR_READY:
		case MUSIC_STATE_FRONTEND_1230:
		case MUSIC_STATE_FRONTEND_1240:
		case MUSIC_STATE_FRONTEND_1250:
		case MUSIC_STATE_FRONTEND_1260:
		case MUSIC_STATE_FRONTEND_1270:
		case MUSIC_STATE_FRONTEND_1280:
			return ImSetState(state);

		default:
			DebugPrintfChannel(256, "Invalid music state: %d.\n", state);
			return 0;
	}
}

void Music_SetDatapadState(int state) {
	musicState = state;
	Aeron_LogDebug("xwa.music", "SetDatapadState(%d) enabled=%d vol=%d", state, g_gameConfig.datapadMusicEnabled,
			  g_gameConfig.datapadMusicVolume);
	if (g_gameConfig.datapadMusicEnabled) {
		Music_SetState(state);
		Music_SetVolume(127 * g_gameConfig.datapadMusicVolume / 10);
	} else {
		Music_Stop();
	}
}

// FUNCTION: XWA 0x49ABE0
int Music_TriggerSequence(int seqId, int playerSlot, char flags) {
	if (!g_musicInitialized || playerSlot != g_players[g_localPlayer].regionIndex || g_setMusicState ||
		((flags & 1) == 0 && g_lastMusicSeq == seqId) ||
		((flags & 2) == 0 && g_musicSeqCooldown > g_gameTime)) {
		return 0;
	}

	switch (seqId) {
		case 2100:
		case 2105:
		case 2110:
		case 2115:
		case 2120:
		case 2125:
		case 2130:
		case 2135:
		case 2140:
		case 2145:
		case 2150:
		case 2155:
		case 2160:
		case 2165:
		case 2170:
		case 2175:
		case 2180:
		case 2185:
		case 2190:
		case 2195:
		case 2200:
		case 2205:
			g_musicSeqCooldown = g_gameTime + 3540;
			g_lastMusicSeq = seqId;
			DebugPrintfChannel(256, "Music sequence %d triggered.\n", seqId);
			return ImSetSequence(seqId);

		default:
			DebugPrintfChannel(256, "Invalid music sequence: %d.\n", seqId);
			return 0;
	}
}

// FUNCTION: XWA 0x49AE20
int Music_TriggerOutOfHyperspaceSequenceForObject(int objectIdx) {
	ObjectRecord* obj;
	uint16_t objectType;
	int genusId;
	uint8_t missionType;
	int regionIdx;
	uint8_t flightGroupIdx;
	uint8_t iff;

	obj = &g_objectTable[objectIdx];
	flightGroupIdx = obj->flightGroupIdx;
	regionIdx = obj->regionIdx;
	genusId = obj->genusId;
	iff = g_missionFlightGroups[flightGroupIdx].fg.iff;
	objectType = obj->objectType;

	if (iff == 1) {
		if (ModelMesh_HasFuselage(objectType) || genusId == GENUS_Fighter) {
			return Music_TriggerSequence(2145, regionIdx, 0);
		}
		return Music_TriggerSequence(2140, regionIdx, 0);
	}

	missionType = g_missionHeader.body.missionType;
	if (iff == 0) {
		if (missionType == XWA_MISSION_TYPE_FAMILY_CAMPAIGN) {
			if (ModelMesh_HasFuselage(objectType) || genusId == GENUS_Fighter) {
				return Music_TriggerSequence(2175, regionIdx, 0);
			}
			return Music_TriggerSequence(2170, regionIdx, 0);
		}

		if (ModelMesh_HasFuselage(objectType) || genusId == GENUS_Fighter) {
			return Music_TriggerSequence(2155, regionIdx, 0);
		}
		return Music_TriggerSequence(2150, regionIdx, 0);
	}

	if (missionType == XWA_MISSION_TYPE_FAMILY_CAMPAIGN) {
		if (ModelMesh_HasFuselage(objectType) || genusId == GENUS_Fighter) {
			return Music_TriggerSequence(2185, regionIdx, 0);
		}
		return Music_TriggerSequence(2180, regionIdx, 0);
	}

	if (ModelMesh_HasFuselage(objectType) || genusId == GENUS_Fighter) {
		return Music_TriggerSequence(2165, regionIdx, 0);
	}
	return Music_TriggerSequence(2160, regionIdx, 0);
}

// FUNCTION: XWA 0x49AD70
void Music_SetVolume(int volume) {
	if (g_musicInitialized) {
		ImSetVolume(3u, 127u);
		ImSetVolume(0, (unsigned int)volume);
	}
}

// FUNCTION: XWA 0x49ADA0
void Music_Stop(void) {
	if (g_musicInitialized) {
		ImSetState(0);
	}
	return;
}

// FUNCTION: XWA 0x49ADC0
void Music_PauseIfInitialized(void) {
	if (g_musicInitialized) {
		g_imApiTable.pause();
	}
}

// FUNCTION: XWA 0x49ADD0
void Music_ResumeIfInitialized(void) {
	if (g_musicInitialized) {
		g_imApiTable.resume();
	}
}

// FUNCTION: XWA 0x49ADE0
void Music_Update(void) {
	int result;

	if (!g_musicInitialized) {
		return;
	}
	ImUpdate();
}
