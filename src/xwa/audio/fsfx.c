#include "xwa/audio/fsfx.h"
#include "xwa/flight/ai/pai.h"
#include "xwa/flight/ai/pai_plan.h"
#include "xwa/flight/ai/paifight.h"
#include "xwa/flight/ai/paiman.h"
#include "xwa/flight/ai/paiorder.h"
#include "xwa/assets/file_io.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_type.h"
#include "xwa/audio/sound.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/fediskio.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/hangar.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/laser.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"
#include "xwa/util/debug.h"
#include "xwa/util/random.h"
#include "xwa/util/string.h"
#include "xwa/util/time.h"
#ifdef XWA_MODERN
#include "xwa_runtime/timing/modern_flight_timing.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef XWA_MODERN
__declspec(dllimport) void __stdcall OutputDebugStringA(const char* lpOutputString);
extern void(__stdcall* g_OutputDebugStringA)(const char* outputString);
#define FSFX_OUTPUT_DEBUG_STRING g_OutputDebugStringA
#else
static void Fsfx_OutputDebugString(const char* outputString) { DebugPrintf("%s", outputString); }
#define FSFX_OUTPUT_DEBUG_STRING Fsfx_OutputDebugString
#endif

// GLOBAL: XWA 0x7B6FF4
unsigned char g_fsfxVoiceQueueCount;
// GLOBAL: XWA 0x63D0CC
int g_fsfxCurrentVoiceSfxSlot;
// GLOBAL: XWA 0x8C1CF6
uint8_t g_fsfxLoaded;
// GLOBAL: XWA 0x9D1760
uint8_t g_fsfxCurrentVoiceSpeakerType;
// GLOBAL: XWA 0x9D1800
uint8_t g_fsfxCurrentVoiceCategory;
// GLOBAL: XWA 0x9E8F40
uint16_t g_fsfxCurrentVoiceObjectSerial;
// GLOBAL: XWA 0x9E8F42
uint8_t g_fsfxCurrentVoiceChainFlag;
// GLOBAL: XWA 0x63D0C8
int g_nextNearbyWeaponSfxScanTime;
// GLOBAL: XWA 0x63D0D8
int g_targetingToneLastSeekBeepTime;
// GLOBAL: XWA 0x63D0DC
uint8_t g_targetingToneWeaponReadyQueued;
// GLOBAL: XWA 0x63D0B0
int g_playerEngineLoopObjectType;
// GLOBAL: XWA 0x63D0E0
int g_playerEngineLoopVolume;
// GLOBAL: XWA 0x63D0E4
int g_playerEngineLoopFrequency;
// GLOBAL: XWA 0x63D0D4
int g_fsfxSmallExplosionRemainingChoices;
// GLOBAL: XWA 0x63D0A8
uint8_t g_fsfxSmallExplosionUsedFlags[8];
// GLOBAL: XWA 0x63D0B8
uint8_t g_fsfxEnemyWarheadAttackCalloutPlayed;
// GLOBAL: XWA 0x63D0BC
uint8_t g_fsfxEnemyFighterAttackCalloutPlayed;
// GLOBAL: XWA 0x63D0C0
uint16_t g_fsfxLocalPlayerTargetedProjectileFriendlyHitCount;
// GLOBAL: XWA 0x63D0C4
int g_unusedFsfxVoiceLoadState;
// GLOBAL: XWA 0x9D1780
uint8_t g_fsfxVoiceQueueChainFlag[128];
// GLOBAL: XWA 0x9D1820
uint16_t g_fsfxVoiceQueueObjectSerial[128];
// GLOBAL: XWA 0x9D1920
uint8_t g_fsfxVoiceQueueSpeakerType[128];
// GLOBAL: XWA 0x9D19A0
uint8_t g_fsfxVoiceQueueCategory[128];
// GLOBAL: XWA 0x9E8AE0
int g_fsfxVoiceQueueSfxSlot[128];
// GLOBAL: XWA 0x9E8D40
int g_fsfxVoiceQueueObjIdx[128];
// GLOBAL: XWA 0x9CFD60
int g_fsfxTacOfficerLastSpeakSecondsByObj[1664];
// GLOBAL: XWA 0x9D1AA0
uint8_t g_fsfxVoiceLinePlayCounts[12][196];
// GLOBAL: XWA 0x5B3EF8
const uint8_t g_fsfxDesignationToVoiceVariant[24] = {
	255, 4, 21, 22, 7, 9, 10, 11, 12, 13, 14, 19, 20, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0,
};
// GLOBAL: XWA 0x5B3F10
const uint8_t g_fsfxHostileVoiceVariantRemap[16] = {
	255, 255, 255, 21, 255, 21, 22, 17, 18, 15, 16, 255, 0, 0, 0, 0,
};
// GLOBAL: XWA 0x5B3F20
const FsfxVoiceVariantReplacement g_fsfxRandomVoiceVariantReplacementPairs[18] = {
	{ 0, 23 },  { 1, 24 },  { 2, 25 },  { 3, 26 },  { 4, 27 },  { 11, 28 },
	{ 12, 29 }, { 13, 30 }, { 14, 31 }, { 15, 32 }, { 16, 33 }, { 17, 34 },
	{ 18, 35 }, { 19, 36 }, { 20, 37 }, { 21, 38 }, { 22, 39 }, { -1, -1 },
};
// GLOBAL: XWA 0x5B3B88
FsfxMissionVoiceFgSlot g_fsfxMissionVoiceFgSlots[16] = {
	{ -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 },
	{ -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 },
};
// GLOBAL: XWA 0x5B3BC8
int g_fsfxMissionVoiceBaseSfxSlot[16] = {
	266, 266, 266, 266, 266, 266, 266, 266, 266, 266, 266, 266, 266, 266, 266, 266,
};
// GLOBAL: XWA 0x63D0D0
int g_fsfxLoadedWingmanVoiceCount;
// GLOBAL: XWA 0x9D23E0
char g_fsfxSfxNameTable[2872][32];
// GLOBAL: XWA 0x63D0B4
int g_incomingMissileWarningState;
// GLOBAL: XWA 0x63D0E8
int16_t g_incomingMissileLockStrength;
// GLOBAL: XWA 0x5B3C08
int g_fsfxVoiceLoadFilter = -1;
// GLOBAL: XWA 0x5B3C10
uint16_t g_fsfxMinDistanceOrRolloffBySfxSlot[196] = {
	0,     0,     0,     0,     8192,  8192,  8192,  10240, 10240, 10240, 10240, 10240, 12288, 12288,
	12288, 8192,  8192,  8192,  10240, 10240, 10240, 8192,  8192,  8192,  49152, 24576, 24576, 24576,
	24576, 24576, 24576, 24576, 24576, 24576, 8192,  24576, 24576, 24576, 24576, 24576, 8192,  8192,
	8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,
	8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,
	8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,
	8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,
	8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,
	8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,
	8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  4096,
	8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,
	8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,
	8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  8192,  16384, 16384, 16384, 16384, 20480,
	20480, 20480, 16384, 16384, 24576, 20480, 16384, 16384, 8192,  12288, 8192,  16384, 24576, 8192,
};
// GLOBAL: XWA 0x5B3D98
int g_fsfxDefaultMinDistanceOrRolloff = 8192;
// GLOBAL: XWA 0x5B3DA0
uint8_t g_fsfxBaseVolumeBySfxSlot[196] = {
	0,   0,   0,   0,   127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 96,  96,  96,  96,  96,  96,
	96,  127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 64,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
};
// GLOBAL: XWA 0x5B3E64
uint8_t g_fsfxDefaultBaseVolume = 127;
// GLOBAL: XWA 0x5B3E68
const uint8_t g_fsfxVoiceCategoryBaseOffset[48] = {
	0,   12,  31,  43,  44,  45,  47,  50,  60,  64,  66,  67,  73,  76,  78,  80,
	81,  82,  83,  89,  94,  101, 102, 105, 106, 122, 132, 133, 145, 146, 147, 153,
	154, 155, 156, 167, 168, 169, 185, 188, 189, 0,   0,   0,   0,   0,   0,   0,
};
// GLOBAL: XWA 0x5B3E98
const uint8_t g_fsfxVoiceCategoryVariantCount[48] = {
	12, 19, 12, 1,  1, 2, 3, 10, 4, 2, 1,  6, 3, 2,  2, 1, 1, 1, 6, 5, 7, 1, 3, 1,
	16, 10, 1,  12, 1, 1, 6, 1,  1, 1, 11, 1, 1, 16, 3, 1, 7, 0, 0, 0, 0, 0, 0, 0,
};
// GLOBAL: XWA 0x5B3EC8
const uint8_t g_fsfxVoiceCategoryRepeatThreshold[48] = {
	0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
};
// GLOBAL: XWA 0x5B3FD0
const uint16_t g_fsfxVoiceCategoryLoadFlags[42] = {
	2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,
	0, 0, 0, 4, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 4, 1, 1, 1, 0,
};
// GLOBAL: XWA 0x5B328C
char g_fsfxWaveDir[8] = "wave\\";
// GLOBAL: XWA 0x9D1A20
char g_fsfxSfxLoadPath[128];
// GLOBAL: XWA 0x9CF760
char g_fsfxWingmanVoiceListPathBySlot[12][128];
// GLOBAL: XWA 0x9E8CE0
int g_objPrevX[XWA_PLAYER_COUNT];
// GLOBAL: XWA 0x9E8D00
int g_objPrevY[XWA_PLAYER_COUNT];
// GLOBAL: XWA 0x9E8D20
int g_objPrevZ[XWA_PLAYER_COUNT];
// GLOBAL: XWA 0x5A94B0
const float g_fsfxHighQualityPitchFrequency = 22050.0f;
// GLOBAL: XWA 0x5A94B4
const float g_fsfxLowQualityPitchFrequency = 11025.0f;

void fsfx_UnloadAllEffects_Thunk(void) { Sound_UnloadAllEffects(); }

// FUNCTION: XWA 0x43A0A0
void fsfx_ResetFlightSfxState(int clearSfxIdTable) {
	uint32_t objIdx;

	if (g_fsfxCurrentVoiceSfxSlot != 0 &&
		Sound_CountPlayingInstances(g_sfxIds[g_fsfxCurrentVoiceSfxSlot]) != 0) {
		do {
			Sound_StopOldestInstance(g_sfxIds[g_fsfxCurrentVoiceSfxSlot]);
		} while (Sound_CountPlayingInstances(g_sfxIds[g_fsfxCurrentVoiceSfxSlot]) != 0);
	}

	if (clearSfxIdTable) {
		memset(g_sfxIds, 0xff, sizeof(g_sfxIds));
	}

	memset(g_fsfxVoiceLinePlayCounts, 0, sizeof(g_fsfxVoiceLinePlayCounts));
	for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
		g_fsfxTacOfficerLastSpeakSecondsByObj[objIdx] = 0;
	}

	g_fsfxVoiceQueueCount = 0;
	g_fsfxCurrentVoiceSfxSlot = 0;
	g_nextNearbyWeaponSfxScanTime = 0;
}

// FUNCTION: XWA 0x43F4B0
int fsfx_IsVoiceQueueEmpty(void) { return g_fsfxVoiceQueueCount == 0; }

// FUNCTION: XWA 0x43A070
int fsfx_ClearSfxNameTable(void) {
	memset(g_fsfxSfxNameTable, 0, sizeof(g_fsfxSfxNameTable));
	return 0;
}

// FUNCTION: XWA 0x43A150
int fsfx_LoadSfxList(char* fileName, int baseSoundId, const char* waveDir) {
#ifdef XWA_MODERN
	XwaFile* stream;
#else
	FILE* stream;
#endif
	char buffer[256];
	unsigned int currentSfxSlot;
	uint16_t loadedCount;
	uint8_t explicitDistanceSeen;
	uint8_t explicitVolumeSeen;
	uint8_t voiceFilterEnabled;
	int voiceVariantIndex;
	const uint8_t* voiceVariantCount;
	const uint16_t* voiceLoadFlags;
	char* cursor;
	uint8_t baseVolume;
	uint16_t minDistanceOrRolloff;
	uint16_t sfxSlot;
	uint8_t shouldLoad;

	explicitDistanceSeen = 0;
	explicitVolumeSeen = 0;
	voiceFilterEnabled = 0;
	voiceVariantIndex = 0;
	if (g_fsfxVoiceLoadFilter != -1) {
		voiceFilterEnabled = 1;
	}

#ifdef XWA_MODERN
	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
	g_stream = stream;
	if (stream == NULL) {
		return 0;
	}
#else
	if (!File_OpenGlobalStream(fileName, "rb", 0, 0)) {
		return 0;
	}
	stream = (FILE*)g_stream;
#endif

	loadedCount = 0;
	currentSfxSlot = baseSoundId;

#ifdef XWA_MODERN
	if (File_ReadLine(stream, buffer, sizeof(buffer))) {
#else
	if (fgets(buffer, sizeof(buffer), stream) != NULL) {
#endif
		voiceVariantCount = g_fsfxVoiceCategoryVariantCount;
		voiceLoadFlags = g_fsfxVoiceCategoryLoadFlags;
		do {
			sfxSlot = (uint16_t)currentSfxSlot;
			if (sfxSlot < 196u) {
				baseVolume = g_fsfxBaseVolumeBySfxSlot[sfxSlot];
				minDistanceOrRolloff = g_fsfxMinDistanceOrRolloffBySfxSlot[sfxSlot];
			} else {
				baseVolume = g_fsfxDefaultBaseVolume;
				minDistanceOrRolloff = g_fsfxDefaultMinDistanceOrRolloff;
			}

			cursor = buffer;
			while (*cursor != '\0' && *cursor != '\n' && *cursor != '\r' && *cursor != ' ' &&
				   *cursor != '\t') {
				++cursor;
			}
			if (*cursor == ' ' || *cursor == '\t') {
				while (*cursor == ' ' || *cursor == '\t') {
					*cursor++ = '\0';
				}
				baseVolume = (uint8_t)atoi(cursor);
				explicitVolumeSeen = 1;
			}

			while (*cursor != '\0' && *cursor != '\n' && *cursor != '\r' && *cursor != ' ' &&
				   *cursor != '\t') {
				++cursor;
			}
			if (*cursor == ' ' || *cursor == '\t') {
				while (*cursor == ' ' || *cursor == '\t') {
					*cursor++ = '\0';
				}
				minDistanceOrRolloff = 0;
				for (;;) {
					char ch;

					ch = *cursor;
					if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f') && (ch < 'F' || ch > 'F')) {
						break;
					}
					minDistanceOrRolloff *= 16;
					if (ch >= '0' && ch <= '9') {
						minDistanceOrRolloff += ch - '0';
					}
					if (ch >= 'a' && ch <= 'f') {
						minDistanceOrRolloff += ch - 'a' + 10;
					}
					if (ch >= 'F' && ch <= 'F') {
						minDistanceOrRolloff += ch - 'A' + 10;
					}
					++cursor;
				}
				explicitDistanceSeen = 1;
			}
			*cursor = '\0';

			if (buffer[0] == '\0') {
				continue;
			}

			if (!g_flightConfVoiceEnabled && sfxSlot > 196u) {
				++currentSfxSlot;
				continue;
			}

			if (!g_sound3DEnabled) {
				switch (sfxSlot) {
					case 102:
					case 104:
					case 106:
					case 108:
					case 110:
					case 112:
						++currentSfxSlot;
						continue;
					default:
						break;
				}
			} else {
				switch (sfxSlot) {
					case 101:
					case 103:
					case 105:
					case 107:
					case 109:
					case 111:
						++currentSfxSlot;
						continue;
					default:
						break;
				}
			}

			if (g_sfxIds[sfxSlot] == -1) {
				strcpy(g_fsfxSfxLoadPath, waveDir);
				strcat(g_fsfxSfxLoadPath, buffer);
				strcpy(g_fsfxSfxNameTable[sfxSlot], buffer);

				shouldLoad = 1;
				if (voiceFilterEnabled) {
					if (g_fsfxVoiceLoadFilter == -2) {
						if ((*voiceLoadFlags & 2u) != 0) {
							shouldLoad = 0;
						}
					} else if (g_fsfxVoiceLoadFilter != -3 && (*voiceLoadFlags & 2u) != 0 &&
							   voiceVariantIndex != g_fsfxVoiceLoadFilter - 1) {
						shouldLoad = 0;
					}

					if ((*voiceLoadFlags & 1u) != 0) {
						shouldLoad = 1;
					}
					if (*voiceLoadFlags == 0) {
						shouldLoad = 0;
					}
					++voiceVariantIndex;
					if (voiceVariantIndex >= *voiceVariantCount) {
						++voiceLoadFlags;
						++voiceVariantCount;
						voiceVariantIndex = 0;
					}
				}

				g_sfxIds[sfxSlot] = -1;
				if (shouldLoad && Sound_LoadEffect(g_fsfxSfxLoadPath, g_fsfxSfxNameTable[sfxSlot],
												   (uint16_t)minDistanceOrRolloff)) {
					g_sfxIds[sfxSlot] = 0;
					if (explicitDistanceSeen) {
						g_fsfxMinDistanceOrRolloffBySfxSlot[sfxSlot] = (uint16_t)minDistanceOrRolloff;
					}
					if (explicitVolumeSeen) {
						g_fsfxBaseVolumeBySfxSlot[sfxSlot] = baseVolume;
					}
				}
			}

			++currentSfxSlot;
			++loadedCount;
			g_fsfxLoaded = 1;
#ifdef XWA_MODERN
		} while (File_ReadLine(stream, buffer, sizeof(buffer)));
#else
		} while (fgets(buffer, sizeof(buffer), stream) != NULL);
#endif
	}

	FeDiskIo_CloseGlobalStream(0);
	for (currentSfxSlot = 0; currentSfxSlot < 2872; ++currentSfxSlot) {
		g_sfxIds[currentSfxSlot] = Sound_FindEffectByName(g_fsfxSfxNameTable[currentSfxSlot]);
	}

	return loadedCount;
}

