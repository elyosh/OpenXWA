#ifndef XWA_RUNTIME_INPUT_WINMM_JOYSTICK_PROVIDER_H
#define XWA_RUNTIME_INPUT_WINMM_JOYSTICK_PROVIDER_H

#ifdef __cplusplus
extern "C" {
#endif

/* XWA's source for the Aeron WinMM joystick shim. It reports only whether a
 * saved controller model is connected, with centred axes and no buttons, so
 * Input_DetectActiveJoystick(), g_joystickEnabled and the force-feedback
 * device checks stay consistent. Flight reads the controllers through
 * XwaFlightControls instead. */

/* Registers the source with AeronCompat_SetJoystickSource. Call once at port
 * startup, before recovered code polls the joystick API. */
void XwaWinmmJoystick_RegisterSource(void);

#ifdef __cplusplus
}
#endif

#endif
