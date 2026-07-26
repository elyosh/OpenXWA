#include "xwa/assets/string_table.h"

#include "xwa/assets/file_io.h"
#include "xwa/assets/linez.h"
#include "xwa/assets/model_def.h"

#include "aeron/log.h"

#include <stdio.h>
#include <string.h>

#define STRING_DATA_TAG "STRINGDATA"

// GLOBAL: XWA 0x5B4AB0
const unsigned char g_goalConditionTextVariantCount[47] = {
	1, 14, 14, 14, 14, 14, 14, 14, 14, 1, 1, 1, 14, 1, 1, 1, 1, 1, 1, 14, 1,  1,  1,  1,
	1, 1,  1,  1,  1,  1,  1,  1,  1,  1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 14, 14, 14, 14,
};

// GLOBAL: XWA 0x808112
MemoryHandle g_stringDataHandle = 0;
int g_gameStringCount = 0;
int g_stringDataBufferSize = 0;
int g_stringDataBufferUsed = 0;
// GLOBAL: XWA 0xABC726
int* g_uiStringOffsets = NULL;
// GLOBAL: XWA 0xABC72A
char* g_uiStringData = NULL;
// GLOBAL: XWA 0xABC72E
int g_uiStringCount = 0;
// GLOBAL: XWA 0xABC732
int g_uiStringCapacity = 0;

// GLOBAL: XWA 0x9E9680
char* g_strDamageSystemNames[12];
// GLOBAL: XWA 0x91B240
char* g_strFileErrorMessages[5];
char* g_strDiskIoMessages[43];
// GLOBAL: XWA 0x9CF628
char* g_strPressSpaceBar;
// GLOBAL: XWA 0x9CC920
char* g_strGoalCondMasculine[XWA_GOAL_CONDITION_TEXT_COUNT];
// GLOBAL: XWA 0x9C70A0
char* g_strGoalCondFeminine[XWA_GOAL_CONDITION_TEXT_COUNT];
// GLOBAL: XWA 0x9C9D00
char* g_strGoalCondNeutered[XWA_GOAL_CONDITION_TEXT_COUNT];
// GLOBAL: XWA 0x9CF540
char* g_strPercentages[15];
// GLOBAL: XWA 0x9C9CB0
char* g_strOperators[3];
// GLOBAL: XWA 0x9C9CC0
char* g_strGoalTitles[10];
// GLOBAL: XWA 0x9CF600
char* g_strConjunctions[9];
// GLOBAL: XWA 0x9CF720
char* g_strSides[4];
// GLOBAL: XWA 0x9CF580
char* g_strShipFamily[8];
// GLOBAL: XWA 0x9CF5A0
char* g_strShipGenus[17];
// GLOBAL: XWA 0x9AFE80
char* g_strMapStrings[17];
// GLOBAL: XWA 0x9B6400
char* g_strInFlightMessages[538];
// GLOBAL: XWA 0x91B2A0
char* g_strPanelStrings[PANEL_STRING_COUNT];
// GLOBAL: XWA 0x91AF60
char* g_strFilmCommands[11];
// GLOBAL: XWA 0x91B120
char* g_strFilmOptions[15];
// GLOBAL: XWA 0x91B0A0
char* g_strWaypointStrings[15];
// GLOBAL: XWA 0x91B160
char* g_strComponentStrings[34];
// GLOBAL: XWA 0x91B320
char* g_strOverlayStrings[42];
// GLOBAL: XWA 0x91AF00
char* g_strThreatStrings[5];
// GLOBAL: XWA 0x91B200
char* g_strStatusStrings[10];
// GLOBAL: XWA 0x91B260
char* g_strWarheadNames[16];
// GLOBAL: XWA 0x91AF14
char* g_strWarheadUnknown;
// GLOBAL: XWA 0x9EA994
char* g_strWarheadSurvivors;
// GLOBAL: XWA 0x91AF20
char* g_strBuoyNames[14];
char* g_strSpeciesNamesLong[222];
// GLOBAL: XWA 0x91B4C0
char* g_strSpeciesNamesPlural[225];
char* g_strDefaultModelName;
char* g_strSpeciesNames[222];
// GLOBAL: XWA 0x91AFA0
char* g_strWingmanCommands[11];
// GLOBAL: XWA 0x9AF020
char* g_strFlightCmdMainMenu[10];
// GLOBAL: XWA 0x9AF160
char* g_strFlightCmdSubMenu[5];
// GLOBAL: XWA 0x9AF140
char* g_strFlightCmdMenuItems[7];
// GLOBAL: XWA 0x9AEFA0
char* g_strFlightCmdSubMenuItems[27];
// GLOBAL: XWA 0x9EA9A0
char* g_strMfdStrings[MFD_STRING_COUNT];
char* g_strConsoleStrings[2];
// GLOBAL: XWA 0x9C6E60
char* g_strProvingGroundDescs[64];
// GLOBAL: XWA 0x9C6F80
char* g_strHangarMenuTitles[HANGAR_MENU_TITLE_COUNT];
// GLOBAL: XWA 0x9C6FC0
char* g_strHangarMenuItems[50];
// GLOBAL: XWA 0x9C6D60
char* g_strHangarMiscStrings[HANGAR_MISC_COUNT];
// GLOBAL: XWA 0x91B0E0
char* g_strWarheadNamesPlural[16];
char* g_strYardStrings[19];
// GLOBAL: XWA 0x9E94E0
char* g_strDiStrings[12];
// GLOBAL: XWA 0x9CF640
unsigned char g_craftGender[XWA_CRAFT_GENDER_COUNT];