#define FSFX_LOAD_NAMED_MISSION_VOICE(slotIdx, filePath, displayLabel)                                       \
	do {                                                                                                     \
		int baseSoundIdForVoice;                                                                             \
		strcpy(fileName, filePath);                                                                          \
		g_fsfxMissionVoiceFgSlots[(slotIdx)].flightGroupIdx = (int16_t)fgIdx;                                \
		g_fsfxVoiceLoadFilter = -3;                                                                          \
		DebugPrintfChannel(0x4000, "Loading wingman %s...\n", displayLabel);                                 \
		baseSoundIdForVoice = 196 * voiceSlot + 266;                                                         \
		fsfx_LoadSfxList(fileName, baseSoundIdForVoice, g_fsfxWaveDir);                                      \
		g_fsfxMissionVoiceBaseSfxSlot[(slotIdx)] = baseSoundIdForVoice;                                      \
		++g_fsfxLoadedWingmanVoiceCount;                                                                     \
		++voiceSlot;                                                                                         \
	} while (0)

#define FSFX_LOAD_REBEL_PILOT_VOICE(usedIndex, slotIdx, filePath, displayLabel, duplicateText)               \
	do {                                                                                                     \
		if (usedVoice[(usedIndex)]) {                                                                        \
			FSFX_OUTPUT_DEBUG_STRING(duplicateText);                                                         \
		} else {                                                                                             \
			usedVoice[(usedIndex)] = 1;                                                                      \
			namedLeaderLoaded = 1;                                                                           \
		}                                                                                                    \
		FSFX_LOAD_NAMED_MISSION_VOICE((slotIdx), filePath, displayLabel);                                    \
	} while (0)

// FUNCTION: XWA 0x43A590
void fsfx_LoadMissionVoiceSfx(void) {
	char voiceEnabled;
	char fileName[64];
	int usedVoice[12];
	int reservedVoice[12];
	char buffer[64];

	voiceEnabled = (char)g_flightConfVoiceEnabled;
	reservedVoice[0] = 0;
	reservedVoice[1] = 0;
	reservedVoice[2] = 0;
	reservedVoice[3] = 0;
	reservedVoice[4] = 0;
	reservedVoice[5] = 0;
	reservedVoice[6] = 0;
	reservedVoice[7] = 0;
	reservedVoice[8] = 0;
	reservedVoice[9] = 0;
	reservedVoice[10] = 0;
	reservedVoice[11] = 0;
	usedVoice[0] = 0;
	usedVoice[1] = 0;
	usedVoice[2] = 0;
	usedVoice[3] = 0;
	usedVoice[4] = 0;
	usedVoice[5] = 0;
	usedVoice[6] = 0;
	usedVoice[7] = 0;
	usedVoice[8] = 0;
	usedVoice[9] = 0;
	usedVoice[10] = 0;
	usedVoice[11] = 0;
	g_fsfxEnemyWarheadAttackCalloutPlayed = 0;
	g_fsfxEnemyFighterAttackCalloutPlayed = 0;
	g_fsfxLocalPlayerTargetedProjectileFriendlyHitCount = 0;
	g_unusedFsfxVoiceLoadState = 0;

	if (voiceEnabled) {
		voiceEnabled = (char)g_gameConfig.voiceVolume;
		if (voiceEnabled) {
			{
				int namedLeaderLoaded;
				int voiceSearchIdx;
				int baseSoundId;
				int craftVoiceIdx;
				int localPlayerGlobalUnit;
				int voiceSlot;

				if (g_gameConfig.voiceSpecialEnabled) {
					int missionNameLen;
					int scanIdx;
					size_t fileNameLen;

					strcpy(fileName, "wave\\MissionVoice\\");
					strcpy(buffer, g_currentMissionFile);
					missionNameLen = (int)strlen(buffer);
					buffer[missionNameLen - 3] = 'l';
					buffer[missionNameLen - 2] = 's';
					buffer[missionNameLen - 1] = 't';
					scanIdx = 0;
					while (buffer[scanIdx] != '\\' && scanIdx < missionNameLen) {
						++scanIdx;
					}
					if (scanIdx < missionNameLen) {
						++scanIdx;
					} else {
						scanIdx = 0;
					}
					fileNameLen = strlen(fileName);
					if (scanIdx < missionNameLen) {
						size_t copyLength;

						copyLength = (size_t)(missionNameLen - scanIdx);
						memcpy(&fileName[fileNameLen], &buffer[scanIdx], copyLength);
						fileNameLen += copyLength;
					}
					fileName[fileNameLen] = '\0';
					fsfx_LoadSfxList(fileName, 196, g_fsfxWaveDir);
				}

				if (g_gameConfig.voiceTacticalOfficerEnabled && !g_provingGroundsModeActive) {
					switch (g_missionHeader.body.briefingOfficer) {
						case 0:
							strcpy(fileName, "wave\\devers.lst");
							fsfx_LoadSfxList(fileName, 2646, g_fsfxWaveDir);
							break;
						case 1:
							strcpy(fileName, "wave\\kupalo.lst");
							fsfx_LoadSfxList(fileName, 2646, g_fsfxWaveDir);
							break;
						case 2:
							strcpy(fileName, "wave\\zaletta.lst");
							fsfx_LoadSfxList(fileName, 2646, g_fsfxWaveDir);
							break;
						case 8:
							strcpy(fileName, "wave\\EmkayTac.lst");
							fsfx_LoadSfxList(fileName, 2646, g_fsfxWaveDir);
							break;
						default:
							break;
					}
				}

				namedLeaderLoaded = g_gameConfig.voicePilotEnabled;
				if (!g_gameConfig.voicePilotEnabled) {
					g_fsfxVoiceLoadFilter = -1;
					return;
				}

				{
					int fgIdx;

					baseSoundId = (int16_t)g_missionHeader.numFlightGroups;
					localPlayerGlobalUnit = 0;
					for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
						if (g_missionFlightGroups[fgIdx].playerOwnerIdx == g_localPlayer) {
							baseSoundId = fgIdx;
							localPlayerGlobalUnit = g_missionFlightGroups[fgIdx].fg.globalUnit;
							break;
						}
					}

					for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
						if (g_missionFlightGroups[fgIdx].fg.globalUnit != localPlayerGlobalUnit) {
							continue;
						}
						if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP1") == 0) {
							reservedVoice[0] = 1;
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP2") == 0) {
							reservedVoice[1] = 1;
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP3") == 0) {
							reservedVoice[2] = 1;
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP4") == 0) {
							reservedVoice[3] = 1;
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP5") == 0) {
							reservedVoice[4] = 1;
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP6") == 0) {
							reservedVoice[5] = 1;
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP7") == 0) {
							reservedVoice[6] = 1;
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP8") == 0) {
							reservedVoice[7] = 1;
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP9") == 0) {
							reservedVoice[8] = 1;
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP10") == 0 ||
								   StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "OLIN") == 0) {
							reservedVoice[9] = 1;
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP11") == 0) {
							reservedVoice[10] = 1;
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP12") == 0) {
							reservedVoice[11] = 1;
						}
					}
				}

				if (baseSoundId < (int16_t)g_missionHeader.numFlightGroups && localPlayerGlobalUnit > 0) {
					int fgIdx;

					voiceSlot = 0;
					voiceSearchIdx = (uint16_t)GameRand2() % 12;
					g_fsfxLoadedWingmanVoiceCount = 0;

					for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
						if (g_missionFlightGroups[fgIdx].fg.globalUnit != localPlayerGlobalUnit) {
							continue;
						}

						craftVoiceIdx = 0;
						namedLeaderLoaded = 0;
						if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "AERON") == 0) {
							namedLeaderLoaded = 1;
							FSFX_LOAD_NAMED_MISSION_VOICE(0, "wave\\aeron.lst", "Aeron");
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "EMON") == 0) {
							namedLeaderLoaded = 1;
							FSFX_LOAD_NAMED_MISSION_VOICE(1, "wave\\emon.lst", "Emon");
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "EMKAY") == 0) {
							namedLeaderLoaded = 1;
							FSFX_LOAD_NAMED_MISSION_VOICE(2, "wave\\emkay.lst", "Emkay");
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "OLIN") == 0) {
							FSFX_LOAD_REBEL_PILOT_VOICE(
								9, 3, "wave\\Rspxwa10.lst", "Olin/Rebel_Pilot10",
								"Rebel_Pilot10/Olin voice is specified as being used twice!!!\n");
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP1") == 0) {
							FSFX_LOAD_REBEL_PILOT_VOICE(
								0, 4, "wave\\Rspxwa1.lst", "Rebel_Pilot1",
								"Rebel_Pilot1 voice is specified as being used twice!!!\n");
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP2") == 0) {
							FSFX_LOAD_REBEL_PILOT_VOICE(
								1, 5, "wave\\Rspxwa2.lst", "Rebel_Pilot2",
								"Rebel_Pilot2 voice is specified as being used twice!!!\n");
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP3") == 0) {
							FSFX_LOAD_REBEL_PILOT_VOICE(
								2, 6, "wave\\Rspxwa3.lst", "Rebel_Pilot3",
								"Rebel_Pilot3 voice is specified as being used twice!!!\n");
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP4") == 0) {
							FSFX_LOAD_REBEL_PILOT_VOICE(
								3, 7, "wave\\Rspxwa4.lst", "Rebel_Pilot4",
								"Rebel_Pilot4 voice is specified as being used twice!!!\n");
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP5") == 0) {
							FSFX_LOAD_REBEL_PILOT_VOICE(
								4, 8, "wave\\Rspxwa5.lst", "Rebel_Pilot5",
								"Rebel_Pilot5 voice is specified as being used twice!!!\n");
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP6") == 0) {
							FSFX_LOAD_REBEL_PILOT_VOICE(
								5, 9, "wave\\Rspxwa6.lst", "Rebel_Pilot6",
								"Rebel_Pilot6 voice is specified as being used twice!!!\n");
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP7") == 0) {
							FSFX_LOAD_REBEL_PILOT_VOICE(
								6, 10, "wave\\Rspxwa7.lst", "Rebel_Pilot7",
								"Rebel_Pilot7 voice is specified as being used twice!!!\n");
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP8") == 0) {
							FSFX_LOAD_REBEL_PILOT_VOICE(
								7, 11, "wave\\Rspxwa8.lst", "Rebel_Pilot8",
								"Rebel_Pilot8 voice is specified as being used twice!!!\n");
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP9") == 0) {
							FSFX_LOAD_REBEL_PILOT_VOICE(
								8, 12, "wave\\Rspxwa9.lst", "Rebel_Pilot9",
								"Rebel_Pilot9 voice is specified as being used twice!!!\n");
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP10") == 0) {
							FSFX_LOAD_REBEL_PILOT_VOICE(
								9, 13, "wave\\Rspxwa10.lst", "Rebel_Pilot10",
								"Rebel_Pilot10/Olin voice is specified as being used twice!!!\n");
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP11") == 0) {
							FSFX_LOAD_REBEL_PILOT_VOICE(
								10, 14, "wave\\Rspxwa11.lst", "Rebel_Pilot11",
								"Rebel_Pilot11 voice is specified as being used twice!!!\n");
						} else if (StrCmpI(g_missionFlightGroups[fgIdx].fg.pilotID, "RP12") == 0) {
							FSFX_LOAD_REBEL_PILOT_VOICE(
								11, 15, "wave\\Rspxwa12.lst", "Rebel_Pilot12",
								"Rebel_Pilot12 voice is specified as being used twice!!!\n");
						} else if (g_missionFlightGroups[fgIdx].fg.numberOfCraft != 0) {
							do {
								strcpy(fileName, "wave\\");
								if (g_players[g_localPlayer].iff == 1) {
									strcpy(buffer, "ISP1.LST");
								} else {
									int stopVoice;

									stopVoice = (uint16_t)voiceSearchIdx - 1;
									if (stopVoice < 0) {
										stopVoice = 11;
									}
									while ((usedVoice[(uint16_t)voiceSearchIdx] ||
											reservedVoice[(uint16_t)voiceSearchIdx]) &&
										   stopVoice != (uint16_t)voiceSearchIdx) {
										++voiceSearchIdx;
										if ((uint16_t)voiceSearchIdx >= 12u) {
											voiceSearchIdx = 0;
										}
									}
									sprintf(buffer, "RSPXWA%d.LST", (uint16_t)voiceSearchIdx + 1);
								}
								strcat(fileName, buffer);

								baseSoundId = 196 * voiceSlot + 266;
								if (voiceSlot < 12) {
									strcpy(g_fsfxWingmanVoiceListPathBySlot[voiceSlot], fileName);
								}
								g_fsfxVoiceLoadFilter = -3;
								if (g_gameConfig.voicePilotEnabled == 2 ||
									(g_gameConfig.voicePilotEnabled == 1 && voiceSlot % 2 == 1)) {
									DebugPrintfChannel(0x4000, "Loading wingman %d...\n", voiceSlot);
									fsfx_LoadSfxList(fileName, baseSoundId, g_fsfxWaveDir);
								}
								++g_fsfxLoadedWingmanVoiceCount;
								usedVoice[(uint16_t)voiceSearchIdx] = 1;
								++voiceSearchIdx;
								if ((uint16_t)voiceSearchIdx >= 12u) {
									voiceSearchIdx = 0;
								}
								++voiceSlot;
								++craftVoiceIdx;
							} while (craftVoiceIdx < g_missionFlightGroups[fgIdx].fg.numberOfCraft);
						}

						if (namedLeaderLoaded && g_missionFlightGroups[fgIdx].fg.numberOfCraft > 1u &&
							craftVoiceIdx < g_missionFlightGroups[fgIdx].fg.numberOfCraft - 1) {
							do {
								strcpy(fileName, "wave\\");
								if (g_players[g_localPlayer].iff == 1) {
									strcpy(buffer, "ISP1.LST");
								} else {
									int stopVoice;

									stopVoice = (uint16_t)voiceSearchIdx - 1;
									if (stopVoice < 0) {
										stopVoice = 11;
									}
									while ((usedVoice[(uint16_t)voiceSearchIdx] ||
											reservedVoice[(uint16_t)voiceSearchIdx]) &&
										   stopVoice != (uint16_t)voiceSearchIdx) {
										++voiceSearchIdx;
										if ((uint16_t)voiceSearchIdx >= 12u) {
											voiceSearchIdx = 0;
										}
									}
									sprintf(buffer, "RSPXWA%d.LST", (uint16_t)voiceSearchIdx + 1);
								}
								strcat(fileName, buffer);

								baseSoundId = 196 * voiceSlot + 266;
								if (voiceSlot < 12) {
									strcpy(g_fsfxWingmanVoiceListPathBySlot[voiceSlot], fileName);
								}
								g_fsfxVoiceLoadFilter = -3;
								if (g_gameConfig.voicePilotEnabled == 2 ||
									(g_gameConfig.voicePilotEnabled == 1 && voiceSlot % 2 == 1)) {
									DebugPrintfChannel(0x4000, "Loading wingman %d...\n", voiceSlot);
									fsfx_LoadSfxList(fileName, baseSoundId, g_fsfxWaveDir);
								}
								++g_fsfxLoadedWingmanVoiceCount;
								usedVoice[(uint16_t)voiceSearchIdx] = 1;
								++voiceSearchIdx;
								if ((uint16_t)voiceSearchIdx >= 12u) {
									voiceSearchIdx = 0;
								}
								++voiceSlot;
								++craftVoiceIdx;
							} while (craftVoiceIdx < g_missionFlightGroups[fgIdx].fg.numberOfCraft - 1);
						}
					}

					namedLeaderLoaded =
						DebugPrintfChannel(0x4000, "Found %d wingmen in player's squadron %d.\n", voiceSlot,
										   localPlayerGlobalUnit);
				}

				g_fsfxVoiceLoadFilter = -1;
			}
		}
	}
}

