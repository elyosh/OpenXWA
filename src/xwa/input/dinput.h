#ifndef XWA_INPUT_DINPUT_H
#define XWA_INPUT_DINPUT_H

#include <stdint.h>

#include "xwa_runtime/compat/directx/dinput.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Recovered DirectInput keyboard/mouse layer. The devices are created and polled
 * through the DirectInput compatibility shim (compat/directx/dinput.h), backed by
 * Aeron input. Consumers read the immediate keyboard state array + cached modifier
 * flags, drain buffered keys via DInput_GetKey/HasKeyReady, and read the DIMOUSESTATE
 * populated by DInput_PollMouseState. */

extern unsigned char g_dinputKeyboardState[256];
extern int           g_dinputShiftDown;
extern int           g_dinputCtrlDown;
extern int           g_dinputAltDown;
extern void*         hwnd;

/* Immediate mouse state, filled by DInput_PollMouseState via GetDeviceState. */
extern DIMOUSESTATE g_dinputMouseState;

char     DInput_Init(void);
void     DInput_Shutdown(void);
int      DInput_ReadKeyboardState(void);
int      DInput_DrainKeyboardEvents(void);
uint16_t DInput_GetKey(void);
int      DInput_HasKeyReady(void);
int      DInput_UpdateKeyboardModifierState(void);
void     DInput_PollMouseState(void);

#ifdef __cplusplus
}
#endif

#endif
