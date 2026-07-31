#include "xwa/config/pilot.h"

#include "xwa/assets/file_io.h"
#include "xwa/assets/ship_list.h"
#include "xwa/assets/string_table.h"
#include "xwa/config/game_config.h"
#include "xwa/flight/flight.h"
#include "xwa/frontend/frontend_file_list.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/yard.h"
#include "xwa/flight/player/player.h"
#include "xwa/util/memory.h"
#include "xwa/util/string.h"
#include "xwa/xwa_options.h"
#include "xwa/util/time.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GLOBAL: XWA 0xAE2A60
PilotData g_pilotData;
// GLOBAL: XWA 0xABD7B4
FrontendMissionSessionMode g_frontendMissionSessionMode;

// GLOBAL: XWA 0x601CD0
const char* const g_defaultPilotNames[40] = {
	"Luke",        "Han Solo",  "Darth Vader",  "Leia",       "Lando",      "Boba Fett",   "Chewbacca",
	"R2-D2",       "C-3PO",     "Jabba",        "Greedo",     "Thrawn",     "Ackbar",      "Wedge",
	"Bollux",      "Blue Max",  "Bossk",        "C'Baoth",    "Palpatine",  "Biggs",       "Dodonna",
	"Bib Fortuna", "Mara Jade", "Talon Karrde", "Obi-Wan",    "Lobot",      "Crix Madine", "Mon Mothma",
	"Nien Nunb",   "Pellaeon",  "Anakin Solo",  "Jacen Solo", "Jaina Solo", "Tarkin",      "Yoda",
	"Zaarin",      "Harkov",    "Tarrak",       "Xizor",      "Guri",
};

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x528CF0
int Pilot_CreateNew(const char* pilotName) {
	int i;
	XwaFile* stream;
	char fileName[32];
#ifdef XWA_MODERN
	PilotData previousPilotData;
	FrontendMissionSessionMode previousSessionMode;

	memcpy(&previousPilotData, &g_pilotData, sizeof(previousPilotData));
	previousSessionMode = g_frontendMissionSessionMode;
#endif

	for (i = 0;; ++i) {
		sprintf(fileName, "%s%d.plt", pilotName, i);
		stream = File_Open(AERON_VFS_ROOT_USER, fileName, "rb");
		if (stream == NULL) {
			break;
		}

		File_Close(stream);
	}

	stream = File_Open(AERON_VFS_ROOT_USER, fileName, "wb");
	if (stream == NULL) {
		return 0;
	}

	memset(&g_pilotData, 0, sizeof(g_pilotData));
	strcpy(g_pilotData.name, pilotName);
	g_pilotData.pilotRating = 2;
	g_pilotData.skipMissionsRemaining = 3;
	strcpy(g_pilotData.pilotRatingName, FrontendString_Get(STR_WASHOUT));
	sprintf(g_pilotData.multiplayerGameName, "%s%s", pilotName, FrontendString_Get(STR_SGAME));
	strcpy(g_pilotData.multiplayerHostName, g_pilotData.multiplayerGameName);

	MissionSetup_LoadMissionList(MISSION_DIRECTORY_TOUR);
	if (g_missionList != NULL) {
		g_pilotData.missionDescriptionIds[4] = g_missionList[0].missionIdx;
		g_pilotData.factionStatistics[0].missionDirectoryId = MISSION_DIRECTORY_TOUR;
		g_pilotData.factionStatistics[0].missionDescriptionIds[4] = g_missionList[0].missionIdx;
		if (!strcmp(pilotName, "topace") || !strcmp(pilotName, "FongFong")) {
			if (g_missionCount > 0) {
				for (i = 0; i < g_missionCount; ++i) {
					g_pilotData.tourOfDutyMissions[g_missionList[i].missionIdx].completedCount = 1;
					g_pilotData.tourOfDutyMissions[g_missionList[i].missionIdx].numberTimesFlown = 1;
					g_pilotData.tourOfDutyMissions[g_missionList[i].missionIdx].awardId = 1;
					g_pilotData.tourOfDutyMissions[g_missionList[i].missionIdx].bestScore = 50000;
					g_pilotData.tourOfDutyMissions[g_missionList[i].missionIdx].bestTime = 60;
				}
			}

			g_pilotData.pilotRating = 0;
			g_pilotData.pilotRank = 0;
			g_pilotData.kalidorCresent = 0;
			if (g_shipCount > 0) {
				for (i = 0; i < g_shipCount; ++i) {
					if (g_shipList[i].typeId != OBJ_IndustrialComplex) {
						g_pilotData.craftKnown[g_shipList[i].typeId] = 1;
					}
				}
			}
		}

		if (!strcmp(pilotName, "FongFong")) {
			g_pilotData.pilotRating = 25;
			g_pilotData.pilotRank = 8;
			g_pilotData.kalidorCresent = 6;
		}

		Mem_Free(g_missionList);
		g_missionList = NULL;
	}

	g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_NET_HOST;
	MissionSetup_LoadMissionList(MISSION_DIRECTORY_MELEE);
	if (g_missionList != NULL) {
		g_pilotData.missionDescriptionIds[0] = g_missionList[0].missionIdx;
		g_pilotData.factionStatistics[0].missionDescriptionIds[1] = g_missionList[0].missionIdx;
		g_pilotData.factionStatistics[2].missionDescriptionIds[1] = g_missionList[0].missionIdx;
		g_pilotData.factionStatistics[2].missionDirectoryId = MISSION_DIRECTORY_SKIRMISH;
		g_pilotData.factionStatistics[2].missionDescriptionIds[3] = 0;
		Mem_Free(g_missionList);
		g_missionList = NULL;
	}

	g_frontendMissionSessionMode = FRONTEND_MISSION_SESSION_NONE;
#ifdef XWA_MODERN
	if (!File_WriteCount(stream, &g_pilotData, sizeof(g_pilotData))) {
		File_Close(stream);
		File_Remove(AERON_VFS_ROOT_USER, fileName);
		memcpy(&g_pilotData, &previousPilotData, sizeof(g_pilotData));
		g_frontendMissionSessionMode = previousSessionMode;
		return 0;
	}
#else
	File_WriteCount(stream, &g_pilotData, sizeof(g_pilotData));
#endif
	if (g_shipCount > 0) {
		for (i = 0; i < g_shipCount; ++i) {
			if (g_shipList[i].known) {
				g_pilotData.craftKnown[g_shipList[i].typeId] = 1;
			}
		}
	}

#ifdef XWA_MODERN
	if (File_Close(stream) != 0 || !Pilot_Save(0)) {
		File_Remove(AERON_VFS_ROOT_USER, fileName);
		memcpy(&g_pilotData, &previousPilotData, sizeof(g_pilotData));
		g_frontendMissionSessionMode = previousSessionMode;
		return 0;
	}
#else
	File_Close(stream);
	Pilot_Save(0);
#endif
	return 1;
}