#undef FSFX_LOAD_REBEL_PILOT_VOICE
#undef FSFX_LOAD_NAMED_MISSION_VOICE

// FUNCTION: XWA 0x43E370
int fsfx_speakorderack(int playerIdx, int speakerObjIdx, int voiceCategory, int voiceVariant,
					   unsigned int relatedObjIdx, uint16_t probability) {
	int playerObjIdx;
	ObjectRecord* speakerObj;
	CraftData* speakerCraft;
	unsigned int speakerVoiceOrdinal;
	unsigned int voiceOrdinal;
	int voiceBaseSlot;
	int speakerCallsignOrdinal;
	uint16_t relatedObjectSerial;
	unsigned int relatedCraftOrdinal;
	int categoryBaseOffset;
	char isMissionLeaderVoice;
	char forcedAnonymousSpeaker;
	unsigned int candidateObjects[12];

	isMissionLeaderVoice = 0;
	forcedAnonymousSpeaker = 0;
	if (!g_gameConfig.voicePilotEnabled) {
		return 0;
	}
	if (playerIdx == -1 || playerIdx != g_localPlayer) {
		return 0;
	}

	playerObjIdx = g_players[playerIdx].objectIndex;
	if (playerObjIdx == 0xffff) {
		return 0;
	}

	if (speakerObjIdx == playerObjIdx || !g_fsfxLoadedWingmanVoiceCount) {
		return 0;
	}

	if (probability != 0xffffu) {
		if (g_gameConfig.voicePilotEnabled == 1) {
			probability = (uint16_t)(probability >> 1);
		}
		if ((uint16_t)GameRand2() >= probability) {
			return 0;
		}
	}

	if (speakerObjIdx == -2) {
		forcedAnonymousSpeaker = 1;
	}
	if (speakerObjIdx == -1 || speakerObjIdx == -2) {
		unsigned int candidateCount;
		unsigned int objIdx;

		candidateCount = 0;
		for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
			ObjectRecord* obj;
			MobileObject* mobj;
			CraftData* craft;

			obj = &g_objectTable[objIdx];
			if (obj->objectType != OBJ_None &&
				g_missionFlightGroups[obj->flightGroupIdx].fg.globalUnit ==
					g_missionFlightGroups[g_objectTable[playerObjIdx].flightGroupIdx].fg.globalUnit) {
				mobj = obj->mobj;
				if (mobj->state == 0 && obj->playerOwnerIdx != playerIdx) {
					craft = mobj->pCraft;
					if ((unsigned int)craft->hullDamage <= (unsigned int)craft->hullMax) {
						candidateObjects[candidateCount++] = objIdx;
						if (candidateCount >= 12) {
							break;
						}
					}
				}
			}
		}

		if (candidateCount == 0) {
			speakerObjIdx = 0xffff;
		} else {
			unsigned int candidateIndex;

			candidateIndex = (uint16_t)GameRand2() % candidateCount;
			speakerObjIdx = (int)candidateObjects[candidateIndex];
			if ((unsigned int)speakerObjIdx == relatedObjIdx) {
				if (candidateCount < 2) {
					speakerObjIdx = 0;
				} else {
					++candidateIndex;
					if (candidateIndex >= candidateCount) {
						candidateIndex = 0;
					}
					speakerObjIdx = (int)candidateObjects[candidateIndex];
				}
			}
		}
		if (speakerObjIdx == 0xffff) {
			return 0;
		}
	} else if (voiceCategory != 1 && voiceCategory != 21 &&
			   g_missionFlightGroups[g_objectTable[playerObjIdx].flightGroupIdx].fg.globalUnit !=
				   g_missionFlightGroups[g_objectTable[speakerObjIdx].flightGroupIdx].fg.globalUnit) {
		return 0;
	}
#ifdef XWA_MODERN
	if (speakerObjIdx == 0xffff) {
		return 0;
	}