static char* g_stringDataBuffer;
static int g_stringLoadFailed;

#ifdef XWA_MODERN
static void FrontendString_StripLineFeed(char* text) {
	size_t length;

	length = strlen(text);
	if (length > 0 && text[length - 1] == '\n') {
		text[--length] = '\0';
	}

	if (length > 0 && text[length - 1] == '\r') {
		text[length - 1] = '\0';
	}
}
#endif

static int StringTable_GetModelDefIndexForSpeciesRecord(int index) {
	if (index <= 217) {
		return index;
	}

	if (index == 218) {
		return 223;
	}

	if (index == 219) {
		return 224;
	}

	if (index == 220) {
		return 234;
	}

	return -1;
}

static int StringTable_ReadStringRecord(XwaFile* stream, char* buffer, size_t bufferSize) {
	size_t length;

	while (File_ReadLine(stream, buffer, bufferSize)) {
		if (buffer[0] == '/' && buffer[1] == '/') {
			continue;
		}

		length = strlen(buffer);
		while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')) {
			buffer[--length] = '\0';
		}

		++g_gameStringCount;
		return 1;
	}

	return 0;
}

static char* StringTable_CopyToStringData(const char* text) {
	size_t length = strlen(text) + 1;
	char* destination;

	if (g_stringDataBufferUsed + (int)length > g_stringDataBufferSize) {
		g_stringLoadFailed = 1;
		return NULL;
	}

	destination = g_stringDataBuffer + g_stringDataBufferUsed;
	memcpy(destination, text, length);
	g_stringDataBufferUsed += (int)length;
	return destination;
}

static char* StringTable_ReadResolvedString(XwaFile* stream) {
	char line[1024];
	char* stored;

	if (!StringTable_ReadStringRecord(stream, line, sizeof(line))) {
		g_stringLoadFailed = 1;
		return NULL;
	}

	stored = StringTable_CopyToStringData(line);
	if (stored == NULL) {
		return NULL;
	}

	return Linez_ResolveString(stored);
}

static __inline int StringTable_StoreTable(XwaFile* stream, char** table, size_t count) {
	size_t i;

	for (i = 0; i < count; ++i) {
		table[i] = StringTable_ReadResolvedString(stream);
		if (table[i] == NULL) {
			return 0;
		}
	}

	return 1;
}

static int StringTable_StoreGoalConditionTable(XwaFile* stream, char** table) {
	int row;

	for (row = 0; row < XWA_GOAL_CONDITION_COUNT; ++row) {
		int variantCount = g_goalConditionTextVariantCount[row % 47];
		int column;

		for (column = 0; column < variantCount; ++column) {
			table[row * XWA_GOAL_CONDITION_TEXT_SLOTS + column] = StringTable_ReadResolvedString(stream);
			if (table[row * XWA_GOAL_CONDITION_TEXT_SLOTS + column] == NULL) {
				return 0;
			}
		}
	}

	return 1;
}

static void StringTable_DecodeInFlightMessage(char* text) {
	char* read = text;
	char* write = text;

	while (*read != '\0') {
		if (*read == '\\' && read[1] != '\0' && read[2] != '\0') {
			if (read[1] == '0') {
				*write++ = (char)(read[2] - '0');
			} else {
				*write++ = (char)(read[2] - 40);
			}

			read += 3;
		} else {
			*write++ = *read++;
		}
	}

	*write = '\0';
}