// FUNCTION: XWA 0x5298C0
int Pilot_LoadFile(const char* fileName) {
	XwaFile* stream;
	char localFileName[128];

	memset(&g_pilotData, 0, sizeof(g_pilotData));
	strcpy(localFileName, fileName);
	stream = File_Open(AERON_VFS_ROOT_USER, localFileName, "rb");
	if (stream == NULL) {
		return 0;
	}

	File_ReadCount(stream, &g_pilotData, sizeof(g_pilotData));
	File_Close(stream);
	return 1;
}

// FUNCTION: XWA 0x529200
int Pilot_FindAndLoadByName(const char* pilotName) {
	FrontFilenameList* list;
	FrontFilenameListNode* node;
	XwaFile* stream;
	const char* suffix;
	int i;
	int loaded;

	list = FrontendFileList_BuildSorted(AERON_VFS_ROOT_USER, "*.plt");
	if (list == NULL) {
		return 0;
	} else if (pilotName == NULL) {
		FrontendFileList_Free(list);
		return 0;
	} else if (!pilotName[0]) {
		FrontendFileList_Free(list);
		return 0;
	} else {
		loaded = 0;
		node = list->head;
		i = 0;
		while (i < list->count) {
			stream = File_Open(AERON_VFS_ROOT_USER, node->fileName, "rb");
			if (stream != NULL) {
				File_ReadCount(stream, g_frontendScratchBuffer, sizeof(g_pilotData.name));
				File_Close(stream);
				if (!Xwa_CrtStricmp(g_frontendScratchBuffer, pilotName)) {
					if (Pilot_LoadFile(node->fileName)) {
						loaded = 1;
					}
					break;
				}
			}

			node = node->next;
			++i;
		}

		FrontendFileList_Free(list);
		if (!g_pilotData.multiplayerGameName[0]) {
			suffix = FrontendString_Get(STR_SGAME);
			sprintf(g_pilotData.multiplayerGameName, "%s%s", g_pilotData.name, suffix);
			strcpy(g_pilotData.multiplayerHostName, g_pilotData.multiplayerGameName);
		}

		return loaded;
	}
}