#endif

	speakerObj = &g_objectTable[speakerObjIdx];
	speakerCraft = speakerObj->mobj->pCraft;
	speakerVoiceOrdinal = (unsigned int)speakerCraft->craftIndexInGroup;
	speakerCallsignOrdinal = (int)speakerVoiceOrdinal;
	if (speakerVoiceOrdinal > 0) {
		voiceOrdinal = (speakerVoiceOrdinal - 2u) % (unsigned int)g_fsfxLoadedWingmanVoiceCount;
	} else {
		voiceOrdinal = 0;
	}

	if (speakerVoiceOrdinal > 12u) {
		speakerCallsignOrdinal = 0;
	}

	if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[0].flightGroupIdx) {
		isMissionLeaderVoice = 1;
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[0] + 196 * speakerCraft->waveNumber;
	} else if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[1].flightGroupIdx) {
		isMissionLeaderVoice = 1;
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[1] + 196 * speakerCraft->waveNumber;
	} else if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[2].flightGroupIdx) {
		isMissionLeaderVoice = 1;
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[2] + 196 * speakerCraft->waveNumber;
	} else if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[3].flightGroupIdx) {
		isMissionLeaderVoice = 1;
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[3] + 196 * speakerCraft->waveNumber;
	} else if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[4].flightGroupIdx) {
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[4] + 196 * speakerCraft->waveNumber;
	} else if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[5].flightGroupIdx) {
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[5] + 196 * speakerCraft->waveNumber;
	} else if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[6].flightGroupIdx) {
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[6] + 196 * speakerCraft->waveNumber;
	} else if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[7].flightGroupIdx) {
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[7] + 196 * speakerCraft->waveNumber;
	} else if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[8].flightGroupIdx) {
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[8] + 196 * speakerCraft->waveNumber;
	} else if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[9].flightGroupIdx) {
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[9] + 196 * speakerCraft->waveNumber;
	} else if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[10].flightGroupIdx) {
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[10] + 196 * speakerCraft->waveNumber;
	} else if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[11].flightGroupIdx) {
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[11] + 196 * speakerCraft->waveNumber;
	} else if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[12].flightGroupIdx) {
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[12] + 196 * speakerCraft->waveNumber;
	} else if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[13].flightGroupIdx) {
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[13] + 196 * speakerCraft->waveNumber;
	} else if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[14].flightGroupIdx) {
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[14] + 196 * speakerCraft->waveNumber;
	} else if (speakerObj->flightGroupIdx == g_fsfxMissionVoiceFgSlots[15].flightGroupIdx) {
		voiceBaseSlot = g_fsfxMissionVoiceBaseSfxSlot[15] + 196 * speakerCraft->waveNumber;
	} else {
		isMissionLeaderVoice = 0;
		voiceBaseSlot = 266 + 196 * (int)voiceOrdinal;
	}

	if (voiceOrdinal != 0 && voiceOrdinal <= 12u && g_fsfxLoadedWingmanVoiceCount) {
		const uint16_t* loadFlags;
		unsigned int category;
		unsigned int loadedVoiceIndex;
		unsigned int voiceIndexOffset;
		char hasLoadedVoice;

		hasLoadedVoice = 1;
		voiceIndexOffset = voiceOrdinal - 1u;
		loadedVoiceIndex = voiceIndexOffset % (unsigned int)g_fsfxLoadedWingmanVoiceCount;
		category = 0;
		loadFlags = g_fsfxVoiceCategoryLoadFlags;
		for (;;) {
			if (((uint8_t)*loadFlags & 2u) != 0) {
				if (g_sfxIds[196u * loadedVoiceIndex + 266u + g_fsfxVoiceCategoryBaseOffset[category] +
							 voiceIndexOffset] == -1) {
					hasLoadedVoice = 0;
				}
				break;
			}
			++loadFlags;
			++category;
			if (loadFlags > &g_fsfxVoiceCategoryLoadFlags[41]) {
				break;
			}
		}
		if (!hasLoadedVoice) {
			g_fsfxVoiceLoadFilter = -1;
		}
	}

	relatedCraftOrdinal = 0;
	if (relatedObjIdx == 0xffffu || relatedObjIdx < g_activeRegionObjectSlotStart ||
		relatedObjIdx >= g_activeRegionCraftObjectSlotEnd) {
		relatedObjectSerial = 0xffffu;
	} else {
		ObjectRecord* relatedObj;
		unsigned int ordinal;

		relatedObj = &g_objectTable[relatedObjIdx];
		relatedObjectSerial = relatedObj->objectSignature;
		ordinal = (unsigned int)relatedObj->mobj->pCraft->craftIndexInGroup;
		relatedCraftOrdinal = ordinal;
		if (ordinal == 0 || ordinal > 12u) {
			relatedCraftOrdinal = 0;
		}
	}

	categoryBaseOffset = g_fsfxVoiceCategoryBaseOffset[voiceCategory];
	if (voiceVariant == -1) {
		int selectedVariant;
		uint8_t* playCount;
		uint8_t repeatThreshold;
		int variantCount;

		variantCount = g_fsfxVoiceCategoryVariantCount[voiceCategory];
		selectedVariant = (uint16_t)GameRand2() % (int)(uint16_t)variantCount;
		if ((voiceCategory == 19 || voiceCategory == 20) && g_objectTable[relatedObjIdx].mobj->iff != 1) {
			switch (voiceCategory) {
				case 19:
					while (selectedVariant + categoryBaseOffset == 93) {
						selectedVariant = (uint16_t)GameRand2() % (int)(uint16_t)variantCount;
					}
					break;

				case 20:
					while (selectedVariant + categoryBaseOffset == 100) {
						selectedVariant = (uint16_t)GameRand2() % (int)(uint16_t)variantCount;
					}
					break;
			}
		}
		repeatThreshold = g_fsfxVoiceCategoryRepeatThreshold[voiceCategory];

		if (repeatThreshold != 0 &&
			g_fsfxVoiceLinePlayCounts[voiceOrdinal][selectedVariant + categoryBaseOffset] >=
				repeatThreshold) {
			int remaining;

			remaining = variantCount - 1;
			if (variantCount == 0) {
				selectedVariant = -1;
			} else {
				for (;;) {
					++selectedVariant;
					if (selectedVariant >= variantCount) {
						selectedVariant = 0;
					}

					if (voiceCategory == 19 || voiceCategory == 20) {
						if (g_objectTable[relatedObjIdx].mobj->iff == 1) {
							if (g_fsfxVoiceLinePlayCounts[voiceOrdinal]
														 [selectedVariant + categoryBaseOffset] <
								repeatThreshold) {
								break;
							}
						} else if (voiceCategory == 19) {
							if (selectedVariant + categoryBaseOffset != 93 &&
								g_fsfxVoiceLinePlayCounts[voiceOrdinal]
														 [selectedVariant + categoryBaseOffset] <
									g_fsfxVoiceCategoryRepeatThreshold[19]) {
								break;
							}
						} else if (voiceCategory == 20) {
							if (selectedVariant + categoryBaseOffset != 100 &&
								g_fsfxVoiceLinePlayCounts[voiceOrdinal]
														 [selectedVariant + categoryBaseOffset] <
									g_fsfxVoiceCategoryRepeatThreshold[20]) {
								break;
							}
						} else if (g_fsfxVoiceLinePlayCounts[voiceOrdinal]
															[selectedVariant + categoryBaseOffset] <
								   repeatThreshold) {
							break;
						}
					} else if (g_fsfxVoiceLinePlayCounts[voiceOrdinal][selectedVariant + categoryBaseOffset] <
							   repeatThreshold) {
						break;
					}

					if (remaining-- == 0) {
						selectedVariant = -1;
						break;
					}
				}
			}
		}

		if (selectedVariant == -1) {
			return 0;
		}

		playCount = &g_fsfxVoiceLinePlayCounts[voiceOrdinal][selectedVariant + categoryBaseOffset];
		if (*playCount >= repeatThreshold && repeatThreshold != 0) {
			return 1;
		}

		switch (voiceCategory) {
			case 4:
				if (g_fsfxVoiceQueueCount) {
					break;
				}
				if (speakerCallsignOrdinal) {
					fsfx_QueueVoiceSfx(voiceBaseSlot + speakerCallsignOrdinal +
										   g_fsfxVoiceCategoryBaseOffset[2] - 1,
									   1u, 2u, 0, relatedObjectSerial, (int)relatedObjIdx);
				}
				++*playCount;
				fsfx_QueueVoiceSfx(categoryBaseOffset + voiceBaseSlot + selectedVariant, 1u,
								   (uint8_t)voiceCategory, 0, relatedObjectSerial, (int)relatedObjIdx);
				break;

			case 9: {
				unsigned int variantScan;
				unsigned int variantCountScan;

				if (g_fsfxVoiceQueueCount || !speakerCallsignOrdinal) {
					break;
				}
				variantCountScan = g_fsfxVoiceCategoryVariantCount[voiceCategory];
				for (variantScan = 0; variantScan < variantCountScan; ++variantScan) {
					if (g_fsfxVoiceLinePlayCounts[voiceOrdinal][categoryBaseOffset + variantScan] >=
						repeatThreshold) {
						break;
					}
				}
				if (variantScan < variantCountScan) {
					fsfx_QueueVoiceSfx(voiceBaseSlot + speakerCallsignOrdinal +
										   g_fsfxVoiceCategoryBaseOffset[0] - 1,
									   1u, 0, 0, relatedObjectSerial, (int)relatedObjIdx);
					++*playCount;
					fsfx_QueueVoiceSfx(categoryBaseOffset + voiceBaseSlot + selectedVariant, 1u,
									   (uint8_t)voiceCategory, 0, relatedObjectSerial, (int)relatedObjIdx);
				}
				break;
			}

			case 10:
				if (speakerCallsignOrdinal) {
					++*playCount;
					fsfx_QueueVoiceSfx(voiceBaseSlot + speakerCallsignOrdinal +
										   g_fsfxVoiceCategoryBaseOffset[0] - 1,
									   1u, 0, 0, relatedObjectSerial, (int)relatedObjIdx);
				}
				fsfx_QueueVoiceSfx(categoryBaseOffset + voiceBaseSlot + selectedVariant, 1u,
								   (uint8_t)voiceCategory, 0, relatedObjectSerial, (int)relatedObjIdx);
				break;

			case 30:
				if (g_fsfxVoiceQueueCount) {
					break;
				}
				++*playCount;
				fsfx_QueueVoiceSfx(categoryBaseOffset + voiceBaseSlot + selectedVariant, 1u,
								   (uint8_t)voiceCategory, 0, relatedObjectSerial, (int)relatedObjIdx);
				break;

			case 19:
			case 20:
			case 38:
				++*playCount;
				fsfx_QueueVoiceSfx(categoryBaseOffset + voiceBaseSlot + selectedVariant, 1u,
								   (uint8_t)voiceCategory, 0, relatedObjectSerial, (int)relatedObjIdx);
				break;

			case 3:
			case 7:
				if (g_fsfxVoiceQueueCount) {
					break;
				}
				if (speakerCallsignOrdinal) {
					fsfx_QueueVoiceSfx(voiceBaseSlot + speakerCallsignOrdinal +
										   g_fsfxVoiceCategoryBaseOffset[2] - 1,
									   1u, 2u, 0, relatedObjectSerial, (int)relatedObjIdx);
				}
				fsfx_QueueVoiceSfx(categoryBaseOffset + voiceBaseSlot + selectedVariant, 1u,
								   (uint8_t)voiceCategory, 1u, relatedObjectSerial, (int)relatedObjIdx);
				++*playCount;
				break;

			case 8:
				if (g_fsfxVoiceQueueCount) {
					break;
				}
				++*playCount;
				fsfx_QueueVoiceSfx(categoryBaseOffset + voiceBaseSlot + selectedVariant, 1u,
								   (uint8_t)voiceCategory, 0, relatedObjectSerial, (int)relatedObjIdx);
				break;

			case 21:
			case 28:
				++*playCount;
				fsfx_QueueVoiceSfx(categoryBaseOffset + voiceBaseSlot + selectedVariant, 1u,
								   (uint8_t)voiceCategory, 0, relatedObjectSerial, (int)relatedObjIdx);
				break;

			case 26:
				if (relatedCraftOrdinal) {
					++*playCount;
					fsfx_QueueVoiceSfx(categoryBaseOffset + voiceBaseSlot + selectedVariant, 1u,
									   (uint8_t)voiceCategory, 0, relatedObjectSerial, (int)relatedObjIdx);
					fsfx_QueueVoiceSfx(voiceBaseSlot + (int)relatedCraftOrdinal +
										   g_fsfxVoiceCategoryBaseOffset[27] - 1,
									   1u, 27u, 1u, relatedObjectSerial, (int)relatedObjIdx);
				} else {
					fsfx_QueueVoiceSfx(g_fsfxVoiceCategoryBaseOffset[29] + voiceBaseSlot, 1u, 29u, 0,
									   relatedObjectSerial, (int)relatedObjIdx);
				}
				break;

			default:
				break;
		}
	} else {
		uint8_t* playCount;
		uint8_t repeatThreshold;

		playCount = &g_fsfxVoiceLinePlayCounts[voiceOrdinal][categoryBaseOffset + voiceVariant];
		repeatThreshold = g_fsfxVoiceCategoryRepeatThreshold[voiceCategory];
		if (*playCount >= repeatThreshold && repeatThreshold != 0) {
			return 1;
		}

		switch (voiceCategory) {
			case 1:
				if (speakerCallsignOrdinal && !forcedAnonymousSpeaker) {
					fsfx_QueueVoiceSfx(voiceBaseSlot + speakerCallsignOrdinal +
										   g_fsfxVoiceCategoryBaseOffset[0] - 1,
									   1u, 0, 0, relatedObjectSerial, (int)relatedObjIdx);
				}
				if (voiceVariant >= 2 && (uint16_t)GameRand2() < 0x5555u) {
					fsfx_QueueVoiceSfx(g_fsfxVoiceCategoryBaseOffset[1] + voiceBaseSlot, 1u, 1u, 1u,
									   relatedObjectSerial, (int)relatedObjIdx);
				}
				if (voiceVariant >= g_fsfxVoiceCategoryVariantCount[voiceCategory]) {
					return 0;
				}
				fsfx_QueueVoiceSfx(voiceVariant + categoryBaseOffset + voiceBaseSlot, 1u,
								   (uint8_t)voiceCategory, 2u, relatedObjectSerial, (int)relatedObjIdx);
				break;

			case 34:
				if (voiceVariant == -1) {
					return 0;
				}
				++*playCount;
				fsfx_QueueVoiceSfx(voiceVariant + categoryBaseOffset + voiceBaseSlot, 1u,
								   (uint8_t)voiceCategory, 0, relatedObjectSerial, (int)relatedObjIdx);
				break;

			case 40:
				if ((uint16_t)GameRand2() < 0x8000u) {
					int randomVariant;
					uint16_t variantCount;

					variantCount = g_fsfxVoiceCategoryVariantCount[38];
					randomVariant = (uint16_t)((uint16_t)GameRand2() % (int)variantCount);
					fsfx_QueueVoiceSfx(g_fsfxVoiceCategoryBaseOffset[38] + voiceBaseSlot + randomVariant, 1u,
									   38u, 0, relatedObjectSerial, (int)relatedObjIdx);
				}
				if (voiceVariant >= g_fsfxVoiceCategoryVariantCount[voiceCategory]) {
					return 0;
				}
				fsfx_QueueVoiceSfx(voiceVariant + categoryBaseOffset + voiceBaseSlot, 1u,
								   (uint8_t)voiceCategory, 1u, relatedObjectSerial, (int)relatedObjIdx);
				++*playCount;
				break;

			case 25:
				if (*playCount >= repeatThreshold && repeatThreshold != 0 && voiceVariant != 1) {
					return 0;
				}
				if (g_fsfxVoiceQueueCount) {
					break;
				}
				++*playCount;
				if (isMissionLeaderVoice) {
					fsfx_QueueVoiceSfx(voiceBaseSlot + g_fsfxVoiceCategoryBaseOffset[24] + 15, 1u, 24u, 0,
									   relatedObjectSerial, (int)relatedObjIdx);
				} else if (relatedCraftOrdinal) {
					fsfx_QueueVoiceSfx(voiceBaseSlot + (int)relatedCraftOrdinal +
										   g_fsfxVoiceCategoryBaseOffset[24] - 1,
									   1u, 24u, 0, relatedObjectSerial, (int)relatedObjIdx);
				}
				fsfx_QueueVoiceSfx(g_fsfxVoiceCategoryBaseOffset[25] + voiceVariant + voiceBaseSlot, 1u, 25u,
								   0, relatedObjectSerial, (int)relatedObjIdx);
				break;

			case 7:
				if (g_fsfxVoiceQueueCount > 2u) {
					return 0;
				}
				++*playCount;
				if (speakerCallsignOrdinal) {
					fsfx_QueueVoiceSfx(voiceBaseSlot + speakerCallsignOrdinal +
										   g_fsfxVoiceCategoryBaseOffset[2] - 1,
									   1u, 2u, 0, relatedObjectSerial, (int)relatedObjIdx);
				}
				fsfx_QueueVoiceSfx(voiceVariant + categoryBaseOffset + voiceBaseSlot, 1u, 7u, 0,
								   relatedObjectSerial, (int)relatedObjIdx);
				break;

			default:
				if (voiceVariant >= g_fsfxVoiceCategoryVariantCount[voiceCategory]) {
					return 0;
				}
				fsfx_QueueVoiceSfx(voiceVariant + categoryBaseOffset + voiceBaseSlot, 1u,
								   (uint8_t)voiceCategory, 0, relatedObjectSerial, (int)relatedObjIdx);
				break;
		}
	}
	return 1;
}

// FUNCTION: XWA 0x43F4C0
int fsfx_QueueVoiceSfx(int sfxSlot, uint8_t speakerType, uint8_t voiceCategory, uint8_t chainFlag,
					   uint16_t objectSerial, int objIdx) {
	uint8_t queueCount;
	uint8_t queueIndex;

	if (g_flightSimSideEffectsSuppressed) {
		return 0;
	}
	if (!g_flightConfVoiceEnabled) {
		return 0;
	}
	if (g_sfxIds[sfxSlot] == -1) {
		return 0;
	}
	if (!g_gameConfig.voiceVolume) {
		return 0;
	}

	queueCount = g_fsfxVoiceQueueCount;
	if (queueCount == 128) {
		return 0;
	}
	if (!g_flightSideEffectsEnabled) {
		return 0;
	}

	if (queueCount != 0 && speakerType == 2 && voiceCategory == 4 && chainFlag == 0) {
		unsigned int prevIndex;

		prevIndex = (unsigned int)queueCount - 1;
		if (g_fsfxVoiceQueueSpeakerType[prevIndex] == speakerType &&
			g_fsfxVoiceQueueCategory[prevIndex] == 4 &&
			g_fsfxVoiceQueueObjectSerial[prevIndex] == (uint16_t)objectSerial &&
			g_fsfxVoiceQueueObjIdx[prevIndex] == objIdx) {
			return 0;
		}
	}

	queueIndex = queueCount;
	g_fsfxVoiceQueueCount = (uint8_t)(queueCount + 1);
	g_fsfxVoiceQueueSpeakerType[queueIndex] = speakerType;
	g_fsfxVoiceQueueObjectSerial[queueIndex] = (uint16_t)objectSerial;
	g_fsfxVoiceQueueObjIdx[queueIndex] = objIdx;
	g_fsfxVoiceQueueSfxSlot[queueIndex] = sfxSlot;
	g_fsfxVoiceQueueCategory[queueIndex] = voiceCategory;
	g_fsfxVoiceQueueChainFlag[queueIndex] = chainFlag;
	return 1;
}

// FUNCTION: XWA 0x43F7F0
void fsfx_RemoveVoiceQueueEntryChain(unsigned int queueIndex) {
	unsigned int scanIndex;
	int removeCount;
	unsigned int dstIndex;
	unsigned int newCount;
	unsigned int savedRemoveCount;

	scanIndex = queueIndex + 1;
	removeCount = 1;
	savedRemoveCount = 1;
	if (g_fsfxVoiceQueueChainFlag[scanIndex] != 0) {
		while (g_fsfxVoiceQueueChainFlag[scanIndex] != 0 && scanIndex < g_fsfxVoiceQueueCount) {
			++removeCount;
			++scanIndex;
		}
		savedRemoveCount = (unsigned int)removeCount;
	}

	g_fsfxVoiceQueueCount = (uint8_t)(g_fsfxVoiceQueueCount - removeCount);
	dstIndex = queueIndex;
	newCount = g_fsfxVoiceQueueCount;
	if (dstIndex < newCount) {
		uint8_t* const shiftedChainFlags = g_fsfxVoiceQueueChainFlag - removeCount;
		unsigned int srcByteIndex;
		uint16_t* dstObjectSerial;
		uint16_t* srcObjectSerial;
		int srcIndex;
		int sfxSlot;
		uint8_t speakerType;
		uint8_t category;

		srcByteIndex = removeCount + queueIndex;
		srcIndex = (int)srcByteIndex;
		dstObjectSerial = &g_fsfxVoiceQueueObjectSerial[queueIndex];
		srcObjectSerial = &g_fsfxVoiceQueueObjectSerial[srcByteIndex];
		do {
			sfxSlot = g_fsfxVoiceQueueSfxSlot[srcIndex];
			speakerType = g_fsfxVoiceQueueSpeakerType[removeCount + dstIndex];
			g_fsfxVoiceQueueSpeakerType[dstIndex] = speakerType;
			category = g_fsfxVoiceQueueCategory[srcByteIndex];
			g_fsfxVoiceQueueSfxSlot[dstIndex] = sfxSlot;
			g_fsfxVoiceQueueCategory[dstIndex] = category;
			shiftedChainFlags[srcByteIndex] = g_fsfxVoiceQueueChainFlag[srcByteIndex];
			*dstObjectSerial = *srcObjectSerial;
			g_fsfxVoiceQueueObjIdx[dstIndex++] = g_fsfxVoiceQueueObjIdx[srcIndex];
			++srcByteIndex;
			++srcObjectSerial;
			++dstObjectSerial;
			++srcIndex;
			removeCount = (int)savedRemoveCount;
		} while (dstIndex < newCount);
	}
}

