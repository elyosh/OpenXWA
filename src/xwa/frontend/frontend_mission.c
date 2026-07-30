#include "xwa/frontend/frontend_mission.h"

#include "aeron/log.h"

#include "xwa/assets/file_io.h"
#include "xwa/assets/linez.h"
#include "xwa/assets/ship_list.h"
#include "xwa/assets/string_table.h"
#include "xwa/assets/ui_string.h"
#include "xwa/config/game_config.h"
#include "xwa/frontend/briefing_script.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_mission_session.h"
#include "xwa/frontend/frontend_net.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa/frontend/mission_briefing.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/frontend/skirmish.h"
#include "xwa/frontend/net_transport.h"
#include "xwa/util/memory.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
	FRONTEND_MISSION_FORMAT_V17 = 17,
	FRONTEND_MISSION_FORMAT_V18 = 18,
	FRONTEND_MISSION_HEADER_SIZE = 0x23EA,
	FRONTEND_MISSION_SKIRMISH_TEAM_NAME_BASE = STR_GAME_ONE,
	FRONTEND_MISSION_LOCALIZE_NAME_SIZE = 128,
	FRONTEND_MISSION_LOCALIZE_TEXT_SIZE = 4096,
	FRONTEND_MISSION_LOCALIZE_KEY_SIZE = 0x8000,
	FRONTEND_MISSION_ORDER_STRING_SIZE = 64,
};

static int FrontendMission_IsSupportedFormat(uint16_t formatVersion) {
	return formatVersion == FRONTEND_MISSION_FORMAT_V18 || formatVersion == FRONTEND_MISSION_FORMAT_V17;
}

