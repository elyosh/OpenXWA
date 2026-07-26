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

#ifdef __cplusplus
}
#endif

#endif /* XWA_FLIGHT_FLIGHT_DEBUG_H */