// FUNCTION: XWA 0x43C7B0
int fsfx_UpdateTargetingTone(int state) {
	unsigned int volume;
	int interiorVolume;

	if (!g_flightConfSfxEnabled) {
		return 0;
	}
	if (!g_gameConfig.sfxInteriorEnabled) {
		return 0;
	}
	if (!g_gameConfig.sfxInteriorVolume) {
		return 0;
	}

	interiorVolume = g_gameConfig.sfxInteriorVolume;
	if (interiorVolume >= 10) {
		volume = 127;
	} else {
		volume = 13 * interiorVolume;
	}

	if (state == 0) {
		if (Sound_CountPlayingInstances(g_sfxIds[77]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[77]);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[76]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[76]);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[78]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[78]);
		}
		g_targetingToneWeaponReadyQueued = 0;
		g_targetingToneLastSeekBeepTime = 0;
		return 1;
	}

	if (state == 1) {
		if (Sound_CountPlayingInstances(g_sfxIds[77]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[77]);
		} else if (Sound_CountPlayingInstances(g_sfxIds[76]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[76]);
		}
		g_targetingToneLastSeekBeepTime = 0;
		if (Sound_CountPlayingInstances(g_sfxIds[78]) == 0 && !g_targetingToneWeaponReadyQueued) {
			Sound_QueueEffect(g_sfxIds[78], 1, 0, 125, volume, 64, -1, 0xffffu);
			g_targetingToneWeaponReadyQueued = 1;
			return 1;
		}
	} else if (state == 2) {
		ModelIndex missileBoatModelIndex;
		uint16_t seekerScale;

		if (Sound_CountPlayingInstances(g_sfxIds[77]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[77]);
		} else if (Sound_CountPlayingInstances(g_sfxIds[78]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[78]);
		}
		g_targetingToneWeaponReadyQueued = 0;

		missileBoatModelIndex = GetModelIndexFromType(OBJ_MissileBoat);
		if (GetModelIndexFromType(g_objectTable[g_players[g_localPlayer].objectIndex].objectType) ==
			missileBoatModelIndex) {
			seekerScale = 354;
		} else {
			seekerScale = 708;
		}
		if (g_gameTime - g_targetingToneLastSeekBeepTime >
			141 *
					(seekerScale - (int16_t)g_objectTable[g_players[g_localPlayer].objectIndex]
									   .mobj->pCraft->warheadLockTicks) /
					seekerScale +
				11) {
			Sound_QueueEffect(g_sfxIds[76], 1, 0, 125, volume, 64, -1, 0xffffu);
			g_targetingToneLastSeekBeepTime = g_gameTime;
			return 1;
		}
	} else if (state == 3) {
		if (Sound_CountPlayingInstances(g_sfxIds[76]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[76]);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[78]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[78]);
		}
		g_targetingToneWeaponReadyQueued = 0;
		g_targetingToneLastSeekBeepTime = 0;
		if (Sound_CountPlayingInstances(g_sfxIds[77]) == 0) {
			Sound_QueueEffect(g_sfxIds[77], 1, 1, 125, volume, 64, -1, 0xffffu);
		}
	}

	return 1;
}

// FUNCTION: XWA 0x43F5D0
void fsfx_UpdateVoiceQueue(void) {
	unsigned int queueIndex;

	if (!g_fsfxLoaded) {
		return;
	}

	queueIndex = 0;
	if (g_fsfxVoiceQueueCount != 0) {
		while (queueIndex < g_fsfxVoiceQueueCount) {
			if (g_fsfxVoiceQueueChainFlag[queueIndex] == 0 && g_fsfxVoiceQueueSpeakerType[queueIndex] == 2 &&
				g_fsfxVoiceQueueCategory[queueIndex] == 4) {
				int objectStillValid;
				int objIdx;
				int exemptNextSfxSlot;
				ObjectRecord* object;

#ifdef XWA_MODERN
				exemptNextSfxSlot = queueIndex + 1u < 128 ? g_fsfxVoiceQueueSfxSlot[queueIndex + 1u] : 0;
#else
				exemptNextSfxSlot = g_fsfxVoiceQueueSfxSlot[queueIndex + 1u];
#endif

				if (exemptNextSfxSlot == 2735 || exemptNextSfxSlot == 2736 || exemptNextSfxSlot == 2737 ||
					exemptNextSfxSlot == 2738 || exemptNextSfxSlot == 2749) {
					++queueIndex;
					continue;
				}

				objIdx = g_fsfxVoiceQueueObjIdx[queueIndex];
				objectStillValid = 0;
				object = &g_objectTable[objIdx];
				if (object->objectSignature == g_fsfxVoiceQueueObjectSerial[queueIndex] &&
					object->objectType != OBJ_None && object->mobj != NULL && object->mobj->pCraft != NULL &&
					object->mobj->pCraft->objectKind != 3 && object->mobj->pCraft->objectKind != 4) {
					objectStillValid = 1;
				}

				if (!objectStillValid) {
					fsfx_RemoveVoiceQueueEntryChain(queueIndex);
					continue;
				}
			}

			++queueIndex;
		}
	}

	if (g_fsfxCurrentVoiceSfxSlot != 0 &&
		Sound_CountPlayingInstances(g_sfxIds[g_fsfxCurrentVoiceSfxSlot]) != 0) {
		return;
	}

	g_fsfxCurrentVoiceSfxSlot = 0;
	if (g_fsfxVoiceQueueCount != 0) {
		int sfxSlot;
		unsigned int remainingCount;
		unsigned int i;
		int soundId;
		int volume;
		unsigned int voiceVolume;

		g_fsfxCurrentVoiceSpeakerType = g_fsfxVoiceQueueSpeakerType[0];
		sfxSlot = g_fsfxVoiceQueueSfxSlot[0];
		g_fsfxCurrentVoiceChainFlag = g_fsfxVoiceQueueChainFlag[0];
		g_fsfxCurrentVoiceCategory = g_fsfxVoiceQueueCategory[0];
		g_fsfxCurrentVoiceObjectSerial = g_fsfxVoiceQueueObjectSerial[0];

		remainingCount = (uint8_t)(g_fsfxVoiceQueueCount - 1u);
		i = 0;
		if (--g_fsfxVoiceQueueCount != 0) {
			do {
				g_fsfxVoiceQueueSfxSlot[i] = g_fsfxVoiceQueueSfxSlot[i + 1u];
				g_fsfxVoiceQueueSpeakerType[i] = g_fsfxVoiceQueueSpeakerType[i + 1u];
				g_fsfxVoiceQueueCategory[i] = g_fsfxVoiceQueueCategory[i + 1u];
				g_fsfxVoiceQueueChainFlag[i] = g_fsfxVoiceQueueChainFlag[i + 1u];
				g_fsfxVoiceQueueObjectSerial[i] = g_fsfxVoiceQueueObjectSerial[i + 1u];
				g_fsfxVoiceQueueObjIdx[i] = g_fsfxVoiceQueueObjIdx[i + 1u];
				++i;
			} while (i < remainingCount);
		}

		soundId = g_sfxIds[sfxSlot];
		if (soundId == -1) {
			return;
		}
		if (!g_gameConfig.voiceVolume) {
			return;
		}

		voiceVolume = g_gameConfig.voiceVolume;
		if (voiceVolume >= 10) {
			volume = 127;
		} else {
			volume = 13 * voiceVolume;
		}

		Sound_QueueEffect(soundId, 1, 0, 126, volume, 64, -1, 0xffffu);
		g_fsfxCurrentVoiceSfxSlot = sfxSlot;
	}
}

// FUNCTION: XWA 0x43DF50
void fsfx_UpdateMissileThreatWarning(void) {
	uint32_t objIdx;
	int warningState;
	int16_t armedLockStrength;
	MobileObject* mobj;
	CraftData* craft;

	if (g_players[g_localPlayer].objectIndex == 0xffff) {
		return;
	}

	for (objIdx = g_projectileObjectSlotStart; objIdx < g_projectileObjectSlotEnd; ++objIdx) {
		ModelGenusId genusId;

		if (g_objectTable[objIdx].objectType == OBJ_None) {
			continue;
		}

		genusId = g_objectTable[objIdx].genusId;
		if ((genusId == GENUS_PlayerProjectile || genusId == GENUS_NpcProjectile) &&
#ifdef XWA_MODERN
			laser_GetProjectileWarheadClass(g_objectTable[objIdx].objectType) > 0) {
#else
			g_projectileWarheadClassByType[g_objectTable[objIdx].objectType - OBJ_LaserRebel] != 0) {
#endif
			WarheadGuidanceState* guidance;

			guidance = g_objectTable[objIdx].mobj->pWarheadGuidance;
			if (guidance != NULL && guidance->targetObjIdx == g_players[g_localPlayer].objectIndex) {
				ObjectRecord* localPlayerObj;

				localPlayerObj = &g_objectTable[g_players[g_localPlayer].objectIndex];
				if ((unsigned int)collide_roughdistance3d(
						g_objectTable[objIdx].world_x - localPlayerObj->world_x,
						g_objectTable[objIdx].world_y - localPlayerObj->world_y,
						g_objectTable[objIdx].world_z - localPlayerObj->world_z) < 0x4000u &&
					Sound_CountPlayingInstances(g_sfxIds[62]) == 0) {
					fsfx_PlaySound(62, 0xffffu, (unsigned int)g_localPlayer);
				}
			}
		}
	}

	armedLockStrength = 0;
	for (objIdx = g_activeRegionObjectSlotStart; objIdx < g_activeRegionCraftObjectSlotEnd; ++objIdx) {
		int launcherIdx;

		if (g_objectTable[objIdx].objectType == OBJ_None) {
			continue;
		}

		mobj = g_objectTable[objIdx].mobj;
		if (mobj->state != 0) {
			continue;
		}

		craft = mobj->pCraft;
		if (craft == NULL || craft->workingSubsystems == 0 || craft->objectKind != 0) {
			continue;
		}

		if (g_objectTable[objIdx].playerOwnerIdx == -1) {
			AiController* aiController;

			aiController = pai_GetEffectiveAIController(craft);
			if (aiController->targetObjIdx != g_players[g_localPlayer].objectIndex ||
				aiController->maneuverMode != 23) {
				continue;
			}

			if ((int16_t)craft->warheadLockTicks > armedLockStrength) {
				ModelIndex modelIndex;

				modelIndex = GetModelIndexFromType((ObjectTypeId)g_objectTable[objIdx].objectType);
				if (modelIndex == (ModelIndex)0xffff) {
					continue;
				}

				for (launcherIdx = 0; launcherIdx < craft->warheadLauncherCount; ++launcherIdx) {
					int warheadSlot;
					int lastSlot;

					if (craft->warheadSlotTypeIds[launcherIdx] == 0) {
						continue;
					}

					lastSlot = g_modelDefs[modelIndex].warheadLauncherLastSlot[launcherIdx];
					for (warheadSlot = g_modelDefs[modelIndex].warheadLauncherFirstSlot[launcherIdx];
						 warheadSlot <= lastSlot; ++warheadSlot) {
						if (craft->warheadData[warheadSlot].count != 0) {
							armedLockStrength = (int16_t)craft->warheadLockTicks;
							launcherIdx = craft->warheadLauncherCount;
							warheadSlot = lastSlot + 1;
						}
					}
				}
			}

			DebugPrintfChannel(1, "MISSILE: mlock set to %d.\n", armedLockStrength);
		} else {
			if ((uint16_t)g_players[g_objectTable[objIdx].playerOwnerIdx].currentTargetObjectIdx !=
					g_players[g_localPlayer].objectIndex ||
				g_players[g_objectTable[objIdx].playerOwnerIdx].selectedWeaponMode == 0) {
				continue;
			}

			if ((int16_t)craft->warheadLockTicks > armedLockStrength) {
				for (launcherIdx = 0; launcherIdx < craft->warheadLauncherCount; ++launcherIdx) {
					ModelIndex modelIndex;
					int warheadSlot;
					int lastSlot;

					if (craft->warheadSlotTypeIds[launcherIdx] == 0) {
						continue;
					}

					modelIndex = GetModelIndexFromType((ObjectTypeId)g_objectTable[objIdx].objectType);
					if (modelIndex == (ModelIndex)0xffff) {
						continue;
					}

					lastSlot = g_modelDefs[modelIndex].warheadLauncherLastSlot[launcherIdx];
					for (warheadSlot = g_modelDefs[modelIndex].warheadLauncherFirstSlot[launcherIdx];
						 warheadSlot <= lastSlot; ++warheadSlot) {
						if (craft->warheadData[warheadSlot].count != 0) {
							armedLockStrength = (int16_t)craft->warheadLockTicks;
							launcherIdx = craft->warheadLauncherCount;
							warheadSlot = lastSlot + 1;
						}
					}
				}
			}
		}
	}

	if (armedLockStrength > 0) {
		if (armedLockStrength > 944) {
			warningState = 2;
		} else {
			warningState = (g_missionElapsedClock.subsecondTicks / 59) & 1;
		}

		g_incomingMissileWarningState = warningState;
		if (g_incomingMissileLockStrength == 0) {
			fsfx_speakorderack(g_localPlayer, -1, 25, 4, (unsigned int)g_players[g_localPlayer].objectIndex,
							   0xffffu);
			warningState = g_incomingMissileWarningState;
		}
	} else {
		warningState = 0;
		g_incomingMissileWarningState = 0;
	}

	g_incomingMissileLockStrength = armedLockStrength;
	fsfx_UpdateIncomingMissileWarning(warningState);
}

// FUNCTION: XWA 0x43CDF0
void fsfx_UpdateIncomingMissileWarning(int warningState) {
	int volume;
	int interiorVolume;

	if (!g_flightConfSfxEnabled) {
		return;
	}

	if (!g_gameConfig.sfxInteriorEnabled) {
		return;
	}

	if (!g_gameConfig.sfxInteriorVolume) {
		return;
	}

	if (warningState == 0) {
		if (Sound_CountPlayingInstances(g_sfxIds[63]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[63]);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[64]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[64]);
		}
		return;
	}

	interiorVolume = g_gameConfig.sfxInteriorVolume;
	if (interiorVolume >= 10) {
		volume = 127;
	} else {
		volume = 13 * interiorVolume;
	}

	if (warningState == 1) {
		if (Sound_CountPlayingInstances(g_sfxIds[64]) == 0) {
			Sound_QueueEffect(g_sfxIds[64], 1, 1, 125, volume, 64, -1, 0xffffu);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[63]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[63]);
		}
	} else {
		if (Sound_CountPlayingInstances(g_sfxIds[63]) == 0) {
			Sound_QueueEffect(g_sfxIds[63], 1, 1, 125, volume, 64, -1, 0xffffu);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[64]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[64]);
		}
	}
}

// FUNCTION: XWA 0x43CF30
void fsfx_UpdateChaffLoop(void) {
	int playerObjIdx;
	ObjectRecord* playerObj;
	CraftData* craft;
	int volume;
	int interiorVolume;

	if (g_flightSimSideEffectsSuppressed) {
		return;
	}
	if (!g_flightConfSfxEnabled) {
		return;
	}
	if (!g_gameConfig.sfxInteriorEnabled) {
		return;
	}
	if (!g_gameConfig.sfxInteriorVolume) {
		return;
	}

	playerObjIdx = g_players[g_localPlayer].objectIndex;
	if (playerObjIdx == 0xffff) {
		if (Sound_CountPlayingInstances(g_sfxIds[22]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[22]);
		}
		return;
	}

	playerObj = &g_objectTable[playerObjIdx];
	craft = playerObj->mobj->pCraft;
	if (craft->cmTypeId != 1) {
		return;
	}

	if (g_players[g_localPlayer].regionSessionId == 1) {
		if (Sound_CountPlayingInstances(g_sfxIds[22]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[22]);
		}
		return;
	}

	interiorVolume = g_gameConfig.sfxInteriorVolume;
	if (interiorVolume >= 10) {
		volume = 127;
	} else {
		volume = 13 * interiorVolume;
	}
	volume /= 4;

	if (craft->chaffActiveTimer != 0) {
		if (Sound_CountPlayingInstances(g_sfxIds[22]) == 0) {
			Sound_QueueEffect(g_sfxIds[22], 1, 1, 125, volume, 64, -1, 0xffffu);
		}
	} else if (Sound_CountPlayingInstances(g_sfxIds[22]) != 0) {
		Sound_StopOldestInstance(g_sfxIds[22]);
	}
}

