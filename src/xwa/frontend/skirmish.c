#include "xwa/frontend/skirmish.h"

#include "xwa/assets/ship_list.h"
#include "xwa/assets/string_table.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/briefing_script.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/math/scalar.h"
#include "xwa/util/memory.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GLOBAL: XWA 0x5AB950
const double g_skirmishMinimumPlacementDistance = 79.5;
// GLOBAL: XWA 0x5AB998
const double g_skirmishWaypointRadiusNegative = -79.5;
// GLOBAL: XWA 0x5AB9A8
const double g_skirmishPiNegative = -3.141592653589793;
// GLOBAL: XWA 0x5AB9B0
const double g_skirmishCraftSeparationScale = 0.159;
// GLOBAL: XWA 0x5AB9B8
const double g_skirmishRandomAngleCenter = 1178.0;
// GLOBAL: XWA 0x5AB9C0
const double g_skirmishRandomAngleScale = 0.001;

// GLOBAL: XWA 0x605120
const double g_skirmishTeamSpawnAnglesRad[8] = {
	1.5707963267948966, 4.71238898038469,  0.0, 3.141592653589793, 0.7853981633974483, 3.9269908169872414,
	2.356194490192345,  5.497787143782138,
};

// GLOBAL: XWA 0x7849A0
int g_skirmishTeamHasCaptureRole[10];
// GLOBAL: XWA 0x7849C8
int g_skirmishTeamHasStrikeRole[10];
// GLOBAL: XWA 0x7849F0
int g_skirmishRegionHasCraft[4];
// GLOBAL: XWA 0x784A00
int g_skirmishTeamHasSuperiorityRole[10];
// GLOBAL: XWA 0x784A28
int g_skirmishTeamHasDisableRole[10];
// GLOBAL: XWA 0x784A50
int g_skirmishTeamHasPrimaryFg[10];
// GLOBAL: XWA 0x784A78
int g_skirmishTeamHasReconRole[10];
// GLOBAL: XWA 0x784AA0
int g_skirmishTeamHasEscortRole[10];

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x57E370
int Skirmish_SetupProvingGroundsSession(void) {
	int slotIndex;

	strcpy(g_mpRoster[0].name, g_pilotData.name);
	g_mpRoster[0].playerId = 1;
	g_mpRoster[0].rating = g_pilotData.pilotRating;
	g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_SINGLEPLAYER;

	g_pilotData.missionDirectoryId = MISSION_DIRECTORY_MELEE;
	g_pilotData.missionDescriptionIds[MISSION_DIRECTORY_MELEE] = 66;
	g_pilotData.campaignMode = 0;

	FrontendMission_LoadCurrent();
	MissionSetup_CountActiveTeams();
	MissionSetup_RebuildCombatSimSlotsFromFrontendMission();

	for (slotIndex = 0; slotIndex < 16; ++slotIndex) {
		if (g_combatSimSlots[slotIndex].craftType != 0) {
			g_combatSimSlots[slotIndex].ownerPlayerId = g_mpRoster[0].playerId;
			break;
		}
	}

	g_pilotData.team = 0;
	g_unusedFrontendMissionLaunchPrepared = 1;
	g_skipFrontendEntryMovie = 1;
	FrontendMission_InitPlayerState();
	return 1;
}

// FUNCTION: XWA 0x57B400
int Skirmish_InitMissionDefaults(void) {
	enum {
		MISSION_FORMAT_VERSION = 18,
		MAX_REGION_COUNT = 4,
		MAX_IFF_NAME_COUNT = 4,
		MAX_GLOBAL_CARGO_COUNT = 16,
		MAX_MESSAGE_COUNT = 64,
		MAX_GLOBAL_GOAL_TEAMS = 10,
		MAX_GLOBAL_GOALS_PER_TEAM = 7,
		MAX_TEAM_COUNT = 10,
		MAX_FLIGHT_GROUP_COUNT = 192,
		BRIEFING_ACTIVE_RESET_COUNT = 2,
	};
	int regionIndex;
	int iffIndex;
	int cargoIndex;
	int messageIndex;
	int teamIndex;
	int globalGoalIndex;
	int flightGroupIndex;
	int allyIndex;
	int resetPass;
	int textTailOffset;

	memset(g_frontendMission, 0, sizeof(*g_frontendMission));

	g_frontendMission->flightGroupCount = 1;
	g_frontendMission->header.legacyAllWayShown = 0;
	g_frontendMission->header.legacyWinType = 1;
	g_frontendMission->header.missionType = XWA_MISSION_TYPE_SKIRMISH;

	for (regionIndex = 0; regionIndex < MAX_REGION_COUNT; ++regionIndex) {
		sprintf(g_frontendMission->header.regions[regionIndex].name, "Region %d", regionIndex + 1);
		g_frontendMission->header.regions[regionIndex].id = 0;
	}

	for (iffIndex = 0; iffIndex < MAX_IFF_NAME_COUNT; ++iffIndex) {
		sprintf(g_frontendMission->header.iffNames[iffIndex], "Neutral %d", iffIndex + 1);
	}

	for (cargoIndex = 0; cargoIndex < MAX_GLOBAL_CARGO_COUNT; ++cargoIndex) {
		memset(g_frontendMission->header.globalCargos[cargoIndex].name, 0,
			   sizeof(g_frontendMission->header.globalCargos[0].name));
		g_frontendMission->header.globalCargos[cargoIndex].id = 0;
		g_frontendMission->header.globalCargos[cargoIndex].count = 1;
		g_frontendMission->header.globalCargos[cargoIndex].type = 0;
		g_frontendMission->header.globalCargos[cargoIndex].volume = 0;
		g_frontendMission->header.globalCargos[cargoIndex].value = 0;
		g_frontendMission->header.globalCargos[cargoIndex].volatility = 0;
	}

	g_frontendMission->formatVersion = MISSION_FORMAT_VERSION;
	g_frontendMission->header.secondaryVersion = FRONTEND_BRIEFING_SECONDARY_VERSION_98;

	for (messageIndex = 0; messageIndex < MAX_MESSAGE_COUNT; ++messageIndex) {
		g_frontendMission->messages[messageIndex].triggers[0].triggers[0].condition = 10;
		g_frontendMission->messages[messageIndex].triggers[0].triggers[1].condition = 10;
		g_frontendMission->messages[messageIndex].triggers[0].t1OrT2 = 1;
		g_frontendMission->messages[messageIndex].triggers[1].triggers[0].condition = 10;
		g_frontendMission->messages[messageIndex].triggers[1].triggers[1].condition = 10;
		g_frontendMission->messages[messageIndex].triggers[1].t1OrT2 = 1;
		g_frontendMission->messages[messageIndex].triggers12OrTriggers34 = 1;
		g_frontendMission->messages[messageIndex].special.triggers[0].condition = 10;
		g_frontendMission->messages[messageIndex].special.triggers[1].condition = 10;
		g_frontendMission->messages[messageIndex].special.t1OrT2 = 1;
		g_frontendMission->messages[messageIndex].triggers12OrTriggers34 = 1;
	}

	for (teamIndex = 0; teamIndex < MAX_GLOBAL_GOAL_TEAMS; ++teamIndex) {
		for (globalGoalIndex = 0; globalGoalIndex < MAX_GLOBAL_GOALS_PER_TEAM; ++globalGoalIndex) {
			g_frontendMission->globalGoals[teamIndex][globalGoalIndex].triggerPairs[0].triggers[0].condition =
				10;
			g_frontendMission->globalGoals[teamIndex][globalGoalIndex].triggerPairs[0].triggers[1].condition =
				10;
			g_frontendMission->globalGoals[teamIndex][globalGoalIndex].triggerPairs[0].t1OrT2 = 1;
			g_frontendMission->globalGoals[teamIndex][globalGoalIndex].triggerPairs[1].triggers[0].condition =
				10;
			g_frontendMission->globalGoals[teamIndex][globalGoalIndex].triggerPairs[1].triggers[1].condition =
				10;
			g_frontendMission->globalGoals[teamIndex][globalGoalIndex].triggerPairs[1].t1OrT2 = 1;
			g_frontendMission->globalGoals[teamIndex][globalGoalIndex].t12AndOrT34 = 1;
		}
	}

	for (teamIndex = 0; teamIndex < MAX_TEAM_COUNT; ++teamIndex) {
		sprintf(g_frontendMission->teams[teamIndex].name, "Team %d", teamIndex + 1);
		for (allyIndex = 0; allyIndex < MAX_TEAM_COUNT; ++allyIndex) {
			g_frontendMission->teams[teamIndex].allies[allyIndex] = 0;
		}
	}

	for (flightGroupIndex = 0; flightGroupIndex < MAX_FLIGHT_GROUP_COUNT; ++flightGroupIndex) {
		Skirmish_ResetFlightGroupDefaults(flightGroupIndex);
	}

	BriefingScript_InitDefaultScript();

	for (resetPass = 0; resetPass < BRIEFING_ACTIVE_RESET_COUNT; ++resetPass) {
		memset(g_briefingMapFgMarkers.active, 0, sizeof(g_briefingMapFgMarkers.active));
		memset(g_briefingMapLabels.active, 0, sizeof(g_briefingMapLabels.active));
		memset(g_briefingTextSlotActive, 0, sizeof(g_briefingTextSlotActive));
	}

	for (textTailOffset = offsetof(FrontendMission, textTail.fgGoalStrings);
		 textTailOffset < (int)offsetof(FrontendMission, textTail.globalGoalStrings);
		 textTailOffset += (int)sizeof(g_frontendMission->textTail.fgGoalStrings[0][0][0])) {
		memset((char*)g_frontendMission + textTailOffset, 0,
			   sizeof(g_frontendMission->textTail.fgGoalStrings[0][0][0]));
	}

	for (textTailOffset = offsetof(FrontendMission, textTail.globalGoalStrings);
		 textTailOffset < (int)offsetof(FrontendMission, textTail.orderStrings);
		 textTailOffset += (int)sizeof(g_frontendMission->textTail.globalGoalStrings[0][0][0])) {
		memset((char*)g_frontendMission + textTailOffset, 0,
			   sizeof(g_frontendMission->textTail.globalGoalStrings[0][0][0]));
	}

	memset(g_frontendMission->textTail.missionDescriptionText, 0,
		   sizeof(g_frontendMission->textTail.missionDescriptionText));
	memset(g_frontendMission->textTail.missionSuccessfulText, 0,
		   sizeof(g_frontendMission->textTail.missionSuccessfulText));
	memset(g_frontendMission->textTail.missionFailedText, 0,
		   sizeof(g_frontendMission->textTail.missionFailedText));

	return 1;
}

// FUNCTION: XWA 0x57B790
void Skirmish_ResetFlightGroupDefaults(int flightGroupIndex) {
	XwaFlightGroup* flightGroup;
	int optionIndex;
	int orderIndex;
	int waypointIndex;
	int skipTriggerIndex;
	int missionPointIndex;
	int goalIndex;

	flightGroup = &g_frontendMission->flightGroups[flightGroupIndex];

	flightGroup->name[0] = '\0';
	flightGroup->comm = 2;
	flightGroup->enableDesignation = 0xFF;
	flightGroup->enableDesignation2 = 0xFF;
	flightGroup->designation1 = 0;
	flightGroup->designation2 = 0;
	flightGroup->craftType = 1;
	flightGroup->numberOfCraft = 1;
	flightGroup->status1 = 0;
	flightGroup->status2 = 0;
	flightGroup->warhead = 0;
	flightGroup->beam = 0;
	flightGroup->iff = 0;
	flightGroup->team = 0;
	flightGroup->groupAI = 2;
	flightGroup->markings = 0;
	flightGroup->radio = 0;
	flightGroup->yaw = 0;
	flightGroup->pitch = 64;
	flightGroup->roll = 0;
	flightGroup->globalGroup = 0;
	flightGroup->unused0x79 = 0;
	flightGroup->formation = 0;
	flightGroup->formationSpacing = 6;
	flightGroup->globalUnit = 0;
	flightGroup->disableWaveNumbering = 0;
	flightGroup->optionalCraftCategory = 0;

	for (optionIndex = 0; optionIndex < 10; ++optionIndex) {
		flightGroup->optionalCraft[optionIndex] = 0;
		flightGroup->numberOfOptionalCraft[optionIndex] = 0;
		flightGroup->numberOfOptionalCraftWaves[optionIndex] = 0;
	}

	for (optionIndex = 0; optionIndex < 8; ++optionIndex) {
		flightGroup->optionalWarheads[optionIndex] = 0;
	}
	for (optionIndex = 0; optionIndex < 6; ++optionIndex) {
		flightGroup->optionalBeams[optionIndex] = 0;
	}
	for (optionIndex = 0; optionIndex < 4; ++optionIndex) {
		flightGroup->optionalCountermeasures[optionIndex] = 0;
	}
	flightGroup->optionalCraftCategory = 0;
	flightGroup->optionalCraft[0] = 0;
	flightGroup->handicap = 0;

	memset(flightGroup->craftRole, 0, sizeof(flightGroup->craftRole));
	flightGroup->globalCargoIndex = 0xFF;
	flightGroup->globalSpecialCargoIndex = 0xFF;
	flightGroup->cargo[0] = '\0';
	flightGroup->specialCargo[0] = '\0';
	flightGroup->specialCargoCraft = flightGroup->numberOfCraft;
	flightGroup->randomSpecialCargoCraft = 0;
	flightGroup->playerNumber = 0;
	flightGroup->arriveOnlyIfHuman = 0;
	flightGroup->playerCraft = 1;
	flightGroup->numberOfWaves = 0;
	flightGroup->wavesDelay = 0;
	flightGroup->stopArrivingWhen = 0;
	flightGroup->craftExplosionTime = 0;
	flightGroup->linkId = 0;
	flightGroup->unused0x86 = 0;
	flightGroup->arrivalDifficulty = 0;

	flightGroup->arrival[0].triggers[0].condition = 0;
	flightGroup->arrival[0].triggers[0].variableType = 1;
	flightGroup->arrival[0].triggers[0].variable = 0;
	flightGroup->arrival[0].triggers[0].amount = 0;
	flightGroup->arrival[0].triggers[0].parameter = 0;
	flightGroup->arrival[0].triggers[1].condition = 0;
	flightGroup->arrival[0].triggers[1].variableType = 1;
	flightGroup->arrival[0].triggers[1].variable = 0;
	flightGroup->arrival[0].triggers[1].amount = 0;
	flightGroup->arrival[0].triggers[1].parameter = 0;
	flightGroup->arrival[0].t1OrT2 = 0;
	flightGroup->arrival[1].triggers[0].condition = 0;
	flightGroup->arrival[1].triggers[0].variableType = 0;
	flightGroup->arrival[1].triggers[0].variable = 0;
	flightGroup->arrival[1].triggers[0].amount = 0;
	flightGroup->arrival[1].triggers[0].parameter = 0;
	flightGroup->arrival[1].triggers[1].condition = 0;
	flightGroup->arrival[1].triggers[1].variableType = 0;
	flightGroup->arrival[1].triggers[1].variable = 0;
	flightGroup->arrival[1].triggers[1].amount = 0;
	flightGroup->arrival[1].triggers[1].parameter = 0;
	flightGroup->arrival[1].t1OrT2 = 0;
	flightGroup->arrivals12OrArrivals34 = 0;
	flightGroup->arrivalRandDelayMinutes = 0;
	flightGroup->arrivalRandDelaySeconds = 0;
	flightGroup->arrivalDelayMinutes = 0;
	flightGroup->arrivalDelaySeconds = 0;

	flightGroup->departure.triggers[0].condition = 10;
	flightGroup->departure.triggers[0].variableType = 1;
	flightGroup->departure.triggers[0].variable = 0;
	flightGroup->departure.triggers[0].amount = 0;
	flightGroup->departure.triggers[0].parameter = 0;
	flightGroup->departure.triggers[1].condition = 10;
	flightGroup->departure.triggers[1].variableType = 1;
	flightGroup->departure.triggers[1].variable = 0;
	flightGroup->departure.triggers[1].amount = 0;
	flightGroup->departure.triggers[1].parameter = 0;
	flightGroup->departure.t1OrT2 = 0;
	flightGroup->departureDelayMinutes = 0;
	flightGroup->departureDelaySeconds = 0;
	flightGroup->abortTrigger = 0;
	flightGroup->departureClockMin = 0;
	flightGroup->departureClockSec = 0;
	flightGroup->arrivalMothership = 0;
	flightGroup->arrivalMethod = 0;
	flightGroup->departureMothership = 0;
	flightGroup->departMethod = 0;
	flightGroup->alternateMothership = 0;
	flightGroup->alternateMothershipUsed = 0;
	flightGroup->capturedDepartureMothership = 0;
	flightGroup->capturedDepartViaMothership = 0;

	for (orderIndex = 0; orderIndex < 16; ++orderIndex) {
		XwaOrder* order;
		int targetIndex;

		order = &flightGroup->orders[orderIndex];
		order->order = 0;
		order->throttle = 10;
		order->variable1 = 1;
		order->variable2 = 1;
		order->variable3 = 0;
		order->variable4 = 0;
		for (targetIndex = 0; targetIndex < 2; ++targetIndex) {
			order->secondaryTargetTypes[targetIndex] = 0;
			order->secondaryTargets[targetIndex] = 0;
		}
		order->target3OrTarget4 = 1;
		order->unused0 = 0;
		order->target1Type = 0;
		order->target1 = 0;
		order->target2Type = 0;
		order->target2 = 0;
		order->target1OrTarget2 = 1;
		order->unused1 = 0;
		order->speed = 0;

		for (waypointIndex = 0; waypointIndex < 8; ++waypointIndex) {
			order->waypoints[waypointIndex].x = 0;
			order->waypoints[waypointIndex].y = 0;
			order->waypoints[waypointIndex].z = 0;
			order->waypoints[waypointIndex].enabled = 0;
		}
	}

	for (skipTriggerIndex = 0; skipTriggerIndex < 16; ++skipTriggerIndex) {
		XwaTriggerPair* skipTrigger;

		skipTrigger = &flightGroup->skipTriggers[skipTriggerIndex];
		skipTrigger->triggers[0].condition = 0;
		skipTrigger->triggers[0].variableType = 0;
		skipTrigger->triggers[0].variable = 0;
		skipTrigger->triggers[0].amount = 0;
		skipTrigger->triggers[0].parameter = 0;
		skipTrigger->triggers[1].condition = 0;
		skipTrigger->triggers[1].variableType = 0;
		skipTrigger->triggers[1].variable = 0;
		skipTrigger->triggers[1].amount = 0;
		skipTrigger->triggers[1].parameter = 0;
		skipTrigger->t1OrT2 = 0;
	}

	flightGroup->missionPoints[XWA_FG_POINT_START_1].x = 0;
	flightGroup->missionPoints[XWA_FG_POINT_START_1].y = 0;
	flightGroup->missionPoints[XWA_FG_POINT_START_1].z = 0;
	flightGroup->missionPoints[XWA_FG_POINT_START_1].enabled = 1;
	flightGroup->missionPointRegions[XWA_FG_POINT_START_1] = 0;
	for (missionPointIndex = 1; missionPointIndex < 4; ++missionPointIndex) {
		flightGroup->missionPoints[missionPointIndex].x = 0;
		flightGroup->missionPoints[missionPointIndex].y = 0;
		flightGroup->missionPoints[missionPointIndex].z = 0;
		flightGroup->missionPoints[missionPointIndex].enabled = 0;
		flightGroup->missionPointRegions[missionPointIndex] = 0;
	}

	for (goalIndex = 0; goalIndex < 8; ++goalIndex) {
		flightGroup->fgGoals[goalIndex].payload.argument = 0;
		flightGroup->fgGoals[goalIndex].payload.condition = 1;
		flightGroup->fgGoals[goalIndex].payload.amount = 0;
		flightGroup->fgGoals[goalIndex].payload.points = 0;
		flightGroup->fgGoals[goalIndex].payload.parameter = 0;
		memset(flightGroup->fgGoals[goalIndex].payload.enabledForTeam, 0,
			   sizeof(flightGroup->fgGoals[goalIndex].payload.enabledForTeam));
	}

	flightGroup->editorWaypointShown = 0;
}