static int StringTable_StoreInFlightMessages(XwaFile* stream) {
	size_t i;

	for (i = 0; i < 538; ++i) {
		char line[1024];
		char* resolved;
		char* stored;

		if (!StringTable_ReadStringRecord(stream, line, sizeof(line))) {
			g_stringLoadFailed = 1;
			return 0;
		}

		resolved = Linez_ResolveString(line);
		StringTable_DecodeInFlightMessage(resolved);
		stored = StringTable_CopyToStringData(resolved);
		if (stored == NULL) {
			return 0;
		}

		g_strInFlightMessages[i] = stored;
	}

	return 1;
}

static int StringTable_StoreWarheadNames(XwaFile* stream) {
	int i;

	for (i = 0; i <= 15; ++i) {
		char* resolved = StringTable_ReadResolvedString(stream);
		if (resolved == NULL) {
			return 0;
		}

		if (i == 13) {
			g_strWarheadUnknown = resolved;
		} else if (i == 14) {
			g_strWarheadSurvivors = resolved;
		} else {
			g_strWarheadNames[i] = resolved;
		}
	}

	return 1;
}

static int StringTable_StoreWarheadNamesPlural(XwaFile* stream) {
	int i;

	for (i = 0; i <= 15; ++i) {
		char* resolved = StringTable_ReadResolvedString(stream);
		if (resolved == NULL) {
			return 0;
		}

		if (i != 13 && i != 14) {
			g_strWarheadNamesPlural[i] = resolved;
		}
	}

	return 1;
}

static int StringTable_StoreSpeciesLongNames(XwaFile* stream) {
	int i;

	for (i = 0; i <= 221; ++i) {
		char* resolved = StringTable_ReadResolvedString(stream);
		if (resolved == NULL) {
			return 0;
		}

		if (i < 221) {
			int modelDefIndex = StringTable_GetModelDefIndexForSpeciesRecord(i);

			switch (resolved[0]) {
				case 'm':
					g_craftGender[i] = 0;
					break;
				case 'f':
					g_craftGender[i] = 1;
					break;
				case 'n':
					g_craftGender[i] = 2;
					break;
				default:
					g_stringLoadFailed = 1;
					return 0;
			}

			g_strSpeciesNamesLong[i] = resolved + 2;
			if (modelDefIndex >= 0) {
				g_modelDefs[modelDefIndex].nameAlt = resolved + 2;
			}
		}
	}

	return 1;
}

static int StringTable_StoreSpeciesPluralNames(XwaFile* stream) {
	int i;

	for (i = 0; i <= 221; ++i) {
		char* resolved = StringTable_ReadResolvedString(stream);
		if (resolved == NULL) {
			return 0;
		}

		if (i < 221) {
			g_strSpeciesNamesPlural[i] = resolved;
		}
	}

	return 1;
}

static int StringTable_StoreDefaultModelName(XwaFile* stream) {
	int i;

	g_strDefaultModelName = StringTable_ReadResolvedString(stream);
	if (g_strDefaultModelName == NULL) {
		return 0;
	}

	for (i = 0; i < XWA_MODEL_DEF_COUNT; ++i) {
		g_modelDefs[i].nameLong = g_strDefaultModelName;
	}

	return 1;
}

static int StringTable_StoreSpeciesNames(XwaFile* stream) {
	int i;

	for (i = 0; i <= 221; ++i) {
		char* resolved = StringTable_ReadResolvedString(stream);
		if (resolved == NULL) {
			return 0;
		}

		if (i < 221) {
			int modelDefIndex = StringTable_GetModelDefIndexForSpeciesRecord(i);

			g_strSpeciesNames[i] = resolved;
			if (modelDefIndex >= 0) {
				g_modelDefs[modelDefIndex].nameLong = resolved;
			}
		}
	}

	return 1;
}

static int StringTable_HasCraftGender(char gender) {
	int i;

	for (i = 0; i < 221; ++i) {
		if (g_craftGender[i] == gender) {
			return 1;
		}
	}

	return 0;
}