// FUNCTION: XWA 0x43D090
void fsfx_UpdatePlayerEngineLoop(void) {
	int engineSlot;
	uint8_t useHighQualityStep;
	int objectIndex;
	int baseFrequency;
	ObjectRecord* objectTable;
	CraftData* craft;
	uint8_t objectType;

	if (g_flightSimSideEffectsSuppressed) {
		return;
	}
	if (!g_flightConfSfxEnabled) {
		return;
	}
	if (!g_gameConfig.sfxEngineEnabled) {
		return;
	}
	if (!g_gameConfig.sfxEngineVolume) {
		return;
	}

	engineSlot = -1;
	useHighQualityStep = 0;
	objectIndex = g_players[g_localPlayer].objectIndex;
	objectTable = g_objectTable;

	if (objectIndex != 0xffff) {
		objectType = (uint8_t)objectTable[objectIndex].objectType;
		switch (objectType) {
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
				engineSlot = 97;
				baseFrequency = 11000;
				if (g_gameConfig.sfxQuality) {
					useHighQualityStep = 1;
				}
				break;

			case 12:
			case 16:
			case 24:
			case 25:
			case 29:
				engineSlot = 98;
				baseFrequency = 11000;
				if (g_gameConfig.sfxQuality) {
					useHighQualityStep = 1;
				}
				break;

			case 1:
			case 4:
			case 14:
			case 15:
			case 31:
				engineSlot = 94;
				baseFrequency = 11000;
				if (g_gameConfig.sfxQuality) {
					useHighQualityStep = 1;
				}
				break;

			case 2:
			case 11:
			case 23:
				engineSlot = 95;
				baseFrequency = 11000;
				if (g_gameConfig.sfxQuality) {
					useHighQualityStep = 1;
				}
				break;

			case 3:
			case 10:
			case 13:
			case 27:
				engineSlot = 96;
				baseFrequency = 5500;
				break;

			case 26:
			case 32:
			case 59:
				engineSlot = 100;
				baseFrequency = 11000;
				if (g_gameConfig.sfxQuality) {
					useHighQualityStep = 1;
				}
				break;

			case 17:
			case 58:
			case 65:
				engineSlot = 99;
				baseFrequency = 11000;
				if (g_gameConfig.sfxQuality) {
					useHighQualityStep = 1;
				}
				break;
		}
	}

	if (engineSlot != -1) {
		uint16_t configVolume;
		unsigned int volume;
		int throttleStep;
		int frequency;

		*(uint8_t*)&g_playerEngineLoopObjectType = objectType;
		if (g_players[g_localPlayer].regionSessionId != 1) {
			craft = objectTable[objectIndex].mobj->pCraft;
			if ((craft->workingSubsystems & 0x40u) != 0) {

				configVolume = g_gameConfig.sfxEngineVolume;
				if (configVolume >= 10u) {
					configVolume = 127;
				} else {
					configVolume = (uint16_t)(13u * configVolume);
				}

				volume = ((uint32_t)configVolume * (uint32_t)g_fsfxBaseVolumeBySfxSlot[engineSlot]) >> 7;
				throttleStep = (int)MATH2_divide(craft->throttleSpeed, 0xffffu);
				throttleStep &= 0xffff;
				throttleStep = throttleStep / 655;
				if (useHighQualityStep) {
					frequency = baseFrequency + 220 * (uint16_t)throttleStep;
				} else {
					frequency = baseFrequency + 110 * (uint16_t)throttleStep;
				}

				volume = (uint16_t)volume;
				if (Sound_CountPlayingInstances(g_sfxIds[engineSlot]) == 0) {
					Sound_QueueEffect(g_sfxIds[engineSlot], 1, 1, 125, (int)volume, 64, frequency, 0xffffu);
					g_playerEngineLoopVolume = (int)volume;
					g_playerEngineLoopFrequency = frequency;
					return;
				}

				if (frequency != g_playerEngineLoopFrequency) {
					Sound_SetLatestInstanceFrequency(g_sfxIds[engineSlot], frequency);
				}
				if ((int)volume != g_playerEngineLoopVolume) {
					Sound_SetLatestInstanceVolume(g_sfxIds[engineSlot], (int)volume);
				}
				g_playerEngineLoopVolume = (int)volume;
				g_playerEngineLoopFrequency = frequency;
				return;
			}
		}

		if (Sound_CountPlayingInstances(g_sfxIds[engineSlot]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[engineSlot]);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[SFX_ENGINE_WASH_CAPITAL]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[SFX_ENGINE_WASH_CAPITAL]);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[SFX_ENGINE_WASH_OTHER]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[SFX_ENGINE_WASH_OTHER]);
		}
		return;
	}

	if (g_players[g_localPlayer].mapCameraState) {
		switch ((uint8_t)g_playerEngineLoopObjectType) {
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
				engineSlot = 97;
				break;

			case 12:
			case 16:
			case 24:
			case 25:
			case 29:
				engineSlot = 98;
				break;

			case 1:
			case 4:
			case 14:
			case 15:
			case 31:
				engineSlot = 94;
				break;

			case 2:
			case 11:
			case 23:
				engineSlot = 95;
				break;

			case 3:
			case 10:
			case 13:
			case 27:
				engineSlot = 96;
				break;

			case 26:
			case 32:
			case 59:
				engineSlot = 100;
				break;

			case 17:
			case 58:
			case 65:
				engineSlot = 99;
				break;
		}

#ifdef XWA_MODERN
		if (engineSlot == -1) {
			return;
		}
#endif

		if (Sound_CountPlayingInstances(g_sfxIds[engineSlot]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[engineSlot]);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[SFX_ENGINE_WASH_CAPITAL]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[SFX_ENGINE_WASH_CAPITAL]);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[SFX_ENGINE_WASH_OTHER]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[SFX_ENGINE_WASH_OTHER]);
		}
	}
}

// FUNCTION: XWA 0x43CAE0
void fsfx_UpdateBeamSystemLoop(int enabled, unsigned int playerIdx) {
	CraftData* craft;

	if (g_flightSimSideEffectsSuppressed) {
		return;
	}

	if (playerIdx != g_localPlayer || !g_flightConfSfxEnabled || !g_gameConfig.sfxInteriorEnabled ||
		!g_gameConfig.sfxInteriorVolume) {
		return;
	}

	craft = g_objectTable[g_players[g_localPlayer].objectIndex].mobj->pCraft;
	if (enabled && (craft->workingSubsystems & 0x100u) != 0) {
		unsigned int loopSlot;
		uint8_t beamTypeId;

		beamTypeId = craft->beamTypeId;
		if (beamTypeId == 1) {
			if (Sound_CountPlayingInstances(g_sfxIds[79]) != 0) {
				return;
			}
			loopSlot = 80;
		} else if (beamTypeId == 2) {
			if (Sound_CountPlayingInstances(g_sfxIds[82]) != 0) {
				return;
			}
			loopSlot = 83;
		} else if (beamTypeId == 3) {
			if (Sound_CountPlayingInstances(g_sfxIds[85]) == 0) {
				if (Sound_CountPlayingInstances(g_sfxIds[86]) == 0) {
					unsigned int volume = fsfx_ComputeSourceVolume(0xffffu, 86, &playerIdx);
					Sound_QueueEffect(g_sfxIds[86], 1, 1, 125, (int)volume, 64, -1, 0xffffu);
				}
			}
			return;
		} else {
			if (Sound_CountPlayingInstances(g_sfxIds[87]) == 0) {
				if (Sound_CountPlayingInstances(g_sfxIds[88]) == 0) {
					unsigned int volume = fsfx_ComputeSourceVolume(0xffffu, 88, &playerIdx);
					Sound_QueueEffect(g_sfxIds[88], 1, 1, 125, (int)volume, 64, -1, 0xffffu);
				}
			}
			return;
		}

		beamTypeId = craft->beamTypeId;
		if (beamTypeId != 1 && beamTypeId != 2) {
			if (Sound_CountPlayingInstances(g_sfxIds[loopSlot]) == 0) {
				unsigned int volume = fsfx_ComputeSourceVolume(0xffffu, loopSlot, &playerIdx);
				Sound_QueueEffect(g_sfxIds[loopSlot], 1, 1, 125, (int)volume, 64, -1, 0xffffu);
			}
			return;
		}

		if ((uint16_t)g_localBeamTargetObjIdx == 0xffffu) {
			if (Sound_CountPlayingInstances(g_sfxIds[loopSlot + 1]) != 0) {
				Sound_StopOldestInstance(g_sfxIds[loopSlot + 1]);
			}
			if (Sound_CountPlayingInstances(g_sfxIds[loopSlot]) == 0) {
				unsigned int volume = fsfx_ComputeSourceVolume(0xffffu, loopSlot, &playerIdx);
				Sound_QueueEffect(g_sfxIds[loopSlot], 1, 1, 125, (int)volume, 64, -1, 0xffffu);
			}
		} else {
			if (Sound_CountPlayingInstances(g_sfxIds[loopSlot]) != 0) {
				Sound_StopOldestInstance(g_sfxIds[loopSlot]);
			}
			if (Sound_CountPlayingInstances(g_sfxIds[loopSlot + 1]) == 0) {
				unsigned int volume = fsfx_ComputeSourceVolume(0xffffu, loopSlot + 1, &playerIdx);
				Sound_QueueEffect(g_sfxIds[loopSlot + 1], 1, 1, 125, (int)volume, 64, -1, 0xffffu);
			}
		}
	} else {
		int sfxSlot;

		for (sfxSlot = 79; sfxSlot <= 88; ++sfxSlot) {
			if (Sound_CountPlayingInstances(g_sfxIds[sfxSlot]) != 0) {
				Sound_StopOldestInstance(g_sfxIds[sfxSlot]);
			}
		}
	}
}

// FUNCTION: XWA 0x43D540
void fsfx_UpdateBeamEffectLoops(void) {
	int playerObjIdx;
	ObjectRecord* playerObj;
	CraftData* craft;
	int volume;
	int result;
	int interiorVolume;

	if (g_flightSimSideEffectsSuppressed) {
		return;
	}

	if (!g_flightConfSfxEnabled) {
		return;
	}

	if (!g_gameConfig.sfxInteriorEnabled) {
		return;
	}

	if (!g_gameConfig.sfxInteriorVolume) {
		return;
	}

	interiorVolume = g_gameConfig.sfxInteriorVolume;
	if (interiorVolume >= 10) {
		volume = 127;
	} else {
		volume = 13 * interiorVolume;
	}

	playerObjIdx = g_players[g_localPlayer].objectIndex;
	if (playerObjIdx == 0xffff) {
		if (Sound_CountPlayingInstances(g_sfxIds[90]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[90]);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[91]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[91]);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[92]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[92]);
		}

		result = Sound_CountPlayingInstances(g_sfxIds[93]);
		if (result != 0) {
			Sound_StopOldestInstance(g_sfxIds[93]);
		}
		return;
	}

	playerObj = &g_objectTable[playerObjIdx];
	craft = playerObj->mobj->pCraft;
	if (craft->beamEffectAccum[1] != 0) {
		if (Sound_CountPlayingInstances(g_sfxIds[90]) == 0) {
			Sound_QueueEffect(g_sfxIds[90], 1, 1, 125, volume, 64, -1, 0xffffu);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[91]) == 0 &&
			Sound_CountPlayingInstances(g_sfxIds[92]) == 0 &&
#ifdef XWA_MODERN
			(!XwaModernFlightTiming_IsHighRate() || XwaModernFlightTiming_IsLegacyCadenceDue()) &&
#endif
			(uint16_t)GameRand2() < 0x1000u) {
			Sound_QueueEffect(g_sfxIds[90 + (GameRand2() & 1u)], 1, 1, 125, volume, 64, -1, 0xffffu);
		}
	} else {
		if (Sound_CountPlayingInstances(g_sfxIds[90]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[90]);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[91]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[91]);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[92]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[92]);
		}
	}

	if (craft->beamEffectAccum[2] != 0) {
		result = Sound_CountPlayingInstances(g_sfxIds[93]);
		if (result == 0) {
			Sound_QueueEffect(g_sfxIds[93], 1, 1, 125, volume / 2, 64, -1, 0xffffu);
		}
		return;
	}

	result = Sound_CountPlayingInstances(g_sfxIds[93]);
	if (result != 0) {
		Sound_StopOldestInstance(g_sfxIds[93]);
	}
}