// FUNCTION: XWA 0x57BBC0
int Skirmish_WriteGeneratedMissionFile(XwaFile* stream) {
	enum {
		MISSION_FORMAT_VERSION = 18,
		MISSION_SECONDARY_VERSION = FRONTEND_BRIEFING_SECONDARY_VERSION_98,
		MAX_MESSAGE_COUNT = 64,
		MAX_GLOBAL_GOAL_TEAMS = 10,
		MAX_GLOBAL_GOALS_PER_TEAM = 7,
		MANDATORY_GLOBAL_GOALS = 3,
		TEAM_RECORD_COUNT = 10,
		BRIEFING_COPY_COUNT = 2,
		BRIEFING_TEAM_BYTE_COUNT = 10,
		BRIEFING_LABEL_OFFSET_COUNT = 128,
		BRIEFING_TEXT_OFFSET_COUNT = 128,
		ZERO_CHUNK_SIZE = 1024,
		ZERO_CHUNK_COUNT = 32,
		FG_STRING_INDEX_BYTES_PER_FG = 24,
		GLOBAL_GOAL_STRING_INDEX_SIZE = 840,
		ORDER_STRING_INDEX_SIZE = 3072,
		TEXT_PAGE_SIZE = 4096,
	};

	FrontendMission* mission;
	unsigned char zeroChunk[ZERO_CHUNK_SIZE];
	int messageCount;
	int messageIndex;
	int16_t flightGroupIndex;
	int teamIndex;
	int globalGoalCount;
	int globalGoalIndex;
	int copyIndex;
	int iconIndex;
	int zeroIndex;

	mission = g_frontendMission;
	mission->formatVersion = MISSION_FORMAT_VERSION;
	mission->header.secondaryVersion = MISSION_SECONDARY_VERSION;

	messageCount = 0;
	for (messageIndex = 0; messageIndex < MAX_MESSAGE_COUNT; ++messageIndex) {
		if (mission->messages[messageIndex].message[0] != '\0') {
			++messageCount;
		}
	}

	File_WriteWord(stream, MISSION_FORMAT_VERSION);
	File_WriteWord(stream, mission->flightGroupCount);
	File_WriteWord(stream, messageCount);
	File_WriteCount(stream, &mission->header, sizeof(mission->header));

	for (flightGroupIndex = 0; flightGroupIndex < (int16_t)mission->flightGroupCount; ++flightGroupIndex) {
		File_WriteCount(stream, &mission->flightGroups[flightGroupIndex], sizeof(mission->flightGroups[0]));
	}

	for (messageIndex = 0; messageIndex < MAX_MESSAGE_COUNT; ++messageIndex) {
		if (mission->messages[messageIndex].message[0] != '\0') {
			File_WriteWord(stream, messageIndex);
			File_WriteCount(stream, &mission->messages[messageIndex], sizeof(mission->messages[0]));
		}
	}

	for (teamIndex = 0; teamIndex < MAX_GLOBAL_GOAL_TEAMS; ++teamIndex) {
		globalGoalCount = 0;
		for (globalGoalIndex = 0; globalGoalIndex < MAX_GLOBAL_GOALS_PER_TEAM; ++globalGoalIndex) {
			if (mission->globalGoals[teamIndex][globalGoalIndex].name[0] != '\0' ||
				globalGoalIndex < MANDATORY_GLOBAL_GOALS) {
				++globalGoalCount;
			}
		}

		File_WriteWord(stream, globalGoalCount);
		for (globalGoalIndex = 0; globalGoalIndex < globalGoalCount; ++globalGoalIndex) {
			if (mission->globalGoals[teamIndex][globalGoalIndex].name[0] != '\0' ||
				globalGoalIndex < MANDATORY_GLOBAL_GOALS) {
				File_WriteCount(stream, &mission->globalGoals[teamIndex][globalGoalIndex],
								sizeof(mission->globalGoals[0][0]));
			}
		}
	}

	for (teamIndex = 0; teamIndex < TEAM_RECORD_COUNT; ++teamIndex) {
		File_WriteWord(stream, 1);
		File_WriteCount(stream, &mission->teams[teamIndex], sizeof(mission->teams[0]));
	}

	for (copyIndex = 0; copyIndex < BRIEFING_COPY_COUNT; ++copyIndex) {
		File_WriteCount(stream, &g_frontendBriefingContent.script, FRONTEND_BRIEFING_SCRIPT_FORMAT_SIZE);

		for (iconIndex = 0; iconIndex < FRONTEND_BRIEFING_MAP_ICON_COUNT; ++iconIndex) {
			File_WriteCount(stream, &g_briefingMapCurrentRegionIcons[iconIndex],
							sizeof(g_briefingMapCurrentRegionIcons[0]));
		}

		for (zeroIndex = 0; zeroIndex < BRIEFING_TEAM_BYTE_COUNT; ++zeroIndex) {
			File_WriteByte(stream, 0);
		}

		for (zeroIndex = 0; zeroIndex < BRIEFING_LABEL_OFFSET_COUNT; ++zeroIndex) {
			File_WriteWord(stream, 0);
		}

		for (zeroIndex = 0; zeroIndex < BRIEFING_TEXT_OFFSET_COUNT; ++zeroIndex) {
			File_WriteWord(stream, 0);
		}
	}

	memset(zeroChunk, 0, sizeof(zeroChunk));
	for (zeroIndex = 0; zeroIndex < ZERO_CHUNK_COUNT; ++zeroIndex) {
		File_WriteCount(stream, zeroChunk, sizeof(zeroChunk));
	}

	for (zeroIndex = 0; zeroIndex < FG_STRING_INDEX_BYTES_PER_FG * (int16_t)mission->flightGroupCount;
		 ++zeroIndex) {
		File_WriteByte(stream, 0);
	}

	for (zeroIndex = 0; zeroIndex < GLOBAL_GOAL_STRING_INDEX_SIZE; ++zeroIndex) {
		File_WriteByte(stream, 0);
	}

	for (zeroIndex = 0; zeroIndex < ORDER_STRING_INDEX_SIZE; ++zeroIndex) {
		File_WriteByte(stream, 0);
	}

	File_WriteCount(stream, mission->textTail.missionSuccessfulText, TEXT_PAGE_SIZE);
	File_WriteCount(stream, mission->textTail.missionFailedText, TEXT_PAGE_SIZE);
	File_WriteCount(stream, mission->textTail.missionDescriptionText, TEXT_PAGE_SIZE);
	return 1;
}