static int StringTable_AllocStringData(XwaFile* stream) {
	int fileSize = File_GetSize(stream);

	if (fileSize > 0x7d00) {
		Memory_FreeHandle(STRING_DATA_TAG, g_stringDataHandle);
		g_stringDataHandle = Memory_AllocHandle(STRING_DATA_TAG, (size_t)fileSize + 10000);
		if (g_stringDataHandle == 0) {
			return 0;
		}
	}

	g_stringDataBuffer = (char*)Memory_LockHandle(g_stringDataHandle);
	if (g_stringDataBuffer == NULL) {
		return 0;
	}

	g_stringDataBufferSize = Memory_GetHandleSize(g_stringDataHandle);
	g_stringDataBufferUsed = 0;
	return 1;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x4E4F00
int StringTable_LoadGameStrings(void) {
	XwaFile* stream;

	g_gameStringCount = 0;
	g_stringLoadFailed = 0;
	memset(g_craftGender, 0, sizeof(g_craftGender));

	/* Original opens strings.txt from the game directory; asset-root mode makes that explicit. */
	stream = File_Open(AERON_VFS_ROOT_ASSET, "ALLIANCE/STRINGS.TXT", "r");
	if (stream == NULL) {
		return 2;
	}

	if (!StringTable_AllocStringData(stream)) {
		File_Close(stream);
		return 2;
	}

	File_Seek(stream, 0, SEEK_SET);

	if (!StringTable_StoreTable(stream, g_strDamageSystemNames, 12) ||
		!StringTable_StoreTable(stream, g_strFileErrorMessages, 5) ||
		!StringTable_StoreTable(stream, g_strDiskIoMessages, 43)) {
		File_Close(stream);
		return 2;
	}

	g_strPressSpaceBar = StringTable_ReadResolvedString(stream);
	if (g_strPressSpaceBar == NULL || !StringTable_StoreGoalConditionTable(stream, g_strGoalCondMasculine) ||
		!StringTable_StoreTable(stream, g_strPercentages, 15) ||
		!StringTable_StoreTable(stream, g_strOperators, 3) ||
		!StringTable_StoreTable(stream, g_strGoalTitles, 10) ||
		!StringTable_StoreTable(stream, g_strConjunctions, 9) ||
		!StringTable_StoreTable(stream, g_strSides, 4) ||
		!StringTable_StoreTable(stream, g_strShipFamily, 8) ||
		!StringTable_StoreTable(stream, g_strShipGenus, 17) ||
		!StringTable_StoreTable(stream, g_strMapStrings, 17) || !StringTable_StoreInFlightMessages(stream) ||
		!StringTable_StoreTable(stream, g_strPanelStrings, PANEL_STRING_COUNT) ||
		!StringTable_StoreTable(stream, g_strFilmCommands, 11) ||
		!StringTable_StoreTable(stream, g_strFilmOptions, 15) ||
		!StringTable_StoreTable(stream, g_strWaypointStrings, 15) ||
		!StringTable_StoreTable(stream, g_strComponentStrings, 34) ||
		!StringTable_StoreTable(stream, g_strOverlayStrings, 42) ||
		!StringTable_StoreTable(stream, g_strThreatStrings, 5) ||
		!StringTable_StoreTable(stream, g_strStatusStrings, 10) || !StringTable_StoreWarheadNames(stream) ||
		!StringTable_StoreTable(stream, g_strBuoyNames, 14) || !StringTable_StoreSpeciesLongNames(stream) ||
		!StringTable_StoreSpeciesPluralNames(stream) || !StringTable_StoreDefaultModelName(stream) ||
		!StringTable_StoreSpeciesNames(stream) || !StringTable_StoreTable(stream, g_strWingmanCommands, 11) ||
		!StringTable_StoreTable(stream, g_strFlightCmdMainMenu, 10) ||
		!StringTable_StoreTable(stream, g_strFlightCmdSubMenu, 5) ||
		!StringTable_StoreTable(stream, g_strFlightCmdMenuItems, 7) ||
		!StringTable_StoreTable(stream, g_strFlightCmdSubMenuItems, 27) ||
		!StringTable_StoreTable(stream, g_strMfdStrings, MFD_STRING_COUNT) ||
		!StringTable_StoreTable(stream, g_strConsoleStrings, 2) ||
		!StringTable_StoreTable(stream, g_strProvingGroundDescs, 64) ||
		!StringTable_StoreTable(stream, g_strHangarMenuTitles, HANGAR_MENU_TITLE_COUNT) ||
		!StringTable_StoreTable(stream, g_strHangarMenuItems, 50) ||
		!StringTable_StoreTable(stream, g_strHangarMiscStrings, HANGAR_MISC_COUNT) ||
		!StringTable_StoreWarheadNamesPlural(stream) ||
		!StringTable_StoreTable(stream, g_strYardStrings, 19) ||
		!StringTable_StoreTable(stream, g_strDiStrings, 12)) {
		File_Close(stream);
		return 2;
	}

	if (StringTable_HasCraftGender(1) &&
		!StringTable_StoreGoalConditionTable(stream, g_strGoalCondFeminine)) {
		File_Close(stream);
		return 2;
	}

	if (StringTable_HasCraftGender(2) &&
		!StringTable_StoreGoalConditionTable(stream, g_strGoalCondNeutered)) {
		File_Close(stream);
		return 2;
	}

	if (g_stringLoadFailed) {
		File_Close(stream);
		return 2;
	}

	return File_Close(stream);
}

// FUNCTION: XWA 0x55C8A0
int FrontendString_LoadTable(char* fileName) {
	XwaFile* stream;
	size_t stringDataSize;
	int fileSize;
	int* offsets;
	char* writePtr;
	char line[1024];
	char resolvedLine[1024];

	stringDataSize = 0;
	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "r");
	if (stream != NULL) {
		FrontendString_UnloadTable();
		if (g_uiStringOffsets != NULL) {
			Mem_Free(g_uiStringOffsets);
			g_uiStringOffsets = NULL;
		}

		g_uiStringOffsets = (int*)Mem_Alloc(0x100u);
		if (g_uiStringOffsets == NULL) {
#ifdef XWA_MODERN
			Aeron_Log("xwa.assets", "Failed to allocate frontend string offsets for '%s'", fileName);
#endif
			return File_Close(stream);
		}

		g_uiStringCapacity = 64;
		fileSize = File_GetSize(stream);
		if (g_uiStringData != NULL) {
			Mem_Free(g_uiStringData);
			g_uiStringData = NULL;
		}

		g_uiStringData = (char*)Mem_Alloc((size_t)(4 * fileSize));
		if (g_uiStringData == NULL) {
#ifdef XWA_MODERN
			Aeron_Log("xwa.assets", "Failed to allocate frontend string table '%s' (%d bytes)", fileName,
					  4 * fileSize);
#endif
			FrontendString_UnloadTable();
			return File_Close(stream);
		}

		offsets = g_uiStringOffsets;
		writePtr = g_uiStringData;
#ifdef XWA_MODERN
		while (File_ReadLine(stream, line, sizeof(line))) {
#else
		while (fgets(line, sizeof(line), stream) != NULL) {
#endif
			int length;

			line[sizeof(line) - 1] = '\0';
			if (line[0] == '/' && line[1] == '/') {
				continue;
			}

			strcpy(resolvedLine, Linez_ResolveString(line));
#ifdef XWA_MODERN
			FrontendString_StripLineFeed(resolvedLine);
			length = (int)strlen(resolvedLine);
#else
			length = (int)strlen(resolvedLine);
			if (resolvedLine[length - 1] == '\n') {
				resolvedLine[length - 1] = '\0';
				--length;
			}
#endif

			offsets[g_uiStringCount] = (int)(writePtr - g_uiStringData);
			memcpy(writePtr, resolvedLine, (size_t)length + 1);
			writePtr += length + 1;
			stringDataSize += length + 1;
			++g_uiStringCount;

			if (g_uiStringCount == g_uiStringCapacity) {
				int* resizedOffsets;

				resizedOffsets =
					(int*)Mem_Realloc(g_uiStringOffsets, (size_t)(4 * g_uiStringCapacity + 0x100));
				if (resizedOffsets == NULL) {
#ifdef XWA_MODERN
					Aeron_Log("xwa.assets", "Failed to grow frontend string offsets for '%s'", fileName);
#endif
					break;
				}

				g_uiStringOffsets = resizedOffsets;
				offsets = resizedOffsets;
				g_uiStringCapacity += 64;
			}
		}

		{
			void* resizedData;

			resizedData = Mem_Realloc(g_uiStringData, stringDataSize);
			if (resizedData != NULL) {
				g_uiStringData = (char*)resizedData;
			} else {
#ifdef XWA_MODERN
				Aeron_Log("xwa.assets", "Failed to trim frontend string table '%s' to %zu bytes", fileName,
						  stringDataSize);
#endif
				Mem_Free(g_uiStringData);
				Mem_Free(g_uiStringOffsets);
				g_uiStringCount = 0;
				g_uiStringData = NULL;
				g_uiStringOffsets = NULL;
			}
		}

		return File_Close(stream);
	}

#ifdef XWA_MODERN
	Aeron_Log("xwa.assets", "Failed to open frontend string table '%s'", fileName);
#endif
	return 0;
}

// FUNCTION: XWA 0x55CB00
void FrontendString_UnloadTable(void) {
	if (g_uiStringOffsets != NULL) {
		Mem_Free(g_uiStringOffsets);
		g_uiStringOffsets = NULL;
	}

	if (g_uiStringData != NULL) {
		Mem_Free(g_uiStringData);
		g_uiStringData = NULL;
	}

	g_uiStringCount = 0;
	g_uiStringCapacity = 0;
}

// FUNCTION: XWA 0x55CB50
const char* FrontendString_Get(UIString index) {
	if (index >= (unsigned int)g_uiStringCount) {
		return "No text.";
	}

	return g_uiStringData + g_uiStringOffsets[index];
}
