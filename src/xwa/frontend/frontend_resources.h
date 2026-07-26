#ifndef XWA_FRONTEND_FRONTEND_RESOURCES_H
#define XWA_FRONTEND_FRONTEND_RESOURCES_H

#include "xwa/frontend/frontend_mission.h"

#ifdef __cplusplus
extern "C" {
#endif

extern FrontendMission* g_frontendMission;
extern int g_gameMainSkipIntroRelaunchGate;
extern int g_unusedFrontendResourcesLoadedFlag;
extern int g_hostCdAvailable;
extern int g_frontendMissionLoaded;
extern int g_currentMissionId;
extern unsigned char* g_cursorBitmap;
extern char* g_frontendChatLogBuffer;
extern int g_frontendChatLogUsedBytes;
extern int g_diskId;
extern const char* g_cmdLine;

int Frontend_MarkHostCdAvailable(void);
int Frontend_LoadResources(void);

// TODO: reimplement localized error-text line loader (XWA 0x529780).
int ErrorText_LoadLine(int lineId, char* buffer);

#ifdef __cplusplus
}
#endif

#endif