// FUNCTION: XWA 0x57BF00
int Skirmish_BuildFlightGroupDutyOrders(int combatSimSlotIdx, int fgIndex) {
	enum {
		SKIRMISH_DUTY_NONE = 0,
		SKIRMISH_DUTY_FIGHTER = 1,
		SKIRMISH_DUTY_HEAVY_CRAFT = 2,

		SHIP_CATEGORY_FIGHTER = 1,
		SHIP_CATEGORY_BOMBER = 2,
		SHIP_CATEGORY_TRANSPORT = 3,
		SHIP_CATEGORY_STATION = 5,
		SHIP_CATEGORY_STARSHIP = 6,
		SHIP_CATEGORY_PLATFORM = 7,
		SHIP_CATEGORY_MINE = 8,
		SHIP_CATEGORY_UTILITY = 9,
		SHIP_CATEGORY_DROID = 11,

		COMBAT_ROLE_SUPERIORITY = 1,
		COMBAT_ROLE_STRIKE = 2,
		COMBAT_ROLE_ESCORT = 3,
		COMBAT_ROLE_DISABLE = 4,
		COMBAT_ROLE_CAPTURE = 5,
		COMBAT_ROLE_RECON = 6,

		ORDER_ATTACK_TARGETS = 7,
		ORDER_ESCORT_TARGETS = 9,
		ORDER_DISABLE_TARGETS = 11,
		ORDER_BOARD_TARGET = 15,
		ORDER_ESCORT_REGION = 24,
		ORDER_PATROL_TARGETS = 26,
		ORDER_PATROL_REGION = 27,
		ORDER_GO_TO_WAYPOINTS = 50,
		ORDER_INSPECT_TARGETS = 53,

		TARGET_GLOBAL_GROUP = 8,
		TARGET_TEAM = 12,
		TARGET_IFF = 21,

		TRIGGER_PERCENT_OF = 50,
		TRIGGER_DESTROYED = 5,

		CRAFT_TYPE_SUPER_BACKDROP = 99,

		SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE = 3,
		SKIRMISH_REGION_ORDER_STRIDE = 4,
		SKIRMISH_ORDER_CLONE_COUNT = 4,
		SKIRMISH_PATROL_WAYPOINT_COUNT = 8,
	};

	int useTeamRegions;
	int opposingTeamBucket;
	int shipCategory;
	int dutyClass;

	if (g_gameConfig.numberOfTeams <= 2u && g_gameConfig.eachTeamOwnRegion && g_gameConfig.environment != 3) {
		useTeamRegions = 1;
		opposingTeamBucket = g_frontendMission->flightGroups[fgIndex].team ^ 1u;
	} else {
		useTeamRegions = 0;
		opposingTeamBucket = 0;
	}

	shipCategory =
		g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[combatSimSlotIdx].craftType]].category;
	dutyClass = SKIRMISH_DUTY_NONE;
	if (shipCategory == SHIP_CATEGORY_FIGHTER || shipCategory == SHIP_CATEGORY_BOMBER ||
		shipCategory == SHIP_CATEGORY_TRANSPORT || shipCategory == SHIP_CATEGORY_UTILITY ||
		shipCategory == SHIP_CATEGORY_DROID) {
		dutyClass = SKIRMISH_DUTY_FIGHTER;
	} else if (shipCategory == SHIP_CATEGORY_STARSHIP || shipCategory == SHIP_CATEGORY_PLATFORM ||
			   shipCategory == SHIP_CATEGORY_MINE || shipCategory == SHIP_CATEGORY_STATION) {
		dutyClass = SKIRMISH_DUTY_HEAVY_CRAFT;
	}

	switch (g_combatSimSlots[combatSimSlotIdx].craftRole) {
		case SKIRMISH_DUTY_NONE:
			break;

		case COMBAT_ROLE_SUPERIORITY:
			if (g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] == 2 ||
				g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] == 3 ||
				g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] == 4) {
				if (dutyClass == SKIRMISH_DUTY_FIGHTER) {
					g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_ATTACK_TARGETS;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
					g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex]
						.orders[0]
						.secondaryTargetTypes[XWA_ORDER_TARGET_3] = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[0].secondaryTargets[XWA_ORDER_TARGET_3] =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
					g_frontendMission->flightGroups[fgIndex].orders[0].target3OrTarget4 = 0;
					g_frontendMission->flightGroups[fgIndex]
						.orders[0]
						.secondaryTargetTypes[XWA_ORDER_TARGET_4] = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].secondaryTargets[XWA_ORDER_TARGET_4] =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex].orders[1].order = ORDER_DISABLE_TARGETS;
				} else if (dutyClass == SKIRMISH_DUTY_HEAVY_CRAFT) {
					if (shipCategory == SHIP_CATEGORY_PLATFORM || shipCategory == SHIP_CATEGORY_MINE) {
						g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_PATROL_TARGETS;
						g_frontendMission->flightGroups[fgIndex].orders[0].variable1 = 0xFF;
					} else {
						g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_PATROL_TARGETS;
					}
					g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
					g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex]
						.orders[0]
						.secondaryTargetTypes[XWA_ORDER_TARGET_3] = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[0].secondaryTargets[XWA_ORDER_TARGET_3] =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
					g_frontendMission->flightGroups[fgIndex].orders[0].target3OrTarget4 = 0;
					g_frontendMission->flightGroups[fgIndex]
						.orders[0]
						.secondaryTargetTypes[XWA_ORDER_TARGET_4] = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].secondaryTargets[XWA_ORDER_TARGET_4] =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex].orders[1].order = ORDER_PATROL_REGION;
				}
			} else if (dutyClass == SKIRMISH_DUTY_FIGHTER) {
				g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_ATTACK_TARGETS;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
					(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
				g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2 =
					g_frontendMission->flightGroups[fgIndex].team;
				g_frontendMission->flightGroups[fgIndex].orders[0].secondaryTargetTypes[XWA_ORDER_TARGET_3] =
					TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[0].secondaryTargets[XWA_ORDER_TARGET_3] =
					(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
				g_frontendMission->flightGroups[fgIndex].orders[0].target3OrTarget4 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[0].secondaryTargetTypes[XWA_ORDER_TARGET_4] =
					TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[0].secondaryTargets[XWA_ORDER_TARGET_4] =
					g_frontendMission->flightGroups[fgIndex].team;
				g_frontendMission->flightGroups[fgIndex].orders[1].order = ORDER_ATTACK_TARGETS;
			} else if (dutyClass == SKIRMISH_DUTY_HEAVY_CRAFT) {
				if (shipCategory == SHIP_CATEGORY_PLATFORM || shipCategory == SHIP_CATEGORY_MINE) {
					g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_PATROL_TARGETS;
					g_frontendMission->flightGroups[fgIndex].orders[0].variable1 = 0xFF;
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_PATROL_TARGETS;
				}
				g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
					g_frontendMission->flightGroups[fgIndex].team;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2 = 9;
			}

			if (dutyClass == SKIRMISH_DUTY_FIGHTER ||
				(g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] >= 2 &&
				 g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] <= 4 &&
				 dutyClass == SKIRMISH_DUTY_HEAVY_CRAFT)) {
				g_frontendMission->flightGroups[fgIndex].orders[1].target1Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[1].target1 =
					(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * (opposingTeamBucket + 1));
				g_frontendMission->flightGroups[fgIndex].orders[1].target1OrTarget2 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[1].target2Type = TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[1].target2 =
					g_frontendMission->flightGroups[fgIndex].team;
			}
			g_skirmishTeamHasSuperiorityRole[g_frontendMission->flightGroups[fgIndex].team] = 1;
			break;

		case COMBAT_ROLE_STRIKE:
			if (g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] != 2 &&
				g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] != 3 &&
				g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] != 4) {
				if (dutyClass == SKIRMISH_DUTY_FIGHTER) {
					g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_ATTACK_TARGETS;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * (opposingTeamBucket + 1));
					g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex].orders[1].order = ORDER_ATTACK_TARGETS;
					g_frontendMission->flightGroups[fgIndex].orders[1].target1Type = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[1].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
					g_frontendMission->flightGroups[fgIndex].orders[1].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex]
						.orders[1]
						.secondaryTargetTypes[XWA_ORDER_TARGET_3] = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_3] =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
					g_frontendMission->flightGroups[fgIndex].orders[1].target3OrTarget4 = 0;
					g_frontendMission->flightGroups[fgIndex]
						.orders[1]
						.secondaryTargetTypes[XWA_ORDER_TARGET_4] = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_4] =
						g_frontendMission->flightGroups[fgIndex].team;
				} else if (dutyClass == SKIRMISH_DUTY_HEAVY_CRAFT) {
					if (shipCategory == SHIP_CATEGORY_PLATFORM || shipCategory == SHIP_CATEGORY_MINE) {
						g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_PATROL_TARGETS;
						g_frontendMission->flightGroups[fgIndex].orders[0].variable1 = 0xFF;
					} else {
						g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_PATROL_TARGETS;
					}
					g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2 = 9;
				}
			} else if (dutyClass == SKIRMISH_DUTY_FIGHTER) {
				g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_DISABLE_TARGETS;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
					(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * (opposingTeamBucket + 1));
				g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2 =
					g_frontendMission->flightGroups[fgIndex].team;
				g_frontendMission->flightGroups[fgIndex].orders[1].order = ORDER_ATTACK_TARGETS;
				g_frontendMission->flightGroups[fgIndex].orders[1].target1Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[1].target1 =
					(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
				g_frontendMission->flightGroups[fgIndex].orders[1].target1OrTarget2 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[1].target2Type = TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[1].target2 =
					g_frontendMission->flightGroups[fgIndex].team;
				g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargetTypes[XWA_ORDER_TARGET_3] =
					TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_3] =
					(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
				g_frontendMission->flightGroups[fgIndex].orders[1].target3OrTarget4 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargetTypes[XWA_ORDER_TARGET_4] =
					TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_4] =
					g_frontendMission->flightGroups[fgIndex].team;
				if (g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] == 4) {
					g_frontendMission->flightGroups[fgIndex].skipTriggers[2].triggers[0].condition =
						TRIGGER_DESTROYED;
					g_frontendMission->flightGroups[fgIndex].skipTriggers[2].triggers[0].variableType =
						TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].skipTriggers[2].triggers[0].variable =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * (opposingTeamBucket + 1));
					g_frontendMission->flightGroups[fgIndex].orders[2].order = ORDER_ATTACK_TARGETS;
					g_frontendMission->flightGroups[fgIndex].orders[2].target1Type = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[2].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * (opposingTeamBucket + 1));
					g_frontendMission->flightGroups[fgIndex].orders[2].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[2].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[2].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
				}
			} else if (dutyClass == SKIRMISH_DUTY_HEAVY_CRAFT) {
				int strikeShipCategory;

				g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_PATROL_REGION;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
					(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * (opposingTeamBucket + 1));
				g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2 =
					g_frontendMission->flightGroups[fgIndex].team;
				strikeShipCategory =
					g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[combatSimSlotIdx].craftType]]
						.category;
				if (strikeShipCategory == SHIP_CATEGORY_PLATFORM ||
					strikeShipCategory == SHIP_CATEGORY_MINE) {
					g_frontendMission->flightGroups[fgIndex].orders[1].order = ORDER_PATROL_TARGETS;
					g_frontendMission->flightGroups[fgIndex].orders[1].variable1 = 0xFF;
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[1].order = ORDER_PATROL_TARGETS;
				}
				g_frontendMission->flightGroups[fgIndex].orders[1].target1Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[1].target1 =
					(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
				g_frontendMission->flightGroups[fgIndex].orders[1].target1OrTarget2 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[1].target2Type = TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[1].target2 =
					g_frontendMission->flightGroups[fgIndex].team;
				g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargetTypes[XWA_ORDER_TARGET_3] =
					TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_3] =
					(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
				g_frontendMission->flightGroups[fgIndex].orders[1].target3OrTarget4 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargetTypes[XWA_ORDER_TARGET_4] =
					TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_4] =
					g_frontendMission->flightGroups[fgIndex].team;
				if (g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] == 4) {
					int strikeOrder2Category;

					g_frontendMission->flightGroups[fgIndex].skipTriggers[2].triggers[0].condition =
						TRIGGER_DESTROYED;
					g_frontendMission->flightGroups[fgIndex].skipTriggers[2].triggers[0].variableType =
						TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].skipTriggers[2].triggers[0].variable =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * (opposingTeamBucket + 1));
					strikeOrder2Category =
						g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[combatSimSlotIdx].craftType]]
							.category;
					if (strikeOrder2Category == SHIP_CATEGORY_PLATFORM ||
						strikeOrder2Category == SHIP_CATEGORY_MINE) {
						g_frontendMission->flightGroups[fgIndex].orders[2].order = ORDER_PATROL_TARGETS;
						g_frontendMission->flightGroups[fgIndex].orders[2].variable1 = 0xFF;
					} else {
						g_frontendMission->flightGroups[fgIndex].orders[2].order = ORDER_PATROL_TARGETS;
					}
					g_frontendMission->flightGroups[fgIndex].orders[2].target1Type = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[2].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * (opposingTeamBucket + 1));
					g_frontendMission->flightGroups[fgIndex].orders[2].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[2].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[2].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
				}
			}
			g_skirmishTeamHasStrikeRole[g_frontendMission->flightGroups[fgIndex].team] = 1;
			break;

		case COMBAT_ROLE_ESCORT:
			if (dutyClass == SKIRMISH_DUTY_FIGHTER) {
				g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_ESCORT_TARGETS;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_GLOBAL_GROUP;
				if (useTeamRegions) {
					g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
						(uint8_t)(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * ((opposingTeamBucket ^ 1u) + 1));
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[0].target1 = 3;
				}
				g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_TEAM;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2 =
					g_frontendMission->flightGroups[fgIndex].team;
				g_frontendMission->flightGroups[fgIndex].orders[1].order = ORDER_ATTACK_TARGETS;
				g_frontendMission->flightGroups[fgIndex].orders[1].target1Type = TARGET_GLOBAL_GROUP;
				if (useTeamRegions) {
					g_frontendMission->flightGroups[fgIndex].orders[1].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[1].target1 = 1;
				}
				g_frontendMission->flightGroups[fgIndex].orders[1].target1OrTarget2 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[1].target2Type = TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[1].target2 =
					g_frontendMission->flightGroups[fgIndex].team;
				g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargetTypes[XWA_ORDER_TARGET_3] =
					TARGET_GLOBAL_GROUP;
				if (useTeamRegions) {
					g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_3] =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_3] =
						2;
				}
				g_frontendMission->flightGroups[fgIndex].orders[1].target3OrTarget4 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargetTypes[XWA_ORDER_TARGET_4] =
					TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_4] =
					g_frontendMission->flightGroups[fgIndex].team;
			} else if (dutyClass == SKIRMISH_DUTY_HEAVY_CRAFT) {
				int escortShipCategory;

				g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_ESCORT_REGION;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_GLOBAL_GROUP;
				if (useTeamRegions) {
					g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
						(uint8_t)(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * ((opposingTeamBucket ^ 1u) + 1));
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[0].target1 = 3;
				}
				g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_TEAM;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2 =
					g_frontendMission->flightGroups[fgIndex].team;
				escortShipCategory =
					g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[combatSimSlotIdx].craftType]]
						.category;
				if (escortShipCategory == SHIP_CATEGORY_PLATFORM ||
					escortShipCategory == SHIP_CATEGORY_MINE) {
					g_frontendMission->flightGroups[fgIndex].orders[1].order = ORDER_PATROL_TARGETS;
					g_frontendMission->flightGroups[fgIndex].orders[1].variable1 = 0xFF;
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[1].order = ORDER_PATROL_TARGETS;
				}
				if (g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] != 2 &&
					g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] != 3 &&
					g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] != 4) {
					g_frontendMission->flightGroups[fgIndex].orders[1].target1Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[1].target1 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex].orders[1].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2 = 9;
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[1].target1Type = TARGET_GLOBAL_GROUP;
					if (useTeamRegions) {
						g_frontendMission->flightGroups[fgIndex].orders[1].target1 =
							(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
					} else {
						g_frontendMission->flightGroups[fgIndex].orders[1].target1 = 1;
					}
					g_frontendMission->flightGroups[fgIndex].orders[1].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex]
						.orders[1]
						.secondaryTargetTypes[XWA_ORDER_TARGET_3] = TARGET_GLOBAL_GROUP;
					if (useTeamRegions) {
						g_frontendMission->flightGroups[fgIndex]
							.orders[1]
							.secondaryTargets[XWA_ORDER_TARGET_3] =
							(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
					} else {
						g_frontendMission->flightGroups[fgIndex]
							.orders[1]
							.secondaryTargets[XWA_ORDER_TARGET_3] = 2;
					}
					g_frontendMission->flightGroups[fgIndex].orders[1].target3OrTarget4 = 0;
					g_frontendMission->flightGroups[fgIndex]
						.orders[1]
						.secondaryTargetTypes[XWA_ORDER_TARGET_4] = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_4] =
						g_frontendMission->flightGroups[fgIndex].team;
				}
			}
			g_skirmishTeamHasEscortRole[g_frontendMission->flightGroups[fgIndex].team] = 1;
			break;

		case COMBAT_ROLE_DISABLE:
			if (dutyClass == SKIRMISH_DUTY_FIGHTER) {
				g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_DISABLE_TARGETS;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
					(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * (opposingTeamBucket + 1));
				g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2 =
					g_frontendMission->flightGroups[fgIndex].team;
				g_frontendMission->flightGroups[fgIndex].orders[1].order = ORDER_ATTACK_TARGETS;
				g_frontendMission->flightGroups[fgIndex].orders[1].target1Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[1].target1 =
					(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
				g_frontendMission->flightGroups[fgIndex].orders[1].target1OrTarget2 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[1].target2Type = TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[1].target2 =
					g_frontendMission->flightGroups[fgIndex].team;
				g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargetTypes[XWA_ORDER_TARGET_3] =
					TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_3] =
					(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
				g_frontendMission->flightGroups[fgIndex].orders[1].target3OrTarget4 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargetTypes[XWA_ORDER_TARGET_4] =
					TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_4] =
					g_frontendMission->flightGroups[fgIndex].team;
				g_frontendMission->flightGroups[fgIndex].formationSpacing = 9;
			} else if (dutyClass == SKIRMISH_DUTY_HEAVY_CRAFT) {
				int disableShipCategory;

				g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_PATROL_REGION;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
					(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * (opposingTeamBucket + 1));
				g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2 =
					g_frontendMission->flightGroups[fgIndex].team;
				disableShipCategory =
					g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[combatSimSlotIdx].craftType]]
						.category;
				if (disableShipCategory == SHIP_CATEGORY_PLATFORM ||
					disableShipCategory == SHIP_CATEGORY_MINE) {
					g_frontendMission->flightGroups[fgIndex].orders[1].order = ORDER_PATROL_TARGETS;
					g_frontendMission->flightGroups[fgIndex].orders[1].variable1 = 0xFF;
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[1].order = ORDER_PATROL_TARGETS;
				}
				if (g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] == 2 ||
					g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] == 3 ||
					g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] == 4) {
					g_frontendMission->flightGroups[fgIndex].orders[1].target1Type = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[1].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
					g_frontendMission->flightGroups[fgIndex].orders[1].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex]
						.orders[1]
						.secondaryTargetTypes[XWA_ORDER_TARGET_3] = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_3] =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
					g_frontendMission->flightGroups[fgIndex].orders[1].target3OrTarget4 = 0;
					g_frontendMission->flightGroups[fgIndex]
						.orders[1]
						.secondaryTargetTypes[XWA_ORDER_TARGET_4] = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_4] =
						g_frontendMission->flightGroups[fgIndex].team;
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[1].target1Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[1].target1 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex].orders[1].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2 = 9;
				}
				g_frontendMission->flightGroups[fgIndex].formationSpacing = 9;
			}
			g_skirmishTeamHasDisableRole[g_frontendMission->flightGroups[fgIndex].team] = 1;
			break;

		case COMBAT_ROLE_CAPTURE:
			if (dutyClass == SKIRMISH_DUTY_FIGHTER) {
				if (shipCategory == SHIP_CATEGORY_FIGHTER || shipCategory == SHIP_CATEGORY_BOMBER) {
					g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_BOARD_TARGET;
					g_frontendMission->flightGroups[fgIndex].orders[0].variable1 = 10;
					g_frontendMission->flightGroups[fgIndex].orders[0].variable2 = 51;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * (opposingTeamBucket + 1));
					g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex].orders[1].order = ORDER_DISABLE_TARGETS;
					g_frontendMission->flightGroups[fgIndex].orders[1].target1Type = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[1].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * (opposingTeamBucket + 1));
					g_frontendMission->flightGroups[fgIndex].orders[1].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex].orders[2].order = ORDER_ATTACK_TARGETS;
					g_frontendMission->flightGroups[fgIndex].orders[2].target1Type = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[2].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
					g_frontendMission->flightGroups[fgIndex].orders[2].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[2].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[2].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex]
						.orders[2]
						.secondaryTargetTypes[XWA_ORDER_TARGET_3] = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[2].secondaryTargets[XWA_ORDER_TARGET_3] =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
					g_frontendMission->flightGroups[fgIndex].orders[2].target3OrTarget4 = 0;
					g_frontendMission->flightGroups[fgIndex]
						.orders[2]
						.secondaryTargetTypes[XWA_ORDER_TARGET_4] = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[2].secondaryTargets[XWA_ORDER_TARGET_4] =
						g_frontendMission->flightGroups[fgIndex].team;
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_ATTACK_TARGETS;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
					g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex]
						.orders[0]
						.secondaryTargetTypes[XWA_ORDER_TARGET_3] = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[0].secondaryTargets[XWA_ORDER_TARGET_3] =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
					g_frontendMission->flightGroups[fgIndex].orders[0].target3OrTarget4 = 0;
					g_frontendMission->flightGroups[fgIndex]
						.orders[0]
						.secondaryTargetTypes[XWA_ORDER_TARGET_4] = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].secondaryTargets[XWA_ORDER_TARGET_4] =
						g_frontendMission->flightGroups[fgIndex].team;
				}
			} else if (dutyClass == SKIRMISH_DUTY_HEAVY_CRAFT) {
				if (shipCategory == SHIP_CATEGORY_PLATFORM || shipCategory == SHIP_CATEGORY_MINE) {
					g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_PATROL_TARGETS;
					g_frontendMission->flightGroups[fgIndex].orders[0].variable1 = 0xFF;
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_PATROL_TARGETS;
				}
				if (g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] != 2 &&
					g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] != 3 &&
					g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] != 4) {
					g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2 = 9;
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
					g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex]
						.orders[0]
						.secondaryTargetTypes[XWA_ORDER_TARGET_3] = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[0].secondaryTargets[XWA_ORDER_TARGET_3] =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
					g_frontendMission->flightGroups[fgIndex].orders[0].target3OrTarget4 = 0;
					g_frontendMission->flightGroups[fgIndex]
						.orders[0]
						.secondaryTargetTypes[XWA_ORDER_TARGET_4] = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].secondaryTargets[XWA_ORDER_TARGET_4] =
						g_frontendMission->flightGroups[fgIndex].team;
				}
			}
			g_skirmishTeamHasCaptureRole[g_frontendMission->flightGroups[fgIndex].team] = 1;
			break;

		case COMBAT_ROLE_RECON:
			if (dutyClass == SKIRMISH_DUTY_FIGHTER) {
				g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_INSPECT_TARGETS;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
					(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * (opposingTeamBucket + 1));
				g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
				g_frontendMission->flightGroups[fgIndex].orders[0].target2 =
					g_frontendMission->flightGroups[fgIndex].team;
				g_frontendMission->flightGroups[fgIndex].orders[1].order = ORDER_ATTACK_TARGETS;
				g_frontendMission->flightGroups[fgIndex].orders[1].target1Type = TARGET_GLOBAL_GROUP;
				if (g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] == 4) {
					g_frontendMission->flightGroups[fgIndex].orders[1].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * (opposingTeamBucket + 1));
					g_frontendMission->flightGroups[fgIndex].orders[1].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex].orders[2].order = ORDER_ATTACK_TARGETS;
					g_frontendMission->flightGroups[fgIndex].orders[2].target1Type = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[2].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
					g_frontendMission->flightGroups[fgIndex].orders[2].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[2].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[2].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex]
						.orders[2]
						.secondaryTargetTypes[XWA_ORDER_TARGET_3] = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[2].secondaryTargets[XWA_ORDER_TARGET_3] =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
					g_frontendMission->flightGroups[fgIndex].orders[2].target3OrTarget4 = 0;
					g_frontendMission->flightGroups[fgIndex]
						.orders[2]
						.secondaryTargetTypes[XWA_ORDER_TARGET_4] = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[2].secondaryTargets[XWA_ORDER_TARGET_4] =
						g_frontendMission->flightGroups[fgIndex].team;
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[1].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
					g_frontendMission->flightGroups[fgIndex].orders[1].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[1].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex]
						.orders[1]
						.secondaryTargetTypes[XWA_ORDER_TARGET_3] = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_3] =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
					g_frontendMission->flightGroups[fgIndex].orders[1].target3OrTarget4 = 0;
					g_frontendMission->flightGroups[fgIndex]
						.orders[1]
						.secondaryTargetTypes[XWA_ORDER_TARGET_4] = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[1].secondaryTargets[XWA_ORDER_TARGET_4] =
						g_frontendMission->flightGroups[fgIndex].team;
				}
			} else if (dutyClass == SKIRMISH_DUTY_HEAVY_CRAFT) {
				if (shipCategory == SHIP_CATEGORY_PLATFORM || shipCategory == SHIP_CATEGORY_MINE) {
					g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_PATROL_TARGETS;
					g_frontendMission->flightGroups[fgIndex].orders[0].variable1 = 0xFF;
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[0].order = ORDER_PATROL_TARGETS;
				}
				if (g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] == 2 ||
					g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] == 3 ||
					g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] == 4) {
					g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
					g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex]
						.orders[0]
						.secondaryTargetTypes[XWA_ORDER_TARGET_3] = TARGET_GLOBAL_GROUP;
					g_frontendMission->flightGroups[fgIndex].orders[0].secondaryTargets[XWA_ORDER_TARGET_3] =
						(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 2);
					g_frontendMission->flightGroups[fgIndex].orders[0].target3OrTarget4 = 0;
					g_frontendMission->flightGroups[fgIndex]
						.orders[0]
						.secondaryTargetTypes[XWA_ORDER_TARGET_4] = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].secondaryTargets[XWA_ORDER_TARGET_4] =
						g_frontendMission->flightGroups[fgIndex].team;
				} else {
					g_frontendMission->flightGroups[fgIndex].orders[0].target1Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1 =
						g_frontendMission->flightGroups[fgIndex].team;
					g_frontendMission->flightGroups[fgIndex].orders[0].target1OrTarget2 = 0;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2Type = TARGET_IFF;
					g_frontendMission->flightGroups[fgIndex].orders[0].target2 = 9;
				}
			}
			g_skirmishTeamHasReconRole[g_frontendMission->flightGroups[fgIndex].team] = 1;
			break;

		default:
			break;
	}

	if (g_combatSimSlots[combatSimSlotIdx].craftType == CRAFT_TYPE_SUPER_BACKDROP) {
		int orderIndex;

		for (orderIndex = 0; orderIndex < SKIRMISH_ORDER_CLONE_COUNT; ++orderIndex) {
			g_frontendMission->flightGroups[fgIndex].orders[orderIndex].speed = 22;
		}
	}

	{
		int regionShipCategory;

		regionShipCategory =
			g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[combatSimSlotIdx].craftType]].category;
		if (useTeamRegions && g_combatSimSlots[combatSimSlotIdx].craftRole != 0 &&
			(regionShipCategory == SHIP_CATEGORY_FIGHTER || regionShipCategory == SHIP_CATEGORY_BOMBER ||
			 regionShipCategory == SHIP_CATEGORY_TRANSPORT || regionShipCategory == SHIP_CATEGORY_STATION)) {
			int orderIndex;

			g_frontendMission->flightGroups[fgIndex].orders[3].order = ORDER_GO_TO_WAYPOINTS;
			g_frontendMission->flightGroups[fgIndex].orders[3].variable1 =
				g_frontendMission->flightGroups[fgIndex].team ? 2 : 1;
			g_frontendMission->flightGroups[fgIndex].orders[3].variable2 = 1;
			g_frontendMission->flightGroups[fgIndex].orders[3].variable3 = 51;
			g_frontendMission->flightGroups[fgIndex].orders[3].waypoints[0].x = 0;
			g_frontendMission->flightGroups[fgIndex].orders[3].waypoints[0].y = 0;
			g_frontendMission->flightGroups[fgIndex].orders[3].waypoints[0].z = 0;
			g_frontendMission->flightGroups[fgIndex].orders[3].waypoints[0].enabled = 1;

			switch (g_combatSimSlots[combatSimSlotIdx].craftRole) {
				case COMBAT_ROLE_SUPERIORITY:
					if (dutyClass == SKIRMISH_DUTY_FIGHTER) {
						if (g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] >= 2u) {
							g_frontendMission->flightGroups[fgIndex].skipTriggers[3].triggers[0].condition =
								TRIGGER_PERCENT_OF;
							g_frontendMission->flightGroups[fgIndex]
								.skipTriggers[3]
								.triggers[0]
								.variableType = TARGET_GLOBAL_GROUP;
							g_frontendMission->flightGroups[fgIndex].skipTriggers[3].triggers[0].variable =
								(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * opposingTeamBucket + 1);
							g_frontendMission->flightGroups[fgIndex].skipTriggers[3].triggers[0].parameter =
								1;
						}
					} else if (dutyClass == SKIRMISH_DUTY_HEAVY_CRAFT) {
						g_frontendMission->flightGroups[fgIndex].skipTriggers[3].triggers[0].condition =
							TRIGGER_PERCENT_OF;
						g_frontendMission->flightGroups[fgIndex].skipTriggers[3].triggers[0].variableType =
							TARGET_TEAM;
						g_frontendMission->flightGroups[fgIndex].skipTriggers[3].triggers[0].variable =
							opposingTeamBucket;
						g_frontendMission->flightGroups[fgIndex].skipTriggers[3].triggers[0].parameter = 1;
					}
					break;

				case COMBAT_ROLE_STRIKE:
					if ((dutyClass == SKIRMISH_DUTY_FIGHTER &&
						 g_gameConfig.teamGoals[g_frontendMission->flightGroups[fgIndex].team] >= 2u) ||
						dutyClass == SKIRMISH_DUTY_HEAVY_CRAFT) {
						g_frontendMission->flightGroups[fgIndex].skipTriggers[3].triggers[0].condition =
							TRIGGER_PERCENT_OF;
						g_frontendMission->flightGroups[fgIndex].skipTriggers[3].triggers[0].variableType =
							TARGET_GLOBAL_GROUP;
						g_frontendMission->flightGroups[fgIndex].skipTriggers[3].triggers[0].variable =
							(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * (opposingTeamBucket + 1));
						g_frontendMission->flightGroups[fgIndex].skipTriggers[3].triggers[0].parameter = 1;
					}
					break;

				case COMBAT_ROLE_ESCORT:
					if (dutyClass == SKIRMISH_DUTY_FIGHTER || dutyClass == SKIRMISH_DUTY_HEAVY_CRAFT) {
						g_frontendMission->flightGroups[fgIndex].skipTriggers[3].triggers[0].condition =
							TRIGGER_PERCENT_OF;
						g_frontendMission->flightGroups[fgIndex].skipTriggers[3].triggers[0].variableType =
							TARGET_GLOBAL_GROUP;
						g_frontendMission->flightGroups[fgIndex].skipTriggers[3].triggers[0].variable =
							(uint8_t)(SKIRMISH_TEAM_GLOBAL_GROUP_STRIDE * ((opposingTeamBucket ^ 1u) + 1));
						g_frontendMission->flightGroups[fgIndex].skipTriggers[3].triggers[0].parameter = 1;
					}
					break;

				case COMBAT_ROLE_DISABLE:
				case COMBAT_ROLE_CAPTURE:
				default:
					break;
			}

			for (orderIndex = 0; orderIndex < SKIRMISH_ORDER_CLONE_COUNT; ++orderIndex) {
				memcpy(&g_frontendMission->flightGroups[fgIndex].orders[orderIndex + 4],
					   &g_frontendMission->flightGroups[fgIndex].orders[orderIndex],
					   sizeof(g_frontendMission->flightGroups[fgIndex].orders[0]));
				memcpy(&g_frontendMission->flightGroups[fgIndex].skipTriggers[orderIndex + 4],
					   &g_frontendMission->flightGroups[fgIndex].skipTriggers[orderIndex],
					   sizeof(g_frontendMission->flightGroups[fgIndex].skipTriggers[0]));
				memcpy(&g_frontendMission->flightGroups[fgIndex].orders[orderIndex + 8],
					   &g_frontendMission->flightGroups[fgIndex].orders[orderIndex],
					   sizeof(g_frontendMission->flightGroups[fgIndex].orders[0]));
				memcpy(&g_frontendMission->flightGroups[fgIndex].skipTriggers[orderIndex + 8],
					   &g_frontendMission->flightGroups[fgIndex].skipTriggers[orderIndex],
					   sizeof(g_frontendMission->flightGroups[fgIndex].skipTriggers[0]));
			}

			g_frontendMission->flightGroups[fgIndex].skipTriggers[7].triggers[0].parameter = 2;
			g_frontendMission->flightGroups[fgIndex].skipTriggers[7].triggers[1].parameter = 2;
			if (g_frontendMission->flightGroups[fgIndex].orders[3].variable1 != 0) {
				g_frontendMission->flightGroups[fgIndex].orders[7].variable1 =
					g_frontendMission->flightGroups[fgIndex].team ? 0 : 2;
			}
			g_frontendMission->flightGroups[fgIndex].orders[7].waypoints[0].x = 0;
			g_frontendMission->flightGroups[fgIndex].orders[7].waypoints[0].y =
				(int16_t)(int)(sin(g_skirmishTeamSpawnAnglesRad[g_frontendMission->flightGroups[fgIndex]
																	.team]) *
							   g_skirmishWaypointRadiusNegative);
			g_frontendMission->flightGroups[fgIndex].orders[7].waypoints[0].z = 0;
			g_frontendMission->flightGroups[fgIndex].orders[7].waypoints[0].enabled = 1;

			g_frontendMission->flightGroups[fgIndex].skipTriggers[11].triggers[0].parameter = 3;
			g_frontendMission->flightGroups[fgIndex].skipTriggers[11].triggers[1].parameter = 3;
			if (g_frontendMission->flightGroups[fgIndex].orders[3].variable1 != 0) {
				g_frontendMission->flightGroups[fgIndex].orders[11].variable1 =
					g_frontendMission->flightGroups[fgIndex].team ? 1 : 0;
			}
			g_frontendMission->flightGroups[fgIndex].orders[11].waypoints[0].x = 0;
			g_frontendMission->flightGroups[fgIndex].orders[11].waypoints[0].y = 0;
			g_frontendMission->flightGroups[fgIndex].orders[11].waypoints[0].z = 0;
			g_frontendMission->flightGroups[fgIndex].orders[11].waypoints[0].enabled = 1;
		}
	}

	if (dutyClass != SKIRMISH_DUTY_FIGHTER) {
		int patrolShipCategory;

		patrolShipCategory =
			g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[combatSimSlotIdx].craftType]].category;
		if (patrolShipCategory == SHIP_CATEGORY_MINE || patrolShipCategory == SHIP_CATEGORY_PLATFORM) {
			int orderIndex;
			int regionIndex;

			for (regionIndex = 0; regionIndex < SKIRMISH_ORDER_CLONE_COUNT; ++regionIndex) {
				for (orderIndex = 0; orderIndex < SKIRMISH_ORDER_CLONE_COUNT; ++orderIndex) {
					int clonedOrderIndex;

					clonedOrderIndex = regionIndex * SKIRMISH_ORDER_CLONE_COUNT + orderIndex;
					if (g_frontendMission->flightGroups[fgIndex].orders[clonedOrderIndex].order != 0) {
						g_frontendMission->flightGroups[fgIndex].orders[clonedOrderIndex].throttle = 0;
					}
				}
			}
			return 1;
		}

		{
			int waypointIndex;
			int orderIndex;
			int startX;
			int startY;
			int startZ;
			double angle;
			double radius;
			double angleStep;

			startX = g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x;
			startY = g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y;
			startZ = g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z;
			angle = g_skirmishTeamSpawnAnglesRad[g_frontendMission->flightGroups[fgIndex].team];
			radius = sqrt((double)(startY * startY + startX * startX));

			if (g_gameConfig.numberOfTeams == 2) {
				angleStep = (g_frontendMission->flightGroups[fgIndex].team == 0) ? -0.7853981633974483
																				 : 0.7853981633974483;
			} else {
				angleStep = (rand() % 2 == 0) ? -0.7853981633974483 : 0.7853981633974483;
			}

			for (waypointIndex = 0; waypointIndex < SKIRMISH_PATROL_WAYPOINT_COUNT; ++waypointIndex) {
				double waypointBaseX;
				double waypointBaseY;

				waypointBaseX = cos(angle) * radius;
				waypointBaseY = sin(angle) * radius;

				for (orderIndex = 0; orderIndex < SKIRMISH_ORDER_CLONE_COUNT; ++orderIndex) {
					g_frontendMission->flightGroups[fgIndex]
						.orders[SKIRMISH_REGION_ORDER_STRIDE *
									g_frontendMission->flightGroups[fgIndex]
										.missionPointRegions[XWA_FG_POINT_START_1] +
								orderIndex]
						.waypoints[waypointIndex]
						.x = (int16_t)(int)((double)(rand() % 80) + waypointBaseX);
					g_frontendMission->flightGroups[fgIndex]
						.orders[SKIRMISH_REGION_ORDER_STRIDE *
									g_frontendMission->flightGroups[fgIndex]
										.missionPointRegions[XWA_FG_POINT_START_1] +
								orderIndex]
						.waypoints[waypointIndex]
						.y = (int16_t)(int)((double)(rand() % 80) + waypointBaseY);
					g_frontendMission->flightGroups[fgIndex]
						.orders[SKIRMISH_REGION_ORDER_STRIDE *
									g_frontendMission->flightGroups[fgIndex]
										.missionPointRegions[XWA_FG_POINT_START_1] +
								orderIndex]
						.waypoints[waypointIndex]
						.z = (int16_t)startZ;
					g_frontendMission->flightGroups[fgIndex]
						.orders[SKIRMISH_REGION_ORDER_STRIDE *
									g_frontendMission->flightGroups[fgIndex]
										.missionPointRegions[XWA_FG_POINT_START_1] +
								orderIndex]
						.waypoints[waypointIndex]
						.enabled = 1;
				}

				angle += angleStep;
			}
		}
		return 1;
	}

	{
		if (useTeamRegions) {
			int team;

			team = g_frontendMission->flightGroups[fgIndex].team;
			g_frontendMission->flightGroups[fgIndex]
				.orders[SKIRMISH_REGION_ORDER_STRIDE * team]
				.waypoints[0]
				.x =
				(int16_t)(g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x -
						  (int)(cos(g_skirmishTeamSpawnAnglesRad[team] - g_skirmishPiNegative) *
								g_skirmishWaypointRadiusNegative));
			g_frontendMission->flightGroups[fgIndex]
				.orders[SKIRMISH_REGION_ORDER_STRIDE * g_frontendMission->flightGroups[fgIndex].team]
				.waypoints[0]
				.y =
				(int16_t)(g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y -
						  (int)(sin(g_skirmishTeamSpawnAnglesRad[g_frontendMission->flightGroups[fgIndex]
																	 .team] -
									g_skirmishPiNegative) *
								g_skirmishWaypointRadiusNegative));
			g_frontendMission->flightGroups[fgIndex]
				.orders[SKIRMISH_REGION_ORDER_STRIDE * g_frontendMission->flightGroups[fgIndex].team]
				.waypoints[0]
				.z = 0;
		} else {
			g_frontendMission->flightGroups[fgIndex].orders[0].waypoints[0].x =
				(int16_t)(g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x -
						  (int)(cos(g_skirmishTeamSpawnAnglesRad[g_frontendMission->flightGroups[fgIndex]
																	 .team] -
									g_skirmishPiNegative) *
								g_skirmishWaypointRadiusNegative));
			g_frontendMission->flightGroups[fgIndex].orders[0].waypoints[0].y =
				(int16_t)(g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y -
						  (int)(sin(g_skirmishTeamSpawnAnglesRad[g_frontendMission->flightGroups[fgIndex]
																	 .team] -
									g_skirmishPiNegative) *
								g_skirmishWaypointRadiusNegative));
			g_frontendMission->flightGroups[fgIndex].orders[0].waypoints[0].z = 0;
		}

		g_frontendMission->flightGroups[fgIndex].orders[0].waypoints[0].enabled = 1;

		if (useTeamRegions) {
			int regionIndex;

			for (regionIndex = 0; regionIndex < 3; ++regionIndex) {
				if (regionIndex != g_frontendMission->flightGroups[fgIndex].team) {
					g_frontendMission->flightGroups[fgIndex]
						.orders[SKIRMISH_REGION_ORDER_STRIDE * regionIndex]
						.waypoints[0]
						.x = (int16_t)(rand() % 128 - 64);
					g_frontendMission->flightGroups[fgIndex]
						.orders[SKIRMISH_REGION_ORDER_STRIDE * regionIndex]
						.waypoints[0]
						.y = (int16_t)(rand() % 128 - 64);
					g_frontendMission->flightGroups[fgIndex]
						.orders[SKIRMISH_REGION_ORDER_STRIDE * regionIndex]
						.waypoints[0]
						.z = (int16_t)(rand() % 128 - 64);
					g_frontendMission->flightGroups[fgIndex]
						.orders[SKIRMISH_REGION_ORDER_STRIDE * regionIndex]
						.waypoints[0]
						.enabled = 1;
				}
			}
		}
	}
	return 1;
}