static __inline unsigned int FrontendMission_ConvertLegacyDelayValue(unsigned int value) {
	if (value < 4) {
		return 5 * value;
	}

	if (value < 0xB4) {
		return value + 16;
	}

	return (value >> 1) + 106;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x56BAB0
int FrontendMission_ConvertLegacyDelayValues(FrontendMission* mission) {
	unsigned int flightGroupIndex;
	int outerOrder;
	int innerOrder;
	unsigned int goalIndex;
	int teamIndex;
	int messageIndex;
	XwaFlightGroup* flightGroup;
	XwaOrder* order;

	flightGroup = mission->flightGroups;
	for (flightGroupIndex = 0; flightGroupIndex < (int16_t)mission->flightGroupCount; ++flightGroupIndex) {
		{
			for (outerOrder = 0; outerOrder < 4; ++outerOrder) {
				for (innerOrder = 0; innerOrder < 4; ++innerOrder) {
					order = &flightGroup->orders[outerOrder * 4 + innerOrder];
					switch (order->order) {
						case 1:
						case 0x0C:
						case 0x0D:
						case 0x0E:
						case 0x0F:
						case 0x11:
						case 0x12:
						case 0x13:
						case 0x14:
						case 0x16:
						case 0x17:
						case 0x18:
						case 0x19:
						case 0x1D:
						case 0x1F:
						case 0x20:
						case 0x21:
						case 0x22:
						case 0x23:
						case 0x24:
						case 0x2B:
						case 0x2C:
						case 0x2D:
						case 0x39:
						case 0x3A:
						case 0x3D:
							order->variable1 = FrontendMission_ConvertLegacyDelayValue(order->variable1);
							break;
						case 0x2A:
							order->variable2 = FrontendMission_ConvertLegacyDelayValue(order->variable2);
							break;
						case 0x10:
							order->variable1 = FrontendMission_ConvertLegacyDelayValue(order->variable1);
							order->variable3 = FrontendMission_ConvertLegacyDelayValue(order->variable3);
							break;
						default:
							break;
					}
				}
			}

			for (goalIndex = 0; goalIndex < 8; ++goalIndex) {
				flightGroup->fgGoals[goalIndex].payload.parameter = FrontendMission_ConvertLegacyDelayValue(
					flightGroup->fgGoals[goalIndex].payload.parameter);
			}

			++flightGroup;
		}
	}

	for (teamIndex = 0; teamIndex < 10; ++teamIndex) {
		int globalGoalIndex;

		for (globalGoalIndex = 0; globalGoalIndex < 7; ++globalGoalIndex) {
			mission->globalGoals[teamIndex][globalGoalIndex].rawDelay =
				FrontendMission_ConvertLegacyDelayValue(
					mission->globalGoals[teamIndex][globalGoalIndex].rawDelay);
		}

		for (goalIndex = 0; goalIndex < 3; ++goalIndex) {
			mission->teams[teamIndex].eomRawDelay[goalIndex] =
				FrontendMission_ConvertLegacyDelayValue(mission->teams[teamIndex].eomRawDelay[goalIndex]);
		}
	}

	for (messageIndex = 0; messageIndex < 64; ++messageIndex) {
		mission->messages[messageIndex].rawDelay =
			FrontendMission_ConvertLegacyDelayValue(mission->messages[messageIndex].rawDelay);
	}

	return 1;
}

// FUNCTION: XWA 0x566640
void FrontendMission_Reset(void) {
	char** briefingText;
	int i;

	g_briefingPlaybackActive = 1;
	g_unusedBriefingMissionResetWord = 0;
	g_briefingMapCenter.x = 0;
	g_briefingMapCenter.y = 0;
	g_briefingMapTargetCenter.x = 0;
	g_briefingMapTargetCenter.y = 0;
	g_briefingMapScale.x = 32;
	g_briefingMapScale.y = 32;
	g_briefingMapTargetScale.x = 32;
	g_briefingMapTargetScale.y = 32;
	g_briefingTeamIndex = 0;
	g_briefingLastNarratedTextBlockIdx = 0xFFFF;
	g_briefingTextPageNumber = 0;

	if (g_frontendMission != NULL) {
		memset(g_frontendMission, 0, sizeof(*g_frontendMission));
		g_frontendMission->flightGroupCount = 1;
		g_frontendMission->header.legacyAllWayShown = 0;
		g_frontendMission->header.legacyWinType = 1;
	}

	BriefingScript_InitDefaultScript();
	BriefingScript_ResetState();

	briefingText = g_frontendBriefingContent.mapLabelTexts;
	for (i = FRONTEND_BRIEFING_MAP_LABEL_COUNT; i != 0; --i) {
		if (*briefingText != NULL) {
			Mem_Free(*briefingText);
			*briefingText = NULL;
		}
		*briefingText = (char*)Mem_Alloc(FRONTEND_BRIEFING_MAP_LABEL_SIZE);
		++briefingText;
	}

	briefingText = g_frontendBriefingContent.textBlocks;
	for (i = FRONTEND_BRIEFING_TEXT_BLOCK_COUNT; i != 0; --i) {
		if (*briefingText != NULL) {
			Mem_Free(*briefingText);
			*briefingText = NULL;
		}
		*briefingText = (char*)Mem_Alloc(FRONTEND_BRIEFING_TEXT_BLOCK_SIZE);
		++briefingText;
	}

	memset(g_briefingTextSlotActive, 0, sizeof(g_briefingTextSlotActive));
	memset(g_briefingMapFgMarkers.active, 0, sizeof(g_briefingMapFgMarkers.active));
	memset(g_briefingMapLabels.active, 0, sizeof(g_briefingMapLabels.active));
	FrontendDraw_RectAssign(&g_briefingMapSourceRect, 0, 0, 510, 335);
}

// FUNCTION: XWA 0x566510
int FrontendMission_LoadForBriefing(void) {
	FrontendMission_Reset();
	FrontendMission_LoadCurrentMissionData();
	BriefingScript_ResetState();
	return 1;
}

static void FrontendMission_CopyLocalized(char* dst, size_t dstSize, const char* key) {
	strncpy(dst, Linez_ResolveString((char*)key), dstSize);
	dst[dstSize - 1] = '\0';
}

static void FrontendMission_BuildNumberedKey(char* buffer, const char* format, const char* basename,
											 int index, const char* originalText) {
	sprintf(buffer, format, basename, index);
	strcat(buffer, originalText);
}

static void FrontendMission_BuildBattleKey(char* buffer, const char* format, int battleIndex,
										   int missionIndex, int textIndex, const char* originalText) {
	sprintf(buffer, format, (uint8_t)battleIndex, (uint8_t)missionIndex, textIndex);
	if (buffer[6] == ' ') {
		buffer[6] = '0';
	}
	strcat(buffer, originalText);
}

static char* FrontendMission_OrderStringGate(FrontendMission* mission, int flightGroupIndex, int orderIndex) {
	/* Original frontend code checks a 24-entry stride but localizes the 16-entry order array. */
	return (char*)mission->textTail.orderStrings +
		   ((flightGroupIndex * 24 + orderIndex) * FRONTEND_MISSION_ORDER_STRING_SIZE);
}

static void FrontendMission_LocalizeDelimitedText(char* text, const char* basename, int useNamedKeys,
												  int battleIndex, int missionIndex, const char* namedFormat,
												  const char* battleFormat) {
	char saved[FRONTEND_MISSION_LOCALIZE_TEXT_SIZE];
	char segment[FRONTEND_MISSION_LOCALIZE_TEXT_SIZE];
	char key[FRONTEND_MISSION_LOCALIZE_KEY_SIZE];
	unsigned int cursor;
	int textIndex;

	strcpy(saved, text);
	*text = '\0';
	cursor = 0;
	textIndex = 1;
	while (1) {
		int segmentIndex;
		char separator;

		while (cursor < FRONTEND_MISSION_LOCALIZE_TEXT_SIZE && saved[cursor] == '$') {
			++cursor;
		}

		segmentIndex = 0;
		while (cursor < FRONTEND_MISSION_LOCALIZE_TEXT_SIZE) {
			char ch;

			ch = saved[cursor];
			if (ch == '\0' || ch == '$') {
				break;
			}

			segment[segmentIndex++] = ch;
			++cursor;
		}

		segment[segmentIndex] = '\0';
		if (useNamedKeys) {
			sprintf(key, namedFormat, basename, textIndex);
		} else {
			sprintf(key, battleFormat, (uint8_t)battleIndex, (uint8_t)missionIndex, textIndex);
		}
		strcat(key, segment);
		strcat(text, Linez_ResolveString(key));

		separator = saved[cursor];
		if (separator == '\0' || cursor >= FRONTEND_MISSION_LOCALIZE_TEXT_SIZE) {
			break;
		}

		strcat(text, "$$");
		++textIndex;
	}
}

// FUNCTION: XWA 0x56BEE0
int FrontendMission_Localize(FrontendMission* mission, FrontendBriefingContent* briefing, int teamIndex,
							 char* missionFileName) {
	char basename[FRONTEND_MISSION_LOCALIZE_NAME_SIZE];
	char key[FRONTEND_MISSION_LOCALIZE_KEY_SIZE];
	char saved[FRONTEND_MISSION_LOCALIZE_TEXT_SIZE];
	char segment[FRONTEND_MISSION_LOCALIZE_TEXT_SIZE];
	char firstChar;
	char secondChar;
	char separator;
	int battleIndex;
	int missionIndex;
	int useNamedKeys;
	int i;
	unsigned int cursor;

	if (!Linez_IsLoaded()) {
		return 0;
	}

	strcpy(basename, missionFileName);
	for (i = 0; basename[i] != '\0'; ++i) {
		if (basename[i] == '.') {
			basename[i] = '\0';
			break;
		}
	}

	firstChar = 0;
	secondChar = 0;
	separator = 0;
	battleIndex = 0;
	missionIndex = 0;
	sscanf(missionFileName, "%c%c%d%c%d", &firstChar, &secondChar, &battleIndex, &separator, &missionIndex);
	useNamedKeys = secondChar != 'b' && secondChar != 'B';

	if (mission != NULL) {
		if (mission->textTail.missionDescriptionText[0] != '\0') {
			char* destination;

			destination = mission->textTail.missionDescriptionText;
			strcpy(saved, destination);
			cursor = 0;
			while (1) {
				char ch;

				ch = saved[cursor];
				if (ch == '#' || ch == '\0') {
					break;
				}
				if (++cursor >= FRONTEND_MISSION_LOCALIZE_TEXT_SIZE) {
					break;
				}
			}

			if (cursor < FRONTEND_MISSION_LOCALIZE_TEXT_SIZE) {
				char separatorChar;

				separatorChar = saved[cursor];
				saved[cursor] = '\0';
				sprintf(key, "!%s_S00!", basename);
				strcat(key, saved);
				saved[cursor] = separatorChar;
				strncpy(destination, Linez_ResolveString(key), FRONTEND_MISSION_LOCALIZE_TEXT_SIZE);
				strcat(destination, "#");

				if (saved[cursor] != '\0') {
					int textIndex;

					++cursor;
					textIndex = 1;
					while (1) {
						int segmentIndex;
						char ch;

						while (cursor < FRONTEND_MISSION_LOCALIZE_TEXT_SIZE && saved[cursor] == '$') {
							++cursor;
						}

						segmentIndex = 0;
						while (cursor < FRONTEND_MISSION_LOCALIZE_TEXT_SIZE) {
							ch = saved[cursor];
							if (ch == '\0' || ch == '$') {
								break;
							}
							segment[segmentIndex++] = ch;
							++cursor;
						}
						segment[segmentIndex] = '\0';

						if (useNamedKeys) {
							sprintf(key, "!S_%s_%d!", basename, textIndex);
						} else {
							sprintf(key, "!S0%d0%d0%d!", (uint8_t)battleIndex, (uint8_t)missionIndex,
									textIndex);
						}
						strcat(key, segment);
						strcat(destination, Linez_ResolveString(key));
						ch = saved[cursor];
						if (ch == '\0' || cursor >= FRONTEND_MISSION_LOCALIZE_TEXT_SIZE) {
							break;
						}
						strcat(destination, "$$");
						++textIndex;
					}
				}
			}
		}

		if (mission->textTail.missionSuccessfulText[0] != '\0') {
			FrontendMission_LocalizeDelimitedText(mission->textTail.missionSuccessfulText, basename,
												  useNamedKeys, battleIndex, missionIndex, "!W_%s_%d!",
												  "!W0%d0%d0%d!");
		}

		if (mission->textTail.missionFailedText[0] != '\0') {
			char* destination;
			char original[FRONTEND_MISSION_LOCALIZE_TEXT_SIZE];
			char firstSection[FRONTEND_MISSION_LOCALIZE_TEXT_SIZE];

			destination = mission->textTail.missionFailedText;
			memset(saved, 0, sizeof(saved));
			strcpy(saved, destination);
			strcpy(original, saved);
			*destination = '\0';
			cursor = 0;
			while (cursor < FRONTEND_MISSION_LOCALIZE_TEXT_SIZE && saved[cursor] != '#') {
				++cursor;
			}

			if (cursor < FRONTEND_MISSION_LOCALIZE_TEXT_SIZE) {
				saved[cursor] = '\0';
			}

			strcpy(firstSection, saved);
			FrontendMission_LocalizeDelimitedText(firstSection, basename, useNamedKeys, battleIndex,
												  missionIndex, "!L_%s_%d!", "!L0%d0%d0%d!");
			strcat(destination, firstSection);
			strcat(destination, "#");
			if (cursor + 1 < FRONTEND_MISSION_LOCALIZE_TEXT_SIZE && original[cursor + 1] != '\0') {
				++cursor;
				while (cursor < FRONTEND_MISSION_LOCALIZE_TEXT_SIZE && original[cursor] == '$') {
					++cursor;
				}
				sprintf(key, "!%s_L00!", basename);
				strcat(key, &original[cursor]);
				strcat(destination, Linez_ResolveString(key));
			}
		}

		for (i = 0; i < 64; ++i) {
			if (mission->messages[i].message[0] != '\0') {
				if (useNamedKeys) {
					FrontendMission_BuildNumberedKey(key, "!R_%s_%d!", basename, i + 1,
													 mission->messages[i].message);
				} else {
					FrontendMission_BuildBattleKey(key, "!R0%d0%d%2d!", battleIndex, missionIndex, i + 1,
												   mission->messages[i].message);
				}
				FrontendMission_CopyLocalized(mission->messages[i].message,
											  sizeof(mission->messages[i].message), key);
			}
		}

		for (i = 0; i < 10; ++i) {
			int messageIndex;

			FrontendMission_BuildNumberedKey(key, "!%s_G%d!", basename, i + 1, mission->teams[i].name);
			FrontendMission_CopyLocalized(mission->teams[i].name, sizeof(mission->teams[i].name), key);
			for (messageIndex = 0; messageIndex < 6; ++messageIndex) {
				if (mission->teams[i].endOfMissionMessages[messageIndex][0] != '\0') {
					if (useNamedKeys) {
						FrontendMission_BuildNumberedKey(
							key, "!M_%s_%d!", basename, messageIndex + 1,
							mission->teams[i].endOfMissionMessages[messageIndex]);
					} else {
						FrontendMission_BuildBattleKey(key, "!M0%d0%d0%d!", battleIndex, missionIndex,
													   messageIndex + 1,
													   mission->teams[i].endOfMissionMessages[messageIndex]);
					}
					FrontendMission_CopyLocalized(
						mission->teams[i].endOfMissionMessages[messageIndex],
						sizeof(mission->teams[i].endOfMissionMessages[messageIndex]), key);
				}
			}
		}

		for (i = 0; i < (int16_t)mission->flightGroupCount; ++i) {
			int textIndex;

			for (textIndex = 0; textIndex < 24; ++textIndex) {
				char* text;

				text = mission->textTail.fgGoalStrings[i][textIndex / 3][textIndex % 3];
				if (text[0] != '\0') {
					sprintf(key, "!%s_O%d_%d!", basename, i + 1, textIndex + 1);
					strcat(key, text);
					FrontendMission_CopyLocalized(text, 64, key);
				}
			}
		}

		for (i = 0; i < 10; ++i) {
			int textIndex;

			for (textIndex = 0; textIndex < 84; ++textIndex) {
				char* text;

				text = mission->textTail.globalGoalStrings[i][textIndex / 3][textIndex % 3];
				if (text[0] != '\0') {
					sprintf(key, "!%s_C%d_%d!", basename, i + 1, textIndex + 1);
					strcat(key, text);
					FrontendMission_CopyLocalized(text, 64, key);
				}
			}
		}

		for (i = 0; i < 4; ++i) {
			if (mission->header.iffNames[i][0] != '\0') {
				FrontendMission_BuildNumberedKey(key, "!%s_N%d!", basename, i + 1,
												 mission->header.iffNames[i]);
				FrontendMission_CopyLocalized(mission->header.iffNames[i],
											  sizeof(mission->header.iffNames[i]), key);
			}
		}

		for (i = 0; i < 4; ++i) {
			if (mission->header.regions[i].name[0] != '\0') {
				FrontendMission_BuildNumberedKey(key, "!%s_I%d!", basename, i + 1,
												 mission->header.regions[i].name);
				FrontendMission_CopyLocalized(mission->header.regions[i].name,
											  sizeof(mission->header.regions[i].name), key);
			}
		}

		for (i = 0; i < 32; ++i) {
			if (mission->header.globalGroups[i].name[0] != '\0') {
				FrontendMission_BuildNumberedKey(key, "!%s_S%d!", basename, i + 1,
												 mission->header.globalGroups[i].name);
				FrontendMission_CopyLocalized(mission->header.globalGroups[i].name,
											  sizeof(mission->header.globalGroups[i].name), key);
			}
			if (mission->header.globalGroups[i].specialCargo[0] != '\0') {
				FrontendMission_BuildNumberedKey(key, "!%s_S%d!", basename, i + 1,
												 mission->header.globalGroups[i].specialCargo);
				FrontendMission_CopyLocalized(mission->header.globalGroups[i].specialCargo,
											  sizeof(mission->header.globalGroups[i].specialCargo), key);
			}
		}

		for (i = 0; i < 40; ++i) {
			if (mission->header.globalUnits[i].name[0] != '\0') {
				FrontendMission_BuildNumberedKey(key, "!%s_S%d!", basename, i + 1,
												 mission->header.globalUnits[i].name);
				FrontendMission_CopyLocalized(mission->header.globalUnits[i].name,
											  sizeof(mission->header.globalUnits[i].name), key);
			}
			if (mission->header.globalUnits[i].specialCargo[0] != '\0') {
				FrontendMission_BuildNumberedKey(key, "!%s_Q%d!", basename, i + 1,
												 mission->header.globalUnits[i].specialCargo);
				FrontendMission_CopyLocalized(mission->header.globalUnits[i].specialCargo,
											  sizeof(mission->header.globalUnits[i].specialCargo), key);
			}
		}

		for (i = 0; i < 16; ++i) {
			if (mission->header.globalCargos[i].name[0] != '\0') {
				FrontendMission_BuildNumberedKey(key, "!%s_X%d!", basename, i + 1,
												 mission->header.globalCargos[i].name);
				FrontendMission_CopyLocalized(mission->header.globalCargos[i].name,
											  sizeof(mission->header.globalCargos[i].name), key);
			}
		}

		for (i = 0; i < (int16_t)mission->flightGroupCount; ++i) {
			int orderIndex;

			for (orderIndex = 0; orderIndex < 16; ++orderIndex) {
				char* gate;
				char* text;

				gate = FrontendMission_OrderStringGate(mission, i, orderIndex);
				text = mission->textTail.orderStrings[i][orderIndex];
				if (gate[0] != '\0') {
					sprintf(key, "!%s_A%d_%d!", basename, i + 1, orderIndex + 1);
					strcat(key, text);
					FrontendMission_CopyLocalized(text, 64, key);
				}
			}
		}

		for (i = 0; i < (int16_t)mission->flightGroupCount; ++i) {
			XwaFlightGroup* flightGroup;

			flightGroup = &mission->flightGroups[i];
			if (flightGroup->name[0] != '\0') {
				sprintf(key, "!%s_F%d_1!", basename, i + 1);
				strcat(key, flightGroup->name);
				FrontendMission_CopyLocalized(flightGroup->name, sizeof(flightGroup->name), key);
			}
			if (flightGroup->cargo[0] != '\0') {
				sprintf(key, "!%s_F%d_3!", basename, i + 1);
				strcat(key, flightGroup->cargo);
				FrontendMission_CopyLocalized(flightGroup->cargo, sizeof(flightGroup->cargo), key);
			}
			if (flightGroup->specialCargo[0] != '\0') {
				sprintf(key, "!%s_F%d_4!", basename, i + 1);
				strcat(key, flightGroup->specialCargo);
				FrontendMission_CopyLocalized(flightGroup->specialCargo, sizeof(flightGroup->specialCargo),
											  key);
			}
			if (flightGroup->craftRole[0] != '\0') {
				sprintf(key, "!%s_F%d_5!", basename, i + 1);
				strcat(key, flightGroup->craftRole);
				FrontendMission_CopyLocalized(flightGroup->craftRole, sizeof(flightGroup->craftRole), key);
			}
			if (flightGroup->pilotID[0] != '\0') {
				sprintf(key, "!%s_F%d_6!", basename, i + 1);
				strcat(key, flightGroup->pilotID);
				FrontendMission_CopyLocalized(flightGroup->pilotID, sizeof(flightGroup->pilotID), key);
			}
		}
	}

	if (briefing != NULL) {
		for (i = 0; i < 128; ++i) {
			if (briefing->textBlocks[i][0] != '\0') {
				if (useNamedKeys) {
					FrontendMission_BuildNumberedKey(key, "!B_%s_%d!", basename, i + 1,
													 briefing->textBlocks[i]);
				} else {
					FrontendMission_BuildBattleKey(key, "!B0%d0%d%2d!", battleIndex, missionIndex, i + 1,
												   briefing->textBlocks[i]);
				}
				FrontendMission_CopyLocalized(briefing->textBlocks[i], 0x140u, key);
			}
		}

		for (i = 0; i < 128; ++i) {
			if (briefing->mapLabelTexts[i][0] != '\0') {
				sprintf(key, "!%s_T%d_%d!", basename, teamIndex + 1, i + 1);
				strcat(key, briefing->mapLabelTexts[i]);
				FrontendMission_CopyLocalized(briefing->mapLabelTexts[i], 0x28u, key);
			}
		}
	}

	return 1;
}

// FUNCTION: XWA 0x5667F0
int FrontendMission_LoadCurrentMissionData(void) {
	XwaFile* stream;
	unsigned int missionListIndex;
	int missionDescriptionId;
	int flightGroupIndex;
	int messageIndex;
	int teamIndex;
	int globalGoalIndex;
	int briefingIndex;
	int briefingTeamIndex;
	char buffer[256];
	unsigned char headerBuffer[FRONTEND_MISSION_HEADER_SIZE];
	FrontendBriefingScript scriptBuffer;
	FrontendBriefingMapIconState regionIcons[FRONTEND_BRIEFING_MAP_ICON_COUNT];
	uint16_t formatVersion;
	int16_t count;
	int16_t indexedRecord;
	int16_t textLength;
	unsigned char teamBriefingPresent;

	MissionSetup_LoadMissionList((MissionDirectoryId)g_pilotData.missionDirectoryId);
	if (g_missionList != NULL) {
		missionListIndex = 0;
		g_selectedMissionListIndex = 0;
		if (g_missionCount != 0) {
			missionDescriptionId = g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
			do {
				if (g_missionList[missionListIndex].missionIdx == missionDescriptionId) {
					break;
				}

				g_selectedMissionListIndex = (int)++missionListIndex;
			} while (missionListIndex < (unsigned int)g_missionCount);
		}
	} else {
		missionListIndex = (unsigned int)g_selectedMissionListIndex;
	}

	sprintf(buffer, "%s\\%s", g_campaignDirNames[g_pilotData.missionDirectoryId],
			g_missionList[missionListIndex].fileName);
	stream = File_Open(AERON_VFS_ROOT_ASSET, buffer, "rb");
	if (stream == NULL) {
		return 0;
	}

	if (g_frontendMission != NULL) {
		memset(g_frontendMission, 0, sizeof(*g_frontendMission));
	}

	File_ReadWord(stream, &formatVersion);
	if (g_frontendMission != NULL) {
		g_frontendMission->formatVersion = formatVersion;
	}

	if (!FrontendMission_IsSupportedFormat(formatVersion)) {
		return File_Close(stream);
	}

	File_ReadWord(stream, &count);
	File_ReadWord(stream, &indexedRecord);
	if (g_frontendMission != NULL) {
		g_frontendMission->flightGroupCount = (uint16_t)count;
		g_frontendMission->messageCount = (uint16_t)indexedRecord;
		File_ReadCount(stream, headerBuffer, sizeof(headerBuffer));
		memcpy(&g_frontendMission->header, headerBuffer, sizeof(g_frontendMission->header));

		for (flightGroupIndex = 0; flightGroupIndex < count; ++flightGroupIndex) {
			File_ReadCount(stream, &g_frontendMission->flightGroups[flightGroupIndex],
						   sizeof(XwaFlightGroup));
		}

		for (messageIndex = 0; messageIndex < indexedRecord; ++messageIndex) {
			File_ReadWord(stream, &count);
			File_ReadCount(stream, &g_frontendMission->messages[count], sizeof(XwaMessage));
		}

		for (teamIndex = 0; teamIndex < 10; ++teamIndex) {
			File_ReadWord(stream, &count);
			for (globalGoalIndex = 0; globalGoalIndex < count; ++globalGoalIndex) {
				File_ReadCount(stream, &g_frontendMission->globalGoals[teamIndex][globalGoalIndex],
							   sizeof(XwaGlobalGoal));
			}
		}

		for (teamIndex = 0; teamIndex < 10; ++teamIndex) {
			File_ReadWord(stream, &count);
			if (count != 0) {
				File_ReadCount(stream, &g_frontendMission->teams[teamIndex], sizeof(XwaTeam));
			}
		}
	} else {
		File_ReadCount(stream, headerBuffer, sizeof(headerBuffer));
		for (flightGroupIndex = 0; flightGroupIndex < count; ++flightGroupIndex) {
			File_Seek(stream, sizeof(XwaFlightGroup), SEEK_CUR);
		}

		for (messageIndex = 0; messageIndex < indexedRecord; ++messageIndex) {
			File_ReadWord(stream, &count);
			File_Seek(stream, sizeof(XwaMessage), SEEK_CUR);
		}

		for (teamIndex = 0; teamIndex < 10; ++teamIndex) {
			File_ReadWord(stream, &count);
			for (globalGoalIndex = 0; globalGoalIndex < count; ++globalGoalIndex) {
				File_Seek(stream, sizeof(XwaGlobalGoal), SEEK_CUR);
			}
		}

		for (teamIndex = 0; teamIndex < 10; ++teamIndex) {
			File_ReadWord(stream, &count);
			if (count != 0) {
				File_Seek(stream, sizeof(XwaTeam), SEEK_CUR);
			}
		}
	}

	briefingTeamIndex = g_briefingTeamIndex;
	for (briefingIndex = 0; briefingIndex < 2; ++briefingIndex) {
		int selected;
		int textIndex;

		selected = 0;
		memset(&scriptBuffer, 0, sizeof(scriptBuffer));
		if (headerBuffer[offsetof(XwaMissionHeaderBody, secondaryVersion)] ==
			FRONTEND_BRIEFING_SECONDARY_VERSION_98) {
			File_ReadCount(stream, &scriptBuffer, FRONTEND_BRIEFING_SCRIPT_FORMAT_SIZE);
		} else {
			File_ReadCount(stream, &scriptBuffer, FRONTEND_BRIEFING_SCRIPT_OLD_SIZE);
		}

		for (textIndex = 0; textIndex < FRONTEND_BRIEFING_MAP_ICON_COUNT; ++textIndex) {
			File_ReadCount(stream, &regionIcons[textIndex], sizeof(regionIcons[textIndex]));
		}

		for (teamIndex = 0; teamIndex < 10; ++teamIndex) {
			File_ReadByte(stream, &teamBriefingPresent);
			if (teamIndex == g_pilotData.team && teamBriefingPresent != 0) {
				memcpy(&g_frontendBriefingContent.script, &scriptBuffer,
					   sizeof(g_frontendBriefingContent.script));
				g_briefingTeamIndex = briefingIndex;
				briefingTeamIndex = briefingIndex;
				selected = 1;
				memcpy(g_briefingMapCurrentRegionIcons, regionIcons, sizeof(g_briefingMapCurrentRegionIcons));
			}
		}

		for (textIndex = 0; textIndex < FRONTEND_BRIEFING_MAP_LABEL_COUNT; ++textIndex) {
			char* text;

			text = g_frontendBriefingContent.mapLabelTexts[textIndex];
			if (selected) {
				memset(text, 0, FRONTEND_BRIEFING_MAP_LABEL_SIZE);
			}
			File_ReadWord(stream, &textLength);
			if (textLength != 0) {
				if (selected) {
					File_ReadCount(stream, text, (size_t)textLength);
				} else {
					File_Seek(stream, textLength, SEEK_CUR);
				}
			}
			if (selected) {
				text[textLength] = 0;
			}
		}

		for (textIndex = 0; textIndex < FRONTEND_BRIEFING_TEXT_BLOCK_COUNT; ++textIndex) {
			char* text;

			text = g_frontendBriefingContent.textBlocks[textIndex];
			if (selected) {
				memset(text, 0, FRONTEND_BRIEFING_TEXT_BLOCK_SIZE);
			}
			File_ReadWord(stream, &textLength);
			if (textLength != 0) {
				if (selected) {
					File_ReadCount(stream, text, (size_t)textLength);
				} else {
					File_Seek(stream, textLength, SEEK_CUR);
				}
			}
			if (selected) {
				text[textLength] = 0;
			}
		}
	}

	if (g_frontendMission != NULL &&
		(int16_t)g_frontendMission->formatVersion < FRONTEND_MISSION_FORMAT_V18) {
		FrontendMission_ConvertLegacyDelayValues(g_frontendMission);
	}

	File_Close(stream);
	FrontendMission_Localize(g_frontendMission, &g_frontendBriefingContent, briefingTeamIndex,
							 g_missionList[g_selectedMissionListIndex].fileName);
	return 1;
}

// FUNCTION: XWA 0x566F10
int FrontendMission_LoadFile(char* fileName) {
	XwaFile* stream;
	int flightGroupIndex;
	int messageIndex;
	int teamIndex;
	int globalGoalIndex;
	uint16_t indexedRecord;

	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
	if (stream == NULL) {
		Aeron_LogError("xwa.frontend", "Failed to open frontend mission '%s'", fileName);
		return 0;
	}

	memset(g_frontendMission, 0, sizeof(*g_frontendMission));
	File_ReadWord(stream, &g_frontendMission->formatVersion);
	if (!FrontendMission_IsSupportedFormat(g_frontendMission->formatVersion)) {
		Aeron_LogError("xwa.frontend", "Unsupported frontend mission format %u in '%s'",
				  (unsigned int)g_frontendMission->formatVersion, fileName);
		File_Close(stream);
		return 0;
	}

	File_ReadWord(stream, &g_frontendMission->flightGroupCount);
	File_ReadWord(stream, &g_frontendMission->messageCount);
	File_ReadCount(stream, &g_frontendMission->header, FRONTEND_MISSION_HEADER_SIZE);

	if ((int16_t)g_frontendMission->flightGroupCount > 0) {
		for (flightGroupIndex = 0; flightGroupIndex < (int16_t)g_frontendMission->flightGroupCount;
			 ++flightGroupIndex) {
			File_ReadCount(stream, &g_frontendMission->flightGroups[flightGroupIndex],
						   sizeof(XwaFlightGroup));
		}
	}

	if ((int16_t)g_frontendMission->messageCount > 0) {
		for (messageIndex = 0; messageIndex < (int16_t)g_frontendMission->messageCount; ++messageIndex) {
			File_ReadWord(stream, &indexedRecord);
			File_ReadCount(stream, &g_frontendMission->messages[(int16_t)indexedRecord], sizeof(XwaMessage));
		}
	}

	for (teamIndex = 0; teamIndex < 10; ++teamIndex) {
		File_ReadWord(stream, &indexedRecord);
		if ((int16_t)indexedRecord > 0) {
			for (globalGoalIndex = 0; globalGoalIndex < (int16_t)indexedRecord; ++globalGoalIndex) {
				File_ReadCount(stream, &g_frontendMission->globalGoals[teamIndex][globalGoalIndex],
							   sizeof(XwaGlobalGoal));
			}
		}
	}

	for (teamIndex = 0; teamIndex < 10; ++teamIndex) {
		File_ReadWord(stream, &indexedRecord);
		if (indexedRecord != 0) {
			File_ReadCount(stream, &g_frontendMission->teams[teamIndex], sizeof(XwaTeam));
		}
	}

	if ((int16_t)g_frontendMission->formatVersion < FRONTEND_MISSION_FORMAT_V18) {
		FrontendMission_ConvertLegacyDelayValues(g_frontendMission);
	}

	File_Close(stream);
	FrontendMission_Localize(g_frontendMission, NULL, 0, g_missionList[g_selectedMissionListIndex].fileName);
	return 1;
}

// FUNCTION: XWA 0x566D90
int FrontendMission_LoadCurrent(void) {
	unsigned int missionListIndex;
	MissionListEntry* missionList;
	int missionDirectoryId;

	MissionSetup_LoadMissionList((MissionDirectoryId)g_pilotData.missionDirectoryId);
	missionList = g_missionList;
	missionDirectoryId = g_pilotData.missionDirectoryId;
	if (missionList != NULL) {
		missionListIndex = 0;
		g_selectedMissionListIndex = 0;
		for (; missionListIndex < (unsigned int)g_missionCount; ++missionListIndex) {
			if (missionList[missionListIndex].missionIdx ==
				g_pilotData.missionDescriptionIds[missionDirectoryId]) {
				break;
			}

			g_selectedMissionListIndex = (int)(missionListIndex + 1);
		}
	} else {
#ifdef XWA_MODERN
		Aeron_LogError("xwa.frontend",
				  "Cannot load current frontend mission: mission list for directory %d is missing",
				  missionDirectoryId);
		return 0;
#else
		missionListIndex = (unsigned int)g_selectedMissionListIndex;
#endif
	}

	if (missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
		int teamIndex;
		int flightGroupIndex;
		int result;

		Skirmish_InitMissionDefaults();
		g_frontendMission->header.hangar = XWA_HANGAR_SKIRMISH;
		g_frontendMission->formatVersion = FRONTEND_MISSION_FORMAT_V18;
		g_frontendMission->header.secondaryVersion = 98;
		g_frontendMission->flightGroupCount = 16;

		for (teamIndex = 0; teamIndex < 8; ++teamIndex) {
			strcpy(g_frontendMission->teams[teamIndex].name,
				   FrontendString_Get((UIString)(FRONTEND_MISSION_SKIRMISH_TEAM_NAME_BASE + teamIndex)));
		}

		for (flightGroupIndex = 0; flightGroupIndex < 16; ++flightGroupIndex) {
			result = flightGroupIndex / (16 / g_gameConfig.numberOfTeams);
			g_frontendMission->flightGroups[flightGroupIndex].team = (uint8_t)result;
		}

		return result;
	}

#ifdef XWA_MODERN
	if (missionListIndex >= (unsigned int)g_missionCount) {
		Aeron_LogError("xwa.frontend",
				  "Cannot load current frontend mission: mission id %d not found in directory %d",
				  g_pilotData.missionDescriptionIds[missionDirectoryId], missionDirectoryId);
		return 0;
	}
#endif

	{
		char buffer[256];

		sprintf(buffer, "%s\\%s", g_campaignDirNames[missionDirectoryId],
				missionList[missionListIndex].fileName);
		return FrontendMission_LoadFile(buffer);
	}
}

// FUNCTION: XWA 0x57E8D0
int FrontendMission_InitPlayerState(void) {
	int rosterIndex;

	if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		memset(&g_mpRoster[1], 0, sizeof(g_mpRoster) - sizeof(g_mpRoster[0]));
	} else {
		for (rosterIndex = 0; rosterIndex < 8; ++rosterIndex) {
			if (g_mpRoster[rosterIndex].playerId != 0) {
				NetPlayerInfo* player;

				player = Net_FindPlayer(g_mpRoster[rosterIndex].playerId);
				if (player == NULL) {
					memset(&g_mpRoster[rosterIndex], 0, sizeof(g_mpRoster[rosterIndex]));
				}
			}
		}
	}

	g_pilotData.missionScore = 0;
	memset(g_pilotData.killsFullOnPlayer, 0, sizeof(g_pilotData.killsFullOnPlayer));
	memset(g_pilotData.killsSharedOnPlayer, 0, sizeof(g_pilotData.killsSharedOnPlayer));
	memset(g_pilotData.killsFullOnFlightGroup, 0, sizeof(g_pilotData.killsFullOnFlightGroup));
	memset(g_pilotData.killsSharedOnFlightGroup, 0, sizeof(g_pilotData.killsSharedOnFlightGroup));
	memset(g_pilotData.killsFullFromPlayer, 0, sizeof(g_pilotData.killsFullFromPlayer));
	memset(g_pilotData.killsSharedFromPlayer, 0, sizeof(g_pilotData.killsSharedFromPlayer));
	memset(g_pilotData.killsFullFromFlightGroup, 0, sizeof(g_pilotData.killsFullFromFlightGroup));
	memset(g_pilotData.killsSharedFromFlightGroup, 0, sizeof(g_pilotData.killsSharedFromFlightGroup));
	memset(&g_pilotData.objectStats, 0, sizeof(g_pilotData.objectStats));
	memset(g_pilotData.teamsStatistics, 0, sizeof(g_pilotData.teamsStatistics));
	g_localPilotNetworkPlayerIndex = 0;

	for (rosterIndex = 0; rosterIndex < 8; ++rosterIndex) {
		int slotIndex;
		int playerId;

		playerId = g_mpRoster[rosterIndex].playerId;
		if (playerId == 0) {
			continue;
		}

		for (slotIndex = 0; slotIndex < 16; ++slotIndex) {
			if (g_combatSimSlots[slotIndex].ownerPlayerId == playerId ||
				g_combatSimSlots[slotIndex].gunnerPlayerId == playerId) {
				break;
			}
		}

		if (slotIndex < 16) {
			memcpy(g_pilotData.networkPlayers[rosterIndex].friendlyName, g_mpRoster[rosterIndex].name, 13);
			g_pilotData.networkPlayers[rosterIndex].friendlyName[12] = '\0';
			g_pilotData.networkPlayers[rosterIndex].m44 = 1;
			g_pilotData.networkPlayers[rosterIndex].craftId = g_combatSimSlots[slotIndex].craftType;
			g_pilotData.networkPlayers[rosterIndex].warheadType = g_combatSimSlots[slotIndex].warhead;
			g_pilotData.networkPlayers[rosterIndex].beamType = g_combatSimSlots[slotIndex].beam;
			g_pilotData.networkPlayers[rosterIndex].counterMeasuresType =
				g_combatSimSlots[slotIndex].countermeasures;
			g_pilotData.networkPlayers[rosterIndex].craftsCount = g_combatSimSlots[slotIndex].numberOfCraft;

			if (g_gameConfig.goalType && g_pilotData.missionDirectoryId == MISSION_DIRECTORY_SKIRMISH) {
				if (g_shipList[g_shipTypeToShipListIndex[g_combatSimSlots[slotIndex].craftType]].flyable) {
					g_pilotData.networkPlayers[rosterIndex].wavesCount = 99;
				} else {
					g_pilotData.networkPlayers[rosterIndex].wavesCount =
						g_combatSimSlots[slotIndex].numberOfWaves;
				}
			} else {
				g_pilotData.networkPlayers[rosterIndex].wavesCount =
					g_combatSimSlots[slotIndex].numberOfWaves;
			}

			g_pilotData.networkPlayers[rosterIndex].directPlayId = playerId;
			g_pilotData.networkPlayers[rosterIndex].rating = g_mpRoster[rosterIndex].rating;
			g_pilotData.networkPlayers[rosterIndex].totalScore = 0;
			g_pilotData.networkPlayers[rosterIndex].kills = 0;
			g_pilotData.networkPlayers[rosterIndex].killsShared = 0;
			g_pilotData.networkPlayers[rosterIndex].m38 = 0;
			g_pilotData.networkPlayers[rosterIndex].killsAssist = 0;
			g_pilotData.networkPlayers[rosterIndex].totalLosses = 0;
			g_pilotData.networkPlayers[rosterIndex].m60 = 0;
			g_pilotData.networkPlayers[rosterIndex].flightGroupId =
				(int16_t)g_combatSimSlots[slotIndex].fgIndex;
			g_pilotData.networkPlayers[rosterIndex].m20 = 0;
			if (g_combatSimSlots[slotIndex].gunnerPlayerId ==
				g_pilotData.networkPlayers[rosterIndex].directPlayId) {
				g_pilotData.networkPlayers[rosterIndex].m20 = 1;
			}
			if (g_mpRoster[rosterIndex].playerId == Net_GetLocalPlayerId()) {
				g_localPilotNetworkPlayerIndex = rosterIndex;
			}
		}
	}

	{
		int flightGroupIndex;
		int startRegion;

		g_pilotData.regionsCount = 0;
		for (flightGroupIndex = 0; flightGroupIndex < (int16_t)g_frontendMission->flightGroupCount;
			 ++flightGroupIndex) {
			startRegion =
				g_frontendMission->flightGroups[flightGroupIndex].missionPointRegions[XWA_FG_POINT_START_1];
			if (startRegion > g_pilotData.regionsCount) {
				g_pilotData.regionsCount = startRegion;
			}
		}
		g_pilotData.regionsCount += 2;
	}

	if (!g_frontendMission->header.hangar) {
		if (g_frontendMissionSessionMode == FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
			g_pilotData.meleeMissionIndex = -1;
		} else {
			g_pilotData.meleeMissionIndex =
				g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] - 58;
		}
	} else {
		g_pilotData.meleeMissionIndex = 0;
	}

	if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
		if ((unsigned int)g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId] < 7u) {
			g_pilotData.hangarType = 1;
		} else {
			g_pilotData.hangarType = 0;
		}
	} else {
		g_pilotData.hangarType = 0;
	}

	if (g_pilotData.campaignMode) {
		if (g_frontendMission->header.hangar == XWA_HANGAR_FAMILYTRANSPORT) {
			g_pilotData.currentFactionId = 3;
		} else {
			g_pilotData.currentFactionId = 0;
		}
	} else {
		g_pilotData.currentFactionId = 2;
	}

	if (g_frontendMissionSessionMode != FRONTEND_MISSION_SESSION_SINGLEPLAYER) {
		g_pilotData.factionStatistics[2].team = g_pilotData.team;
		g_pilotData.factionStatistics[2].missionDirectoryId = g_pilotData.missionDirectoryId;
		memcpy(g_pilotData.factionStatistics[2].missionDescriptionIds, g_pilotData.missionDescriptionIds,
			   sizeof(g_pilotData.factionStatistics[2].missionDescriptionIds));
		g_pilotData.factionStatistics[2].m0048 = g_pilotData.unk2;
		g_pilotData.factionStatistics[0].m0048 = 0;
		g_pilotData.factionStatistics[1].m0048 = 0;
		g_pilotData.factionStatistics[3].m0048 = 0;
		return NetSession_CompactReliablePeerSlotsForRoster();
	}

	if (g_frontendMission->header.hangar) {
		g_pilotData.factionStatistics[0].team = g_pilotData.team;
		g_pilotData.factionStatistics[0].missionDirectoryId = g_pilotData.missionDirectoryId;
		g_pilotData.factionStatistics[0].missionDescriptionIds[g_pilotData.missionDirectoryId] =
			g_pilotData.missionDescriptionIds[g_pilotData.missionDirectoryId];
		g_pilotData.factionStatistics[0].m0048 = 0;
		g_pilotData.factionStatistics[1].m0048 = 0;
		g_pilotData.factionStatistics[3].m0048 = 0;
	}

	return NetSession_CompactReliablePeerSlotsForRoster();
}