// FUNCTION: XWA 0x43D7C0
void fsfx_UpdateFlightSfx(void) {
	int playerObjIdx;
	uint32_t sourceObjIdx;

	if (!g_fsfxLoaded) {
		return;
	}

	playerObjIdx = g_players[g_localPlayer].objectIndex;
	if (playerObjIdx == 0xffff) {
		return;
	}

	fsfx_UpdateChaffLoop();
	fsfx_UpdatePlayerEngineLoop();
	fsfx_UpdateBeamEffectLoops();

	if (g_flightSimSideEffectsSuppressed) {
		return;
	}

	if (!g_flightConfSfxEnabled) {
		return;
	}

	if (!g_gameConfig.sfxEngineEnabled) {
		return;
	}

	if (!g_gameConfig.sfxEngineVolume) {
		return;
	}

	if ((uint16_t)g_players[g_localPlayer].engineWashSourceObjIdx != 0xffffu) {
		uint16_t engineWashObjIdx;
		ObjectTypeId objectType;
		int sfxSlot;
		int washVolume;
		int scaledVolume;

		engineWashObjIdx = (uint16_t)g_players[g_localPlayer].engineWashSourceObjIdx;
		objectType = g_objectTable[engineWashObjIdx].objectType;
		if (objectType == OBJ_Interdictor2 || objectType == OBJ_VictoryStarDestroyer2 ||
			objectType == OBJ_ImperialStarDestroyer2 || objectType == OBJ_SuperStarDestroyer) {
			sfxSlot = SFX_ENGINE_WASH_CAPITAL;
		} else {
			sfxSlot = SFX_ENGINE_WASH_OTHER;
		}

		washVolume = 16 * g_players[g_localPlayer].engineWashStrength;
		if (washVolume > 127) {
			washVolume = 127;
		}
		scaledVolume = (int)g_gameConfig.sfxExteriorVolume * washVolume / 10;
		if (scaledVolume > 127) {
			scaledVolume = 127;
		}

		if (Sound_CountPlayingInstances(g_sfxIds[sfxSlot]) == 0) {
			msg_emitInFlightMessage(MSG_ENGINE_WASH_DAMAGE, g_localPlayer);
			Sound_QueueEffect(g_sfxIds[sfxSlot], 1, 1, scaledVolume, 65, 64, -1, 0xffffu);
		} else {
			Sound_SetLatestInstanceVolume(g_sfxIds[sfxSlot], scaledVolume);
		}
	} else {
		if (Sound_CountPlayingInstances(g_sfxIds[SFX_ENGINE_WASH_CAPITAL]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[SFX_ENGINE_WASH_CAPITAL]);
		}
		if (Sound_CountPlayingInstances(g_sfxIds[SFX_ENGINE_WASH_OTHER]) != 0) {
			Sound_StopOldestInstance(g_sfxIds[SFX_ENGINE_WASH_OTHER]);
		}
	}

	for (sourceObjIdx = g_activeRegionObjectSlotStart; sourceObjIdx < g_activeRegionCraftObjectSlotEnd;
		 ++sourceObjIdx) {
		ObjectRecord* sourceObj;
		MobileObject* sourceMobj;
		CraftData* sourceCraft;
		unsigned int objectType;
		uint16_t sfxSlot;
		int prevX;
		int prevY;
		int prevZ;
		int currentDistance;
		int previousDistance;
		int threshold;

		if ((int)sourceObjIdx == playerObjIdx) {
			continue;
		}

		sourceObj = &g_objectTable[sourceObjIdx];
		if (sourceObj->objectType == OBJ_None) {
			continue;
		}

		sourceMobj = sourceObj->mobj;
		sourceCraft = sourceMobj->pCraft;
		if (sourceCraft->objectKind != GENUS_Fighter || sourceCraft->workingSubsystems == 0 ||
			sourceMobj->speed == 0) {
			continue;
		}

		sfxSlot = 0xffffu;
		objectType = (uint16_t)sourceObj->objectType;
		switch (objectType) {
			case OBJ_TIEFighter:
			case OBJ_TIEInterceptor:
			case OBJ_TIEBomber:
			case OBJ_TIEAdvanced:
			case OBJ_TIEDefender:
				if (g_sound3DEnabled && g_sfxIds[102] != -1) {
					sfxSlot = 102u;
				} else {
					sfxSlot = 101u;
				}
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
				if (g_sound3DEnabled && g_sfxIds[104] != -1) {
					sfxSlot = 104u;
				} else {
					sfxSlot = 103u;
				}
				break;

			case OBJ_XWing:
			case OBJ_BWing:
			case OBJ_Z95:
			case OBJ_R41:
			case OBJ_SlaveOne:
				if (g_sound3DEnabled && g_sfxIds[106] != -1) {
					sfxSlot = 106u;
				} else {
					sfxSlot = 105u;
				}
				break;

			case OBJ_YWing:
			case OBJ_ToscanFighter:
			case OBJ_CloakshapeFighter:
				if (g_sound3DEnabled && g_sfxIds[108] != -1) {
					sfxSlot = 108u;
				} else {
					sfxSlot = 107u;
				}
				break;

			case OBJ_AWing:
			case OBJ_IrdFighter:
			case OBJ_Twing:
			case OBJ_Piggyback:
				if (g_sound3DEnabled && g_sfxIds[110] != -1) {
					sfxSlot = 110u;
				} else {
					sfxSlot = 109u;
				}
				break;

			case OBJ_SkiprayBlastBoat:
			case OBJ_SupaFighter:
			case OBJ_SlaveTwo:
			case OBJ_CorellianTransport2:
			case OBJ_MilleniumFalcon2:
			case OBJ_FamilyTransport:
				if (g_sound3DEnabled && g_sfxIds[112] != -1) {
					sfxSlot = 112u;
				} else {
					sfxSlot = 111u;
				}
				break;
		}
		if (sfxSlot == 0xffffu) {
			continue;
		}

		if (sourceObj->playerOwnerIdx != -1) {
			prevX = g_objPrevX[sourceObj->playerOwnerIdx];
			prevY = g_objPrevY[sourceObj->playerOwnerIdx];
			prevZ = g_objPrevZ[sourceObj->playerOwnerIdx];
			g_objPrevX[sourceObj->playerOwnerIdx] = sourceObj->world_x;
			g_objPrevY[sourceObj->playerOwnerIdx] = sourceObj->world_y;
			g_objPrevZ[sourceObj->playerOwnerIdx] = sourceObj->world_z;
		} else {
			prevX = sourceMobj->prevWorldX;
			prevY = sourceMobj->prevWorldY;
			prevZ = sourceMobj->prevWorldZ;
		}

		currentDistance = collide_roughdistance3d(sourceObj->world_x - g_objectTable[playerObjIdx].world_x,
												  sourceObj->world_y - g_objectTable[playerObjIdx].world_y,
												  sourceObj->world_z - g_objectTable[playerObjIdx].world_z);
		previousDistance =
			collide_roughdistance3d(prevX - g_objPrevX[g_localPlayer], prevY - g_objPrevY[g_localPlayer],
									prevZ - g_objPrevZ[g_localPlayer]);
		if (g_sound3DEnabled) {
			threshold = g_modelTypeTable[objectType].maxBoundsExtent + 2048;
		} else {
			threshold = g_modelTypeTable[objectType].maxBoundsExtent + 1024;
		}

		if (currentDistance < threshold && previousDistance >= threshold) {
			fsfx_PlaySound((int)sfxSlot, (uint16_t)sourceObjIdx, (unsigned int)g_localPlayer);
		}
	}

	g_objPrevX[g_localPlayer] = g_objectTable[playerObjIdx].world_x;
	g_objPrevY[g_localPlayer] = g_objectTable[playerObjIdx].world_y;
	g_objPrevZ[g_localPlayer] = g_objectTable[playerObjIdx].world_z;
}

// FUNCTION: XWA 0x43DD10
void fsfx_UpdateNearbyWeaponLoop(void) {
	unsigned int closestRangeScore;
	uint8_t stopLoop;
	uint8_t genusId;
	unsigned int objIdx;
	unsigned int minDistance;

	closestRangeScore = 0x07d00000u;
	stopLoop = 1;
	if (g_nextNearbyWeaponSfxScanTime > g_gameTime) {
		return;
	}
	if (!g_flightSideEffectsEnabled) {
		return;
	}

	genusId = g_modelTypeTable[OBJ_HyperBuoy].genusId;
	g_objectSlotRangeByGenus[genusId].next = g_activeRegionObjectSlotStart;
	g_objectSlotRangeByGenus[genusId].end = g_activeRegionCraftObjectSlotEnd;

	for (objIdx = g_objectSlotRangeByGenus[genusId].next; objIdx < g_objectSlotRangeByGenus[genusId].end;
		 ++objIdx) {
		if ((g_modelTypeTable[(uint16_t)g_objectTable[objIdx].objectType].flags & 4u) == 0) {
			continue;
		}

		pai_ObjectRefUpdateApproxRangeScore(g_players[g_localPlayer].objectIndex, objIdx);
		if ((unsigned int)g_targetRangeScore < g_fsfxMinDistanceOrRolloffBySfxSlot[134]) {
			unsigned int priority;
			unsigned int volume;

			volume = fsfx_ComputeSourceVolume(objIdx, 134u, &priority);
			if (volume != 0) {
				int soundPan;

#ifdef XWA_MODERN
				soundPan = 64;
#endif
				if (!g_sound3DEnabled) {
					soundPan = fsfx_ComputeSourcePan(objIdx, &volume);
				}

				if (priority >= 125u) {
					priority = 124;
				} else {
					priority = volume;
				}

				if (Sound_CountPlayingInstances(g_sfxIds[134]) == 0) {
					Sound_QueueEffect(g_sfxIds[134], 1, 1, (int)priority, (int)volume, soundPan, 0, objIdx);
				} else {
					Sound_SetLatestInstanceVolume(g_sfxIds[134], (int)volume);
					Sound_SetLatestInstancePan(g_sfxIds[134], soundPan);
					Sound_SetEffectCurrentPriority(g_sfxIds[134], (int)priority);
				}
				stopLoop = 0;
			}
		}

		if ((unsigned int)g_targetRangeScore < closestRangeScore) {
			closestRangeScore = (unsigned int)g_targetRangeScore;
		}
	}

	if (stopLoop && Sound_CountPlayingInstances(g_sfxIds[134]) != 0) {
		Sound_StopOldestInstance(g_sfxIds[134]);
	}

	minDistance = g_fsfxMinDistanceOrRolloffBySfxSlot[134];
	if (closestRangeScore < 2u * minDistance) {
		if (closestRangeScore < (minDistance >> 2)) {
			g_nextNearbyWeaponSfxScanTime = g_gameTime + 118;
		} else {
			g_nextNearbyWeaponSfxScanTime = g_gameTime + 236;
		}
	} else {
		g_nextNearbyWeaponSfxScanTime = g_gameTime + 944;
	}
}

// FUNCTION: XWA 0x43BCB0
int fsfx_PickRandomSmallExplosionSfx(void) {
	uint16_t choice;

	if (g_fsfxSmallExplosionRemainingChoices <= 0) {
		g_fsfxSmallExplosionRemainingChoices = 8;
		memset(g_fsfxSmallExplosionUsedFlags, 0, sizeof(g_fsfxSmallExplosionUsedFlags));
	}

	do {
		choice = (uint16_t)((GameRand2() & 0xffff) % 8);
	} while (g_fsfxSmallExplosionUsedFlags[choice] != 0);

	g_fsfxSmallExplosionUsedFlags[choice] = 1;
	g_fsfxSmallExplosionRemainingChoices -= 1;
	return (int)choice + 25;
}

// FUNCTION: XWA 0x43F200
int fsfx_SelectTacOfficerObjectVoiceVariant(unsigned int objIdx) {
	unsigned int voiceVariant;

	if (objIdx == 0xffffu || objIdx >= g_activeRegionCraftObjectSlotEnd) {
		return 255;
	}

	voiceVariant = g_fsfxDesignationToVoiceVariant
		[g_missionFlightRuntimeState.teamFgDesignationCode[(uint16_t)g_players[g_localPlayer].playerIff]
														  [g_objectTable[objIdx].flightGroupIdx]];

	if (voiceVariant == 4) {
		if (StrCmpI(g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.name, "defiance") == 0) {
			voiceVariant = 0;
		} else if (StrCmpI(g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.name, "liberty") ==
				   0) {
			voiceVariant = 1;
		} else {
			voiceVariant =
				StrCmpI(g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.name, "independence")
					? 4
					: 2;
		}
	}

	if (voiceVariant == 7 && (GameRand2() & 0x1000) != 0) {
		voiceVariant = 8;
	}

	if (Object_IsHostileToTeam((uint16_t)objIdx, (uint16_t)g_players[g_localPlayer].playerIff) &&
		voiceVariant <= 11u) {
		voiceVariant = g_fsfxHostileVoiceVariantRemap[voiceVariant];
	}

	if ((GameRand2() & 0x1000) != 0) {
		int replacementIndex;

		for (replacementIndex = 0;
			 g_fsfxRandomVoiceVariantReplacementPairs[replacementIndex].fromVariant != -1;
			 ++replacementIndex) {
			if (g_fsfxRandomVoiceVariantReplacementPairs[replacementIndex].fromVariant == (int)voiceVariant) {
				voiceVariant =
					(unsigned int)g_fsfxRandomVoiceVariantReplacementPairs[replacementIndex].toVariant;
				break;
			}
		}
	}

	return (int)voiceVariant;
}

// FUNCTION: XWA 0x43F390
int fsfx_SpeakTacticalOfficerEvent(int voiceCategory, int messageId, unsigned int objIdx,
								   unsigned int probability) {
	uint16_t objectSerial;

	if (g_gameConfig.voiceTacticalOfficerEnabled == 0) {
		return 0;
	}

	objectSerial = 0xffffu;
	if (g_players[g_localPlayer].objectIndex == 0xffff) {
		return 0;
	}

	if ((uint16_t)probability != objectSerial && (uint16_t)GameRand2() >= (uint16_t)probability) {
		return 0;
	}

	if (voiceCategory == 4) {
		probability = (unsigned int)fsfx_SelectTacOfficerObjectVoiceVariant(objIdx);
		if (probability == 255) {
			return 0;
		}

		objectSerial = g_objectTable[objIdx].objectSignature;
		{
			int nowSeconds;

			nowSeconds = Mission_GameTimeToSeconds(g_missionElapsedClock.hours, g_missionElapsedClock.minutes,
												   g_missionElapsedClock.seconds);
			if (messageId != 89 && messageId != 93) {
				int lastSpeakSeconds;

				lastSpeakSeconds = g_fsfxTacOfficerLastSpeakSecondsByObj[objIdx];
				if (lastSpeakSeconds != 0 && (unsigned int)(nowSeconds - lastSpeakSeconds) <= 10u) {
					return 0;
				}
				g_fsfxTacOfficerLastSpeakSecondsByObj[objIdx] = nowSeconds;
			}
		}
	}

	if (voiceCategory != 4) {
		fsfx_QueueVoiceSfx(messageId + 2646, 2, (uint8_t)voiceCategory, 0, (int16_t)objectSerial,
						   (int)objIdx);
		return 1;
	}

	fsfx_QueueVoiceSfx((int)probability + 2646, 2, 4, 0, (int16_t)objectSerial, (int)objIdx);
	fsfx_QueueVoiceSfx(messageId + 2646, 2, 4, 1, (int16_t)objectSerial, (int)objIdx);

	return 1;
}

// FUNCTION: XWA 0x43BEA0
char fsfx_ShouldSuppressFlightSfx(unsigned int sfxSlot, int playerIdx) {
	if (g_flightSfxSideEffectGate != 0) {
		if (g_flightSimSideEffectsSuppressed == 0) {
			return 1;
		}
		if (g_flightSfxSideEffectGate == 2) {
			return 1;
		}
	} else if (g_flightSimSideEffectsSuppressed != 0) {
		return 1;
	}

	if (playerIdx != g_localPlayer) {
		return 1;
	}
	if (g_flightSideEffectsEnabled == 0) {
		return 1;
	}
	if (g_flightConfSfxEnabled == 0) {
		return 1;
	}
	if (g_sfxIds[sfxSlot] == -1) {
		return 1;
	}

	if (sfxSlot >= 121u && sfxSlot <= 125u && g_inHangarReady == 0) {
		ObjectIndex playerObjectIdx;
		ObjectRecord* playerObject;

		playerObjectIdx = g_players[playerIdx].objectIndex;
		if ((uint16_t)playerObjectIdx == 0xffffu) {
			return 1;
		}

		playerObject = &g_objectTable[playerObjectIdx];
		if (playerObject->objectType != OBJ_XWing && playerObject->objectType != OBJ_YWing) {
			ObjectIndex carriedObjectIdx;

			carriedObjectIdx = playerObject->mobj->pCraft->carriedObjectIndex;
			if ((uint16_t)carriedObjectIdx == 0xffffu) {
				return 1;
			}
			if (g_objectTable[carriedObjectIdx].objectType != OBJ_R2D2) {
				return 1;
			}
		}
	}

	return 0;
}

// FUNCTION: XWA 0x43BD20
int fsfx_PlaySfxAtWorldPosition(unsigned int sfxSlot, float pitchScale, int worldX, int worldY, int worldZ,
								int playerIdx) {
	int volume;
	int pan = 64;
	int pitch;

	if (fsfx_ShouldSuppressFlightSfx(sfxSlot, playerIdx)) {
		return 0;
	}
	if (!g_gameConfig.sfxExteriorEnabled) {
		return 0;
	}
	if (!g_gameConfig.sfxExteriorVolume) {
		return 0;
	}

	volume = (int)fsfx_ComputeDistanceVolume(worldX - g_players[g_localPlayer].viewState.savedTargetX,
											 worldY - g_players[g_localPlayer].viewState.savedTargetY,
											 worldZ - g_players[g_localPlayer].viewState.savedTargetZ,
											 sfxSlot, (unsigned int*)&playerIdx);
	if (volume != 0) {
		pan = fsfx_ComputeSoftwarePan(worldX - g_players[g_localPlayer].viewState.savedTargetX,
									  worldY - g_players[g_localPlayer].viewState.savedTargetY,
									  worldZ - g_players[g_localPlayer].viewState.savedTargetZ,
									  (unsigned int*)&volume);
		if (volume != 0) {
			if ((unsigned int)playerIdx >= 125u) {
				playerIdx = 124;
			} else {
				playerIdx = volume;
			}

			if (sfxSlot >= 101u && sfxSlot <= 114u) {
				if (Sound_CountPlayingInstances(g_sfxIds[sfxSlot]) != 0) {
					return 0;
				}
			}

			if (g_gameConfig.sfxQuality) {
				pitch = (int)(pitchScale * g_fsfxHighQualityPitchFrequency);
			} else {
				pitch = (int)(pitchScale * g_fsfxLowQualityPitchFrequency);
			}
			Sound_QueueEffectAtWorldPosition(g_sfxIds[sfxSlot], 1, 0, playerIdx, volume, pan, pitch, worldX,
											 worldY, worldZ);
		}
	}
	return 1;
}