// FUNCTION: XWA 0x57E0B0
int Skirmish_PlaceFlightGroupNearPrevious(int fgIndex) {
	enum {
		SHIP_CATEGORY_FIGHTER = 1,
		SHIP_CATEGORY_BOMBER = 2,
		SHIP_CATEGORY_MINE = 8,
		SHIP_CATEGORY_UTILITY = 9,
		RETRY_COUNT = 4,
		MIN_SEPARATION_DISTANCE = 79,
		RANDOM_ANGLE_SPAN = 2356,
		RANDOM_Z_SPAN = 256,
		RANDOM_Z_CENTER = 128,
		HEAVY_CRAFT_TEAM_Z_STEP = 79,
	};

#ifdef __clang__
	int currentSizeRating = 0;
	int previousSizeRating = 0;
#else
	int currentSizeRating;
	int previousSizeRating;
#endif
	int currentCategory;
	int useRandomZ;
	int baseDistance;
	int previousFgCount;
	int retriesRemaining;
	CraftTechStats* cachedStats;
	FrontendMission* mission;

	cachedStats = g_cachedCraftTechStats;
	mission = g_frontendMission;

	{
		int statsIndex;

		for (statsIndex = 0; statsIndex < g_cachedCraftTechStatsCount; ++statsIndex) {
			if (cachedStats[statsIndex].craftType == mission->flightGroups[fgIndex].craftType) {
				currentSizeRating = cachedStats[statsIndex].sizeRating;
				break;
			}
		}
	}

	currentCategory = g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[fgIndex].craftType]].category;
	useRandomZ = currentCategory == SHIP_CATEGORY_FIGHTER || currentCategory == SHIP_CATEGORY_BOMBER ||
				 currentCategory == SHIP_CATEGORY_MINE || currentCategory == SHIP_CATEGORY_UTILITY;

	{
		int statsIndex;

		for (statsIndex = 0; statsIndex < g_cachedCraftTechStatsCount; ++statsIndex) {
			if (cachedStats[statsIndex].craftType == mission->flightGroups[fgIndex - 1].craftType) {
				previousSizeRating = cachedStats[statsIndex].sizeRating;
				break;
			}
		}
	}

	retriesRemaining = RETRY_COUNT;
	baseDistance = (int)((double)(currentSizeRating + previousSizeRating) * g_skirmishCraftSeparationScale);
	previousFgCount = fgIndex - 1;

	do {
		int separationDistance;
		double angle;

		separationDistance = baseDistance;
		if ((double)separationDistance < g_skirmishMinimumPlacementDistance) {
			separationDistance = MIN_SEPARATION_DISTANCE;
		}

		angle = ((double)(rand() % RANDOM_ANGLE_SPAN) - g_skirmishRandomAngleCenter) *
					g_skirmishRandomAngleScale +
				g_skirmishTeamSpawnAnglesRad[g_frontendMission->flightGroups[fgIndex].team];
		g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x =
			(int16_t)(g_frontendMission->flightGroups[fgIndex - 1].missionPoints[XWA_FG_POINT_START_1].x +
					  (int)(cos(angle) * (double)separationDistance));
		g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y =
			(int16_t)(g_frontendMission->flightGroups[fgIndex - 1].missionPoints[XWA_FG_POINT_START_1].y +
					  (int)(sin(angle) * (double)separationDistance));

		if (useRandomZ) {
			int randomZ;

			randomZ = (rand() % RANDOM_Z_SPAN) - RANDOM_Z_CENTER;
			g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z = (int16_t)randomZ;
		} else {
			g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z =
				(int16_t)(HEAVY_CRAFT_TEAM_Z_STEP * g_frontendMission->flightGroups[fgIndex].team);
		}

		{
			int tooClose;
			int priorFgIndex;

			tooClose = 0;
			for (priorFgIndex = 0; priorFgIndex < previousFgCount; ++priorFgIndex) {
				int zDelta;
				int statsIndex;

				for (statsIndex = 0; statsIndex < g_cachedCraftTechStatsCount; ++statsIndex) {
					if (g_cachedCraftTechStats[statsIndex].craftType ==
						g_frontendMission->flightGroups[priorFgIndex].craftType) {
						separationDistance = g_cachedCraftTechStats[statsIndex].sizeRating;
						break;
					}
				}

				separationDistance = (int)((double)(currentSizeRating + separationDistance) *
										   g_skirmishCraftSeparationScale) >>
									 1;
				separationDistance *= separationDistance;
				zDelta = g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z -
						 g_frontendMission->flightGroups[priorFgIndex].missionPoints[XWA_FG_POINT_START_1].z;
				if (zDelta * zDelta < separationDistance) {
					tooClose = 1;
					break;
				}
			}

			if (!tooClose) {
				break;
			}
		}

		--retriesRemaining;
	} while (retriesRemaining != 0);

	return 1;
}

