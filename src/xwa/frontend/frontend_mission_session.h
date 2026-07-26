#ifndef XWA_FRONTEND_FRONTEND_MISSION_SESSION_H
#define XWA_FRONTEND_FRONTEND_MISSION_SESSION_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FrontendMissionSessionMode {
	FRONTEND_MISSION_SESSION_NONE = 0,
	FRONTEND_MISSION_SESSION_SINGLEPLAYER = 2,
	FRONTEND_MISSION_SESSION_NET_CLIENT = 3,
	FRONTEND_MISSION_SESSION_NET_HOST = 4
} FrontendMissionSessionMode;

extern FrontendMissionSessionMode g_frontendMissionSessionMode;
extern int g_frontendSinglePlayerFlightSessionActive;
extern int g_frontendQuickStartLaunchFlag;
extern int g_skipFrontendEntryMovie;
extern int g_unusedFrontendMissionLaunchPrepared;
extern int g_unusedFrontendConcourseHostLatch;
extern int g_unusedConcourseEntryResetFlag;

#ifdef __cplusplus
}
#endif

#endif