// FUNCTION: XWA 0x43BF90
int fsfx_PlaySound(int sfxSlot, unsigned int sourceObjOrPointRef, unsigned int playerIdx) {
	unsigned int sourceRef;
	unsigned int priority;
	unsigned int volume;
	int pan;

	if (fsfx_ShouldSuppressFlightSfx((unsigned int)sfxSlot, (int)playerIdx)) {
		return 0;
	}

	sourceRef = sourceObjOrPointRef;
	if (sourceObjOrPointRef == 0xffffu) {
		if (!g_gameConfig.sfxInteriorEnabled) {
			return 0;
		}
		if (!g_gameConfig.sfxInteriorVolume) {
			return 0;
		}
	} else {
		if (!g_gameConfig.sfxExteriorEnabled) {
			return 0;
		}
		if (!g_gameConfig.sfxExteriorVolume) {
			return 0;
		}
	}

	if (sfxSlot == 116 || sfxSlot == 118 || sfxSlot == 120) {
		Sound_StopOldestInstance(g_sfxIds[sfxSlot]);
	}

	volume = fsfx_ComputeSourceVolume(sourceRef, (unsigned int)sfxSlot, &priority);
	if (volume == 0) {
		return 1;
	}

	pan = (int)sourceObjOrPointRef;
	if (!g_sound3DEnabled) {
		pan = fsfx_ComputeSourcePan(sourceRef, &volume);
	}

	if (priority >= 125u) {
		priority = 124;
	} else {
		priority = volume;
	}

	if (sourceRef == 0xffffu || g_objectTable[sourceRef].playerOwnerIdx == g_localPlayer) {
		priority = (sfxSlot == 61) ? 127u : 125u;
	} else {
		ObjectRecord* sourceObj;
		MobileObject* sourceMobileObj;

		sourceObj = &g_objectTable[sourceRef];
		sourceMobileObj = sourceObj->mobj;
		if (sourceMobileObj != NULL && (uint16_t)sourceMobileObj->sourceObjIdx != 0xffffu &&
			g_objectTable[(uint16_t)sourceMobileObj->sourceObjIdx].playerOwnerIdx == g_localPlayer) {
			priority = 125;
		}
	}

	if ((unsigned int)sfxSlot >= 101u && (unsigned int)sfxSlot <= 114u) {
		if (Sound_CountPlayingInstances(g_sfxIds[sfxSlot]) != 0) {
			return 0;
		}
	}

	if ((unsigned int)sfxSlot >= 50u && (unsigned int)sfxSlot <= 51u) {
		int pitch;

		if (g_gameConfig.sfxQuality) {
			pitch = ((int16_t)GameRand2() % 3300) + 22000;
		} else {
			pitch = ((int16_t)GameRand2() % 1650) + 11000;
		}
		Sound_QueueEffect(g_sfxIds[sfxSlot], 1, 0, (int)priority, (int)volume, pan, pitch, sourceRef);
	} else {
		Sound_QueueEffect(g_sfxIds[sfxSlot], 1, 0, (int)priority, (int)volume, pan, -1, sourceRef);
	}

	return 1;
}

// FUNCTION: XWA 0x43C180
int fsfx_triggerweaponsfx(unsigned int sourceObjIdx, unsigned int playerIdx) {
	ObjectRecord* sourceObj;
	ObjectTypeId objectType;
	uint16_t shooterObjectType;

	if (playerIdx != (unsigned int)g_localPlayer) {
		return 0;
	}
	if (!g_flightConfSfxEnabled) {
		return 0;
	}
	if (!g_gameConfig.sfxExteriorEnabled) {
		return 0;
	}
	if (!g_gameConfig.sfxExteriorVolume) {
		return 0;
	}

	sourceObj = &g_objectTable[sourceObjIdx];
	objectType = sourceObj->objectType;

	switch (objectType) {
		case OBJ_LaserRebel:
		case OBJ_LaserRebelTurbo:
		case OBJ_LaserImperial:
		case OBJ_LaserImperialTurbo:
		case OBJ_LaserIon:
		case OBJ_LaserIonTurbo:
		case OBJ_WarheadTorpedo:
		case OBJ_WarheadMissile:
		case OBJ_WarheadIon: {
			unsigned int projectileSfxBase;

			projectileSfxBase = objectType - OBJ_LaserRebel;
			shooterObjectType = (uint16_t)sourceObj->mobj->sourceObjectType;
			if (shooterObjectType == 58u || shooterObjectType == 65u || shooterObjectType == 59u) {
				if (objectType == OBJ_LaserRebel) {
					projectileSfxBase = 11;
					return fsfx_PlaySound((int)projectileSfxBase + 4, (uint16_t)sourceObjIdx, playerIdx);
				}
				if (objectType == OBJ_LaserRebelTurbo) {
					projectileSfxBase = 12;
					return fsfx_PlaySound((int)projectileSfxBase + 4, (uint16_t)sourceObjIdx, playerIdx);
				}
			} else if (((shooterObjectType >= 5u && shooterObjectType <= 9u) ||
						(shooterObjectType >= 18u && shooterObjectType <= 22u)) &&
					   g_projectileWarheadClassByType[(uint16_t)objectType - OBJ_LaserRebel] == 0) {
				projectileSfxBase = 16;
				return fsfx_PlaySound((int)projectileSfxBase + 4, (uint16_t)sourceObjIdx, playerIdx);
			}
			return fsfx_PlaySound((int)projectileSfxBase + 4, (uint16_t)sourceObjIdx, playerIdx);
		}

		case OBJ_WarheadLaser1:
		case OBJ_WarheadLaser3:
		case OBJ_LaserRebelTurbo_301:
		case OBJ_LaserRebelTurbo_302:
			return fsfx_PlaySound(12, (uint16_t)sourceObjIdx, playerIdx);

		case OBJ_WarheadLaser2:
		case OBJ_LaserImperialTurbo_303:
		case OBJ_LaserImperialTurbo_304:
		case OBJ_LaserImperialTurbo_305:
			return fsfx_PlaySound(13, (uint16_t)sourceObjIdx, playerIdx);

		case OBJ_WarheadAdvancedTorpedo:
		case OBJ_WarheadAdvancedMissile:
			return fsfx_PlaySound((int)objectType - 281, (uint16_t)sourceObjIdx, playerIdx);

		case OBJ_WarheadSpaceBomb:
		case OBJ_WarheadRocket:
			return fsfx_PlaySound((int)objectType - 276, (uint16_t)sourceObjIdx, playerIdx);

		case OBJ_WarheadMagPulse:
		case OBJ_WarheadIonPulse:
		case OBJ_WarheadFlare:
			return fsfx_PlaySound(19, (uint16_t)sourceObjIdx, playerIdx);

		default:
			return (int)objectType;
	}
}

// FUNCTION: XWA 0x43C350
unsigned int fsfx_ComputeSourceVolume(unsigned int sourceObjOrPointRef, unsigned int sfxSlot,
									  unsigned int* outPriority) {
	unsigned int interiorVolume;
	unsigned int baseVolume;
	unsigned int result;

	if (sourceObjOrPointRef == 0xffffu) {
		interiorVolume = g_gameConfig.sfxInteriorVolume;
		if (interiorVolume >= 10u) {
			interiorVolume = 127;
		} else {
			interiorVolume = 13u * interiorVolume;
		}

		if (sfxSlot < 196u) {
			baseVolume = g_fsfxBaseVolumeBySfxSlot[sfxSlot];
		} else {
			baseVolume = g_fsfxDefaultBaseVolume;
		}

		result = interiorVolume * baseVolume / 127u;
		*outPriority = result;
		return result;
	}

	return fsfx_ComputeDistanceVolume(
		g_objectTable[sourceObjOrPointRef].world_x - g_players[g_localPlayer].viewState.savedTargetX,
		g_objectTable[sourceObjOrPointRef].world_y - g_players[g_localPlayer].viewState.savedTargetY,
		g_objectTable[sourceObjOrPointRef].world_z - g_players[g_localPlayer].viewState.savedTargetZ, sfxSlot,
		outPriority);
}

// FUNCTION: XWA 0x43C410
unsigned int fsfx_ComputeDistanceVolume(int dx, int dy, int dz, unsigned int sfxSlot,
										unsigned int* outPriority) {
	unsigned int minDistance;
	unsigned int baseVolume;
	unsigned int distance;
	unsigned int result;

	if (sfxSlot >= 196u) {
		minDistance = 0x2000;
		baseVolume = 112;
	} else {
		minDistance = g_fsfxMinDistanceOrRolloffBySfxSlot[sfxSlot];
		baseVolume = g_fsfxBaseVolumeBySfxSlot[sfxSlot];
	}

	distance = (unsigned int)collide_roughdistance3d(dx, dy, dz);
	if (g_sound3DEnabled) {
		unsigned int remainingSound3DDistance;
		unsigned int maxSound3DDistance;

		maxSound3DDistance = 2 * g_fsfxMinDistanceOrRolloffBySfxSlot[sfxSlot];
		remainingSound3DDistance = g_fsfxMinDistanceOrRolloffBySfxSlot[sfxSlot];
		if (distance > maxSound3DDistance) {
			return 0;
		}

		remainingSound3DDistance = 2 * remainingSound3DDistance - distance;
		*outPriority = baseVolume * remainingSound3DDistance / maxSound3DDistance;
		return baseVolume;
	}

	if ((distance >> 2) >= minDistance) {
		return 0;
	}
	if ((distance >> 1) >= minDistance) {
		return baseVolume >> 3;
	}
	if (distance >= minDistance) {
		return baseVolume >> 2;
	}

	result = (baseVolume >> 2) +
			 (minDistance - distance) * (baseVolume - (baseVolume >> 2)) / (minDistance - (minDistance >> 5));
	if (g_gameConfig.sfxExteriorVolume != 10) {
		result = (unsigned int)g_gameConfig.sfxExteriorVolume * result / 10;
	}
	if (result > 127) {
		result = 127;
	}

	*outPriority = result;
	return result;
}

static __inline int fsfx_ComputeCameraDotQ15(int x, int y, int z, int rowX, int rowY, int rowZ) {
#ifdef XWA_MODERN
	x = (int32_t)((uint32_t)x * (uint32_t)rowX + (uint32_t)y * (uint32_t)rowY + (uint32_t)z * (uint32_t)rowZ);
#else
	x = x * rowX + y * rowY + z * rowZ;
#endif
	if (x >= 0x40000000) {
		x = 0x3fffffff;
	}
	if (x <= -0x40000000) {
		x = -0x3fff0000;
	}
	return x >> 15;
}

static __inline int16_t fsfx_ComputeListenerAngle(const int listener[2]) {
	return trig2_arctan((int16_t)listener[1], (int16_t)listener[0]);
}

#ifndef XWA_MODERN
#pragma optimize("y", off)
#endif
// FUNCTION: XWA 0x43C5C0
int fsfx_ComputeSoftwarePan(int dx, int dy, int dz, unsigned int* volumeInOut) {
	int16_t panAngle;
	int listener[2];

	if (g_sound3DEnabled) {
		return 64;
	}

	dx = (int16_t)dx;
	dy = (int16_t)dy;
	dz = (int16_t)dz;
	listener[1] = fsfx_ComputeCameraDotQ15(dx, dy, dz, g_camMatR0_X, g_camMatR0_Y, g_camMatR0_Z);

	listener[0] = fsfx_ComputeCameraDotQ15(dx, dy, dz, g_camMatR2_X, g_camMatR2_Y, g_camMatR2_Z);
	panAngle = fsfx_ComputeListenerAngle(listener);

	if (panAngle >= 0x4000 || panAngle <= -0x4000) {
		int16_t verticalAngle;
		int16_t rearAngle;
		int16_t rearScale;
		int16_t reduction;

		listener[1] = fsfx_ComputeCameraDotQ15(dx, dy, dz, g_camMatR1_X, g_camMatR1_Y, g_camMatR1_Z);
		verticalAngle = -32768 - fsfx_ComputeListenerAngle(listener);
		panAngle = -32768 - panAngle;
		rearAngle = panAngle;
		if (verticalAngle < 0) {
			verticalAngle = -verticalAngle;
		}
		if (panAngle < 0) {
			rearAngle = -rearAngle;
		}

		rearScale = 0x4000 - rearAngle;
		reduction = 0x4000 - verticalAngle;
		rearScale >>= 8;
		reduction >>= 8;
		reduction *= rearScale;
		reduction /= 64;
		reduction *= (int16_t)*volumeInOut;
		reduction /= 128;
		*volumeInOut -= reduction;
	}

	panAngle >>= 7;
	if (panAngle < -64) {
		panAngle = -64;
	}
	if (panAngle > 63) {
		panAngle = 63;
	}
	return (int16_t)(panAngle + 64);
}
#ifndef XWA_MODERN
#pragma optimize("y", on)
#endif

// FUNCTION: XWA 0x43C510
int fsfx_ComputeSourcePan(unsigned int sourceObjOrPointRef, unsigned int* volumeInOut) {
	MobileObject* sourceMobileObj;
	int dx;
	int dy;
	int dz;

	if (sourceObjOrPointRef == 0xffffu) {
		return 64;
	}

	sourceMobileObj = g_objectTable[sourceObjOrPointRef].mobj;
	if (sourceMobileObj != NULL && g_inHangarReady == 0) {
		dx = sourceMobileObj->prevWorldX - g_players[g_localPlayer].viewState.savedTargetX;
		dy = sourceMobileObj->prevWorldY - g_players[g_localPlayer].viewState.savedTargetY;
		dz = sourceMobileObj->prevWorldZ;
	} else {
		Mission_ResolveObjectOrMissionPointWorldLoc(sourceObjOrPointRef, 0, 0, 0);
		dx = worldlocx - g_players[g_localPlayer].viewState.savedTargetX;
		dy = worldlocy - g_players[g_localPlayer].viewState.savedTargetY;
		dz = worldlocz;
	}

	dz -= g_players[g_localPlayer].viewState.savedTargetZ;
	return fsfx_ComputeSoftwarePan(dx, dy, dz, volumeInOut);
}

// FUNCTION: XWA 0x43BC50
void fsfx_StopHyperZoomImp(unsigned int playerIdx) {
	if (g_flightSimSideEffectsSuppressed) {
		return;
	}

	if (playerIdx != (unsigned int)g_localPlayer) {
		return;
	}

	if (!g_flightConfSfxEnabled) {
		return;
	}

	if (!g_gameConfig.sfxInteriorEnabled) {
		return;
	}

	if (!g_gameConfig.sfxInteriorVolume) {
		return;
	}

	if (Sound_CountPlayingInstances(g_sfxIds[116])) {
		Sound_StopOldestInstance(g_sfxIds[116]);
	}
}