// FUNCTION: XWA 0x5294D0
int Pilot_ParseCommandLine(const char* cmdLine) {
	int cmdIndex;
	int cmdLength;
	int hasPilotName;
	int hasAddress;
	char pilotName[13];
	char parameter[256];

	cmdIndex = 0;
	cmdLength = strlen(cmdLine);
	hasAddress = 0;
	hasPilotName = 0;
	if (cmdLength > 0) {
		do {
			if (cmdLine[cmdIndex] == '"') {
				char ch;
				int parameterIndex;

				ch = cmdLine[++cmdIndex];
				parameterIndex = 0;
				while (ch != '"' && cmdIndex < cmdLength && parameterIndex < 255) {
					parameter[parameterIndex] = ch;
					ch = cmdLine[cmdIndex + 1];
					++parameterIndex;
					++cmdIndex;
				}

				parameter[parameterIndex] = '\0';
				if (parameter[0] == 'a' && parameter[1] == '=') {
					strncpy(g_gameConfig.ipAddress, &parameter[2], 63);
					g_gameConfig.ipAddress[63] = '\0';
					hasAddress = 1;
				} else if (parameter[0] = 'p' && parameter[1] == '=') {
					strncpy(pilotName, &parameter[2], 12);
					pilotName[12] = '\0';
					hasPilotName = 1;
				}
			}

			++cmdIndex;
		} while (cmdIndex < cmdLength);
	}

	if (hasPilotName) {
		if (!g_gameConfig.lastPilotName[0]) {
			if (!strcmp(pilotName, "joiner") || !strcmp(pilotName, "host")) {
				srand(GetTickCount());
				strcpy(g_gameConfig.lastPilotName, g_defaultPilotNames[rand() % 40]);
			} else {
				strcpy(g_gameConfig.lastPilotName, pilotName);
			}

			Pilot_CreateNew(g_gameConfig.lastPilotName);
		} else if (!Pilot_FindAndLoadByName(g_gameConfig.lastPilotName)) {
			if (!strcmp(pilotName, "joiner") || !strcmp(pilotName, "host")) {
				srand(GetTickCount());
				strcpy(g_gameConfig.lastPilotName, g_defaultPilotNames[rand() % 40]);
			} else {
				strcpy(g_gameConfig.lastPilotName, pilotName);
			}

			Pilot_CreateNew(g_gameConfig.lastPilotName);
		}
	}

	if (hasPilotName && hasAddress) {
		g_gameConfig.networkType = 1;
		g_gameConfig.asyncFlag = 1;
		return 1;
	}

	g_optIsHost = 0;
	g_optIsClient = 0;
	return 1;
}

// FUNCTION: XWA 0x529090
int Pilot_Save(int toTempFile) {
	FrontFilenameList* list;
	FrontFilenameListNode* node;
	XwaFile* stream;
	int i;
	char fileName[30];

	if (!g_pilotData.name[0]) {
		return 0;
	}

	memset(fileName, 0, sizeof(fileName));
	if (toTempFile) {
		strcpy(fileName, "__temp__.tmp");
	} else {
		list = FrontendFileList_BuildSorted(AERON_VFS_ROOT_USER, "*.plt");
		if (list != NULL) {
			node = list->head;
			i = 0;
			if (list->count > 0) {
				while (1) {
					stream = File_Open(AERON_VFS_ROOT_USER, node->fileName, "rb");
					if (stream != NULL) {
						File_ReadCount(stream, g_frontendScratchBuffer, sizeof(g_pilotData.name));
						File_Close(stream);
						if (!Xwa_Stricmp(g_frontendScratchBuffer, g_pilotData.name)) {
							strcpy(fileName, node->fileName);
							break;
						}
					}

					node = node->next;
					if (++i >= list->count) {
						break;
					}
				}
			}

			FrontendFileList_Free(list);
		}
	}

	if (!fileName[0]) {
		sprintf(fileName, "%s0.plt", g_pilotData.name);
	}

	stream = File_Open(AERON_VFS_ROOT_USER, fileName, "wb");
	if (stream == NULL) {
		return 0;
	}

#ifdef XWA_MODERN
	if (!File_WriteCount(stream, &g_pilotData, sizeof(g_pilotData))) {
		File_Close(stream);
		return 0;
	}
	return File_Close(stream) == 0;
#else
	File_WriteCount(stream, &g_pilotData, sizeof(g_pilotData));
	File_Close(stream);
	return 1;
#endif
}