// FUNCTION: XWA 0x57DEA0
int Skirmish_ApplyGeneratedMissionGoals(int flightGroupCount) {
	enum {
		SKIRMISH_GLOBAL_GROUP_PRIMARY = 3,
		SKIRMISH_GLOBAL_GROUP_SECONDARY = 6,

		SKIRMISH_TEAM_GOAL_MODE_1 = 0,
		SKIRMISH_TEAM_GOAL_MODE_2 = 1,
		SKIRMISH_TEAM_GOAL_MODE_3 = 2,
		SKIRMISH_TEAM_GOAL_MODE_4 = 3,
		SKIRMISH_TEAM_GOAL_MODE_5 = 4,

		MISSION_COND_DESTROYED = 2,
		MISSION_COND_INSPECTED = 5,
		MISSION_COND_BOARDED = 6,
		MISSION_COND_13 = 13,

		TRIGVAR_NOT_TEAM = 21,
	};

	int flightGroupIndex;
	int teamIndex;

	for (flightGroupIndex = 0; flightGroupIndex < flightGroupCount; ++flightGroupIndex) {
		for (teamIndex = 0; teamIndex < g_teamCount; ++teamIndex) {
			if (teamIndex == g_frontendMission->flightGroups[flightGroupIndex].team) {
				continue;
			}

			switch (g_gameConfig.teamGoals[teamIndex]) {
				case SKIRMISH_TEAM_GOAL_MODE_1:
					g_frontendMission->flightGroups[flightGroupIndex].fgGoals[0].payload.argument = 0;
					g_frontendMission->flightGroups[flightGroupIndex].fgGoals[0].payload.condition =
						MISSION_COND_DESTROYED;
					g_frontendMission->flightGroups[flightGroupIndex]
						.fgGoals[0]
						.payload.enabledForTeam[teamIndex] = 1;
					break;

				case SKIRMISH_TEAM_GOAL_MODE_2:
					if (g_frontendMission->flightGroups[flightGroupIndex].globalGroup ==
							SKIRMISH_GLOBAL_GROUP_PRIMARY ||
						g_frontendMission->flightGroups[flightGroupIndex].globalGroup ==
							SKIRMISH_GLOBAL_GROUP_SECONDARY) {
						g_frontendMission->flightGroups[flightGroupIndex].fgGoals[0].payload.argument = 0;
						g_frontendMission->flightGroups[flightGroupIndex].fgGoals[0].payload.condition =
							MISSION_COND_DESTROYED;
						g_frontendMission->flightGroups[flightGroupIndex]
							.fgGoals[0]
							.payload.enabledForTeam[teamIndex] = 1;
					}
					break;

				case SKIRMISH_TEAM_GOAL_MODE_3:
					if (g_frontendMission->flightGroups[flightGroupIndex].globalGroup ==
							SKIRMISH_GLOBAL_GROUP_PRIMARY ||
						g_frontendMission->flightGroups[flightGroupIndex].globalGroup ==
							SKIRMISH_GLOBAL_GROUP_SECONDARY) {
						g_frontendMission->flightGroups[flightGroupIndex].fgGoals[3].payload.argument = 0;
						g_frontendMission->flightGroups[flightGroupIndex].fgGoals[3].payload.condition =
							MISSION_COND_BOARDED;
						g_frontendMission->flightGroups[flightGroupIndex]
							.fgGoals[3]
							.payload.enabledForTeam[teamIndex] = 1;
					}
					break;

				case SKIRMISH_TEAM_GOAL_MODE_4:
					if (g_frontendMission->flightGroups[flightGroupIndex].globalGroup ==
							SKIRMISH_GLOBAL_GROUP_PRIMARY ||
						g_frontendMission->flightGroups[flightGroupIndex].globalGroup ==
							SKIRMISH_GLOBAL_GROUP_SECONDARY) {
						g_frontendMission->flightGroups[flightGroupIndex].fgGoals[0].payload.argument = 0;
						g_frontendMission->flightGroups[flightGroupIndex].fgGoals[0].payload.condition =
							MISSION_COND_INSPECTED;
						g_frontendMission->flightGroups[flightGroupIndex]
							.fgGoals[0]
							.payload.enabledForTeam[teamIndex] = 1;
					}
					break;

				case SKIRMISH_TEAM_GOAL_MODE_5:
					if (g_frontendMission->flightGroups[flightGroupIndex].globalGroup ==
							SKIRMISH_GLOBAL_GROUP_PRIMARY ||
						g_frontendMission->flightGroups[flightGroupIndex].globalGroup ==
							SKIRMISH_GLOBAL_GROUP_SECONDARY) {
						g_frontendMission->flightGroups[flightGroupIndex].fgGoals[0].payload.argument = 0;
						g_frontendMission->flightGroups[flightGroupIndex].fgGoals[0].payload.condition =
							MISSION_COND_DESTROYED;
						g_frontendMission->flightGroups[flightGroupIndex]
							.fgGoals[0]
							.payload.enabledForTeam[teamIndex] = 1;
						g_frontendMission->flightGroups[flightGroupIndex].fgGoals[1].payload.argument = 0;
						g_frontendMission->flightGroups[flightGroupIndex].fgGoals[1].payload.condition =
							MISSION_COND_INSPECTED;
						g_frontendMission->flightGroups[flightGroupIndex]
							.fgGoals[1]
							.payload.enabledForTeam[teamIndex] = 1;
					}
					break;

				default:
					break;
			}
		}
	}

	if (g_gameConfig.goalType == 0) {
		for (teamIndex = 0; teamIndex < g_teamCount; ++teamIndex) {
			g_frontendMission->globalGoals[teamIndex][1].triggerPairs[0].triggers[0].condition =
				MISSION_COND_13;
			g_frontendMission->globalGoals[teamIndex][1].triggerPairs[0].triggers[0].variableType =
				TRIGVAR_NOT_TEAM;
			g_frontendMission->globalGoals[teamIndex][1].triggerPairs[0].triggers[0].variable =
				(uint8_t)teamIndex;
		}
	}

	return 1;
}

