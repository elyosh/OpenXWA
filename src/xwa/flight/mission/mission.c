#include "xwa/flight/mission/mission.h"
#include "xwa/flight/fediskio.h"
#include "xwa/flight/hangar.h"

#include "xwa/flight/ai/pai.h"
#include "xwa/flight/ai/pai_plan.h"
#include "xwa/flight/ai/paifight.h"
#include "xwa/flight/ai/paiman.h"
#include "xwa/flight/ai/paiorder.h"
#include "xwa/assets/linez.h"
#include "xwa/assets/model_bounds.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/model_type.h"
#include "xwa/audio/music.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/object/collision.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/flight_light.h"
#include "xwa/audio/fsfx.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/flight/hud/hud.h"
#include "xwa/math/trig2.h"
#include "xwa/flight/yard.h"
#include "xwa/flight/film.h"
#include "xwa/net/net.h"
#include "xwa/flight/object/laser.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"
#include "xwa/util/debug.h"
#include "xwa/util/random.h"
#include "xwa/util/time.h"

#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GLOBAL: XWA 0x80DC80
XwaMissionFlightGroup g_missionFlightGroups[192];
// GLOBAL: XWA 0x8BF3A0
XwaGlobalGoal g_missionGlobalGoals[10][7];
// GLOBAL: XWA 0x7B7020
MissionFgRuntimeStats g_missionFgStats[192];
// GLOBAL: XWA 0x8D4460
XwaTeam g_missionTeams[10];
// GLOBAL: XWA 0x8D6BC0
XwaMessage g_missionMessages[64];
// GLOBAL: XWA 0x80541E
MissionFlightRuntimeState g_missionFlightRuntimeState;
// GLOBAL: XWA 0x805416
int g_connectedPlayerCount;
// GLOBAL: XWA 0x80541A
int g_maxConnectedPlayerCountThisMission;
// GLOBAL: XWA 0x8053F4
MissionClock g_missionElapsedClock;
// GLOBAL: XWA 0x8053FC
MissionClock g_missionCountdownClock;
// GLOBAL: XWA 0x805412
uint8_t g_missionTimeLimitActive;
// GLOBAL: XWA 0x8053E4
uint8_t g_flightMissionEndPending;
// GLOBAL: XWA 0x805413
uint8_t g_teamVictoryTimeLimitMinutes;
// GLOBAL: XWA 0x805414
uint8_t g_teamVictoryTimeLimitStarted;
// GLOBAL: XWA 0x8053E6
uint8_t g_missionStateByte8053E6;
// GLOBAL: XWA 0x8053E8
uint16_t g_unusedMissionInitStateWord0;
// GLOBAL: XWA 0x8053EA
uint16_t g_unusedMissionInitStateWord1;
// GLOBAL: XWA 0x8053EC
uint16_t g_unusedMissionInitStateWord2;
// GLOBAL: XWA 0x8053EE
uint16_t g_unusedMissionInitStateWord3;
// GLOBAL: XWA 0x8053F0
uint16_t g_unusedMissionInitStateWord4;
// GLOBAL: XWA 0x8053F2
uint16_t g_unusedMissionInitStateWord5;
// GLOBAL: XWA 0x805406
int g_unusedMissionInitStateDword805406;
// GLOBAL: XWA 0x80540D
uint8_t g_missionRandomVariationEnabled;
// GLOBAL: XWA 0x805410
uint8_t g_aiOpponentsEnabled;
// GLOBAL: XWA 0x805411
uint8_t g_playerFlightGroupWaveMode;
// GLOBAL: XWA 0x8053E0
int g_missionFormatVersion;
// GLOBAL: XWA 0x74D5C4
uint16_t g_missionConditionTotalCount;
// GLOBAL: XWA 0x74D5C8
uint16_t g_missionConditionCurrentCount;
// GLOBAL: XWA 0x807CA4
uint8_t g_missionMessageTriggered[64];
// GLOBAL: XWA 0x807CE4
int g_missionMessageDelayCountdown[64];
// GLOBAL: XWA 0x9E96C0
int g_preparedSpawnMissionX;
// GLOBAL: XWA 0x9E96C4
int g_preparedSpawnMissionY;
// GLOBAL: XWA 0x9E96C8
int g_preparedSpawnMissionZ;
// GLOBAL: XWA 0x9E96CC
uint16_t g_preparedSpawnYawByte;
// GLOBAL: XWA 0x9E96CE
uint16_t g_preparedSpawnPitchByte;
// GLOBAL: XWA 0x9E96D0
uint16_t g_preparedSpawnRollByte;
// GLOBAL: XWA 0x9E96D2
uint8_t g_spawnTeamId;
// GLOBAL: XWA 0x9E96D3
uint8_t g_spawnIff;
// GLOBAL: XWA 0x9E96D4
uint8_t g_spawnFormationSpacing;
// GLOBAL: XWA 0x9E96D8
int g_spawnLeaderObjIdx;
// GLOBAL: XWA 0x9E96DC
uint8_t g_spawnRegionIdx;
// GLOBAL: XWA 0x9E96DD
uint8_t g_spawnGroupAI;
// GLOBAL: XWA 0x9E96DE
Q16Angle g_spawnYaw;
// GLOBAL: XWA 0x9E96E0
uint8_t g_spawnOutOfHyperspaceFlag;
// GLOBAL: XWA 0x9E96E1
uint8_t g_spawnStatus1;
// GLOBAL: XWA 0x9E96E2
uint8_t g_spawnLinkedObjectFlag;
// GLOBAL: XWA 0x9E96E7
uint8_t g_spawnFromMothershipFlag;
// GLOBAL: XWA 0x9E96E8
uint8_t g_spawnUseExactPosition;
// GLOBAL: XWA 0x9E96EA
Q16Angle g_spawnPitch;
// GLOBAL: XWA 0x9E96EC
uint16_t g_spawnCraftOrdinal;
// GLOBAL: XWA 0x9E96F2
uint16_t g_spawnObjectType;
// GLOBAL: XWA 0x9E96F4
uint8_t g_spawnObjectKind;
// GLOBAL: XWA 0x9E96F5
uint8_t g_spawnStatus2;
// GLOBAL: XWA 0x9E96F8
int g_spawnWorldX;
// GLOBAL: XWA 0x9E96FC
int g_spawnWorldY;
// GLOBAL: XWA 0x9E9700
int g_spawnWorldZ;
// GLOBAL: XWA 0x9E9704
ModelGenusId g_spawnGenusId;
// GLOBAL: XWA 0x9E9705
uint8_t g_spawnLastAssignedIff;
// GLOBAL: XWA 0x9E9706
uint8_t g_spawnFormation;
// GLOBAL: XWA 0x9E96B0
uint8_t g_spawnSavedStatus1;
// GLOBAL: XWA 0x9E96E4
uint16_t g_spawnSavedFlightGroupIdx;
// GLOBAL: XWA 0x9E96E6
uint8_t g_spawnSavedStatus2;
// GLOBAL: XWA 0x9E96EE
uint16_t g_unusedSpawnCarrierObjIdxLatch;
// GLOBAL: XWA 0x9E96F0
ModelGenusId g_spawnSavedGenusId;
// GLOBAL: XWA 0x631854
uint8_t g_initialSpawnBindPlayerCraftSlots;
// GLOBAL: XWA 0x9E9708
uint16_t g_currentFlightGroupIdx;
// GLOBAL: XWA 0x5AE030
int16_t g_escapePodPilotFlightGroupIdx = -1;
// GLOBAL: XWA 0x5AE034
int16_t g_unusedMissionLoadEscapePodWord = -1;
// GLOBAL: XWA 0x807DE0
int g_missionGlobalUnitCraftCount[41];
// GLOBAL: XWA 0x807E84
int g_activeMissionRegionCount;
// GLOBAL: XWA 0x807E88
MissionRegionHyperPointTables g_missionRegionHyperPoints;
// GLOBAL: XWA 0x9109BC
int worldlocx;
// GLOBAL: XWA 0x9109B8
int worldlocy;
// GLOBAL: XWA 0x9109B4
int worldlocz;
// GLOBAL: XWA 0x80B620
MemoryHandle g_missionFgOverrideStringHandles[192][8][3];
// GLOBAL: XWA 0x7B33E0
MemoryHandle g_missionOrderStringHandles[192][4][4];
// GLOBAL: XWA 0x8B8E20
MemoryHandle g_globalGoalOverrideStringHandles[10][7][4][3];
// GLOBAL: XWA 0x63185C
MemoryHandle g_objectTableHandle;
// GLOBAL: XWA 0x631850
MemoryHandle g_mobileObjectPoolHandle;
// GLOBAL: XWA 0x631858
MemoryHandle g_mobileObjectCharDataHandle;
// GLOBAL: XWA 0x63184C
MemoryHandle g_craftDataPoolHandle;
// GLOBAL: XWA 0x631848
MemoryHandle g_warheadGuidancePoolHandle;

// GLOBAL: XWA 0x5B1340 — arrival-enable bitmask per mission difficulty.
static const uint8_t g_missionDifficultyArrivalMasks[4] = { 1, 2, 4, 0 };
// GLOBAL: XWA 0x5B1338 — arrival-enable bitmask per flight-group arrivalDifficulty.
static const uint8_t g_fgArrivalDifficultyMasks[8] = { 7, 1, 2, 4, 6, 3, 0, 0 };

// GLOBAL: XWA 0x5BA8F0
const int g_beamTypePointValue[6] = {
	0, 100, 75, 200, 50, 0,
};

// GLOBAL: XWA 0x5BA908
const int g_countermeasureTypePointValue[4] = {
	0,
	150,
	100,
	150,
};

// GLOBAL: XWA 0x5B1298
const uint8_t g_genusConvert[12] = {
	GENUS_Fighter,   GENUS_Transport,         GENUS_Freighter,  GENUS_Starship,
	GENUS_Utility,   GENUS_Platform,          GENUS_Mine,       GENUS_SatelliteBuoy,
	GENUS_Container, GENUS_WeaponEmplacement, GENUS_PilotDroid, 0,
};

// GLOBAL: XWA 0x5B12A4
const uint8_t g_familyConvert[4] = {
	0,
	1,
	2,
	0,
};

// GLOBAL: XWA 0x5BA8D8
const int g_defaultPilotRatingByAiLevel[6] = {
	2, 4, 7, 9, 10, 11,
};

enum {
	MISSION_FORMAT_XWA_V16 = 16,
	MISSION_FORMAT_XWA_V17 = 17,
	MISSION_FORMAT_XWA_V18 = 18,
	MISSION_BRIEFING_TEAM_COUNT = 2,
	MISSION_BRIEFING_LABEL_COUNT = 192,
	MISSION_BRIEFING_ICON_COUNT = 10,
	MISSION_BRIEFING_STRING_COUNT = 128,
	MISSION_MODERN_STRING_TAIL = 0x8000,
	MISSION_MODEL_ASSET_REQUIRED = 0x10,
};

#ifndef XWA_MODERN
int File_OpenGlobalStream(const char* fileName, const char* mode, int promptOnFail, int locationMode);
#endif

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define XWA_LEGACY_PACKED
#else
#define XWA_LEGACY_PACKED __attribute__((packed))
#endif

typedef struct XWA_LEGACY_PACKED LegacyMissionHeader {
	uint16_t numFlightGroups;
	uint16_t numMessages;
	uint8_t timeLimitMin;
	uint8_t timeLimitSec;
	uint8_t winType;
	uint8_t backdrop;
	uint8_t rescue;
	uint8_t allWaypointsShown;
	uint8_t variables[8];
	char iffNames[4][20];
	uint8_t hangar;
	uint8_t goalsUnimportant;
	uint8_t missionTimeLimit;
	uint8_t reserved[61];
} LegacyMissionHeader;

typedef struct XWA_LEGACY_PACKED LegacyTrigger {
	uint8_t condition;
	uint8_t variableType;
	uint8_t variable;
	uint8_t amount;
} LegacyTrigger;

typedef struct XWA_LEGACY_PACKED LegacyTriggerPair {
	LegacyTrigger trigger1;
	LegacyTrigger trigger2;
	uint8_t unused[2];
	uint8_t t1OrT2;
} LegacyTriggerPair;

typedef struct XWA_LEGACY_PACKED LegacyOrder {
	uint8_t order;
	uint8_t throttle;
	uint8_t variable1;
	uint8_t variable2;
	uint8_t variable3;
	uint8_t variable4;
	uint8_t target3Type;
	uint8_t target4Type;
	uint8_t target3;
	uint8_t target4;
	uint8_t target3OrTarget4;
	uint8_t unused0;
	uint8_t target1Type;
	uint8_t target1;
	uint8_t target2Type;
	uint8_t target2;
	uint8_t target1OrTarget2;
	uint8_t unused1;
	uint8_t speed;
	char designation[16];
	uint8_t reserved[47];
} LegacyOrder;

typedef struct XWA_LEGACY_PACKED LegacyFlightGroupGoal {
	uint8_t argument;
	uint8_t condition;
	uint8_t amount;
	int8_t points;
	uint8_t enabledForTeam[10];
	uint8_t parameter;
	uint8_t reserved[63];
} LegacyFlightGroupGoal;

typedef struct XWA_LEGACY_PACKED LegacyFlightGroup {
	char name[20];
	char craftRole[16];
	uint8_t unusedCraftRole[4];
	char cargo[20];
	char specialCargo[20];
	uint8_t specialCargoCraft;
	uint8_t randomSpecialCargoCraft;
	uint8_t craftType;
	uint8_t numberOfCraft;
	uint8_t status1;
	uint8_t warhead;
	uint8_t beam;
	uint8_t iff;
	uint8_t team;
	uint8_t groupAI;
	uint8_t markings;
	uint8_t radio;
	uint8_t unused0x5C;
	uint8_t formation;
	uint8_t formationSpacing;
	uint8_t globalGroup;
	uint8_t unused0x60;
	uint8_t numberOfWaves;
	uint8_t wavesDelay;
	uint8_t stopArrivingWhen;
	uint8_t playerNumber;
	uint8_t arriveOnlyIfHuman;
	uint8_t playerCraft;
	uint8_t yaw;
	uint8_t pitch;
	uint8_t roll;
	uint8_t legacyPermaDeathEnabled;
	uint8_t legacyPermaDeathId;
	uint8_t unusedPermaDeath;
	uint8_t arrivalDifficulty;
	LegacyTriggerPair arrival[2];
	uint8_t arrivals12OrArrivals34;
	uint8_t arrivalRandDelayMinutes;
	uint8_t arrivalDelayMinutes;
	uint8_t arrivalDelaySeconds;
	LegacyTriggerPair departure;
	uint8_t departureDelayMinutes;
	uint8_t departureDelaySeconds;
	uint8_t abortTrigger;
	uint8_t arrivalRandDelaySeconds;
	int16_t editorMothership;
	uint8_t unusedMothership;
	uint8_t arrivalMothership;
	uint8_t arrivalMethod;
	uint8_t departureMothership;
	uint8_t departureMethod;
	uint8_t alternateMothership;
	uint8_t alternateMothershipUsed;
	uint8_t capturedDepartureMothership;
	uint8_t capturedDepartViaMothership;
	LegacyOrder orders[4];
	LegacyTriggerPair skipToOrder4;
	LegacyFlightGroupGoal goals[8];
	uint8_t unusedGoalsTail;
	int16_t waypointX[22];
	int16_t waypointY[22];
	int16_t waypointZ[22];
	int16_t waypointEnabled[22];
	uint8_t unusedOptionsPrefix[10];
	uint8_t disableWaveNumbering;
	uint8_t departureClockMin;
	uint8_t departureClockSec;
	uint8_t countermeasures;
	uint8_t craftExplosionTime;
	uint8_t status2;
	uint8_t globalUnit;
	uint8_t unusedOptions[8];
	uint8_t handicap;
	uint8_t optionalWarheads[8];
	uint8_t optionalBeams[6];
	uint8_t optionalCountermeasures[4];
	uint8_t optionalCraftCategory;
	uint8_t optionalCraft[10];
	uint8_t numberOfOptionalCraft[10];
	uint8_t numberOfOptionalCraftWaves[10];
	uint8_t reservedTail;
} LegacyFlightGroup;

typedef struct XWA_LEGACY_PACKED LegacyMessage {
	char message[64];
	uint8_t sentToTeam[10];
	LegacyTriggerPair triggers[2];
	char voice[8];
	int32_t originatingFG;
	int32_t type;
	uint8_t rawDelay;
	uint8_t triggers12OrTriggers34;
} LegacyMessage;

typedef struct XWA_LEGACY_PACKED LegacyGlobalGoal {
	LegacyTriggerPair triggers12;
	LegacyTriggerPair triggers34;
	char name[16];
	uint8_t version;
	uint8_t t12AndOrT34;
	uint8_t rawDelay;
	int8_t rawPoints;
} LegacyGlobalGoal;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif
#undef XWA_LEGACY_PACKED

static __inline unsigned int Mission_ConvertLegacyDelayValue(unsigned int value) {
	if (value < 4) {
		return 5 * value;
	}
	if (value < 0xb4) {
		return value + 16;
	}
	return (value >> 1) + 106;
}

// FUNCTION: XWA 0x420300
int Mission_LoadFile(char* fileName) {
	int16_t fgIdx, messageIdx, teamIdx, i;
	int16_t groupIdx, slotIdx, goalIdx, conditionIdx, textIdx;
	XwaFile* stream;
	int16_t diskFormatVersion;
	int formatVersion;
	int16_t count;
	int16_t indexedRecord;
	int16_t textLength;
	LegacyMissionHeader legacyHeader;
	LegacyFlightGroup legacyFlightGroup;
	LegacyMessage legacyMessage;
	LegacyGlobalGoal legacyGlobalGoal;
	char stringBuffer[1024];
	char baseName[256];
	int firstCharacter;
	int16_t pathIndex;

	Mission_FreeOverrideStringHandles();

	if (g_filmPlaybackMode) {
		stream = g_filmFile;
		Film_SeekToEmbeddedMissionData();
	} else {
#ifdef XWA_MODERN
		stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
		if (stream == NULL) {
			return 0;
		}
		g_stream = stream;
#else
		if (!File_OpenGlobalStream(fileName, "rb", 1, 0)) {
			return 0;
		}
		stream = g_stream;
#endif
	}

	FeDiskIo_ReadWithRetryPrompt(&diskFormatVersion, 2u, 1u, stream);
	formatVersion = diskFormatVersion;
	g_missionFormatVersion = formatVersion;
	if (formatVersion != 14 && formatVersion != -1) {
		if (formatVersion != 12 && formatVersion != 15 && formatVersion != 17 && formatVersion != 16 &&
			formatVersion != 18) {
			return 0;
		}
	}

	if (formatVersion == 12 || formatVersion == 14 || formatVersion == 15 || formatVersion == 17 ||
		formatVersion == 16 || formatVersion == 18) {

		if (formatVersion == 17 || formatVersion == 16 || formatVersion == 18) {
			FeDiskIo_ReadWithRetryPrompt(&g_missionHeader, sizeof(g_missionHeader), 1u, stream);
		} else {
			FeDiskIo_ReadWithRetryPrompt(&legacyHeader, sizeof(legacyHeader), 1u, stream);
			memset(&g_missionHeader, 0, sizeof(g_missionHeader));
			g_missionHeader.numFlightGroups = legacyHeader.numFlightGroups;
			g_missionHeader.numMessages = legacyHeader.numMessages;
			g_missionHeader.body.legacyTimeLimitMin = legacyHeader.timeLimitMin;
			g_missionHeader.body.legacyTimeLimitSec = legacyHeader.timeLimitSec;
			g_missionHeader.body.legacyWinType = legacyHeader.winType;
			g_missionHeader.body.legacyBackdrop = legacyHeader.backdrop;
			g_missionHeader.body.legacyRescue = legacyHeader.rescue;
			g_missionHeader.body.legacyAllWayShown = legacyHeader.allWaypointsShown;
			memcpy(&g_missionHeader.body.legacyVars[4], &legacyHeader.variables[4], 4u);
			g_escapePodPilotFlightGroupIdx = -1;
			g_unusedMissionLoadEscapePodWord = -1;
			memcpy(g_missionHeader.body.legacyVars, legacyHeader.variables, 4u);
			{
				int iffCount = 4;
				i = 0;
				do {
					memcpy(g_missionHeader.body.iffNames[i], legacyHeader.iffNames[i], 12u);
					++i;
				} while (--iffCount != 0);
			}
			g_missionHeader.body.hangar = legacyHeader.hangar;
			g_missionHeader.body.goalsUnimportant = legacyHeader.goalsUnimportant;
			g_missionHeader.body.timeLimitMin = legacyHeader.missionTimeLimit;
		}

		for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
			if (g_missionFormatVersion == 17 || g_missionFormatVersion == 16 ||
				g_missionFormatVersion == 18) {
				FeDiskIo_ReadWithRetryPrompt(&g_missionFlightGroups[fgIdx].fg, sizeof(XwaFlightGroup), 1u,
											 stream);
			} else {
				XwaFlightGroup* fg;
				int pairIdx;
				int orderIdx;
				int waypointIdx;

				FeDiskIo_ReadWithRetryPrompt(&legacyFlightGroup, sizeof(legacyFlightGroup), 1u, stream);
				memset(&g_missionFlightGroups[fgIdx], 0, sizeof(g_missionFlightGroups[fgIdx]));
				fg = &g_missionFlightGroups[fgIdx].fg;
				memcpy(fg->name, legacyFlightGroup.name, sizeof(fg->name));
				memcpy(fg->cargo, legacyFlightGroup.cargo, sizeof(fg->cargo));
				memcpy(fg->specialCargo, legacyFlightGroup.specialCargo, sizeof(fg->specialCargo));
				fg->specialCargoCraft = legacyFlightGroup.specialCargoCraft;
				fg->randomSpecialCargoCraft = legacyFlightGroup.randomSpecialCargoCraft;
				fg->craftType = legacyFlightGroup.craftType;
				fg->numberOfCraft = legacyFlightGroup.numberOfCraft;
				fg->status1 = legacyFlightGroup.status1;
				fg->warhead = legacyFlightGroup.warhead;
				fg->beam = legacyFlightGroup.beam;
				fg->iff = legacyFlightGroup.iff;
				fg->team = legacyFlightGroup.team;
				fg->groupAI = legacyFlightGroup.groupAI;
				fg->markings = legacyFlightGroup.markings;
				fg->radio = legacyFlightGroup.radio;
				fg->unused0x75 = legacyFlightGroup.unused0x5C;
				fg->formation = legacyFlightGroup.formation;
				fg->formationSpacing = legacyFlightGroup.formationSpacing;
				fg->globalGroup = legacyFlightGroup.globalGroup;
				fg->numberOfWaves = legacyFlightGroup.numberOfWaves;
				fg->wavesDelay = legacyFlightGroup.wavesDelay;
				fg->stopArrivingWhen = legacyFlightGroup.stopArrivingWhen;
				fg->playerNumber = legacyFlightGroup.playerNumber;
				fg->arriveOnlyIfHuman = legacyFlightGroup.arriveOnlyIfHuman;
				fg->playerCraft = legacyFlightGroup.playerCraft;
				fg->yaw = legacyFlightGroup.yaw;
				fg->pitch = legacyFlightGroup.pitch;
				fg->roll = legacyFlightGroup.roll;
				fg->arrivalDifficulty = legacyFlightGroup.arrivalDifficulty;
				for (pairIdx = 0; pairIdx < 2; ++pairIdx) {
					XwaTriggerPair* dstPair = &fg->arrival[pairIdx];
					LegacyTriggerPair* srcPair = &legacyFlightGroup.arrival[pairIdx];
					dstPair->triggers[0].condition = srcPair->trigger1.condition;
					dstPair->triggers[0].variableType = srcPair->trigger1.variableType;
					dstPair->triggers[0].variable = srcPair->trigger1.variable;
					dstPair->triggers[0].amount = srcPair->trigger1.amount;
					dstPair->triggers[1].condition = srcPair->trigger2.condition;
					dstPair->triggers[1].variableType = srcPair->trigger2.variableType;
					dstPair->triggers[1].variable = srcPair->trigger2.variable;
					dstPair->triggers[1].amount = srcPair->trigger2.amount;
					dstPair->t1OrT2 = srcPair->t1OrT2;
				}
				fg->arrivals12OrArrivals34 = legacyFlightGroup.arrivals12OrArrivals34;
				fg->arrivalRandDelayMinutes = legacyFlightGroup.arrivalRandDelayMinutes;
				fg->arrivalDelayMinutes = legacyFlightGroup.arrivalDelayMinutes;
				fg->arrivalDelaySeconds = legacyFlightGroup.arrivalDelaySeconds;
				fg->departure.triggers[0].condition = legacyFlightGroup.departure.trigger1.condition;
				fg->departure.triggers[0].variableType = legacyFlightGroup.departure.trigger1.variableType;
				fg->departure.triggers[0].variable = legacyFlightGroup.departure.trigger1.variable;
				fg->departure.triggers[0].amount = legacyFlightGroup.departure.trigger1.amount;
				fg->departure.triggers[1].condition = legacyFlightGroup.departure.trigger2.condition;
				fg->departure.triggers[1].variableType = legacyFlightGroup.departure.trigger2.variableType;
				fg->departure.triggers[1].variable = legacyFlightGroup.departure.trigger2.variable;
				fg->departure.triggers[1].amount = legacyFlightGroup.departure.trigger2.amount;
				fg->departure.t1OrT2 = legacyFlightGroup.departure.t1OrT2;
				fg->departureDelayMinutes = legacyFlightGroup.departureDelayMinutes;
				fg->departureDelaySeconds = legacyFlightGroup.departureDelaySeconds;
				fg->abortTrigger = legacyFlightGroup.abortTrigger;
				fg->arrivalRandDelaySeconds = legacyFlightGroup.arrivalRandDelaySeconds;
				fg->editorMothership = legacyFlightGroup.editorMothership;
				fg->arrivalMothership = legacyFlightGroup.arrivalMothership;
				fg->arrivalMethod = legacyFlightGroup.arrivalMethod;
				fg->departureMothership = legacyFlightGroup.departureMothership;
				fg->departMethod = legacyFlightGroup.departureMethod;
				fg->alternateMothership = legacyFlightGroup.alternateMothership;
				fg->alternateMothershipUsed = legacyFlightGroup.alternateMothershipUsed;
				fg->capturedDepartureMothership = legacyFlightGroup.capturedDepartureMothership;
				fg->capturedDepartViaMothership = legacyFlightGroup.capturedDepartViaMothership;
				for (orderIdx = 0; orderIdx < 4; ++orderIdx) {
					XwaOrder* order = &fg->orders[orderIdx];
					LegacyOrder* legacyOrder = &legacyFlightGroup.orders[orderIdx];
					order->order = legacyOrder->order;
					order->throttle = legacyOrder->throttle;
					order->variable1 = legacyOrder->variable1;
					order->variable2 = legacyOrder->variable2;
					order->variable3 = legacyOrder->variable3;
					order->variable4 = legacyOrder->variable4;
					order->secondaryTargetTypes[XWA_ORDER_TARGET_3] = legacyOrder->target3Type;
					order->secondaryTargetTypes[XWA_ORDER_TARGET_4] = legacyOrder->target4Type;
					order->secondaryTargets[XWA_ORDER_TARGET_3] = legacyOrder->target3;
					order->secondaryTargets[XWA_ORDER_TARGET_4] = legacyOrder->target4;
					order->target3OrTarget4 = legacyOrder->target3OrTarget4;
					order->target1Type = legacyOrder->target1Type;
					order->target1 = legacyOrder->target1;
					order->target2Type = legacyOrder->target2Type;
					order->target2 = legacyOrder->target2;
					order->target1OrTarget2 = legacyOrder->target1OrTarget2;
					order->speed = legacyOrder->speed;
					for (waypointIdx = 0; waypointIdx < 8; ++waypointIdx) {
						fg->orders[orderIdx].waypoints[waypointIdx].x =
							legacyFlightGroup.waypointX[waypointIdx + 4];
						fg->orders[orderIdx].waypoints[waypointIdx].y =
							legacyFlightGroup.waypointY[waypointIdx + 4];
						fg->orders[orderIdx].waypoints[waypointIdx].z =
							legacyFlightGroup.waypointZ[waypointIdx + 4];
						fg->orders[orderIdx].waypoints[waypointIdx].enabled =
							legacyFlightGroup.waypointEnabled[waypointIdx + 4];
					}
				}
				fg->skipTriggers[3].triggers[0].condition = legacyFlightGroup.skipToOrder4.trigger1.condition;
				fg->skipTriggers[3].triggers[0].variableType =
					legacyFlightGroup.skipToOrder4.trigger1.variableType;
				fg->skipTriggers[3].triggers[0].variable = legacyFlightGroup.skipToOrder4.trigger1.variable;
				fg->skipTriggers[3].triggers[0].amount = legacyFlightGroup.skipToOrder4.trigger1.amount;
				fg->skipTriggers[3].triggers[1].condition = legacyFlightGroup.skipToOrder4.trigger2.condition;
				fg->skipTriggers[3].triggers[1].variableType =
					legacyFlightGroup.skipToOrder4.trigger2.variableType;
				fg->skipTriggers[3].triggers[1].variable = legacyFlightGroup.skipToOrder4.trigger2.variable;
				fg->skipTriggers[3].triggers[1].amount = legacyFlightGroup.skipToOrder4.trigger2.amount;
				fg->skipTriggers[3].t1OrT2 = legacyFlightGroup.skipToOrder4.t1OrT2;
				for (goalIdx = 0; goalIdx < 8; ++goalIdx) {
					memcpy(&fg->fgGoals[goalIdx].payload, &legacyFlightGroup.goals[goalIdx],
						   offsetof(XwaFlightGroupGoalPayload, activeSequence));
				}
				for (waypointIdx = 0; waypointIdx < 2; ++waypointIdx) {
					fg->missionPoints[waypointIdx].x = legacyFlightGroup.waypointX[waypointIdx];
					fg->missionPoints[waypointIdx].y = legacyFlightGroup.waypointY[waypointIdx];
					fg->missionPoints[waypointIdx].z = legacyFlightGroup.waypointZ[waypointIdx];
					fg->missionPoints[waypointIdx].enabled = legacyFlightGroup.waypointEnabled[waypointIdx];
					fg->missionPointRegions[waypointIdx] = 0;
				}
				fg->missionPoints[XWA_FG_POINT_CAPTURE_HYPER].x = legacyFlightGroup.waypointX[12];
				fg->missionPoints[XWA_FG_POINT_CAPTURE_HYPER].y = legacyFlightGroup.waypointY[12];
				fg->missionPoints[XWA_FG_POINT_CAPTURE_HYPER].z = legacyFlightGroup.waypointZ[12];
				fg->missionPoints[XWA_FG_POINT_CAPTURE_HYPER].enabled = legacyFlightGroup.waypointEnabled[12];
				fg->missionPointRegions[XWA_FG_POINT_CAPTURE_HYPER] = 0;
				fg->missionPoints[XWA_FG_POINT_HYPER].x = legacyFlightGroup.waypointX[13];
				fg->missionPoints[XWA_FG_POINT_HYPER].y = legacyFlightGroup.waypointY[13];
				fg->missionPoints[XWA_FG_POINT_HYPER].z = legacyFlightGroup.waypointZ[13];
				fg->missionPoints[XWA_FG_POINT_HYPER].enabled = legacyFlightGroup.waypointEnabled[13];
				fg->missionPointRegions[XWA_FG_POINT_HYPER] = 0;
				fg->disableWaveNumbering = legacyFlightGroup.disableWaveNumbering;
				fg->departureClockMin = legacyFlightGroup.departureClockMin;
				fg->departureClockSec = legacyFlightGroup.departureClockSec;
				fg->countermeasures = legacyFlightGroup.countermeasures;
				fg->craftExplosionTime = legacyFlightGroup.craftExplosionTime;
				fg->status2 = legacyFlightGroup.status2;
				fg->globalUnit = legacyFlightGroup.globalUnit;
				fg->handicap = legacyFlightGroup.handicap;
				memcpy(fg->optionalWarheads, legacyFlightGroup.optionalWarheads,
					   sizeof(fg->optionalWarheads));
				memcpy(fg->optionalBeams, legacyFlightGroup.optionalBeams, sizeof(fg->optionalBeams));
				memcpy(fg->optionalCountermeasures, legacyFlightGroup.optionalCountermeasures,
					   sizeof(fg->optionalCountermeasures));
				fg->optionalCraftCategory = legacyFlightGroup.optionalCraftCategory;
				memcpy(fg->optionalCraft, legacyFlightGroup.optionalCraft, sizeof(fg->optionalCraft));
				memcpy(fg->numberOfOptionalCraft, legacyFlightGroup.numberOfOptionalCraft,
					   sizeof(fg->numberOfOptionalCraft));
				memcpy(fg->numberOfOptionalCraftWaves, legacyFlightGroup.numberOfOptionalCraftWaves,
					   sizeof(fg->numberOfOptionalCraftWaves));
			}
			g_missionFlightGroups[fgIdx].playerOwnerIdx = -1;
		}

		for (messageIdx = 0; messageIdx < (int16_t)g_missionHeader.numMessages; ++messageIdx) {
			FeDiskIo_ReadWithRetryPrompt(&indexedRecord, 2u, 1u, stream);
			if (g_missionFormatVersion == 17 || g_missionFormatVersion == 16 ||
				g_missionFormatVersion == 18) {
				FeDiskIo_ReadWithRetryPrompt(&g_missionMessages[indexedRecord], sizeof(XwaMessage), 1u,
											 stream);
			} else {
				XwaMessage* message;
				int pairIdx;

				FeDiskIo_ReadWithRetryPrompt(&legacyMessage, sizeof(legacyMessage), 1u, stream);
				message = &g_missionMessages[indexedRecord];
				memset(message, 0, sizeof(*message));
				memcpy(message->message, legacyMessage.message, sizeof(legacyMessage.message));
				memcpy(message->sentToTeam, legacyMessage.sentToTeam, sizeof(message->sentToTeam));
				for (pairIdx = 0; pairIdx < 2; ++pairIdx) {
					message->triggers[pairIdx].triggers[0].condition =
						legacyMessage.triggers[pairIdx].trigger1.condition;
					message->triggers[pairIdx].triggers[0].variableType =
						legacyMessage.triggers[pairIdx].trigger1.variableType;
					message->triggers[pairIdx].triggers[0].variable =
						legacyMessage.triggers[pairIdx].trigger1.variable;
					message->triggers[pairIdx].triggers[0].amount =
						legacyMessage.triggers[pairIdx].trigger1.amount;
					message->triggers[pairIdx].triggers[1].condition =
						legacyMessage.triggers[pairIdx].trigger2.condition;
					message->triggers[pairIdx].triggers[1].variableType =
						legacyMessage.triggers[pairIdx].trigger2.variableType;
					message->triggers[pairIdx].triggers[1].variable =
						legacyMessage.triggers[pairIdx].trigger2.variable;
					message->triggers[pairIdx].triggers[1].amount =
						legacyMessage.triggers[pairIdx].trigger2.amount;
					message->triggers[pairIdx].t1OrT2 = legacyMessage.triggers[pairIdx].t1OrT2;
				}
				message->originatingFG = legacyMessage.originatingFG;
				message->type = legacyMessage.type;
				message->rawDelay = legacyMessage.rawDelay;
				message->triggers12OrTriggers34 = legacyMessage.triggers12OrTriggers34;
			}
		}

		for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
			FeDiskIo_ReadWithRetryPrompt(&count, 2u, 1u, stream);
			if (g_missionFormatVersion == 17 || g_missionFormatVersion == 16 ||
				g_missionFormatVersion == 18) {
				FeDiskIo_ReadWithRetryPrompt(&g_missionGlobalGoals[teamIdx][0], sizeof(XwaGlobalGoal), count,
											 stream);
			} else {
				for (goalIdx = 0; goalIdx < count; ++goalIdx) {
					XwaGlobalGoal* goal;
					FeDiskIo_ReadWithRetryPrompt(&legacyGlobalGoal, sizeof(legacyGlobalGoal), 1u, stream);
					goal = &g_missionGlobalGoals[teamIdx][goalIdx];
					memset(goal, 0, sizeof(*goal));
					goal->triggerPairs[0].triggers[0].condition =
						legacyGlobalGoal.triggers12.trigger1.condition;
					goal->triggerPairs[0].triggers[0].variableType =
						legacyGlobalGoal.triggers12.trigger1.variableType;
					goal->triggerPairs[0].triggers[0].variable =
						legacyGlobalGoal.triggers12.trigger1.variable;
					goal->triggerPairs[0].triggers[0].amount = legacyGlobalGoal.triggers12.trigger1.amount;
					goal->triggerPairs[0].triggers[1].condition =
						legacyGlobalGoal.triggers12.trigger2.condition;
					goal->triggerPairs[0].triggers[1].variableType =
						legacyGlobalGoal.triggers12.trigger2.variableType;
					goal->triggerPairs[0].triggers[1].variable =
						legacyGlobalGoal.triggers12.trigger2.variable;
					goal->triggerPairs[0].triggers[1].amount = legacyGlobalGoal.triggers12.trigger2.amount;
					goal->triggerPairs[0].t1OrT2 = legacyGlobalGoal.triggers12.t1OrT2;
					goal->triggerPairs[1].triggers[0].condition =
						legacyGlobalGoal.triggers34.trigger1.condition;
					goal->triggerPairs[1].triggers[0].variableType =
						legacyGlobalGoal.triggers34.trigger1.variableType;
					goal->triggerPairs[1].triggers[0].variable =
						legacyGlobalGoal.triggers34.trigger1.variable;
					goal->triggerPairs[1].triggers[0].amount = legacyGlobalGoal.triggers34.trigger1.amount;
					goal->triggerPairs[1].triggers[1].condition =
						legacyGlobalGoal.triggers34.trigger2.condition;
					goal->triggerPairs[1].triggers[1].variableType =
						legacyGlobalGoal.triggers34.trigger2.variableType;
					goal->triggerPairs[1].triggers[1].variable =
						legacyGlobalGoal.triggers34.trigger2.variable;
					goal->triggerPairs[1].triggers[1].amount = legacyGlobalGoal.triggers34.trigger2.amount;
					goal->triggerPairs[1].t1OrT2 = legacyGlobalGoal.triggers34.t1OrT2;
					memcpy(goal->name, legacyGlobalGoal.name, sizeof(goal->name));
					goal->version = legacyGlobalGoal.version;
					goal->t12AndOrT34 = legacyGlobalGoal.t12AndOrT34;
					goal->rawDelay = legacyGlobalGoal.rawDelay;
					goal->rawPoints = legacyGlobalGoal.rawPoints;
				}
			}
		}

		for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
			FeDiskIo_ReadWithRetryPrompt(&count, 2u, 1u, stream);
			if (count != 0) {
				FeDiskIo_ReadWithRetryPrompt(&g_missionTeams[teamIdx], sizeof(XwaTeam), 1u, stream);
			}
		}

		for (i = 0; i < 2; ++i) {
#ifdef XWA_MODERN
			File_Seek(stream, g_missionHeader.body.secondaryVersion == 98 ? 12810 : 3210, SEEK_CUR);
#else
			fseek((FILE*)stream, g_missionHeader.body.secondaryVersion == 98 ? 12810 : 3210, SEEK_CUR);
#endif
			for (textIdx = 0; textIdx < 192; ++textIdx) {
#ifdef XWA_MODERN
				File_Seek(stream, 24, SEEK_CUR);
#else
				fseek((FILE*)stream, 24, SEEK_CUR);
#endif
			}
			for (textIdx = 0; textIdx < 10; ++textIdx) {
#ifdef XWA_MODERN
				File_Seek(stream, 1, SEEK_CUR);
#else
				fseek((FILE*)stream, 1, SEEK_CUR);
#endif
			}
			for (textIdx = 0; textIdx < 128; ++textIdx) {
				FeDiskIo_ReadWithRetryPrompt(&textLength, 2u, 1u, stream);
				if (textLength != 0) {
#ifdef XWA_MODERN
					File_Seek(stream, textLength, SEEK_CUR);
#else
					fseek((FILE*)stream, textLength, SEEK_CUR);
#endif
				}
			}
			for (textIdx = 0; textIdx < 128; ++textIdx) {
				FeDiskIo_ReadWithRetryPrompt(&textLength, 2u, 1u, stream);
				if (textLength != 0) {
#ifdef XWA_MODERN
					File_Seek(stream, textLength, SEEK_CUR);
#else
					fseek((FILE*)stream, textLength, SEEK_CUR);
#endif
				}
			}
		}
		if (g_missionFormatVersion == MISSION_FORMAT_XWA_V18) {
#ifdef XWA_MODERN
			File_Seek(stream, MISSION_MODERN_STRING_TAIL, SEEK_CUR);
#else
			fseek((FILE*)stream, MISSION_MODERN_STRING_TAIL, SEEK_CUR);
#endif
		}

		for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
			for (groupIdx = 0; groupIdx < 8; ++groupIdx) {
				for (slotIdx = 0; slotIdx < 3; ++slotIdx) {
					uint8_t length;
					MemoryHandle handle;
					if (g_missionFormatVersion == 17 || g_missionFormatVersion == 16 ||
						g_missionFormatVersion == 18) {
						FeDiskIo_ReadWithRetryPrompt(&firstCharacter, 1u, 1u, stream);
						if ((char)firstCharacter != '\0') {
							FeDiskIo_ReadWithRetryPrompt(&stringBuffer[1], 0x3fu, 1u, stream);
						}
						stringBuffer[0] = (char)firstCharacter;
					} else {
						FeDiskIo_ReadWithRetryPrompt(stringBuffer, 0x40u, 1u, stream);
					}
#ifdef XWA_MODERN
					stringBuffer[64] = '\0';
#endif
					length = (uint8_t)strlen(stringBuffer);
					handle = 0;
					if (length != 0) {
						handle = Memory_AllocHandleZeroed("OVERRIDESTRING", length + 1u);
						if (handle != 0) {
							char* dst = (char*)Memory_LockHandle(handle);
							memcpy(dst, stringBuffer, length);
							dst[length] = '\0';
							Memory_UnlockHandle(handle);
						}
					}
					g_missionFgOverrideStringHandles[fgIdx][groupIdx][slotIdx] = handle;
				}
			}
		}

		for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
			for (goalIdx = 0; goalIdx < 7; ++goalIdx) {
				for (conditionIdx = 0; conditionIdx < 4; ++conditionIdx) {
					for (textIdx = 0; textIdx < 3; ++textIdx) {
						uint8_t length;
						MemoryHandle handle;
						if (g_missionFormatVersion == 17 || g_missionFormatVersion == 16 ||
							g_missionFormatVersion == 18) {
							FeDiskIo_ReadWithRetryPrompt(&firstCharacter, 1u, 1u, stream);
							if ((char)firstCharacter != '\0') {
								FeDiskIo_ReadWithRetryPrompt(&stringBuffer[1], 0x3fu, 1u, stream);
							}
							stringBuffer[0] = (char)firstCharacter;
						} else {
							FeDiskIo_ReadWithRetryPrompt(stringBuffer, 0x40u, 1u, stream);
						}
#ifdef XWA_MODERN
						stringBuffer[64] = '\0';
#endif
						length = (uint8_t)strlen(stringBuffer);
						handle = 0;
						if (length != 0) {
							handle = Memory_AllocHandleZeroed("GLOBALOVERRIDESTRING", length + 1u);
							if (handle != 0) {
								char* dst = (char*)Memory_LockHandle(handle);
								memcpy(dst, stringBuffer, length);
								dst[length] = '\0';
								Memory_UnlockHandle(handle);
							}
						}
						g_globalGoalOverrideStringHandles[teamIdx][goalIdx][conditionIdx][textIdx] = handle;
					}
				}
			}
		}

		if (g_missionFormatVersion == 17 || g_missionFormatVersion == 16 || g_missionFormatVersion == 18) {
			for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
				for (groupIdx = 0; groupIdx < 4; ++groupIdx) {
					for (slotIdx = 0; slotIdx < 4; ++slotIdx) {
						uint8_t length;
						MemoryHandle handle;
						FeDiskIo_ReadWithRetryPrompt(&firstCharacter, 1u, 1u, stream);
						handle = 0;
						if ((char)firstCharacter != '\0') {
							FeDiskIo_ReadWithRetryPrompt(&stringBuffer[1], 0x3fu, 1u, stream);
							stringBuffer[0] = (char)firstCharacter;
#ifdef XWA_MODERN
							stringBuffer[64] = '\0';
#endif
							length = (uint8_t)strlen(stringBuffer);
							handle = Memory_AllocHandleZeroed("AIOVERRIDESTRING", length + 1u);
							if (handle != 0) {
								char* dst = (char*)Memory_LockHandle(handle);
								memcpy(dst, stringBuffer, length);
								dst[length] = '\0';
								Memory_UnlockHandle(handle);
							}
						}
						g_missionOrderStringHandles[fgIdx][groupIdx][slotIdx] = handle;
					}
				}
			}
		}

		if (g_missionFormatVersion != MISSION_FORMAT_XWA_V18) {
			for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
				XwaFlightGroup* fg = &g_missionFlightGroups[fgIdx].fg;
				int outerOrder;
				int innerOrder;
				for (outerOrder = 0; outerOrder < 4; ++outerOrder) {
					for (innerOrder = 0; innerOrder < 4; ++innerOrder) {
						XwaOrder* order = &fg->orders[outerOrder * 4 + innerOrder];
						switch (order->order) {
							case 1:
							case 0x0c:
							case 0x0d:
							case 0x0e:
							case 0x0f:
							case 0x11:
							case 0x12:
							case 0x13:
							case 0x14:
							case 0x16:
							case 0x17:
							case 0x18:
							case 0x19:
							case 0x1d:
							case 0x1f:
							case 0x20:
							case 0x21:
							case 0x22:
							case 0x23:
							case 0x24:
							case 0x2b:
							case 0x2c:
							case 0x2d:
							case 0x39:
							case 0x3a:
							case 0x3d:
								order->variable1 = Mission_ConvertLegacyDelayValue(order->variable1);
								break;
							case 0x2a:
								order->variable2 = Mission_ConvertLegacyDelayValue(order->variable2);
								break;
							case 0x10:
								order->variable1 = Mission_ConvertLegacyDelayValue(order->variable1);
								order->variable3 = Mission_ConvertLegacyDelayValue(order->variable3);
								break;
						}
					}
				}
				for (goalIdx = 0; goalIdx < 8; ++goalIdx) {
					fg->fgGoals[goalIdx].payload.parameter =
						Mission_ConvertLegacyDelayValue(fg->fgGoals[goalIdx].payload.parameter);
				}
			}
			{
				int delayTeamIdx;
				unsigned int eomGoalIdx;
				for (delayTeamIdx = 0; delayTeamIdx < 10; ++delayTeamIdx) {
					for (goalIdx = 0; goalIdx < 7; ++goalIdx) {
						g_missionGlobalGoals[delayTeamIdx][goalIdx].rawDelay =
							Mission_ConvertLegacyDelayValue(
								g_missionGlobalGoals[delayTeamIdx][goalIdx].rawDelay);
					}
					for (eomGoalIdx = 0; eomGoalIdx < 3; ++eomGoalIdx) {
						g_missionTeams[delayTeamIdx].eomRawDelay[eomGoalIdx] =
							Mission_ConvertLegacyDelayValue(
								g_missionTeams[delayTeamIdx].eomRawDelay[eomGoalIdx]);
					}
				}
			}
			{
				int delayMessageIdx;
				for (delayMessageIdx = 0; delayMessageIdx < 64; ++delayMessageIdx) {
					g_missionMessages[delayMessageIdx].rawDelay =
						Mission_ConvertLegacyDelayValue(g_missionMessages[delayMessageIdx].rawDelay);
				}
			}
		}
	}

	for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
		char* text = (char*)g_missionFlightGroups[fgIdx].fg.unused0x1B;
		for (i = 0; i < 16 && text[i] != '\0'; ++i) {
			if (text[i] >= 'a' && text[i] <= 'z') {
				text[i] = (char)(text[i] - 32);
			}
		}
	}
	for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
		g_missionTeams[teamIdx].allies[teamIdx] = 1;
	}
	for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
		for (goalIdx = 0; goalIdx < 3; ++goalIdx) {
			XwaGlobalGoal* goal = &g_missionGlobalGoals[teamIdx][goalIdx];
			uint8_t condition1 = goal->triggerPairs[0].triggers[0].condition;
			uint8_t condition2 = goal->triggerPairs[0].triggers[1].condition;
			uint8_t condition3 = goal->triggerPairs[1].triggers[0].condition;
			uint8_t condition4 = goal->triggerPairs[1].triggers[1].condition;
			if (condition1 != 10 && condition2 == 10 && condition3 == 10 && condition4 == 10) {
				goal->triggerPairs[0].triggers[1].condition = 0;
				goal->triggerPairs[0].t1OrT2 = 0;
				goal->triggerPairs[1].triggers[0].condition = 0;
				goal->triggerPairs[1].t1OrT2 = 0;
				goal->triggerPairs[1].triggers[1].condition = 0;
				goal->t12AndOrT34 = 0;
			} else if (condition1 != 10 && condition2 != 10 && condition3 == 10 && condition4 == 10) {
				goal->triggerPairs[1].triggers[0].condition = 0;
				goal->triggerPairs[1].t1OrT2 = 0;
				goal->triggerPairs[1].triggers[1].condition = 0;
				goal->t12AndOrT34 = 0;
			} else if (condition1 != 10 && condition2 != 10 && condition3 != 10 && condition4 == 10) {
				goal->triggerPairs[1].t1OrT2 = 0;
				goal->triggerPairs[1].triggers[1].condition = 0;
			}
		}
	}

	if (!g_filmPlaybackMode) {
		g_stream = stream;
		if (FeDiskIo_CloseGlobalStream(0)) {
			return 0;
		}
	}

#ifdef XWA_MODERN
	strncpy(baseName, fileName, sizeof(baseName) - 1u);
	baseName[sizeof(baseName) - 1u] = '\0';
#else
	strcpy(baseName, fileName);
#endif
	for (pathIndex = (int16_t)strlen(baseName) - 1; pathIndex > 0; --pathIndex) {
		if (baseName[pathIndex] == '\\'
#ifdef XWA_MODERN
			|| baseName[pathIndex] == '/'
#endif
		) {
			break;
		}
	}
	Mission_ApplyStringLocalization(&baseName[pathIndex + 1]);
	return 1;
}

// FUNCTION: XWA 0x421A80
int Mission_ApplyStringLocalization(char* fileName) {
	unsigned int messageIdx;
	unsigned int teamIdx;
	unsigned int fgIdx;
	unsigned int groupIdx;
	unsigned int slotIdx;
	unsigned int goalIdx;
	unsigned int conditionIdx;
	unsigned int textIdx;
	unsigned int iffIdx;
	unsigned int rgnIdx;
	unsigned int unitIdx;
	unsigned int cargoIdx;
	char baseName[128];
	char key[32768];
	char firstChar;
	char markerChar;
	char regionChar;
	int battleNo;
	int missionNo;
	char useFilenameKey;
	int dotIndex;

	useFilenameKey = 0;
	if (!Linez_IsLoaded()) {
		return 0;
	}

	dotIndex = 0;

	strcpy(baseName, fileName);
	if (strlen(baseName) != 0) {
		while (baseName[dotIndex] != '.') {
			++dotIndex;
			if (dotIndex >= (int)strlen(baseName)) {
				break;
			}
		}
		if (baseName[dotIndex] == '.') {
			baseName[dotIndex] = '\0';
		}
	}

	sscanf(fileName, "%c%c%d%c%d", &firstChar, &markerChar, &battleNo, &regionChar, &missionNo);
	if (markerChar != 'b' && markerChar != 'B') {
		useFilenameKey = 1;
	}

	for (messageIdx = 0; messageIdx < 64; ++messageIdx) {
		XwaMessage* message;

		message = &g_missionMessages[messageIdx];
		if (message->message[0] != '\0') {
			char* resolvedText;

			if (useFilenameKey) {
				sprintf(key, "!R_%s_%d!", baseName, messageIdx + 1);
			} else {
				sprintf(key, "!R0%d0%d%2d!", (uint8_t)battleNo, (uint8_t)missionNo, messageIdx + 1);
				if (key[6] == ' ') {
					key[6] = '0';
				}
			}
			strcat(key, message->message);
			resolvedText = Linez_ResolveString(key);
			strncpy(message->message, resolvedText, 0x50u);
			message->message[79] = '\0';
		}
	}

	for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
		XwaTeam* team;
		char* resolvedText;

		team = &g_missionTeams[teamIdx];
		sprintf(key, "!%s_G%d!", baseName, teamIdx + 1);
		strcat(key, team->name);
		resolvedText = Linez_ResolveString(key);
		strncpy(team->name, resolvedText, 0x10u);
		team->name[15] = '\0';

		if (team->endOfMissionMessages[0][0] != '\0') {
			if (useFilenameKey) {
				sprintf(key, "!M_%s_%d!", baseName, 1);
			} else {
				sprintf(key, "!M0%d0%d0%d!", (uint8_t)battleNo, (uint8_t)missionNo, 1);
			}
			strcat(key, team->endOfMissionMessages[0]);
			resolvedText = Linez_ResolveString(key);
			strncpy(team->endOfMissionMessages[0], resolvedText, 0x40u);
			team->endOfMissionMessages[0][63] = '\0';
		}
		if (team->endOfMissionMessages[1][0] != '\0') {
			if (useFilenameKey) {
				sprintf(key, "!M_%s_%d!", baseName, 2);
			} else {
				sprintf(key, "!M0%d0%d0%d!", (uint8_t)battleNo, (uint8_t)missionNo, 2);
			}
			strcat(key, team->endOfMissionMessages[1]);
			resolvedText = Linez_ResolveString(key);
			strncpy(team->endOfMissionMessages[1], resolvedText, 0x40u);
			team->endOfMissionMessages[1][63] = '\0';
		}
		if (team->endOfMissionMessages[2][0] != '\0') {
			if (useFilenameKey) {
				sprintf(key, "!M_%s_%d!", baseName, 3);
			} else {
				sprintf(key, "!M0%d0%d0%d!", (uint8_t)battleNo, (uint8_t)missionNo, 3);
			}
			strcat(key, team->endOfMissionMessages[2]);
			resolvedText = Linez_ResolveString(key);
			strncpy(team->endOfMissionMessages[2], resolvedText, 0x40u);
			team->endOfMissionMessages[2][63] = '\0';
		}
		if (team->endOfMissionMessages[3][0] != '\0') {
			if (useFilenameKey) {
				sprintf(key, "!M_%s_%d!", baseName, 4);
			} else {
				sprintf(key, "!M0%d0%d0%d!", (uint8_t)battleNo, (uint8_t)missionNo, 4);
			}
			strcat(key, team->endOfMissionMessages[3]);
			resolvedText = Linez_ResolveString(key);
			strncpy(team->endOfMissionMessages[3], resolvedText, 0x40u);
			team->endOfMissionMessages[3][63] = '\0';
		}
		if (team->endOfMissionMessages[4][0] != '\0') {
			if (useFilenameKey) {
				sprintf(key, "!M_%s_%d!", baseName, 5);
			} else {
				sprintf(key, "!M0%d0%d0%d!", (uint8_t)battleNo, (uint8_t)missionNo, 5);
			}
			strcat(key, team->endOfMissionMessages[4]);
			resolvedText = Linez_ResolveString(key);
			strncpy(team->endOfMissionMessages[4], resolvedText, 0x40u);
			team->endOfMissionMessages[4][63] = '\0';
		}
		if (team->endOfMissionMessages[5][0] != '\0') {
			if (useFilenameKey) {
				sprintf(key, "!M_%s_%d!", baseName, 6);
			} else {
				sprintf(key, "!M0%d0%d0%d!", (uint8_t)battleNo, (uint8_t)missionNo, 6);
			}
			strcat(key, team->endOfMissionMessages[5]);
			resolvedText = Linez_ResolveString(key);
			strncpy(team->endOfMissionMessages[5], resolvedText, 0x40u);
			team->endOfMissionMessages[5][63] = '\0';
		}
	}

	for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
		for (groupIdx = 0; groupIdx < 8; ++groupIdx) {
			for (slotIdx = 0; slotIdx < 3; ++slotIdx) {
				MemoryHandle* handle;

				handle = &g_missionFgOverrideStringHandles[fgIdx][groupIdx][slotIdx];
				if (*handle != 0) {
					const char* originalText;
					const char* resolvedText;
					size_t resolvedSize;
					MemoryHandle newHandle;

					originalText = (const char*)Memory_LockHandle(*handle);
					sprintf(key, "!%s_O%d_%d!", baseName, fgIdx + 1, groupIdx * 3 + slotIdx + 1);
					strcat(key, originalText);
					resolvedText = Linez_ResolveString(key);
					Memory_UnlockHandle(*handle);
					resolvedSize = strlen(resolvedText) + 1u;
					Memory_FreeHandle("OVERRIDESTRING", *handle);
					newHandle = Memory_AllocHandleZeroed("OVERRIDESTRING", resolvedSize);
					*handle = newHandle;
					if (newHandle != 0) {
						strcpy((char*)Memory_LockHandle(newHandle), resolvedText);
						Memory_UnlockHandle(*handle);
					}
				}
			}
		}
	}

	for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
		for (goalIdx = 0; goalIdx < 7; ++goalIdx) {
			for (conditionIdx = 0; conditionIdx < 4; ++conditionIdx) {
				for (textIdx = 0; textIdx < 3; ++textIdx) {
					MemoryHandle* handle;

					handle = &g_globalGoalOverrideStringHandles[teamIdx][goalIdx][conditionIdx][textIdx];
					if (*handle != 0) {
						const char* originalText;
						const char* resolvedText;
						size_t resolvedSize;
						MemoryHandle newHandle;

						originalText = (const char*)Memory_LockHandle(*handle);
						sprintf(key, "!%s_C%d_%d!", baseName, teamIdx + 1,
								(goalIdx * 4 + conditionIdx) * 3 + textIdx + 1);
						strcat(key, originalText);
						resolvedText = Linez_ResolveString(key);
						Memory_UnlockHandle(*handle);
						resolvedSize = strlen(resolvedText) + 1u;
						Memory_FreeHandle("GLOBALOVERRIDESTRING", *handle);
						newHandle = Memory_AllocHandleZeroed("GLOBALOVERRIDESTRING", resolvedSize);
						*handle = newHandle;
						if (newHandle != 0) {
							strcpy((char*)Memory_LockHandle(newHandle), resolvedText);
							Memory_UnlockHandle(*handle);
						}
					}
				}
			}
		}
	}

	for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
		for (groupIdx = 0; groupIdx < 4; ++groupIdx) {
			for (slotIdx = 0; slotIdx < 4; ++slotIdx) {
				MemoryHandle* handle;

				handle = &g_missionOrderStringHandles[fgIdx][groupIdx][slotIdx];
				if (*handle != 0) {
					const char* originalText;
					const char* resolvedText;
					size_t resolvedSize;
					MemoryHandle newHandle;

					originalText = (const char*)Memory_LockHandle(*handle);
					sprintf(key, "!%s_A%d_%d!", baseName, fgIdx + 1, groupIdx * 4 + slotIdx + 1);
					strcat(key, originalText);
					resolvedText = Linez_ResolveString(key);
					Memory_UnlockHandle(*handle);
					resolvedSize = strlen(resolvedText) + 1u;
					Memory_FreeHandle("AIOVERRIDESTRING", *handle);
					newHandle = Memory_AllocHandleZeroed("AIOVERRIDESTRING", resolvedSize);
					*handle = newHandle;
					if (newHandle != 0) {
						strcpy((char*)Memory_LockHandle(newHandle), resolvedText);
						Memory_UnlockHandle(*handle);
					}
				}
			}
		}
	}

	for (iffIdx = 0; iffIdx < 4; ++iffIdx) {
		if (g_missionHeader.body.iffNames[iffIdx][0] != '\0') {
			char* resolvedText;

			sprintf(key, "!%s_N%d!", baseName, iffIdx + 1);
			strcat(key, g_missionHeader.body.iffNames[iffIdx]);
			resolvedText = Linez_ResolveString(key);
			strncpy(g_missionHeader.body.iffNames[iffIdx], resolvedText, 0x14u);
			g_missionHeader.body.iffNames[iffIdx][19] = '\0';
		}
	}

	for (rgnIdx = 0; rgnIdx < 4; ++rgnIdx) {
		if (g_missionHeader.body.regions[rgnIdx].name[0] != '\0') {
			char* resolvedText;

			sprintf(key, "!%s_I%d!", baseName, rgnIdx + 1);
			strcat(key, g_missionHeader.body.regions[rgnIdx].name);
			resolvedText = Linez_ResolveString(key);
			strncpy(g_missionHeader.body.regions[rgnIdx].name, resolvedText, 0x40u);
			g_missionHeader.body.regions[rgnIdx].name[63] = '\0';
		}
	}

	for (groupIdx = 0; groupIdx < 32; ++groupIdx) {
		XwaGlobalUnit* globalGroup;

		globalGroup = &g_missionHeader.body.globalGroups[groupIdx];
		if (globalGroup->name[0] != '\0') {
			char* resolvedText;

			sprintf(key, "!%s_S%d!", baseName, groupIdx + 1);
			strcat(key, globalGroup->name);
			resolvedText = Linez_ResolveString(key);
			strncpy(globalGroup->name, resolvedText, 0x40u);
			globalGroup->name[63] = '\0';
		}
		if (globalGroup->specialCargo[0] != '\0') {
			char* resolvedText;

			sprintf(key, "!%s_S%d!", baseName, groupIdx + 1);
			strcat(key, globalGroup->specialCargo);
			resolvedText = Linez_ResolveString(key);
			strncpy(globalGroup->specialCargo, resolvedText, 0x14u);
			globalGroup->specialCargo[19] = '\0';
		}
	}

	for (unitIdx = 0; unitIdx < 40; ++unitIdx) {
		XwaGlobalUnit* globalUnit;

		globalUnit = &g_missionHeader.body.globalUnits[unitIdx];
		if (globalUnit->name[0] != '\0') {
			char* resolvedText;

			sprintf(key, "!%s_S%d!", baseName, unitIdx + 1);
			strcat(key, globalUnit->name);
			resolvedText = Linez_ResolveString(key);
			strncpy(globalUnit->name, resolvedText, 0x40u);
			globalUnit->name[63] = '\0';
		}
		if (globalUnit->specialCargo[0] != '\0') {
			char* resolvedText;

			sprintf(key, "!%s_Q%d!", baseName, unitIdx + 1);
			strcat(key, globalUnit->specialCargo);
			resolvedText = Linez_ResolveString(key);
			strncpy(globalUnit->specialCargo, resolvedText, 0x14u);
			globalUnit->specialCargo[19] = '\0';
		}
	}

	for (cargoIdx = 0; cargoIdx < 16; ++cargoIdx) {
		if (g_missionHeader.body.globalCargos[cargoIdx].name[0] != '\0') {
			char* resolvedText;

			sprintf(key, "!%s_X%d!", baseName, cargoIdx + 1);
			strcat(key, g_missionHeader.body.globalCargos[cargoIdx].name);
			resolvedText = Linez_ResolveString(key);
			strncpy(g_missionHeader.body.globalCargos[cargoIdx].name, resolvedText, 0x40u);
			g_missionHeader.body.globalCargos[cargoIdx].name[63] = '\0';
		}
	}

	for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
		XwaFlightGroup* fg;

		fg = &g_missionFlightGroups[fgIdx].fg;
		if (fg->name[0] != '\0') {
			char* resolvedText;

			sprintf(key, "!%s_F%d_1!", baseName, fgIdx + 1);
			strcat(key, fg->name);
			resolvedText = Linez_ResolveString(key);
			strncpy(fg->name, resolvedText, 0x14u);
			fg->name[19] = '\0';
		}
		if (fg->cargo[0] != '\0') {
			char* resolvedText;

			sprintf(key, "!%s_F%d_3!", baseName, fgIdx + 1);
			strcat(key, fg->cargo);
			resolvedText = Linez_ResolveString(key);
			strncpy(fg->cargo, resolvedText, 0x14u);
			fg->cargo[19] = '\0';
		}
		if (fg->specialCargo[0] != '\0') {
			char* resolvedText;

			sprintf(key, "!%s_F%d_4!", baseName, fgIdx + 1);
			strcat(key, fg->specialCargo);
			resolvedText = Linez_ResolveString(key);
			strncpy(fg->specialCargo, resolvedText, 0x14u);
			fg->specialCargo[19] = '\0';
		}
		if (fg->craftRole[0] != '\0') {
			char* resolvedText;

			sprintf(key, "!%s_F%d_5!", baseName, fgIdx + 1);
			strcat(key, fg->craftRole);
			resolvedText = Linez_ResolveString(key);
			strncpy(fg->craftRole, resolvedText, 0x19u);
			fg->craftRole[24] = '\0';
		}
		if (fg->pilotID[0] != '\0') {
			char* resolvedText;

			sprintf(key, "!%s_F%d_6!", baseName, fgIdx + 1);
			strcat(key, fg->pilotID);
			resolvedText = Linez_ResolveString(key);
			strncpy(fg->pilotID, resolvedText, 0x14u);
			fg->pilotID[19] = '\0';
		}
	}

	return 1;
}

// FUNCTION: XWA 0x4D7460
uint16_t Mission_FlightGroupMatchesTriggerVariable(uint16_t fgIdx, uint16_t variableType, uint16_t variable) {
	uint8_t status1;
	uint16_t objectType;
	uint16_t triggerVariableType;
	int result;

	status1 = g_missionFlightGroups[fgIdx].fg.status1;
	if (status1 == 27) {
		return 0;
	}

	triggerVariableType = (uint16_t)variableType;
	objectType = g_objectTypeTables.craftTypeToObjectType[g_missionFlightGroups[fgIdx].fg.craftType];
	result = 0;
	switch (triggerVariableType) {
		case TRIGVAR_NONE:
			return result;

		case TRIGVAR_FLIGHT_GROUP:
			if (variable == fgIdx) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_SHIP_TYPE:
			if (g_missionFormatVersion < 16) {
				if (g_objectTypeTables.craftTypeToObjectType[(uint16_t)variable + 1] == objectType) {
					result = 1;
					return result;
				}
				break;
			}
			if (g_objectTypeTables.craftTypeToObjectType[variable] == objectType) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_SHIP_CLASS:
			if (g_genusConvert[variable] == g_modelTypeTable[(uint16_t)objectType].genusId) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_OBJECT_TYPE:
			if (g_familyConvert[variable] == g_modelTypeTable[(uint16_t)objectType].familyId) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_IFF:
			if (variable == g_missionFlightGroups[fgIdx].fg.iff) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_SHIP_ORDERS:
			if (variable == g_missionFlightGroups[fgIdx].fg.orders[0].order) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_CRAFT_WHEN:
			if (variable == 9) {
				if (g_missionFlightGroups[fgIdx].playerOwnerIdx != -1) {
					result = 1;
					return result;
				}
				break;
			}
			if (variable == 10) {
				if (g_missionFlightGroups[fgIdx].playerOwnerIdx == -1) {
					result = 1;
					return result;
				}
				break;
			}
			result = 1;
			break;

		case TRIGVAR_GLOBAL_GROUP:
			if (variable == g_missionFlightGroups[fgIdx].fg.globalGroup) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_AI_LEVEL:
			if (variable == g_missionFlightGroups[fgIdx].fg.groupAI) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_STATUS:
			if (variable == status1) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_ALL_CRAFT:
			result = 1;
			break;

		case TRIGVAR_TEAM:
			if (variable == g_missionFlightGroups[fgIdx].fg.team) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_NOT_FLIGHT_GROUP:
			if (variable != fgIdx) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_NOT_SHIP_TYPE:
			if (g_missionFormatVersion < 16) {
				if (g_objectTypeTables.craftTypeToObjectType[(uint16_t)variable + 1] != objectType) {
					result = 1;
					return result;
				}
				break;
			}
			if (g_objectTypeTables.craftTypeToObjectType[variable] != objectType) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_NOT_SHIP_CLASS:
			if (g_genusConvert[variable] != g_modelTypeTable[(uint16_t)objectType].genusId) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_NOT_OBJECT_TYPE:
			if (g_familyConvert[variable] != g_modelTypeTable[(uint16_t)objectType].familyId) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_NOT_IFF:
			if (variable != g_missionFlightGroups[fgIdx].fg.iff) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_NOT_GLOBAL_GROUP:
			if (variable != g_missionFlightGroups[fgIdx].fg.globalGroup) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_NOT_TEAM:
			if (variable != g_missionFlightGroups[fgIdx].fg.team) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_GLOBAL_UNIT:
			if (variable == g_missionFlightGroups[fgIdx].fg.globalUnit) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_NOT_GLOBAL_UNIT:
			if (variable != g_missionFlightGroups[fgIdx].fg.globalUnit) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_GLOBAL_CARGO:
			if (variable == g_missionFlightGroups[fgIdx].fg.globalCargoIndex) {
				result = 1;
				break;
			}
			if (variable == g_missionFlightGroups[fgIdx].fg.globalSpecialCargoIndex) {
				result = 1;
				return result;
			}
			break;

		case TRIGVAR_NOT_GLOBAL_CARGO:
			if (variable != g_missionFlightGroups[fgIdx].fg.globalCargoIndex &&
				variable != g_missionFlightGroups[fgIdx].fg.globalSpecialCargoIndex) {
				result = 1;
				return result;
			}
			break;

		default:
			break;
	}

	return result;
}

// FUNCTION: XWA 0x4D7860
int Mission_ObjectMatchesTriggerVariable(uint16_t objectIdx, uint16_t variableType, uint16_t variable) {
	return Mission_ObjectMatchesTriggerVariableEx(objectIdx, variableType, variable, 0xffffu);
}

// FUNCTION: XWA 0x4D7880
int Mission_ObjectMatchesTriggerVariableEx(uint16_t objectIdx, uint16_t variableType, uint16_t variable,
										   uint16_t contextFgIdx) {
	ObjectRecord* object;
	MobileObject* mobj;
	CraftData* craft;
	uint16_t flightGroupIdx;
	uint16_t team;
	uint16_t objectType;
	int result;

	flightGroupIdx = g_objectTable[objectIdx].flightGroupIdx;
	object = &g_objectTable[objectIdx];
	mobj = object->mobj;
	if (mobj != NULL) {
		craft = mobj->pCraft;
		team = mobj->team;
	} else {
		craft = NULL;
		team = g_missionFlightGroups[flightGroupIdx].fg.team;
	}

	objectType = g_objectTypeTables.craftTypeToObjectType[g_missionFlightGroups[flightGroupIdx].fg.craftType];
	result = 0;
	switch ((uint16_t)variableType) {
		case TRIGVAR_NONE:
			break;

		case TRIGVAR_FLIGHT_GROUP:
			if (variable == flightGroupIdx) {
				return 1;
			}
			break;

		case TRIGVAR_SHIP_TYPE:
			if (g_missionFormatVersion < 16) {
				if (g_objectTypeTables.craftTypeToObjectType[(uint16_t)variable + 1] == objectType) {
					return 1;
				}
				break;
			}
			if (g_objectTypeTables.craftTypeToObjectType[variable] == objectType) {
				return 1;
			}
			break;

		case TRIGVAR_SHIP_CLASS:
			if (g_genusConvert[variable] == g_modelTypeTable[(uint16_t)objectType].genusId) {
				return 1;
			}
			break;

		case TRIGVAR_OBJECT_TYPE:
			if (g_familyConvert[variable] == g_modelTypeTable[(uint16_t)objectType].familyId) {
				return 1;
			}
			break;

		case TRIGVAR_IFF:
			if (mobj == NULL) {
				if (variable == g_missionFlightGroups[flightGroupIdx].fg.iff) {
					return 1;
				}
				break;
			}
			if (variable == (uint8_t)mobj->iff) {
				return 1;
			}
			break;

		case TRIGVAR_SHIP_ORDERS:
			if (variable == g_missionFlightGroups[flightGroupIdx].fg.orders[0].order) {
				return 1;
			}
			break;

		case TRIGVAR_CRAFT_WHEN:
			if (mobj == NULL) {
				break;
			}
			switch (variable) {
				uint16_t i;
				uint32_t hullThreshold;

				case 0:
					if (craft->wasCaptured != 0) {
						return 1;
					}
					break;
				case 1:
					result = 0;
					for (i = 0; i < 10; ++i) {
						if (i != mobj->team && (int8_t)craft->iffVisibility[i] > 0) {
							result = 1;
						}
					}
					return result;
				case 2:
					if (craft->aiFlight.orderActionCounter != 0) {
						return 1;
					}
					break;
				case 3:
					if (craft->aiFlight.objSignatureCount != 0) {
						return 1;
					}
					break;
				case 4:
					if (craft->workingSubsystems == 0) {
						return 1;
					}
					break;
				case 5:
					result = 0;
					for (i = 0; i < 10; ++i) {
						if (i != mobj->team && craft->attackedByTeam[i] != 0) {
							result = 1;
						}
					}
					return result;
				case 6:
					if (craft->hullDamage != 0) {
						return 1;
					}
					break;
				case 7:
					if (craft->waveNumber == g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft) {
						return 1;
					}
					break;
				case 8:
					if (craft->waveNumber != g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft) {
						return 1;
					}
					break;
				case 9:
					if (object->playerOwnerIdx != -1) {
						return 1;
					}
					break;
				case 10:
					if (object->playerOwnerIdx == -1) {
						return 1;
					}
					break;
				case 11:
					result = 0;
					for (i = 0; i < 10; ++i) {
						if (craft->attackedByTeam[i] == 0) {
							result = 1;
						}
					}
					return result;
				case 12:
					if (craft->workingSubsystems != 0) {
						return 1;
					}
					break;
				case 13:
					if (craft->wasCaptured == 0) {
						return 1;
					}
					break;
				case 14:
					result = 0;
					for (i = 0; i < 10; ++i) {
						if ((int8_t)craft->iffVisibility[i] < 1) {
							result = 1;
						}
					}
					return result;
				case 15:
				case 17:
					break;
				case 16:
					if (craft->aiFlight.orderActionCounter == 0) {
						return 1;
					}
					break;
				case 18:
					if (craft->aiFlight.objSignatureCount == 0) {
						return 1;
					}
					break;
				case 22:
					hullThreshold = (uint32_t)craft->hullMax >> 2;
					goto compareHullThreshold;
				case 23:
					if ((uint32_t)craft->hullDamage >= (uint32_t)craft->hullMax >> 1) {
						return 1;
					}
					break;
				case 24:
					hullThreshold = MATH2_longfraction((uint32_t)craft->hullMax, 0xc000u);
				compareHullThreshold:
					if ((uint32_t)craft->hullDamage >= hullThreshold) {
						result = 1;
					}
					break;
				case 25: {
					uint16_t warheadLauncherCount;
					uint8_t* warheadLauncherFirstSlot;
					int lastSlot;
					int16_t firstCount;
					int16_t count;

					warheadLauncherCount = (uint8_t)craft->warheadLauncherCount;
					count = 0;
					if (warheadLauncherCount > 0) {
						warheadLauncherFirstSlot = g_modelDefs[craft->modelIndex].warheadLauncherFirstSlot;
						do {
							lastSlot = warheadLauncherFirstSlot[2];
							firstCount = craft->warheadData[*warheadLauncherFirstSlot++].count;
							--warheadLauncherCount;
							count += firstCount + craft->warheadData[lastSlot].count;
						} while (warheadLauncherCount != 0);
					}
					if (count == 0) {
						return 1;
					}
					break;
				}
				case 27:
					result = 0;
					for (i = 0; i < 10; ++i) {
						if (i != mobj->team && (int8_t)craft->iffVisibility[i] >= 0) {
							result = 1;
						}
					}
					return result;
				case 28:
					result = 0;
					for (i = 0; i < 10; ++i) {
						if ((int8_t)craft->iffVisibility[i] < 0) {
							result = 1;
						}
					}
					return result;
				default:
					break;
			}
			break;

		case TRIGVAR_GLOBAL_GROUP:
			if (variable == g_missionFlightGroups[flightGroupIdx].fg.globalGroup) {
				return 1;
			}
			break;

		case TRIGVAR_AI_LEVEL:
			if (variable == g_missionFlightGroups[flightGroupIdx].fg.groupAI) {
				return 1;
			}
			break;

		case TRIGVAR_STATUS:
			if (variable == g_missionFlightGroups[flightGroupIdx].fg.status1) {
				return 1;
			}
			break;

		case TRIGVAR_ALL_CRAFT:
			return 1;

		case TRIGVAR_TEAM:
			if (variable >= 10u) {
				if (contextFgIdx != 0xffffu &&
					g_missionTeams[g_missionFlightGroups[contextFgIdx].fg.team].allies[team] == 0) {
					return 1;
				}
				break;
			}
			if (variable == team) {
				return 1;
			}
			break;

		case TRIGVAR_PLAYER_NUM:
			if (variable ==
				g_missionFlightGroups[g_players[object->playerOwnerIdx].boundFlightGroupIdx].fg.playerNumber -
					1) {
				return 1;
			}
			break;

		case TRIGVAR_BEFORE_TIME: {
			uint16_t triggerMinutes;
			uint16_t triggerSeconds;
			uint8_t elapsedMinutes;

			triggerMinutes = (uint16_t)(5u * variable) / 60u;
			elapsedMinutes = g_missionElapsedClock.minutes;
			if (triggerMinutes < elapsedMinutes) {
				return 0;
			}
			if (triggerMinutes <= elapsedMinutes) {
				triggerSeconds = (uint16_t)(5u * variable) % 60u;
				return triggerSeconds >= g_missionElapsedClock.seconds;
			}
			return 1;
		}

		case TRIGVAR_NOT_FLIGHT_GROUP:
			if (variable != flightGroupIdx) {
				return 1;
			}
			break;

		case TRIGVAR_NOT_SHIP_TYPE:
			if (g_missionFormatVersion < 16) {
				if (g_objectTypeTables.craftTypeToObjectType[(uint16_t)variable + 1] != objectType) {
					return 1;
				}
				break;
			}
			if (g_objectTypeTables.craftTypeToObjectType[variable] != objectType) {
				return 1;
			}
			break;

		case TRIGVAR_NOT_SHIP_CLASS:
			if (g_genusConvert[variable] != g_modelTypeTable[(uint16_t)objectType].genusId) {
				return 1;
			}
			break;

		case TRIGVAR_NOT_OBJECT_TYPE:
			if (g_familyConvert[variable] != g_modelTypeTable[(uint16_t)objectType].familyId) {
				return 1;
			}
			break;

		case TRIGVAR_NOT_IFF:
			if (mobj == NULL) {
				if (variable != g_missionFlightGroups[flightGroupIdx].fg.iff) {
					return 1;
				}
				break;
			}
			if (variable != (uint8_t)mobj->iff) {
				return 1;
			}
			break;

		case TRIGVAR_NOT_GLOBAL_GROUP:
			if (variable != g_missionFlightGroups[flightGroupIdx].fg.globalGroup) {
				return 1;
			}
			break;

		case TRIGVAR_NOT_TEAM:
			if (variable != team) {
				return 1;
			}
			break;

		case TRIGVAR_NOT_PLAYER_NUM:
			if (variable != object->playerOwnerIdx) {
				return 1;
			}
			break;

		case TRIGVAR_GLOBAL_UNIT:
			if (variable == g_missionFlightGroups[flightGroupIdx].fg.globalUnit) {
				return 1;
			}
			break;

		case TRIGVAR_NOT_GLOBAL_UNIT:
			if (variable != g_missionFlightGroups[flightGroupIdx].fg.globalUnit) {
				return 1;
			}
			break;

		case TRIGVAR_GLOBAL_CARGO:
			if (mobj == NULL) {
				break;
			}
			if (variable == craft->cargoIndex) {
				return 1;
			}
			break;

		case TRIGVAR_NOT_GLOBAL_CARGO:
			if (mobj == NULL) {
				break;
			}
			if (variable != craft->cargoIndex) {
				return 1;
			}
			break;

		default:
			break;
	}

	return result;
}

typedef enum MissionConditionType {
	MISSION_COND_ALWAYS_TRUE = 0,
	MISSION_COND_ARRIVED = 1,
	MISSION_COND_DESTROYED = 2,
	MISSION_COND_ATTACKED = 3,
	MISSION_COND_CAPTURED = 4,
	MISSION_COND_INSPECTED = 5,
	MISSION_COND_BOARDED = 6,
	MISSION_COND_DOCKED = 7,
	MISSION_COND_DISABLED = 8,
	MISSION_COND_SURVIVED = 9,
	MISSION_COND_NEVER_FALSE = 10,
	MISSION_COND_12 = 12,
	MISSION_COND_13 = 13,
	MISSION_COND_14 = 14,
	MISSION_COND_17 = 17,
	MISSION_COND_18 = 18,
	MISSION_COND_20 = 20,
	MISSION_COND_SHIELDS_DEPLETED = 21,
	MISSION_COND_HULL_ABOVE_50 = 22,
	MISSION_COND_NO_WARHEADS = 23,
	MISSION_COND_SYSTEM_DAMAGED = 24,
	MISSION_COND_25 = 25,
	MISSION_COND_26 = 26,
	MISSION_COND_27 = 27,
	MISSION_COND_28 = 28,
	MISSION_COND_29 = 29,
	MISSION_COND_30 = 30,
	MISSION_COND_31 = 31,
	MISSION_COND_32 = 32,
	MISSION_COND_33 = 33,
	MISSION_COND_SHIELDS_BELOW_50 = 34,
	MISSION_COND_SHIELDS_BELOW_25 = 35,
	MISSION_COND_HULL_ABOVE_25 = 36,
	MISSION_COND_HULL_ABOVE_75 = 37,
	MISSION_COND_38 = 38,
	MISSION_COND_41 = 41,
	MISSION_COND_42 = 42,
	MISSION_COND_43 = 43,
	MISSION_COND_44 = 44,
	MISSION_COND_45 = 45,
	MISSION_COND_46 = 46,
	MISSION_COND_47 = 47,
	MISSION_COND_48 = 48,
	MISSION_COND_49 = 49,
	MISSION_COND_50 = 50,
	MISSION_COND_52 = 52,
	MISSION_COND_54 = 54,
	MISSION_COND_55 = 55,
	MISSION_COND_56 = 56,
	MISSION_COND_57 = 57,
	MISSION_COND_58 = 58,
	MISSION_COND_59 = 59,
} MissionConditionType;

typedef enum MissionGoalAmount {
	MISSION_GOAL_AMT_100 = 0,
	MISSION_GOAL_AMT_75 = 1,
	MISSION_GOAL_AMT_50 = 2,
	MISSION_GOAL_AMT_25 = 3,
	MISSION_GOAL_AMT_AT_LEAST_1 = 4,
	MISSION_GOAL_AMT_ALL_BUT_1 = 5,
	MISSION_GOAL_AMT_ALL_SPECIAL_CARGO = 6,
	MISSION_GOAL_AMT_ALL_NON_SPECIAL = 7,
	MISSION_GOAL_AMT_100_OF_SUBSET = 10,
	MISSION_GOAL_AMT_75_OF_SUBSET = 11,
	MISSION_GOAL_AMT_50_OF_SUBSET = 12,
	MISSION_GOAL_AMT_25_OF_SUBSET = 13,
	MISSION_GOAL_AMT_AT_LEAST_1_ALT = 14,
	MISSION_GOAL_AMT_ALL_BUT_1_OF_SUBSET = 15,
	MISSION_GOAL_AMT_66 = 16,
	MISSION_GOAL_AMT_33 = 17,
} MissionGoalAmount;

// GLOBAL: XWA 0x5BA898
const uint8_t g_missionConditionUsesCountByTriggerType[64] = {
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0,
};

// FUNCTION: XWA 0x4D5AE0
int Mission_EvaluateCondition(const XwaTrigger* trigger, uint8_t includeDepartedAsDestroyed,
							  uint16_t teamOrVariable) {
	MissionConditionType condition;
	uint8_t variableType;
	uint8_t variable;
	MissionGoalAmount amount;
	int16_t parameter;
	uint16_t fgIdx;
	uint32_t met;
	uint32_t metSpecialCargo;
	uint32_t failed;
	uint32_t failedSpecialCargo;
	uint32_t total;
	uint32_t totalSpecialCargo;
	uint32_t subsetTotal;
	uint16_t status;

	met = 0;
	condition = (MissionConditionType)trigger->condition;
	variableType = trigger->variableType;
	variable = trigger->variable;
	amount = (MissionGoalAmount)trigger->amount;
	parameter = trigger->parameter;

	g_missionConditionCurrentCount = (uint16_t)met;
	g_missionConditionTotalCount = (uint16_t)met;

	if (g_missionConditionUsesCountByTriggerType[(uint8_t)condition]) {
		if (variableType == TRIGVAR_NONE) {
			return 2;
		}

		metSpecialCargo = 0;
		failed = 0;
		failedSpecialCargo = 0;
		total = 0;
		totalSpecialCargo = 0;
		subsetTotal = 0;

		if (variableType != TRIGVAR_GLOBAL_CARGO) {
			for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
				if (g_missionFlightGroups[fgIdx].fg.craftType != 0 &&
					g_missionFgStats[fgIdx].arrivalEnabled &&
					Mission_FlightGroupMatchesTriggerVariable(fgIdx, (MissionTriggerVariableType)variableType,
															  variable)) {
					total += g_missionFgStats[fgIdx].outcomeCount[0];
					totalSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[0];
					subsetTotal += g_missionFgStats[fgIdx].outcomeCount[1];

					switch (condition) {
						case MISSION_COND_ARRIVED:
							met += g_missionFgStats[fgIdx].outcomeCount[1];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[1];
							break;
						case MISSION_COND_DESTROYED:
							met += g_missionFgStats[fgIdx].outcomeCount[2];
							met += g_missionFgStats[fgIdx].outcomeCount[3];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[2];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[3];
							if (includeDepartedAsDestroyed) {
								met += g_missionFgStats[fgIdx].outcomeCount[17];
								met += g_missionFgStats[fgIdx].outcomeCount[18];
								met += g_missionFgStats[fgIdx].outcomeCount[19];
								met += g_missionFgStats[fgIdx].outcomeCount[20];
								metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[17];
								metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[18];
								metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[19];
								metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[20];
							} else {
								failed += g_missionFgStats[fgIdx].outcomeCount[17];
								failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[17];
							}
							break;
						case MISSION_COND_ATTACKED:
							met += g_missionFgStats[fgIdx].outcomeCount[4];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[4];
							failed += g_missionFgStats[fgIdx].outcomeCount[5];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[5];
							break;
						case MISSION_COND_CAPTURED:
							met += g_missionFgStats[fgIdx].outcomeCount[6];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[6];
							failed += g_missionFgStats[fgIdx].outcomeCount[7];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[7];
							break;
						case MISSION_COND_INSPECTED:
							if (teamOrVariable < 10u) {
								met += g_missionFgStats[fgIdx].teamInspected[teamOrVariable];
								metSpecialCargo +=
									g_missionFgStats[fgIdx].teamSpecialCargoInspected[teamOrVariable];
								failed += g_missionFgStats[fgIdx].teamUninspectedLost[teamOrVariable];
								failedSpecialCargo +=
									g_missionFgStats[fgIdx].teamSpecialCargoUninspectedLost[teamOrVariable];
							} else {
								met += g_missionFgStats[fgIdx].outcomeCount[8];
								metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[8];
								failed += g_missionFgStats[fgIdx].outcomeCount[9];
								failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[9];
							}
							break;
						case MISSION_COND_BOARDED:
							met += g_missionFgStats[fgIdx].outcomeCount[10];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[10];
							failed += g_missionFgStats[fgIdx].outcomeCount[11];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[11];
							break;
						case MISSION_COND_DOCKED:
							met += g_missionFgStats[fgIdx].outcomeCount[12];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[12];
							failed += g_missionFgStats[fgIdx].outcomeCount[13];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[13];
							break;
						case MISSION_COND_DISABLED:
							met += g_missionFgStats[fgIdx].outcomeCount[14];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[14];
							failed += g_missionFgStats[fgIdx].outcomeCount[15];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[15];
							break;
						case MISSION_COND_SURVIVED:
							met += g_missionFgStats[fgIdx].outcomeCount[0] -
								   g_missionFgStats[fgIdx].outcomeCount[2] -
								   g_missionFgStats[fgIdx].outcomeCount[3];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[0] -
											   g_missionFgStats[fgIdx].specialCargoOutcome[2] -
											   g_missionFgStats[fgIdx].specialCargoOutcome[3];
							failed += g_missionFgStats[fgIdx].outcomeCount[2];
							failed += g_missionFgStats[fgIdx].outcomeCount[3];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[2];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[3];
							break;
						case 12:
							met += g_missionFgStats[fgIdx].outcomeCount[16];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[16];
							failed += g_missionFgStats[fgIdx].outcomeCount[22];
							failed += g_missionFgStats[fgIdx].outcomeCount[21];
							failed += g_missionFgStats[fgIdx].outcomeCount[2];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[22];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[21];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[2];
							break;
						case MISSION_COND_SHIELDS_DEPLETED:
						case MISSION_COND_HULL_ABOVE_50:
						case MISSION_COND_NO_WARHEADS:
						case MISSION_COND_SYSTEM_DAMAGED:
						case MISSION_COND_SHIELDS_BELOW_50:
						case MISSION_COND_SHIELDS_BELOW_25:
						case MISSION_COND_HULL_ABOVE_25:
						case MISSION_COND_HULL_ABOVE_75: {
							uint32_t objectIdx;
							for (objectIdx = 0; objectIdx < g_regionObjectSlotEnd; ++objectIdx) {
								ObjectRecord* object;
								CraftData* craft;
								int maxShield;
								int16_t matchesCondition;

								object = &g_objectTable[objectIdx];
								if (object->objectType == 0 || object->mobj == NULL ||
									object->mobj->pCraft == NULL || object->flightGroupIdx != fgIdx) {
									continue;
								}

								craft = object->mobj->pCraft;
								matchesCondition = 0;
								maxShield = 2 * g_modelDefs[craft->modelIndex].shieldStrength;
								if (condition == MISSION_COND_SHIELDS_DEPLETED) {
									if (craft->shieldFront + craft->shieldRear <= 0) {
										matchesCondition = 1;
									}
								} else if (condition == MISSION_COND_SHIELDS_BELOW_25) {
									if (craft->shieldFront + craft->shieldRear <= maxShield / 4) {
										matchesCondition = 1;
									}
								} else if (condition == MISSION_COND_SHIELDS_BELOW_50) {
									if (craft->shieldFront + craft->shieldRear <= maxShield / 2) {
										matchesCondition = 1;
									}
								} else if (condition == MISSION_COND_HULL_ABOVE_50) {
									if ((uint32_t)craft->hullDamage > (uint32_t)craft->hullMax >> 1) {
										matchesCondition = 1;
									}
								} else if (condition == MISSION_COND_HULL_ABOVE_25) {
									if ((uint32_t)craft->hullDamage > (uint32_t)craft->hullMax >> 2) {
										matchesCondition = 1;
									}
								} else if (condition == MISSION_COND_HULL_ABOVE_75) {
									if ((uint32_t)craft->hullDamage > 3u * ((uint32_t)craft->hullMax >> 2)) {
										matchesCondition = 1;
									}
								} else if (condition == MISSION_COND_NO_WARHEADS) {
									const ModelDef* modelDef;
									uint16_t launcherIdx;
									uint16_t warheadCount;

									modelDef = &g_modelDefs[craft->modelIndex];
									warheadCount = 0;
									for (launcherIdx = 0; launcherIdx < craft->warheadLauncherCount;
										 ++launcherIdx) {
										warheadCount +=
											craft
												->warheadData[modelDef->warheadLauncherFirstSlot[launcherIdx]]
												.count;
										warheadCount +=
											craft->warheadData[modelDef->warheadLauncherLastSlot[launcherIdx]]
												.count;
									}
									if (warheadCount == 0) {
										matchesCondition = 1;
									}
								} else {
									if ((craft->workingSubsystems & 0x10u) == 0) {
										matchesCondition = 1;
									}
								}

								if (matchesCondition) {
									++met;
									if (g_missionFlightGroups[fgIdx].fg.specialCargoCraft ==
										craft->waveNumber) {
										++metSpecialCargo;
									}
								}
							}
							break;
						}
						case 25:
							failed += g_missionFgStats[fgIdx].outcomeCount[1];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[1];
							break;
						case 26:
							met += g_missionFgStats[fgIdx].outcomeCount[5];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[5];
							failed += g_missionFgStats[fgIdx].outcomeCount[4];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[4];
							break;
						case 27:
							met += g_missionFgStats[fgIdx].outcomeCount[15];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[15];
							failed += g_missionFgStats[fgIdx].outcomeCount[14];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[14];
							break;
						case 28:
							met += g_missionFgStats[fgIdx].outcomeCount[7];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[7];
							failed += g_missionFgStats[fgIdx].outcomeCount[6];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[6];
							break;
						case 29:
							if (teamOrVariable < 10u) {
								met += g_missionFgStats[fgIdx].teamUninspectedLost[teamOrVariable];
								metSpecialCargo +=
									g_missionFgStats[fgIdx].teamSpecialCargoUninspectedLost[teamOrVariable];
								failed += g_missionFgStats[fgIdx].teamInspected[teamOrVariable];
								failedSpecialCargo +=
									g_missionFgStats[fgIdx].teamSpecialCargoInspected[teamOrVariable];
							} else {
								met += g_missionFgStats[fgIdx].outcomeCount[9];
								metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[9];
								failed += g_missionFgStats[fgIdx].outcomeCount[8];
								failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[8];
							}
							break;
						case 30:
							met += g_missionFgStats[fgIdx].outcomeCount[25];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[25];
							break;
						case 31:
							met += g_missionFgStats[fgIdx].outcomeCount[11];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[11];
							failed += g_missionFgStats[fgIdx].outcomeCount[10];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[10];
							break;
						case 32:
							met += g_missionFgStats[fgIdx].outcomeCount[26];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[26];
							break;
						case 33:
							met += g_missionFgStats[fgIdx].outcomeCount[13];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[13];
							failed += g_missionFgStats[fgIdx].outcomeCount[12];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[12];
							break;
						case 43:
							met += g_missionFgStats[fgIdx].outcomeCount[17];
							met += g_missionFgStats[fgIdx].outcomeCount[18];
							met += g_missionFgStats[fgIdx].outcomeCount[19];
							met += g_missionFgStats[fgIdx].outcomeCount[20];
							met += g_missionFgStats[fgIdx].outcomeCount[2];
							met += g_missionFgStats[fgIdx].outcomeCount[3];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[17];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[18];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[19];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[20];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[2];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[3];
							break;
						case 44:
							if (teamOrVariable < 10u) {
								met += g_missionFgStats[fgIdx].teamCondition44Count[teamOrVariable];
								metSpecialCargo +=
									g_missionFgStats[fgIdx].teamCondition44SpecialCargo[teamOrVariable];
								failed +=
									g_missionFgStats[fgIdx].teamCondition44OtherTeamCount[teamOrVariable];
								failedSpecialCargo +=
									g_missionFgStats[fgIdx]
										.teamCondition44OtherTeamSpecialCargo[teamOrVariable];
							} else {
								uint16_t teamIdx;
								for (teamIdx = 0; teamIdx < 10u; ++teamIdx) {
									met += g_missionFgStats[fgIdx].teamCondition44Count[teamIdx];
									metSpecialCargo +=
										g_missionFgStats[fgIdx].teamCondition44SpecialCargo[teamIdx];
									if (g_missionFlightRuntimeState.teamHasCountableCraft[teamIdx]) {
										failed +=
											g_missionFgStats[fgIdx].teamCondition44OtherTeamCount[teamIdx];
										failedSpecialCargo +=
											g_missionFgStats[fgIdx]
												.teamCondition44OtherTeamSpecialCargo[teamIdx];
									}
								}
							}
							break;
						case 45:
							met += g_missionFgStats[fgIdx].outcomeCount[22];
							met += g_missionFgStats[fgIdx].outcomeCount[21];
							met += g_missionFgStats[fgIdx].outcomeCount[2];
							met += g_missionFgStats[fgIdx].outcomeCount[3];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[22];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[21];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[2];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[3];
							failed += g_missionFgStats[fgIdx].outcomeCount[16];
							failed += g_missionFgStats[fgIdx].outcomeCount[27];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[16];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[27];
							break;
						case 46:
							met += g_missionFgStats[fgIdx].outcomeCount[23];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[23];
							failed += g_missionFgStats[fgIdx].outcomeCount[24];
							failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[24];
							break;
						case 47:
							met += g_missionFgStats[fgIdx].goalState[parameter + 79];
							metSpecialCargo += g_missionFgStats[fgIdx].tailEventCounts[parameter + 4];
							break;
						case 48:
							met += g_missionFgStats[fgIdx].tailEventCounts[parameter + 9];
							metSpecialCargo += g_missionFgStats[fgIdx].tailEventCounts[parameter + 14];
							break;
						case 52:
							met += g_missionFgStats[fgIdx].outcomeCount[32];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[32];
							break;
						case 54:
							met += g_missionFgStats[fgIdx].outcomeCount[28];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[28];
							break;
						case 55:
							met += g_missionFgStats[fgIdx].outcomeCount[31];
							metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[31];
							break;
						case 57:
							if (teamOrVariable < 10u) {
								met += g_missionFgStats[fgIdx].teamPartiallyInspected[teamOrVariable];
								metSpecialCargo += g_missionFgStats[fgIdx]
													   .teamSpecialCargoPartiallyInspected[teamOrVariable];
								failed += g_missionFgStats[fgIdx].teamPartialInspectLost[teamOrVariable];
								failedSpecialCargo += g_missionFgStats[fgIdx]
														  .teamSpecialCargoPartialInspectLost[teamOrVariable];
							} else {
								met += g_missionFgStats[fgIdx].outcomeCount[29];
								metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[29];
								failed += g_missionFgStats[fgIdx].outcomeCount[30];
								failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[30];
							}
							break;
						case 58:
							if (teamOrVariable < 10u) {
								met += g_missionFgStats[fgIdx].teamPartialInspectLost[teamOrVariable];
								metSpecialCargo += g_missionFgStats[fgIdx]
													   .teamSpecialCargoPartialInspectLost[teamOrVariable];
								failed += g_missionFgStats[fgIdx].teamPartiallyInspected[teamOrVariable];
								failedSpecialCargo += g_missionFgStats[fgIdx]
														  .teamSpecialCargoPartiallyInspected[teamOrVariable];
							} else {
								met += g_missionFgStats[fgIdx].outcomeCount[30];
								metSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[30];
								failed += g_missionFgStats[fgIdx].outcomeCount[29];
								failedSpecialCargo += g_missionFgStats[fgIdx].specialCargoOutcome[29];
							}
							break;
						default:
							break;
					}
				}
			}
		}

		status = 4;
		if ((uint16_t)total != 0) {
			switch (amount) {
				case MISSION_GOAL_AMT_100:
					if ((uint16_t)met >= (uint16_t)total) {
						g_missionConditionCurrentCount = (uint16_t)met;
						g_missionConditionTotalCount = (uint16_t)total;
						return 1;
					} else if ((uint16_t)failed != 0) {
						status = 2;
					}
					break;
				case MISSION_GOAL_AMT_75:
					if ((uint16_t)MATH2_divide((uint16_t)met, (uint16_t)total) >= 0xc000u) {
						g_missionConditionCurrentCount = (uint16_t)met;
						g_missionConditionTotalCount = (uint16_t)total;
						return 1;
					} else if ((uint16_t)MATH2_divide((uint16_t)failed, (uint16_t)total) > 0x4000u) {
						status = 2;
					}
					break;
				case MISSION_GOAL_AMT_50:
					if ((uint16_t)MATH2_divide((uint16_t)met, (uint16_t)total) >= 0x8000u) {
						g_missionConditionCurrentCount = (uint16_t)met;
						g_missionConditionTotalCount = (uint16_t)total;
						return 1;
					} else if ((uint16_t)MATH2_divide((uint16_t)failed, (uint16_t)total) > 0x8000u) {
						status = 2;
					}
					break;
				case MISSION_GOAL_AMT_25:
					if ((uint16_t)MATH2_divide((uint16_t)met, (uint16_t)total) >= 0x4000u) {
						g_missionConditionCurrentCount = (uint16_t)met;
						g_missionConditionTotalCount = (uint16_t)total;
						return 1;
					} else if ((uint16_t)MATH2_divide((uint16_t)failed, (uint16_t)total) > 0xc000u) {
						status = 2;
					}
					break;
				case MISSION_GOAL_AMT_AT_LEAST_1:
				case MISSION_GOAL_AMT_AT_LEAST_1_ALT:
					if ((uint16_t)met != 0) {
						g_missionConditionCurrentCount = (uint16_t)met;
						g_missionConditionTotalCount = (uint16_t)total;
						return 1;
					} else if ((uint16_t)failed == (uint16_t)total) {
						status = 2;
					}
					break;
				case MISSION_GOAL_AMT_ALL_BUT_1:
					if ((uint16_t)met >= (uint16_t)total - 1u) {
						g_missionConditionCurrentCount = (uint16_t)met;
						g_missionConditionTotalCount = (uint16_t)total;
						return 1;
					} else if ((uint16_t)failed > 1u) {
						status = 2;
					}
					break;
				case MISSION_GOAL_AMT_ALL_SPECIAL_CARGO:
					if ((uint16_t)totalSpecialCargo != 0) {
						if ((uint16_t)metSpecialCargo == (uint16_t)totalSpecialCargo) {
							g_missionConditionCurrentCount = (uint16_t)met;
							g_missionConditionTotalCount = (uint16_t)total;
							return 1;
						} else if ((uint16_t)failedSpecialCargo != 0) {
							status = 2;
						}
					}
					break;
				case MISSION_GOAL_AMT_ALL_NON_SPECIAL:
					if ((uint16_t)total - (uint16_t)totalSpecialCargo == (uint16_t)met) {
						g_missionConditionCurrentCount = (uint16_t)met;
						g_missionConditionTotalCount = (uint16_t)total;
						return 1;
					} else if (((uint16_t)failed != 0 && (uint16_t)metSpecialCargo == 0) ||
							   ((uint16_t)failed > 1u && (uint16_t)metSpecialCargo != 0)) {
						status = 2;
					}
					break;
				case MISSION_GOAL_AMT_100_OF_SUBSET:
					if ((uint16_t)met < (uint16_t)subsetTotal) {
						break;
					}
					g_missionConditionCurrentCount = (uint16_t)met;
					g_missionConditionTotalCount = (uint16_t)total;
					return 1;
				case MISSION_GOAL_AMT_75_OF_SUBSET:
					if ((uint16_t)MATH2_divide((uint16_t)met, (uint16_t)subsetTotal) < 0xc000u) {
						break;
					}
					g_missionConditionCurrentCount = (uint16_t)met;
					g_missionConditionTotalCount = (uint16_t)total;
					return 1;
				case MISSION_GOAL_AMT_50_OF_SUBSET:
					if ((uint16_t)MATH2_divide((uint16_t)met, (uint16_t)subsetTotal) < 0x8000u) {
						break;
					}
					g_missionConditionCurrentCount = (uint16_t)met;
					g_missionConditionTotalCount = (uint16_t)total;
					return 1;
				case MISSION_GOAL_AMT_25_OF_SUBSET:
					if ((uint16_t)MATH2_divide((uint16_t)met, (uint16_t)subsetTotal) < 0x4000u) {
						break;
					}
					g_missionConditionCurrentCount = (uint16_t)met;
					g_missionConditionTotalCount = (uint16_t)total;
					return 1;
				case MISSION_GOAL_AMT_ALL_BUT_1_OF_SUBSET:
					if ((uint16_t)met < (uint16_t)subsetTotal - 1u) {
						break;
					}
					g_missionConditionCurrentCount = (uint16_t)met;
					g_missionConditionTotalCount = (uint16_t)total;
					return 1;
				case MISSION_GOAL_AMT_66:
					if ((uint16_t)MATH2_divide((uint16_t)met, (uint16_t)total) >= 0xaaaau) {
						g_missionConditionCurrentCount = (uint16_t)met;
						g_missionConditionTotalCount = (uint16_t)total;
						return 1;
					} else if ((uint16_t)MATH2_divide((uint16_t)failed, (uint16_t)total) > 0x5555u) {
						status = 2;
					}
					break;
				case MISSION_GOAL_AMT_33:
					if ((uint16_t)MATH2_divide((uint16_t)met, (uint16_t)total) >= 0x5555u) {
						g_missionConditionCurrentCount = (uint16_t)met;
						g_missionConditionTotalCount = (uint16_t)total;
						return 1;
					} else if ((uint16_t)MATH2_divide((uint16_t)failed, (uint16_t)total) > 0xaaaau) {
						status = 2;
					}
					break;
				default:
					break;
			}
		}

		g_missionConditionCurrentCount = (uint16_t)met;
		g_missionConditionTotalCount = (uint16_t)total;
		return status;
	}

	status = 4;
	switch (condition) {
		case MISSION_COND_ALWAYS_TRUE:
			return 1;
		case MISSION_COND_NEVER_FALSE:
			return 0;
		case 13: {
			uint8_t goalStatus;
			if (variableType == TRIGVAR_TEAM) {
				goalStatus = g_missionFlightRuntimeState.teamGoalStatus[variable][TEAM_GOAL_PRIMARY];
			} else if (variableType == TRIGVAR_NOT_TEAM) {
				for (fgIdx = 0; fgIdx < 10u; ++fgIdx) {
					if (fgIdx == variable) {
						if (g_missionFlightRuntimeState.teamGoalStatus[fgIdx][TEAM_GOAL_PRIMARY] == 1) {
							return 2;
						}
					} else if (g_missionFlightRuntimeState.teamGoalStatus[fgIdx][TEAM_GOAL_PRIMARY] == 1) {
						return 1;
					}
				}
				return status;
			} else {
				goalStatus = g_missionFlightRuntimeState.globalPrimaryGoalStatus;
			}
			if (goalStatus == 1) {
				return 1;
			}
			if (goalStatus == 2) {
				return 2;
			}
			return status;
		}
		case 14: {
			uint8_t goalStatus;
			if (variableType == TRIGVAR_TEAM) {
				goalStatus = g_missionFlightRuntimeState.teamGoalStatus[variable][TEAM_GOAL_PRIMARY];
			} else {
				goalStatus = g_missionFlightRuntimeState.globalPrimaryGoalStatus;
			}
			if (goalStatus == 2) {
				return 1;
			}
			if (goalStatus == 1) {
				return 2;
			}
			return status;
		}
		case 17: {
			uint8_t goalStatus;
			if (variableType == TRIGVAR_TEAM) {
				goalStatus = g_missionFlightRuntimeState.teamGoalStatus[variable][TEAM_GOAL_BONUS];
				if (goalStatus == 2) {
					return 1;
				}
				if (goalStatus == 1) {
					return 2;
				}
				return status;
			}
			goalStatus = g_missionFlightRuntimeState.globalBonusGoalStatus;
			if (goalStatus == 1) {
				return 1;
			}
			if (goalStatus == 2) {
				return 2;
			}
			return status;
		}
		case 18: {
			uint8_t goalStatus;
			goalStatus = g_missionFlightRuntimeState.globalBonusGoalStatus;
			if (goalStatus == 2) {
				return 1;
			}
			if (goalStatus == 1) {
				return 2;
			}
			return status;
		}
		case 20:
			return 2 - (g_missionFlightRuntimeState.teamReinforcementCalled[variable] != 0);
		case 38:
			return 2;
		case 41:
		case 42: {
			int playerIdx;
			uint16_t totalByTeam[10];
			uint16_t connectedByTeam[10];
			uint16_t connectedByIff[8];
			uint16_t totalByIff[10];
			uint16_t current;

			memset(totalByTeam, 0, sizeof(totalByTeam));
			memset(connectedByTeam, 0, sizeof(connectedByTeam));
			memset(connectedByIff, 0, sizeof(connectedByIff));
			memset(totalByIff, 0, sizeof(totalByIff));

			for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
				if (g_players[playerIdx].connectedFlag) {
					++connectedByTeam[(uint16_t)g_players[playerIdx].playerIff];
					++connectedByIff[(uint16_t)g_players[playerIdx].iff];
				}
			}

			for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
				if (g_missionFlightGroups[fgIdx].fg.playerNumber) {
					++totalByTeam[g_missionFlightGroups[fgIdx].fg.team];
					++totalByIff[g_missionFlightGroups[fgIdx].fg.iff];
				}
			}

			if (variableType == TRIGVAR_PLAYER_NUM) {
				status = 2;
				if (condition == 41) {
					for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
						if (g_missionFlightGroups[fgIdx].fg.playerNumber &&
							variable == g_missionFlightGroups[fgIdx].fg.playerNumber - 1u &&
							g_missionFlightGroups[fgIdx].playerOwnerIdx != -1) {
							status = 1;
						}
					}
				} else {
					for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
						if (g_missionFlightGroups[fgIdx].fg.playerNumber &&
							variable == g_missionFlightGroups[fgIdx].fg.playerNumber - 1u &&
							g_missionFlightGroups[fgIdx].playerOwnerIdx == -1) {
							status = 1;
						}
					}
				}
				return status;
			}

			total = 0;
			current = 0;
			if (variableType == TRIGVAR_TEAM) {
				total = totalByTeam[variable];
				current = condition == 41 ? connectedByTeam[variable]
										  : (uint16_t)(total - connectedByTeam[variable]);
			} else if (variableType == TRIGVAR_IFF) {
				total = totalByIff[variable];
				current =
					condition == 41 ? connectedByIff[variable] : (uint16_t)(total - connectedByIff[variable]);
			}

			switch (amount) {
				case MISSION_GOAL_AMT_100:
					if (current == total) {
						return 1;
					}
					break;
				case MISSION_GOAL_AMT_75:
					if ((uint16_t)MATH2_divide(current, total) >= 0xc000u) {
						return 1;
					}
					break;
				case MISSION_GOAL_AMT_50:
					if ((uint16_t)MATH2_divide(current, total) >= 0x8000u) {
						return 1;
					}
					break;
				case MISSION_GOAL_AMT_25:
					if ((uint16_t)MATH2_divide(current, total) >= 0x4000u) {
						return 1;
					}
					break;
				case MISSION_GOAL_AMT_AT_LEAST_1:
					if (current != 0) {
						return 1;
					}
					break;
				case MISSION_GOAL_AMT_ALL_BUT_1:
					if (current == (uint16_t)(total - 1u)) {
						return 1;
					}
					break;
				case MISSION_GOAL_AMT_66:
					if ((uint16_t)MATH2_divide(current, total) >= 0xaaaau) {
						return 1;
					}
					break;
				case MISSION_GOAL_AMT_33:
					if ((uint16_t)MATH2_divide(current, total) >= 0x5555u) {
						return 1;
					}
					break;
				default:
					break;
			}
			return 2;
		}
		case 49:
		case 50:
			if ((uint16_t)trigger->parameter < 5u && trigger->parameter != 0) {
				uint32_t objectIdx;
				for (objectIdx = 0; objectIdx < g_regionObjectSlotEnd; ++objectIdx) {
					if (g_objectTable[objectIdx].objectType != 0 &&
						Mission_ObjectMatchesTriggerVariableEx(
							(uint16_t)objectIdx, (MissionTriggerVariableType)variableType, variable,
							g_paiContext.curOrderCoord.fields.flightGroupIdx)) {
						if (condition == 49) {
							if (g_objectTable[objectIdx].regionIdx == (uint16_t)trigger->parameter - 1u) {
								break;
							}
						} else if (g_objectTable[objectIdx].regionIdx == (uint16_t)trigger->parameter - 1u) {
							break;
						}
					}
				}

				if (condition == 49) {
					if (objectIdx < g_regionObjectSlotEnd) {
						return 1;
					}
					return status;
				}
				if (objectIdx >= g_regionObjectSlotEnd) {
					return 1;
				}
				return status;
			} else {
				uint32_t rangeLimit;
				uint8_t targetFgIdx;
				uint32_t objectIdx;

				if (trigger->amount == 0) {
					rangeLimit = 2035;
				} else if (trigger->amount > 10u) {
					rangeLimit = 20350u * trigger->amount - 162800u;
				} else {
					rangeLimit = 4070u * trigger->amount;
				}

				targetFgIdx = (uint8_t)(trigger->parameter - 5);
				for (objectIdx = 0; objectIdx < g_regionObjectSlotEnd; ++objectIdx) {
					ObjectRecord* object;
					uint32_t regionSlotBase;
					uint32_t scanObjIdx;
					uint32_t scanObjEnd;

					object = &g_objectTable[objectIdx];
					if (object->objectType == 0 ||
						!Mission_ObjectMatchesTriggerVariableEx((uint16_t)objectIdx,
																(MissionTriggerVariableType)variableType,
																variable, 0xffffu)) {
						continue;
					}

					regionSlotBase =
						(g_regionObjectSlotEnd / (uint32_t)g_missionRegionCount) * object->regionIdx;
					scanObjIdx = regionSlotBase;
					scanObjEnd = regionSlotBase +
								 g_regionStaticObjectSlotsTotal / (uint32_t)g_missionRegionCount +
								 g_regionMainObjectSlotsTotal / (uint32_t)g_missionRegionCount;
					for (; scanObjIdx < scanObjEnd; ++scanObjIdx) {
						if (g_objectTable[scanObjIdx].objectType == 0 ||
							g_objectTable[scanObjIdx].flightGroupIdx != targetFgIdx) {
							continue;
						}

						pai_ObjectRefUpdateApproxRangeScore(objectIdx, scanObjIdx);
						if (condition == 49) {
							if ((uint32_t)g_targetRangeScore < rangeLimit &&
								object->regionIdx == g_objectTable[scanObjIdx].regionIdx) {
								return 1;
							}
						} else if ((uint32_t)g_targetRangeScore > rangeLimit) {
							return 1;
						}
					}
				}
				return status;
			}
		case 56:
			if (g_missionMessageTriggered[variable] && g_missionMessageDelayCountdown[variable] == 0) {
				status = 1;
			}
			return status;
		case 59:
			if (!g_missionMessageTriggered[variable]) {
				status = 1;
			}
			return status;
		default:
			return status;
	}
}
// FUNCTION: XWA 0x4D5A70
int Mission_EvaluateTriggerPair(const XwaTriggerPair* triggerPair, char includePending) {
	const XwaTrigger* trigger2;
	int teamOrVariable;
	int trigger1Result;
	int trigger2Result;

	teamOrVariable = 10;
	trigger2 = &triggerPair->triggers[1];
	if (triggerPair->triggers[1].condition == 39 && triggerPair->triggers[1].variableType == TRIGVAR_TEAM) {
		teamOrVariable = triggerPair->triggers[1].variable;
	}

	trigger1Result =
		Mission_EvaluateCondition(&triggerPair->triggers[0], includePending, (uint16_t)teamOrVariable);
	if (trigger2->condition != 39) {
		trigger2Result = Mission_EvaluateCondition(trigger2, includePending, 10);
	} else {
		trigger2Result = 0;
	}

	if (triggerPair->t1OrT2 == 1 || trigger2->condition == 39) {
		return trigger1Result | trigger2Result;
	}
	return trigger1Result & trigger2Result;
}

enum {
	MISSION_GOAL_EVALUATION_TIMER_IDX = 1,
	MISSION_ARRIVAL_TRIGGER_SCAN_TIMER_IDX = 3,
	MISSION_ARRIVAL_DELAY_SCAN_TIMER_IDX = 4,
	MISSION_MESSAGE_SCAN_TIMER_IDX = 5,
	MISSION_LOGIC_REFRESH_TICKS = 236,
	MISSION_TEAM_COUNT = 10,
	MISSION_FG_GOAL_COUNT = 8,
	MISSION_GLOBAL_GOAL_KIND_COUNT = 3,
	MISSION_GLOBAL_TRIGGER_COUNT = 4,
};

static __inline uint8_t Mission_EvaluateGlobalGoalState(uint16_t teamIdx, uint16_t goalKind,
														uint8_t triggerResults[MISSION_GLOBAL_TRIGGER_COUNT],
														int16_t* hasRealTrigger) {
	XwaGlobalGoal* goal;
	uint8_t condition2;
	uint8_t condition4;
	uint16_t teamForPair12;
	uint16_t teamForPair34;
	int r0;
	int r1;
	int r2;
	int r3;
	uint8_t state12;
	int state34;

	goal = &g_missionGlobalGoals[teamIdx][goalKind];
	condition2 = goal->triggerPairs[0].triggers[1].condition;
	condition4 = goal->triggerPairs[1].triggers[1].condition;
	{
		uint16_t* currentCount;
		uint16_t* totalCount;
		uint8_t* triggerResult;
		int triggersRemaining;

		currentCount = g_missionFlightRuntimeState
						   .globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_CURRENT][teamIdx][goalKind];
		totalCount = g_missionFlightRuntimeState
						 .globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_TOTAL][teamIdx][goalKind];
		triggerResult = triggerResults;
		triggersRemaining = MISSION_GLOBAL_TRIGGER_COUNT;
		do {
			*currentCount++ = 0;
			*totalCount++ = 0;
			if (goalKind == 0) {
				*triggerResult = 0;
			}
			++triggerResult;
		} while (--triggersRemaining != 0);
	}

	if ((goal->triggerPairs[0].triggers[0].condition == 10 ||
		 goal->triggerPairs[0].triggers[0].condition == 0) &&
		(condition2 == 10 || condition2 == 0) &&
		(goal->triggerPairs[1].triggers[0].condition == 10 ||
		 goal->triggerPairs[1].triggers[0].condition == 0) &&
		(condition4 == 10 || condition4 == 0)) {
		return 0;
	}
	*hasRealTrigger = 1;

	teamForPair12 = teamIdx;
	if (condition2 == 39 && goal->triggerPairs[0].triggers[1].variableType == TRIGVAR_TEAM) {
		teamForPair12 = goal->triggerPairs[0].triggers[1].variable;
	}
	r0 = Mission_EvaluateCondition(&goal->triggerPairs[0].triggers[0], 0, (uint16_t)teamForPair12);
	g_missionFlightRuntimeState.globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_CURRENT][teamIdx][goalKind][0] =
		g_missionConditionCurrentCount;
	g_missionFlightRuntimeState.globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_TOTAL][teamIdx][goalKind][0] =
		g_missionConditionTotalCount;
	if (goalKind == 0) {
		triggerResults[0] = (uint8_t)r0;
	}

	if (condition2 != 39) {
		r1 = Mission_EvaluateCondition(&goal->triggerPairs[0].triggers[1], 0, (uint16_t)teamForPair12);
		g_missionFlightRuntimeState
			.globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_CURRENT][teamIdx][goalKind][1] =
			g_missionConditionCurrentCount;
		g_missionFlightRuntimeState
			.globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_TOTAL][teamIdx][goalKind][1] =
			g_missionConditionTotalCount;
	} else {
		r1 = 0;
	}
	if (goalKind == 0) {
		triggerResults[1] = (uint8_t)r1;
	}

	if (condition2 == 39) {
		if ((r0 & 1) != 0) {
			state12 = 1;
		} else {
			state12 = (r0 & 2) != 0 ? 2 : 4;
		}
	} else if (goal->triggerPairs[0].t1OrT2 == 1) {
		if (((r0 | r1) & 1) != 0) {
			state12 = 1;
		} else {
			state12 = ((r0 & r1) & 2) != 0 ? 2 : 4;
		}
	} else if (((r0 & r1) & 1) != 0) {
		state12 = 1;
	} else {
		state12 = ((r0 | r1) & 2) != 0 ? 2 : 4;
	}

	teamForPair34 = teamIdx;
	if (condition4 == 39 && goal->triggerPairs[1].triggers[1].variableType == TRIGVAR_TEAM) {
		teamForPair34 = goal->triggerPairs[1].triggers[1].variable;
	}
	r2 = Mission_EvaluateCondition(&goal->triggerPairs[1].triggers[0], 0, (uint16_t)teamForPair34);
	g_missionFlightRuntimeState.globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_CURRENT][teamIdx][goalKind][2] =
		g_missionConditionCurrentCount;
	g_missionFlightRuntimeState.globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_TOTAL][teamIdx][goalKind][2] =
		g_missionConditionTotalCount;
	if (goalKind == 0) {
		triggerResults[2] = (uint8_t)r2;
	}

	if (condition4 != 39) {
		r3 = Mission_EvaluateCondition(&goal->triggerPairs[1].triggers[1], 0, (uint16_t)teamForPair34);
		g_missionFlightRuntimeState
			.globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_CURRENT][teamIdx][goalKind][3] =
			g_missionConditionCurrentCount;
		g_missionFlightRuntimeState
			.globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_TOTAL][teamIdx][goalKind][3] =
			g_missionConditionTotalCount;
	} else {
		r3 = 0;
	}
	if (goalKind == 0) {
		triggerResults[3] = (uint8_t)r3;
	}

	if (condition4 == 39) {
		if ((r2 & 1) != 0) {
			state34 = 1;
		} else {
			state34 = (r2 & 2) != 0 ? 2 : 4;
		}
	} else if (goal->triggerPairs[1].t1OrT2 == 1) {
		if (((r2 | r3) & 1) != 0) {
			state34 = 1;
		} else {
			state34 = ((r2 & r3) & 2) != 0 ? 2 : 4;
		}
	} else if (((r2 & r3) & 1) != 0) {
		state34 = 1;
	} else {
		state34 = ((r2 | r3) & 2) != 0 ? 2 : 4;
	}

	if (goal->t12AndOrT34 == 1) {
		if (((state12 | state34) & 1u) != 0) {
			return 1;
		}
		return ((state12 & state34) & 2u) != 0 ? 2 : 4;
	}

	if (((state12 & state34) & 1u) != 0) {
		return 1;
	}
	return ((state12 | state34) & 2u) != 0 ? 2 : 4;
}

static __inline int Mission_FindLastConnectedPlayerOnTeam(int teamIdx) {
	int playerIdx;
	int foundPlayer;

	foundPlayer = -1;
	for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
		if (g_players[playerIdx].connectedFlag && g_players[playerIdx].playerIff == teamIdx) {
			foundPlayer = playerIdx;
		}
	}
	return foundPlayer;
}

static __inline void Mission_HandlePrimaryGoalStatusChanged(int teamIdx, uint8_t newStatus,
															int completedTeamCount,
															int missionElapsedSeconds) {
	if (newStatus == 1) {
		int playerIdx;
		int lastWinningPlayer;

		g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[teamIdx] = missionElapsedSeconds;
		if (g_flightPlayerCount == 1 && g_players[g_localPlayer].playerIff == teamIdx) {
			g_players[g_localPlayer].missionStats.missionScore += 250;
		}
		g_missionFlightRuntimeState.globalPrimaryGoalStatus = 1;

		lastWinningPlayer = Mission_FindLastConnectedPlayerOnTeam(teamIdx);
		for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
			if (!g_players[playerIdx].connectedFlag) {
				continue;
			}

			if (g_players[playerIdx].playerIff == teamIdx) {
				int emitComplete;

				g_players[playerIdx].missionStats.field18 = completedTeamCount;
				g_msgSenderIff = (uint16_t)g_players[playerIdx].iff;
				emitComplete = 1;
				if (g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) {
					emitComplete =
						g_missionFlightRuntimeState.teamGoalStatus[teamIdx][TEAM_GOAL_SECONDARY] != 1;
				}

				if (emitComplete) {
					msg_emitInFlightMessage(MSG_MISSION_COMPLETE, playerIdx);
					if ((g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
						 g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) &&
						g_flightPlayerCount > 1) {
						g_msgArgTable[0] = (uint16_t)(completedTeamCount + 386);
						msg_emitInFlightMessage(MSG_GENERAL_WIN, playerIdx);
					}
					if (playerIdx == g_localPlayer) {
						uint16_t lineIdx;

						for (lineIdx = 0; lineIdx < 2; ++lineIdx) {
							const char* message;

							message = g_missionTeams[teamIdx].endOfMissionMessages[lineIdx];
							if (message[0] != '\0') {
								msg_addMessagePtr(0, message);
								g_pendingHudMessageVoiceSfxId = (uint16_t)(260 + lineIdx);
								if ((uint8_t)message[0] >= '1' && (uint8_t)message[0] <= '6') {
									msg_emitInFlightMessage(MSG_RADIO_BLANK, playerIdx);
								} else {
									msg_emitInFlightMessage(MSG_BLANK, playerIdx);
								}
								g_pendingHudMessageVoiceSfxId = 0;
							}
						}
						g_playerFlightTransientTimers[g_localPlayer].missionSuccessMusicTimer = 2478;
					}
				} else if (teamIdx != 0) {
					msg_emitInFlightMessage(MSG_REBEL_DRAWS, playerIdx);
				} else {
					msg_emitInFlightMessage(MSG_EMPIRE_DRAWS, playerIdx);
				}
			} else {
				g_msgArgTable[1] = (uint16_t)(completedTeamCount + 386);
				if ((g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
					 g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH ||
					 g_missionHeader.body.hangar == XWA_HANGAR_SIMULATOR1) &&
					lastWinningPlayer != -1) {
					msg_addMessagePtr(0, NetSession_GetPlayerName(lastWinningPlayer));
					msg_emitInFlightMessage(MSG_OTHER_PLAYER_WIN, playerIdx);
				}
			}
		}
	} else {
		unsigned int playerIdx;

		for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
			if (g_players[playerIdx].connectedFlag && g_players[playerIdx].playerIff == teamIdx &&
				g_missionFlightRuntimeState.teamGoalStatus[teamIdx][TEAM_GOAL_SECONDARY] != 1 &&
				playerIdx == (unsigned int)g_localPlayer) {
				g_msgSenderIff = (uint16_t)g_players[playerIdx].iff;
				fsfx_PlaySound(61, 0xffffu, (unsigned int)playerIdx);
				msg_emitInFlightMessage(MSG_MISSION_LOST, playerIdx);
				msg_emitInFlightMessage(MSG_MISSION_IMPOSSIBLE, playerIdx);
				{
					uint16_t lineIdx;

					for (lineIdx = 0; lineIdx < 2; ++lineIdx) {
						const char* message;

						message = g_missionTeams[teamIdx].endOfMissionMessages[2 + lineIdx];
						if (message[0] != '\0') {
							msg_addMessagePtr(0, message);
							g_pendingHudMessageVoiceSfxId = (uint16_t)(262 + lineIdx);
							if ((uint8_t)message[0] >= '1' && (uint8_t)message[0] <= '6') {
								msg_emitInFlightMessage(MSG_RADIO_BLANK, playerIdx);
							} else {
								msg_emitInFlightMessage(MSG_BLANK, playerIdx);
							}
							g_pendingHudMessageVoiceSfxId = 0;
						}
					}
				}
			}
		}
	}
}

static __inline void Mission_HandleSecondaryGoalComplete(int teamIdx) {
	unsigned int playerIdx;

	for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
		if (!g_players[playerIdx].connectedFlag || g_players[playerIdx].playerIff != teamIdx ||
			g_missionFlightRuntimeState.teamGoalStatus[teamIdx][TEAM_GOAL_PRIMARY] == 2 ||
			playerIdx != (unsigned int)g_localPlayer) {
			continue;
		}

		{
			int emitLoss;

			emitLoss = 1;
			if (g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) {
				emitLoss = g_missionFlightRuntimeState.teamGoalStatus[teamIdx][TEAM_GOAL_PRIMARY] != 1;
			}
			if (emitLoss) {
				g_msgSenderIff = (uint16_t)g_players[playerIdx].iff;
				fsfx_PlaySound(61, 0xffffu, (unsigned int)playerIdx);
				msg_emitInFlightMessage(MSG_MISSION_LOST, playerIdx);
				msg_emitInFlightMessage(MSG_MISSION_IMPOSSIBLE, playerIdx);
				{
					uint16_t lineIdx;

					for (lineIdx = 0; lineIdx < 2; ++lineIdx) {
						const char* message;

						message = g_missionTeams[teamIdx].endOfMissionMessages[2 + lineIdx];
						if (message[0] != '\0') {
							msg_addMessagePtr(0, message);
							g_pendingHudMessageVoiceSfxId = (uint16_t)(262 + lineIdx);
							if ((uint8_t)message[0] >= '1' && (uint8_t)message[0] <= '6') {
								msg_emitInFlightMessage(MSG_RADIO_BLANK, playerIdx);
							} else {
								msg_emitInFlightMessage(MSG_BLANK, playerIdx);
							}
							g_pendingHudMessageVoiceSfxId = 0;
						}
					}
				}
			} else if (teamIdx != 0) {
				msg_emitInFlightMessage(MSG_REBEL_LOST_WIN, playerIdx);
			} else {
				msg_emitInFlightMessage(MSG_EMPIRE_LOST_WIN, playerIdx);
			}
			g_playerFlightTransientTimers[g_localPlayer].missionLossMusicTimer = 2478;
		}
	}
}

static __inline void Mission_HandleBonusGoalComplete(int teamIdx) {
	int playerIdx;

	for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
		if (g_players[playerIdx].connectedFlag && g_players[playerIdx].playerIff == teamIdx &&
			playerIdx == g_localPlayer) {
			g_msgSenderIff = (uint16_t)g_players[playerIdx].iff;
			{
				uint16_t lineIdx;

				for (lineIdx = 0; lineIdx < 2; ++lineIdx) {
					const char* message;

					message = g_missionTeams[teamIdx].endOfMissionMessages[4 + lineIdx];
					if (message[0] != '\0') {
						msg_addMessagePtr(0, message);
						g_pendingHudMessageVoiceSfxId = (uint16_t)(264 + lineIdx);
						if ((uint8_t)message[0] >= '1' && (uint8_t)message[0] <= '6') {
							msg_emitInFlightMessage(MSG_RADIO_BLANK, playerIdx);
						} else {
							msg_emitInFlightMessage(MSG_BLANK, playerIdx);
						}
						g_pendingHudMessageVoiceSfxId = 0;
					}
				}
			}
		}
	}
}

static __inline uint8_t Mission_ComputeTeamActiveGoalSequence(uint16_t teamIdx,
															  const uint8_t primaryTriggerResults[4]) {
	uint8_t sequence;
	char sequenceOpen;

	sequence = 1;
	sequenceOpen = 1;
	while (sequence < 7u) {
		int16_t sequenceHasSuccess;
		uint16_t triggerIdx;
		int fgIdx;
		uint8_t globalActiveSequence;

		sequenceHasSuccess = 0;
		globalActiveSequence = g_missionGlobalGoals[teamIdx][0].activeSequence;
		if (globalActiveSequence == sequence) {
			for (triggerIdx = 0; triggerIdx < 4; ++triggerIdx) {
				uint8_t result;

				result = primaryTriggerResults[triggerIdx];
				if (result == 1) {
					sequenceHasSuccess = 1;
				} else if (result != 0) {
					sequenceOpen = 0;
					break;
				}
			}
		}

		if (sequenceOpen) {
			for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
				uint16_t goalIdx;

				for (goalIdx = 0; goalIdx < MISSION_FG_GOAL_COUNT; ++goalIdx) {
					XwaFlightGroupGoalPayload* goal;
					uint8_t state;

					goal = &g_missionFlightGroups[fgIdx].fg.fgGoals[goalIdx].payload;
					if (goal->enabledForTeam[teamIdx] == 0 || goal->argument != 0 ||
						goal->activeSequence != sequence) {
						continue;
					}

					state = g_missionFgStats[fgIdx].goalState[8 * teamIdx + goalIdx];
					if (state == 1) {
						sequenceHasSuccess = 1;
					} else if (state != 0) {
						sequenceOpen = 0;
					}
				}
			}
		}

		if (!sequenceHasSuccess) {
			sequenceOpen = 0;
		}
		if (!sequenceOpen) {
			break;
		}
		++sequence;
	}

	return sequence;
}

// FUNCTION: XWA 0x4D4760
void Mission_UpdateLogic(void) {
	uint16_t messageIdx;

	if (g_provingGroundsModeActive) {
		return;
	}

	if (g_flightGlobalCountdownTimers[MISSION_GOAL_EVALUATION_TIMER_IDX] == 0 || g_flightMissionEndPending) {
		unsigned int missionElapsedSeconds;

		g_connectedPlayerCount = 0;
		{
			unsigned int playerIdx;

			for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
				if (g_players[playerIdx].connectedFlag) {
					++g_connectedPlayerCount;
				}
			}
		}
		if ((unsigned int)g_connectedPlayerCount > (unsigned int)g_maxConnectedPlayerCountThisMission) {
			g_maxConnectedPlayerCountThisMission = g_connectedPlayerCount;
		}

		missionElapsedSeconds = g_missionElapsedClock.seconds +
								60 * (g_missionElapsedClock.minutes + 60 * g_missionElapsedClock.hours);

		{
			uint16_t fgIdx;
			uint16_t goalIdx;

			for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
				for (goalIdx = 0; goalIdx < MISSION_FG_GOAL_COUNT; ++goalIdx) {
					unsigned int goalTeamIdx;

					if (!g_missionFgStats[fgIdx].arrivalEnabled &&
						g_missionFlightGroups[fgIdx].playerOwnerIdx == -1) {
						for (goalTeamIdx = 0; goalTeamIdx < MISSION_TEAM_COUNT; ++goalTeamIdx) {
							g_missionFgStats[fgIdx].goalState[8 * goalTeamIdx + goalIdx] = 0;
						}
					} else {
						uint8_t* enabledForTeam;
						uint8_t* goalState;
						int* teamScore;
						int teamsRemaining;

						enabledForTeam =
							g_missionFlightGroups[fgIdx].fg.fgGoals[goalIdx].payload.enabledForTeam;
						goalState = &g_missionFgStats[fgIdx].goalState[goalIdx];
						teamScore = g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS];
						goalTeamIdx = 0;
						teamsRemaining = MISSION_TEAM_COUNT;
						do {
							XwaTrigger condition;
							int result;

							if (*enabledForTeam == 0) {
								*goalState = 0;
							} else if (*goalState == 4) {
								if (g_missionFlightGroups[fgIdx].fg.fgGoals[goalIdx].payload.condition == 0 ||
									g_missionFlightGroups[fgIdx].fg.fgGoals[goalIdx].payload.condition ==
										10) {
									*goalState = 0;
								} else {
									condition.condition =
										g_missionFlightGroups[fgIdx].fg.fgGoals[goalIdx].payload.condition;
									condition.variableType = TRIGVAR_FLIGHT_GROUP;
									condition.variable = (uint8_t)fgIdx;
									condition.amount =
										g_missionFlightGroups[fgIdx].fg.fgGoals[goalIdx].payload.amount;
									condition.parameter = 0;
									if (g_missionFlightGroups[fgIdx].fg.fgGoals[goalIdx].payload.condition ==
											47 ||
										g_missionFlightGroups[fgIdx].fg.fgGoals[goalIdx].payload.condition ==
											48) {
										condition.parameter = 1;
										if (g_missionFlightGroups[fgIdx]
												.fg.fgGoals[goalIdx]
												.payload.parameter == 2) {
											condition.parameter = 2;
										} else if (g_missionFlightGroups[fgIdx]
													   .fg.fgGoals[goalIdx]
													   .payload.parameter == 3) {
											condition.parameter = 3;
										} else if (g_missionFlightGroups[fgIdx]
													   .fg.fgGoals[goalIdx]
													   .payload.parameter == 4) {
											condition.parameter = 4;
										}
									}

									result = Mission_EvaluateCondition(&condition, 0, (uint16_t)goalTeamIdx);
									if (result == 1) {
										if (g_missionFlightGroups[fgIdx]
													.fg.fgGoals[goalIdx]
													.payload.parameter == 0 ||
											condition.condition == 47 || condition.condition == 48) {
											result = 8;
										} else {
											unsigned int timeLimitSeconds;

											timeLimitSeconds = g_missionFlightGroups[fgIdx]
																   .fg.fgGoals[goalIdx]
																   .payload.parameter;
											if (timeLimitSeconds >= 20u) {
												if (timeLimitSeconds < 196u) {
													timeLimitSeconds = 5u * timeLimitSeconds - 80u;
												} else {
													timeLimitSeconds = 10u * timeLimitSeconds - 1060u;
												}
											}
											result = timeLimitSeconds < missionElapsedSeconds ? 2 : 8;
										}
									}

									if (result == 8 &&
										(g_missionFlightGroups[fgIdx].fg.fgGoals[goalIdx].payload.argument !=
											 2 ||
										 (g_missionFlightGroups[fgIdx].fg.fgGoals[goalIdx].payload.amount !=
											  18 &&
										  g_missionFlightGroups[fgIdx].fg.fgGoals[goalIdx].payload.amount !=
											  19))) {
										*teamScore += 250 * (int)g_missionFlightGroups[fgIdx]
																.fg.fgGoals[goalIdx]
																.payload.points;
										result = 1;
									}
									*goalState = (uint8_t)result;
								}
							}

							++goalTeamIdx;
							++teamScore;
							goalState += MISSION_FG_GOAL_COUNT;
							++enabledForTeam;
						} while (--teamsRemaining != 0);
					}
				}
			}
		}

		{
			uint8_t primaryTriggerResults[MISSION_GLOBAL_TRIGGER_COUNT];
			uint16_t teamIdx;
			uint16_t goalKind;

			for (teamIdx = 0; teamIdx < MISSION_TEAM_COUNT; ++teamIdx) {
				for (goalKind = 0; goalKind < MISSION_GLOBAL_GOAL_KIND_COUNT; ++goalKind) {
					int16_t globalState;
					int16_t aggregateState;
					int16_t sawSuccess;

					sawSuccess = 0;
					globalState = Mission_EvaluateGlobalGoalState((uint16_t)teamIdx, (uint16_t)goalKind,
																  primaryTriggerResults, &sawSuccess);
					aggregateState = goalKind != 1;

					if (goalKind == 1) {
						if (globalState == 1) {
							aggregateState = 1;
							sawSuccess = 1;
							if (g_missionFlightRuntimeState.teamGlobalGoalState[teamIdx][goalKind] != 1) {
								g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][teamIdx] +=
									250 * (int)g_missionGlobalGoals[teamIdx][goalKind].rawPoints;
							}
						}
					} else {
						switch (globalState) {
							case 1:
								sawSuccess = 1;
								if (g_missionFlightRuntimeState.teamGlobalGoalState[teamIdx][goalKind] != 1) {
									g_missionFlightRuntimeState
										.teamScores[TEAM_SCORE_BONUS_TENTHS][teamIdx] +=
										250 * (int)g_missionGlobalGoals[teamIdx][goalKind].rawPoints;
								}
								break;
							case 2:
								aggregateState = 2;
								break;
							case 4:
								aggregateState = 0;
								break;
						}
					}

					g_missionFlightRuntimeState.teamGlobalGoalState[teamIdx][goalKind] = globalState;

					{
						int aggregateFgIdx;
						int aggregateGoalIdx;
						XwaGoalFG* fgGoal;

						for (aggregateFgIdx = 0; aggregateFgIdx < (int16_t)g_missionHeader.numFlightGroups;
							 ++aggregateFgIdx) {
							fgGoal = g_missionFlightGroups[aggregateFgIdx].fg.fgGoals;
							for (aggregateGoalIdx = 0; aggregateGoalIdx < MISSION_FG_GOAL_COUNT;
								 ++aggregateGoalIdx) {
								XwaFlightGroupGoalPayload* goal;
								uint8_t state;

								goal = &fgGoal->payload;
								if (goal->enabledForTeam[teamIdx] == 0 || goal->argument != goalKind) {
									++fgGoal;
									continue;
								}

								state = g_missionFgStats[aggregateFgIdx]
											.goalState[8 * teamIdx + aggregateGoalIdx];
								if (goalKind == 1) {
									if (state == 1) {
										aggregateState = 1;
										sawSuccess = 1;
									}
								} else if (state == 1) {
									sawSuccess = 1;
								} else if (state == 2) {
									aggregateState = 2;
								} else if (state == 4 && aggregateState != 2) {
									aggregateState = 0;
								}
								++fgGoal;
							}
						}
					}

					if (aggregateState == 1 && sawSuccess == 0) {
						aggregateState = 0;
					}
					if (goalKind == 0 && aggregateState == 1 &&
						g_missionFlightRuntimeState.teamGoalStatus[teamIdx][TEAM_GOAL_SECONDARY] == 1) {
						aggregateState = 0;
					}
					if (g_flightPlayerCount == 1 && g_missionHeader.body.hangar != XWA_HANGAR_SKIRMISH &&
						goalKind == g_flightPlayerCount && aggregateState == g_flightPlayerCount &&
						g_missionFlightRuntimeState.teamGoalStatus[teamIdx][TEAM_GOAL_PRIMARY] == 1) {
						g_missionFlightRuntimeState.teamGoalStatus[teamIdx][TEAM_GOAL_PRIMARY] = 2;
					}
					if (goalKind == 0 &&
						(g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
						 g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) &&
						g_missionHeader.body.goalsUnimportant) {
						aggregateState = 0;
					}

					{
						uint8_t* statusBytes;
						uint8_t oldStatus;

						statusBytes = g_missionFlightRuntimeState.teamGoalStatus[teamIdx];
						oldStatus = statusBytes[goalKind];
						if (oldStatus != aggregateState && aggregateState != 0 && oldStatus != 1 &&
							!g_missionHeader.body.goalsUnimportant) {
							int completedTeamCount;
							int completedTeamIdx;

							statusBytes[goalKind] = aggregateState;
							completedTeamCount = 0;
							if (aggregateState == 1) {
								for (completedTeamIdx = 0; completedTeamIdx < MISSION_TEAM_COUNT;
									 ++completedTeamIdx) {
									const uint8_t* teamStatusBytes;

									teamStatusBytes =
										g_missionFlightRuntimeState.teamGoalStatus[completedTeamIdx];
									if (teamStatusBytes[goalKind] == 1) {
										++completedTeamCount;
									}
								}
							}
							if (goalKind == 0) {
								Mission_HandlePrimaryGoalStatusChanged(
									teamIdx, aggregateState, completedTeamCount, missionElapsedSeconds);
							} else if (aggregateState == 1) {
								if (goalKind == 2) {
									g_missionFlightRuntimeState.globalBonusGoalStatus = 1;
								}
								if (goalKind == 1) {
									Mission_HandleSecondaryGoalComplete(teamIdx);
								} else {
									Mission_HandleBonusGoalComplete(teamIdx);
								}
							}
						}
					}
				}

				g_missionFlightRuntimeState.teamActiveGoalSequence[teamIdx] =
					Mission_ComputeTeamActiveGoalSequence((uint16_t)teamIdx, primaryTriggerResults);
			}
		}

		g_flightGlobalCountdownTimers[MISSION_GOAL_EVALUATION_TIMER_IDX] = MISSION_LOGIC_REFRESH_TICKS;
	}

	if ((int16_t)g_flightGlobalCountdownTimers[MISSION_MESSAGE_SCAN_TIMER_IDX] > 0) {
		return;
	}

	for (messageIdx = 0; messageIdx < (int16_t)g_missionHeader.numMessages; ++messageIdx) {
		if (!g_missionMessageTriggered[messageIdx]) {
			int specialGate;
			int teamOrVar;
			int r1;
			int r2;
			XwaTriggerPair* pair;
			XwaTrigger* trigger2;

			pair = &g_missionMessages[messageIdx].special;
			trigger2 = &pair->triggers[1];
			teamOrVar = 10;
			if (trigger2->condition == 39 && trigger2->variableType == TRIGVAR_TEAM) {
				teamOrVar = trigger2->variable;
			}
			r1 = Mission_EvaluateCondition(&pair->triggers[0], 0, (uint16_t)teamOrVar);
			if (trigger2->condition != 39) {
				r2 = Mission_EvaluateCondition(trigger2, 0, 10);
			} else {
				r2 = 0;
			}
			if (pair->t1OrT2 == 1 || trigger2->condition == 39) {
				specialGate = r1 | r2;
			} else {
				specialGate = r1 & r2;
			}
			if (pair->triggers[0].condition == 0 && trigger2->condition == 0) {
				specialGate = 2;
			}

			if ((specialGate & 1) == 0 &&
				g_missionFgStats[g_missionMessages[messageIdx].originatingFG].outcomeCount[2] <
					g_missionFgStats[g_missionMessages[messageIdx].originatingFG].outcomeCount[0]) {
				int triggers12;
				int triggers34;
				int triggered;

				pair = &g_missionMessages[messageIdx].triggers[0];
				trigger2 = &pair->triggers[1];
				teamOrVar = 10;
				if (trigger2->condition == 39 && trigger2->variableType == TRIGVAR_TEAM) {
					teamOrVar = trigger2->variable;
				}
				r1 = Mission_EvaluateCondition(&pair->triggers[0], 0, (uint16_t)teamOrVar);
				if (trigger2->condition != 39) {
					r2 = Mission_EvaluateCondition(trigger2, 0, 10);
				} else {
					r2 = 0;
				}
				if (pair->t1OrT2 == 1 || trigger2->condition == 39) {
					triggers12 = r1 | r2;
				} else {
					triggers12 = r1 & r2;
				}

				pair = &g_missionMessages[messageIdx].triggers[1];
				trigger2 = &pair->triggers[1];
				teamOrVar = 10;
				if (trigger2->condition == 39 && trigger2->variableType == TRIGVAR_TEAM) {
					teamOrVar = trigger2->variable;
				}
				r1 = Mission_EvaluateCondition(&pair->triggers[0], 0, (uint16_t)teamOrVar);
				if (trigger2->condition != 39) {
					r2 = Mission_EvaluateCondition(trigger2, 0, 10);
				} else {
					r2 = 0;
				}
				if (pair->t1OrT2 == 1 || trigger2->condition == 39) {
					triggers34 = r1 | r2;
				} else {
					triggers34 = r1 & r2;
				}
				if (g_missionMessages[messageIdx].triggers12OrTriggers34 == 1) {
					triggered = triggers12 | triggers34;
				} else {
					triggered = triggers12 & triggers34;
				}

				if ((triggered & 1) != 0) {
					unsigned int delaySeconds;

					delaySeconds = g_missionMessages[messageIdx].rawDelay;
					g_missionMessageTriggered[messageIdx] = 1;
					if (delaySeconds >= 20u) {
						if (delaySeconds < 196u) {
							delaySeconds = 5u * delaySeconds - 80u;
						} else {
							delaySeconds = 10u * delaySeconds - 1060u;
						}
					}
					g_missionMessageDelayCountdown[messageIdx] = (int)delaySeconds;
					if (g_missionMessageDelayCountdown[messageIdx] == 0) {
						if (g_missionMessages[messageIdx]
								.sentToTeam[(uint16_t)g_players[g_localPlayer].playerIff]) {
							msg_addMessagePtr(0, g_missionMessages[messageIdx].message);
							if (messageIdx < 64) {
								g_pendingHudMessageVoiceSfxId = (uint16_t)(messageIdx + 196);
							} else {
								g_pendingHudMessageVoiceSfxId = 0;
							}
							g_msgSenderIff = g_missionMessages[messageIdx].colorIff;
							msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
							g_pendingHudMessageVoiceSfxId = 0;
						}
					}
				}
			}
		} else if (g_missionMessageDelayCountdown[messageIdx] != 0) {
			--g_missionMessageDelayCountdown[messageIdx];
			if (g_missionMessageDelayCountdown[messageIdx] == 0) {
				if (g_missionMessages[messageIdx].sentToTeam[(uint16_t)g_players[g_localPlayer].playerIff]) {
					msg_addMessagePtr(0, g_missionMessages[messageIdx].message);
					if (messageIdx < 64) {
						g_pendingHudMessageVoiceSfxId = (uint16_t)(messageIdx + 196);
					} else {
						g_pendingHudMessageVoiceSfxId = 0;
					}
					g_msgSenderIff = g_missionMessages[messageIdx].colorIff;
					msg_emitInFlightMessage(MSG_RADIO_BLANK, g_localPlayer);
					g_pendingHudMessageVoiceSfxId = 0;
				}
			}
		}
	}

	g_flightGlobalCountdownTimers[MISSION_MESSAGE_SCAN_TIMER_IDX] = MISSION_LOGIC_REFRESH_TICKS;
}

// FUNCTION: XWA 0x4154A0
void Mission_SetActiveRegionObjectRanges(int newRegionIdx) {
	uint32_t regionSlotBase;
	uint32_t craftSlotEnd;
	uint32_t debrisSlotEnd;
	uint32_t explosionSlotEnd;
	uint32_t salvageJunkSlotEnd;
	uint32_t projectileSlotEnd;
	int genus;

	regionIdx = newRegionIdx;

	regionSlotBase = (uint32_t)newRegionIdx * (g_regionObjectSlotEnd / (uint32_t)g_missionRegionCount);
	g_activeRegionObjectSlotStart = regionSlotBase;
	g_regionMainObjectSlotStart = regionSlotBase;
	craftSlotEnd = regionSlotBase + g_craftObjectSlotsTotal / (uint32_t)g_missionRegionCount;
	g_activeRegionCraftObjectSlotEnd = craftSlotEnd;
	g_projectileObjectSlotEnd = craftSlotEnd + g_projectileObjectSlotsPerRegion;
	g_salvageJunkObjectSlotStart = g_projectileObjectSlotEnd;
	g_debrisObjectSlotStart = g_salvageJunkObjectSlotStart + g_salvageJunkObjectSlotsPerRegion;
	g_salvageJunkObjectSlotEnd = g_debrisObjectSlotStart;
	g_projectileObjectSlotStart = craftSlotEnd;

	debrisSlotEnd = g_debrisObjectSlotStart + g_debrisObjectSlotsTotal / (uint32_t)g_missionRegionCount;
	g_debrisObjectSlotEnd = debrisSlotEnd;
	g_explosionObjectSlotStart = debrisSlotEnd;
	explosionSlotEnd = debrisSlotEnd + g_explosionObjectSlotsTotal / (uint32_t)g_missionRegionCount;
	g_explosionObjectSlotEnd = explosionSlotEnd;
	g_mobileObjectCharDataSlotStart = explosionSlotEnd;
	g_mobileObjectCharDataSlotEnd =
		explosionSlotEnd + g_mobileObjectCharDataCount / (uint32_t)g_missionRegionCount;
	g_regionMainObjectSlotEnd =
		regionSlotBase + g_regionMainObjectSlotsTotal / (uint32_t)g_missionRegionCount;
	g_objScanStart = g_regionMainObjectSlotEnd;
	g_regionStaticObjectSlotEnd =
		g_regionMainObjectSlotEnd + g_regionStaticObjectSlotsTotal / (uint32_t)g_missionRegionCount;

	for (genus = GENUS_Fighter; genus <= GENUS_Platform; ++genus) {
		g_objectSlotRangeByGenus[genus].next = regionSlotBase;
		g_objectSlotRangeByGenus[genus].end = craftSlotEnd;
	}

	salvageJunkSlotEnd = g_debrisObjectSlotStart;
	g_objectSlotRangeByGenus[GENUS_PlayerProjectile].next = craftSlotEnd;
	g_objectSlotRangeByGenus[GENUS_SatelliteBuoy].next = regionSlotBase;
	projectileSlotEnd = g_projectileObjectSlotEnd;
	g_objectSlotRangeByGenus[GENUS_PlayerProjectile].end =
		craftSlotEnd + g_sharedPlayerProjectileSlotsPerRegion + g_playerProjectileSlotsTotal;
	g_objectSlotRangeByGenus[GENUS_NpcProjectile].next =
		craftSlotEnd + g_sharedPlayerProjectileSlotsPerRegion + g_playerProjectileSlotsTotal;
	g_objectSlotRangeByGenus[GENUS_NpcProjectile].end = projectileSlotEnd;
	g_objectSlotRangeByGenus[GENUS_Mine].next = 0;
	g_objectSlotRangeByGenus[GENUS_Mine].end = 0;
	g_objectSlotRangeByGenus[GENUS_Asteroid].next = 0;
	g_objectSlotRangeByGenus[GENUS_Asteroid].end = 0;
	g_objectSlotRangeByGenus[GENUS_TextureSprite].next = 0;
	g_objectSlotRangeByGenus[GENUS_TextureSprite].end = 0;
	g_objectSlotRangeByGenus[GENUS_DeathStarTunnelSegment].next = 0;
	g_objectSlotRangeByGenus[GENUS_DeathStarTunnelSegment].end = 0;
	g_objectSlotRangeByGenus[GENUS_SatelliteBuoy].end = craftSlotEnd;
	g_objectSlotRangeByGenus[16].end = g_mobileObjectCharDataSlotEnd;
	g_objectSlotRangeByGenus[GENUS_Debris].next = g_debrisObjectSlotStart;
	g_objectSlotRangeByGenus[GENUS_Debris].end = debrisSlotEnd;
	g_objectSlotRangeByGenus[GENUS_Explosion].next = debrisSlotEnd;
	g_objectSlotRangeByGenus[GENUS_Explosion].end = explosionSlotEnd;
	g_objectSlotRangeByGenus[GENUS_LargeScenery].next = regionSlotBase;
	g_objectSlotRangeByGenus[GENUS_LargeScenery].end = craftSlotEnd;
	g_objectSlotRangeByGenus[16].next = explosionSlotEnd;
	g_objectSlotRangeByGenus[GENUS_Rubble].next = regionSlotBase;
	g_objectSlotRangeByGenus[GENUS_Rubble].end = craftSlotEnd;

	for (genus = GENUS_Container; genus <= GENUS_WeaponEmplacement; ++genus) {
		g_objectSlotRangeByGenus[genus].next = regionSlotBase;
		g_objectSlotRangeByGenus[genus].end = craftSlotEnd;
	}

	g_objectSlotRangeByGenus[GENUS_SalvageJunk].end = salvageJunkSlotEnd;
	g_objectSlotRangeByGenus[GENUS_SalvageJunk].next = projectileSlotEnd;
}

// FUNCTION: XWA 0x41DA50
void Mission_ResolveObjectOrMissionPointWorldLoc(unsigned int objOrMissionPointRef, int flightGroupIdx,
												 int orderIdx, int waypointSetIdx) {
	int missionX;
	int missionY;
	int missionZ;

	if (objOrMissionPointRef < 0x8000u) {
		worldlocx = g_objectTable[objOrMissionPointRef].world_x;
		worldlocy = g_objectTable[objOrMissionPointRef].world_y;
		worldlocz = g_objectTable[objOrMissionPointRef].world_z;
		return;
	}

	if (objOrMissionPointRef == 0x8000u) {
		objOrMissionPointRef = g_missionFgStats[flightGroupIdx].currentMissionPointRef;
	}

	objOrMissionPointRef -= 0x8000u;
	if (objOrMissionPointRef < 4u) {
		unsigned int missionPointIdx;

		missionPointIdx = objOrMissionPointRef;
		missionX = g_missionFlightGroups[flightGroupIdx].fg.missionPoints[missionPointIdx].x;
		missionY = g_missionFlightGroups[flightGroupIdx].fg.missionPoints[missionPointIdx].y;
		missionZ = g_missionFlightGroups[flightGroupIdx].fg.missionPoints[missionPointIdx].z;
	} else {
		unsigned int waypointIdx;

		objOrMissionPointRef -= 4u;
		waypointIdx = objOrMissionPointRef;
		missionX = g_missionFlightGroups[flightGroupIdx]
					   .fg.orders[waypointSetIdx + 4 * orderIdx]
					   .waypoints[waypointIdx]
					   .x;
		missionY = g_missionFlightGroups[flightGroupIdx]
					   .fg.orders[waypointSetIdx + 4 * orderIdx]
					   .waypoints[waypointIdx]
					   .y;
		missionZ = g_missionFlightGroups[flightGroupIdx]
					   .fg.orders[waypointSetIdx + 4 * orderIdx]
					   .waypoints[waypointIdx]
					   .z;
	}

#ifdef XWA_MODERN
	worldlocx = missionX * 256;
	worldlocy = -(missionY * 256);
	worldlocz = missionZ * 256;
#else
	worldlocx = missionX << 8;
	worldlocy = -(missionY << 8);
	worldlocz = missionZ << 8;
#endif
}

// FUNCTION: XWA 0x41EA00
void Mission_ResolveFormationSlotWorldLoc(uint16_t flightGroupIdx, uint16_t formationSlotIdx,
										  uint16_t basisObjIdx) {
	ModelIndex modelIndex;
	uint16_t objectType;
	int16_t modelMaxZ;

	objectType =
		(ObjectTypeId)
			g_objectTypeTables.craftTypeToObjectType[g_missionFlightGroups[flightGroupIdx].fg.craftType];
	modelIndex = g_modelTypeTable[objectType].modelIndex;
	modelMaxZ = ModelBounds_GetMaxZ(objectType);

	Mission_ResolveObjectOrMissionPointWorldLoc(0x8000u, flightGroupIdx, 0, 0);
	if (g_modelTypeTable[objectType].flags & 0x80) {
		ModelGenusId genusId;

		genusId = g_modelTypeTable[objectType].genusId;
		if (genusId == GENUS_SatelliteBuoy) {
			int startX;

			startX = g_missionFlightGroups[flightGroupIdx].fg.missionPoints[XWA_FG_POINT_START_1].x;
			worldlocy =
				-256 * (int)g_missionFlightGroups[flightGroupIdx].fg.missionPoints[XWA_FG_POINT_START_1].y;
			worldlocx = startX << 8;
			worldlocz =
				modelMaxZ +
				((int)g_missionFlightGroups[flightGroupIdx].fg.missionPoints[XWA_FG_POINT_START_1].z << 8);
			return;
		}

		if (genusId == GENUS_Mine) {
			int zColStep;
			int xStep;
			int yStep;
			int zRowStep;
			int16_t craftCount;
			int countMinusOne;
			int16_t startX;
			int16_t startY;
			int16_t startZ;
			int16_t row;
			uint16_t slot;

			zColStep = 0;
			xStep = 0;
			yStep = 0;
			zRowStep = 0;
			switch (g_missionFlightGroups[flightGroupIdx].fg.status1 & 3u) {
				case 0:
					xStep = 64;
					yStep = 64;
					break;
				case 1:
					yStep = 64;
					zColStep = 64;
					break;
				case 2:
					xStep = 64;
					zRowStep = 64;
					break;
			}

			craftCount = g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft;
			countMinusOne = (int16_t)(craftCount - 1);
			startX =
				(int16_t)(g_missionFlightGroups[flightGroupIdx].fg.missionPoints[XWA_FG_POINT_START_1].x -
						  (countMinusOne * xStep) / 2);
			startY =
				(int16_t)(-((countMinusOne * yStep) / 2) -
						  g_missionFlightGroups[flightGroupIdx].fg.missionPoints[XWA_FG_POINT_START_1].y);
			startZ =
				(int16_t)(g_missionFlightGroups[flightGroupIdx].fg.missionPoints[XWA_FG_POINT_START_1].z -
						  (countMinusOne * zColStep) / 2);
			startZ = (int16_t)(startZ - (countMinusOne * zRowStep) / 2);

			slot = 0;
			if (craftCount > 0) {
				for (row = 0; row < craftCount; ++row) {
					int16_t col;

					for (col = 0; col < craftCount; ++col) {
						if (slot == formationSlotIdx) {
							int x;
							int y;
							int z;

							x = (int)startX + xStep * col;
							y = (int)startY + yStep * row;
							z = (int)startZ + zColStep * col + zRowStep * row;
							worldlocx = x << 8;
							worldlocy = y << 8;
							worldlocz = modelMaxZ + (z << 8);
							return;
						}
						++slot;
					}
				}
			}
		}

	} else {
		uint16_t spacingScale;
		uint16_t formation;
		int formX;
		int formY;
		int formZ;
		int16_t boundX;
		int16_t boundZ;
		int16_t boundY;
		int sideArg;
		int upArg;
		int fwdArg;
		ObjectRecord* basisObj;
		MobileObject* basisMobj;

		spacingScale = (uint16_t)g_missionFlightGroups[flightGroupIdx].fg.formationSpacing + 1;
		formation = g_missionFlightGroups[flightGroupIdx].fg.formation;
		formX = g_formPosX[formation][formationSlotIdx];
		formY = g_formPosY[formation][formationSlotIdx];
		formZ = g_formPosZ[formation][formationSlotIdx];
		boundX = (int16_t)g_modelDefs[modelIndex].boundSizeX;
		boundZ = (int16_t)g_modelDefs[modelIndex].boundSizeZ;
		boundY = (int16_t)g_modelDefs[modelIndex].boundSizeY;
		sideArg = formX * spacingScale * boundX;
		upArg = formZ * spacingScale * boundZ;
		fwdArg = formY * spacingScale * boundY;

		if ((uint16_t)spacingScale == 1) {
			sideArg += formX * (boundX / 2);
			upArg += formZ * (boundZ / 2);
			fwdArg += formY * (boundY / 4);
		}

		Mission_ResolveObjectOrMissionPointWorldLoc(0x8000u, flightGroupIdx, 0, 0);
		if (basisObjIdx != 0xffffu) {
			basisObj = &g_objectTable[basisObjIdx];
			basisMobj = basisObj->mobj;
			if (basisMobj != NULL && basisMobj->speed == 0) {
				pai_calcrotatedpoint(basisObj, (int16_t)sideArg, (int16_t)upArg, (int16_t)fwdArg);
				if (g_modelDefs[modelIndex].boundSizeShift != 0) {
					unsigned int shift;

					shift = g_modelDefs[modelIndex].boundSizeShift;
					g_rotatedX <<= shift;
					g_rotatedY <<= shift;
					g_rotatedZ <<= shift;
				}

				worldlocx += g_rotatedX;
				worldlocy += g_rotatedY;
				worldlocz += modelMaxZ + g_rotatedZ;
				return;
			}
		}
	}

	worldlocz += modelMaxZ;
}

// FUNCTION: XWA 0x4DA340
int Mission_DecodeOrderTime(uint8_t encodedTime) {
	unsigned int result;

	result = encodedTime;
	if (result >= 20) {
		if (result < 196) {
			return (int)(result + 4 * result - 80);
		}
		result += 4 * result;
		return (int)(result + result - 1060);
	}
	return (int)result;
}

// FUNCTION: XWA 0x4DA310
__inline int Mission_GameTimeToSeconds(uint8_t hours, uint8_t minutes, uint8_t seconds) {
	return seconds + 60 * (minutes + 60 * hours);
}

// FUNCTION: XWA 0x511A60
int Mission_GetElapsedClockSeconds(void) {
	return Mission_GameTimeToSeconds(g_missionElapsedClock.hours, g_missionElapsedClock.minutes,
									 g_missionElapsedClock.seconds);
}

// Resets per-mission scoring/kill accounting for a player slot bound to a
// freshly spawned player craft. The original inlines this block twice (pilot
// and gunner slots); factored out here for clarity.
static __inline void Mission_ResetPlayerSpawnStats(int playerIdx, uint16_t boundSignature, uint16_t fgIdx) {
	PerMissionKills* pk = &g_players[playerIdx].perMissionKills;
	int i;

	g_players[playerIdx].boundObjectSignature = boundSignature;
	g_players[playerIdx].boundFlightGroupIdx = fgIdx;
	g_players[playerIdx].missionStats.laserHitsScored = 0;
	g_players[playerIdx].missionStats.laserShotsFired = 0;
	g_players[playerIdx].missionStats.ionHitsScored = 0;
	g_players[playerIdx].missionStats.ionShotsFired = 0;
	pk->warheadHits = 0;
	g_players[playerIdx].warheadsFired = 0;
	g_players[playerIdx].missionStats.missionScore = 0;
	g_players[playerIdx].missionStats.missionBonusScoreTenths = 0;
	g_players[playerIdx].missionStats.ratingPromoPoints = 0;
	g_players[playerIdx].missionStats.worseRatingPromoPoints = 0;
	g_players[playerIdx].missionStats.field10 = 0;
	g_players[playerIdx].missionStats.field14 = 0;
	g_players[playerIdx].missionStats.field18 = 0;
	pk->friendliesKilled = 0;
	pk->field2 = 0;
	pk->numSpecialInspected = 0;
	for (i = 0; i < 192; ++i) {
		pk->killsFullOnFlightGroup[i] = 0;
		pk->killsSharedOnFlightGroup[i] = 0;
		pk->killsAssistOnFlightGroup[i] = 0;
		pk->killsFullFromFlightGroup[i] = 0;
		pk->killsSharedFromFlightGroup[i] = 0;
	}
	for (i = 0; i < 25; ++i) {
		pk->killsFullOnPlayerRating[i] = 0;
		pk->killsSharedOnPlayerRating[i] = 0;
		pk->killsAssistOnPlayerRating[i] = 0;
		pk->killedByPlayerRating[i] = 0;
	}
	for (i = 0; i < 6; ++i) {
		pk->killsFullOnAiRating[i] = 0;
		pk->killsSharedOnAiRating[i] = 0;
		pk->killsAssistOnAiRating[i] = 0;
	}
	for (i = 0; i < 8; ++i) {
		pk->killsFullOnPlayer[i] = 0;
		pk->killsSharedOnPlayer[i] = 0;
		pk->killsFullFromPlayer[i] = 0;
		pk->killsSharedFromPlayer[i] = 0;
	}
	pk->killedByAiRating = 0;
	pk->totalCraftLosses = 0;
	pk->lossesByCollisions = 0;
	pk->lossesByStarships = 0;
	pk->lossesByMines = 0;
}

// FUNCTION: XWA 0x418FA0
// Runs flight-group wave-completion handling after one of its craft leaves or
// dies. If no active, non-departing craft from flightGroupIdx remains in any
// scanned region, it either closes the remaining waves (when departure /
// stop-arrival / team-goal rules say the flight group cannot continue) or frees
// leftover slots and spawns the next wave in the start region, decrementing
// wavesRemaining. The original leaves an incidental value in the return
// register, which every caller discards.
void Mission_ProcessFlightGroupWaveCompletion(uint16_t flightGroupIdx) {
	uint8_t wavesRemaining;
	uint16_t craftObjType;
	unsigned int numberOfCraft;
	unsigned int emptyCount;
	unsigned int end;
	unsigned int start;
	int spawnRegion;
	int stopArriving;
	char clear;
	int savedRegion;
	unsigned int slot;
	if (!g_missionFgStats[flightGroupIdx].wavesRemaining || !g_missionFgStats[flightGroupIdx].outcomeCount[0])
		return;

	// Scan regions (starting at the active region) for any live, non-departing
	// craft still belonging to this flight group. Single-region missions skip
	// the scan and proceed straight to completion.
	savedRegion = regionIdx;
	clear = 1;
	{
		int region = regionIdx;
		int iter = g_missionRegionCount - 1;
		while (iter) {
			unsigned int scanSlot;
			Mission_SetActiveRegionObjectRanges(region);
			for (scanSlot = g_activeRegionObjectSlotStart;
				 scanSlot < (unsigned int)g_activeRegionCraftObjectSlotEnd; ++scanSlot) {
				if (g_objectTable[scanSlot].objectType) {
					MobileObject* mobj = g_objectTable[scanSlot].mobj;
					if (!mobj->state && g_objectTable[scanSlot].flightGroupIdx == flightGroupIdx) {
						uint8_t objectKind = mobj->pCraft->objectKind;
						if (objectKind != 3 && objectKind != 4) {
							clear = 0;
							break;
						}
					}
				}
			}
			if (!clear)
				break;
			--iter;
			++region;
			if (region >= g_missionRegionCount)
				region = 0;
		}
	}
	Mission_SetActiveRegionObjectRanges(savedRegion);
	if (!clear)
		return;

	// Already-complete team goals halt further wave processing.
	if ((g_missionFlightRuntimeState
				 .teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team][TEAM_GOAL_PRIMARY] == 1 &&
		 g_missionFlightRuntimeState
				 .teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team][TEAM_GOAL_SECONDARY] == 1) ||
		(g_missionFlightRuntimeState
				 .teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team][TEAM_GOAL_PRIMARY] == 1 &&
		 g_missionFlightRuntimeState
				 .teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team][TEAM_GOAL_SECONDARY] == 2) ||
		(g_missionFlightRuntimeState
				 .teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team][TEAM_GOAL_PRIMARY] == 2 &&
		 g_missionFlightRuntimeState
				 .teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team][TEAM_GOAL_SECONDARY] == 1))
		return;

	// Decide whether the flight group must stop sending waves.
	stopArriving = 0;
	if (g_playerFlightGroupWaveMode != 2) {
		uint8_t stopArrivingWhen;
		if ((g_missionFlightGroups[flightGroupIdx].fg.departure.triggers[0].condition ||
			 g_missionFlightGroups[flightGroupIdx].fg.departure.triggers[1].condition) &&
			(Mission_EvaluateTriggerPair(&g_missionFlightGroups[flightGroupIdx].fg.departure, 0) & 1) != 0)
			stopArriving = 1;
		stopArrivingWhen = g_missionFlightGroups[flightGroupIdx].fg.stopArrivingWhen;
		if (stopArrivingWhen == 1 && g_missionFgStats[flightGroupIdx].outcomeCount[16])
			stopArriving = 1;
		if (stopArrivingWhen == 2 &&
			g_missionFlightRuntimeState
					.teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team][TEAM_GOAL_PRIMARY] == 1)
			stopArriving = 1;
		if (stopArrivingWhen == 3 &&
			(g_missionFlightRuntimeState
					 .teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team][TEAM_GOAL_PRIMARY] == 2 ||
			 g_missionFlightRuntimeState.teamGoalStatus[g_missionFlightGroups[flightGroupIdx].fg.team]
													   [TEAM_GOAL_SECONDARY] == 1))
			stopArriving = 1;
	}

	if (stopArriving) {
		uint8_t departMethod;
		// Close the flight group: tally the never-arrived craft against the
		// departed/aborted/loss outcome counters and clear remaining waves.
		int notArrived = g_missionFgStats[flightGroupIdx].outcomeCount[0] -
						 g_missionFgStats[flightGroupIdx].outcomeCount[1];
		int notArrivedSpecial = g_missionFgStats[flightGroupIdx].specialCargoOutcome[0] -
								g_missionFgStats[flightGroupIdx].specialCargoOutcome[1];
		g_missionFgStats[flightGroupIdx].outcomeCount[1] += notArrived;
		g_missionFgStats[flightGroupIdx].outcomeCount[22] += notArrived;
		g_missionFgStats[flightGroupIdx].specialCargoOutcome[22] += notArrivedSpecial;
		g_missionFgStats[flightGroupIdx].outcomeCount[9] += notArrived;
		g_missionFgStats[flightGroupIdx].specialCargoOutcome[9] += notArrivedSpecial;
		g_missionFgStats[flightGroupIdx].outcomeCount[15] += notArrived;
		g_missionFgStats[flightGroupIdx].specialCargoOutcome[15] += notArrivedSpecial;
		g_missionFgStats[flightGroupIdx].outcomeCount[7] += notArrived;
		g_missionFgStats[flightGroupIdx].specialCargoOutcome[7] += notArrivedSpecial;
		g_missionFgStats[flightGroupIdx].outcomeCount[5] += notArrived;
		g_missionFgStats[flightGroupIdx].specialCargoOutcome[5] += notArrivedSpecial;
		g_missionFgStats[flightGroupIdx].outcomeCount[11] += notArrived;
		g_missionFgStats[flightGroupIdx].specialCargoOutcome[11] += notArrivedSpecial;
		departMethod = g_missionFlightGroups[flightGroupIdx].fg.departMethod;
		g_missionFgStats[flightGroupIdx].wavesRemaining = 0;
		if ((departMethod || g_missionFlightGroups[flightGroupIdx].fg.alternateMothershipUsed) &&
			(departMethod != 2 || g_missionFlightGroups[flightGroupIdx].fg.alternateMothershipUsed))
			g_missionFgStats[flightGroupIdx].outcomeCount[18] += notArrived;
		else
			g_missionFgStats[flightGroupIdx].outcomeCount[17] += notArrived;
		return;
	}

	// Otherwise, free leftover slots in the spawn region and launch next wave.
	g_currentFlightGroupIdx = flightGroupIdx;
	if (g_missionFlightGroups[flightGroupIdx].fg.missionPoints[XWA_FG_POINT_START_2].enabled)
		spawnRegion = g_missionFlightGroups[flightGroupIdx].fg.missionPointRegions[XWA_FG_POINT_START_2];
	else
		spawnRegion = g_missionFlightGroups[flightGroupIdx].fg.missionPointRegions[XWA_FG_POINT_START_1];
	Mission_SetActiveRegionObjectRanges(spawnRegion);

	start = g_activeRegionObjectSlotStart;
	end = g_activeRegionCraftObjectSlotEnd;
	emptyCount = 0;
	for (slot = start; slot < end; ++slot) {
		if (g_objectTable[slot].objectType == OBJ_None)
			++emptyCount;
	}
	numberOfCraft = g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft;
	if (emptyCount < numberOfCraft) {
		unsigned int reclaimSlot = g_activeRegionObjectSlotStart;
		unsigned int needed;
		needed = numberOfCraft - emptyCount;
		// Pass 1: reclaim ownerless explosion objects.
		for (; reclaimSlot < g_activeRegionCraftObjectSlotEnd; ++reclaimSlot) {
			if (g_objectTable[reclaimSlot].objectType &&
				g_objectTable[reclaimSlot].genusId == GENUS_Explosion &&
				g_objectTable[reclaimSlot].playerOwnerIdx == -1) {
				g_objectTable[reclaimSlot].objectType = OBJ_None;
				Mission_RecordCraftOutcome((uint16_t)reclaimSlot, g_objectTable[reclaimSlot].flightGroupIdx,
										   2u);
				if (!--needed)
					break;
			}
		}
		// Pass 2: reclaim ownerless departing craft (and their linked piece).
		if (needed) {
			for (reclaimSlot = g_activeRegionObjectSlotStart; reclaimSlot < g_activeRegionCraftObjectSlotEnd;
				 ++reclaimSlot) {
				uint8_t kind;
				CraftData* pCraft;
				MobileObject* mobj;
				if (!g_objectTable[reclaimSlot].objectType)
					continue;
				mobj = g_objectTable[reclaimSlot].mobj;
				if (mobj->state || g_objectTable[reclaimSlot].playerOwnerIdx != -1)
					continue;
				pCraft = mobj->pCraft;
				kind = pCraft->objectKind;
				if (kind != 3 && kind != 4)
					continue;
				g_objectTable[reclaimSlot].objectType = OBJ_None;
				if (pCraft->effectiveAiObjectLink) {
					pCraft->effectiveAiObjectLink->objectType = OBJ_None;
					pCraft->effectiveAiObjectLink = NULL;
				}
				Mission_RecordCraftOutcome((uint16_t)reclaimSlot, g_objectTable[reclaimSlot].flightGroupIdx,
										   2u);
				if (!--needed)
					break;
			}
		}
	}

	craftObjType = (uint16_t)g_objectTypeTables
					   .craftTypeToObjectType[g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType];
	if (!(g_modelTypeTable[craftObjType].flags & 0x80))
		Mission_SpawnFlightGroupWaveCraft(0xFFFFu);
	else
		Mission_SpawnFlightGroupStaticObjects(0xFFFFu);

	wavesRemaining = g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining;
	if (wavesRemaining &&
		(!g_missionFlightGroups[g_currentFlightGroupIdx].fg.playerNumber || g_playerFlightGroupWaveMode != 2))
		g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining = wavesRemaining - 1;

	// Proving-grounds: snap the bound player's craft back to its recovery pose.
	if (g_provingGroundsModeActive) {
		int pi = 0;
		PlayerData* player = g_players;
		while (pi < g_flightPlayerCount) {
			if (player->boundFlightGroupIdx == flightGroupIdx) {
				int objectIndex = g_players[pi].objectIndex;
				if (objectIndex != 0xFFFF) {
					g_objectTable[objectIndex].mobj->collisionObjIdx =
						g_yardContext.playerChallengeStates[pi].recoveryCollisionObjIdx;
					g_objectTable[objectIndex].mobj->prevWorldX =
						g_yardContext.playerChallengeStates[pi].recoveryWorldX;
					g_objectTable[objectIndex].world_x = g_objectTable[objectIndex].mobj->prevWorldX;
					g_objectTable[objectIndex].mobj->prevWorldY =
						g_yardContext.playerChallengeStates[pi].recoveryWorldY;
					g_objectTable[objectIndex].world_y = g_objectTable[objectIndex].mobj->prevWorldY;
					g_objectTable[objectIndex].mobj->prevWorldZ =
						g_yardContext.playerChallengeStates[pi].recoveryWorldZ;
					g_objectTable[objectIndex].world_z = g_objectTable[objectIndex].mobj->prevWorldZ;
					g_objectTable[objectIndex].yaw = g_yardContext.playerChallengeStates[pi].recoveryYaw;
					g_objectTable[objectIndex].pitch = g_yardContext.playerChallengeStates[pi].recoveryPitch;
				}
				break;
			}
			++pi;
			++player;
		}
	}
	return;
}

// Combines two arrival-trigger condition results (0/1/2/4) with AND semantics:
// both met -> 1, either pending -> 2, otherwise -> 4.
static __inline char Mission_CombineArrivalConditionPair(int result1, int result2) {
	if ((result1 & result2 & 1) != 0)
		return 1;
	return ((result1 | result2) & 2) != 0 ? 2 : 4;
}

// Stamps a flight group's designation code into g_missionFlightRuntimeState.teamFgDesignationCode for the
// team(s) selected by the FG's enableDesignation byte (direct team index < 8, or
// a relationship selector: 8 all, 9 other teams, 10 non-allies, 11 allies).
static __inline void Mission_AssignFgDesignationCode(int fgIdx, int team, uint8_t selector, int code) {
	int t;
	if (selector < 8) {
		g_missionFlightRuntimeState.teamFgDesignationCode[selector][fgIdx] = code;
		return;
	}
	for (t = 0; t < 10; ++t) {
		if (selector == 8)
			g_missionFlightRuntimeState.teamFgDesignationCode[t][fgIdx] = code;
		else if (selector == 9) {
			if (team != t)
				g_missionFlightRuntimeState.teamFgDesignationCode[t][fgIdx] = code;
		} else if (selector == 11) {
			if (g_missionTeams[team].allies[t] == 1)
				g_missionFlightRuntimeState.teamFgDesignationCode[t][fgIdx] = code;
		} else if (selector == 10) {
			if (g_missionTeams[team].allies[t] == 0)
				g_missionFlightRuntimeState.teamFgDesignationCode[t][fgIdx] = code;
		}
	}
}

static __inline void Mission_SpawnCurrentFlightGroupAtMissionStart(void) {
	ObjectTypeId objType;

	Mission_SetActiveRegionObjectRanges(
		g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPointRegions[XWA_FG_POINT_START_1]);
	g_missionFgStats[g_currentFlightGroupIdx].hasArrived = 1;
	objType = (ObjectTypeId)g_objectTypeTables
				  .craftTypeToObjectType[g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType];
	if ((g_modelTypeTable[(uint16_t)objType].flags & 0x80) == 0) {
		g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining =
			g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfWaves;
		Mission_SpawnFlightGroupWaveCraft(0xFFFFu);
	} else {
		if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
			g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH)
			g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining =
				g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfWaves;
		else
			g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining = 0;
		Mission_SpawnFlightGroupStaticObjects(0xFFFFu);
	}
}

// FUNCTION: XWA 0x417580
// Initializes in-flight runtime state after mission load and model/SFX loading:
// builds per-region hyperspace point tables from FG designations, pre-evaluates
// arrival availability, resets MissionFgRuntimeStats accounting, spawns the
// initially-arrived flight groups, fixes up linked carried objects, clears
// team/player/HUD/message/input runtime state, applies single-player cheat
// loadout flags, and reinitializes the flight command menu.
void Mission_InitFlightRuntimeState(void) {
	unsigned int p;

	FlightSurface_Unlock();
	g_initialSpawnBindPlayerCraftSlots = 1;
	g_nextObjectSignature = 2;
	g_escapePodPilotFlightGroupIdx = -1;

	// Clear all region hyperspace point validity flags.
	{
		int from;
		int to;
		for (from = 0; from < 5; ++from) {
			for (to = 0; to < 5; ++to) {
				g_missionRegionHyperPoints.arrivalPointValid[from][to] = 0;
				g_missionRegionHyperPoints.departureRoutePointValid[from][to] = 0;
			}
		}
	}

	// Build hyperspace point tables from each FG's designation markers.
	{
		int i;
		int16_t flightGroupCount = (int16_t)g_missionHeader.numFlightGroups;
		g_currentFlightGroupIdx = 0;
		if (flightGroupCount > 0) {
			do {
				int startRegion = g_missionFlightGroups[g_currentFlightGroupIdx]
									  .fg.missionPointRegions[XWA_FG_POINT_START_1];
				if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.enableDesignation != 0xFF &&
					g_missionFlightGroups[g_currentFlightGroupIdx].fg.designation1 >= 12) {
					Mission_ResolveObjectOrMissionPointWorldLoc(0x8000u, g_currentFlightGroupIdx, 0, 0);
					{
						uint8_t designation = g_missionFlightGroups[g_currentFlightGroupIdx].fg.designation1;
						if (designation < 16) {
							int idx = designation - 12;
							g_missionRegionHyperPoints.arrivalPointValid[startRegion][idx] = 1;
							g_missionRegionHyperPoints.arrivalPoint[startRegion][idx].x = worldlocx;
							g_missionRegionHyperPoints.arrivalPoint[startRegion][idx].y = worldlocy;
							g_missionRegionHyperPoints.arrivalPoint[startRegion][idx].z = worldlocz;
						} else if (designation < 20) {
							int idx = designation - 16;
							g_missionRegionHyperPoints.departureRoutePointValid[startRegion][idx] = 1;
							g_missionRegionHyperPoints.departureRoutePoint[startRegion][idx].x = worldlocx;
							g_missionRegionHyperPoints.departureRoutePoint[startRegion][idx].y = worldlocy;
							g_missionRegionHyperPoints.departureRoutePoint[startRegion][idx].z = worldlocz;
						} else if (designation == 20) {
							for (i = 0; i < 5; ++i) {
								g_missionRegionHyperPoints.arrivalPointValid[startRegion][i] = 1;
								g_missionRegionHyperPoints.arrivalPoint[startRegion][i].x = worldlocx;
								g_missionRegionHyperPoints.arrivalPoint[startRegion][i].y = worldlocy;
								g_missionRegionHyperPoints.arrivalPoint[startRegion][i].z = worldlocz;
							}
						} else if (designation == 21) {
							for (i = 0; i < 5; ++i) {
								g_missionRegionHyperPoints.departureRoutePointValid[startRegion][i] = 1;
								g_missionRegionHyperPoints.departureRoutePoint[startRegion][i].x = worldlocx;
								g_missionRegionHyperPoints.departureRoutePoint[startRegion][i].y = worldlocy;
								g_missionRegionHyperPoints.departureRoutePoint[startRegion][i].z = worldlocz;
							}
						}
					}
				}
				if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.enableDesignation2 != 0xFF &&
					g_missionFlightGroups[g_currentFlightGroupIdx].fg.designation2 >= 12) {
					Mission_ResolveObjectOrMissionPointWorldLoc(0x8000u, g_currentFlightGroupIdx, 0, 0);
					{
						uint8_t designation = g_missionFlightGroups[g_currentFlightGroupIdx].fg.designation2;
						if (designation < 16) {
							int idx = designation - 12;
							g_missionRegionHyperPoints.arrivalPointValid[startRegion][idx] = 1;
							g_missionRegionHyperPoints.arrivalPoint[startRegion][idx].x = worldlocx;
							g_missionRegionHyperPoints.arrivalPoint[startRegion][idx].y = worldlocy;
							g_missionRegionHyperPoints.arrivalPoint[startRegion][idx].z = worldlocz;
						} else if (designation < 20) {
							int idx = designation - 16;
							g_missionRegionHyperPoints.departureRoutePointValid[startRegion][idx] = 1;
							g_missionRegionHyperPoints.departureRoutePoint[startRegion][idx].x = worldlocx;
							g_missionRegionHyperPoints.departureRoutePoint[startRegion][idx].y = worldlocy;
							g_missionRegionHyperPoints.departureRoutePoint[startRegion][idx].z = worldlocz;
						} else if (designation == 20) {
							int x = worldlocx;
							int y = worldlocy;
							int z = worldlocz;
							for (i = 0; i < 5; ++i) {
								g_missionRegionHyperPoints.arrivalPointValid[startRegion][i] = 1;
								g_missionRegionHyperPoints.arrivalPoint[startRegion][i].x = x;
								g_missionRegionHyperPoints.arrivalPoint[startRegion][i].y = y;
								g_missionRegionHyperPoints.arrivalPoint[startRegion][i].z = z;
							}
						} else if (designation == 21) {
							int x = worldlocx;
							int y = worldlocy;
							int z = worldlocz;
							for (i = 0; i < 5; ++i) {
								g_missionRegionHyperPoints.departureRoutePointValid[startRegion][i] = 1;
								g_missionRegionHyperPoints.departureRoutePoint[startRegion][i].x = x;
								g_missionRegionHyperPoints.departureRoutePoint[startRegion][i].y = y;
								g_missionRegionHyperPoints.departureRoutePoint[startRegion][i].z = z;
							}
						}
					}
				}
			} while (++g_currentFlightGroupIdx < (int16_t)g_missionHeader.numFlightGroups);
		}
	}

	// Pre-evaluate arrival availability per FG (difficulty mask + special
	// condition 41/42 pending-state check).
	{
		int16_t flightGroupCount = (int16_t)g_missionHeader.numFlightGroups;
		g_currentFlightGroupIdx = 0;
		if (flightGroupCount > 0) {
			do {
				char combined;
				uint8_t d1;
				uint8_t d2;
				uint8_t c1;
				uint8_t c2;
				char a1state;
				char a2state;
				uint8_t enabled = g_missionDifficultyArrivalMasks[g_flightDifficulty] &
								  g_fgArrivalDifficultyMasks[g_missionFlightGroups[g_currentFlightGroupIdx]
																 .fg.arrivalDifficulty];
				g_missionFgStats[g_currentFlightGroupIdx].arrivalEnabled = enabled;
				if (enabled) {
					a1state = 4;
					a2state = 4;
					c1 = g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrival[0].triggers[0].condition;
					c2 = g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrival[0].triggers[1].condition;
					if ((c1 == 41 || c2 == 41 || c1 == 42 || c2 == 42) &&
						!g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrival[0].t1OrT2) {
						int r1 = Mission_EvaluateCondition(
							&g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrival[0].triggers[0], 0, 10);
						int r2 = Mission_EvaluateCondition(
							&g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrival[0].triggers[1], 0, 10);
						a1state = Mission_CombineArrivalConditionPair(r1, r2);
					}
					d1 = g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrival[1].triggers[0].condition;
					d2 = g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrival[1].triggers[1].condition;
					if ((d1 == 41 || d2 == 41 || d1 == 42 || d2 == 42) &&
						!g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrival[1].t1OrT2) {
						int r1 = Mission_EvaluateCondition(
							&g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrival[1].triggers[0], 0, 10);
						int r2 = Mission_EvaluateCondition(
							&g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrival[1].triggers[1], 0, 10);
						a2state = Mission_CombineArrivalConditionPair(r1, r2);
					}
					if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivals12OrArrivals34 == 1) {
						if (((a1state | a2state) & 1) != 0)
							combined = 1;
						else
							combined = (a1state & a2state & 2) != 0 ? 2 : 4;
					} else {
						if ((a1state & a2state & 1) != 0)
							combined = 1;
						else
							combined = ((a1state | a2state) & 2) != 0 ? 2 : 4;
					}
					if (combined & 2)
						g_missionFgStats[g_currentFlightGroupIdx].arrivalEnabled = 0;
				}
			} while (++g_currentFlightGroupIdx < (int16_t)g_missionHeader.numFlightGroups);
		}
	}

	// Reset per-FG mission accounting and seed the expected craft instance count.
	{
		uint16_t cur;
		int16_t numFlightGroups = (int16_t)g_missionHeader.numFlightGroups;
		int i;
		int t;
		int r;
		for (cur = 0; cur < numFlightGroups; ++cur) {
			ObjectTypeId objType;
			int16_t instances;
			uint8_t numberOfCraft;
			g_missionFgStats[cur].hasArrived = 0;
			g_missionFgStats[cur].arrivalDelayPending = 0;
			g_missionFgStats[cur].wavesRemaining = 0;
			g_missionFgStats[cur].spawnedCraftCount = 0;
			g_missionFgStats[cur].arrivalDelayTimer = 0;

			numberOfCraft = g_missionFlightGroups[cur].fg.numberOfCraft;
			instances = numberOfCraft;
			objType = (ObjectTypeId)
						  g_objectTypeTables.craftTypeToObjectType[g_missionFlightGroups[cur].fg.craftType];
			if (g_modelTypeTable[(uint16_t)objType].genusId == GENUS_Mine)
				instances = numberOfCraft * numberOfCraft;
			if (objType == OBJ_CrewCabinFront || objType == OBJ_EngineFront)
				instances *= 2;

			if (g_missionFgStats[cur].arrivalEnabled || g_missionFlightGroups[cur].playerOwnerIdx != -1) {
				uint8_t numberOfWaves = g_missionFlightGroups[cur].fg.numberOfWaves;
				g_missionFgStats[cur].outcomeCount[0] = instances * (numberOfWaves + 1);
				if (g_missionFlightGroups[cur].fg.randomSpecialCargoCraft == 0 &&
					g_missionFlightGroups[cur].fg.specialCargoCraft >= numberOfCraft)
					g_missionFgStats[cur].specialCargoOutcome[0] = 0;
				else
					g_missionFgStats[cur].specialCargoOutcome[0] = numberOfWaves + 1;
			} else {
				g_missionFgStats[cur].outcomeCount[0] = 0;
				g_missionFgStats[cur].specialCargoOutcome[0] = 0;
			}

			for (i = 1; i < 33; ++i) {
				g_missionFgStats[cur].outcomeCount[i] = 0;
				g_missionFgStats[cur].specialCargoOutcome[i] = 0;
			}
			for (t = 0; t < 10; ++t) {
				g_missionFgStats[cur].teamInspected[t] = 0;
				g_missionFgStats[cur].teamSpecialCargoInspected[t] = 0;
				g_missionFgStats[cur].teamUninspectedLost[t] = 0;
				g_missionFgStats[cur].teamSpecialCargoUninspectedLost[t] = 0;
				g_missionFgStats[cur].teamPartiallyInspected[t] = 0;
				g_missionFgStats[cur].teamSpecialCargoPartiallyInspected[t] = 0;
				g_missionFgStats[cur].teamPartialInspectLost[t] = 0;
				g_missionFgStats[cur].teamSpecialCargoPartialInspectLost[t] = 0;
				g_missionFgStats[cur].teamCondition44Count[t] = 0;
				g_missionFgStats[cur].teamCondition44SpecialCargo[t] = 0;
				g_missionFgStats[cur].teamCondition44OtherTeamCount[t] = 0;
				g_missionFgStats[cur].teamCondition44OtherTeamSpecialCargo[t] = 0;
				g_missionFgStats[cur].teamEventExtra[0][t] = 0;
				g_missionFgStats[cur].teamEventExtra[1][t] = 0;
				g_missionFgStats[cur].teamEventExtra[2][t] = 0;
				g_missionFgStats[cur].teamEventExtra[3][t] = 0;
			}
			for (r = 0; r < 5; ++r) {
				g_missionFgStats[cur].tailEventCounts[r] = 0;
				g_missionFgStats[cur].tailEventCounts[r + 5] = 0;
				g_missionFgStats[cur].tailEventCounts[r + 10] = 0;
				g_missionFgStats[cur].tailEventCounts[r + 15] = 0;
			}
			memset(g_missionFgStats[cur].goalState, 4, sizeof(g_missionFgStats[cur].goalState));
		}
	}

	// Spawn the flight groups that arrive at mission start.
	for (g_currentFlightGroupIdx = 0; g_currentFlightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
		 ++g_currentFlightGroupIdx) {
		ObjectTypeId objType2;
		if (!g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType ||
			(!g_missionFgStats[g_currentFlightGroupIdx].arrivalEnabled &&
			 g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx == -1) ||
			!g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[0])
			continue;

		if (g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx != -1)
			Mission_SpawnCurrentFlightGroupAtMissionStart();
		else if (!g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrival[0].triggers[0].condition &&
				 !g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalDelayMinutes &&
				 !g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalDelaySeconds &&
				 (!g_missionFlightGroups[g_currentFlightGroupIdx].fg.playerNumber ||
				  !g_missionFlightGroups[g_currentFlightGroupIdx].fg.arriveOnlyIfHuman))
			Mission_SpawnCurrentFlightGroupAtMissionStart();
		// Count this FG's craft toward its team's countable-craft flag.
		objType2 = (ObjectTypeId)g_objectTypeTables
					   .craftTypeToObjectType[g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType];
		if ((g_modelTypeTable[(uint16_t)objType2].flags & 0x20) == 0)
			g_missionFlightRuntimeState
				.teamHasCountableCraft[g_missionFlightGroups[g_currentFlightGroupIdx].fg.team] = 1;
	}

	// Re-anchor linked carried objects to their parent mount points.
	{
		unsigned int slot;
		for (slot = 0; slot < g_regionObjectSlotEnd; ++slot) {
			uint16_t childIdx;
			CraftData* parentCraft;
			ObjectRecord* parent = &g_objectTable[slot];
			if (!parent->objectType || !parent->mobj || !parent->mobj->pCraft)
				continue;
			parentCraft = parent->mobj->pCraft;
			if (parentCraft->nextLinkObjectIdx != 0xFFFF)
				continue;
			childIdx = parentCraft->linkedPrevObjectIdx;
			while (childIdx != 0xFFFF) {
				ObjectRecord* child;
				int parentMountZ;
				int parentMountY;
				int parentMountX;
				CraftData* childCraft = g_objectTable[childIdx].mobj->pCraft;
				ModelIndex childModel = childCraft->modelIndex;
				pai_calcrotatedpoint(parent, g_modelDefs[parentCraft->modelIndex].childMountPoints[0],
									 g_modelDefs[parentCraft->modelIndex].childMountPoints[1],
									 g_modelDefs[parentCraft->modelIndex].childMountPoints[2]);
				parentMountX = g_rotatedX;
				parentMountY = g_rotatedY;
				parentMountZ = g_rotatedZ;
				child = &g_objectTable[childIdx];
				pai_calcrotatedpoint(child, g_modelDefs[childModel].childMountPoints[3],
									 g_modelDefs[childModel].childMountPoints[4],
									 g_modelDefs[childModel].childMountPoints[5]);
				child->world_x = parentMountX + parent->world_x - g_rotatedX;
				child->world_y = parentMountY + parent->world_y - g_rotatedY;
				child->world_z = parentMountZ + parent->world_z - g_rotatedZ;
				child->mobj->prevWorldX = child->world_x;
				child->mobj->prevWorldY = child->world_y;
				child->mobj->prevWorldZ = child->world_z;
				parent = child;
				parentCraft = childCraft;
				childIdx = childCraft->linkedPrevObjectIdx;
			}
		}
	}

	// Reset per-team mission state.
	{
		unsigned int teamIdx;
		unsigned int goalKind;
		unsigned int triggerIdx;
		unsigned int fgIdx;

		for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
			g_missionFlightRuntimeState.teamReinforcementCalled[teamIdx] = 0;
			g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][teamIdx] = 0;
			g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION][teamIdx] = 0;
			g_missionFlightRuntimeState.teamMissionCompletionTimeSeconds[teamIdx] = 0;
			g_missionFlightRuntimeState.teamActiveGoalSequence[teamIdx] = 1;
			for (goalKind = 0; goalKind < 3; ++goalKind) {
				g_missionFlightRuntimeState.teamGlobalGoalState[teamIdx][goalKind] = 4;
				g_missionFlightRuntimeState.teamGoalStatus[teamIdx][goalKind] = 0;
				for (triggerIdx = 0; triggerIdx < 4; ++triggerIdx) {
					g_missionFlightRuntimeState.globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_CURRENT][teamIdx]
																	   [goalKind][triggerIdx] = 0;
					g_missionFlightRuntimeState
						.globalGoalTriggerCounts[GOAL_TRIGGER_COUNTER_TOTAL][teamIdx][goalKind][triggerIdx] =
						0;
				}
			}
			g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_FULL][teamIdx] = 0;
			g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_SHARED][teamIdx] = 0;
			g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_ASSIST][teamIdx] = 0;
			g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_LOSS][teamIdx] = 0;
			for (fgIdx = 0; fgIdx < 192; ++fgIdx) {
				g_missionFlightRuntimeState.teamFgCounters[TEAM_FG_COUNTER_INSPECTED][teamIdx][fgIdx] = 0;
				g_missionFlightRuntimeState.teamFgCounters[TEAM_FG_COUNTER_TRANSFER][teamIdx][fgIdx] = 0;
				g_missionFlightRuntimeState.teamFgDesignationCode[teamIdx][fgIdx] = 0;
			}
		}
	}

	// Stamp FG designation codes into the per-team designation table.
	for (g_currentFlightGroupIdx = 0; g_currentFlightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
		 ++g_currentFlightGroupIdx) {
		XwaFlightGroup* fg = &g_missionFlightGroups[g_currentFlightGroupIdx].fg;
		int fgTeam = fg->team;
		int designationCode = 0;
		uint8_t selector;

		if (fg->enableDesignation != 0xFF) {
			int designation = fg->designation1;
			selector = fg->enableDesignation;
			if (designation != -1 && ++designation <= 12)
				designationCode = designation;
		}
		if (designationCode)
			Mission_AssignFgDesignationCode(g_currentFlightGroupIdx, fgTeam, selector, designationCode);

		designationCode = 0;
		if (fg->enableDesignation2 != 0xFF) {
			int designation = fg->designation2;
			selector = fg->enableDesignation2;
			if (designation != -1 && ++designation <= 12)
				designationCode = designation;
		}
		if (designationCode)
			Mission_AssignFgDesignationCode(g_currentFlightGroupIdx, fgTeam, selector, designationCode);
	}

	// Reset per-player flight runtime state.
	for (p = 0; p < XWA_PLAYER_COUNT; ++p) {
		PlayerData* pl = &g_players[p];
		pl->regionSessionId = 0;
		pl->hyperspaceRuntime.targetBoxEnabled = 1;
		pl->targetSubState = 0;
		pl->currentTargetObjectIdx = 0xffffu;
		pl->targetCycleStart = -1;
		pl->selectedTargetComponent = -1;
		pl->targetingState = -1;
		pl->targetPresetSlot[0] = -1;
		pl->targetPresetSlot[1] = -1;
		pl->targetPresetSlot[2] = -1;
		pl->targetPresetSlot[3] = -1;
		pl->engineWashSourceObjIdx = -1;
		pl->engineWashStrength = 0;
		pl->aiControlledFlag =
			(uint8_t)(g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] == 1);
		pl->hyperspacePhase = 0;
		pl->hyperspaceRuntime.targetRegionOrMode = 4;
		pl->hyperspaceRuntime.phaseElapsedTicks = 0;
		pl->hyperspaceRuntime.hyperBuoyPromptCooldown = 0;
		pl->hyperspaceRuntime.regionTransferArrivalCounted[0] = 0;
		pl->hyperspaceRuntime.regionTransferArrivalCounted[1] = 0;
		pl->hyperspaceRuntime.regionTransferArrivalCounted[2] = 0;
		pl->hyperspaceRuntime.regionTransferArrivalCounted[3] = 0;
		pl->hyperspaceRuntime.regionTransferArrivalCounted[4] = 0;
		pl->inputDisabledFlag = 0;
		pl->gap71_field16 = 0;
		pl->smoothedInputYaw = 0;
		pl->smoothedInputPitch = 0;
		pl->smoothedInputRoll = 0;
		pl->savedKeyMods = 0;
		pl->keyModsHoldTimer = 0;
		pl->turretAutoFireState = 0;
		pl->gunnerHardpointToggle = 0;
		memset(pl->turretCamMat, 0, sizeof(pl->turretCamMat));
		memset(pl->msgText, 0, sizeof(pl->msgText));
		pl->msgLength = 0;
		pl->msgTypeId = 0;
	}

	g_localDebrisRecycleSlotCursor = g_localTransientSlotStart;
	Mission_CreateRegionMarkerObjects(0, 131072000, 0);
	g_flightRegionSessionGateMode = 1;

	{
		int i;
		memset(g_flightGlobalCountdownTimers, 0, sizeof(g_flightGlobalCountdownTimers));
		for (i = 0; i < 13; ++i) {
			for (p = 0; p < XWA_PLAYER_COUNT; ++p)
				((uint16_t*)&g_playerFlightTransientTimers[p])[i] = 0;
		}
	}
	for (p = 0; p < XWA_PLAYER_COUNT; ++p) {
		g_players[p].pendingActionTimer = 0;
		g_players[p].beamFireCooldownTimer = 0;
	}
	for (p = 0; p < XWA_PLAYER_COUNT; ++p) {
		PlayerData* pl = &g_players[p];
		pl->viewState.hudAimXSnapState = 0;
		pl->viewState.hudAimX = 0;
		pl->viewState.hudAimY = 0;
		pl->viewState.externalCameraActive = 0;
		pl->viewState.cameraDistance = 1024;
		pl->viewState.playerInputBlocked = 0;
		pl->viewState.transitionTimer = 0;
		pl->viewState.transitionDuration = 0;
		pl->viewState.cameraFocusObjIdx = g_players[p].objectIndex;
		pl->viewState.cameraPanDeltaX = 0;
		pl->viewState.cameraPanDeltaY = 0;
		pl->viewState.cameraPanDeltaZ = 0;
		pl->viewState.cameraPitchDelta = 0;
		pl->viewState.cameraYawDelta = 0;
		pl->viewState.cameraRollDelta = 0;
		pl->viewState.field_32 = 0;
	}
	for (p = 0; p < XWA_PLAYER_COUNT; ++p) {
		g_players[p].savedCraftSettingsRaw[10] = 0;
		Mission_SetActiveRegionObjectRanges(g_players[p].regionIndex);
		Hud_SetHudViewState(19, p);
		Player_UpdateHudViewForCameraFocus(p);
		if ((int)p == g_localPlayer)
			Hud_ResetFlightMessagePanes(0);
	}

	g_actionKey = 0; /* KEY_NONE */
	g_flightInitialTextureCacheFlushPending = 1;
	g_simStepScale = 15;
	g_elapsedTicks = 15;
	g_unusedFlightSimStepScaleWordMirror = 15;
	Time_GetFrameDelta();
	g_inputTimestamp = 0;
	g_readyMessageQueueCount = 0;
	{
		int messageIdx;
#ifdef XWA_MODERN
		for (messageIdx = 0; messageIdx < 64; ++messageIdx) {
			g_missionMessageTriggered[messageIdx] = 0;
			g_missionMessageDelayCountdown[messageIdx] = 0;
		}
#else
		int* messageDelay = g_missionMessageDelayCountdown;
		messageIdx = 0;
		do {
			g_missionMessageTriggered[messageIdx] = 0;
			*messageDelay = 0;
			++messageDelay;
			++messageIdx;
		} while (messageDelay < &g_missionGlobalUnitCraftCount[1]);
#endif
	}

	// Single-player cheat loadout flags.
	if ((!g_filmPlaybackMode || (uint16_t)g_filmVersion > 3) && g_flightPlayerCount == 1 &&
		g_pilotData.missionDirectoryId != 3) {
		if (g_pilotData.campaignMode) {
			if (g_gameConfig.tourInvulnerable)
				g_missionFlightGroups[g_objectTable[g_players[g_localPlayer].objectIndex].flightGroupIdx]
					.fg.status1 = 20;
			if (g_gameConfig.tourUnlimitedAmmo)
				g_missionFlightGroups[g_objectTable[g_players[g_localPlayer].objectIndex].flightGroupIdx]
					.fg.status2 = 21;
		} else {
			if (g_gameConfig.invulnerable)
				g_missionFlightGroups[g_objectTable[g_players[g_localPlayer].objectIndex].flightGroupIdx]
					.fg.status1 = 20;
			if (g_gameConfig.unlimitedAmmo)
				g_missionFlightGroups[g_objectTable[g_players[g_localPlayer].objectIndex].flightGroupIdx]
					.fg.status2 = 21;
		}
	}

	g_flightMissionEndPending = 0;
	g_maxConnectedPlayerCountThisMission = 0;
	g_missionFlightRuntimeState.globalPrimaryGoalStatus = 0;
	g_missionFlightRuntimeState.globalGoalStatusUnused = 0;
	g_missionFlightRuntimeState.globalBonusGoalStatus = 0;
	g_initialSpawnBindPlayerCraftSlots = 0;
	FlightSurface_Lock();
	Mfd_InitCommandMenuRuntimeState();
}

// FUNCTION: XWA 0x4187D0
// Per-second mission arrival scheduler. On the trigger-scan tick it evaluates
// each flight group's arrival triggers and starts (optionally randomized)
// arrival-delay timers, and for already-arrived groups either closes the
// remaining waves (departure / stop-arrival / team rules) or launches the next
// wave when object slots are free. On the delay-scan tick it counts down armed
// arrival-delay timers and spawns the first wave when they expire.
void Mission_UpdateFlightGroupArrivals(void) {
	if (!g_flightGlobalCountdownTimers[MISSION_ARRIVAL_TRIGGER_SCAN_TIMER_IDX]) {
		g_flightGlobalCountdownTimers[MISSION_ARRIVAL_TRIGGER_SCAN_TIMER_IDX] = MISSION_LOGIC_REFRESH_TICKS;
		g_currentFlightGroupIdx = 0;
		if ((int16_t)g_missionHeader.numFlightGroups > 0) {
			do {
				if (!g_missionFgStats[g_currentFlightGroupIdx].hasArrived &&
					!g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayPending) {
					// Not yet arrived: evaluate arrival triggers and arm the timer.
					if ((g_missionFgStats[g_currentFlightGroupIdx].arrivalEnabled ||
						 g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx != -1) &&
						g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[0]) {
						char combined = Mission_EvaluateTriggerPair(
							&g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrival[0], 1);
						char e1 = Mission_EvaluateTriggerPair(
							&g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrival[1], 1);
						if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivals12OrArrivals34 == 1)
							combined |= e1;
						else
							combined &= e1;
						if ((combined & 1) &&
							(g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx != -1 ||
							 !g_missionFlightGroups[g_currentFlightGroupIdx].fg.arriveOnlyIfHuman)) {
							int16_t baseDelay = (int16_t)(g_missionFlightGroups[g_currentFlightGroupIdx]
															  .fg.arrivalDelaySeconds +
														  60 * g_missionFlightGroups[g_currentFlightGroupIdx]
																   .fg.arrivalDelayMinutes);
							g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayTimer = baseDelay;
							{
								uint16_t randRange =
									(uint16_t)(g_missionFlightGroups[g_currentFlightGroupIdx]
												   .fg.arrivalRandDelaySeconds +
											   60 * g_missionFlightGroups[g_currentFlightGroupIdx]
														.fg.arrivalRandDelayMinutes);
								if (g_missionRandomVariationEnabled) {
									int16_t jitter;
									if (!randRange)
										jitter = 0;
									else
										jitter = (int16_t)((uint16_t)GameRand() % (int)(uint16_t)randRange);
									g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayTimer += jitter;
									g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayPending = 1;
								} else {
									g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayTimer =
										(int16_t)(baseDelay + (randRange >> 1));
									g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayPending = 1;
								}
							}
						}
					}
					continue;
				}

				// Already arrived (or delay pending): consider launching a new wave.
				if (!(g_missionFgStats[g_currentFlightGroupIdx].arrivalEnabled &&
					  g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining &&
					  g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[0] &&
					  g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx == -1))
					continue;
				if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.playerNumber) {
					// Player teams stop arriving once their goals are decided.
					uint8_t primary =
						g_missionFlightRuntimeState
							.teamGoalStatus[g_missionFlightGroups[g_currentFlightGroupIdx].fg.team]
										   [TEAM_GOAL_PRIMARY];
					if (primary == 1) {
						uint8_t secondary =
							g_missionFlightRuntimeState
								.teamGoalStatus[g_missionFlightGroups[g_currentFlightGroupIdx].fg.team]
											   [TEAM_GOAL_SECONDARY];
						if (secondary == primary)
							continue;
						if (g_missionFlightRuntimeState
									.teamGoalStatus[g_missionFlightGroups[g_currentFlightGroupIdx].fg.team]
												   [TEAM_GOAL_PRIMARY] == primary &&
							secondary == 2)
							continue;
					}
					if (primary == 2 &&
						g_missionFlightRuntimeState
								.teamGoalStatus[g_missionFlightGroups[g_currentFlightGroupIdx].fg.team]
											   [TEAM_GOAL_SECONDARY] == 1)
						continue;
				}

				{
					char hasNoInstance = 1;
					int craftObjType = g_objectTypeTables.craftTypeToObjectType
										   [g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType];
					if (!(g_modelTypeTable[craftObjType].flags & 0x80)) {
						// Normal craft: a live, non-departing craft of this FG counts.
						unsigned int slot;
						for (slot = 0; slot < g_objectTableSlotCount; ++slot) {
							ObjectRecord* o = &g_objectTable[slot];
							if (o->objectType) {
								MobileObject* mobj = o->mobj;
								if (mobj) {
									if (mobj->pCraft && !mobj->state &&
										o->flightGroupIdx == g_currentFlightGroupIdx) {
										uint8_t kind;
										g_curCraft = mobj->pCraft;
										kind = g_curCraft->objectKind;
										if (kind != 3 && kind != 4) {
											hasNoInstance = 0;
											break;
										}
									}
								}
							}
						}
					} else {
						// Big/static craft: an objectless placeholder slot for this FG
						// counts as still present.
						unsigned int slot;
						for (slot = 0; slot < g_objectTableSlotCount; ++slot) {
							ObjectRecord* o = &g_objectTable[slot];
							if (o->objectType && !o->mobj && g_currentFlightGroupIdx == o->flightGroupIdx) {
								hasNoInstance = 0;
								break;
							}
						}
					}
					if (hasNoInstance) {
						uint8_t stopArrivingWhen;
						int stopArriving = 0;
						if ((g_missionFlightGroups[g_currentFlightGroupIdx]
								 .fg.departure.triggers[0]
								 .condition ||
							 g_missionFlightGroups[g_currentFlightGroupIdx]
								 .fg.departure.triggers[1]
								 .condition) &&
							(Mission_EvaluateTriggerPair(
								 &g_missionFlightGroups[g_currentFlightGroupIdx].fg.departure, 0) &
							 1) != 0)
							stopArriving = 1;
						stopArrivingWhen = g_missionFlightGroups[g_currentFlightGroupIdx].fg.stopArrivingWhen;
						if (stopArrivingWhen == 1 &&
							g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[16])
							stopArriving = 1;
						if (stopArrivingWhen == 2 &&
							g_missionFlightRuntimeState
									.teamGoalStatus[g_missionFlightGroups[g_currentFlightGroupIdx].fg.team]
												   [TEAM_GOAL_PRIMARY] == 1)
							stopArriving = 1;
						if (stopArrivingWhen == 3 &&
							(g_missionFlightRuntimeState
									 .teamGoalStatus[g_missionFlightGroups[g_currentFlightGroupIdx].fg.team]
													[TEAM_GOAL_PRIMARY] == 2 ||
							 g_missionFlightRuntimeState
									 .teamGoalStatus[g_missionFlightGroups[g_currentFlightGroupIdx].fg.team]
													[TEAM_GOAL_SECONDARY] == 1))
							stopArriving = 1;

						if (stopArriving) {
							uint8_t departMethod;
							int notArrived = g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[0] -
											 g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[1];
							int notArrivedSpecial =
								g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[0] -
								g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[1];
							g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[1] += notArrived;
							g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[22] += notArrived;
							g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[22] +=
								notArrivedSpecial;
							g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[9] += notArrived;
							g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[9] +=
								notArrivedSpecial;
							g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[15] += notArrived;
							g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[15] +=
								notArrivedSpecial;
							g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[7] += notArrived;
							g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[7] +=
								notArrivedSpecial;
							g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[5] += notArrived;
							g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[5] +=
								notArrivedSpecial;
							g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[11] += notArrived;
							g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[11] +=
								notArrivedSpecial;
							departMethod = g_missionFlightGroups[g_currentFlightGroupIdx].fg.departMethod;
							g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining = 0;
							if ((departMethod ||
								 g_missionFlightGroups[g_currentFlightGroupIdx].fg.alternateMothershipUsed) &&
								(departMethod != 2 ||
								 g_missionFlightGroups[g_currentFlightGroupIdx].fg.alternateMothershipUsed))
								g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[18] += notArrived;
							else
								g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[17] += notArrived;
						} else {
							int spawnRegion;
							int16_t isBig;
							if (g_missionFlightGroups[g_currentFlightGroupIdx]
									.fg.missionPoints[XWA_FG_POINT_START_2]
									.enabled)
								spawnRegion = g_missionFlightGroups[g_currentFlightGroupIdx]
												  .fg.missionPointRegions[XWA_FG_POINT_START_2];
							else
								spawnRegion = g_missionFlightGroups[g_currentFlightGroupIdx]
												  .fg.missionPointRegions[XWA_FG_POINT_START_1];
							Mission_SetActiveRegionObjectRanges(spawnRegion);
							isBig = (int16_t)g_modelTypeTable
										[g_objectTypeTables.craftTypeToObjectType
											 [g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType]]
											.flags;
							isBig &= 0x80;
							if (isBig || g_activeRegionObjectSlotStart < g_activeRegionCraftObjectSlotEnd) {
								unsigned int slot = g_activeRegionObjectSlotStart;
								int free = 0;
								if (!isBig) {
									while (g_objectTable[slot].objectType ||
										   ++free <
											   (unsigned int)g_missionFlightGroups[g_currentFlightGroupIdx]
												   .fg.numberOfCraft) {
										++slot;
										if (slot >= (unsigned int)g_activeRegionCraftObjectSlotEnd)
											goto next_trigger_flight_group;
									}
								}
								if (!isBig)
									Mission_SpawnFlightGroupWaveCraft(0xFFFFu);
								else
									Mission_SpawnFlightGroupStaticObjects(0xFFFFu);
								{
									uint8_t wavesRemaining =
										g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining;
									if (wavesRemaining &&
										(!g_missionFlightGroups[g_currentFlightGroupIdx].fg.playerNumber ||
										 g_playerFlightGroupWaveMode != 2))
										g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining =
											wavesRemaining - 1;
								}
							}
						}
					}
				}
			next_trigger_flight_group:;
			} while (++g_currentFlightGroupIdx < (int16_t)g_missionHeader.numFlightGroups);
		}
	}

	// Delay-scan tick: count down armed arrival timers and spawn first waves.
	if (!g_flightGlobalCountdownTimers[MISSION_ARRIVAL_DELAY_SCAN_TIMER_IDX]) {
		g_flightGlobalCountdownTimers[MISSION_ARRIVAL_DELAY_SCAN_TIMER_IDX] = MISSION_LOGIC_REFRESH_TICKS;
		for (g_currentFlightGroupIdx = 0; g_currentFlightGroupIdx < (int16_t)g_missionHeader.numFlightGroups;
			 ++g_currentFlightGroupIdx) {
			int16_t timer;
			if (g_missionFgStats[g_currentFlightGroupIdx].hasArrived ||
				g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayPending != 1)
				continue;
			timer = g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayTimer;
			if (!timer) {
				Mission_SetActiveRegionObjectRanges(g_missionFlightGroups[g_currentFlightGroupIdx]
														.fg.missionPointRegions[XWA_FG_POINT_START_1]);
				{
					int craftObjType = g_objectTypeTables.craftTypeToObjectType
										   [g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType];
					int16_t isBig = (int16_t)(g_modelTypeTable[craftObjType].flags & 0x80);
					if (!isBig) {
						unsigned int slot = g_activeRegionObjectSlotStart;
						int free = 0;
						if (slot >= (unsigned int)g_activeRegionCraftObjectSlotEnd)
							continue;
						while (g_objectTable[slot].objectType ||
							   ++free < (unsigned int)g_missionFlightGroups[g_currentFlightGroupIdx]
											.fg.numberOfCraft) {
							++slot;
							if (slot >= (unsigned int)g_activeRegionCraftObjectSlotEnd)
								goto next_delay_flight_group;
						}
					}

					g_missionFgStats[g_currentFlightGroupIdx].hasArrived = 1;
					if (!isBig) {
						g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining =
							g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfWaves;
						Mission_SpawnFlightGroupWaveCraft(0xFFFFu);
					} else {
						if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
							g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH)
							g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining =
								g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfWaves;
						else
							g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining = 0;
						Mission_SpawnFlightGroupStaticObjects(0xFFFFu);
					}
				}
			} else {
				g_missionFgStats[g_currentFlightGroupIdx].arrivalDelayTimer = timer - 1;
			}
		next_delay_flight_group:;
		}
	}
}

// Resolves the model genus id for a flight group's craft type, matching the
// inline g_modelTypeTable[g_objectTypeTables.craftTypeToObjectType[craftType]].genusId lookup
// the original performs in several places.
static __inline ModelGenusId Mission_FlightGroupCraftGenus(uint16_t fgIdx) {
	uint16_t objType =
		(uint16_t)g_objectTypeTables.craftTypeToObjectType[g_missionFlightGroups[fgIdx].fg.craftType];
	return g_modelTypeTable[objType].genusId;
}

// FUNCTION: XWA 0x418720
// Mark the current flight group (g_currentFlightGroupIdx) as arrived and dispatch
// its initial spawn path: static/scenery-style groups (model flag 0x80) use
// Mission_SpawnFlightGroupStaticObjects, normal mobile craft use
// Mission_SpawnFlightGroupWaveCraft. instanceFilter selects one craft ordinal or
// 0xFFFF for all.
short Mission_StartFlightGroupArrival(uint16_t instanceFilter) {
	g_missionFgStats[g_currentFlightGroupIdx].hasArrived = 1;
	if ((g_modelTypeTable[(uint16_t)g_objectTypeTables.craftTypeToObjectType
							  [g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType]]
			 .flags &
		 0x80) == 0) {
		g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining =
			g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfWaves;
		Mission_SpawnFlightGroupWaveCraft(instanceFilter);
		return 1;
	}
	if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
		g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH)
		g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining =
			g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfWaves;
	else
		g_missionFgStats[g_currentFlightGroupIdx].wavesRemaining = 0;
	Mission_SpawnFlightGroupStaticObjects(instanceFilter);
	return 1;
}

// FUNCTION: XWA 0x4195E0
// Materializes the normal craft objects for the current flight group
// (g_currentFlightGroupIdx). instanceFilter == 0xFFFF spawns the whole current
// wave; any other value spawns the single craft ordinal in instanceFilter.
// Resolves arrival/start/mothership world positions, heading and pitch,
// formation and quickstart/skirmish player placement, then calls
// Mission_InitFlightGroupObjectSlot for each craft plus any carried companion
// or linked (connector-rod) segment objects. Returns nonzero when spawn work
// was performed.
short Mission_SpawnFlightGroupWaveCraft(uint16_t instanceFilter) {
	uint8_t missionRunning;
	uint8_t arrivalMethod;
	int didSpawn;

	// "mission clock is running" flag: nonzero once any elapsed time accrued.
	missionRunning = (uint8_t)(g_missionElapsedClock.hours | g_missionElapsedClock.minutes |
							   g_missionElapsedClock.seconds);
	arrivalMethod = g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalMethod;

	g_spawnOutOfHyperspaceFlag = 0;
	g_spawnFromMothershipFlag = 0;
	g_spawnUseExactPosition = 0;
	g_spawnLinkedObjectFlag = 0;

	if (!arrivalMethod || !missionRunning || instanceFilter != 0xFFFF || arrivalMethod == 2) {
		// --- Start-point / arrival positioning ------------------------------
		g_spawnRegionIdx =
			g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPointRegions[XWA_FG_POINT_START_1];
		Mission_ResolveObjectOrMissionPointWorldLoc(0x8000u, g_currentFlightGroupIdx, g_spawnRegionIdx, 0);
		g_spawnWorldX = worldlocx;
		g_spawnWorldY = worldlocy;
		g_spawnWorldZ = worldlocz;

		if (missionRunning) {
			if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
				g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) {
				uint8_t playerNumber = g_missionFlightGroups[g_currentFlightGroupIdx].fg.playerNumber;
				if (playerNumber) {
					uint8_t i;
					int16_t numFg = (int16_t)g_missionHeader.numFlightGroups;
					uint8_t teamPlayerFgCount[10];
					memset(teamPlayerFgCount, 0, sizeof(teamPlayerFgCount));
					for (i = 0; i < numFg; ++i) {
						if (g_missionFlightGroups[i].fg.playerNumber)
							++teamPlayerFgCount[g_missionFlightGroups[i].fg.team];
					}
					// Single human flight group on team 0: place opposite the
					// rotating opponent player number for this wave.
					if (teamPlayerFgCount[0] == 1) {
						int numberOfCraft = g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft;
						int wave = (uint8_t)g_missionFgStats[g_currentFlightGroupIdx].spawnedCraftCount /
								   numberOfCraft;
						int opp = (playerNumber & 1) != 0 ? (playerNumber + 3 * (wave - 3)) & 7
														  : (playerNumber - 3 * wave - 1) & 7;
						uint8_t targetPlayerNumber = (uint8_t)(opp + 1);
						if (numFg > 0) {
							uint8_t scan = 0;
							int found = 0;
							while (scan < (uint8_t)numFg) {
								if (g_missionFlightGroups[scan].fg.playerNumber == targetPlayerNumber) {
									found = 1;
									break;
								}
								++scan;
							}
							if (found) {
								g_spawnRegionIdx =
									g_missionFlightGroups[scan].fg.missionPointRegions[XWA_FG_POINT_START_1];
								Mission_ResolveObjectOrMissionPointWorldLoc(0x8000u, scan, g_spawnRegionIdx,
																			0);
								g_spawnWorldX = worldlocx;
								g_spawnWorldY = worldlocy;
								g_spawnWorldZ = worldlocz;
							}
						}
					}
				}
				// Random scatter around the start point for human-owned craft.
				if (g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx != -1) {
					int j = GameRand() & 0x7FFF;
					g_spawnWorldX = (GameRand() & 1) ? g_spawnWorldX + j : g_spawnWorldX - j;
					j = GameRand() & 0x7FFF;
					g_spawnWorldY = (GameRand() & 1) ? g_spawnWorldY + j : g_spawnWorldY - j;
				}
			} else if (g_missionFormatVersion >= 14) {
				// Newer missions can carry a dedicated second player start point.
				if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.playerNumber &&
					g_missionFlightGroups[g_currentFlightGroupIdx]
						.fg.missionPoints[XWA_FG_POINT_START_2]
						.enabled) {
					g_spawnRegionIdx = g_missionFlightGroups[g_currentFlightGroupIdx]
										   .fg.missionPointRegions[XWA_FG_POINT_START_2];
					Mission_ResolveObjectOrMissionPointWorldLoc(0x8001u, g_currentFlightGroupIdx,
																g_spawnRegionIdx, 0);
					g_spawnWorldX = worldlocx;
					g_spawnWorldY = worldlocy;
					g_spawnWorldZ = worldlocz;
				}
			}
		}

		// Heading/pitch: aim at the first order waypoint if enabled, else use
		// the flight group's authored yaw/pitch (clamping a rear-facing pitch).
		if (g_missionFlightGroups[g_currentFlightGroupIdx]
				.fg.orders[4 * g_spawnRegionIdx]
				.waypoints[0]
				.enabled) {
			Mission_ResolveObjectOrMissionPointWorldLoc(0x8004u, g_currentFlightGroupIdx, g_spawnRegionIdx,
														0);
			trig2_ctop(worldlocx - g_spawnWorldX, worldlocy - g_spawnWorldY, worldlocz - g_spawnWorldZ);
			g_spawnYaw = trig2_xyangle;
			g_spawnPitch = targetPitch;
		} else {
			Q16Angle pitchAngle;
			Q16Angle yawAngle = (Q16Angle)(g_missionFlightGroups[g_currentFlightGroupIdx].fg.yaw << 8);
			trig2_xyangle = yawAngle;
			g_spawnYaw = yawAngle;
			pitchAngle = (Q16Angle)(g_missionFlightGroups[g_currentFlightGroupIdx].fg.pitch << 8);
			targetPitch = pitchAngle;
			g_spawnPitch = pitchAngle;
			if (pitchAngle >= 0x8000u) {
				targetPitch = 0x4000;
				g_spawnPitch = 0x4000;
			}
		}

		// Arrival method 2: orient toward the mothership and, if it lives in a
		// different active region, switch to that region (after confirming the
		// region has room for the whole wave).
		if (missionRunning && g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalMethod == 2) {
			int16_t arrivalMothership = g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalMothership;
			unsigned int region;
			for (region = 0; (unsigned int)region < (unsigned int)g_activeMissionRegionCount; ++region) {
				unsigned int regionBase = region * (g_regionObjectSlotEnd / g_missionRegionCount);
				unsigned int regionEnd = regionBase + g_craftObjectSlotsTotal / g_missionRegionCount;
				uint16_t slot = (uint16_t)regionBase;
				if (slot < regionEnd) {
					for (;;) {
						ObjectRecord* o = &g_objectTable[slot];
						if (o->objectType && o->flightGroupIdx == arrivalMothership)
							break;
						slot = (uint16_t)(slot + 1);
						if (slot >= regionEnd)
							goto next_method2_region;
					}
					trig2_ctop(g_objectTable[slot].world_x - g_spawnWorldX,
							   g_objectTable[slot].world_y - g_spawnWorldY,
							   g_objectTable[slot].world_z - g_spawnWorldZ);
					g_spawnYaw = trig2_xyangle;
					g_spawnPitch = targetPitch;
					if (region != g_spawnRegionIdx) {
						uint16_t craftObjType;
						g_spawnRegionIdx = (uint8_t)region;
						Mission_SetActiveRegionObjectRanges((uint8_t)region);
						craftObjType = (uint16_t)g_objectTypeTables.craftTypeToObjectType
										   [g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType];
						if (!(g_modelTypeTable[craftObjType].flags & 0x80)) {
							unsigned int craftSlot = g_activeRegionObjectSlotStart;
							unsigned int free = 0;
							if (g_activeRegionObjectSlotStart >= g_activeRegionCraftObjectSlotEnd)
								return 0;
							while (g_objectTable[craftSlot].objectType ||
								   ++free < (unsigned int)g_missionFlightGroups[g_currentFlightGroupIdx]
												.fg.numberOfCraft) {
								if (++craftSlot >= g_activeRegionCraftObjectSlotEnd)
									return 0;
							}
						}
					}
				}
			next_method2_region:;
			}
		}

		// Non-mothership-launched, non-player craft drop in along a reversed
		// heading 8 units out, marked as arriving from hyperspace.
		if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalMethod != 1 && missionRunning &&
			instanceFilter == 0xFFFF) {
			int playerOwnerIdx = g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx;
			ModelGenusId genus = Mission_FlightGroupCraftGenus(g_currentFlightGroupIdx);
			g_spawnGenusId = genus;
			if (playerOwnerIdx == -1 && genus != GENUS_SatelliteBuoy) {
				trig2_xyangle += 0x8000;
				targetPitch = 0x8000 - targetPitch;
				trig2_movexyz(0xFFFFu, trig2_xyangle, targetPitch);
#ifdef XWA_MODERN
				trig2_xmovedist *= 8;
				trig2_ymovedist *= 8;
				trig2_zmovedist *= 8;
#else
				trig2_xmovedist <<= 3;
				trig2_ymovedist <<= 3;
				trig2_zmovedist <<= 3;
#endif
				g_spawnWorldX += trig2_xmovedist;
				g_spawnWorldY += trig2_ymovedist;
				g_spawnWorldZ += trig2_zmovedist;
				g_spawnOutOfHyperspaceFlag = 1;
			}
		}

		g_spawnFormation = g_missionFlightGroups[g_currentFlightGroupIdx].fg.formation;
		g_spawnFormationSpacing = g_missionFlightGroups[g_currentFlightGroupIdx].fg.formationSpacing;
	} else {
		ModelIndex modelIndex;
		// --- Launch from an existing mothership -----------------------------
		int16_t motherFg = g_missionFlightGroups[g_currentFlightGroupIdx].fg.arrivalMothership;
		int foundMother = 0;
		uint16_t motherObjIdx = 0;
		if (g_activeMissionRegionCount) {
			unsigned int regionStride = g_regionObjectSlotEnd / g_missionRegionCount;
			unsigned int craftPerRegion = g_craftObjectSlotsTotal / g_missionRegionCount;
			unsigned int regionBase = 0;
			unsigned int regionEnd = craftPerRegion;
			unsigned int regionsRemaining = (unsigned int)g_activeMissionRegionCount;
			if (regionsRemaining > 0) {
				do {
					uint16_t regionCursor = (uint16_t)regionBase;
					if (regionBase < regionEnd) {
						for (;;) {
							if (g_objectTable[regionCursor].objectType != OBJ_None) {
								g_curCraft = g_objectTable[regionCursor].mobj->pCraft;
								if (g_objectTable[regionCursor].flightGroupIdx == motherFg &&
									g_curCraft->leader_obj_idx == -1)
									break;
							}
							regionCursor = (uint16_t)(regionCursor + 1);
							if (regionCursor >= regionEnd)
								goto next_mother_region;
						}
						foundMother = 1;
						motherObjIdx = regionCursor;
					}
				next_mother_region:
					regionBase += regionStride;
					regionEnd += regionStride;
				} while (--regionsRemaining);
			}
		}
		if (!foundMother)
			return 0;

		g_curCraft = g_objectTable[motherObjIdx].mobj->pCraft;
		modelIndex = g_curCraft->modelIndex;
		// Launch position = mothership hangar attach point (mesh slots 5..7).
		pai_RotateLocalVectorToWorldScratch(
			&g_objectTable[motherObjIdx], g_modelDefs[modelIndex].meshAttachData[5],
			g_modelDefs[modelIndex].meshAttachData[6], g_modelDefs[modelIndex].meshAttachData[7]);
		g_spawnWorldX = g_rotatedX + g_objectTable[motherObjIdx].world_x;
		g_spawnWorldY = g_rotatedY + g_objectTable[motherObjIdx].world_y;
		g_spawnWorldZ = g_rotatedZ + g_objectTable[motherObjIdx].world_z;
		g_spawnRegionIdx = g_objectTable[motherObjIdx].regionIdx;
		Mission_SetActiveRegionObjectRanges(g_spawnRegionIdx);
		// Launch heading = toward mothership launch-vector attach point (8..10).
		pai_RotateLocalVectorToWorldScratch(
			&g_objectTable[motherObjIdx], g_modelDefs[modelIndex].meshAttachData[8],
			g_modelDefs[modelIndex].meshAttachData[9], g_modelDefs[modelIndex].meshAttachData[10]);
		worldlocx = g_rotatedX + g_objectTable[motherObjIdx].world_x;
		worldlocy = g_rotatedY + g_objectTable[motherObjIdx].world_y;
		worldlocz = g_rotatedZ + g_objectTable[motherObjIdx].world_z;
		trig2_ctop(worldlocx - g_spawnWorldX, worldlocy - g_spawnWorldY, worldlocz - g_spawnWorldZ);
		g_spawnYaw = trig2_xyangle;
		g_spawnPitch = targetPitch;
		g_spawnFromMothershipFlag = 1;
		g_spawnFormationSpacing = 0;
		g_spawnFormation = g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft > 3u ? 6 : 0;
	}

	// --- Common spawn: populate g_spawn* context, then create the craft -----
	g_spawnIff = g_missionFlightGroups[g_currentFlightGroupIdx].fg.iff;
	g_spawnObjectKind = 0;
	g_spawnTeamId = g_missionFlightGroups[g_currentFlightGroupIdx].fg.team;
	g_spawnStatus1 = g_missionFlightGroups[g_currentFlightGroupIdx].fg.status1;
	g_spawnStatus2 = g_missionFlightGroups[g_currentFlightGroupIdx].fg.status2;
	g_spawnGenusId = Mission_FlightGroupCraftGenus(g_currentFlightGroupIdx);
	g_spawnGroupAI = g_missionFlightGroups[g_currentFlightGroupIdx].fg.groupAI;

	didSpawn = 0;
	if (instanceFilter == 0xFFFF) {
		uint8_t numCraft = g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft;
		uint16_t inited = 0;
		int spawnedCount = 0;
		g_spawnLeaderObjIdx = -1;
		g_spawnCraftOrdinal = 0;
		if (numCraft) {
			do {
				if (g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[1] <
					g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[0]) {
					uint8_t savedStatus1;
					ModelGenusId savedGenus;
					int compFg;
					inited = Mission_InitFlightGroupObjectSlot(0xFFFFu, (ObjectIndex)0xFFFF);
					if ((uint16_t)inited == 0xFFFF)
						return 0;
					++spawnedCount;
					++g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[1];
					if (g_spawnCraftOrdinal ==
						g_missionFlightGroups[g_currentFlightGroupIdx].fg.specialCargoCraft)
						++g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[1];

					// Carried companion craft: any flight group whose first
					// arrival trigger is (cond 51, FLIGHT_GROUP == g_currentFlightGroupIdx) and that
					// still has instances left is spawned and attached as cargo.
					savedGenus = g_spawnGenusId;
					savedStatus1 = g_spawnStatus1;
					for (compFg = 0; compFg < (int16_t)g_missionHeader.numFlightGroups; ++compFg) {
						XwaTrigger* trig = &g_missionFlightGroups[compFg].fg.arrival[0].triggers[0];
						if (g_missionFgStats[compFg].outcomeCount[1] <
								g_missionFgStats[compFg].outcomeCount[0] &&
							trig->condition == 51 && trig->variableType == 1 &&
							trig->variable == g_currentFlightGroupIdx) {
							uint16_t comp;
							g_spawnSavedGenusId = savedGenus;
							g_spawnSavedStatus2 = g_spawnStatus2;
							g_spawnSavedFlightGroupIdx = g_currentFlightGroupIdx;
							g_spawnSavedStatus1 = savedStatus1;
							g_spawnUseExactPosition = 1;
							g_unusedSpawnCarrierObjIdxLatch = inited;
							g_currentFlightGroupIdx = (uint16_t)compFg;
							g_spawnGenusId = Mission_FlightGroupCraftGenus((uint16_t)compFg);
							g_spawnStatus1 = g_missionFlightGroups[compFg].fg.status1;
							g_spawnStatus2 = g_missionFlightGroups[compFg].fg.status2;
							comp = Mission_InitFlightGroupObjectSlot(0xFFFFu, (ObjectIndex)0xFFFF);
							if ((uint16_t)comp != 0xFFFF) {
								++g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[1];
								if (g_spawnCraftOrdinal ==
									g_missionFlightGroups[g_currentFlightGroupIdx].fg.specialCargoCraft)
									++g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[1];
								g_objectTable[comp].mobj->pCraft->carrierObjIdx = inited;
								g_objectTable[inited].mobj->pCraft->carriedObjectIndex = comp;
							}
							savedStatus1 = g_spawnSavedStatus1;
							g_spawnStatus2 = g_spawnSavedStatus2;
							savedGenus = g_spawnSavedGenusId;
							g_spawnStatus1 = g_spawnSavedStatus1;
							g_spawnGenusId = g_spawnSavedGenusId;
							g_currentFlightGroupIdx = g_spawnSavedFlightGroupIdx;
							g_spawnUseExactPosition = 0;
						}
					}

					// Linked (connector-rod) ships: front cabin/engine pieces
					// chain mid segments from flight groups whose first arrival
					// trigger is (cond 53, FLIGHT_GROUP == g_currentFlightGroupIdx), capped with a
					// trailing rod and a matching back piece.
					if (g_objectTable[inited].objectType == OBJ_CrewCabinFront ||
						g_objectTable[inited].objectType == OBJ_EngineFront) {
						uint16_t back;
						uint8_t tailRodSeq;
						CraftData* tailRodCraft;
						uint16_t tailRod;
						uint8_t linkSeq;
						uint16_t headType;
						uint16_t headFg;
						int segFg;
						uint8_t m;
						g_spawnSavedGenusId = savedGenus;
						headFg = g_currentFlightGroupIdx;
						g_spawnSavedStatus1 = savedStatus1;
						g_spawnSavedFlightGroupIdx = g_currentFlightGroupIdx;
						g_spawnSavedStatus2 = g_spawnStatus2;
						g_spawnUseExactPosition = 1;
						headType = (ObjectTypeId)g_objectTable[inited].objectType;
						linkSeq = 0;
						for (segFg = 0; segFg < (int16_t)g_missionHeader.numFlightGroups; ++segFg) {
							XwaTrigger* trig = &g_missionFlightGroups[segFg].fg.arrival[0].triggers[0];
							if (trig->condition != 53 || trig->variableType != 1 || trig->variable != headFg)
								continue;
							for (m = 0; m < g_missionFlightGroups[segFg].fg.numberOfCraft; ++m) {
								uint16_t seg;
								uint8_t rodSeq;
								CraftData* rodCraft;
								uint16_t rod;
								if (g_missionFgStats[segFg].outcomeCount[1] >=
									g_missionFgStats[segFg].outcomeCount[0])
									continue;
								// Connector rod spawned with the head FG context.
								g_currentFlightGroupIdx = headFg;
								g_spawnLinkedObjectFlag = 1;
								g_spawnGenusId = Mission_FlightGroupCraftGenus(headFg);
								g_spawnStatus1 = g_missionFlightGroups[headFg].fg.status1;
								g_spawnStatus2 = g_missionFlightGroups[headFg].fg.status2;
								rod = Mission_InitFlightGroupObjectSlot(0xFFFFu, (ObjectIndex)0xFFFF);
								if ((uint16_t)rod == 0xFFFF)
									goto link_done;
								g_objectTable[rod].objectType = OBJ_ConnectorRod;
								rodCraft = g_objectTable[rod].mobj->pCraft;
								rodCraft->nextLinkObjectIdx = inited;
								rodSeq = linkSeq + 1;
								rodCraft->modelIndex = GetModelIndexFromType(OBJ_ConnectorRod);
								rodCraft->linkSequenceIndex = (uint16_t)rodSeq;
								g_objectTable[inited].mobj->pCraft->linkedPrevObjectIdx = rod;
								// Mid segment spawned with the segment FG context.
								g_currentFlightGroupIdx = (uint16_t)segFg;
								g_spawnLinkedObjectFlag = 0;
								g_spawnGenusId = Mission_FlightGroupCraftGenus((uint16_t)segFg);
								g_spawnStatus1 = g_missionFlightGroups[segFg].fg.status1;
								g_spawnStatus2 = g_missionFlightGroups[segFg].fg.status2;
								seg = Mission_InitFlightGroupObjectSlot(0xFFFFu, (ObjectIndex)0xFFFF);
								if ((uint16_t)seg == 0xFFFF)
									goto link_done;
								++g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[1];
								if (g_spawnCraftOrdinal ==
									g_missionFlightGroups[g_currentFlightGroupIdx].fg.specialCargoCraft)
									++g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[1];
								linkSeq = rodSeq + 1;
								g_objectTable[seg].mobj->pCraft->nextLinkObjectIdx = rod;
								inited = seg;
								g_objectTable[seg].mobj->pCraft->linkSequenceIndex = (uint16_t)linkSeq;
								g_objectTable[rod].mobj->pCraft->linkedPrevObjectIdx = seg;
								headFg = g_spawnSavedFlightGroupIdx;
							}
						}
						// Trailing connector rod (head FG context).
						g_currentFlightGroupIdx = g_spawnSavedFlightGroupIdx;
						g_spawnLinkedObjectFlag = 1;
						g_spawnGenusId = Mission_FlightGroupCraftGenus(g_spawnSavedFlightGroupIdx);
						g_spawnStatus1 = g_missionFlightGroups[g_spawnSavedFlightGroupIdx].fg.status1;
						g_spawnStatus2 = g_missionFlightGroups[g_spawnSavedFlightGroupIdx].fg.status2;
						tailRod = Mission_InitFlightGroupObjectSlot(0xFFFFu, (ObjectIndex)0xFFFF);
						if ((uint16_t)tailRod == 0xFFFF)
							goto link_done;
						g_objectTable[tailRod].objectType = OBJ_ConnectorRod;
						tailRodCraft = g_objectTable[tailRod].mobj->pCraft;
						tailRodSeq = linkSeq + 1;
						tailRodCraft->nextLinkObjectIdx = inited;
						tailRodCraft->modelIndex = GetModelIndexFromType(OBJ_ConnectorRod);
						tailRodCraft->linkSequenceIndex = (uint16_t)tailRodSeq;
						g_objectTable[inited].mobj->pCraft->linkedPrevObjectIdx = tailRod;
						// Matching back piece spawned from the saved head FG.
						g_currentFlightGroupIdx = g_spawnSavedFlightGroupIdx;
						g_spawnGenusId = Mission_FlightGroupCraftGenus(g_spawnSavedFlightGroupIdx);
						g_spawnStatus1 = g_missionFlightGroups[g_spawnSavedFlightGroupIdx].fg.status1;
						g_spawnStatus2 = g_missionFlightGroups[g_spawnSavedFlightGroupIdx].fg.status2;
						back = Mission_InitFlightGroupObjectSlot(0xFFFFu, (ObjectIndex)0xFFFF);
						if ((uint16_t)back != 0xFFFF) {
							CraftData* backCraft;
							uint16_t backType;
							g_objectTable[back].objectType =
								(headType != OBJ_CrewCabinFront) ? OBJ_CrewCabinBack : OBJ_EngineBack;
							backType = (headType != OBJ_CrewCabinFront) ? OBJ_CrewCabinBack : OBJ_EngineBack;
							backCraft = g_objectTable[back].mobj->pCraft;
							backCraft->nextLinkObjectIdx = tailRod;
							backCraft->modelIndex = GetModelIndexFromType(backType);
							backCraft->linkSequenceIndex = (uint8_t)(tailRodSeq + 1);
							g_objectTable[tailRod].mobj->pCraft->linkedPrevObjectIdx = back;
							g_spawnStatus1 = g_spawnSavedStatus1;
							g_spawnStatus2 = g_spawnSavedStatus2;
							g_spawnGenusId = g_spawnSavedGenusId;
							g_currentFlightGroupIdx = g_spawnSavedFlightGroupIdx;
							g_spawnUseExactPosition = 0;
							g_spawnLinkedObjectFlag = 0;
						}
					link_done:;
					}
				}
				++g_spawnCraftOrdinal;
			} while (g_spawnCraftOrdinal < g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft);
			didSpawn = spawnedCount;
		}
		if (g_spawnOutOfHyperspaceFlag)
			Music_TriggerOutOfHyperspaceSequenceForObject((uint16_t)inited);
	} else if (g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[1] <
			   g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[0]) {
		g_spawnCraftOrdinal = instanceFilter;
		if ((uint16_t)Mission_InitFlightGroupObjectSlot(0xFFFFu, (ObjectIndex)0xFFFF) == 0xFFFF)
			return 0;
		didSpawn = 1;
		++g_missionFgStats[g_currentFlightGroupIdx].outcomeCount[1];
		if (g_spawnCraftOrdinal == g_missionFlightGroups[g_currentFlightGroupIdx].fg.specialCargoCraft)
			++g_missionFgStats[g_currentFlightGroupIdx].specialCargoOutcome[1];
	}

	// Announce a named flight group's arrival in the local player's region.
	if (missionRunning && instanceFilter == 0xFFFF && didSpawn &&
		(uint16_t)GetModelIndexFromType(g_spawnObjectType) != 0xFFFF &&
		g_modelDefs[(uint16_t)GetModelIndexFromType(g_spawnObjectType)].nameAlt &&
		g_spawnRegionIdx == g_players[g_localPlayer].regionIndex) {
		msg_reportfgcreation(g_currentFlightGroupIdx, GetModelIndexFromType(g_spawnObjectType));
	}
	return 1;
}

// FUNCTION: XWA 0x41A960
// Initializes one ObjectRecord/MobileObject/CraftData slot from the current
// flight-group spawn context (the g_spawn* globals set by the caller).
// objectTypeOverride == 0xFFFF takes the current FG craft type; existingObjIdx
// == 0xFFFF allocates a free genus-range slot, otherwise reinitializes that
// slot in place. Returns the object index, or -1 on failure.
ObjectIndex Mission_InitFlightGroupObjectSlot(uint16_t objectTypeOverride, uint16_t existingObjIdx) {
	ObjectTypeId objType;
	int objectType;
	uint16_t objIdx;
	int playerOwnerIdx = 0;
	int boundPlayerSlot;
	uint8_t hasPlayerOwner;
	ModelIndex modelIndex;
	int groupAiSaved;
	uint8_t engineGlowCount;

	// Resolve the object type to spawn.
	if ((uint16_t)objectTypeOverride == 0xFFFF) {
		g_spawnObjectType =
			(ObjectTypeId)g_objectTypeTables
				.craftTypeToObjectType[g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType];
		objType = g_spawnObjectType;
	} else {
		g_spawnObjectType = objectTypeOverride;
		objType = objectTypeOverride;
	}
	objectType = (uint16_t)objType;
	if (!(uint16_t)objType || (g_modelTypeTable[objectType].flags & 0x20) != 0)
		return -1;

	// Allocate (or reuse) the object slot.
	if (((uint16_t)existingObjIdx == 0xFFFF)) {
		int genus = g_spawnGenusId;
		uint16_t end = (uint16_t)g_objectSlotRangeByGenus[genus].end;
		uint16_t next = (uint16_t)g_objectSlotRangeByGenus[genus].next;
		while (next < end) {
			if (!g_objectTable[next].objectType) {
				g_objectTable[next].mobj->sourceObjIdx = -1;
				g_objectTable[next].mobj->instanceExtent = 0;
				break;
			}
			++next;
		}
		if (next < end) {
			if (g_flightPlayerCount > 1 || g_filmRecording || g_filmPlaybackMode)
				g_objectTable[next].objectSignature = 1;
			collide_ResetObjectProximityForSlot(next);
			objIdx = (ObjectIndex)next;
		} else {
			objIdx = -1;
		}
		if ((uint16_t)objIdx == 0xFFFF)
			return -1;

		// Fresh-slot mobile-object/craft state reset.
		g_objectTable[objIdx].typeSpecificWord = 0;
		memset(&g_objectTable[objIdx].mobj->proximityList, 0,
			   sizeof(g_objectTable[objIdx].mobj->proximityList));
		g_objectTable[objIdx].mobj->spinDecelRate = 0;
		g_objectTable[objIdx].mobj->collisionObjIdx = 0xFFFF;
		g_objectTable[objIdx].mobj->velocityOverrideSpeed = 0;
		g_objectTable[objIdx].mobj->velocityOverrideElapsed = 0;
		g_objectTable[objIdx].mobj->velocityOverrideDuration = 0;
		g_objectTable[objIdx].mobj->velocityOverrideDirX = 0;
		g_objectTable[objIdx].mobj->velocityOverrideDirY = 0;
		g_objectTable[objIdx].mobj->velocityOverrideDirZ = 0;
		g_objectTable[objIdx].mobj->renderOffsetX = 0;
		g_objectTable[objIdx].mobj->renderOffsetY = 0;
		g_objectTable[objIdx].mobj->renderOffsetZ = 0;
		g_objectTable[objIdx].mobj->spinAxisX = 0.0f;
		g_objectTable[objIdx].mobj->spinAxisY = 0.0f;
		g_objectTable[objIdx].mobj->spinAxisZ = 0.0f;
		g_objectTable[objIdx].mobj->pCraft->aiLinkResolving = 0;
		g_objectTable[objIdx].mobj->pCraft->breakupPitchRate = 0;
		g_objectTable[objIdx].mobj->pCraft->breakupYawRate = 0;
		g_objectTable[objIdx].mobj->pCraft->aiFlight.headingStep = 0;
		g_objectTable[objIdx].mobj->pCraft->aiFlight.rollStep = 0;
		g_objectTable[objIdx].mobj->pCraft->aiFlight.turnStep = 0;
		*(uint32_t*)&g_objectTable[objIdx].mobj->pCraft->specialCargoName[16] = 0;
		*(uint32_t*)&g_objectTable[objIdx].mobj->pCraft->reserved3ED[0] = 0;
		*(uint32_t*)&g_objectTable[objIdx].mobj->pCraft->reserved3ED[4] = 0;
		memset(g_objectTable[objIdx].mobj->pCraft->warheadData, 0,
			   sizeof(g_objectTable[objIdx].mobj->pCraft->warheadData));
		memset(g_objectTable[objIdx].mobj->pCraft->aiFlight.objSignatures, 0,
			   sizeof(g_objectTable[objIdx].mobj->pCraft->aiFlight.objSignatures));
	} else {
		objIdx = existingObjIdx;
	}
	collide_ResetObjectProximityForSlot(objIdx);

	// Bind player slot(s) when this is the flight group's player craft.
	boundPlayerSlot = -1;
	if (g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx != -1 &&
		g_spawnCraftOrdinal == g_missionFlightGroups[g_currentFlightGroupIdx].fg.playerCraft &&
		g_initialSpawnBindPlayerCraftSlots) {
		uint8_t region;
		int i;
		playerOwnerIdx = g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx;
		for (i = 0; i < 8; ++i) {
			if (g_pilotData.networkPlayers[i].directPlayId && g_pilotData.networkPlayers[i].m20 &&
				g_pilotData.networkPlayers[i].flightGroupId == g_currentFlightGroupIdx)
				boundPlayerSlot = NetSession_FindPlayerSlotByDpid(g_pilotData.networkPlayers[i].directPlayId);
		}
		region = (uint8_t)regionIdx;
		g_players[playerOwnerIdx].objectIndex = objIdx;
		g_players[playerOwnerIdx].regionIndex = region;
		g_players[playerOwnerIdx].currentSeatIdx = 0;
		if (boundPlayerSlot != -1) {
			g_players[boundPlayerSlot].objectIndex = objIdx;
			g_players[boundPlayerSlot].regionIndex = region;
			g_players[boundPlayerSlot].currentSeatIdx = 1;
			g_players[boundPlayerSlot].hasCheckpointFlag = 1;
		}
		objectTypeOverride = (ObjectTypeId)1;
		g_objectTable[objIdx].playerOwnerIdx = playerOwnerIdx;
	} else if (((uint16_t)existingObjIdx == 0xFFFF)) {
		objectTypeOverride = (ObjectTypeId)0;
		g_objectTable[objIdx].playerOwnerIdx = -1;
	}
	if (g_missionFlightGroups[g_currentFlightGroupIdx].playerOwnerIdx != -1)
		hasPlayerOwner = 1;
	else
		hasPlayerOwner = 0;
	if (g_inHangarReady && (uint16_t)existingObjIdx != 0xFFFF)
		objectTypeOverride = (ObjectTypeId)1;

	g_curCraft = g_objectTable[objIdx].mobj->pCraft;
	if (((uint16_t)existingObjIdx == 0xFFFF)) {
		g_curCraft->effectiveAiObjectLink = NULL;
		g_curCraft->turretAim.effectiveAiObjectSignature = 0;
	}

	// Identity and signature.
	g_objectTable[objIdx].objectType = objType;
	g_objectTable[objIdx].objectSignature = g_nextObjectSignature++;
	if (!g_nextObjectSignature)
		g_nextObjectSignature = 2;

	// Collision damage amount derived from bounds, with per-type overrides.
	{
		int damage = g_modelTypeTable[objectType].maxBoundsExtent;
		switch (objectType) {
			case OBJ_ContainerGem:
				damage *= 8;
				break;
			case OBJ_HomingMineA:
				damage = 10000;
				break;
			case OBJ_HomingMineB:
				damage = 20000;
				break;
			case OBJ_ProximityMineA:
				damage = 5000;
				break;
			case OBJ_ProximityMineB:
				damage = 30000;
				break;
			case OBJ_RebelPilot:
			case OBJ_ImperialPilot:
			case OBJ_CivilianPilot:
			case OBJ_ZeroGStormtrooper:
			case OBJ_ZeroGUtility:
				damage = 12;
				break;
			default:
				break;
		}
		if (damage < 0x2000)
			damage *= 4;
		g_objectTable[objIdx].mobj->damageAmount = damage;
	}

	modelIndex = GetModelIndexFromType(objType);
	g_curCraft->modelIndex = modelIndex;
	g_objectTable[objIdx].mobj->iff = g_spawnIff;
	g_spawnLastAssignedIff = (uint8_t)g_objectTable[objIdx].mobj->iff;
	g_objectTable[objIdx].mobj->team = g_spawnTeamId;
	g_objectTable[objIdx].genusId = g_spawnGenusId;
	g_objectTable[objIdx].regionIdx = (uint8_t)regionIdx;
	g_objectTable[objIdx].mobj->state = g_modelTypeTable[objectType].familyId;
	g_objectTable[objIdx].mobj->motionFlags = 0;
	if (((uint16_t)existingObjIdx == 0xFFFF)) {
		g_objectTable[objIdx].mobj->framesAlive = 0;
		g_objectTable[objIdx].mobj->lifetimeTimer = 0;
		g_objectTable[objIdx].mobj->simStateTimestamp = 0;
	}

	// Ejection-pod spawn count by genus.
	switch (g_objectTable[objIdx].genusId) {
		case GENUS_Fighter: {
			int ejectionObjectType = g_objectTable[objIdx].objectType;
			if (ejectionObjectType >= OBJ_TIEBizarro && ejectionObjectType <= OBJ_TIEBooster)
				g_objectTable[objIdx].mobj->ejectionSpawnCount = 0;
			if ((GameRand() & 7) != 0)
				g_objectTable[objIdx].mobj->ejectionSpawnCount = 0;
			else
				g_objectTable[objIdx].mobj->ejectionSpawnCount = 1;
			break;
		}
		case GENUS_Transport:
			g_objectTable[objIdx].mobj->ejectionSpawnCount = 1;
			break;
		case GENUS_Starship:
			g_objectTable[objIdx].mobj->ejectionSpawnCount = 2;
			break;
		case GENUS_Platform:
			g_objectTable[objIdx].mobj->ejectionSpawnCount = 1;
			break;
		default:
			g_objectTable[objIdx].mobj->ejectionSpawnCount = 0;
			break;
	}
	if (g_spawnStatus1 == 22 || g_spawnStatus2 == 22)
		g_objectTable[objIdx].mobj->ejectionSpawnCount = 0;

	g_objectTable[objIdx].mobj->nodeSwitchIndex = g_missionFlightGroups[g_currentFlightGroupIdx].fg.markings;
	g_objectTable[objIdx].mobj->sourceObjIdx = objIdx;
	g_objectTable[objIdx].mobj->sourceObjectType = objType;
	g_objectTable[objIdx].flightGroupIdx = (uint8_t)g_currentFlightGroupIdx;

	// Leader / formation / world placement (fresh spawn only).
	if (((uint16_t)existingObjIdx == 0xFFFF)) {
		int separation;
		g_curCraft->leader_obj_idx = g_spawnLeaderObjIdx;
		if (g_spawnLeaderObjIdx == -1)
			g_spawnLeaderObjIdx = objIdx;
		g_curCraft->aiFlight.formationType = (char)g_spawnFormation;
		g_curCraft->waveNumber = (uint8_t)g_spawnCraftOrdinal;
		g_curCraft->followFormationSlot = 1;
		separation = g_spawnFromMothershipFlag ? 0 : g_spawnFormationSpacing;
		g_curCraft->aiFlight.separation = (char)separation;
		g_curCraft->pushAccumZ = 0;
		g_curCraft->pushAccumY = 0;
		g_curCraft->pushAccumX = 0;

		// craftIndexInGroup: global-unit running count, else trailing digit of name.
		if (!g_missionFlightGroups[g_currentFlightGroupIdx].fg.globalUnit ||
			g_missionFlightGroups[g_currentFlightGroupIdx].fg.disableWaveNumbering) {
			char lastChar = g_missionFlightGroups[g_currentFlightGroupIdx]
								.fg.name[strlen(g_missionFlightGroups[g_currentFlightGroupIdx].fg.name) - 1];
			if (lastChar < '1' || lastChar > '9')
				g_curCraft->craftIndexInGroup = 0;
			else
				g_curCraft->craftIndexInGroup = lastChar - '0';
		} else {
			int globalUnit = g_missionFlightGroups[g_currentFlightGroupIdx].fg.globalUnit;
			int* count = &g_missionGlobalUnitCraftCount[globalUnit];
			++*count;
			g_curCraft->craftIndexInGroup = *count;
		}

		if (!g_spawnUseExactPosition) {
			int leaderObjIdx;
			int16_t fwdArg;
			int16_t upArg;
			int16_t sideArg;
			int16_t divisor;
			int offUp;
			int offFwd;
			int offSide;
			int fpz;
			int fpy;
			int fpx;
			int formIdx;
			int sepPlus1;
			int boundX = 0, boundY = 0, boundZ = 0;
			if (modelIndex != 0xFFFF) {
				boundX = g_modelDefs[modelIndex].boundSizeX;
				boundY = g_modelDefs[modelIndex].boundSizeY;
				boundZ = g_modelDefs[modelIndex].boundSizeZ;
			}
			// Formation slot offset in body space. The factors only survive as
			// 16-bit quantities (pai_calcrotatedpoint takes int16 args), so plain
			// 32-bit math reproduces the original exactly.
			sepPlus1 = separation + 1;
			formIdx = (uint16_t)g_spawnCraftOrdinal + 6 * g_spawnFormation;
			fpx = (uint16_t)((const int16_t*)g_formPosX)[formIdx];
			fpy = (uint16_t)((const int16_t*)g_formPosY)[formIdx];
			fpz = (uint16_t)((const int16_t*)g_formPosZ)[formIdx];
			offSide = fpx * (sepPlus1 * boundX);
			offFwd = fpy * sepPlus1 * boundY;
			offUp = fpz * sepPlus1 * boundZ;
			if (sepPlus1 == 1) {
				offSide += fpx * (boundX / 2);
				offUp += fpz * (boundZ / 2);
				offFwd += fpy * (boundY / 4);
			}
			divisor = g_formationDivisor[g_spawnFormation];
			sideArg = (int16_t)offSide;
			upArg = (int16_t)offUp;
			fwdArg = (int16_t)offFwd;
			if (divisor != 1) {
				sideArg = (int16_t)(sideArg / divisor);
				fwdArg = (int16_t)(fwdArg / divisor);
				upArg = (int16_t)(upArg / divisor);
			}
			leaderObjIdx = g_curCraft->leader_obj_idx;
			if (leaderObjIdx == -1) {
				g_objectTable[objIdx].world_x = g_spawnWorldX;
				g_objectTable[objIdx].world_y = g_spawnWorldY;
				g_objectTable[objIdx].world_z = g_spawnWorldZ;
				g_objectTable[objIdx].yaw = g_spawnYaw;
				g_objectTable[objIdx].pitch = g_spawnPitch;
				g_objectTable[objIdx].roll =
					(Q16Angle)(g_missionFlightGroups[g_currentFlightGroupIdx].fg.roll << 8);
				g_objectTable[objIdx].angleD = 0;
				g_objectTable[objIdx].mobj->orientMatrixDirty = 1;
				g_objectTable[objIdx].mobj->moveVectorDirty = 1;
				pai_calcrotatedpoint(&g_objectTable[objIdx], sideArg, upArg, fwdArg);
			} else {
				pai_calcrotatedpoint(&g_objectTable[leaderObjIdx], sideArg, upArg, fwdArg);
				g_objectTable[objIdx].roll =
					(Q16Angle)(g_missionFlightGroups[g_currentFlightGroupIdx].fg.roll << 8);
				g_objectTable[objIdx].angleD = 0;
			}
			if (modelIndex != 0xFFFF) {
				uint16_t shift = g_modelDefs[modelIndex].boundSizeShift;
				if (shift) {
					g_rotatedX <<= shift;
					g_rotatedY <<= shift;
					g_rotatedZ <<= shift;
				}
			}
			g_objectTable[objIdx].world_x = g_rotatedX + g_spawnWorldX;
			g_objectTable[objIdx].world_y = g_rotatedY + g_spawnWorldY;
			g_objectTable[objIdx].world_z = g_rotatedZ + g_spawnWorldZ;
		} else {
			g_objectTable[objIdx].world_x = g_spawnWorldX;
			g_objectTable[objIdx].world_y = g_spawnWorldY;
			g_objectTable[objIdx].world_z = g_spawnWorldZ;
		}
	}

	g_objectTable[objIdx].mobj->prevWorldX = g_objectTable[objIdx].world_x;
	g_objectTable[objIdx].mobj->prevWorldY = g_objectTable[objIdx].world_y;
	g_objectTable[objIdx].mobj->prevWorldZ = g_objectTable[objIdx].world_z;

	// Cargo (fresh spawn only).
	if (((uint16_t)existingObjIdx == 0xFFFF)) {
		const char* cargoSrc;
		int cargoCharIdx;
		if (g_curCraft->waveNumber == g_missionFlightGroups[g_currentFlightGroupIdx].fg.specialCargoCraft) {
			g_curCraft->cargoIndex =
				g_missionFlightGroups[g_currentFlightGroupIdx].fg.globalSpecialCargoIndex;
			cargoSrc = g_missionFlightGroups[g_currentFlightGroupIdx].fg.specialCargo;
		} else {
			g_curCraft->cargoIndex = g_missionFlightGroups[g_currentFlightGroupIdx].fg.globalCargoIndex;
			cargoSrc = g_missionFlightGroups[g_currentFlightGroupIdx].fg.cargo;
		}
		for (cargoCharIdx = 0; cargoCharIdx < 16; ++cargoCharIdx)
			g_curCraft->specialCargoName[cargoCharIdx] = cargoSrc[cargoCharIdx];
	}

	g_curCraft->boardingState = 0;
	g_curCraft->systemFlags = 1023;
	g_curCraft->installedHudFeatureMask = 0x1FFF;
	if (((uint16_t)existingObjIdx == 0xFFFF)) {
		g_objectTable[objIdx].yaw = g_spawnYaw;
		g_objectTable[objIdx].pitch = g_spawnPitch;
	}
	g_objectTable[objIdx].mobj->rollImpulseRate = 0;
	g_objectTable[objIdx].mobj->spinRate = 0;
	g_objectTable[objIdx].mobj->spinRateFrac = 0;
	g_objectTable[objIdx].mobj->spinAngleQ16 = 0;
	g_objectTable[objIdx].mobj->velocityOverrideActive = 0;
	g_objectTable[objIdx].mobj->orientMatrixDirty = 1;
	g_objectTable[objIdx].mobj->moveVectorDirty = 1;
	g_curCraft->aiFlight.enterFlag = 0;
	g_curCraft->aiFlight.headingState = 0;
	g_curCraft->aiFlight.turnState = 0;
	g_curCraft->aiFlight.climbState = 0;
	g_curCraft->aiFlight.diveState = 0;
	g_curCraft->aiFlight.headingForce = 0;
	g_curCraft->aiFlight.motionScale = -1;
	g_curCraft->aiFlight.rollAccel = -1;
	g_curCraft->aiFlight.pitchAccel = -1;
	g_curCraft->aiFlight.turnAccel = -1;
	engineGlowCount = 0;
	if (modelIndex != 0xFFFF) {
		g_curCraft->aiFlight.rollRate = g_modelDefs[modelIndex].rollRate;
		g_curCraft->aiFlight.pitchRate = g_modelDefs[modelIndex].pitchRate;
		g_curCraft->aiFlight.turnRate = g_modelDefs[modelIndex].yawRate;
		g_curCraft->aiFlight.maxSpeedCache = g_modelDefs[modelIndex].maxSpeed;
	}
	if (((uint16_t)existingObjIdx == 0xFFFF)) {
		groupAiSaved = g_spawnGroupAI;
		g_curCraft->skillValue = g_aiSkillValueQ16ByLevel[g_spawnGroupAI];
	}

	// Laser/cannon group setup.
	{
		int laserSlotAccum;
		laserSlotAccum = 0;
		g_curCraft->cannonClassCount = 0;
		{
			int i = 0;
			int remaining = 3;
			do {
				g_curCraft->laserProjectileTypeId[i] = 0;
				if ((uint8_t)objectTypeOverride)
					g_curCraft->laserLinkMode[i] = 1;
				else
					g_curCraft->laserLinkMode[i] = 0;
				g_curCraft->laserLinkMode[i + 3] = 0;
				g_curCraft->laserLinkNextSlot[i] = 0;
				g_curCraft->laserFireCooldownTicks[i] = 0;
				g_curCraft->laserLastFireTimestamp[i] = 0;
				++i;
			} while (--remaining);
		}

		if (modelIndex != 0xFFFF) {
			uint8_t convergeLevel;
			uint16_t g;
			uint16_t launcher;
			uint16_t slot;
			if ((g_modelTypeTable[(uint16_t)g_spawnObjectType].flags & 0x800) == 0) {
				for (g = 0; g < 3; ++g) {
					g_curCraft->laserProjectileTypeId[g] = g_modelDefs[modelIndex].laserGroupWeaponType[g];
					if (g_curCraft->laserProjectileTypeId[g]) {
						uint8_t firstSlot = g_modelDefs[modelIndex].laserGroupFirstSlot[g];
						uint8_t lastSlot = g_modelDefs[modelIndex].laserGroupLastSlot[g];
						laserSlotAccum += g_modelDefs[modelIndex].laserGroupSlotCount[g];
						if (g_modelDefs[modelIndex].laserGroupMountType[g] == 1 ||
							g_modelDefs[modelIndex].laserGroupMountType[g] == 2) {
							++g_curCraft->cannonClassCount;
							g_curCraft->laserLinkNextSlot[g] = (char)firstSlot;
						}
						if (firstSlot <= lastSlot) {
							for (slot = firstSlot; slot <= lastSlot; ++slot) {
								g_curCraft->warheadData[slot].projectileTypeId =
									g_curCraft->laserProjectileTypeId[g];
								g_curCraft->warheadData[slot].weaponType =
									(g_modelDefs[modelIndex].laserGroupMountType[g] == 1 ||
									 g_modelDefs[modelIndex].laserGroupMountType[g] == 2)
										? g_modelDefs[modelIndex].laserGroupMountType[g]
										: 4;
								g_curCraft->warheadData[slot].weaponGroupIdx = (uint8_t)g;
								g_curCraft->warheadData[slot].laserCharge = 127;
								g_curCraft->warheadData[slot].count = 0;
								g_curCraft->warheadData[slot].lastFireMeshIdx = 0xFF;
								g_curCraft->warheadData[slot].lastFireHardpointIdx = 0xFF;
								g_curCraft->warheadData[slot].turretTargetObjIdx = -1;
							}
						}
					}
				}
			}

			// Laser convergence level.
			switch (g_modelDefs[modelIndex].laserConvergeMode) {
				case 0:
					convergeLevel = 0;
					break;
				case 1:
					convergeLevel = 3;
					break;
				case 2:
					convergeLevel = 4;
					break;
				default:
					convergeLevel = (uint8_t)existingObjIdx;
					break;
			}
			g_curCraft->laserConvergeLevel = convergeLevel;

			// Expanded-probe craft place warhead-class laser groups into free slots.
			if ((g_modelTypeTable[(uint16_t)g_spawnObjectType].flags & 0x800) != 0) {
				for (g = 0; g < 3; ++g) {
					uint8_t remaining = g_modelDefs[modelIndex].laserGroupSlotCount[g];
					if (g_modelDefs[modelIndex].laserGroupMountType[g] >= 4 && remaining) {
						for (slot = 0; slot < 16; ++slot) {
							if (!g_curCraft->warheadData[slot].weaponType) {
								g_curCraft->warheadData[slot].projectileTypeId =
									g_modelDefs[modelIndex].laserGroupWeaponType[g]
										? g_modelDefs[modelIndex].laserGroupWeaponType[g]
										: 281;
								g_curCraft->warheadData[slot].weaponType =
									g_modelDefs[modelIndex].laserGroupMountType[g];
								g_curCraft->warheadData[slot].laserCharge = 127;
								g_curCraft->warheadData[slot].count = 0;
								g_curCraft->warheadData[slot].lastFireMeshIdx = 0xFF;
								g_curCraft->warheadData[slot].lastFireHardpointIdx = 0xFF;
								g_curCraft->warheadData[slot].weaponGroupIdx = (uint8_t)g;
								g_curCraft->warheadData[slot].turretTargetObjIdx = -1;
								// Original only updates the low byte of the slot accumulator.
								laserSlotAccum = (laserSlotAccum & ~0xFF) | (uint8_t)(laserSlotAccum + 1);
								if (remaining == 1)
									break;
								--remaining;
							}
						}
					}
				}
			}

			g_curCraft->laserRedirect = 2;
			g_curCraft->laserSlotCount = (uint8_t)laserSlotAccum;

			// Laser HUD bit cleared when no cannon-class weapons (per hangar/player rules).
			if (g_curCraft->cannonClassCount == 0 && (!(uint8_t)objectTypeOverride || g_inHangarReady))
				g_curCraft->systemFlags ^= 0x10;

			// Warhead launcher setup.
			g_curCraft->warheadLauncherCount = 0;
			if (g_modelDefs[modelIndex].warheadLauncherType[1]) {
				g_curCraft->warheadSlotTypeIds[0] = 287;
				g_curCraft->warheadSlotTypeIds[1] =
					g_warheadTypeIds[g_missionFlightGroups[g_currentFlightGroupIdx].fg.warhead];
			} else {
				g_curCraft->warheadSlotTypeIds[0] =
					g_warheadTypeIds[g_missionFlightGroups[g_currentFlightGroupIdx].fg.warhead];
				g_curCraft->warheadSlotTypeIds[1] = 0;
			}
			for (launcher = 0; launcher < 2; ++launcher) {
				g_curCraft->warheadLauncherFlags[launcher] = 1;
				g_curCraft->warheadLauncherCooldownTicks[launcher] = 0;
				if (g_provingGroundsModeActive) {
					if (g_curCraft->warheadSlotTypeIds[launcher]) {
						uint8_t lastSlot;
						uint8_t firstSlot;
						g_curCraft->warheadSlotTypeIds[launcher] = 0;
						g_curCraft->warheadLauncherCount = 0;
						firstSlot = g_modelDefs[modelIndex].warheadLauncherFirstSlot[launcher];
						lastSlot = g_modelDefs[modelIndex].warheadLauncherLastSlot[launcher];
						if (firstSlot <= lastSlot) {
							for (slot = firstSlot; slot <= lastSlot; ++slot) {
								g_curCraft->warheadData[slot].projectileTypeId = 0;
								g_curCraft->warheadData[slot].weaponType = 3;
								g_curCraft->warheadData[slot].laserCharge = 0;
								g_curCraft->warheadData[slot].turretTargetObjIdx = -1;
								g_curCraft->warheadData[slot].count = 0;
							}
						}
					}
				} else {
					if (g_curCraft->warheadSlotTypeIds[launcher]) {
						uint8_t lastSlot;
						uint8_t firstSlot;
						++g_curCraft->warheadLauncherCount;
						firstSlot = g_modelDefs[modelIndex].warheadLauncherFirstSlot[launcher];
						lastSlot = g_modelDefs[modelIndex].warheadLauncherLastSlot[launcher];
						if (firstSlot <= lastSlot) {
							for (slot = firstSlot; slot <= lastSlot; ++slot) {
								uint8_t ammo;
								int warheadAmmoClass;
								g_curCraft->warheadData[slot].projectileTypeId =
									g_curCraft->warheadSlotTypeIds[launcher];
								g_curCraft->warheadData[slot].weaponType = 3;
								g_curCraft->warheadData[slot].laserCharge = 127;
								g_curCraft->warheadData[slot].turretTargetObjIdx = -1;
								if (g_modelDefs[modelIndex].warheadLauncherType[1] == 0 || launcher != 0)
									warheadAmmoClass =
										g_missionFlightGroups[g_currentFlightGroupIdx].fg.warhead;
								else
									warheadAmmoClass = 3;
								ammo = (uint8_t)MATH2_fraction(
									g_modelDefs[modelIndex].warheadLauncherValue[launcher],
									(uint16_t)g_warheadAmmoCounts[warheadAmmoClass]);
								if (!ammo)
									ammo = 1;
								if (g_spawnStatus1 == 1 || g_spawnStatus2 == 1)
									ammo *= 2;
								else if (g_spawnStatus1 == 2 || g_spawnStatus2 == 2)
									ammo >>= 1;
								if (!ammo)
									ammo = 1;
								if (hasPlayerOwner && ammo > 9 &&
									g_objectTable[objIdx].objectType != OBJ_MissileBoat)
									ammo = 9;
								g_curCraft->warheadData[slot].count = ammo;
							}
						}
					}
				}
			}
			g_curCraft->warheadLockTicks = 0;
			if (!g_curCraft->warheadLauncherCount)
				g_curCraft->systemFlags ^= 8;
		} else {
			g_curCraft->systemFlags = 0;
			g_curCraft->workingSubsystems = 0;
			g_curCraft->installedHudFeatureMask = 0;
		}
	}

	// Per-mission counters and player-stat binding (fresh spawn only).
	if (((uint16_t)existingObjIdx == 0xFFFF)) {
		g_curCraft->laserHitsScoredCount = 0;
		g_curCraft->laserShotsFiredCount = 0;
		g_curCraft->ionHitsScoredCount = 0;
		g_curCraft->ionShotsFiredCount = 0;
		g_curCraft->warheadHitsScoredCount = 0;
		g_curCraft->warheadsFiredCount = 0;
	}
	if ((uint8_t)objectTypeOverride && ((uint16_t)existingObjIdx == 0xFFFF)) {
		uint16_t boundSig = g_objectTable[objIdx].objectSignature;
		Mission_ResetPlayerSpawnStats(playerOwnerIdx, boundSig, g_currentFlightGroupIdx);
		if (boundPlayerSlot != -1)
			Mission_ResetPlayerSpawnStats(boundPlayerSlot, boundSig, g_currentFlightGroupIdx);
	}

	// Hull / damage state.
	if (modelIndex != 0xFFFF) {
		g_curCraft->hullMax = g_modelDefs[modelIndex].hullStrength;
		g_curCraft->systemDamageHullThreshold = g_modelDefs[modelIndex].systemDamageHullThreshold;
	}
	g_curCraft->hullDamage = 0;
	g_curCraft->subsystemDamage = 0;
	g_curCraft->lastSystemHitTime = 0;
	g_curCraft->systemHitFlag = 0;
	if (((uint16_t)existingObjIdx == 0xFFFF)) {
		int i;
		g_curCraft->damageReceivedTotal = 0;
		g_curCraft->damageReceivedByPlayerOwnedCraft = 0;
		g_curCraft->damageFromCollision = 0;
		g_curCraft->damageFromStarship = 0;
		g_curCraft->damageFromMine = 0;
		for (i = 0; i < 8; ++i)
			g_curCraft->damageFromPlayer[i] = 0;
		for (i = 0; i < 10; ++i)
			g_curCraft->attackedByTeam[i] = 0;
		for (i = 0; i < 8; ++i) {
			g_curCraft->damageFromFlightGroupAmount[i] = 0;
			g_curCraft->damageFromFlightGroupIdx[i] = -1;
		}
		for (i = 0; i < 6; ++i)
			g_curCraft->damageFromAiSkill[i] = 0;
	}
	g_curCraft->weaponFireInhibitTimer = 0;
	g_curCraft->missionAccountingDone = 0;
	g_curCraft->unusedMissionFlag189 = 0;
	g_curCraft->notDisabledAccountingSuppress = 0;
	g_curCraft->wasCaptured = 0;
	g_curCraft->sFoilState = 0;
	{
		int i;
		for (i = 0; i < 5; ++i)
			g_curCraft->beamEffectAccum[i] = 0;
	}
	g_curCraft->beamActive = 0;
	g_curCraft->beamTimer = 0;
	g_curCraft->beamTargetObjIdx = -1;

	// IFF visibility per team.
	{
		int n;
		for (n = 0; n < 10; ++n) {
			g_curCraft->iffVisibility[n] = 0xFF;
			if (g_objectTable[objIdx].mobj->team == n) {
				if (g_spawnStatus1 != 24 && g_spawnStatus2 != 24)
					g_curCraft->iffVisibility[n] = 1;
			} else if (g_spawnStatus1 == 25 || g_spawnStatus2 == 25) {
				g_curCraft->iffVisibility[n] = 1;
			} else if (g_spawnStatus1 == 26 || g_spawnStatus2 == 26) {
				g_curCraft->iffVisibility[n] = 0;
			}
		}
	}

	if (modelIndex != 0xFFFF) {
		// Hyperdrive availability.
		if (!g_modelDefs[modelIndex].hasHyperdrive && g_spawnStatus1 != 9 && g_spawnStatus2 != 9 &&
			g_spawnStatus1 != 16 && g_spawnStatus2 != 16)
			g_curCraft->systemFlags ^= 0x80;
		if (g_spawnStatus1 == 6 || g_spawnStatus2 == 6)
			g_curCraft->systemFlags ^= 0x80;

		// Shields.
		g_curCraft->shieldFront = g_modelDefs[modelIndex].shieldStrength;
		if ((uint8_t)objectTypeOverride) {
			g_curCraft->shieldRear = g_modelDefs[modelIndex].shieldStrength;
			g_curCraft->shieldDistribMode = 1;
		} else {
			g_curCraft->shieldFront += g_modelDefs[modelIndex].shieldStrength;
			g_curCraft->shieldRear = 0;
			g_curCraft->shieldDistribMode = 0;
		}
		if (g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH && !g_missionHeader.body.goalsUnimportant &&
			g_spawnGenusId == GENUS_Fighter) {
			g_curCraft->shieldFront *= 2;
			g_curCraft->shieldRear *= 2;
		} else if (g_spawnStatus1 == 3 || g_spawnStatus2 == 3) {
			g_curCraft->shieldFront = 0;
			g_curCraft->shieldRear = 0;
			g_curCraft->systemFlags ^= 1;
		} else if (g_spawnStatus1 == 4 || g_spawnStatus2 == 4) {
			g_curCraft->shieldFront >>= 1;
			g_curCraft->shieldRear >>= 1;
			g_curCraft->systemFlags ^= 1;
		} else if (g_spawnStatus1 == 7 || g_spawnStatus2 == 7) {
			g_curCraft->shieldFront = 0;
			g_curCraft->shieldRear = 0;
		} else if (g_spawnStatus1 == 19 || g_spawnStatus2 == 19 || g_spawnStatus1 == 13 ||
				   g_spawnStatus2 == 13) {
			g_curCraft->shieldFront >>= 1;
			g_curCraft->shieldRear >>= 1;
		} else if (g_spawnStatus1 == 18 || g_spawnStatus2 == 18 || g_spawnStatus1 == 12 ||
				   g_spawnStatus2 == 12) {
			g_curCraft->shieldFront *= 2;
			g_curCraft->shieldRear *= 2;
		}
		g_curCraft->shieldRedirect = 2;
		if (!g_modelDefs[modelIndex].hasShields && g_spawnStatus1 != 8 && g_spawnStatus2 != 8 &&
			g_spawnStatus1 != 16 && g_spawnStatus2 != 16) {
			g_curCraft->systemFlags ^= 1;
			g_curCraft->shieldFront = 0;
			g_curCraft->shieldRear = 0;
			g_curCraft->installedHudFeatureMask ^= 0x800;
		}

		// Beam weapon.
		g_curCraft->beamTypeId = g_missionFlightGroups[g_currentFlightGroupIdx].fg.beam;
		if (g_spawnObjectType == OBJ_XWing || g_spawnObjectType == OBJ_YWing ||
			g_spawnObjectType == OBJ_BWing || g_spawnObjectType == OBJ_AWing ||
			g_spawnObjectType == OBJ_Z95 || g_spawnObjectType == OBJ_TIEFighter)
			g_curCraft->beamTypeId = 0;
		g_curCraft->beamLevel = 2;
		g_curCraft->beamPresent = 9999;
		if (!g_curCraft->beamTypeId) {
			g_curCraft->beamPresent = 0;
			g_curCraft->systemFlags ^= 0x100;
			g_curCraft->installedHudFeatureMask ^= 0x1000;
			g_curCraft->installedHudFeatureMask ^= 0x10;
		}

		// Countermeasures.
		g_curCraft->cmTypeId = g_missionFlightGroups[g_currentFlightGroupIdx].fg.countermeasures;
		if (g_curCraft->cmTypeId) {
			g_curCraft->cmAmmoCount = g_modelDefs[modelIndex].countermeasureCount;
			if (g_curCraft->cmTypeId == 2)
				g_curCraft->cmAmmoCount = (uint8_t)MATH2_fraction(g_curCraft->cmAmmoCount, 0xAAAC);
		} else {
			g_curCraft->cmAmmoCount = 0;
			g_curCraft->systemFlags ^= 2;
		}
		g_curCraft->chaffActiveTimer = 0;
		g_curCraft->cmFireCooldownTimer = 0;
		g_curCraft->workingSubsystems = g_curCraft->systemFlags;
		g_curCraft->activeHudFeatureMask = g_curCraft->installedHudFeatureMask;
	}

	g_objectTable[objIdx].typeSpecificByte[0] = 0;
	g_objectTable[objIdx].typeSpecificByte[1] = 0;
	{
		int i;
		for (i = 0; i < 50; ++i) {
			g_curCraft->componentState[i] = 0;
			g_curCraft->meshRotation[i] = 0;
			g_curCraft->componentHp[i] = 0xFF;
		}
	}

	// Component hit points per mesh, plus status-driven turret/HP overrides.
	{
		int meshCount = ModelMesh_GetObjectTypeMeshCount(objectType);
		if (meshCount > 0) {
			int meshIdx = 0;
			do {
				MeshType meshType = ModelMesh_GetObjectTypeMeshType(objectType, meshIdx);
				if (ModelMesh_HasExplosionType1(objectType, meshIdx)) {
					if (objType == OBJ_SuperStarDestroyer && meshType == MESH_ShieldGenerator)
						g_curCraft->componentHp[meshIdx] = (uint8_t)(2 * g_meshTypeComponentMaxHp[8] - 1);
					else
						g_curCraft->componentHp[meshIdx] = g_meshTypeComponentMaxHp[meshType];
				}
				if ((g_spawnStatus1 == 5 || g_spawnStatus2 == 5 || g_spawnStatus1 == 14 ||
					 g_spawnStatus2 == 14) &&
					(meshType == MESH_GunTurret || meshType == MESH_RotaryGunTurret ||
					 meshType == MESH_SmallGun)) {
					g_curCraft->componentHp[meshIdx] = 0;
					g_curCraft->componentState[meshIdx] = 4;
				}
				++meshIdx;
			} while ((uint16_t)meshIdx < meshCount);
		}
	}

	// Beam-mounted platforms disable a fixed set of components.
	if (g_spawnGenusId == GENUS_Platform && (uint16_t)g_spawnObjectType >= OBJ_Platform1 &&
		(uint16_t)g_spawnObjectType <= OBJ_Platform5) {
		uint8_t beam = g_missionFlightGroups[g_currentFlightGroupIdx].fg.beam;
		if (beam) {
			uint16_t tableIdx = (uint16_t)(12 * (uint16_t)g_spawnObjectType - 12 * OBJ_Platform1);
			int count = (beam != 1) ? 12 : 6;
			int end = tableIdx + count;
			while (tableIdx < end) {
				uint8_t comp = g_platformBeamDisabledComponentIds[tableIdx];
				if (comp != 0xFF) {
					g_curCraft->componentHp[comp] = 0;
					g_curCraft->componentState[comp] = 4;
				}
				++tableIdx;
			}
		}
	}

	// AI plan assignment from the spawn-region order.
	{
		PlanRecord* plan;
		uint16_t leaderPlanId;
		uint8_t followerPlanId;
		int order;
		uint16_t throttle;
		order = g_missionFlightGroups[g_currentFlightGroupIdx].fg.orders[4 * g_spawnRegionIdx].order;
		leaderPlanId = g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order]];
		followerPlanId = g_builtinPlanIdByNameIndex[g_orderFollowerBuiltinPlanNameIndex[order]];
		g_curCraft->aiController.currentPlanId = leaderPlanId;
		if (g_spawnLinkedObjectFlag) {
			g_curCraft->aiController.pendingPlanId = 0;
			g_curCraft->aiController.currentPlanId = 0;
		} else if (g_spawnOutOfHyperspaceFlag) {
			g_curCraft->aiController.pendingPlanId = g_builtinPlanIdByNameIndex[52];
		} else if (g_spawnFromMothershipFlag) {
			g_curCraft->aiController.pendingPlanId = g_builtinPlanIdByNameIndex[50];
		} else if ((g_missionElapsedClock.hours | g_missionElapsedClock.minutes |
					g_missionElapsedClock.seconds) != 0 ||
				   g_orderLeaderBuiltinPlanNameIndex[order] != 110) {
			if (g_curCraft->leader_obj_idx == -1)
				g_curCraft->aiController.pendingPlanId = leaderPlanId;
			else
				g_curCraft->aiController.pendingPlanId = followerPlanId;
		} else {
			g_curCraft->aiController.pendingPlanId = g_builtinPlanIdByNameIndex[111];
		}

		// Initial throttle from the order's plan/throttle.
		plan = &g_planTable[leaderPlanId];
		if ((!strcmp(plan->name, "nullpln") || !strcmp(plan->name, "stationaryldrpln") ||
			 !strcmp(plan->name, "stationaryflwpln") || !strcmp(plan->name, "disabledpln")) &&
			!(uint8_t)objectTypeOverride) {
			throttle = 0;
		} else if (!strcmp(plan->name, "escortldr1pln")) {
			throttle = 0x8000;
		} else {
			throttle = g_orderThrottleToCraftThrottleSpeed
				[g_missionFlightGroups[g_currentFlightGroupIdx].fg.orders[4 * g_spawnRegionIdx].throttle];
		}

		if (modelIndex != 0xFFFF) {
			g_curCraft->objectKind = g_spawnObjectKind;
			engineGlowCount = g_modelDefs[modelIndex].engineGlowCount;
			if (((uint16_t)existingObjIdx == 0xFFFF))
				g_curCraft->throttleSpeed = throttle;
			g_curCraft->engineOutputScale = -1;
			g_curCraft->slamActive = 0;
			g_objectTable[objIdx].mobj->speed =
				(uint16_t)MATH2_fraction(g_modelDefs[modelIndex].maxSpeed, throttle);
			g_objectTable[objIdx].mobj->speedRemainder = 0;
			{
				int i;
				for (i = 0; i < engineGlowCount; ++i)
					g_curCraft->engineEmitterHealth[i] = g_modelDefs[modelIndex].componentMaxHp;
			}
		}

		if ((uint8_t)objectTypeOverride) {
			g_players[playerOwnerIdx].selectedWarhead = 0;
			g_players[playerOwnerIdx].selectedWeaponMode = 0;
			g_players[playerOwnerIdx].boundCraftEngineGlowCount = engineGlowCount;
			if (boundPlayerSlot != -1) {
				g_players[boundPlayerSlot].selectedWarhead = 0;
				g_players[boundPlayerSlot].selectedWeaponMode = 0;
				g_players[boundPlayerSlot].boundCraftEngineGlowCount = engineGlowCount;
			}
		}

		// Per-order completion/goal scratch (5 order groups x 4 slots, flat layout).
		{
			int grp;
			for (grp = 0; grp < 20; grp += 4) {
				uint16_t kk;
				for (kk = 0; kk < 4; ++kk) {
					int idx = grp + kk;
					if (kk == 0) {
						((uint8_t*)&g_curCraft->aiController.orderScratch)[idx] = 0;
					} else {
						const uint8_t* skipConds =
							(const uint8_t*)&g_missionFlightGroups[g_currentFlightGroupIdx]
								.fg.skipTriggers[0];
						uint8_t t1 = skipConds[idx * (int)sizeof(XwaTriggerPair)];
						uint8_t t2 = skipConds[idx * (int)sizeof(XwaTriggerPair) +
											   (int)offsetof(XwaTriggerPair, triggers[1])];
						((uint8_t*)&g_curCraft->aiController.orderScratch)[idx] = (t1 || t2) ? 1 : 0;
						if (((const uint8_t*)&g_missionFlightGroups[g_currentFlightGroupIdx]
								 .fg.orders[0])[idx * (int)sizeof(XwaOrder)] == 0)
							((uint8_t*)&g_curCraft->aiController.orderScratch)[idx] = 2;
					}
					((uint8_t*)&g_curCraft->aiController.orderScratch)[idx + 0x14] = 0;
				}
			}
		}

		// AI controller / link / target state (fresh spawn only).
		if (((uint16_t)existingObjIdx == 0xFFFF)) {
			g_curCraft->aiController.targetComponent = -1;
			g_curCraft->carriedObjectIndex = -1;
			g_curCraft->carrierObjIdx = -1;
			g_curCraft->lastReleasedObjectIdx = -1;
			g_curCraft->releaseClearTimer = 0;
			g_curCraft->linkedPrevObjectIdx = -1;
			g_curCraft->nextLinkObjectIdx = -1;
			g_curCraft->linkSequenceIndex = 0;
			g_curCraft->aiFlight.impactObjIdx = -1;
			g_curCraft->aiFlight.goHomeFlag = 0;
			g_curCraft->aiFlight.missionAbortedFlag = 0;
			g_curCraft->aiFlight.departTimerFlag = 0;
			g_curCraft->aiFlight.departClockHours = 0;
			g_curCraft->aiFlight.departClockMin = 0;
			g_curCraft->aiFlight.departClockSec = 0;
			g_curCraft->aiFlight.reactionTimer = 0;
			g_curCraft->commandedSpeed = 0;
			g_curCraft->aiFlight.reserved0C = 0;
			g_curCraft->aiFlight.orderActionCounter = 0;
			g_curCraft->aiFlight.orderActionFlag = 0;
			g_curCraft->aiFlight.objSignatureCount = 0;
			g_curCraft->aiController.escortTargetFG = -1;
			g_curCraft->aiController.currentOrderSlot = 0;
			g_curCraft->aiController.orderStateFlag = 0;
			g_curCraft->aiController.targetObjIdx = -1;
			g_curCraft->aiController.candidateTargetIdx = -1;
			g_curCraft->aiController.targetSignature = 0;
			g_curCraft->aiController.hasLiveTarget = 0;
			g_curCraft->aiController.targetComponent = -1;
			g_curCraft->aiController.escortTargetFG = -1;
			g_curCraft->aiController.aimPointX = 0;
			g_curCraft->aiController.aimPointY = 0;
			g_curCraft->aiController.aimPointZ = 0;
			g_curCraft->aiController.maneuverDist = 0;
			g_curCraft->aiController.orbitRadius = 0;
			g_curCraft->aiController.targetZAngle = 0x4000;
			g_curCraft->aiController.targetRoll = 0;
			g_curCraft->aiController.targetXYAngle = 0;
			g_curCraft->aiController.waypointIndex = 0;
			g_curCraft->aiController.savedPlanId = 0;
			g_curCraft->aiController.thinkInterval = g_aiThinkIntervalByGroupAI[groupAiSaved];
			{
				int randomSeed = GameRand() ^ 0xBEEF;
				g_curCraft->aiController.savedRandSeed = (int16_t)randomSeed;
			}
			g_curCraft->aiController.maneuverMode = 0;
			g_curCraft->aiController.maneuverPhase = 0;
			g_curCraft->aiController.maneuverTimer = 0;
		}

		// System display slots / health / timers.
		{
			uint16_t i;
			for (i = 0; i < 10; ++i) {
				g_curCraft->systemDisplaySlotBySystem[i] = (char)i;
				g_curCraft->systemHealth[i] = 100;
				g_curCraft->systemTimer[i] = 0;
			}
		}

		if (((uint16_t)existingObjIdx == 0xFFFF)) {
			pai_setupcraftcontext(objIdx);
			pai_ApplyPendingPlanTargetAndManeuver(objIdx);
		}

		// Player craft re-applies throttle/speed after AI context setup.
		if ((uint8_t)objectTypeOverride) {
			uint16_t throttle2;
			if (!strcmp(plan->name, "nullpln") || !strcmp(plan->name, "stationaryldrpln") ||
				!strcmp(plan->name, "stationaryflwpln"))
				throttle2 = 0;
			else if (!strcmp(plan->name, "escortldr1pln"))
				throttle2 = 0x8000;
			else
				throttle2 = g_orderThrottleToCraftThrottleSpeed
					[g_missionFlightGroups[g_currentFlightGroupIdx].fg.orders[4 * g_spawnRegionIdx].throttle];
			if (modelIndex != 0xFFFF) {
				g_curCraft->objectKind = g_spawnObjectKind;
				if (((uint16_t)existingObjIdx == 0xFFFF))
					g_curCraft->throttleSpeed = throttle2;
				g_curCraft->engineOutputScale = -1;
				g_curCraft->slamActive = 0;
				g_objectTable[objIdx].mobj->speed =
					(uint16_t)MATH2_fraction(g_modelDefs[modelIndex].maxSpeed, throttle2);
				g_objectTable[objIdx].mobj->speedRemainder = 0;
			}
		}
	}

	g_curCraft->playerCommandAvoidTargetObjIdx = -1;
	g_curCraft->followPlayerMode = 0;
	g_curCraft->followPlayerIdx = 0;
	g_curCraft->followTimer = 0;
	g_curCraft->playerCommandCraftTypeFilter = 0;
	g_curCraft->playerCommandTeamFilter = 0;
	g_curCraft->savedCurrentPlan = 0;
	g_curCraft->savedPendingPlan = 0;
	if (((uint16_t)existingObjIdx == 0xFFFF))
		++g_missionFgStats[g_currentFlightGroupIdx].spawnedCraftCount;

	// Turret aim state (both gunner slots).
	{
		int aimIdx = 0;
		int aimCount = 2;
		do {
			g_curCraft->turretAim.aimAngleA[aimIdx] = 0;
			g_curCraft->turretAim.aimAngleB[aimIdx] = 0;
			g_curCraft->turretAim.aimAccumA[aimIdx] = 0.0f;
			g_curCraft->turretAim.aimAccumB[aimIdx] = 0.0f;
			++aimIdx;
			--aimCount;
		} while (aimCount);
	}

	return objIdx;
}

// FUNCTION: XWA 0x41D810
uint16_t Mission_SpawnPreparedObject(uint16_t flightGroupIdx, uint16_t genusId, ObjectTypeId objectType) {
	uint32_t slot;
	uint32_t slotEnd;
	ObjectRecord* objectTable;
	uint32_t objectIdx;
	uint8_t typeSpecificByte0;

	slot = g_objScanStart;
	slotEnd = g_regionStaticObjectSlotEnd;
	objectTable = g_objectTable;
	for (; slot < slotEnd; ++slot) {
		if (objectTable[slot].objectType == OBJ_None) {
			if (g_flightPlayerCount > 1 || g_filmRecording || g_filmPlaybackMode) {
				objectTable[slot].objectSignature = 1;
			}
			objectIdx = slot;
			break;
		}
	}
	if (slot == slotEnd) {
		objectIdx = 0xffffu;
	}

	if ((uint16_t)objectIdx != 0xffffu) {
		g_objectTable[(uint16_t)objectIdx].regionIdx = (uint8_t)regionIdx;
		g_objectTable[objectIdx].world_x = g_preparedSpawnMissionX;
		g_objectTable[objectIdx].world_y = g_preparedSpawnMissionY;
		g_objectTable[objectIdx].world_z = g_preparedSpawnMissionZ;
		g_objectTable[objectIdx].yaw = g_preparedSpawnYawByte;
		g_objectTable[objectIdx].pitch = g_preparedSpawnPitchByte;
		g_objectTable[objectIdx].roll = g_preparedSpawnRollByte;
		g_objectTable[objectIdx].angleD = 0;
		g_objectTable[objectIdx].world_x <<= 8;
		g_objectTable[objectIdx].world_y <<= 8;
		g_objectTable[objectIdx].world_z <<= 8;
		g_objectTable[objectIdx].yaw = (Q16Angle)((uint8_t)g_objectTable[objectIdx].yaw << 8);
		g_objectTable[objectIdx].pitch = (Q16Angle)((uint8_t)g_objectTable[objectIdx].pitch << 8);
		g_objectTable[objectIdx].roll = (Q16Angle)((uint8_t)g_objectTable[objectIdx].roll << 8);

		g_objectTable[objectIdx].objectSignature = g_nextObjectSignature;
		++g_nextObjectSignature;
		if (g_nextObjectSignature == 0) {
			g_nextObjectSignature = 2;
		}

		g_objectTable[objectIdx].flightGroupIdx = (uint8_t)flightGroupIdx;
		g_objectTable[objectIdx].genusId = (ModelGenusId)genusId;
		g_objectTable[objectIdx].objectType = objectType;
		g_objectTable[objectIdx].typeSpecificWord = 1023;
		if ((g_modelTypeTable[(uint16_t)objectType].assetFlags & 6u) != 0) {
			g_objectTable[objectIdx].typeSpecificByte[0] = 1;
		} else {
			g_objectTable[objectIdx].typeSpecificByte[0] = 0;
		}
		if (genusId == GENUS_Mine) {
			typeSpecificByte0 = (uint8_t)(29u * (g_missionFgStats[flightGroupIdx].outcomeCount[1] & 7u));
			objectTable = g_objectTable;
		} else {
			objectTable = g_objectTable;
			typeSpecificByte0 = objectTable[objectIdx].typeSpecificByte[0];
		}
		objectTable[objectIdx].typeSpecificByte[1] = typeSpecificByte0;
		++g_missionFgStats[flightGroupIdx].outcomeCount[1];
	}
	return (uint16_t)objectIdx;
}

// FUNCTION: XWA 0x41D410
void Mission_SpawnFlightGroupStaticObjects(uint16_t instanceFilter) {
	unsigned int rowCount;
	int baseY;
	int baseX;
	int baseZ;
	ObjectTypeId objectType;
	uint16_t flightGroupIdx;

	flightGroupIdx = g_currentFlightGroupIdx;
	if (g_missionFgStats[flightGroupIdx].outcomeCount[0] == 0) {
		return;
	}

	objectType =
		(ObjectTypeId)
			g_objectTypeTables.craftTypeToObjectType[g_missionFlightGroups[flightGroupIdx].fg.craftType];
	if ((int8_t)g_modelTypeTable[(uint16_t)objectType].flags >= 0) {
		return;
	}

	switch (g_modelTypeTable[(uint16_t)objectType].genusId) {
		case GENUS_Asteroid: {
			GameRand_SavePrimarySeed();
			Math_SeedRandom(GameRand_GetSavedSeed());

			rowCount = 0;
			baseX = g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPoints[XWA_FG_POINT_START_1].x;
			baseY = -g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPoints[XWA_FG_POINT_START_1].y;
			baseZ = g_missionFlightGroups[g_currentFlightGroupIdx].fg.missionPoints[XWA_FG_POINT_START_1].z;

			if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft != 0) {
				do {
					int spawnX;
					int spawnY;
					int spawnZ;
					uint32_t objectIdx;

					for (;;) {
						spawnX = (GameRand() & 0x1ff) + baseX - 256;
						spawnY = (GameRand() & 0x1ff) + baseY - 256;
						spawnZ = (GameRand() & 0x1ff) + baseZ - 256;

						for (objectIdx = g_objScanStart; objectIdx < g_regionStaticObjectSlotEnd;
							 ++objectIdx) {
							ObjectRecord* object;

							object = &g_objectTable[objectIdx];
							if (object->objectType != OBJ_None && spawnX == object->world_x &&
								spawnY == object->world_y && spawnZ == object->world_z) {
								break;
							}
						}
						if (objectIdx >= g_regionStaticObjectSlotEnd) {
							break;
						}
					}

					g_preparedSpawnMissionX = spawnX;
					g_preparedSpawnMissionY = spawnY;
					g_preparedSpawnMissionZ = spawnZ;
					g_preparedSpawnYawByte = 0;
					g_preparedSpawnPitchByte = 0;
					g_preparedSpawnRollByte = 0;
					Mission_SpawnPreparedObject(
						g_currentFlightGroupIdx, GENUS_Asteroid,
						(ObjectTypeId)(uint8_t)(((int)(uint16_t)GameRand() % 6) - 33));
					++rowCount;
				} while (rowCount < g_missionFlightGroups[g_currentFlightGroupIdx].fg.numberOfCraft);
			}

			GameRand_SetSavedSeed((uint16_t)GameRand());
			GameRand_RestorePrimarySeed();
			return;
		}
		case GENUS_Mine: {
			int xStep;
			int yStep;
			int zColStep;
			int zRowStep;
			int numberOfCraft;
			int ordinal;
			int rowZ;

			xStep = 0;
			yStep = 0;
			zColStep = 0;
			zRowStep = 0;
			switch (g_missionFlightGroups[flightGroupIdx].fg.status1 & 3u) {
				case 0:
					xStep = 64;
					yStep = 64;
					break;
				case 1:
					yStep = 64;
					zColStep = 64;
					break;
				case 2:
					xStep = 64;
					zRowStep = 64;
					break;
			}

			numberOfCraft = g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft;
			baseX = (int)g_missionFlightGroups[flightGroupIdx].fg.missionPoints[XWA_FG_POINT_START_1].x -
					(int)(((uint32_t)((numberOfCraft - 1) * xStep)) >> 1);
			baseY = -((int)g_missionFlightGroups[flightGroupIdx].fg.missionPoints[XWA_FG_POINT_START_1].y +
					  (int)(((uint32_t)((numberOfCraft - 1) * yStep)) >> 1));
			baseZ = (int)g_missionFlightGroups[flightGroupIdx].fg.missionPoints[XWA_FG_POINT_START_1].z -
					(int)(((uint32_t)((numberOfCraft - 1) * zColStep)) >> 1) -
					(int)(((uint32_t)((numberOfCraft - 1) * zRowStep)) >> 1);

			g_preparedSpawnYawByte = g_missionFlightGroups[flightGroupIdx].fg.yaw;
			g_preparedSpawnPitchByte = g_missionFlightGroups[flightGroupIdx].fg.pitch;
			g_preparedSpawnRollByte = g_missionFlightGroups[flightGroupIdx].fg.roll;

			ordinal = 0;
			rowCount = 0;
			if ((unsigned int)numberOfCraft > 0u) {
				rowZ = 0;
				do {
					int colCount;
					int x;
					int colZ;

					colCount = 0;
					if (numberOfCraft > 0) {
						x = baseX;
						colZ = 0;
						do {
							if ((instanceFilter == 0xffffu || instanceFilter == ordinal) &&
								g_missionFgStats[flightGroupIdx].outcomeCount[1] <
									g_missionFgStats[flightGroupIdx].outcomeCount[0]) {
								g_preparedSpawnMissionY = baseY;
								g_preparedSpawnMissionX = x;
								g_preparedSpawnMissionZ = baseZ + rowZ + colZ;
								Mission_SpawnPreparedObject(flightGroupIdx, GENUS_Mine, objectType);
								flightGroupIdx = g_currentFlightGroupIdx;
							}

							x += xStep;
							++ordinal;
							colZ += zColStep;
							++colCount;
						} while (colCount < g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft);
					}

					baseY += yStep;
					rowZ += zRowStep;
					++rowCount;
					numberOfCraft = g_missionFlightGroups[flightGroupIdx].fg.numberOfCraft;
				} while (rowCount < (unsigned int)numberOfCraft);
			}
			return;
		}
		default:
			return;
	}
}

// FUNCTION: XWA 0x4945E0
void Mission_CreateRegionMarkerObjects(int worldX, int worldY, int worldZ) {
	uint8_t regionCursor;
	int regionIdx;
	unsigned int objectIdx;
	int markerWorldY;
	int markerWorldZ;

	regionCursor = 0;
	markerWorldZ = worldZ;
	markerWorldY = worldY;
	regionIdx = 0;
	while (regionIdx < g_missionRegionCount) {
		Mission_SetActiveRegionObjectRanges(regionIdx);
		objectIdx = Object_AllocSlotForGenus(GENUS_TextureSprite);
		if (objectIdx == 0xffff) {
			/* Pool exhausted: bail out without restoring region 0, as the
			 * original does. */
			return;
		}

		g_objectTable[objectIdx].objectSignature = 0x6789;
		g_objectTable[objectIdx].world_x = worldX;
		g_objectTable[objectIdx].world_y = markerWorldY;
		g_objectTable[objectIdx].world_z = markerWorldZ;
		g_objectTable[objectIdx].regionIdx = regionCursor;
		g_objectTable[objectIdx].objectType = OBJ_ExplosionTextureGroup2000;
		++regionCursor;
		regionIdx = regionCursor;
		g_objectTable[objectIdx].genusId = GENUS_TextureSprite;
		g_objectTable[objectIdx].mobj->state = 5;
		g_objectTable[objectIdx].typeSpecificByte[0] = 2;
		g_objectTable[objectIdx].mobj->speed = 0;
		g_objectTable[objectIdx].mobj->instanceExtent =
			g_modelTypeTable[OBJ_ExplosionTextureGroup2000].maxBoundsExtent;
		g_objectTable[objectIdx].mobj->framesAlive = 0;
		g_objectTable[objectIdx].mobj->lifetimeTimer = 0;
		g_objectTable[objectIdx].pitch = 0;
		g_objectTable[objectIdx].yaw = 0;
		g_objectTable[objectIdx].roll = 0;
		g_objectTable[objectIdx].angleD = 0;
		g_objectTable[objectIdx].mobj->orientMatrixDirty = 0;
		g_objectTable[objectIdx].mobj->moveVectorDirty = 0;
	}
	Mission_SetActiveRegionObjectRanges(0);
}

// FUNCTION: XWA 0x421990
int Mission_FreeOverrideStringHandles(void) {
	int teamIdx;
	int goalIdx;
	int conditionIdx;
	int textIdx;
	int flightGroupIdx;
	int groupIdx;
	int slotIdx;

	for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups; ++flightGroupIdx) {
		for (groupIdx = 0; groupIdx < 8; ++groupIdx) {
			for (slotIdx = 0; slotIdx < 3; ++slotIdx) {
				MemoryHandle* handle;

				handle = &g_missionFgOverrideStringHandles[flightGroupIdx][groupIdx][slotIdx];
				if (*handle != 0) {
					Memory_FreeHandle("OVERRIDESTRING", *handle);
					*handle = 0;
				}
			}
		}
	}

	for (flightGroupIdx = 0; flightGroupIdx < (int16_t)g_missionHeader.numFlightGroups; ++flightGroupIdx) {
		for (groupIdx = 0; groupIdx < 4; ++groupIdx) {
			for (slotIdx = 0; slotIdx < 4; ++slotIdx) {
				MemoryHandle* handle;

				handle = &g_missionOrderStringHandles[flightGroupIdx][groupIdx][slotIdx];
				if (*handle != 0) {
					Memory_FreeHandle("AIOVERRIDESTRING", *handle);
					*handle = 0;
				}
			}
		}
	}

	for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
		for (goalIdx = 0; goalIdx < 7; ++goalIdx) {
			for (conditionIdx = 0; conditionIdx < 4; ++conditionIdx) {
				for (textIdx = 0; textIdx < 3; ++textIdx) {
					MemoryHandle* handle;

					handle = &g_globalGoalOverrideStringHandles[teamIdx][goalIdx][conditionIdx][textIdx];
					if (*handle != 0) {
						Memory_FreeHandle("GLOBALOVERRIDESTRING", *handle);
						*handle = 0;
					}
				}
			}
		}
	}

	return 1;
}

// FUNCTION: XWA 0x415660
void Mission_FreeObjectStorageHandles(void) {
	if (g_objectTableHandle != 0) {
		Memory_UnlockHandle(g_objectTableHandle);
		Memory_FreeHandle("OBJECTSHANDLE", g_objectTableHandle);
		g_objectTableHandle = 0;
	}

	if (g_mobileObjectPoolHandle != 0) {
		Memory_UnlockHandle(g_mobileObjectPoolHandle);
		Memory_FreeHandle("MOBILEOBJHANDLE", g_mobileObjectPoolHandle);
		g_mobileObjectPoolHandle = 0;
	}

	if (g_mobileObjectCharDataHandle != 0) {
		Memory_UnlockHandle(g_mobileObjectCharDataHandle);
		Memory_FreeHandle("CHARSHANDLE", g_mobileObjectCharDataHandle);
		g_mobileObjectCharDataHandle = 0;
	}

	if (g_craftDataPoolHandle != 0) {
		Memory_UnlockHandle(g_craftDataPoolHandle);
		Memory_FreeHandle("CRAFTSHANDLE", g_craftDataPoolHandle);
		g_craftDataPoolHandle = 0;
	}

	if (g_warheadGuidancePoolHandle != 0) {
		Memory_UnlockHandle(g_warheadGuidancePoolHandle);
		Memory_FreeHandle("WARHEADSHANDLE", g_warheadGuidancePoolHandle);
		g_warheadGuidancePoolHandle = 0;
	}

#ifdef XWA_MODERN
	/* The original leaves the locked base pointers dangling after the frees
	 * above; nothing in recovered code reads them between missions, but port
	 * code (snapshot capture, overlays) can tick before Mission_Init
	 * reallocates. Clear them so a stale read fails loudly on NULL instead. */
	g_objectTable = NULL;
	g_mobileObjectPoolBase = NULL;
	g_mobileObjectCharDataPool = NULL;
	g_craftDataPoolBase = NULL;
	g_warheadGuidancePoolBase = NULL;
#endif
}

// FUNCTION: XWA 0x415760
uint16_t Mission_Init(char* fileName) {
	uint16_t lastNonWhitespace;
	uint16_t scanPos;
	uint16_t slot;
	uint32_t mobileObjectCount;
	uint32_t craftPoolIdx;
	uint32_t mobilePoolIdx;
	uint32_t warheadPoolIdx;
	uint16_t fgIdx;
	int selectedCraftType;
	int selectedWarhead;
	int teamPlayerOwnerCounts[10];
	int teamPlayerFgCounts[10];
	int teamOwnedFg[10];
	int teamIdx;
	uint8_t timeLimitActive;
#ifdef XWA_MODERN
	lastNonWhitespace = 0;
#endif

	for (scanPos = 0; scanPos < strlen(fileName); ++scanPos) {
		char ch;

		ch = fileName[scanPos];
		if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
			lastNonWhitespace = scanPos;
		}
	}
	fileName[lastNonWhitespace + 1u] = 0;

	g_missionRegionCount = g_pilotData.regionsCount;
	if (g_provingGroundsModeActive) {
		g_craftObjectSlotsTotal = 36u * (uint32_t)g_pilotData.regionsCount;
		g_salvageJunkObjectSlotsTotal = 224u * (uint32_t)g_pilotData.regionsCount;
		g_salvageJunkObjectSlotsPerRegion = 224;
		g_craftObjectSlotsPerRegion = g_craftObjectSlotsTotal / (uint32_t)g_pilotData.regionsCount;
	} else {
		g_craftObjectSlotsPerRegion = 96;
		g_craftObjectSlotsTotal = 96u * (uint32_t)g_pilotData.regionsCount;
		g_salvageJunkObjectSlotsTotal = 0;
		g_salvageJunkObjectSlotsPerRegion = 0;
	}

	g_sharedPlayerProjectileSlotsPerRegion = 32;
	g_playerProjectileSlotsTotal = 12u * (uint32_t)g_flightPlayerCount;
	if (g_flightPlayerCount == 1) {
		g_sharedPlayerProjectileSlotsPerRegion = 16;
	}
	g_projectileObjectSlotsPerRegion =
		g_sharedPlayerProjectileSlotsPerRegion + 12u * (uint32_t)g_flightPlayerCount + 64u;
	g_projectileObjectSlotsTotal = g_projectileObjectSlotsPerRegion * (uint32_t)g_pilotData.regionsCount;
	g_explosionObjectSlotsTotal = 32u * (uint32_t)g_pilotData.regionsCount;
	g_regionMainObjectSlotsTotal = g_craftObjectSlotsTotal + g_projectileObjectSlotsTotal +
								   32u * (uint32_t)g_pilotData.regionsCount +
								   32u * (uint32_t)g_pilotData.regionsCount + g_salvageJunkObjectSlotsTotal;
	g_debrisObjectSlotsTotal = 32u * (uint32_t)g_pilotData.regionsCount;
	g_localDebrisObjectSlotsTotal = 8;
	g_localEffectObjectSlotsTotal = 24;
	g_mobileObjectCharDataCount = 0;
	g_mainObjectSlotsPerRegion = g_regionMainObjectSlotsTotal / (uint32_t)g_pilotData.regionsCount;
	g_regionStaticObjectSlotsTotal = (uint32_t)g_pilotData.regionsCount << 7;
	g_regionObjectSlotEnd = g_regionStaticObjectSlotsTotal + g_regionMainObjectSlotsTotal;
	g_localTransientSlotStart = g_regionObjectSlotEnd;
	g_objectTableSlotCount = g_regionObjectSlotEnd + 32;
	g_objectSlotsPerRegion = (g_objectTableSlotCount - 32) / (uint32_t)g_pilotData.regionsCount;
	g_localDebrisSlotEnd = g_regionObjectSlotEnd + 8;
	g_localEffectSlotStart = g_localDebrisSlotEnd;
	g_localTransientSlotEnd = g_localEffectSlotStart + 24;
	Mission_SetActiveRegionObjectRanges(0);

	if (g_objectTableHandle != 0) {
		Memory_UnlockHandle(g_objectTableHandle);
		Memory_FreeHandle("OBJECTSHANDLE", g_objectTableHandle);
		g_objectTableHandle = 0;
	}
	if (g_mobileObjectPoolHandle != 0) {
		Memory_UnlockHandle(g_mobileObjectPoolHandle);
		Memory_FreeHandle("MOBILEOBJHANDLE", g_mobileObjectPoolHandle);
		g_mobileObjectPoolHandle = 0;
	}
	if (g_mobileObjectCharDataHandle != 0) {
		Memory_UnlockHandle(g_mobileObjectCharDataHandle);
		Memory_FreeHandle("CHARSHANDLE", g_mobileObjectCharDataHandle);
		g_mobileObjectCharDataHandle = 0;
	}
	if (g_craftDataPoolHandle != 0) {
		Memory_UnlockHandle(g_craftDataPoolHandle);
		Memory_FreeHandle("CRAFTSHANDLE", g_craftDataPoolHandle);
		g_craftDataPoolHandle = 0;
	}
	if (g_warheadGuidancePoolHandle != 0) {
		Memory_UnlockHandle(g_warheadGuidancePoolHandle);
		Memory_FreeHandle("WARHEADSHANDLE", g_warheadGuidancePoolHandle);
		g_warheadGuidancePoolHandle = 0;
	}

	g_objectTableHandle =
		Memory_AllocHandleZeroed("OBJECTSHANDLE", sizeof(ObjectRecord) * (size_t)g_objectTableSlotCount);
	g_mobileObjectPoolHandle = Memory_AllocHandleZeroed(
		"MOBILEOBJHANDLE",
		sizeof(MobileObject) * (size_t)(g_localDebrisObjectSlotsTotal + g_localEffectObjectSlotsTotal +
										g_regionMainObjectSlotsTotal));
	g_craftDataPoolHandle =
		Memory_AllocHandleZeroed("CRAFTSHANDLE", sizeof(CraftData) * (size_t)g_craftObjectSlotsTotal);
	g_warheadGuidancePoolHandle = Memory_AllocHandleZeroed(
		"WARHEADSHANDLE", sizeof(WarheadGuidanceState) * (size_t)(g_projectileObjectSlotsTotal + 1));
	if (g_objectTableHandle != 0) {
		g_objectTable = (ObjectRecord*)Memory_LockHandle(g_objectTableHandle);
	}
	if (g_mobileObjectPoolHandle != 0) {
		g_mobileObjectPoolBase = (MobileObject*)Memory_LockHandle(g_mobileObjectPoolHandle);
	}
	if (g_mobileObjectCharDataHandle != 0) {
		g_mobileObjectCharDataPool = (MobileObjectCharData*)Memory_LockHandle(g_mobileObjectCharDataHandle);
	}
	if (g_craftDataPoolHandle != 0) {
		g_craftDataPoolBase = (CraftData*)Memory_LockHandle(g_craftDataPoolHandle);
	}
	if (g_warheadGuidancePoolHandle != 0) {
		g_warheadGuidancePoolBase = (WarheadGuidanceState*)Memory_LockHandle(g_warheadGuidancePoolHandle);
	}

	for (slot = 0; slot < g_regionMainObjectSlotsTotal; ++slot) {
		g_mobileObjectPoolBase[slot].framesAlive = 0;
		g_mobileObjectPoolBase[slot].lifetimeTimer = 0;
		g_mobileObjectPoolBase[slot].simStateTimestamp = 0;
		g_mobileObjectPoolBase[slot].orientMatrixDirty = 0;
		g_mobileObjectPoolBase[slot].pCraft = NULL;
		g_mobileObjectPoolBase[slot].pWarheadGuidance = NULL;
		g_mobileObjectPoolBase[slot].pCharData = NULL;
	}

	mobileObjectCount = 0;
	craftPoolIdx = 0;
	mobilePoolIdx = 0;
	warheadPoolIdx = 0;
	for (slot = 0; slot < g_regionObjectSlotEnd; ++slot) {
		uint32_t slotInRegion;

		g_objectTable[slot].objectType = OBJ_None;
		g_objectTable[slot].playerOwnerIdx = -1;
		slotInRegion = slot % g_objectSlotsPerRegion;
		if (slotInRegion >= g_mainObjectSlotsPerRegion) {
			g_objectTable[slot].mobj = NULL;
		} else {
			MobileObject* mobj;

			mobj = &g_mobileObjectPoolBase[mobilePoolIdx++];
			g_objectTable[slot].mobj = mobj;
			++mobileObjectCount;
			if (slotInRegion < g_craftObjectSlotsPerRegion) {
				mobj->iff = -1;
				mobj->pCraft = &g_craftDataPoolBase[craftPoolIdx++];
			} else if (slotInRegion >= g_projectileObjectSlotStart &&
					   slotInRegion < g_projectileObjectSlotEnd) {
				mobj->pWarheadGuidance = &g_warheadGuidancePoolBase[warheadPoolIdx++];
			}
		}
	}

	for (slot = g_localTransientSlotStart; slot < g_localTransientSlotEnd; ++slot) {
		MobileObject* mobj;

		mobj = &g_mobileObjectPoolBase[mobileObjectCount++];
		g_objectTable[slot].mobj = mobj;
		if (slot < g_localDebrisSlotEnd) {
			g_objectTable[slot].genusId = GENUS_Debris;
			g_objectTable[slot].mobj->state = 3;
			g_objectTable[slot].flightGroupIdx = -1;
		}
	}

	for (slot = 0; slot < 32; ++slot) {
		g_modelTextureOverrideSlots[slot].modelType = 0;
		g_modelTextureOverrideSlots[slot].textureNode = NULL;
	}
	g_modelTextureOverrideNextSlot = 0;

	for (fgIdx = 0; fgIdx < 8; ++fgIdx) {
		g_players[fgIdx].objectIndex = 0xffff;
		g_players[fgIdx].altViewObjectIdx = 0xffff;
		g_players[fgIdx].boundObjectSignature = 0;
		g_players[fgIdx].regionIndex = 0;
	}

	for (slot = 0; slot < OBJ_Count; ++slot) {
		if (g_modelTypeTable[slot].recordFlags != 0) {
			g_modelTypeTable[slot].assetFlags &= (uint8_t)~MISSION_MODEL_ASSET_REQUIRED;
		}
	}

	for (fgIdx = 0; fgIdx < 64; ++fgIdx) {
		g_missionMessages[fgIdx].message[0] = 0;
	}
	memset(&g_missionGlobalUnitCraftCount[1], 0, sizeof(g_missionGlobalUnitCraftCount) - sizeof(int));
	memset(g_missionFlightRuntimeState.teamHasCountableCraft, 0,
		   sizeof(g_missionFlightRuntimeState.teamHasCountableCraft));
	g_missionStateByte8053E6 = 0;
	g_yardChallengeMode = 0;
	g_unusedMissionInitStateDword805406 = 0;
	g_unusedMissionInitStateWord0 = 0;
	g_unusedMissionInitStateWord1 = 0;
	g_unusedMissionInitStateWord2 = 0;
	g_unusedMissionInitStateWord3 = 0;
	g_unusedMissionInitStateWord4 = 0;
	g_unusedMissionInitStateWord5 = 0;
	g_activeMissionRegionCount = g_missionRegionCount - 1;

	if (!Mission_LoadFile(fileName)) {
		return 0;
	}

	if (g_pilotData.campaignMode) {
		for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
			uint8_t craftType;

			craftType = g_missionFlightGroups[fgIdx].fg.craftType;
			if (craftType != 0xb6 && g_pilotData.craftKnown[craftType] == 0) {
				g_pilotData.newCraftAddedToTechRoom = 1;
				g_pilotData.craftKnown[craftType] = 2;
			}
		}
	}

	for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
		g_missionFlightGroups[fgIdx].playerOwnerIdx = -1;
	}

	if (!g_flightConfNoPilot) {
		selectedCraftType = 0;
		selectedWarhead = 0;
		for (fgIdx = 0; fgIdx < 8; ++fgIdx) {
			if (g_pilotData.networkPlayers[fgIdx].directPlayId != 0) {
				int selectedFg;

				selectedFg = g_pilotData.networkPlayers[fgIdx].flightGroupId;
				selectedCraftType = g_missionFlightGroups[selectedFg].fg.craftType;
				selectedWarhead = g_missionFlightGroups[selectedFg].fg.warhead;
				break;
			}
		}

		for (fgIdx = 0; fgIdx < 8; ++fgIdx) {
			PilotNetworkPlayer* networkPlayer;

			networkPlayer = &g_pilotData.networkPlayers[fgIdx];
			if (networkPlayer->directPlayId != 0) {
				uint16_t flightGroupId;
				unsigned int playerSlot;

				flightGroupId = (uint16_t)networkPlayer->flightGroupId;
				playerSlot =
					g_filmPlaybackMode ? 0u : NetSession_FindPlayerSlotByDpid(networkPlayer->directPlayId);
				if (playerSlot < 8u) {
					g_players[playerSlot].iff = g_missionFlightGroups[flightGroupId].fg.iff;
					g_players[playerSlot].playerIff = g_missionFlightGroups[flightGroupId].fg.team;
					if (networkPlayer->m20 == 0) {
						g_missionFlightGroups[flightGroupId].playerOwnerIdx = (int)playerSlot;
						g_missionFlightGroups[flightGroupId].fg.craftType = (uint8_t)networkPlayer->craftId;
						g_missionFlightGroups[flightGroupId].fg.warhead = (uint8_t)networkPlayer->warheadType;
						g_missionFlightGroups[flightGroupId].fg.beam = (uint8_t)networkPlayer->beamType;
						g_missionFlightGroups[flightGroupId].fg.countermeasures =
							(uint8_t)networkPlayer->counterMeasuresType;
						g_missionFlightGroups[flightGroupId].fg.numberOfCraft =
							(uint8_t)networkPlayer->craftsCount;
						g_missionFlightGroups[flightGroupId].fg.numberOfWaves =
							(uint8_t)networkPlayer->wavesCount;
					}
				}
			}
		}

		memset(teamPlayerOwnerCounts, 0, sizeof(teamPlayerOwnerCounts));
		for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
			if (g_missionFlightGroups[fgIdx].playerOwnerIdx != -1) {
				++teamPlayerOwnerCounts[g_missionFlightGroups[fgIdx].fg.team];
			}
		}
		teamIdx = 0;
		for (fgIdx = 0; fgIdx < 10; ++fgIdx) {
			if (teamPlayerOwnerCounts[fgIdx] != 0) {
				++teamIdx;
			}
		}
		if ((g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
			 g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) &&
			teamIdx == 1) {
			g_aiOpponentsEnabled = 1;
		}

		if ((g_gameConfig.craftSelection == 2 || g_gameConfig.craftSelection == 1) &&
			g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART &&
			g_pilotData.numHumanPlayersLastMission == 1) {
			uint16_t sourceFgIdx;

			sourceFgIdx = 0;
			for (fgIdx = 0; fgIdx < 8; ++fgIdx) {
				if (g_pilotData.networkPlayers[fgIdx].directPlayId != 0) {
					sourceFgIdx = (uint16_t)g_pilotData.networkPlayers[fgIdx].flightGroupId;
					break;
				}
			}
			for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
				if (g_missionFlightGroups[fgIdx].fg.playerNumber != 0) {
					if (g_missionFlightGroups[fgIdx].fg.craftType == selectedCraftType &&
						g_missionFlightGroups[fgIdx].fg.warhead == selectedWarhead) {
						g_missionFlightGroups[fgIdx].fg.warhead =
							g_missionFlightGroups[sourceFgIdx].fg.warhead;
					}
					g_missionFlightGroups[fgIdx].fg.craftType =
						g_missionFlightGroups[sourceFgIdx].fg.craftType;
					g_missionFlightGroups[fgIdx].fg.numberOfCraft =
						g_missionFlightGroups[sourceFgIdx].fg.numberOfCraft;
					g_missionFlightGroups[fgIdx].fg.numberOfWaves =
						g_missionFlightGroups[sourceFgIdx].fg.numberOfWaves;
					g_missionFlightGroups[fgIdx].fg.beam = g_missionFlightGroups[sourceFgIdx].fg.beam;
					g_missionFlightGroups[fgIdx].fg.countermeasures =
						g_missionFlightGroups[sourceFgIdx].fg.countermeasures;
				}
			}
		} else if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART && g_gameConfig.craftSelection == 1) {
			int ownedTeamCount;

			memset(teamOwnedFg, 0, sizeof(teamOwnedFg));
			memset(teamPlayerOwnerCounts, 0, sizeof(teamPlayerOwnerCounts));
			memset(teamPlayerFgCounts, 0, sizeof(teamPlayerFgCounts));
			for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
				if (g_missionFlightGroups[fgIdx].fg.playerNumber != 0) {
					++teamPlayerFgCounts[g_missionFlightGroups[fgIdx].fg.team];
					if (g_missionFlightGroups[fgIdx].playerOwnerIdx != -1) {
						teamOwnedFg[g_missionFlightGroups[fgIdx].fg.team] = fgIdx;
						++teamPlayerOwnerCounts[g_missionFlightGroups[fgIdx].fg.team];
					}
				}
			}

			ownedTeamCount = 0;
			for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
				if (teamPlayerOwnerCounts[teamIdx] != 0) {
					++ownedTeamCount;
				}
			}
			for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
				if (g_missionFlightGroups[fgIdx].fg.playerNumber != 0 &&
					g_missionFlightGroups[fgIdx].playerOwnerIdx == -1 &&
					teamPlayerOwnerCounts[g_missionFlightGroups[fgIdx].fg.team] != 0) {
					int sourceFgIdx;

					sourceFgIdx = teamOwnedFg[g_missionFlightGroups[fgIdx].fg.team];
					g_missionFlightGroups[fgIdx].fg.craftType =
						g_missionFlightGroups[sourceFgIdx].fg.craftType;
					g_missionFlightGroups[fgIdx].fg.numberOfCraft =
						g_missionFlightGroups[sourceFgIdx].fg.numberOfCraft;
					g_missionFlightGroups[fgIdx].fg.numberOfWaves =
						g_missionFlightGroups[sourceFgIdx].fg.numberOfWaves;
					g_missionFlightGroups[fgIdx].fg.warhead = g_missionFlightGroups[sourceFgIdx].fg.warhead;
					g_missionFlightGroups[fgIdx].fg.beam = g_missionFlightGroups[sourceFgIdx].fg.beam;
					g_missionFlightGroups[fgIdx].fg.countermeasures =
						g_missionFlightGroups[sourceFgIdx].fg.countermeasures;
				}
			}

			if (g_aiOpponentsEnabled == 1) {
				for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
					if (teamPlayerFgCounts[teamIdx] != 0 && teamPlayerOwnerCounts[teamIdx] == 0) {
						uint16_t selectedOwnedTeamOrdinal;
						int ordinal;
						int sourceTeam;
						int sourceFgIdx;

						selectedOwnedTeamOrdinal =
							ownedTeamCount != 0 ? (uint16_t)((uint16_t)GameRand() % ownedTeamCount) : 0;
						ordinal = 0;
						sourceTeam = 0;
						for (; sourceTeam < 10; ++sourceTeam) {
							if (teamPlayerFgCounts[sourceTeam] != 0 &&
								teamPlayerOwnerCounts[sourceTeam] != 0) {
								if (ordinal == selectedOwnedTeamOrdinal) {
									break;
								}
								++ordinal;
							}
						}
						sourceFgIdx = teamOwnedFg[sourceTeam];
						if (sourceFgIdx != g_missionHeader.numFlightGroups) {
							for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
								if (g_missionFlightGroups[fgIdx].fg.team == teamIdx &&
									g_missionFlightGroups[fgIdx].fg.playerNumber != 0) {
									g_missionFlightGroups[fgIdx].fg.craftType =
										g_missionFlightGroups[sourceFgIdx].fg.craftType;
									g_missionFlightGroups[fgIdx].fg.numberOfCraft =
										g_missionFlightGroups[sourceFgIdx].fg.numberOfCraft;
									g_missionFlightGroups[fgIdx].fg.numberOfWaves =
										g_missionFlightGroups[sourceFgIdx].fg.numberOfWaves;
									g_missionFlightGroups[fgIdx].fg.warhead =
										g_missionFlightGroups[sourceFgIdx].fg.warhead;
									g_missionFlightGroups[fgIdx].fg.beam =
										g_missionFlightGroups[sourceFgIdx].fg.beam;
									g_missionFlightGroups[fgIdx].fg.countermeasures =
										g_missionFlightGroups[sourceFgIdx].fg.countermeasures;
								}
							}
						}
					}
				}
			}
		}
	} else {
		for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
			int playerNumber;

			playerNumber = g_missionFlightGroups[fgIdx].fg.playerNumber;
			if (playerNumber != 0 && playerNumber <= g_activeFlightPlayerCount) {
				int playerIdx;

				playerIdx = playerNumber - 1;
				g_missionFlightGroups[fgIdx].playerOwnerIdx = playerIdx;
				g_players[playerIdx].iff = g_missionFlightGroups[fgIdx].fg.iff;
				g_players[playerIdx].playerIff = g_missionFlightGroups[fgIdx].fg.team;
			} else {
				g_missionFlightGroups[fgIdx].playerOwnerIdx = -1;
			}
		}
	}

	if (g_missionRandomVariationEnabled) {
		for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
			if (g_missionFlightGroups[fgIdx].playerOwnerIdx == -1 &&
				g_missionFlightGroups[fgIdx].fg.optionalCraftCategory == 4) {
				unsigned int optionCount;
				unsigned int selectedOption;

				optionCount = 1;
				for (slot = 0; slot < 10; ++slot) {
					if (g_missionFlightGroups[fgIdx].fg.optionalCraft[slot] != 0) {
						++optionCount;
					}
				}
				selectedOption = optionCount != 0 ? (uint16_t)GameRand() % optionCount : 0;
				if (optionCount > 1 && selectedOption != 0) {
					unsigned int ordinal;

					ordinal = 1;
					for (slot = 0; slot < 10; ++slot) {
						if (g_missionFlightGroups[fgIdx].fg.optionalCraft[slot] != 0) {
							if (selectedOption == ordinal) {
								break;
							}
							++ordinal;
						}
					}
					if (g_missionFlightGroups[fgIdx].fg.optionalCraft[slot] != 0) {
						g_missionFlightGroups[fgIdx].fg.craftType =
							g_missionFlightGroups[fgIdx].fg.optionalCraft[slot];
						g_missionFlightGroups[fgIdx].fg.numberOfCraft =
							g_missionFlightGroups[fgIdx].fg.numberOfOptionalCraft[slot];
						g_missionFlightGroups[fgIdx].fg.numberOfWaves =
							g_missionFlightGroups[fgIdx].fg.numberOfOptionalCraftWaves[slot];
					}
				}
			}
		}
	}

	if (g_flightDifficulty != 1 && g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] != 36) {
		int playerIff;

		playerIff = (uint16_t)g_players[0].playerIff;
		for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
			if (g_flightDifficulty == 2) {
				if (Team_IsHostileToTeam(playerIff, g_missionFlightGroups[fgIdx].fg.team)) {
					if (++g_missionFlightGroups[fgIdx].fg.groupAI > 5u) {
						g_missionFlightGroups[fgIdx].fg.groupAI = 5;
					}
				}
			} else if (Team_IsFriendlyToTeam(playerIff, g_missionFlightGroups[fgIdx].fg.team)) {
				g_missionFlightGroups[fgIdx].fg.groupAI += 2;
				if (g_missionFlightGroups[fgIdx].fg.groupAI > 5u) {
					g_missionFlightGroups[fgIdx].fg.groupAI = 5;
				}
			} else if (g_missionFlightGroups[fgIdx].fg.groupAI != 0) {
				--g_missionFlightGroups[fgIdx].fg.groupAI;
			}
		}
	}

	if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART) {
		int teamsWithoutOwnedPlayers;
		int chosenOrdinal;
		int chosenTeam;
		int aiBoostCount;

		memset(teamPlayerFgCounts, 0, sizeof(teamPlayerFgCounts));
		memset(teamPlayerOwnerCounts, 0, sizeof(teamPlayerOwnerCounts));
		for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
			if (g_missionFlightGroups[fgIdx].fg.playerNumber != 0) {
				++teamPlayerFgCounts[g_missionFlightGroups[fgIdx].fg.team];
			}
			if (g_missionFlightGroups[fgIdx].playerOwnerIdx != -1) {
				++teamPlayerOwnerCounts[g_missionFlightGroups[fgIdx].fg.team];
			}
		}
		teamsWithoutOwnedPlayers = 0;
		for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
			if (teamPlayerFgCounts[teamIdx] != 0 && teamPlayerOwnerCounts[teamIdx] == 0) {
				++teamsWithoutOwnedPlayers;
			}
		}
		chosenOrdinal = teamsWithoutOwnedPlayers != 0 ? GameRand() % teamsWithoutOwnedPlayers : 0;
#ifdef XWA_MODERN
		chosenTeam = 0;
#endif
		teamIdx = -1;
		for (fgIdx = 0; fgIdx < 10; ++fgIdx) {
			if (teamPlayerFgCounts[fgIdx] != 0 && teamPlayerOwnerCounts[fgIdx] == 0 &&
				++teamIdx == chosenOrdinal) {
				chosenTeam = fgIdx;
				break;
			}
		}

		aiBoostCount = g_flightDifficulty != 0 ? 2 - (g_flightDifficulty != 1) : 3;
		for (teamIdx = 0; teamIdx < aiBoostCount; ++teamIdx) {
			int targetTeam;

			targetTeam = chosenTeam + teamIdx;
			for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
				if (g_missionFlightGroups[fgIdx].fg.playerNumber != 0 &&
					g_missionFlightGroups[fgIdx].playerOwnerIdx == -1 &&
					g_missionFlightGroups[fgIdx].fg.team == targetTeam &&
					g_missionFlightGroups[fgIdx].fg.groupAI < 5u) {
					++g_missionFlightGroups[fgIdx].fg.groupAI;
					break;
				}
			}
		}
	}

	for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
		ObjectTypeId objectType;
		ModelIndex modelIndex;

		if (g_missionFlightGroups[fgIdx].playerOwnerIdx != -1) {
			if (g_missionFlightGroups[fgIdx].fg.numberOfCraft > 1u) {
				if (g_missionFlightGroups[fgIdx].fg.playerCraft == 0) {
					g_missionFlightGroups[fgIdx].fg.playerCraft = 1;
				}
			} else {
				g_missionFlightGroups[fgIdx].fg.playerCraft = 0;
			}
		}
		if (g_missionFlightGroups[fgIdx].fg.playerNumber != 0 &&
			(g_missionFlightGroups[fgIdx].playerOwnerIdx != -1 ||
			 g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
			 g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH)) {
			if (g_playerFlightGroupWaveMode == 0) {
				g_missionFlightGroups[fgIdx].fg.numberOfWaves = 0;
			} else if (g_playerFlightGroupWaveMode == 2) {
				g_missionFlightGroups[fgIdx].fg.numberOfWaves = 99;
			}
		}

		objectType =
			(ObjectTypeId)g_objectTypeTables.craftTypeToObjectType[g_missionFlightGroups[fgIdx].fg.craftType];
		g_modelTypeTable[objectType].assetFlags |= MISSION_MODEL_ASSET_REQUIRED;
		{
			const uint16_t* companionModelTypes;

			companionModelTypes = g_objectTypeTables.craftCompanionModelTypes;
			for (slot = 0; companionModelTypes[0] != OBJ_None; ++slot, companionModelTypes += 2) {
				if ((ObjectTypeId)companionModelTypes[0] == objectType) {
					g_modelTypeTable[companionModelTypes[1]].assetFlags |= MISSION_MODEL_ASSET_REQUIRED;
					break;
				}
			}
		}
		if (objectType == OBJ_AsteroidHR1) {
			for (slot = 1; slot <= 5; ++slot) {
				g_modelTypeTable[OBJ_AsteroidHR1 + slot].assetFlags |= MISSION_MODEL_ASSET_REQUIRED;
			}
		}
		if (objectType == OBJ_CrewCabinFront) {
			g_modelTypeTable[OBJ_ConnectorRod].assetFlags |= MISSION_MODEL_ASSET_REQUIRED;
			g_modelTypeTable[OBJ_EngineBack].assetFlags |= MISSION_MODEL_ASSET_REQUIRED;
		}
		if (objectType == OBJ_EngineFront) {
			g_modelTypeTable[OBJ_ConnectorRod].assetFlags |= MISSION_MODEL_ASSET_REQUIRED;
			g_modelTypeTable[OBJ_CrewCabinBack].assetFlags |= MISSION_MODEL_ASSET_REQUIRED;
		}

		modelIndex = GetModelIndexFromType(objectType);
		if (modelIndex != 0xffffu && g_modelDefs[modelIndex].turretModelIndex[0] != 0) {
			g_modelTypeTable[g_modelDefs[modelIndex].turretModelIndex[0]].assetFlags |=
				MISSION_MODEL_ASSET_REQUIRED;
		}

		if (g_missionFlightGroups[fgIdx].fg.randomSpecialCargoCraft != 0) {
			g_missionFlightGroups[fgIdx].fg.specialCargoCraft =
				g_missionFlightGroups[fgIdx].fg.numberOfCraft != 0
					? (uint8_t)((uint16_t)GameRand() % g_missionFlightGroups[fgIdx].fg.numberOfCraft)
					: 0;
		}

		g_missionFgStats[fgIdx].currentMissionPointRef = MISSION_MODERN_STRING_TAIL;
		if ((g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
			 g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) &&
			g_missionFlightGroups[fgIdx].fg.playerNumber != 0) {
			if (objectType == OBJ_TIEFighter || objectType == OBJ_TIEInterceptor ||
				objectType == OBJ_TIEBomber || objectType == OBJ_TIEAdvanced ||
				objectType == OBJ_TIEDefender) {
				if (++g_missionFlightGroups[fgIdx].fg.markings > 3u) {
					g_missionFlightGroups[fgIdx].fg.markings = 0;
				}
			}
			if (g_missionFlightGroups[fgIdx].fg.playerNumber != 0 &&
				g_missionFlightGroups[fgIdx].playerOwnerIdx == -1 && !g_aiOpponentsEnabled) {
				int hasHumanOnTeam;

				hasHumanOnTeam = 0;
				for (teamIdx = 0; teamIdx < (int16_t)g_missionHeader.numFlightGroups; ++teamIdx) {
					if (g_missionFlightGroups[teamIdx].fg.playerNumber != 0 &&
						g_missionFlightGroups[teamIdx].playerOwnerIdx != -1 &&
						g_missionFlightGroups[fgIdx].fg.team == g_missionFlightGroups[teamIdx].fg.team) {
						hasHumanOnTeam = 1;
						break;
					}
				}
				if (!hasHumanOnTeam) {
					g_missionFlightGroups[fgIdx].fg.arriveOnlyIfHuman = 1;
				}
			}
		}
	}

	GameRand_SavePrimarySeed();
	Math_SeedRandom((uint16_t)(g_missionHeader.body.legacyBackdrop - 16657));
	GameRand_SetSavedSeed(GameRand_GetPrimarySeed());
	GameRand_RestorePrimarySeed();

	memset(g_backdropCountByRegion, 0, sizeof(g_backdropCountByRegion));
	{
		DeathStarTunnelLaserRegionState* laser;

		laser = g_deathStarTunnelLaserRegions;
		for (teamIdx = 5; teamIdx != 0; --teamIdx, ++laser) {
			laser->enabled = 0;
			laser->shotActive = 0;
			laser->beamLightActive = 0;
			laser->alternateDelayPhase = 0;
			laser->shotStartGameTime = 0;
			laser->warmupTicks = 944;
			laser->holdTicks = 236;
			laser->travelTicks = 118;
			laser->targetObjIdx = 4095;
			laser->firstShotDelayTicks = 944;
			laser->repeatShotDelayTicks = 3540;
			laser->emitterRect = NULL;
			laser->beamSpriteRect = NULL;
		}
	}

	g_currentFlightGroupIdx = 0;
	while ((int16_t)g_currentFlightGroupIdx < (int16_t)g_missionHeader.numFlightGroups) {
		int backdropId;
		ObjectTypeId backdropType;

		backdropId = g_missionFlightGroups[g_currentFlightGroupIdx].fg.backdrop;
		if (backdropId != 0 &&
			(ObjectTypeId)g_objectTypeTables
					.craftTypeToObjectType[g_missionFlightGroups[g_currentFlightGroupIdx].fg.craftType] ==
				OBJ_BackdropTextureGroup9001_Sprite1100_263) {
			backdropType = (ObjectTypeId)g_objectTypeTables.missionBackdropDescriptors[backdropId].modelType;
			if (backdropType != OBJ_None && (g_modelTypeTable[(ObjectTypeId)backdropType].flags &
											 MODEL_TYPE_FLAG_SINGLE_MIP_LEVEL) != 0) {
				unsigned int backdropRegion;
				unsigned int recordIdx;
				unsigned int flatIdx;
				int absX;
				int absY;
				int scale;
				float backdropScale;
				float backdropIntensity;
				float backdropR;
				float backdropG;
				float backdropB;

				backdropRegion = g_missionFlightGroups[g_currentFlightGroupIdx]
									 .fg.missionPointRegions[XWA_FG_POINT_START_1];
				recordIdx = (unsigned int)g_backdropCountByRegion[backdropRegion];
				if (recordIdx < XWA_BACKDROP_RECORDS_PER_REGION) {
					flatIdx = recordIdx + XWA_BACKDROP_RECORDS_PER_REGION * backdropRegion;
					g_backdropRecordsByRegion[0][flatIdx].modelType = backdropType;
					g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x =
						g_missionFlightGroups[g_currentFlightGroupIdx]
							.fg.missionPoints[XWA_FG_POINT_START_1]
							.x;
					g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y =
						-g_missionFlightGroups[g_currentFlightGroupIdx]
							 .fg.missionPoints[XWA_FG_POINT_START_1]
							 .y;
					g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z =
						g_missionFlightGroups[g_currentFlightGroupIdx]
							.fg.missionPoints[XWA_FG_POINT_START_1]
							.z;
					if (abs(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x) > 1024) {
						scale = (g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x << 10) /
								g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x;
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x = scale;
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y =
							(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y << 10) / scale;
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z =
							(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z << 10) /
							g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x;
					}
					if (abs(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y) > 1024) {
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x =
							(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x << 10) /
							g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y;
						scale = (g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y << 10) /
								g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y;
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y = scale;
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z =
							(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z << 10) / scale;
					}
					if (abs(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z) > 1024) {
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x =
							(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x << 10) /
							g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z;
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y =
							(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y << 10) /
							g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z;
						scale = (g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z << 10) /
								g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z;
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z = scale;
					}
					absY = abs(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y);
					absX = abs(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x);
					if (absX <= absY && absY >= abs(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z)) {
						trig2_ctop(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x,
								   g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y,
								   g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z);
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x =
							(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x << 20) / trig2_polardistance;
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y =
							(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y << 20) / trig2_polardistance;
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z =
							(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z << 20) / trig2_polardistance;
						if (g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y > 0) {
							g_backdropRecordsByRegion[0][flatIdx].side = 0;
						} else {
							g_backdropRecordsByRegion[0][flatIdx].side = 1;
						}
					} else if (absX >= abs(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z)) {
						trig2_ctop(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x,
								   g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y,
								   g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z);
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x =
							(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x << 20) / trig2_polardistance;
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y =
							(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y << 20) / trig2_polardistance;
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z =
							(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z << 20) / trig2_polardistance;
						if (g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x > 0) {
							g_backdropRecordsByRegion[0][flatIdx].side = 3;
						} else {
							g_backdropRecordsByRegion[0][flatIdx].side = 2;
						}
					} else {
						int multiplier;

						if (g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z == 0) {
							g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z = 1;
						}
						multiplier = 0x100000 / abs(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z);
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x =
							(int)(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x * (double)multiplier);
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y =
							(int)(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y * (double)multiplier);
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z =
							(int)(g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z * (double)multiplier);
						if (g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z > 0) {
							g_backdropRecordsByRegion[0][flatIdx].side = 4;
						} else {
							g_backdropRecordsByRegion[0][flatIdx].side = 5;
						}
					}
					DebugPrintfChannel(
						1024,
						"Created backdrop %d : %d, side %d, from world %d, %d, %d to world %d, %d, %d.\n",
						backdropRegion, recordIdx, g_backdropRecordsByRegion[0][flatIdx].side,
						g_missionFlightGroups[g_currentFlightGroupIdx]
							.fg.missionPoints[XWA_FG_POINT_START_1]
							.x,
						-g_missionFlightGroups[g_currentFlightGroupIdx]
							 .fg.missionPoints[XWA_FG_POINT_START_1]
							 .y,
						g_missionFlightGroups[g_currentFlightGroupIdx]
							.fg.missionPoints[XWA_FG_POINT_START_1]
							.z,
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.x,
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.y,
						g_backdropRecordsByRegion[0][flatIdx].worldDirQ20.z);

					backdropScale = 1.0f;
					g_modelTypeTable[(ObjectTypeId)backdropType].assetFlags |= MISSION_MODEL_ASSET_REQUIRED;
					backdropB = 0.0f;
					backdropG = 0.0f;
					backdropR = 0.0f;
					backdropIntensity = 0.0f;

					g_backdropRecordsByRegion[0][flatIdx].flags =
						g_objectTypeTables.missionBackdropDescriptors[backdropId].flags;
					if ((g_backdropRecordsByRegion[0][flatIdx].flags & 4) != 0) {
						g_backdropRecordsByRegion[0][flatIdx].drawFlags = 1;
					} else if (backdropType != OBJ_DeathStarFireTextureGroup6250_Sprite000 &&
							   !g_useHardware3D) {
						g_backdropRecordsByRegion[0][flatIdx].drawFlags = 1;
					} else {
						g_backdropRecordsByRegion[0][flatIdx].drawFlags = 0;
					}

					if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.specialCargo[0] != 0) {
						setlocale(LC_ALL, "English");
						sscanf(g_missionFlightGroups[g_currentFlightGroupIdx].fg.specialCargo, "%f",
							   &backdropScale);
						setlocale(LC_ALL, g_emptyString);
						if (backdropScale == 15.5f) {
							backdropScale = 1.0f;
							g_backdropRecordsByRegion[0][flatIdx].flags |= 2;
						}
					}

					if (backdropType != OBJ_DeathStarFireTextureGroup6250_Sprite000) {
						if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.cargo[0] != 0) {
							setlocale(LC_ALL, "English");
							sscanf(g_missionFlightGroups[g_currentFlightGroupIdx].fg.cargo, "%f",
								   &backdropIntensity);
							setlocale(LC_ALL, g_emptyString);
						}
						if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.name[0] != 0) {
							setlocale(LC_ALL, "English");
							sscanf(g_missionFlightGroups[g_currentFlightGroupIdx].fg.name, "%f %f %f",
								   &backdropR, &backdropG, &backdropB);
							setlocale(LC_ALL, g_emptyString);
						}
					}

					if (backdropType != OBJ_DeathStarFireTextureGroup6250_Sprite000 &&
						(g_backdropRecordsByRegion[0][flatIdx].flags & 1) != 0) {
						g_backdropRecordsByRegion[0][flatIdx].frame =
							g_missionFlightGroups[g_currentFlightGroupIdx].fg.globalCargoIndex + 1;
					} else {
						g_backdropRecordsByRegion[0][flatIdx].frame = 0;
					}

					g_backdropRecordsByRegion[0][flatIdx].angularScale =
						(uint16_t)(int)(backdropScale * 256.0f);
					if (!g_useHardware3D && (g_backdropRecordsByRegion[0][flatIdx].flags & 2) == 0) {
						g_backdropRecordsByRegion[0][flatIdx].angularScale >>= 1;
					}
					DebugPrintfChannel(
						1024, "Set backdrop scale %f, intensity %f, R%f, G%f, B%f, flags %d, imgnum %d.\n",
						backdropScale, backdropIntensity, backdropR, backdropG, backdropB,
						g_backdropRecordsByRegion[0][flatIdx].flags,
						g_backdropRecordsByRegion[0][flatIdx].frame);
					g_backdropRecordsByRegion[0][flatIdx].colorR = backdropR;
					g_backdropRecordsByRegion[0][flatIdx].colorG = backdropG;
					g_backdropRecordsByRegion[0][flatIdx].colorB = backdropB;
					g_backdropRecordsByRegion[0][flatIdx].intensity = backdropIntensity;
				}

				if (backdropType == OBJ_DeathStarFireTextureGroup6250_Sprite000
#ifdef XWA_MODERN
					&& recordIdx < XWA_BACKDROP_RECORDS_PER_REGION
#endif
				) {
					DeathStarTunnelLaserRegionState* laser;
					const XwaOrder* order;
					float firstShotDelay;
					float repeatShotDelay;
					unsigned int laserFlatIdx;

#ifdef XWA_MODERN
					firstShotDelay = 0.0f;
					repeatShotDelay = 0.0f;
#endif

					laserFlatIdx = recordIdx + XWA_BACKDROP_RECORDS_PER_REGION * backdropRegion;
					laser = &g_deathStarTunnelLaserRegions[backdropRegion];
					laser->emitterRect = &g_backdropRecordsByRegion[0][laserFlatIdx];
					laser->enabled = 1;
					laser->emitterOffsetX = g_backdropRecordsByRegion[0][laserFlatIdx].worldDirQ20.x;
					laser->emitterOffsetY = g_backdropRecordsByRegion[0][laserFlatIdx].worldDirQ20.y;
					laser->emitterOffsetZ = g_backdropRecordsByRegion[0][laserFlatIdx].worldDirQ20.z;
					++g_backdropCountByRegion[backdropRegion];

					laser->beamSpriteRect = &g_backdropRecordsByRegion[0][laserFlatIdx + 1];
					g_backdropRecordsByRegion[0][laserFlatIdx + 1].modelType =
						OBJ_DeathStarFireTextureGroup6251;
					g_backdropRecordsByRegion[0][laserFlatIdx + 1].worldDirQ20 =
						g_backdropRecordsByRegion[0][laserFlatIdx].worldDirQ20;
					g_backdropRecordsByRegion[0][laserFlatIdx + 1].angularScale =
						g_backdropRecordsByRegion[0][laserFlatIdx].angularScale;
					g_backdropRecordsByRegion[0][laserFlatIdx + 1].drawFlags =
						g_backdropRecordsByRegion[0][laserFlatIdx].drawFlags;
					g_backdropRecordsByRegion[0][laserFlatIdx + 1].flags =
						g_backdropRecordsByRegion[0][laserFlatIdx].flags;
					g_backdropRecordsByRegion[0][laserFlatIdx + 1].frame = 1;
					laser->nextTargetFlightGroupIndex = 0;

					if (g_missionFlightGroups[g_currentFlightGroupIdx].fg.name[0] != 0) {
						setlocale(LC_ALL, "English");
						sscanf(g_missionFlightGroups[g_currentFlightGroupIdx].fg.name, "%f %f",
							   &firstShotDelay, &repeatShotDelay);
						setlocale(LC_ALL, g_emptyString);
						laser->firstShotDelayTicks = -laser->warmupTicks - (int)(firstShotDelay * -236.0f);
						laser->repeatShotDelayTicks = -laser->warmupTicks - (int)(repeatShotDelay * -236.0f);
					}

					order = &g_missionFlightGroups[g_currentFlightGroupIdx].fg.orders[4 * backdropRegion];
					if (order->target1Type != 0) {
						laser->targetFlightGroupIds[0] = order->target1;
						laser->targetFlightGroupCount = 1;
						if (order->target2Type != 0) {
							laser->targetFlightGroupIds[1] = order->target2;
							laser->targetFlightGroupCount = 2;
							if (order->secondaryTargetTypes[XWA_ORDER_TARGET_3] != 0) {
								laser->targetFlightGroupIds[2] = order->secondaryTargets[XWA_ORDER_TARGET_3];
								laser->targetFlightGroupCount = 3;
								if (order->secondaryTargetTypes[XWA_ORDER_TARGET_4] != 0) {
									laser->targetFlightGroupIds[3] =
										order->secondaryTargets[XWA_ORDER_TARGET_4];
									laser->targetFlightGroupCount = 4;
								}
							}
						}
					} else {
						laser->targetFlightGroupCount = 0;
					}
				}
#ifndef XWA_MODERN
				++g_backdropCountByRegion[backdropRegion];
#else
				if (recordIdx < XWA_BACKDROP_RECORDS_PER_REGION) {
					++g_backdropCountByRegion[backdropRegion];
				}
#endif
			}
		}
		++g_currentFlightGroupIdx;
	}

	FlightLight_ClearDirectionalLights();
	FlightLight_AddCurrentRegionBackdropLights();
	g_teamVictoryTimeLimitStarted = 0;
	g_missionElapsedClock.subsecondTicks = 0;
	g_missionCountdownClock.hours = 0;
	g_missionCountdownClock.minutes = 0;
	g_missionCountdownClock.subsecondTicks = 0;
	timeLimitActive = g_missionTimeLimitActive;
	g_missionElapsedClock.hours = 0;
	g_missionElapsedClock.minutes = 0;
	g_missionElapsedClock.seconds = 0;
	if (timeLimitActive != 0) {
		if (timeLimitActive != 0xffu) {
			g_missionCountdownClock.minutes = timeLimitActive;
		} else if (g_missionHeader.body.timeLimitMin != 0) {
			g_missionCountdownClock.minutes = g_missionHeader.body.timeLimitMin;
			g_missionTimeLimitActive = g_missionHeader.body.timeLimitMin;
		} else {
			g_missionTimeLimitActive = 0;
		}
	} else {
		g_missionTimeLimitActive = 0;
	}
	g_missionCountdownClock.seconds = 0;
	return 1;
}

// FUNCTION: XWA 0x505DC0
int Team_IsHostileToTeam(int otherTeamIdx, int teamIdx) {
	if (otherTeamIdx == teamIdx) {
		return 0;
	}

	return g_missionTeams[teamIdx].allies[otherTeamIdx] == 0;
}

// FUNCTION: XWA 0x505E70
int Team_IsFriendlyToTeam(int otherTeamIdx, int teamIdx) {
	if (otherTeamIdx == teamIdx) {
		return 1;
	}

	return g_missionTeams[teamIdx].allies[otherTeamIdx] == 1;
}

// FUNCTION: XWA 0x421920
int Mission_SyncPilotNetworkPlayersToSessionSlots(void) {
	int rosterCount;
	int rosterIdx;
	SessionPlayerInfo* roster;

	roster = NetSession_GetPlayerRoster(&rosterCount);
	rosterIdx = 0;
	while (rosterIdx < rosterCount) {
		int pilotPlayerIdx;

		pilotPlayerIdx = 0;
		while (pilotPlayerIdx < 8) {
			if (g_pilotData.networkPlayers[pilotPlayerIdx].directPlayId != 0 &&
				g_pilotData.networkPlayers[pilotPlayerIdx].directPlayId == roster[rosterIdx].dplayId) {
				break;
			}
			++pilotPlayerIdx;
		}

		if (pilotPlayerIdx < 8) {
			g_players[NetSession_FindPlayerSlotByDpid(roster[rosterIdx].dplayId)].network.directPlayId =
				roster[rosterIdx].dplayId;
		}

		++rosterIdx;
	}

	return 1;
}

// FUNCTION: XWA 0x4D9E50
void Mission_RecordPlayerCraftLossAttribution(int attackerPlayerIdx, int victimObjIdx, int contributionTier) {
	CraftData* craft;
	int ownerPlayerIdx;
	int rating;

	if (contributionTier != 2 && contributionTier != 3) {
		return;
	}

	ownerPlayerIdx = g_objectTable[victimObjIdx].playerOwnerIdx;
	if (ownerPlayerIdx == -1) {
		ownerPlayerIdx = g_missionFlightGroups[g_objectTable[victimObjIdx].flightGroupIdx].playerOwnerIdx;
		if (ownerPlayerIdx == -1) {
			return;
		}
	}

	craft = g_objectTable[victimObjIdx].mobj->pCraft;
	if ((uint16_t)MATH2_percentage((uint32_t)craft->damageReceivedByPlayerOwnedCraft,
								   (uint32_t)craft->damageReceivedTotal) < 0x8000u) {
		return;
	}

	if (attackerPlayerIdx != -1) {
		rating = g_players[attackerPlayerIdx].pilotRating;
		if (contributionTier == 3) {
			++g_players[ownerPlayerIdx].perMissionKills.killedByPlayerRating[rating];
		}
	} else if (contributionTier == 3) {
		++g_players[ownerPlayerIdx].perMissionKills.killedByAiRating;
	}
}

static __inline unsigned int Mission_GetKillCreditObjectType(unsigned int victimObjIdx) {
	ObjectRecord* object;

	object = &g_objectTable[victimObjIdx];
	return g_objectTypeTables
		.craftTypeToObjectType[g_missionFlightGroups[object->flightGroupIdx].fg.craftType];
}

static __inline int Mission_ComputeKillCreditPointValue(unsigned int victimObjIdx) {
	unsigned int objectType;
	int pointValue;

	objectType = Mission_GetKillCreditObjectType(victimObjIdx);
	if (g_modelTypeTable[objectType].familyId == 0) {
		return Mission_ComputeCraftPointValue(victimObjIdx);
	}

	if ((objectType >= 0xd2u && objectType <= 0xd4u) || (objectType >= 0xd5u && objectType <= 0xd9u)) {
		pointValue = 500;
	} else {
		pointValue = 10;
	}

	if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
		g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) {
		pointValue *= 3;
	}

	return pointValue;
}

// FUNCTION: XWA 0x4D88D0
void Mission_CreditDestructionDamageContributors(uint16_t sourceObjIdx, uint16_t victimObjIdx) {
	unsigned int playerIdx;
	int teamIdx, damageIdx;
	int teamPlayerContributionCount[10];
	MobileObject* victimMobj;
	CraftData* victimCraft;
	int specialCargoFlag;
	int flightGroupIdx;
	int creditedOwnerIdx;
	int victimRating;

	memset(teamPlayerContributionCount, 0, sizeof(teamPlayerContributionCount));
	specialCargoFlag = 0;
	victimMobj = g_objectTable[victimObjIdx].mobj;
	victimCraft = NULL;
	if (victimMobj != NULL) {
		victimCraft = victimMobj->pCraft;
	}

	if (victimCraft == NULL || victimCraft->damageReceivedTotal == 0) {
		int sourceOwnerIdx;
		int sourceTeam;

		sourceOwnerIdx = g_objectTable[sourceObjIdx].playerOwnerIdx;
		flightGroupIdx = g_objectTable[victimObjIdx].flightGroupIdx;
		if (sourceOwnerIdx != -1) {
			Mission_CreditPlayerKillContribution(victimObjIdx, 0, 3, (unsigned int)sourceOwnerIdx, -1, 0);
			if (g_objectTable[victimObjIdx].genusId == GENUS_Mine) {
				g_players[g_objectTable[sourceObjIdx].playerOwnerIdx].missionStats.worseRatingPromoPoints +=
					4;
			}
		}

		sourceTeam = g_missionFlightGroups[g_objectTable[sourceObjIdx].flightGroupIdx].fg.team;
		Mission_CreditTeamKillContribution(victimObjIdx, 0, 3, sourceTeam);
		return;
	}

	flightGroupIdx = g_objectTable[victimObjIdx].flightGroupIdx;
	if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == victimCraft->waveNumber) {
		specialCargoFlag = 1;
	}

	creditedOwnerIdx = -1;
	if ((uint16_t)MATH2_percentage((uint32_t)victimCraft->damageReceivedByPlayerOwnedCraft,
								   (uint32_t)victimCraft->damageReceivedTotal) >= 0x8000u) {
		creditedOwnerIdx = g_missionFlightGroups[flightGroupIdx].playerOwnerIdx;
	}

	if (creditedOwnerIdx != -1) {
		victimRating = g_players[creditedOwnerIdx].pilotRating;
	} else {
		victimRating = g_missionFlightGroups[g_objectTable[victimObjIdx].flightGroupIdx].fg.groupAI;
	}

	for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
		PlayerData* player;
		int contributionTier;
		uint16_t contributionPercentage;

		player = &g_players[playerIdx];
		if (player->connectedFlag == 0) {
			continue;
		}

		contributionPercentage = (uint16_t)MATH2_percentage(
			(uint32_t)victimCraft->damageFromPlayer[playerIdx], (uint32_t)victimCraft->damageReceivedTotal);
		if (contributionPercentage >= 0xaaaau) {
			contributionTier = 3;
		} else if (contributionPercentage >= 0x5999u) {
			contributionTier = 2;
		} else if (contributionPercentage >= 0x0cccu) {
			contributionTier = 1;
		} else {
			continue;
		}

		Mission_CreditPlayerKillContribution(victimObjIdx, specialCargoFlag, contributionTier, playerIdx,
											 creditedOwnerIdx, victimRating);

		if (g_missionTeams[(uint16_t)player->playerIff]
				.allies[g_missionFlightGroups[g_objectTable[victimObjIdx].flightGroupIdx].fg.team] == 0) {
			AiController* aiController;
			const char* planName;
			int suppressRatingAward;
			int victimRatingWeight;
			ObjectTypeId playerObjectType;
			int playerRatingWeight;
			int creditedPilotRating;
			int ratingDelta;

			aiController = pai_GetEffectiveAIController(victimCraft);
			planName = g_planTable[aiController->currentPlanId].name;
			suppressRatingAward =
				(strcmp(planName, "nullpln") == 0 || strcmp(planName, "stationaryldrpln") == 0 ||
				 strcmp(planName, "formldr1pln") == 0 || strcmp(planName, "formflw1pln") == 0 ||
				 strcmp(planName, "formevadeldr1pln") == 0 || strcmp(planName, "formevadeflw1pln") == 0 ||
				 strcmp(planName, "exithangarpln") == 0 || strcmp(planName, "enterhangarpln") == 0 ||
				 strcmp(planName, "disabledpln") == 0 || strcmp(planName, "selfdestroypln") == 0) &&
				creditedOwnerIdx == -1;
			if (GetModelIndexFromType(g_objectTable[victimObjIdx].objectType) != 0xffffu) {
				victimRatingWeight =
					g_modelDefs[(uint16_t)GetModelIndexFromType(g_objectTable[victimObjIdx].objectType)]
						.ratingWeight;
				if (victimRatingWeight == 0) {
					suppressRatingAward = 1;
				}
			}

			playerObjectType =
				(ObjectTypeId)g_objectTypeTables
					.craftTypeToObjectType[g_missionFlightGroups[player->boundFlightGroupIdx].fg.craftType];
			if (GetModelIndexFromType(playerObjectType) != 0xffffu) {
				playerRatingWeight =
					g_modelDefs[(uint16_t)GetModelIndexFromType(playerObjectType)].ratingWeight;
			}

			if (creditedOwnerIdx == -1) {
				creditedPilotRating =
					g_defaultPilotRatingByAiLevel[g_missionFlightGroups[flightGroupIdx].fg.groupAI];
			} else {
				creditedPilotRating = g_players[creditedOwnerIdx].pilotRating;
			}

			ratingDelta = creditedPilotRating - player->pilotRating;
			if (!suppressRatingAward && ratingDelta >= -4) {
				int ratingScale;
				int points;

				ratingScale = ratingDelta + 4;
				if (player->pilotRating >= 15u) {
					if (ratingScale < 3) {
						ratingScale = 3;
					}
				} else if (ratingScale < 4) {
					ratingScale = 4;
				}

				points = ratingScale * creditedPilotRating * victimRatingWeight / playerRatingWeight;
				if (contributionTier == 2) {
					points /= 2;
				} else if (contributionTier == 1) {
					points /= 10;
				}
				player->missionStats.ratingPromoPoints += points;
				sprintf(Buffer, "Rating points awarded: %d to player: %d Better total: %d\n", points,
						playerIdx, player->missionStats.ratingPromoPoints);
			} else {
				int ratingScale;
				int points;

				if (suppressRatingAward) {
					ratingScale = 1;
					if (victimRatingWeight == 0) {
						victimRatingWeight = 1;
					}
				} else {
					ratingScale = creditedPilotRating;
				}

				if (ratingScale == 0) {
					ratingScale = 1;
				}

				points = ratingScale * victimRatingWeight / playerRatingWeight;
				if (contributionTier == 2) {
					points /= 2;
				} else if (contributionTier == 1) {
					points /= 10;
				}
				if (points == 0) {
					points = 1;
				}
				player->missionStats.worseRatingPromoPoints += points;
				sprintf(Buffer, "Rating points awarded: %d to player: %d Worse total: %d\n", points,
						playerIdx, player->missionStats.worseRatingPromoPoints);
			}

			if (contributionTier == 2 || contributionTier == 3) {
				Mission_RecordPlayerCraftLossAttribution((int)playerIdx, victimObjIdx, contributionTier);
				teamPlayerContributionCount[(uint16_t)player->playerIff] += contributionTier == 2 ? 1 : 2;
			}
		}
	}

	for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
		uint32_t teamDamage;
		uint32_t maxFgDamage;
		int maxDamageFgIdx;
		int contributionTier;
		uint16_t contributionPercentage;
		char* damageFgIdxPtr;
		int32_t* damageAmountPtr;

		teamDamage = 0;
		maxFgDamage = 0;
		maxDamageFgIdx = 0;
		damageFgIdxPtr = victimCraft->damageFromFlightGroupIdx;
		damageAmountPtr = victimCraft->damageFromFlightGroupAmount;
		for (damageIdx = 0; damageIdx < 8; ++damageIdx) {
			int damageFgIdx;

			damageFgIdx = *damageFgIdxPtr;
			if (damageFgIdx != -1 && g_missionFlightGroups[(uint8_t)damageFgIdx].fg.team == teamIdx) {
				uint32_t amount;

				amount = (uint32_t)*damageAmountPtr;
				teamDamage += amount;
				if (amount > maxFgDamage) {
					maxFgDamage = amount;
					maxDamageFgIdx = (uint8_t)damageFgIdx;
				}
			}
			++damageFgIdxPtr;
			++damageAmountPtr;
		}

		contributionPercentage =
			(uint16_t)MATH2_percentage(teamDamage, (uint32_t)victimCraft->damageReceivedTotal);
		if (contributionPercentage >= 0xaaaau) {
			contributionTier = 3;
		} else if (contributionPercentage >= 0x5999u) {
			contributionTier = 2;
		} else if (contributionPercentage >= 0x0cccu) {
			contributionTier = 1;
		} else {
			contributionTier = 0;
		}

		if (contributionTier != 0) {
			Mission_CreditTeamKillContribution(victimObjIdx, specialCargoFlag, contributionTier, teamIdx);
		}

		if ((contributionTier != 2 && contributionTier != 3) || teamPlayerContributionCount[teamIdx] >= 2) {
			continue;
		}

		{
			int missingPlayerContribution;

			missingPlayerContribution =
				(contributionTier == 2 ? 1 : 2) - teamPlayerContributionCount[teamIdx];
			if (missingPlayerContribution == 2) {
				Mission_RecordPlayerCraftLossAttribution(g_missionFlightGroups[maxDamageFgIdx].playerOwnerIdx,
														 victimObjIdx, 3);
				if (creditedOwnerIdx != -1) {
					++g_players[creditedOwnerIdx].perMissionKills.killsFullFromFlightGroup[maxDamageFgIdx];
				}
			} else if (missingPlayerContribution == 1) {
				Mission_RecordPlayerCraftLossAttribution(g_missionFlightGroups[maxDamageFgIdx].playerOwnerIdx,
														 victimObjIdx, 2);
				if (creditedOwnerIdx != -1) {
					++g_players[creditedOwnerIdx].perMissionKills.killsSharedFromFlightGroup[maxDamageFgIdx];
				}
			}
		}
	}

	return;
}

// FUNCTION: XWA 0x4D9190
void Mission_CreditPlayerKillContribution(uint16_t victimObjIdx, int specialCargoFlag, int contributionTier,
										  unsigned int playerIdx, int creditedOwnerIdx, int victimRating) {
	unsigned int victimIndex;
	uint16_t flightGroupIdx;
	uint8_t victimTeam;
	int scoreDivisor;
	int killVoiceProbability;
	int pointValue;

	victimIndex = victimObjIdx;
	flightGroupIdx = g_objectTable[victimIndex].flightGroupIdx;
	if (g_objectTable[victimIndex].mobj != NULL) {
		victimTeam = g_objectTable[victimIndex].mobj->team;
	} else {
		victimTeam = g_missionFlightGroups[flightGroupIdx].fg.team;
	}

	scoreDivisor = 1;
	killVoiceProbability = 0;

	if (flightGroupIdx <= (int16_t)g_missionHeader.numFlightGroups) {
		int goalIdx;

		for (goalIdx = 0; goalIdx < 8; ++goalIdx) {
			if (g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.argument != 0 ||
				g_missionFlightGroups[flightGroupIdx]
						.fg.fgGoals[goalIdx]
						.payload.enabledForTeam[(uint16_t)g_players[playerIdx].playerIff] != 1) {
				continue;
			}

			switch (g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.condition) {
				case 0:
				case 10:
					killVoiceProbability = 0x6000;
					break;

				case 2:
					killVoiceProbability = 0xf000;
					break;
			}
		}
	}

	if (g_missionTeams[(uint16_t)g_players[playerIdx].playerIff].allies[victimTeam] == 0) {
		unsigned int objectType;

		objectType = Mission_GetKillCreditObjectType(victimObjIdx);
		if (g_modelTypeTable[objectType].familyId == 0) {
			pointValue = Mission_ComputeCraftPointValue(victimObjIdx);
		} else {
			if ((objectType >= 0xd2u && objectType <= 0xd4u) ||
				(objectType >= 0xd5u && objectType <= 0xd9u)) {
				pointValue = 500;
			} else {
				pointValue = 10;
			}

			if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
				g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) {
				pointValue *= 3;
			}
		}
		switch (contributionTier) {
			case 1:
				++g_players[playerIdx].perMissionKills.killsAssistOnFlightGroup[flightGroupIdx];
				if (creditedOwnerIdx != -1) {
					++g_players[playerIdx].perMissionKills.killsAssistOnPlayerRating[victimRating];
				} else {
					++g_players[playerIdx].perMissionKills.killsAssistOnAiRating[victimRating];
				}
				scoreDivisor = 10;
				pointValue /= 10;
				break;

			case 2:
				++g_players[playerIdx].perMissionKills.killsSharedOnFlightGroup[flightGroupIdx];
				if (creditedOwnerIdx != -1) {
					++g_players[playerIdx].perMissionKills.killsSharedOnPlayer[creditedOwnerIdx];
					++g_players[playerIdx].perMissionKills.killsSharedOnPlayerRating[victimRating];
					++g_players[creditedOwnerIdx].perMissionKills.killsSharedFromPlayer[playerIdx];
				} else {
					++g_players[playerIdx].perMissionKills.killsSharedOnAiRating[victimRating];
				}
				scoreDivisor = 6;
				pointValue /= 2;
				break;

			case 3:
				++g_players[playerIdx].perMissionKills.killsFullOnFlightGroup[flightGroupIdx];
				if (creditedOwnerIdx != -1) {
					++g_players[playerIdx].perMissionKills.killsFullOnPlayer[creditedOwnerIdx];
					++g_players[playerIdx].perMissionKills.killsFullOnPlayerRating[victimRating];
					++g_players[creditedOwnerIdx].perMissionKills.killsFullFromPlayer[playerIdx];
				} else {
					++g_players[playerIdx].perMissionKills.killsFullOnAiRating[victimRating];
				}
				scoreDivisor = 1;
				break;

			default:
				break;
		}

		if (Mission_ApplyFlightGroupGoalScore(2, flightGroupIdx, (int)playerIdx, scoreDivisor,
											  specialCargoFlag,
											  (uint16_t)g_players[playerIdx].playerIff) >= 0) {
			g_players[playerIdx].missionStats.missionScore += pointValue;
		}

		if (contributionTier == 3 && playerIdx == (unsigned int)g_localPlayer) {
			fsfx_speakorderack(g_localPlayer, -1, 38, -1, 0xffffu, killVoiceProbability);
		}
	} else if (contributionTier == 2 || contributionTier == 3) {
		unsigned int objectType;

		objectType = Mission_GetKillCreditObjectType(victimObjIdx);
		if (g_modelTypeTable[objectType].familyId == 0) {
			pointValue = Mission_ComputeCraftPointValue(victimObjIdx);
		} else {
			if ((objectType >= 0xd2u && objectType <= 0xd4u) ||
				(objectType >= 0xd5u && objectType <= 0xd9u)) {
				pointValue = 500;
			} else {
				pointValue = 10;
			}

			if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
				g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) {
				pointValue *= 3;
			}
		}
		g_players[playerIdx].missionStats.missionScore -= pointValue;
		++g_players[playerIdx].perMissionKills.friendliesKilled;
		g_players[playerIdx].missionStats.ratingPromoPoints -= 500;
		g_msgSenderIff = (uint16_t)g_players[playerIdx].iff;
		msg_emitInFlightMessage(MSG_OWN_SIDE, (int)playerIdx);
		if (pointValue != 0) {
			g_msgArgTable[0] = (uint16_t)pointValue;
			msg_emitInFlightMessage(MSG_PENALTY_POINTS, (int)playerIdx);
		}

		if (playerIdx == (unsigned int)g_localPlayer) {
			fsfx_SpeakTacticalOfficerEvent(5, 123, victimObjIdx, 0xffffu);
		}

		if (g_pilotData.missionDirectoryId == 3) {
			uint8_t globalGroup;

			globalGroup = g_missionFlightGroups[flightGroupIdx].fg.globalGroup;
			if (globalGroup == 3 || globalGroup == 6) {
				int activeTeamPresent[10];
				int teamIdx;
				int fgIdx;

				memset(activeTeamPresent, 0, sizeof(activeTeamPresent));
				for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
					if (g_missionFlightGroups[fgIdx].fg.globalGroup <= 6u) {
						activeTeamPresent[g_missionFlightGroups[fgIdx].fg.team] = 1;
					}
				}

				for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
					if (teamIdx == victimTeam || activeTeamPresent[teamIdx] == 0 ||
						g_gameConfig.teamGoals[teamIdx] <= 1u) {
						continue;
					}

					if (g_missionFlightRuntimeState.teamGoalStatus[victimTeam][TEAM_GOAL_SECONDARY] != 1 &&
						g_missionFlightRuntimeState.teamGoalStatus[victimTeam][TEAM_GOAL_PRIMARY] != 2) {
						g_msgSenderIff = (uint16_t)g_players[playerIdx].iff;
						fsfx_PlaySound(61, 0xffffu, playerIdx);
						msg_emitInFlightMessage(MSG_MISSION_LOST, (int)playerIdx);
						msg_emitInFlightMessage(MSG_MISSION_IMPOSSIBLE, (int)playerIdx);

						for (teamIdx = 0; teamIdx < 2; ++teamIdx) {
							const char* message;

							message = g_missionTeams[victimTeam].endOfMissionMessages[2 + teamIdx];
							if (message[0] != '\0') {
								msg_addMessagePtr(0, message);
								g_pendingHudMessageVoiceSfxId = (uint16_t)(teamIdx + 262);
								if ((uint8_t)message[0] < '1' || (uint8_t)message[0] > '6') {
									msg_emitInFlightMessage(MSG_BLANK, (int)playerIdx);
								} else {
									msg_emitInFlightMessage(MSG_RADIO_BLANK, (int)playerIdx);
								}
								g_pendingHudMessageVoiceSfxId = 0;
							}
						}

						g_playerFlightTransientTimers[g_localPlayer].missionLossMusicTimer = 2478;
					}

					g_missionFlightRuntimeState.teamGoalStatus[victimTeam][TEAM_GOAL_SECONDARY] = 1;
					g_missionFlightRuntimeState.teamGoalStatus[victimTeam][TEAM_GOAL_PRIMARY] = 2;
				}
			}
		}
	}
}

// FUNCTION: XWA 0x4D9770
int Mission_CreditTeamKillContribution(uint16_t victimObjIdx, int specialCargoFlag, int contributionTier,
									   int teamIdx) {
	unsigned int victimIndex;
	ObjectRecord* victimObj;
	MobileObject* victimMobj;
	uint16_t flightGroupIdx;
	uint8_t victimTeam;
	uint16_t scoreDivisor;

	victimIndex = victimObjIdx;
	victimObj = &g_objectTable[victimIndex];
	victimMobj = victimObj->mobj;
	flightGroupIdx = victimObj->flightGroupIdx;
	if (victimMobj != NULL) {
		victimTeam = victimMobj->team;
	} else {
		victimTeam = g_missionFlightGroups[flightGroupIdx].fg.team;
	}

	scoreDivisor = 1;
	if (g_missionTeams[teamIdx].allies[victimTeam] == 0) {
		unsigned int objectType;
		int killPointValue;
		int pointValue;
		int goalScoreResult;

		objectType = Mission_GetKillCreditObjectType(victimObjIdx);
		if (g_modelTypeTable[objectType].familyId == 0) {
			killPointValue = Mission_ComputeCraftPointValue(victimObjIdx);
		} else {
			if ((objectType >= 0xd2u && objectType <= 0xd4u) ||
				(objectType >= 0xd5u && objectType <= 0xd9u)) {
				killPointValue = 500;
			} else {
				killPointValue = 10;
			}

			if (g_missionHeader.body.hangar == XWA_HANGAR_QUICKSTART ||
				g_missionHeader.body.hangar == XWA_HANGAR_SKIRMISH) {
				killPointValue *= 3;
			}
		}

		pointValue = killPointValue;
		switch (contributionTier) {
			case 1:
				++g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_ASSIST][teamIdx];
				scoreDivisor = 10;
				pointValue = killPointValue / 10;
				break;

			case 2:
				++g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_SHARED][teamIdx];
				scoreDivisor = 6;
				pointValue = killPointValue / 2;
				break;

			case 3:
				++g_missionFlightRuntimeState.teamKillStats[TEAM_KILL_STAT_FULL][teamIdx];
				scoreDivisor = 1;
				break;

			default:
				break;
		}

		goalScoreResult =
			Mission_ApplyFlightGroupGoalScore(2, flightGroupIdx, -1, scoreDivisor, specialCargoFlag, teamIdx);
		if (goalScoreResult >= 0) {
			g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION][teamIdx] += pointValue;
		}
		return goalScoreResult;
	} else if (contributionTier == 2 || contributionTier == 3) {
		int pointValue;

		pointValue = Mission_ComputeKillCreditPointValue(victimObjIdx);
		g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION][teamIdx] -= pointValue;
		return pointValue;
	}

	return (int)victimIndex;
}

// FUNCTION: XWA 0x4D9B90
int Mission_RecordPlayerCraftLoss(unsigned int objIdx, int allowPendingDamageCredit) {
	unsigned int playerIdx;
	int aiRatingIdx;
	int pointPenalty;
	ObjectRecord* object;
	int teamIdx;
	int ownerPlayerIdx;
	CraftData* craft;
	int creditedPlayerIdx;

	creditedPlayerIdx = -1;
	pointPenalty = (int)Mission_ComputeCraftPointValue(objIdx) / 2;
	object = &g_objectTable[objIdx];
	teamIdx = g_missionFlightGroups[object->flightGroupIdx].fg.team;
	g_missionFlightRuntimeState.teamScores[TEAM_SCORE_MISSION][teamIdx] -= pointPenalty;
	++g_missionFlightRuntimeState
		  .teamKillStats[TEAM_KILL_STAT_LOSS][g_missionFlightGroups[object->flightGroupIdx].fg.team];

	ownerPlayerIdx = object->playerOwnerIdx;
	if (ownerPlayerIdx == -1) {
		ownerPlayerIdx = g_missionFlightGroups[object->flightGroupIdx].playerOwnerIdx;
		if (ownerPlayerIdx == -1) {
			return 0;
		}
		if (g_players[ownerPlayerIdx].connectedFlag == 0) {
			return 0;
		}
	}

	craft = object->mobj->pCraft;
	g_players[ownerPlayerIdx].missionStats.missionScore -= pointPenalty;

	if (allowPendingDamageCredit) {
		uint32_t remainingDurability;
		uint32_t pendingDamage;

		if ((uint32_t)craft->hullDamage < (uint32_t)craft->hullMax) {
			remainingDurability = (uint32_t)(craft->hullMax - craft->hullDamage);
		} else {
			remainingDurability = 0;
		}

		if (craft->shieldFront > 0) {
			remainingDurability += (uint16_t)craft->shieldFront;
		}
		if (craft->shieldRear > 0) {
			remainingDurability += (uint16_t)craft->shieldRear;
		}
		remainingDurability >>= 1;

		pendingDamage = 0;
		for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
			pendingDamage += (uint32_t)craft->damageFromPlayer[playerIdx];
		}
		for (aiRatingIdx = 0; aiRatingIdx < 6; ++aiRatingIdx) {
			pendingDamage += (uint32_t)craft->damageFromAiSkill[aiRatingIdx];
		}

		if (pendingDamage > remainingDurability) {
			Mission_CreditDestructionDamageContributors(objIdx, objIdx);
		}
	}

	if ((uint16_t)MATH2_percentage((uint32_t)craft->damageReceivedByPlayerOwnedCraft,
								   (uint32_t)craft->damageReceivedTotal) >= 0x8000u) {
		uint32_t playerDamage;
		uint32_t aiDamage;
		uint32_t collisionDamage;
		uint32_t mineDamage;
		uint32_t starshipDamage;
		uint32_t totalClassifiedDamage;

		++g_players[ownerPlayerIdx].perMissionKills.totalCraftLosses;

		playerDamage = 0;
		for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
			playerDamage += (uint32_t)craft->damageFromPlayer[playerIdx];
		}

		aiDamage = 0;
		for (aiRatingIdx = 0; aiRatingIdx < 6; ++aiRatingIdx) {
			aiDamage += (uint32_t)craft->damageFromAiSkill[aiRatingIdx];
		}

		starshipDamage = (uint32_t)craft->damageFromStarship;
		collisionDamage = (uint32_t)craft->damageFromCollision;
		mineDamage = (uint32_t)craft->damageFromMine;
		totalClassifiedDamage = starshipDamage | (mineDamage | (collisionDamage | (aiDamage | playerDamage)));
		if (totalClassifiedDamage != 0) {
			if (collisionDamage >= mineDamage && collisionDamage >= starshipDamage &&
				collisionDamage >= playerDamage && collisionDamage >= aiDamage) {
				++g_players[ownerPlayerIdx].perMissionKills.lossesByCollisions;
			} else if (starshipDamage >= mineDamage && starshipDamage >= collisionDamage &&
					   starshipDamage >= playerDamage && starshipDamage >= aiDamage) {
				++g_players[ownerPlayerIdx].perMissionKills.lossesByStarships;
			} else if (mineDamage >= starshipDamage && mineDamage >= collisionDamage &&
					   mineDamage >= playerDamage && mineDamage >= aiDamage) {
				++g_players[ownerPlayerIdx].perMissionKills.lossesByMines;
			} else if (playerDamage >= starshipDamage && playerDamage >= collisionDamage &&
					   playerDamage >= mineDamage && playerDamage >= aiDamage) {
				uint32_t maxPlayerDamage;
				int topPlayerIdx;
				int topPlayerRating;

				maxPlayerDamage = 0;
				topPlayerRating = -1;
				for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
					if ((uint32_t)craft->damageFromPlayer[playerIdx] > maxPlayerDamage) {
						maxPlayerDamage = (uint32_t)craft->damageFromPlayer[playerIdx];
						topPlayerRating = g_players[playerIdx].pilotRating;
						topPlayerIdx = playerIdx;
					}
				}
				if (topPlayerRating != -1) {
					creditedPlayerIdx = topPlayerIdx;
				}
			} else {
				uint32_t maxAiDamage;

				maxAiDamage = 0;
				for (aiRatingIdx = 0; aiRatingIdx < 6; ++aiRatingIdx) {
					if ((uint32_t)craft->damageFromAiSkill[aiRatingIdx] > maxAiDamage) {
						maxAiDamage = (uint32_t)craft->damageFromAiSkill[aiRatingIdx];
					}
				}
			}
		}
	}

	return creditedPlayerIdx;
}

// FUNCTION: XWA 0x4D99A0
void Mission_RecordProjectileHitStats(unsigned int projectileObjIdx, char awardWarheadPoints) {
	uint16_t ownerObjIdx;
	CraftData* ownerCraft;
	int ownerPlayerIdx;
	uint16_t playerProjectileSlotEnd;

	ownerObjIdx = (uint16_t)g_objectTable[projectileObjIdx].mobj->sourceObjIdx;
	if (ownerObjIdx < g_activeRegionObjectSlotStart || ownerObjIdx >= g_activeRegionCraftObjectSlotEnd) {
		return;
	}

	if (g_objectTable[ownerObjIdx].objectType == OBJ_None) {
		return;
	}

	ownerCraft = g_objectTable[ownerObjIdx].mobj->pCraft;
	playerProjectileSlotEnd =
		(uint16_t)(g_playerProjectileSlotsTotal + g_sharedPlayerProjectileSlotsPerRegion +
				   g_projectileObjectSlotStart);
	ownerPlayerIdx = g_missionFlightGroups[g_objectTable[ownerObjIdx].flightGroupIdx].playerOwnerIdx;

	switch (g_objectTable[projectileObjIdx].objectType) {
		case OBJ_LaserRebel:
		case OBJ_LaserRebelTurbo:
		case OBJ_LaserImperial:
		case OBJ_LaserImperialTurbo:
			++ownerCraft->laserHitsScoredCount;
			if (projectileObjIdx < playerProjectileSlotEnd && ownerPlayerIdx != -1) {
				++g_players[ownerPlayerIdx].missionStats.laserHitsScored;
			}
			break;

		case OBJ_LaserIon:
		case OBJ_LaserIonTurbo:
			++ownerCraft->ionHitsScoredCount;
			if (projectileObjIdx < playerProjectileSlotEnd && ownerPlayerIdx != -1) {
				++g_players[ownerPlayerIdx].missionStats.ionHitsScored;
			}
			break;

		case OBJ_WarheadTorpedo:
		case OBJ_WarheadMissile:
		case OBJ_WarheadAdvancedTorpedo:
		case OBJ_WarheadAdvancedMissile:
		case OBJ_WarheadSpaceBomb:
		case OBJ_WarheadRocket:
		case OBJ_WarheadMagPulse:
		case OBJ_WarheadIonPulse: {
			int flightGroupIdx;

			++ownerCraft->warheadHitsScoredCount;
			flightGroupIdx = g_objectTable[ownerObjIdx].flightGroupIdx;
			if (projectileObjIdx < playerProjectileSlotEnd && ownerPlayerIdx != -1) {
				++g_players[ownerPlayerIdx].perMissionKills.warheadHits;
				if (!awardWarheadPoints) {
					return;
				}
				g_players[ownerPlayerIdx].missionStats.missionScore +=
					g_warheadProjectilePointValueByObjectType[g_objectTable[projectileObjIdx].objectType];
			}
			if (awardWarheadPoints) {
				g_missionFlightRuntimeState
					.teamScores[TEAM_SCORE_MISSION][g_missionFlightGroups[flightGroupIdx].fg.team] +=
					g_warheadProjectilePointValueByObjectType[g_objectTable[projectileObjIdx].objectType];
			}
			break;
		}

		default:
			break;
	}
}

// FUNCTION: XWA 0x4D8800
void Mission_CloseUnavailableFlightGroupAccounting(unsigned int flightGroupIdx) {
	uint16_t arrivedCount;
	uint16_t unarrivedCount;
	uint16_t specialCargoTotal;
	uint16_t specialCargoArrived;
	uint16_t unarrivedSpecialCargo;

	arrivedCount = g_missionFgStats[flightGroupIdx].outcomeCount[1];
	unarrivedCount = (uint16_t)(g_missionFgStats[flightGroupIdx].outcomeCount[0] - arrivedCount);
	specialCargoTotal = g_missionFgStats[flightGroupIdx].specialCargoOutcome[0];
	specialCargoArrived = g_missionFgStats[flightGroupIdx].specialCargoOutcome[1];
	unarrivedSpecialCargo = specialCargoTotal - specialCargoArrived;
	arrivedCount = (uint16_t)(arrivedCount + unarrivedCount);
	g_missionFgStats[flightGroupIdx].outcomeCount[1] = arrivedCount;

	g_missionFgStats[flightGroupIdx].outcomeCount[3] =
		(uint16_t)(g_missionFgStats[flightGroupIdx].outcomeCount[3] + unarrivedCount);
	g_missionFgStats[flightGroupIdx].specialCargoOutcome[3] =
		(uint8_t)(g_missionFgStats[flightGroupIdx].specialCargoOutcome[3] + unarrivedSpecialCargo);
	g_missionFgStats[flightGroupIdx].outcomeCount[9] =
		(uint16_t)(g_missionFgStats[flightGroupIdx].outcomeCount[9] + unarrivedCount);
	g_missionFgStats[flightGroupIdx].specialCargoOutcome[9] =
		(uint8_t)(g_missionFgStats[flightGroupIdx].specialCargoOutcome[9] + unarrivedSpecialCargo);
	g_missionFgStats[flightGroupIdx].outcomeCount[15] =
		(uint16_t)(g_missionFgStats[flightGroupIdx].outcomeCount[15] + unarrivedCount);
	g_missionFgStats[flightGroupIdx].specialCargoOutcome[15] =
		(uint8_t)(g_missionFgStats[flightGroupIdx].specialCargoOutcome[15] + unarrivedSpecialCargo);
	g_missionFgStats[flightGroupIdx].outcomeCount[7] =
		(uint16_t)(g_missionFgStats[flightGroupIdx].outcomeCount[7] + unarrivedCount);
	g_missionFgStats[flightGroupIdx].specialCargoOutcome[7] =
		(uint8_t)(g_missionFgStats[flightGroupIdx].specialCargoOutcome[7] + unarrivedSpecialCargo);
	g_missionFgStats[flightGroupIdx].outcomeCount[5] =
		(uint16_t)(g_missionFgStats[flightGroupIdx].outcomeCount[5] + unarrivedCount);
	g_missionFgStats[flightGroupIdx].specialCargoOutcome[5] =
		(uint8_t)(unarrivedSpecialCargo + g_missionFgStats[flightGroupIdx].specialCargoOutcome[5]);
	g_missionFgStats[flightGroupIdx].outcomeCount[11] =
		(uint16_t)(g_missionFgStats[flightGroupIdx].outcomeCount[11] + unarrivedCount);
	g_missionFgStats[flightGroupIdx].specialCargoOutcome[11] =
		(uint8_t)(unarrivedSpecialCargo + g_missionFgStats[flightGroupIdx].specialCargoOutcome[11]);

	g_missionFgStats[flightGroupIdx].hasArrived = 1;
	g_missionFgStats[flightGroupIdx].wavesRemaining = 0;
}

// FUNCTION: XWA 0x4D8140
void Mission_RecordCraftOutcome(uint16_t objIdx, uint16_t flightGroupIdx, uint16_t outcomeId) {
	CraftData* craft;
	int objIdxLocal;

	objIdxLocal = objIdx;
	craft = g_objectTable[objIdxLocal].mobj->pCraft;
	if (craft->missionAccountingDone != 1) {
		craft->missionAccountingDone = 1;
		if (g_objectTable[objIdxLocal].objectType != OBJ_ConnectorRod) {

			++g_missionFgStats[flightGroupIdx].outcomeCount[outcomeId];
			if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
				g_missionFgStats[flightGroupIdx].specialCargoOutcome[outcomeId] = 1;
			}

			if (outcomeId == 2) {
				if (craft->wasCaptured) {
					--g_missionFgStats[flightGroupIdx].outcomeCount[6];
					if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[6] = 0;
					}
					craft->wasCaptured = 0;
				}
				if (craft->aiFlight.departTimerFlag) {
					--g_missionFgStats[flightGroupIdx].outcomeCount[22];
					if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[22] = 0;
					}
				}
				if (craft->aiFlight.missionAbortedFlag) {
					--g_missionFgStats[flightGroupIdx].outcomeCount[21];
					if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[21] = 0;
					}
				}
				++g_missionFgStats[flightGroupIdx].outcomeCount[24];
				if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
					g_missionFgStats[flightGroupIdx].specialCargoOutcome[24] = 1;
				}
			} else if (g_objectTable[objIdxLocal].playerOwnerIdx != -1 && !craft->aiFlight.departTimerFlag &&
					   !craft->aiFlight.missionAbortedFlag &&
					   g_missionFlightRuntimeState.teamGoalStatus[g_objectTable[objIdxLocal].mobj->team]
																 [TEAM_GOAL_PRIMARY] != 1) {
				++g_missionFgStats[flightGroupIdx].outcomeCount[22];
				if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
					g_missionFgStats[flightGroupIdx].specialCargoOutcome[22] = 1;
				}
			}

			{
				const uint8_t* iffVisibility;
				int teamIdx;
				int remainingTeams;

				teamIdx = 0;
				iffVisibility = craft->iffVisibility;
				remainingTeams = 10;
				do {
					if ((int8_t)*iffVisibility < 1) {
						++g_missionFgStats[flightGroupIdx].teamUninspectedLost[teamIdx];
						if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
							g_missionFgStats[flightGroupIdx].teamSpecialCargoUninspectedLost[teamIdx] = 1;
						}
					}
					++teamIdx;
					++iffVisibility;
					--remainingTeams;
				} while (remainingTeams);
			}

			{
				const uint8_t* iffVisibility;
				int teamIdx;
				int remainingTeams;

				teamIdx = 0;
				iffVisibility = craft->iffVisibility;
				remainingTeams = 10;
				do {
					if ((int8_t)*iffVisibility < 0) {
						++g_missionFgStats[flightGroupIdx].teamPartialInspectLost[teamIdx];
						if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
							g_missionFgStats[flightGroupIdx].teamSpecialCargoPartialInspectLost[teamIdx] = 1;
						}
					}
					++teamIdx;
					++iffVisibility;
					--remainingTeams;
				} while (remainingTeams);
			}

			if (!craft->notDisabledAccountingSuppress) {
				++g_missionFgStats[flightGroupIdx].outcomeCount[15];
				if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
					g_missionFgStats[flightGroupIdx].specialCargoOutcome[15] = 1;
				}
			}

			if (!craft->wasCaptured) {
				int fgTeam;

				++g_missionFgStats[flightGroupIdx].outcomeCount[7];
				if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
					g_missionFgStats[flightGroupIdx].specialCargoOutcome[7] = 1;
				}

				fgTeam = g_missionFlightGroups[flightGroupIdx].fg.team;
				{
					uint8_t* teamCondition44OtherTeamCount;
					uint8_t* teamCondition44OtherTeamSpecialCargo;
					int teamIdx;

					teamIdx = 0;
					teamCondition44OtherTeamCount =
						g_missionFgStats[flightGroupIdx].teamCondition44OtherTeamCount;
					teamCondition44OtherTeamSpecialCargo =
						g_missionFgStats[flightGroupIdx].teamCondition44OtherTeamSpecialCargo;
					do {
						if (teamIdx != fgTeam) {
							++*teamCondition44OtherTeamCount;
							*teamCondition44OtherTeamSpecialCargo = 1;
						}
						++teamIdx;
						++teamCondition44OtherTeamCount;
						++teamCondition44OtherTeamSpecialCargo;
					} while (teamIdx < 10);
				}
			}

			{
				const char* attackedByTeam;
				int16_t anyAttackerTeam;
				int remainingTeams;

				anyAttackerTeam = 0;
				attackedByTeam = craft->attackedByTeam;
				remainingTeams = 10;
				do {
					if (*attackedByTeam == 1) {
						anyAttackerTeam = 1;
					}
					++attackedByTeam;
					--remainingTeams;
				} while (remainingTeams);
				if (!anyAttackerTeam) {
					++g_missionFgStats[flightGroupIdx].outcomeCount[5];
					if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
						g_missionFgStats[flightGroupIdx].specialCargoOutcome[5] = 1;
					}
				}
			}

			if (!craft->aiFlight.orderActionCounter) {
				++g_missionFgStats[flightGroupIdx].outcomeCount[11];
				if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
					g_missionFgStats[flightGroupIdx].specialCargoOutcome[11] = 1;
				}
			}

			if (!craft->aiFlight.objSignatureCount) {
				++g_missionFgStats[flightGroupIdx].outcomeCount[13];
				if (g_missionFlightGroups[flightGroupIdx].fg.specialCargoCraft == craft->waveNumber) {
					g_missionFgStats[flightGroupIdx].specialCargoOutcome[13] = 1;
				}
			}

			if (outcomeId == 2) {
				uint16_t destroyedFlightGroupIdx;
				uint16_t fgIdx;
				unsigned int rgnIdx;

				destroyedFlightGroupIdx = flightGroupIdx;
				for (fgIdx = 0; fgIdx < (int16_t)g_missionHeader.numFlightGroups; ++fgIdx) {
					if (fgIdx != destroyedFlightGroupIdx && g_missionFgStats[fgIdx].arrivalEnabled) {
						if (g_missionFlightGroups[fgIdx].fg.arrivalMethod != 0 &&
							g_missionFlightGroups[fgIdx].fg.arrivalMothership == destroyedFlightGroupIdx) {
							Mission_CloseUnavailableFlightGroupAccounting(fgIdx);
						}
						if (g_missionFlightGroups[fgIdx].fg.departMethod == 1 &&
							g_missionFlightGroups[fgIdx].fg.departureMothership == destroyedFlightGroupIdx) {
							g_missionFgStats[fgIdx].outcomeCount[3] =
								(uint16_t)(g_missionFgStats[fgIdx].outcomeCount[3] +
										   g_missionFgStats[fgIdx].outcomeCount[18]);
							g_missionFgStats[fgIdx].outcomeCount[18] = 0;
						}
						if (g_missionFlightGroups[fgIdx].fg.alternateMothershipUsed == 1 &&
							g_missionFlightGroups[fgIdx].fg.alternateMothership == destroyedFlightGroupIdx) {
							g_missionFgStats[fgIdx].outcomeCount[3] =
								(uint16_t)(g_missionFgStats[fgIdx].outcomeCount[3] +
										   g_missionFgStats[fgIdx].outcomeCount[19]);
							g_missionFgStats[fgIdx].outcomeCount[19] = 0;
						}
						if (g_missionFlightGroups[fgIdx].fg.capturedDepartViaMothership == 1 &&
							g_missionFlightGroups[fgIdx].fg.capturedDepartureMothership ==
								destroyedFlightGroupIdx) {
							g_missionFgStats[fgIdx].outcomeCount[3] =
								(uint16_t)(g_missionFgStats[fgIdx].outcomeCount[3] +
										   g_missionFgStats[fgIdx].outcomeCount[20]);
							g_missionFgStats[fgIdx].outcomeCount[20] = 0;
						}
					}
				}

				for (rgnIdx = 0; rgnIdx < (unsigned int)g_activeMissionRegionCount; ++rgnIdx) {
					int orderSlot;

					for (orderSlot = 0; orderSlot < 4; ++orderSlot) {
						const XwaOrder* order;
						const uint8_t* dropoffFlightGroup;
						uint8_t planId;

						order = &g_missionFlightGroups[flightGroupIdx].fg.orders[4 * rgnIdx + orderSlot];
						dropoffFlightGroup = &order->variable2;
						planId = g_builtinPlanIdByNameIndex[g_orderLeaderBuiltinPlanNameIndex[order->order]];
						if (strcmp(g_planTable[planId].name, "dropoffldr1pln") == 0) {
							Mission_CloseUnavailableFlightGroupAccounting(*dropoffFlightGroup);
						}
					}
				}
			}

			if (outcomeId == 17) {
				uint16_t clearedCount;
				uint16_t departedFlightGroupIdx;
				uint16_t fgIdx;
				int16_t numFlightGroups;

				clearedCount = 0;
				departedFlightGroupIdx = flightGroupIdx;
				numFlightGroups = (int16_t)g_missionHeader.numFlightGroups;
				for (fgIdx = 0; fgIdx < numFlightGroups; ++fgIdx) {
					if (fgIdx != departedFlightGroupIdx && g_missionFgStats[fgIdx].arrivalEnabled) {
						if (g_missionFlightGroups[fgIdx].fg.departMethod == 1 &&
							g_missionFlightGroups[fgIdx].fg.departureMothership == departedFlightGroupIdx) {
							g_missionFgStats[fgIdx].outcomeCount[17] =
								(uint16_t)(g_missionFgStats[fgIdx].outcomeCount[17] +
										   g_missionFgStats[fgIdx].outcomeCount[18]);
							g_missionFgStats[fgIdx].outcomeCount[18] = clearedCount;
						}
						if (g_missionFlightGroups[fgIdx].fg.alternateMothershipUsed == 1 &&
							g_missionFlightGroups[fgIdx].fg.alternateMothership == departedFlightGroupIdx) {
							g_missionFgStats[fgIdx].outcomeCount[17] =
								(uint16_t)(g_missionFgStats[fgIdx].outcomeCount[17] +
										   g_missionFgStats[fgIdx].outcomeCount[19]);
							g_missionFgStats[fgIdx].outcomeCount[19] = clearedCount;
						}
						if (g_missionFlightGroups[fgIdx].fg.capturedDepartViaMothership == 1 &&
							g_missionFlightGroups[fgIdx].fg.capturedDepartureMothership ==
								departedFlightGroupIdx) {
							g_missionFgStats[fgIdx].outcomeCount[17] =
								(uint16_t)(g_missionFgStats[fgIdx].outcomeCount[17] +
										   g_missionFgStats[fgIdx].outcomeCount[20]);
							g_missionFgStats[fgIdx].outcomeCount[20] = clearedCount;
						}
					}
				}
			}

			{
				uint16_t emptyObjectType;
				uint16_t invalidObjectIdx;
				uint32_t objectIdx;

				emptyObjectType = OBJ_None;
				invalidObjectIdx = 0xffffu;
				for (objectIdx = g_activeRegionObjectSlotStart; objectIdx < g_activeRegionCraftObjectSlotEnd;
					 ++objectIdx) {
					ObjectRecord* activeObject;

					activeObject = &g_objectTable[objectIdx];
					if (activeObject->objectType != emptyObjectType) {
						CraftData* activeCraft;

						activeCraft = activeObject->mobj->pCraft;
						if (activeCraft->lastAttackerObjIdx == objIdx) {
							activeCraft->lastAttackerObjIdx = invalidObjectIdx;
						}
					}
				}
			}

			{
				int playerIdx;

				for (playerIdx = 0; playerIdx < XWA_PLAYER_COUNT; ++playerIdx) {
					if (g_players[playerIdx].connectedFlag) {
						int slotIdx;
						int16_t* targetPresetSlot;

						targetPresetSlot = g_players[playerIdx].targetPresetSlot;
						for (slotIdx = 0; slotIdx < 4; ++slotIdx) {
							if (targetPresetSlot[slotIdx] == (int16_t)objIdx) {
								targetPresetSlot[slotIdx] = -1;
							}
						}
					}
				}
			}
		}
	}
}

// FUNCTION: XWA 0x4DA130
void Mission_ApplyTeamGoalScoreAllEnabledTeams(int16_t eventCondition, uint16_t flightGroupIdx,
											   int specialCargoFlag) {
	XwaFlightGroupGoalPayload* goal;
	int goalIdx;
	int scoreTenths;

	if ((int)flightGroupIdx > (int16_t)g_missionHeader.numFlightGroups) {
		return;
	}

	scoreTenths = eventCondition;
	for (goalIdx = 0; goalIdx < 8; ++goalIdx) {
		goal = &g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload;
		if (goal->argument == 2 && (uint16_t)goal->condition == (uint16_t)eventCondition &&
			(goal->amount == 18 || (goal->amount == 19 && specialCargoFlag == 1))) {
			unsigned int missionTimeSeconds;
			int teamIdx;
			unsigned int timeLimitSeconds;

			missionTimeSeconds = g_missionElapsedClock.seconds +
								 60 * (g_missionElapsedClock.minutes + 60 * g_missionElapsedClock.hours);
			timeLimitSeconds = 5u * (unsigned int)goal->parameter;
			if (timeLimitSeconds == 0 || missionTimeSeconds <= timeLimitSeconds) {
				scoreTenths = 250 * (int)goal->points;
			} else {
				continue;
			}
			for (teamIdx = 0; teamIdx < 10; ++teamIdx) {
				if (goal->enabledForTeam[teamIdx] != 0) {
					g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS][teamIdx] += scoreTenths;
				}
			}
		}
	}
}

// FUNCTION: XWA 0x4DA210
void Mission_ApplyTeamGoalScoreForTeam(int eventCondition, uint16_t flightGroupIdx, int specialCargoFlag,
									   uint8_t teamIdx) {
	int scoreTenths;
	int goalIdx;

	if ((int)flightGroupIdx > (int16_t)g_missionHeader.numFlightGroups) {
		return;
	}

	scoreTenths = eventCondition;
	for (goalIdx = 0; goalIdx < 8; ++goalIdx) {
		if (g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.argument == 2 &&
			(uint16_t)g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.condition ==
				(uint16_t)eventCondition &&
			(g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.amount == 18 ||
			 (g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.amount == 19 &&
			  specialCargoFlag == 1))) {
			unsigned int missionTimeSeconds;
			unsigned int timeLimitSeconds;

			missionTimeSeconds = g_missionElapsedClock.seconds +
								 60 * (g_missionElapsedClock.minutes + 60u * g_missionElapsedClock.hours);
			timeLimitSeconds =
				5u * g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.parameter;
			if (timeLimitSeconds == 0 || missionTimeSeconds <= timeLimitSeconds) {
				scoreTenths = g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.points;
			}
			scoreTenths *= 250;
			if (g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.enabledForTeam[teamIdx] !=
				0) {
				int* teamScores;
				int teamScore;

				teamScores = g_missionFlightRuntimeState.teamScores[TEAM_SCORE_BONUS_TENTHS];
				teamScore = scoreTenths;
				teamScore += teamScores[teamIdx];
				teamScores[teamIdx] = teamScore;
			}
		}
	}
}

// FUNCTION: XWA 0x4D9F20
int Mission_ApplyFlightGroupGoalScore(int16_t eventCondition, uint16_t flightGroupIdx, int playerIdx,
									  uint16_t scoreDivisor, int specialCargoFlag, int teamIdx) {
	int goalIdx;
	int scoreDeltaTotal;
	unsigned int missionTimeSeconds;

	if (flightGroupIdx > (uint16_t)g_missionHeader.numFlightGroups) {
		return 0;
	}

	scoreDeltaTotal = 0;
	missionTimeSeconds = g_missionElapsedClock.seconds +
						 60 * (g_missionElapsedClock.minutes + 60 * g_missionElapsedClock.hours);
	for (goalIdx = 0; goalIdx < 8; ++goalIdx) {
		int scoreDelta;
		int scoreTenth;
		int timeLimitSeconds;

		if ((g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.argument == 2 &&
			 g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.condition == eventCondition &&
			 g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.amount == 18 &&
			 g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.enabledForTeam[teamIdx] !=
				 0) ||
			(g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.argument == 2 &&
			 g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.condition == eventCondition &&
			 g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.amount == 19 &&
			 g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.enabledForTeam[teamIdx] != 0 &&
			 specialCargoFlag == 1)) {
			timeLimitSeconds =
				5 * (int)g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.parameter;
			if (timeLimitSeconds == 0 || missionTimeSeconds <= (unsigned int)timeLimitSeconds) {
				scoreDelta =
					250 * (int)g_missionFlightGroups[flightGroupIdx].fg.fgGoals[goalIdx].payload.points;
				scoreTenth = scoreDelta / 10;
				if (scoreDivisor > 10u) {
					scoreDivisor = 9;
				}
				if (scoreDivisor > 1u) {
					if (scoreDelta > 0) {
						scoreDelta -= scoreTenth * ((int)scoreDivisor - 1);
					} else {
						scoreDelta += scoreTenth * ((int)scoreDivisor - 1);
					}
				}

				scoreDeltaTotal += scoreDelta;
				if (playerIdx != -1) {
					g_players[playerIdx].missionStats.missionBonusScoreTenths += scoreDelta;
				}
			}
		}
	}

	if (scoreDeltaTotal != 0 && playerIdx == g_localPlayer) {
		if (scoreDeltaTotal > 0) {
			g_msgArgTable[0] = (uint16_t)((uint16_t)scoreDeltaTotal / 10);
			msg_emitInFlightMessage(MSG_BONUS_POINTS, playerIdx);
		} else {
			g_msgArgTable[0] = (uint16_t)(scoreDeltaTotal / -10);
			msg_emitInFlightMessage(MSG_PENALTY_POINTS, playerIdx);
		}
	}

	return scoreDeltaTotal;
}

// FUNCTION: XWA 0x4DA370
// Loadout point-value component for a craft type plus its selected warhead / beam /
// countermeasure types (used by combat-sim team point totals). Craft with a model
// (familyId == 0) sum per-launcher warhead values; satellites/probes use fixed
// non-craft values.
int Mission_ComputeCraftLoadoutPointValue(int craftType, int warheadType, int beamType,
										  int countermeasureType) {
	int loadoutPointValue;
	int objType = g_objectTypeTables.craftTypeToObjectType[craftType];
	if (g_modelTypeTable[objType].familyId == 0) {
		unsigned int launcherIdx;
		int modelIndex = (uint16_t)GetModelIndexFromType((ObjectTypeId)objType);
		if (modelIndex == 0xFFFF)
			return 0;

		loadoutPointValue = 0;
		for (launcherIdx = 0; launcherIdx < 2; ++launcherIdx) {
			if (g_modelDefs[modelIndex].warheadLauncherType[1] != 0 && launcherIdx == 0) {
				int rounds = g_modelDefs[modelIndex].warheadLauncherValue[0];
				rounds = (uint8_t)MATH2_fraction(rounds, g_warheadAmmoCounts[3]);
				if (rounds == 0)
					rounds = 1;
				loadoutPointValue += g_warheadProjectilePointValue[1] * rounds;
			}

			if (((g_modelDefs[modelIndex].warheadLauncherType[1] != 0 && launcherIdx == 1) ||
				 (g_modelDefs[modelIndex].warheadLauncherType[1] == 0 && launcherIdx == 0)) &&
				warheadType != 0) {
				int rounds2 = g_modelDefs[modelIndex].warheadLauncherValue[launcherIdx];
				rounds2 = (uint8_t)MATH2_fraction(rounds2, g_warheadAmmoCounts[warheadType]);
				if (rounds2 == 0)
					rounds2 = 1;
				loadoutPointValue +=
					(g_modelDefs[modelIndex].warheadLauncherLastSlot[launcherIdx] -
					 g_modelDefs[modelIndex].warheadLauncherFirstSlot[launcherIdx] + 1) *
					g_warheadProjectilePointValueByObjectType[g_warheadTypeIds[warheadType]] * rounds2;
			}
		}

		loadoutPointValue = g_beamTypePointValue[beamType] +
							g_countermeasureTypePointValue[countermeasureType] + loadoutPointValue;
	} else {
		if ((objType < OBJ_CommSat1 || objType > OBJ_CommSat3) &&
			(objType < OBJ_Probe || objType > OBJ_NavBuoy3))
			loadoutPointValue = 10;
		else
			loadoutPointValue = 500;
		loadoutPointValue *= 3;
	}

	return loadoutPointValue;
}

// FUNCTION: XWA 0x4DA500
unsigned int Mission_ComputeCraftPointValue(int objIdx) {
	CraftData* craft;
	unsigned int modelIndex;
	unsigned int points;
	unsigned int groupAI;
	unsigned int launcherIdx;

	craft = g_objectTable[objIdx].mobj->pCraft;
	modelIndex = GetModelIndexFromType(g_objectTable[objIdx].objectType);
	if (modelIndex == 0xffffu) {
		return 0;
	}

	points = g_modelDefs[modelIndex].craftPointValue;
	switch (g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.groupAI) {
		case 0:
			break;
		case 1:
			points += points >> 1;
			break;
		default:
			points *= g_missionFlightGroups[g_objectTable[objIdx].flightGroupIdx].fg.groupAI;
			break;
	}

	for (launcherIdx = 0; launcherIdx < craft->warheadLauncherCount; ++launcherIdx) {
		unsigned int projectileType;
		unsigned int warheadCount;

		projectileType = craft->warheadSlotTypeIds[launcherIdx];
		warheadCount =
			craft->warheadData[g_modelDefs[modelIndex].warheadLauncherFirstSlot[launcherIdx]].count +
			craft->warheadData[g_modelDefs[modelIndex].warheadLauncherLastSlot[launcherIdx]].count;
		points += g_warheadProjectilePointValueByObjectType[projectileType] * warheadCount;
	}

	return points + g_beamTypePointValue[craft->beamTypeId] + g_countermeasureTypePointValue[craft->cmTypeId];
}

// FUNCTION: XWA 0x509530
char Mission_ShouldApplyEndMissionPenalty(unsigned int playerIdx) {
	unsigned int missionRegionCount;
	ObjectRecord* objects;
	unsigned int regionObjIdx;
	unsigned int regionObjEnd;
	unsigned int hostileFighterCount;
	unsigned int friendlyFighterCount;
	unsigned int nearestHostileRange;
	unsigned int nearestFriendlyRange;
	int savedTargetX;
	int savedTargetY;
	int savedTargetZ;

	missionRegionCount = (uint32_t)g_missionRegionCount;
	regionObjEnd = g_regionObjectSlotEnd / missionRegionCount;
	regionObjIdx = regionObjEnd * g_players[playerIdx].regionIndex;
	regionObjEnd = regionObjIdx + g_craftObjectSlotsTotal / missionRegionCount;
	savedTargetX = g_players[playerIdx].viewState.savedTargetX;
	savedTargetY = g_players[playerIdx].viewState.savedTargetY;
	savedTargetZ = g_players[playerIdx].viewState.savedTargetZ;
	hostileFighterCount = 0;
	friendlyFighterCount = 0;
	nearestHostileRange = 0xffffffffu;
	nearestFriendlyRange = 0xffffffffu;

	if (regionObjIdx < regionObjEnd) {
		MobileObject* mobj;
		CraftData* craft;
		unsigned int team;
		unsigned int playerIff;

		objects = g_objectTable;
		do {
			if (objects[regionObjIdx].objectType != OBJ_None) {
				mobj = objects[regionObjIdx].mobj;
				craft = mobj->pCraft;
				if (craft->workingSubsystems != 0) {
					playerIff = (uint16_t)g_players[playerIdx].playerIff;
					team = mobj->team;
					if (playerIff != team && g_missionTeams[team].allies[playerIff] == 0) {
						switch (objects[regionObjIdx].genusId) {
							case GENUS_Fighter:
								++hostileFighterCount;
								break;

							case GENUS_Transport:
							case GENUS_Freighter:
							case GENUS_Starship:
							case GENUS_Platform:
								if (pai_IsObjectTargetable(regionObjIdx)) {
									unsigned int range;

									range = (unsigned int)collide_roughdistance3d(
										savedTargetX - g_objectTable[regionObjIdx].world_x,
										savedTargetY - g_objectTable[regionObjIdx].world_y,
										savedTargetZ - g_objectTable[regionObjIdx].world_z);
									g_targetRangeScore = (int)range;
									if (range < nearestHostileRange) {
										nearestHostileRange = range;
									}
								}
								objects = g_objectTable;
								break;

							default:
								break;
						}
					} else if (playerIff == team || g_missionTeams[team].allies[playerIff] == 1) {
						switch (objects[regionObjIdx].genusId) {
							case GENUS_Fighter:
								++friendlyFighterCount;
								break;

							case GENUS_Transport:
							case GENUS_Freighter:
							case GENUS_Starship:
							case GENUS_Platform:
								if (pai_IsObjectTargetable(regionObjIdx)) {
									unsigned int range;

									range = (unsigned int)collide_roughdistance3d(
										savedTargetX - g_objectTable[regionObjIdx].world_x,
										savedTargetY - g_objectTable[regionObjIdx].world_y,
										savedTargetZ - g_objectTable[regionObjIdx].world_z);
									g_targetRangeScore = (int)range;
									if (range < nearestFriendlyRange) {
										nearestFriendlyRange = range;
									}
								}
								objects = g_objectTable;
								break;

							default:
								break;
						}
					}
				}
			}

			++regionObjIdx;
		} while (regionObjIdx < regionObjEnd);
	}

	if (g_players[playerIdx].objectIndex == 0xffff ||
		objects[g_players[playerIdx].objectIndex].objectType == OBJ_None) {
		return nearestHostileRange < nearestFriendlyRange;
	}

	return hostileFighterCount > 1u && hostileFighterCount > friendlyFighterCount;
}
