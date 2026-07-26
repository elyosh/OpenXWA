#ifndef XWA_RUNTIME_COMPAT_WINMM_JOYSTICK_H
#define XWA_RUNTIME_COMPAT_WINMM_JOYSTICK_H

/* WinMM joystick API compatibility shim, backed by Aeron gamepads.
 *
 * The recovered joystick code (Joystick_InitDevices / UpdateState / PollRawAxes) uses
 * the Windows Multimedia joystick API -- joyGetNumDevs / joyGetDevCapsA / joyGetPosEx
 * -- exactly as the original did. This header declares that API plus the JOYCAPS /
 * JOYINFOEX layouts; the shim (winmm/joystick_compat.c) fills them from
 * Aeron_InputSnapshot()->gamepads[], applying the YAML device-layout config (axis
 * slot assignment, inversion, deadzone override). */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MMRESULT: 0 (JOYERR_NOERROR) on success. */
typedef uint32_t MMRESULT;

#if defined(_WIN32) && defined(_M_IX86)
#define XWA_WINMMAPI __stdcall
#if defined(XWA_WINMM_COMPAT_IMPLEMENTATION)
#define XWA_WINMMIMPORT
#else
#define XWA_WINMMIMPORT __declspec(dllimport)
#endif
#else
#define XWA_WINMMAPI
#define XWA_WINMMIMPORT
#endif

#define JOYERR_NOERROR   0u
#define JOYERR_PARMS     165u
#define JOYERR_UNPLUGGED 167u

/* JOYCAPS.wCaps bits used by the recovered code. */
#define JOYCAPS_HASZ   0x0001u
#define JOYCAPS_HASR   0x0002u
#define JOYCAPS_HASU   0x0004u
#define JOYCAPS_HASV   0x0008u
#define JOYCAPS_HASPOV 0x0010u

/* JOYINFOEX.dwFlags bits (which fields joyGetPosEx returns). The shim fills every
 * axis regardless, so these are accepted and not required. */
#define JOY_RETURNX        0x00000001u
#define JOY_RETURNY        0x00000002u
#define JOY_RETURNZ        0x00000004u
#define JOY_RETURNR        0x00000008u
#define JOY_RETURNPOV      0x00000040u
#define JOY_RETURNBUTTONS  0x00000080u
#define JOY_RETURNCENTERED 0x00000400u
#define JOY_RETURNALL      0x000000FFu

/* Centered POV value. */
#define JOY_POVCENTERED 0xFFFFu

/* joyGetDevCapsA capabilities structure (ANSI). Full Win32 layout so the recovered
 * code's sizeof/0x194 argument and field offsets match. */
typedef struct tagJOYCAPSA {
	uint16_t wMid;
	uint16_t wPid;
	char     szPname[32];
	uint32_t wXmin;
	uint32_t wXmax;
	uint32_t wYmin;
	uint32_t wYmax;
	uint32_t wZmin;
	uint32_t wZmax;
	uint32_t wNumButtons;
	uint32_t wPeriodMin;
	uint32_t wPeriodMax;
	uint32_t wRmin;
	uint32_t wRmax;
	uint32_t wUmin;
	uint32_t wUmax;
	uint32_t wVmin;
	uint32_t wVmax;
	uint32_t wCaps;
	uint32_t wMaxAxes;
	uint32_t wNumAxes;
	uint32_t wMaxButtons;
	char     szRegKey[32];
	char     szOEMVxD[260];
} JOYCAPSA;

/* joyGetPosEx extended position structure. */
typedef struct joyinfoex_tag {
	uint32_t dwSize;
	uint32_t dwFlags;
	uint32_t dwXpos;
	uint32_t dwYpos;
	uint32_t dwZpos;
	uint32_t dwRpos;
	uint32_t dwUpos;
	uint32_t dwVpos;
	uint32_t dwButtons;
	uint32_t dwButtonNumber;
	uint32_t dwPOV;
	uint32_t dwReserved1;
	uint32_t dwReserved2;
} JOYINFOEX;

/* Latest successful Aeron-to-WinMM axis conversion, for modern diagnostics. */
typedef struct WinmmJoystickTraceSample {
	uint32_t deviceId;
	int8_t   sourceAxisX;
	int8_t   sourceAxisY;
	int8_t   sourceAxisR;
	int16_t  sourceValueX;
	int16_t  sourceValueY;
	int16_t  sourceValueR;
	uint32_t winmmX;
	uint32_t winmmY;
	uint32_t winmmR;
} WinmmJoystickTraceSample;

/* Number of joystick devices supported (recovered code treats >0 as "devices exist"). */
XWA_WINMMIMPORT uint32_t XWA_WINMMAPI joyGetNumDevs(void);

/* Fills caps for device uJoyID; JOYERR_NOERROR if present, else an error. */
XWA_WINMMIMPORT MMRESULT XWA_WINMMAPI joyGetDevCapsA(uint32_t uJoyID, JOYCAPSA* pjc, uint32_t cbjc);

/* Fills the extended position for device uJoyID from the mapped Aeron gamepad. */
XWA_WINMMIMPORT MMRESULT XWA_WINMMAPI joyGetPosEx(uint32_t uJoyID, JOYINFOEX* pji);

/* Copies the most recent successful joyGetPosEx axis conversion. */
int WinmmJoystick_GetLastTraceSample(WinmmJoystickTraceSample* sample);

#ifdef __cplusplus
}
#endif

#endif
