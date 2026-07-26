#include "xwa/flight/ai/ai_internal.h"

#ifndef XWA_MODERN
#include "xwa/flight/fediskio.h"

#include <stdio.h>
#endif

typedef struct PaiTokenInfo {
	char name[80];
	uint16_t id;
} PaiTokenInfo;

// GLOBAL: XWA 0x7D5260
uint8_t g_planOrderData[0xffff];

// GLOBAL: XWA 0x9109E0
uint8_t* g_planDataPtrs[256];

// GLOBAL: XWA 0x7FFDA0
PlanRecord g_planTable[256];

// GLOBAL: XWA 0x7CAB48
int g_planCount;

// GLOBAL: XWA 0x7CA240
uint8_t g_builtinPlanIdByNameIndex[PAI_BUILTIN_PLAN_ID_CACHE_COUNT];

// GLOBAL: XWA 0x5B0EE0
const uint8_t g_orderLeaderBuiltinPlanNameIndex[PAI_ORDER_PLAN_NAME_INDEX_COUNT] = {
	0x01, 0x2f, 0x03, 0x05, 0x27, 0x2a, 0x2b, 0x45, 0x08, 0x09, 0x14, 0x13, 0x1c, 0x1d, 0x1e,
	0x1f, 0x20, 0x21, 0x25, 0x42, 0x42, 0x38, 0x3a, 0x3b, 0x3c, 0x3c, 0x3e, 0x3f, 0x01, 0x35,
	0x01, 0x22, 0x44, 0x01, 0x01, 0x01, 0x43, 0x46, 0x4b, 0x4d, 0x4f, 0x01, 0x45, 0x52, 0x2a,
	0x55, 0x50, 0x4c, 0x01, 0x53, 0x49, 0x01, 0x4a, 0x57, 0x01, 0x01, 0x01, 0x6e, 0x01, 0x51,
	0x01, 0x70, 0x01, 0x74, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// GLOBAL: XWA 0x5B0F28
const uint8_t g_orderFollowerBuiltinPlanNameIndex[PAI_ORDER_PLAN_NAME_INDEX_COUNT] = {
	0x02, 0x30, 0x04, 0x06, 0x29, 0x2a, 0x2b, 0x0e, 0x0e, 0x0e, 0x18, 0x0e, 0x1c, 0x1d, 0x1e,
	0x1f, 0x20, 0x21, 0x04, 0x42, 0x42, 0x39, 0x3a, 0x3b, 0x39, 0x39, 0x39, 0x39, 0x02, 0x36,
	0x02, 0x22, 0x44, 0x01, 0x01, 0x01, 0x43, 0x46, 0x4b, 0x4d, 0x4f, 0x01, 0x0e, 0x52, 0x2a,
	0x55, 0x50, 0x4c, 0x01, 0x53, 0x49, 0x01, 0x4a, 0x57, 0x01, 0x01, 0x01, 0x6e, 0x01, 0x51,
	0x01, 0x70, 0x01, 0x74, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// GLOBAL: XWA 0x5FFBE8
const uint16_t g_planReportMessageIdByPlanId[PAI_BUILTIN_PLAN_COUNT] = {
	0x00b7, 0x00b7, 0x00b7, 0x00cf, 0x00ca, 0x00d2, 0x00cf, 0x00ca, 0x00cf, 0x00ca, 0x00cf, 0x00ca,
	0x00ba, 0x00bc, 0x00be, 0x00bb, 0x00ba, 0x00bd, 0x00bd, 0x00bf, 0x00bc, 0x00be, 0x00bd, 0x00bd,
	0x00c0, 0x00c1, 0x00bc, 0x00be, 0x00bd, 0x00c1, 0x00bc, 0x00be, 0x00bd, 0x00c2, 0x00c5, 0x00c2,
	0x00c2, 0x00c3, 0x00c2, 0x00c4, 0x00c2, 0x00c6, 0x00d5, 0x00d5, 0x00c6, 0x00c7, 0x00c6, 0x00c8,
	0x00c9, 0x00d3, 0x00cd, 0x00cb, 0x00cd, 0x00cc, 0x00ce, 0x00cd, 0x00b7, 0x00ca, 0x00cf, 0x00cf,
	0x00d0, 0x00d1, 0x00b9, 0x00b9, 0x00cf, 0x00cf, 0x00d3, 0x00d4, 0x00d6, 0x00c2, 0x00ba, 0x00d7,
	0x00d7, 0x00d7, 0x00cd, 0x00d9, 0x00d8, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df, 0x00e0,
	0x00e1, 0x00e2, 0x00e3, 0x00e5, 0x00e6, 0x00e7, 0x00e8, 0x00e9, 0x00f9, 0x00eb, 0x00ea, 0x00ec,
	0x00ed, 0x00ee, 0x00ef, 0x00f0, 0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7, 0x00f8,
	0x00fa, 0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00ff, 0x0100, 0x0101, 0x0002, 0x0003, 0x0005
};

// GLOBAL: XWA 0x5B7A28
static const PaiTokenInfo g_planTargetNameTable[] = {
	{ "LOCATARGET", 249 },    { "LOCBTARGET", 250 },
	{ "ABORTTARGET", 251 },   { "NORMALTARGET", 252 },
	{ "PRIMARYTARGET", 253 }, { "HOMETARGET", 254 },
	{ "NULLTARGET", 255 },    { "NOTARGET", 0xffff },
	{ "0x80", 128 },          { "", 0 },
};

// GLOBAL: XWA 0x5B7D60
static const PaiTokenInfo g_maneuverNameTable[] = {
	{ "NULLMANR", 0 },
	{ "TURNINSIDEMANR", 1 },
	{ "SPLITSMANR", 2 },
	{ "IMMELMANNMANR", 3 },
	{ "SCISSORSMANR", 4 },
	{ "RENDEZVOUSMANR", 5 },
	{ "CRUISEMANR", 6 },
	{ "HEADTOWARDFULLMANR", 7 },
	{ "RUNAWAYMANR", 8 },
	{ "HEADONATTACKMANR", 9 },
	{ "FOLLOWLEADERMANR", 10 },
	{ "SETUPATTACKMANR", 11 },
	{ "ATTACKMANR", 12 },
	{ "ZOOMMANR", 13 },
	{ "DIVEMANR", 14 },
	{ "SPLITSDIVEMANR", 15 },
	{ "SPEEDAWAYMANR", 16 },
	{ "ESCORTMANR", 17 },
	{ "BOARDMANR", 18 },
	{ "AWAITBOARDMANR", 19 },
	{ "HEADTOWARDMANR", 20 },
	{ "INTOHYPERSPACEMANR", 21 },
	{ "OUTOFHYPERSPACEMANR", 22 },
	{ "ROCKETATTACKMANR", 23 },
	{ "TURNAWAYMANR", 24 },
	{ "STOPMANR", 25 },
	{ "OUTOFHANGARMANR", 26 },
	{ "EVASIVEMANR", 27 },
	{ "AVOIDSTARSHIPMANR", 28 },
	{ "WAITMANR", 29 },
	{ "DROPOFFMANR", 30 },
	{ "KAMIKAZEMANR", 31 },
	{ "AVOIDATTACKERMANR", 32 },
	{ "DODGEMANR", 33 },
	{ "ORBITMANR", 34 },
	{ "RELEASEMANR", 35 },
	{ "BACKUPMANR", 36 },
	{ "PARKMANR", 37 },
	{ "WORKONMANR", 38 },
	{ "DEATHSTARFOLLOWMANR", 39 },
	{ "FOLLOWTARGETMANR", 40 },
	{ "", 0 },
};

// GLOBAL: XWA 0x5B8AD8
static const PaiTokenInfo g_orderNameTable[] = {
	{ "NULLORDR", 0 },
	{ "UPDATECOURSEORDR", 1 },
	{ "UNDERATTACKORDR", 2 },
	{ "STILLATTACKORDR", 3 },
	{ "FLYHOMEORDR", 4 },
	{ "FIGHTERSHOOTORDR", 5 },
	{ "GUNNERSELFDEFENSEORDR", 6 },
	{ "GUNNEROFFENSEORDR", 7 },
	{ "MISSILEDEFENSEORDR", 8 },
	{ "SCANFORTARGETORDR", 9 },
	{ "WAITRUNORDR", 10 },
	{ "BREAKOFFORDR", 11 },
	{ "LEADERDEADORDR", 12 },
	{ "COVERLEADERORDR", 13 },
	{ "FOLLOWLEADATKORDR", 14 },
	{ "ABORTMISSIONORDR", 15 },
	{ "ONTAILORDR", 16 },
	{ "ALWAYSORDR", 17 },
	{ "CHECKESCORTORDR", 18 },
	{ "LEADERGOHOMEORDR", 19 },
	{ "HYPERSPACEORDR", 20 },
	{ "ENTERHANGARORDR", 21 },
	{ "MOTHERSHIPORDR", 22 },
	{ "ESCORTTARGETORDR", 23 },
	{ "LOOKFORCRAFTTOBOARDORDR", 24 },
	{ "ABORTBOARDORDR", 25 },
	{ "RETURNBOARDORDR", 26 },
	{ "AWAITBOARDORDR", 27 },
	{ "MAKEDISABLEDORDR", 28 },
	{ "NEARTARGETORDR", 29 },
	{ "ROCKETSONBOARDORDR", 30 },
	{ "AVOIDHITORDR", 31 },
	{ "WAITFORALLRETURNORDR", 32 },
	{ "WAITFORALLCREATEORDR", 33 },
	{ "EVASIVEORDR", 34 },
	{ "TARGETFROMPLAYERORDR", 35 },
	{ "AVOIDSTARSHIPORDR", 36 },
	{ "CHECKHYPERORDR", 37 },
	{ "STOPGOHOMEORDR", 38 },
	{ "COMPLETEGOHOMEORDR", 39 },
	{ "COMPLETEGOOTHERORDR", 40 },
	{ "COMPLETEFOLLOWORDR", 41 },
	{ "WAITGOOTHERORDR", 42 },
	{ "ORDERSWITCHORDR", 43 },
	{ "KILLSELFORDR", 44 },
	{ "DROPOFFDESTORDR", 45 },
	{ "ABORTMOTHERWAITORDR", 46 },
	{ "CHECKCONDITIONALORDR", 47 },
	{ "TRANSFERCARGOORDR", 48 },
	{ "SELFCAPTUREORDR", 49 },
	{ "CHECKRELEASEORDR", 50 },
	{ "CHECKDELIVERORDR", 51 },
	{ "CHANGESIDESORDR", 52 },
	{ "STARTOVERORDR", 53 },
	{ "DISAPPEARORDR", 54 },
	{ "COMMANDFROMPLAYERORDR", 55 },
	{ "RESUMEMISSIONORDR", 56 },
	{ "SCANFORPLAYERTARGETTYPEORDR", 57 },
	{ "INSPECTEDORDR", 58 },
	{ "SCANFORPLAYERINSPECTTYPEORDR", 59 },
	{ "COMPONENTGONEORDR", 60 },
	{ "REPAIRONESELFORDR", 61 },
	{ "FLYHOMEOTHERREGIONORDR", 62 },
	{ "PICKEDUPOBJECTORDR", 63 },
	{ "LOOKFORPARKORDR", 64 },
	{ "PLAYERGONEORDR", 65 },
	{ "", 0 },
};

// GLOBAL: XWA 0x5B7848
const char* g_builtinPlanNameTable[] = {
	"nullpln",
	"stationaryldrpln",
	"stationaryflwpln",
	"formldr1pln",
	"formflw1pln",
	"formevadeldr1pln",
	"formevadeflw1pln",
	"capldr1pln",
	"capescortersldr1pln",
	"caprespondldr1pln",
	"capldr2pln",
	"capldr3pln",
	"capldr4pln",
	"capldr5pln",
	"capflw1pln",
	"capflw2pln",
	"capflw3pln",
	"capflw4pln",
	"capflw5pln",
	"disableldr1pln",
	"escortldr1pln",
	"escortldr2pln",
	"escortldr3pln",
	"escortldr4pln",
	"escortflw1pln",
	"escortflw2pln",
	"escortflw3pln",
	"escortflw4pln",
	"boardtogivepln",
	"boardtotakepln",
	"boardtoexchangepln",
	"boardtocapturepln",
	"boardtodestroypln",
	"boardtopickuppln",
	"boardtocontactpln",
	"board2pln",
	"board3pln",
	"dropoffldr1pln",
	"dropoffldr2pln",
	"rendezvous1pln",
	"rendezvous2pln",
	"rendezvousflw1pln",
	"disabledpln",
	"waitforboardpln",
	"craftwaitforgopln",
	"flyhomepln",
	"followhomepln",
	"flyhomeevadepln",
	"followhomeevadepln",
	"enterhangarpln",
	"exithangarpln",
	"intohyperspacepln",
	"outofhyperspacepln",
	"starshipintohyperpln",
	"starshipfollowhomepln",
	"starshipstatpln",
	"starshipformpln",
	"starshipfollowpln",
	"starshipwaitreturnpln",
	"starshipwaitcreatepln",
	"starshipprotectpln",
	"starshipescortpln",
	"starshipattackpln",
	"starshipdisablepln",
	"starshipwaitforgopln",
	"variablepln",
	"waitpln",
	"selfdestroypln",
	"boardtorepairpln",
	"capfreeldr1pln",
	"kamikaze1pln",
	"kamikaze2pln",
	"kamikaze3pln",
	"hyperspacepln",
	"transfercargopln",
	"orbitpln",
	"selfcapturepln",
	"release1pln",
	"release2pln",
	"deliverpln",
	"changesidespln",
	"startoverpln",
	"backuppln",
	"hyperbuoypln",
	"disappearpln",
	"repaironeselfpln",
	"homeviahyperspacepln",
	"inspectldr1pln",
	"inspectldr2pln",
	"inspectldr3pln",
	"playercapldr1pln",
	"playercapldr2pln",
	"playercapldr3pln",
	"playercapldr4pln",
	"playercapldr5pln",
	"playerfollowpln",
	"playerinspectldr1pln",
	"playerinspectldr2pln",
	"playerinspectldr3pln",
	"playerdisableldr2pln",
	"playerboardtorepairpln",
	"playerboardtocapturepln",
	"playerboardtopickuppln",
	"playerboardtodestroypln",
	"playerboardtodefusepln",
	"playerboard2pln",
	"playerboard3pln",
	"resumemissionpln",
	"homing1pln",
	"homing2pln",
	"park1pln",
	"park2pln",
	"workon1pln",
	"workon2pln",
	"workon3pln",
	"deathstarfollowpln",
	"followtarget1pln",
	"followtarget2pln",
	"followtarget3pln",
	"",
};

static int pai_FindTokenIndex(const PaiTokenInfo* table, const char* token);
static int pai_FindPlanSlotByName(const char* planName);
static int pai_FindEmptyPlanSlot(void);
static void pai_ClosePlanCompileStream(XwaFile* stream);
uint8_t* pai_getplandataptrbyname(const char* planName);

// FUNCTION: XWA 0x4BE5C0
int pai_ReadPlanTextToken(char* token, XwaFile* stream) {
	uint8_t ch;
	int tokenLength;

	*token = '\0';
	for (;;) {
#ifdef XWA_MODERN
		if (!File_ReadByte(stream, &ch)) {
#else
		if (fread(&ch, 1u, 1u, stream) != 1) {
#endif
			return 1;
		}

		if (ch == ' ' || ch == '\t' || ch == '\n' || ch == ',') {
			continue;
		}

		if (ch == ';') {
			do {
#ifdef XWA_MODERN
				if (!File_ReadByte(stream, &ch)) {
#else
				if (fread(&ch, 1u, 1u, stream) != 1) {
#endif
					return 1;
				}
			} while (ch != '\n');
			continue;
		}

		token[0] = (char)ch;
		tokenLength = 1;
#ifdef XWA_MODERN
		while (File_ReadByte(stream, &ch) && ch != ',' && ch != '\n' && ch != ' ' && ch != '\t') {
#else
		while (fread(&ch, 1u, 1u, stream) == 1 && ch != ',' && ch != '\n' && ch != ' ' && ch != '\t') {
#endif
			token[tokenLength++] = (char)ch;
		}
		token[tokenLength] = '\0';
		return 1;
	}
}

static int pai_FindTokenIndex(const PaiTokenInfo* table, const char* token) {
	int index;

	index = 0;
	while (table[index].name[0] != '\0') {
		if (strcmp(table[index].name, token) == 0) {
			return index;
		}
		++index;
	}

	return -1;
}

static int pai_FindPlanSlotByName(const char* planName) {
	int planId;

	for (planId = 0; planId < 256; ++planId) {
		if (strncmp(g_planTable[planId].name, planName, sizeof(g_planTable[planId].name)) == 0) {
			return planId;
		}
	}

	return 256;
}

static int pai_FindEmptyPlanSlot(void) {
	int planId;

	for (planId = 0; planId < 256; ++planId) {
		if (g_planTable[planId].name[0] == '\0') {
			return planId;
		}
	}

	return 256;
}

static void pai_ClosePlanCompileStream(XwaFile* stream) {
	if (stream != NULL) {
		File_Close(stream);
	}
	if (g_stream == stream) {
		g_stream = NULL;
	}
}

// FUNCTION: XWA 0x4BE670
int pai_CompilePlansFromText(const char* baseName) {
	char fileName[256];
	char token[256];
	XwaFile* stream;
	uint8_t* cursor;
	int planId;

	strcpy(fileName, baseName);
	strcat(fileName, ".pln");
	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "r");
	g_stream = stream;
	if (stream == NULL) {
		return 0;
	}

	cursor = g_planOrderData;
	for (;;) {
		int targetIndex;
		int maneuverIndex;

		if (!pai_ReadPlanTextToken(token, stream)) {
			pai_ClosePlanCompileStream(stream);
			return 0;
		}

		if (token[0] == '*') {
			break;
		}

		planId = pai_FindPlanSlotByName(token);
		if (planId == 256) {
			planId = pai_FindEmptyPlanSlot();
			if (planId == 256) {
				pai_ClosePlanCompileStream(stream);
				return 0;
			}
		} else if (g_planTable[planId].isDefined == 1) {
			pai_ClosePlanCompileStream(stream);
			return 0;
		}

		strncpy(g_planTable[planId].name, token, sizeof(g_planTable[planId].name));
		g_planTable[planId].name[79] = '\0';
		g_planTable[planId].isDefined = 1;
		g_planTable[planId].dataOffset = (uint32_t)(cursor - g_planOrderData);
		g_planDataPtrs[planId] = cursor;

		if (!pai_ReadPlanTextToken(token, stream)) {
			pai_ClosePlanCompileStream(stream);
			return 0;
		}

		targetIndex = pai_FindTokenIndex(g_planTargetNameTable, token);
		if (targetIndex < 0) {
			pai_ClosePlanCompileStream(stream);
			return 0;
		}
		*cursor++ = (uint8_t)g_planTargetNameTable[targetIndex].id;

		if (!pai_ReadPlanTextToken(token, stream)) {
			pai_ClosePlanCompileStream(stream);
			return 0;
		}

		maneuverIndex = pai_FindTokenIndex(g_maneuverNameTable, token);
		if (maneuverIndex < 0) {
			pai_ClosePlanCompileStream(stream);
			return 0;
		}
		*cursor++ = (uint8_t)g_maneuverNameTable[maneuverIndex].id;

		for (;;) {
			int orderIndex;
			int nextPlanId;

			if (!pai_ReadPlanTextToken(token, stream)) {
				pai_ClosePlanCompileStream(stream);
				return 0;
			}

			orderIndex = pai_FindTokenIndex(g_orderNameTable, token);
			if (orderIndex < 0) {
				pai_ClosePlanCompileStream(stream);
				return 0;
			}

			*cursor++ = (uint8_t)g_orderNameTable[orderIndex].id;
			if (orderIndex == 0) {
				++g_planCount;
				break;
			}

			if (!pai_ReadPlanTextToken(token, stream)) {
				pai_ClosePlanCompileStream(stream);
				return 0;
			}

			nextPlanId = pai_FindPlanSlotByName(token);
			if (nextPlanId == 256) {
				nextPlanId = pai_FindEmptyPlanSlot();
				if (nextPlanId == 256) {
					pai_ClosePlanCompileStream(stream);
					return 0;
				}

				strncpy(g_planTable[nextPlanId].name, token, sizeof(g_planTable[nextPlanId].name));
				g_planTable[nextPlanId].name[79] = '\0';
				g_planTable[nextPlanId].isDefined = 0;
			}
			*cursor++ = (uint8_t)nextPlanId;
		}
	}

	pai_ClosePlanCompileStream(stream);

	for (planId = 0; planId < 256; ++planId) {
		if (g_planTable[planId].name[0] != '\0' && g_planTable[planId].isDefined != 1) {
			return 0;
		}
	}

	strcpy(fileName, baseName);
	strcat(fileName, ".plo");
	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "wb");
	g_stream = stream;
	if (stream != NULL) {
		File_WriteDword(stream, (int)sizeof(g_planTable));
		File_WriteCount(stream, g_planTable, sizeof(g_planTable));
		File_WriteDword(stream, 0xffff);
		File_WriteCount(stream, g_planOrderData, 0xffffu);
		pai_ClosePlanCompileStream(stream);
	}

	return 1;
}

// FUNCTION: XWA 0x4BEC20
int pai_loadplans(const char* baseName) {
	char fileName[256];
#ifdef XWA_MODERN
	XwaFile* stream;
	int tableSize;
	int orderDataSize;
#else
	FILE* stream;
	int size;
#endif
	int planId;

	strcpy(fileName, baseName);
	strcat(fileName, ".plo");
	g_planCount = 0;
	memset(g_planTable, 0, sizeof(g_planTable));

#ifdef XWA_MODERN
	memset(g_planDataPtrs, 0, sizeof(g_planDataPtrs));

	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
	g_stream = stream;
	if (stream == NULL) {
		return pai_CompilePlansFromText(baseName);
	}

	if (!File_ReadDword(stream, &tableSize) || tableSize != (int)sizeof(g_planTable) ||
		!File_ReadCount(stream, g_planTable, (size_t)tableSize) || !File_ReadDword(stream, &orderDataSize) ||
		orderDataSize != 0xffff || !File_ReadCount(stream, g_planOrderData, (size_t)orderDataSize)) {
		pai_ClosePlanCompileStream(stream);
		return pai_CompilePlansFromText(baseName);
	}

	pai_ClosePlanCompileStream(stream);

	for (planId = 0; planId < 256; ++planId) {
		if (g_planTable[planId].name[0] != '\0') {
			g_planDataPtrs[planId] = &g_planOrderData[g_planTable[planId].dataOffset];
			++g_planCount;
		}
	}

	return 1;
#else
	memset(g_planDataPtrs, 0, 256);

	File_OpenGlobalStream(fileName, "rb", 0, 1);
	stream = (FILE*)g_stream;
	if (g_stream == NULL) {
		return pai_CompilePlansFromText(baseName);
	}

	if (fread(&size, 4, 1, (FILE*)g_stream) != 1) {
		fclose(stream);
		return pai_CompilePlansFromText(baseName);
	}
	if (fread(g_planTable, (size_t)size, 1, stream) != 1) {
		fclose(stream);
		return pai_CompilePlansFromText(baseName);
	}
	if (fread(&size, 4, 1, stream) != 1) {
		fclose(stream);
		return pai_CompilePlansFromText(baseName);
	}
	if (fread(g_planOrderData, (size_t)size, 1, stream) != 1) {
		fclose(stream);
		return pai_CompilePlansFromText(baseName);
	}

	fclose(stream);

	for (planId = 0; planId < 256; ++planId) {
		if (g_planTable[planId].name[0] != '\0') {
			g_planDataPtrs[planId] = &g_planOrderData[g_planTable[planId].dataOffset];
			++g_planCount;
		}
	}

	return 1;
#endif
}

// FUNCTION: XWA 0x4BEE40
int pai_findplanbyname(const char* planName) {
	int planId;
	PlanRecord* plan;

	for (planId = 0, plan = g_planTable; (intptr_t)plan < (intptr_t)&g_planTable[256]; ++plan, ++planId) {
		if (strncmp(plan->name, planName, sizeof(plan->name)) == 0) {
			return planId;
		}
	}

	return 0;
}

// FUNCTION: XWA 0x4BEE00
uint8_t* pai_getplandataptrbyname(const char* planName) {
	int i;
	PlanRecord* plan;
	for (i = 0; i < 256; i++) {
		if (!strncmp(g_planTable[i].name, planName, sizeof(plan->name))) {
			break;
		}
	}

	return g_planDataPtrs[i];
}

// FUNCTION: XWA 0x4BEDA0
void pai_cacheBuiltinPlanIds(void) {
	const char** namePtr;
	uint8_t* outPlanIds;
	unsigned int cacheIndex;
	const char* planName;

	outPlanIds = g_builtinPlanIdByNameIndex;
	namePtr = g_builtinPlanNameTable;
	cacheIndex = 0;
	for (;;) {
		unsigned int planId;
		PlanRecord* plan;

		planId = 0;
		plan = g_planTable;
		planName = *namePtr;
		if (*planName == '\0') {
			break;
		}

		for (;
#ifdef _M_IX86
			 (intptr_t)plan < (intptr_t)&g_mobileObjectPoolBase;
#else
			 plan < &g_planTable[256];
#endif
		) {
			if (strncmp(plan->name, planName, sizeof(plan->name)) == 0) {
				break;
			}
			++planId;
			++plan;
		}

		outPlanIds[cacheIndex] = planId;
		++cacheIndex;
		++namePtr;
	}
}
