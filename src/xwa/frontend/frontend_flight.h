#ifndef XWA_FRONTEND_FRONTEND_FLIGHT_H
#define XWA_FRONTEND_FRONTEND_FLIGHT_H

#ifdef __cplusplus
extern "C" {
#endif

extern char cmdLine[256];
extern char g_filmFilePath[256];

int FrontendFlight_LaunchSession(int frameCounter);
int FrontendFlight_HasPendingLaunch(void);
int FrontendFlight_BeginPendingLaunch(void);
void FrontendFlight_CompleteLaunchSession(int flightResult);

#ifdef __cplusplus
}
#endif

#endif