// FUNCTION: XWA 0x5793E0
int Skirmish_GenerateMission(const char* outMissionFile) {
	enum {
		COMBAT_SIM_SLOT_COUNT = 16,
		TEAM_COUNT = 10,

		SHIP_CATEGORY_FIGHTER = 1,
		SHIP_CATEGORY_BOMBER = 2,
		SHIP_CATEGORY_TRANSPORT = 3,
		SHIP_CATEGORY_PLATFORM = 7,
		SHIP_CATEGORY_MINE = 8,
		SHIP_CATEGORY_UTILITY = 9,

		CRAFT_TYPE_ASTEROID_MIN = 75,
		CRAFT_TYPE_ASTEROID_MAX = 77,
		CRAFT_TYPE_ASTEROID_3 = 77,
		CRAFT_TYPE_HYPER_BUOY = 85,
		CRAFT_TYPE_SPACE_DEBRIS = 86,
		CRAFT_TYPE_BACKDROP = 183,
		CRAFT_TYPE_CLUSTER_MINE_A = 77,

		GLOBAL_GROUP_FIGHTERS = 1,
		GLOBAL_GROUP_HEAVY_CRAFT = 2,
		GLOBAL_GROUP_PRIMARY = 3,
		GLOBAL_GROUP_REGION_STEP = 3,
		GLOBAL_GROUP_SCENERY = 10,
		GLOBAL_GROUP_ORDER_TARGET_1 = 0,
		GLOBAL_GROUP_ORDER_TARGET_2 = 1,

		TARGET_GLOBAL_GROUP = 8,

		STATUS_START_IN_REGION = 9,
		STATUS_SCENERY = 20,
		STATUS2_ACTIVE = 22,
		STATUS2_BUOY = 25,

		NEUTRAL_IFF = 3,
		BACKDROP_IFF = 2,
		NEUTRAL_TEAM = 9,
		HYPER_BUOY_Z = 159,
		REGION_STRIDE = 4,

		FIRST_PLAYER_NUMBER = 1,
		FIRST_GLOBAL_UNIT = 1,
		PRIMARY_FG_DESIGNATION = 3,
		PRIMARY_FG_DESIGNATION2 = 7,
		PRIMARY_FG_CARGO_BASE = 1195,
		PRIMARY_FG_CARGO_COUNT = 9,

		REGION_BUOY_ENABLE_DESIGNATION = 8,
		REGION_BUOY_TO_TEAM1_DESIGNATION = 16,
		REGION_BUOY_TO_TEAM2_DESIGNATION = 18,
		REGION_BUOY_TO_NEUTRAL_DESIGNATION = 17,
		REGION_BUOY_FROM_TEAM1_DESIGNATION = 12,
		REGION_BUOY_FROM_TEAM2_DESIGNATION = 14,
		REGION_BUOY_FROM_NEUTRAL_DESIGNATION = 20,

		RANDOM_BYTE_CENTER = 128,
		MAX_BACKDROP_REGION_COUNT = 3,
	};

	XwaFile* outputStream;
	XwaFile* specStream;
	size_t specSize;
	int previousTeam;
	int slotIndex;
	int fgIndex;
	int nextPlayerNumber;
	unsigned int heavyCraftPerTeam[10];
	int scanFgIndex;
	int teamIndex;
	int regionIndex;
	int backdropRegionCount;
	int angleIndex;
	int orderIndex;
	int allyIndex;
	double distance;
	double radius;
	double angle;
	char path[256];

	if (outMissionFile != NULL) {
		sprintf(path, "%s\\%s", g_campaignDirNames[MISSION_DIRECTORY_SKIRMISH], outMissionFile);
		outputStream = File_Open(AERON_VFS_ROOT_ASSET, path, "wb");
		if (outputStream == NULL) {
			return 0;
		}
		strcpy(g_pilotData.missionFileName, outMissionFile);
	}

	specStream = File_Open(AERON_VFS_ROOT_ASSET, "spec.rci", "rb");
	if (specStream != NULL) {
		specSize = (size_t)File_GetSize(specStream);
		if (g_cachedCraftTechStats != NULL) {
			Mem_Free(g_cachedCraftTechStats);
			g_cachedCraftTechStats = NULL;
		}
		g_cachedCraftTechStats = (CraftTechStats*)Mem_Alloc(specSize);
		if (g_cachedCraftTechStats != NULL) {
			File_ReadCount(specStream, g_cachedCraftTechStats, specSize);
			g_cachedCraftTechStatsCount = specSize / 0x30;
		}
		File_Close(specStream);
	}

	srand(g_gameConfig.randomSeed);
	Math_SetFpuSinglePrecisionMode();
	Skirmish_InitMissionDefaults();

	g_skirmishRegionHasCraft[0] = 0;
	g_skirmishRegionHasCraft[1] = 0;
	g_skirmishRegionHasCraft[2] = 0;
	fgIndex = 0;
	nextPlayerNumber = FIRST_PLAYER_NUMBER;
	previousTeam = -1;
	g_skirmishRegionHasCraft[3] = 0;

	if (g_gameConfig.goalType) {
		g_frontendMission->header.goalsUnimportant = 1;
		g_frontendMission->header.timeLimitMin = g_gameConfig.timeLimit;
	}

	memset(heavyCraftPerTeam, 0, sizeof(heavyCraftPerTeam));
	memset(g_skirmishTeamHasPrimaryFg, 0, sizeof(g_skirmishTeamHasPrimaryFg));
	memset(g_skirmishTeamHasEscortRole, 0, sizeof(g_skirmishTeamHasEscortRole));
	memset(g_skirmishTeamHasStrikeRole, 0, sizeof(g_skirmishTeamHasStrikeRole));
	memset(g_skirmishTeamHasSuperiorityRole, 0, sizeof(g_skirmishTeamHasSuperiorityRole));
	memset(g_skirmishTeamHasReconRole, 0, sizeof(g_skirmishTeamHasReconRole));
	memset(g_skirmishTeamHasDisableRole, 0, sizeof(g_skirmishTeamHasDisableRole));
	memset(g_skirmishTeamHasCaptureRole, 0, sizeof(g_skirmishTeamHasCaptureRole));

	if (g_gameConfig.numberOfTeams > 2u || g_gameConfig.environment == 3 || !g_gameConfig.eachTeamOwnRegion) {
		g_skirmishRegionHasCraft[0] = 1;
	} else {
		for (slotIndex = 0; slotIndex < COMBAT_SIM_SLOT_COUNT; ++slotIndex) {
			if (g_combatSimSlots[slotIndex].craftType != 0) {
				g_skirmishRegionHasCraft[slotIndex / (COMBAT_SIM_SLOT_COUNT / g_teamCount)] = 1;
			}
		}
	}

	for (slotIndex = 0; slotIndex < COMBAT_SIM_SLOT_COUNT; ++slotIndex) {
		int shipCategory;
		int isSmallCraft;

		if (g_combatSimSlots[slotIndex].craftType == 0) {
			continue;
		}

		g_combatSimSlots[slotIndex].fgIndex = fgIndex;

		g_frontendMission->flightGroups[fgIndex].team = slotIndex / (COMBAT_SIM_SLOT_COUNT / g_teamCount);
		strcpy(g_frontendMission->flightGroups[fgIndex].name, g_combatSimSlotNames[slotIndex]);
		strcpy(g_frontendMission->flightGroups[fgIndex].craftRole,
			   FrontendString_Get((UIString)(g_combatSimSlots[slotIndex].craftRole + STR_GAME_STATIONARY)));
		g_frontendMission->flightGroups[fgIndex].craftType = g_combatSimSlots[slotIndex].craftType;

		if (g_combatSimSlots[slotIndex].ownerPlayerId != 0) {
			g_frontendMission->flightGroups[fgIndex].playerNumber = nextPlayerNumber;
			g_frontendMission->flightGroups[fgIndex].globalUnit = slotIndex + FIRST_GLOBAL_UNIT;
			++nextPlayerNumber;
		} else if (!g_combatSimSlots[slotIndex].primaryFg) {
			g_frontendMission->flightGroups[fgIndex].radio =
				g_frontendMission->flightGroups[fgIndex].team + 1;
		}

		if (g_gameConfig.goalType) {
			if (g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[slotIndex].craftType]].flyable) {
				g_frontendMission->flightGroups[fgIndex].numberOfWaves = 99;
			} else {
				g_frontendMission->flightGroups[fgIndex].numberOfWaves =
					g_combatSimSlots[slotIndex].numberOfWaves;
			}
		} else {
			g_frontendMission->flightGroups[fgIndex].numberOfWaves =
				g_combatSimSlots[slotIndex].numberOfWaves;
		}

		g_frontendMission->flightGroups[fgIndex].numberOfCraft = g_combatSimSlots[slotIndex].numberOfCraft;
		g_frontendMission->flightGroups[fgIndex].groupAI = g_combatSimSlots[slotIndex].groupAI;
		g_frontendMission->flightGroups[fgIndex].warhead = g_combatSimSlots[slotIndex].warhead;
		if (g_frontendMission->flightGroups[fgIndex].craftType == CRAFT_TYPE_CLUSTER_MINE_A) {
			g_frontendMission->flightGroups[fgIndex].warhead = 3;
		}
		g_frontendMission->flightGroups[fgIndex].countermeasures =
			g_combatSimSlots[slotIndex].countermeasures;
		g_frontendMission->flightGroups[fgIndex].beam = g_combatSimSlots[slotIndex].beam;
		g_frontendMission->flightGroups[fgIndex].status2 = STATUS2_ACTIVE;

		if (g_gameConfig.numberOfTeams <= 4u) {
			g_frontendMission->flightGroups[fgIndex].iff = g_frontendMission->flightGroups[fgIndex].team;
			if (g_frontendMission->flightGroups[fgIndex].iff == 3) {
				g_frontendMission->flightGroups[fgIndex].iff = 5;
			}
		} else {
			g_frontendMission->flightGroups[fgIndex].iff = 0;
		}

		shipCategory = g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[slotIndex].craftType]].category;
		if (shipCategory == SHIP_CATEGORY_PLATFORM || shipCategory == SHIP_CATEGORY_MINE) {
			g_frontendMission->flightGroups[fgIndex].formationSpacing = 9;
		}

		shipCategory = g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[slotIndex].craftType]].category;
		isSmallCraft = shipCategory == SHIP_CATEGORY_FIGHTER || shipCategory == SHIP_CATEGORY_BOMBER ||
					   shipCategory == SHIP_CATEGORY_MINE || shipCategory == SHIP_CATEGORY_UTILITY;

		if (g_gameConfig.numberOfTeams > 2u || g_gameConfig.environment == 3) {
			if (g_frontendMission->flightGroups[fgIndex].team != previousTeam) {
				if (g_gameConfig.environment == 3) {
					g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x =
						(int16_t)(int)(cos(g_skirmishTeamSpawnAnglesRad
											   [g_frontendMission->flightGroups[fgIndex].team]) *
									   238.5);
					g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y =
						(int16_t)(int)(sin(g_skirmishTeamSpawnAnglesRad
											   [g_frontendMission->flightGroups[fgIndex].team]) *
									   238.5);
					g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z = 0;
				} else {
					g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x =
						(int16_t)(int)(cos(g_skirmishTeamSpawnAnglesRad
											   [g_frontendMission->flightGroups[fgIndex].team]) *
									   ((double)g_gameConfig.initialDistance * 79.5));
					g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y =
						(int16_t)(int)(sin(g_skirmishTeamSpawnAnglesRad
											   [g_frontendMission->flightGroups[fgIndex].team]) *
									   ((double)g_gameConfig.initialDistance * 79.5));
					if (isSmallCraft) {
						g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z =
							(int16_t)(rand() % 256 - RANDOM_BYTE_CENTER);
					} else {
						g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z =
							(int16_t)(int)((double)heavyCraftPerTeam[g_frontendMission->flightGroups[fgIndex]
																		 .team] *
											   ((double)g_gameConfig.numberOfTeams * 79.5) +
										   (double)g_frontendMission->flightGroups[fgIndex].team * 79.5);
						++heavyCraftPerTeam[g_frontendMission->flightGroups[fgIndex].team];
					}
				}
			} else {
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x =
					(int16_t)(g_frontendMission->flightGroups[fgIndex - 1]
								  .missionPoints[XWA_FG_POINT_START_1]
								  .x +
							  rand() % 256 - RANDOM_BYTE_CENTER);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y =
					(int16_t)(g_frontendMission->flightGroups[fgIndex - 1]
								  .missionPoints[XWA_FG_POINT_START_1]
								  .y +
							  rand() % 256 - RANDOM_BYTE_CENTER);
				if (isSmallCraft) {
					g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z =
						(int16_t)(rand() % 256 - RANDOM_BYTE_CENTER);
				} else {
					g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z =
						(int16_t)(int)((double)heavyCraftPerTeam[g_frontendMission->flightGroups[fgIndex]
																	 .team] *
										   ((double)g_gameConfig.numberOfTeams * 79.5) +
									   (double)g_frontendMission->flightGroups[fgIndex].team * 79.5);
					++heavyCraftPerTeam[g_frontendMission->flightGroups[fgIndex].team];
				}
			}
			g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].enabled = 1;
		} else if (g_gameConfig.eachTeamOwnRegion) {
			g_frontendMission->flightGroups[fgIndex].status1 = STATUS_START_IN_REGION;
			if (g_frontendMission->flightGroups[fgIndex].team != previousTeam) {
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x =
					(int16_t)(int)(cos(g_skirmishTeamSpawnAnglesRad[g_frontendMission->flightGroups[fgIndex]
																		.team]) *
								   ((double)g_gameConfig.initialDistance * 79.5));
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y =
					(int16_t)(int)(sin(g_skirmishTeamSpawnAnglesRad[g_frontendMission->flightGroups[fgIndex]
																		.team]) *
								   ((double)g_gameConfig.initialDistance * 79.5));
				if (isSmallCraft) {
					g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z =
						(int16_t)(rand() % 256 - RANDOM_BYTE_CENTER);
				} else {
					g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z =
						(int16_t)(int)((double)heavyCraftPerTeam[g_frontendMission->flightGroups[fgIndex]
																	 .team] *
										   ((double)g_gameConfig.numberOfTeams * 79.5) +
									   (double)g_frontendMission->flightGroups[fgIndex].team * 79.5);
					++heavyCraftPerTeam[g_frontendMission->flightGroups[fgIndex].team];
				}
			} else {
				Skirmish_PlaceFlightGroupNearPrevious(fgIndex);
			}
			g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].enabled = 1;
			g_frontendMission->flightGroups[fgIndex].missionPointRegions[XWA_FG_POINT_START_1] =
				2 * g_frontendMission->flightGroups[fgIndex].team;
		} else {
			if (g_frontendMission->flightGroups[fgIndex].team != previousTeam) {
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x =
					(int16_t)(int)(cos(g_skirmishTeamSpawnAnglesRad[g_frontendMission->flightGroups[fgIndex]
																		.team]) *
								   ((double)g_gameConfig.initialDistance * 79.5));
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y =
					(int16_t)(int)(sin(g_skirmishTeamSpawnAnglesRad[g_frontendMission->flightGroups[fgIndex]
																		.team]) *
								   ((double)g_gameConfig.initialDistance * 79.5));
				if (isSmallCraft) {
					g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z =
						(int16_t)(rand() % 256 - RANDOM_BYTE_CENTER);
				} else {
					g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z =
						(int16_t)(int)((double)heavyCraftPerTeam[g_frontendMission->flightGroups[fgIndex]
																	 .team] *
										   ((double)g_gameConfig.numberOfTeams * 79.5) +
									   (double)g_frontendMission->flightGroups[fgIndex].team * 79.5);
					++heavyCraftPerTeam[g_frontendMission->flightGroups[fgIndex].team];
				}
			} else {
				Skirmish_PlaceFlightGroupNearPrevious(fgIndex);
			}
			g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].enabled = 1;
		}

		if (g_gameConfig.numberOfTeams > 2u || g_gameConfig.environment == 3 ||
			!g_gameConfig.eachTeamOwnRegion) {
			if (g_combatSimSlots[slotIndex].primaryFg) {
				g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_PRIMARY;
			} else if (g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[slotIndex].craftType]]
							   .category == SHIP_CATEGORY_FIGHTER ||
					   g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[slotIndex].craftType]]
							   .category == SHIP_CATEGORY_BOMBER ||
					   g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[slotIndex].craftType]]
							   .category == SHIP_CATEGORY_TRANSPORT) {
				g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_FIGHTERS;
			} else {
				g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_HEAVY_CRAFT;
			}
		} else if (g_combatSimSlots[slotIndex].primaryFg) {
			g_frontendMission->flightGroups[fgIndex].globalGroup =
				GLOBAL_GROUP_REGION_STEP * (g_frontendMission->flightGroups[fgIndex].team + 1);
		} else if (g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[slotIndex].craftType]].category ==
					   SHIP_CATEGORY_FIGHTER ||
				   g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[slotIndex].craftType]].category ==
					   SHIP_CATEGORY_BOMBER ||
				   g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[slotIndex].craftType]].category ==
					   SHIP_CATEGORY_TRANSPORT) {
			g_frontendMission->flightGroups[fgIndex].globalGroup =
				GLOBAL_GROUP_REGION_STEP * g_frontendMission->flightGroups[fgIndex].team + 1;
		} else {
			g_frontendMission->flightGroups[fgIndex].globalGroup =
				GLOBAL_GROUP_REGION_STEP * g_frontendMission->flightGroups[fgIndex].team + 2;
		}

		if (g_combatSimSlots[slotIndex].primaryFg) {
			g_skirmishTeamHasPrimaryFg[g_frontendMission->flightGroups[fgIndex].team] = 1;
			g_frontendMission->flightGroups[fgIndex].enableDesignation =
				g_frontendMission->flightGroups[fgIndex].team;
			g_frontendMission->flightGroups[fgIndex].designation1 = PRIMARY_FG_DESIGNATION;
			g_frontendMission->flightGroups[fgIndex].enableDesignation2 = TEAM_COUNT;
			g_frontendMission->flightGroups[fgIndex].designation2 = PRIMARY_FG_DESIGNATION2;
			strcpy(g_frontendMission->flightGroups[fgIndex].cargo,
				   FrontendString_Get((UIString)(rand() % PRIMARY_FG_CARGO_COUNT + PRIMARY_FG_CARGO_BASE)));
		}

		Skirmish_BuildFlightGroupDutyOrders(slotIndex, fgIndex);
		previousTeam = g_frontendMission->flightGroups[fgIndex].team;
		++fgIndex;
	}

	for (teamIndex = 0; teamIndex < g_gameConfig.numberOfTeams; ++teamIndex) {
		for (scanFgIndex = 0; scanFgIndex < fgIndex; ++scanFgIndex) {
			if (g_frontendMission->flightGroups[scanFgIndex].team == teamIndex &&
				g_frontendMission->flightGroups[scanFgIndex].playerNumber != 0) {
				break;
			}
		}

		if (scanFgIndex >= fgIndex) {
			for (scanFgIndex = 0; scanFgIndex < fgIndex; ++scanFgIndex) {
				if (g_frontendMission->flightGroups[scanFgIndex].team == teamIndex) {
					g_frontendMission->flightGroups[scanFgIndex].playerNumber = nextPlayerNumber;
					++nextPlayerNumber;
					break;
				}
			}
		}
	}

	Skirmish_ApplyGeneratedMissionGoals(fgIndex);

	if (g_gameConfig.numberOfTeams <= 2u && g_gameConfig.environment != 3 && g_gameConfig.eachTeamOwnRegion) {
		for (regionIndex = 0; regionIndex < MAX_BACKDROP_REGION_COUNT; ++regionIndex) {
			g_frontendMission->flightGroups[fgIndex].craftType = CRAFT_TYPE_HYPER_BUOY;
			g_frontendMission->flightGroups[fgIndex].numberOfWaves = 0;
			g_frontendMission->flightGroups[fgIndex].numberOfCraft = 1;
			g_frontendMission->flightGroups[fgIndex].status1 = STATUS_SCENERY;
			g_frontendMission->flightGroups[fgIndex].status2 = STATUS2_BUOY;
			g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_SCENERY;
			g_frontendMission->flightGroups[fgIndex].iff = NEUTRAL_IFF;
			g_frontendMission->flightGroups[fgIndex].team = NEUTRAL_TEAM;
			if (regionIndex == 1) {
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x = 0;
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y =
					(int16_t)(int)(sin(g_skirmishTeamSpawnAnglesRad[0]) * 238.5);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z = 0;
			}
			g_frontendMission->flightGroups[fgIndex].missionPointRegions[XWA_FG_POINT_START_1] = regionIndex;
			g_frontendMission->flightGroups[fgIndex].enableDesignation = REGION_BUOY_ENABLE_DESIGNATION;
			if (regionIndex == 0) {
				g_frontendMission->flightGroups[fgIndex].designation1 = REGION_BUOY_TO_NEUTRAL_DESIGNATION;
				strcpy(g_frontendMission->flightGroups[fgIndex].name, FrontendString_Get(STR_TO_NEUTRAL_RGN));
			} else if (regionIndex == 1) {
				g_frontendMission->flightGroups[fgIndex].designation1 = REGION_BUOY_TO_TEAM1_DESIGNATION;
				strcpy(g_frontendMission->flightGroups[fgIndex].name, FrontendString_Get(STR_TO_TEAM1_RGN));
			} else {
				g_frontendMission->flightGroups[fgIndex].designation1 = REGION_BUOY_TO_NEUTRAL_DESIGNATION;
				strcpy(g_frontendMission->flightGroups[fgIndex].name, FrontendString_Get(STR_TO_NEUTRAL_RGN));
			}
			++fgIndex;

			g_frontendMission->flightGroups[fgIndex].craftType = CRAFT_TYPE_HYPER_BUOY;
			g_frontendMission->flightGroups[fgIndex].numberOfWaves = 0;
			g_frontendMission->flightGroups[fgIndex].numberOfCraft = 1;
			g_frontendMission->flightGroups[fgIndex].status1 = STATUS_SCENERY;
			g_frontendMission->flightGroups[fgIndex].status2 = STATUS2_BUOY;
			g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_SCENERY;
			g_frontendMission->flightGroups[fgIndex].iff = NEUTRAL_IFF;
			g_frontendMission->flightGroups[fgIndex].team = NEUTRAL_TEAM;
			if (regionIndex == 1) {
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x = 0;
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y =
					(int16_t)(int)(sin(g_skirmishTeamSpawnAnglesRad[0]) * 238.5);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z = HYPER_BUOY_Z;
			} else {
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z = HYPER_BUOY_Z;
			}
			g_frontendMission->flightGroups[fgIndex].missionPointRegions[XWA_FG_POINT_START_1] = regionIndex;
			g_frontendMission->flightGroups[fgIndex].enableDesignation = REGION_BUOY_ENABLE_DESIGNATION;
			g_frontendMission->flightGroups[fgIndex].designation1 = REGION_BUOY_FROM_NEUTRAL_DESIGNATION;
			if (regionIndex == 1) {
				g_frontendMission->flightGroups[fgIndex].designation1 = REGION_BUOY_FROM_TEAM1_DESIGNATION;
				strcpy(g_frontendMission->flightGroups[fgIndex].name, FrontendString_Get(STR_FROM_TEAM1_RGN));
			} else {
				strcpy(g_frontendMission->flightGroups[fgIndex].name,
					   FrontendString_Get(STR_FROM_NEUTRAL_RGN));
			}

			++fgIndex;

			if (regionIndex == 1) {
				g_frontendMission->flightGroups[fgIndex].craftType = CRAFT_TYPE_HYPER_BUOY;
				g_frontendMission->flightGroups[fgIndex].numberOfWaves = 0;
				g_frontendMission->flightGroups[fgIndex].numberOfCraft = 1;
				g_frontendMission->flightGroups[fgIndex].status1 = STATUS_SCENERY;
				g_frontendMission->flightGroups[fgIndex].status2 = STATUS2_BUOY;
				g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_SCENERY;
				g_frontendMission->flightGroups[fgIndex].iff = NEUTRAL_IFF;
				g_frontendMission->flightGroups[fgIndex].team = NEUTRAL_TEAM;
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x = 0;
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y =
					(int16_t)(int)-(sin(g_skirmishTeamSpawnAnglesRad[0]) * 238.5);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z = 0;
				g_frontendMission->flightGroups[fgIndex].missionPointRegions[XWA_FG_POINT_START_1] = 1;
				g_frontendMission->flightGroups[fgIndex].enableDesignation = REGION_BUOY_ENABLE_DESIGNATION;
				g_frontendMission->flightGroups[fgIndex].designation1 = REGION_BUOY_TO_TEAM2_DESIGNATION;
				strcpy(g_frontendMission->flightGroups[fgIndex].name, FrontendString_Get(STR_TO_TEAM2_RGN));
				++fgIndex;

				g_frontendMission->flightGroups[fgIndex].craftType = CRAFT_TYPE_HYPER_BUOY;
				g_frontendMission->flightGroups[fgIndex].numberOfWaves = 0;
				g_frontendMission->flightGroups[fgIndex].numberOfCraft = 1;
				g_frontendMission->flightGroups[fgIndex].status1 = STATUS_SCENERY;
				g_frontendMission->flightGroups[fgIndex].status2 = STATUS2_BUOY;
				g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_SCENERY;
				g_frontendMission->flightGroups[fgIndex].iff = NEUTRAL_IFF;
				g_frontendMission->flightGroups[fgIndex].team = NEUTRAL_TEAM;
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x = 0;
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y =
					(int16_t)(int)-(sin(g_skirmishTeamSpawnAnglesRad[0]) * 238.5);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z = HYPER_BUOY_Z;
				g_frontendMission->flightGroups[fgIndex].missionPointRegions[XWA_FG_POINT_START_1] = 1;
				g_frontendMission->flightGroups[fgIndex].enableDesignation = REGION_BUOY_ENABLE_DESIGNATION;
				g_frontendMission->flightGroups[fgIndex].designation1 = REGION_BUOY_FROM_TEAM2_DESIGNATION;
				strcpy(g_frontendMission->flightGroups[fgIndex].name, FrontendString_Get(STR_FROM_TEAM2_RGN));

				++fgIndex;
			}
		}
	}

	if (g_gameConfig.numberOfTeams <= 2u && g_gameConfig.environment != 3) {
		regionIndex = g_gameConfig.eachTeamOwnRegion != 0;
	} else {
		regionIndex = 0;
	}

	switch (g_gameConfig.environment) {
		case 1:
			if (g_gameConfig.initialDistance > 3u) {
				distance = (double)(g_gameConfig.initialDistance - 1);
			} else {
				distance = 3.0;
			}
			radius = distance * 79.5;
			for (angleIndex = 0; angleIndex < 8; ++angleIndex) {
				angle = (double)angleIndex * 0.7853981633974483;
				g_frontendMission->flightGroups[fgIndex].craftType = CRAFT_TYPE_SPACE_DEBRIS;
				g_frontendMission->flightGroups[fgIndex].numberOfWaves = 0;
				g_frontendMission->flightGroups[fgIndex].numberOfCraft = 3;
				g_frontendMission->flightGroups[fgIndex].status1 = STATUS_SCENERY;
				g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_SCENERY;
				g_frontendMission->flightGroups[fgIndex].iff = NEUTRAL_IFF;
				g_frontendMission->flightGroups[fgIndex].team = NEUTRAL_TEAM;
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x =
					(int16_t)(int)(cos(angle) * radius);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y =
					(int16_t)(int)(sin(angle) * radius);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z = 0;
				g_frontendMission->flightGroups[fgIndex].missionPointRegions[XWA_FG_POINT_START_1] =
					regionIndex;
				++fgIndex;
			}

			for (angleIndex = 0; angleIndex < 3; ++angleIndex) {
				g_frontendMission->flightGroups[fgIndex].craftType = CRAFT_TYPE_SPACE_DEBRIS;
				g_frontendMission->flightGroups[fgIndex].numberOfWaves = 0;
				g_frontendMission->flightGroups[fgIndex].numberOfCraft = 3;
				g_frontendMission->flightGroups[fgIndex].status1 = STATUS_SCENERY;
				g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_SCENERY;
				g_frontendMission->flightGroups[fgIndex].iff = NEUTRAL_IFF;
				g_frontendMission->flightGroups[fgIndex].team = NEUTRAL_TEAM;
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x =
					(int16_t)(int)(cos((double)(angleIndex + 1) * 0.7853981633974483) * radius);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y =
					(int16_t)(int)(((double)(rand() % 256) * 0.62109375 - 79.5) * 0.5);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z =
					(int16_t)(int)((double)(rand() % 256) * 0.62109375 - 79.5);
				g_frontendMission->flightGroups[fgIndex].missionPointRegions[XWA_FG_POINT_START_1] =
					regionIndex;
				++fgIndex;
			}

			for (angleIndex = 0; angleIndex < 3; ++angleIndex) {
				g_frontendMission->flightGroups[fgIndex].craftType = CRAFT_TYPE_SPACE_DEBRIS;
				g_frontendMission->flightGroups[fgIndex].numberOfWaves = 0;
				g_frontendMission->flightGroups[fgIndex].numberOfCraft = 3;
				g_frontendMission->flightGroups[fgIndex].status1 = STATUS_SCENERY;
				g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_SCENERY;
				g_frontendMission->flightGroups[fgIndex].iff = NEUTRAL_IFF;
				g_frontendMission->flightGroups[fgIndex].team = NEUTRAL_TEAM;
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x =
					(int16_t)(int)(cos((double)(angleIndex + 5) * 0.7853981633974483) * radius);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y =
					(int16_t)(int)(((double)(rand() % 256) * 0.62109375 - 79.5) * 0.5);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z =
					(int16_t)(int)((double)(rand() % 256) * 0.62109375 - 79.5);
				g_frontendMission->flightGroups[fgIndex].missionPointRegions[XWA_FG_POINT_START_1] =
					regionIndex;
				++fgIndex;
			}

			for (angleIndex = 1; angleIndex < 9; angleIndex += 2) {
				g_frontendMission->flightGroups[fgIndex].craftType = CRAFT_TYPE_SPACE_DEBRIS;
				g_frontendMission->flightGroups[fgIndex].numberOfWaves = 0;
				g_frontendMission->flightGroups[fgIndex].numberOfCraft = 3;
				g_frontendMission->flightGroups[fgIndex].status1 = STATUS_SCENERY;
				g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_SCENERY;
				g_frontendMission->flightGroups[fgIndex].iff = NEUTRAL_IFF;
				g_frontendMission->flightGroups[fgIndex].team = NEUTRAL_TEAM;
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x =
					(int16_t)(int)(((double)(rand() % 256) * 0.62109375 - 79.5) * 0.5);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y =
					(int16_t)(int)(cos((double)angleIndex * 0.7853981633974483) * radius);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z =
					(int16_t)(int)((double)(rand() % 256) * 0.62109375 - 79.5);
				g_frontendMission->flightGroups[fgIndex].missionPointRegions[XWA_FG_POINT_START_1] =
					regionIndex;
				++fgIndex;
			}

			g_frontendMission->flightGroups[fgIndex].craftType = CRAFT_TYPE_SPACE_DEBRIS;
			g_frontendMission->flightGroups[fgIndex].numberOfWaves = 0;
			g_frontendMission->flightGroups[fgIndex].numberOfCraft = 6;
			g_frontendMission->flightGroups[fgIndex].status1 = STATUS_SCENERY;
			g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_SCENERY;
			g_frontendMission->flightGroups[fgIndex].iff = NEUTRAL_IFF;
			g_frontendMission->flightGroups[fgIndex].team = NEUTRAL_TEAM;
			g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x = 0;
			g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y = 0;
			g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z = 0;
			g_frontendMission->flightGroups[fgIndex].missionPointRegions[XWA_FG_POINT_START_1] = regionIndex;
			++fgIndex;
			break;

		case 2:
			distance = g_gameConfig.initialDistance <= 3u ? 3.0 : (double)(g_gameConfig.initialDistance - 1);
			radius = distance * 79.5;
			orderIndex = regionIndex * REGION_STRIDE;

			for (angleIndex = 0; angleIndex < 8; ++angleIndex) {
				int asteroidFgIndex;

				asteroidFgIndex = fgIndex++;
				angle = (double)angleIndex * 0.7853981633974483;
				g_frontendMission->flightGroups[asteroidFgIndex].craftType =
					rand() % (CRAFT_TYPE_ASTEROID_MAX - CRAFT_TYPE_ASTEROID_MIN + 1) +
					CRAFT_TYPE_ASTEROID_MIN;
				g_frontendMission->flightGroups[asteroidFgIndex].numberOfWaves = 0;
				g_frontendMission->flightGroups[asteroidFgIndex].numberOfCraft = 2;
				g_frontendMission->flightGroups[asteroidFgIndex].status1 = STATUS_SCENERY;
				g_frontendMission->flightGroups[asteroidFgIndex].globalGroup = GLOBAL_GROUP_SCENERY;
				g_frontendMission->flightGroups[asteroidFgIndex].iff = NEUTRAL_IFF;
				g_frontendMission->flightGroups[asteroidFgIndex].team = NEUTRAL_TEAM;
				if (g_frontendMission->flightGroups[asteroidFgIndex].craftType == CRAFT_TYPE_ASTEROID_3) {
					g_frontendMission->flightGroups[asteroidFgIndex].warhead = 3;
				}
				g_frontendMission->flightGroups[asteroidFgIndex].missionPoints[XWA_FG_POINT_START_1].x =
					(int16_t)(int)(cos(angle) * radius);
				g_frontendMission->flightGroups[asteroidFgIndex].missionPoints[XWA_FG_POINT_START_1].y =
					(int16_t)(int)(sin(angle) * radius);
				g_frontendMission->flightGroups[asteroidFgIndex].missionPoints[XWA_FG_POINT_START_1].z = 0;
				g_frontendMission->flightGroups[asteroidFgIndex].missionPointRegions[XWA_FG_POINT_START_1] =
					regionIndex;
				g_frontendMission->flightGroups[asteroidFgIndex].orders[orderIndex].target1Type =
					TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[asteroidFgIndex].orders[orderIndex].target1 =
					GLOBAL_GROUP_ORDER_TARGET_1;
				g_frontendMission->flightGroups[asteroidFgIndex].orders[orderIndex].target1OrTarget2 = 1;
				g_frontendMission->flightGroups[asteroidFgIndex].orders[orderIndex].target2Type =
					TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[asteroidFgIndex].orders[orderIndex].target2 =
					GLOBAL_GROUP_ORDER_TARGET_2;
			}

			for (angleIndex = 1; angleIndex < 4; ++angleIndex) {
				g_frontendMission->flightGroups[fgIndex].craftType =
					(uint8_t)(rand() % (CRAFT_TYPE_ASTEROID_MAX - CRAFT_TYPE_ASTEROID_MIN + 1) +
							  CRAFT_TYPE_ASTEROID_MIN);
				g_frontendMission->flightGroups[fgIndex].numberOfWaves = 0;
				g_frontendMission->flightGroups[fgIndex].numberOfCraft = 2;
				g_frontendMission->flightGroups[fgIndex].status1 = STATUS_SCENERY;
				g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_SCENERY;
				g_frontendMission->flightGroups[fgIndex].iff = NEUTRAL_IFF;
				g_frontendMission->flightGroups[fgIndex].team = NEUTRAL_TEAM;
				if (g_frontendMission->flightGroups[fgIndex].craftType == CRAFT_TYPE_ASTEROID_3) {
					g_frontendMission->flightGroups[fgIndex].warhead = 3;
				}
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x =
					(int16_t)(int)(cos((double)angleIndex * 0.7853981633974483) * radius);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y = 0;
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z =
					(int16_t)(int)(sin((double)angleIndex * 0.7853981633974483) * radius);
				g_frontendMission->flightGroups[fgIndex].missionPointRegions[XWA_FG_POINT_START_1] =
					(uint8_t)regionIndex;
				g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target1Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target1 =
					GLOBAL_GROUP_ORDER_TARGET_1;
				g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target1OrTarget2 = 1;
				g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target2Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target2 =
					GLOBAL_GROUP_ORDER_TARGET_2;
				++fgIndex;
			}

			for (angleIndex = 5; angleIndex < 8; ++angleIndex) {
				g_frontendMission->flightGroups[fgIndex].craftType =
					(uint8_t)(rand() % (CRAFT_TYPE_ASTEROID_MAX - CRAFT_TYPE_ASTEROID_MIN + 1) +
							  CRAFT_TYPE_ASTEROID_MIN);
				g_frontendMission->flightGroups[fgIndex].numberOfWaves = 0;
				g_frontendMission->flightGroups[fgIndex].numberOfCraft = 2;
				g_frontendMission->flightGroups[fgIndex].status1 = STATUS_SCENERY;
				g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_SCENERY;
				g_frontendMission->flightGroups[fgIndex].iff = NEUTRAL_IFF;
				g_frontendMission->flightGroups[fgIndex].team = NEUTRAL_TEAM;
				if (g_frontendMission->flightGroups[fgIndex].craftType == CRAFT_TYPE_ASTEROID_3) {
					g_frontendMission->flightGroups[fgIndex].warhead = 3;
				}
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x =
					(int16_t)(int)(cos((double)angleIndex * 0.7853981633974483) * radius);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y = 0;
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z =
					(int16_t)(int)(sin((double)angleIndex * 0.7853981633974483) * radius);
				g_frontendMission->flightGroups[fgIndex].missionPointRegions[XWA_FG_POINT_START_1] =
					(uint8_t)regionIndex;
				g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target1Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target1 =
					GLOBAL_GROUP_ORDER_TARGET_1;
				g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target1OrTarget2 = 1;
				g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target2Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target2 =
					GLOBAL_GROUP_ORDER_TARGET_2;
				++fgIndex;
			}

			for (angleIndex = 1; angleIndex < 9; angleIndex += 2) {
				g_frontendMission->flightGroups[fgIndex].craftType =
					(uint8_t)(rand() % (CRAFT_TYPE_ASTEROID_MAX - CRAFT_TYPE_ASTEROID_MIN + 1) +
							  CRAFT_TYPE_ASTEROID_MIN);
				g_frontendMission->flightGroups[fgIndex].numberOfWaves = 0;
				g_frontendMission->flightGroups[fgIndex].numberOfCraft = 2;
				g_frontendMission->flightGroups[fgIndex].status1 = STATUS_SCENERY;
				g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_SCENERY;
				g_frontendMission->flightGroups[fgIndex].iff = NEUTRAL_IFF;
				g_frontendMission->flightGroups[fgIndex].team = NEUTRAL_TEAM;
				if (g_frontendMission->flightGroups[fgIndex].craftType == CRAFT_TYPE_ASTEROID_3) {
					g_frontendMission->flightGroups[fgIndex].warhead = 3;
				}
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x = 0;
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y =
					(int16_t)(int)(cos((double)angleIndex * 0.7853981633974483) * radius);
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z =
					(int16_t)(int)(sin((double)angleIndex * 0.7853981633974483) * radius);
				g_frontendMission->flightGroups[fgIndex].missionPointRegions[XWA_FG_POINT_START_1] =
					(uint8_t)regionIndex;
				g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target1Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target1 =
					GLOBAL_GROUP_ORDER_TARGET_1;
				g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target1OrTarget2 = 1;
				g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target2Type = TARGET_GLOBAL_GROUP;
				g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target2 =
					GLOBAL_GROUP_ORDER_TARGET_2;
				++fgIndex;
			}

			g_frontendMission->flightGroups[fgIndex].craftType =
				(uint8_t)(rand() % (CRAFT_TYPE_ASTEROID_MAX - CRAFT_TYPE_ASTEROID_MIN + 1) +
						  CRAFT_TYPE_ASTEROID_MIN);
			g_frontendMission->flightGroups[fgIndex].numberOfWaves = 0;
			g_frontendMission->flightGroups[fgIndex].numberOfCraft = 2;
			g_frontendMission->flightGroups[fgIndex].status1 = STATUS_SCENERY;
			g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_SCENERY;
			g_frontendMission->flightGroups[fgIndex].iff = NEUTRAL_IFF;
			g_frontendMission->flightGroups[fgIndex].team = NEUTRAL_TEAM;
			if (g_frontendMission->flightGroups[fgIndex].craftType == CRAFT_TYPE_ASTEROID_3) {
				g_frontendMission->flightGroups[fgIndex].warhead = 3;
			}
			g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x = 0;
			g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y = 0;
			g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z = 0;
			g_frontendMission->flightGroups[fgIndex].missionPointRegions[XWA_FG_POINT_START_1] =
				(uint8_t)regionIndex;
			g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target1Type = TARGET_GLOBAL_GROUP;
			g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target1 = GLOBAL_GROUP_ORDER_TARGET_1;
			g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target1OrTarget2 = 1;
			g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target2Type = TARGET_GLOBAL_GROUP;
			g_frontendMission->flightGroups[fgIndex].orders[orderIndex].target2 = GLOBAL_GROUP_ORDER_TARGET_2;
			++fgIndex;
			break;

		default:
			break;
	}

	backdropRegionCount = 1;
	if (g_gameConfig.numberOfTeams <= 2u && g_gameConfig.environment != 3) {
		backdropRegionCount = g_gameConfig.eachTeamOwnRegion ? 3 : 1;
	}

	for (regionIndex = 0; regionIndex < backdropRegionCount; ++regionIndex) {
		if (!g_skirmishRegionHasCraft[regionIndex] && regionIndex != 1) {
			continue;
		}

		g_frontendMission->flightGroups[fgIndex].craftType = CRAFT_TYPE_BACKDROP;
		g_frontendMission->flightGroups[fgIndex + 1].craftType = CRAFT_TYPE_BACKDROP;
		do {
			g_frontendMission->flightGroups[fgIndex].backdrop = rand() % 60 + 1;
		} while (g_frontendMission->flightGroups[fgIndex].backdrop == 55);
		g_frontendMission->flightGroups[fgIndex + 1].backdrop = rand() % 10 + 84;

		g_frontendMission->flightGroups[fgIndex].globalCargoIndex = rand() % 7;
		g_frontendMission->flightGroups[fgIndex + 1].globalCargoIndex = 0;

		g_frontendMission->flightGroups[fgIndex].numberOfWaves = 0;
		g_frontendMission->flightGroups[fgIndex].numberOfCraft = 1;
		g_frontendMission->flightGroups[fgIndex].globalGroup = GLOBAL_GROUP_SCENERY;
		g_frontendMission->flightGroups[fgIndex].iff = BACKDROP_IFF;
		g_frontendMission->flightGroups[fgIndex].team = NEUTRAL_TEAM;
		g_frontendMission->flightGroups[fgIndex + 1].numberOfWaves = 0;
		g_frontendMission->flightGroups[fgIndex + 1].numberOfCraft = 1;
		g_frontendMission->flightGroups[fgIndex + 1].globalGroup = GLOBAL_GROUP_SCENERY;
		g_frontendMission->flightGroups[fgIndex + 1].iff = BACKDROP_IFF;
		g_frontendMission->flightGroups[fgIndex + 1].team = NEUTRAL_TEAM;

		switch (rand() % 6) {
			case 0:
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x = 1;
				g_frontendMission->flightGroups[fgIndex + 1].missionPoints[XWA_FG_POINT_START_1].z =
					(rand() % 2) ? -1 : 1;
				break;
			case 1:
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].x = -1;
				g_frontendMission->flightGroups[fgIndex + 1].missionPoints[XWA_FG_POINT_START_1].z =
					(rand() % 2) ? -1 : 1;
				break;
			case 2:
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y = 1;
				g_frontendMission->flightGroups[fgIndex + 1].missionPoints[XWA_FG_POINT_START_1].z =
					(rand() % 2) ? -1 : 1;
				break;
			case 3:
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].y = -1;
				g_frontendMission->flightGroups[fgIndex + 1].missionPoints[XWA_FG_POINT_START_1].z =
					(rand() % 2) ? -1 : 1;
				break;
			case 4:
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z = 1;
				g_frontendMission->flightGroups[fgIndex + 1].missionPoints[XWA_FG_POINT_START_1].z = -1;
				break;
			case 5:
				g_frontendMission->flightGroups[fgIndex].missionPoints[XWA_FG_POINT_START_1].z = -1;
				g_frontendMission->flightGroups[fgIndex + 1].missionPoints[XWA_FG_POINT_START_1].z = 1;
				break;
			default:
				break;
		}

		g_frontendMission->flightGroups[fgIndex].missionPointRegions[XWA_FG_POINT_START_1] = regionIndex;
		g_frontendMission->flightGroups[fgIndex + 1].missionPointRegions[XWA_FG_POINT_START_1] = regionIndex;
		strcpy(g_frontendMission->flightGroups[fgIndex].name, "1.0 1.0 1.0");
		strcpy(g_frontendMission->flightGroups[fgIndex + 1].name, "1.0 1.0 1.0");
		sprintf(g_frontendMission->flightGroups[fgIndex].cargo, "%.1f", (double)(rand() % 6 + 5) * 0.1);
		sprintf(g_frontendMission->flightGroups[fgIndex].specialCargo, "%.1f",
				(double)(rand() % 16 + 15) * 0.1);
		sprintf(g_frontendMission->flightGroups[fgIndex + 1].cargo, "%.1f", (double)(rand() % 6 + 5) * 0.1);
		sprintf(g_frontendMission->flightGroups[fgIndex + 1].specialCargo, "1.0");
		fgIndex += 2;
	}

	g_frontendMission->flightGroupCount = (uint16_t)fgIndex;

	for (teamIndex = 0; teamIndex < TEAM_COUNT; ++teamIndex) {
		for (allyIndex = 0; allyIndex < TEAM_COUNT; ++allyIndex) {
			if (allyIndex == teamIndex) {
				g_frontendMission->teams[teamIndex].allies[allyIndex] = 1;
			} else if (allyIndex == 9) {
				g_frontendMission->teams[teamIndex].allies[allyIndex] = 0;
			} else {
				g_frontendMission->teams[teamIndex].allies[allyIndex] = 0;
			}
		}
	}

	if (g_cachedCraftTechStats != NULL) {
		Mem_Free(g_cachedCraftTechStats);
		g_cachedCraftTechStats = NULL;
	}

	if (outMissionFile != NULL) {
		Skirmish_WriteGeneratedMissionFile(outputStream);
		File_Close(outputStream);
	}

	return 1;
}
