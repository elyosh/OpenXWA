#ifndef XWA_FLIGHT_FLIGHT_DEBUG_H
#define XWA_FLIGHT_FLIGHT_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Modern-port diagnostic control used by the optional debug overlay. */
int FlightDebug_JoystickTraceEnabled(void);
void FlightDebug_SetJoystickTraceEnabled(int enabled);
int FlightDebug_GimbalLockFixEnabled(void);
void FlightDebug_SetGimbalLockFixEnabled(int enabled);
/* Cockpit eye of the local player's craft: its g_modelDefs entry
 * primaryHardpointX/Y/Z (the craft-table fields at +0x238 that
 * ComputeHardpointWorldPos reads every frame). Get returns 0 without a
 * player craft. Set edits the table in place until the model is reloaded;
 * Restore puts back the values seen before the first Set for that model. */
int FlightDebug_GetPlayerCockpitEye(int* modelIndex, int eye[3]);
int FlightDebug_SetPlayerCockpitEye(const int eye[3]);
int FlightDebug_RestorePlayerCockpitEye(void);
#ifdef XWA_MODERN
void FlightDebug_CaptureJoystickInputSample(int timestamp);
#endif

#ifdef __cplusplus
}
#endif

#endif /* XWA_FLIGHT_FLIGHT_DEBUG_H */
