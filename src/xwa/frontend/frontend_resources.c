#include "xwa/frontend/frontend_resources.h"

#include "xwa/assets/file_io.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/assets/ship_list.h"
#include "xwa/assets/string_table.h"
#include "xwa/audio/music.h"
#include "xwa/config/game_config.h"
#include "xwa/config/pilot.h"
#include "xwa/frontend/cutscene.h"
#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_file_stream.h"
#include "xwa/frontend/frontend_medals.h"
#include "xwa/frontend/frontend_rect.h"
#include "xwa/frontend/frontend_scratch.h"
#include "xwa/frontend/frontend_sound.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa/util/debug.h"
#include "xwa/util/memory.h"

#include <stdlib.h>
#include <string.h>

enum {
	FRONTEND_CHAT_LOG_SIZE = 0x400,
	FRONTEND_FILE_STREAM_SLOT = 1,
};

// GLOBAL: XWA 0x9EB8E0
FrontendMission* g_frontendMission;
// GLOBAL: XWA 0x603070
int g_gameMainSkipIntroRelaunchGate;
// GLOBAL: XWA 0xB07C70
int g_unusedFrontendResourcesLoadedFlag;
// GLOBAL: XWA 0xABD7D4
int g_hostCdAvailable;
// GLOBAL: XWA 0xABCF80
int g_frontendMissionLoaded;
// GLOBAL: XWA 0xABC970
int g_currentMissionId;
// GLOBAL: XWA 0xAE2A3C
unsigned char* g_cursorBitmap;
// GLOBAL: XWA 0xABD21C
char* g_frontendChatLogBuffer;
// GLOBAL: XWA 0xAE2A4C
int g_frontendChatLogUsedBytes;
// GLOBAL: XWA 0xABD7C8
int g_diskId;
// GLOBAL: XWA 0xABC94C
const char* g_cmdLine = "";

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x529870
int Frontend_MarkHostCdAvailable(void) {
	unsigned char unused[104];

	memset(unused, 0, sizeof(unused));
	g_hostCdAvailable = 1;
	return 1;
}

// TODO: stub for XWA 0x529780 — original reads a localized error-text line from
// the error-text table into the caller buffer and returns non-zero on success.
int ErrorText_LoadLine(int lineId, char* buffer) {
	(void)lineId;
	(void)buffer;
	return 0;
}

// FUNCTION: XWA 0x528A50
int Frontend_LoadResources(void) {
	XwaFile* stream;
	FrontendRect rect;

	g_frontendMission = (FrontendMission*)Mem_Alloc(sizeof(*g_frontendMission));
	memset(g_frontendMission, 0, sizeof(*g_frontendMission));
	g_gameMainSkipIntroRelaunchGate = 1;
	g_unusedFrontendResourcesLoadedFlag = 1;
	Frontend_MarkHostCdAvailable();
	FrontendDisplay_DisableEscapeClose();
	FrontendDisplay_SetSurfaceClearColor(0);
	FrontendCursor_Show();
	FrontendDisplay_ClearPresentFrameReady();
	FrontendDisplay_EnableOffscreenRestore();
	FrontendText_LoadFont(15);
	FrontendText_LoadFont(12);
	FrontendText_LoadFont(10);
	FrontImage_LoadResourceList("frontres\\combat\\combat.lst");
	FrontImage_LoadResourceList("frontres\\datapad\\top.lst");
	FrontImage_LoadResourceList("frontres\\datapad\\awards.lst");
	FrontImage_LoadResourceList("frontres\\icons\\icons.lst");
	FrontImage_LoadResourceList("frontres\\skirmish\\skirmish.lst");
	FrontImage_LoadResourceList("frontres\\family\\family.lst");
	FrontImage_LoadResourceList("frontres\\cutscene\\cutscene.lst");
	FrontImage_LoadResourceList("frontres\\config\\config.lst");
	FrontImage_InitAtlasSprites();
	FrontendCursor_LoadResources();
	FrontendSound_LoadList("sfx\\sfx.lst");
	FrontImage_GetResourceRect("cursor", &rect);
	if (g_cursorBitmap) {
		Mem_Free(g_cursorBitmap);
		g_cursorBitmap = 0;
	}

	g_cursorBitmap = (unsigned char*)Mem_Alloc((size_t)(2 * (rect.bottom + 1) * (rect.right + 1)));
	FrontendCursor_SetImageResourceForCurrentTheme("cursor", g_cursorBitmap);
	memset(&g_pilotData, 0, sizeof(g_pilotData));
	FrontendString_LoadTable("ALLIANCE/fronttxt.txt");
	ShipList_Load();
	FrontendMedals_LoadTable();
	Cutscene_LoadTable("frontres\\cutscene\\cutscene.txt");
	g_frontendChatLogBuffer = (char*)Mem_Alloc(FRONTEND_CHAT_LOG_SIZE);
	g_frontendChatLogUsedBytes = 0;
	if (g_frontendChatLogBuffer) {
		memset(g_frontendChatLogBuffer, 0, FRONTEND_CHAT_LOG_SIZE);
	}

	FrontendColor_Init();
	if (!Pilot_FindAndLoadByName(g_gameConfig.lastPilotName) && g_gameConfig.lastPilotName[0]) {
		Pilot_CreateNew(g_gameConfig.lastPilotName);
	}

	Music_Init(FrontendSound_GetDirectSound());
	FrontendFileStream_InitSlotBuffer(FRONTEND_FILE_STREAM_SLOT, 1024000, 512000);
	stream = File_Open(AERON_VFS_ROOT_ASSET, "disk1.id", "r");
	if (stream) {
		File_ReadLine(stream, g_frontendScratchBuffer, sizeof(g_frontendScratchBuffer));
		g_diskId = atoi(g_frontendScratchBuffer);
		if (!g_diskId) {
			g_diskId = 22;
		}
		File_Close(stream);
	} else {
		g_diskId = 9;
	}

	DebugPrintf(0);
	Pilot_ParseCommandLine(g_cmdLine);
	return 0;
}
