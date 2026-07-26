#include "xwa/input/dinput.h"

#include "xwa/assets/string_table.h"
#include "xwa/input/forcefeedback.h"
#include "xwa/util/debug.h"

#include <string.h>

/* Main window handle used for SetCooperativeLevel; owned by the display layer. The
 * DirectInput shim ignores it, but the recovered init passes it as the original did. */
extern void* g_hWnd;

// GLOBAL: XWA 0x91AD38
void* hwnd;

#ifndef XWA_MODERN
extern void(__stdcall* g_OutputDebugStringA)(const char* outputString);
// GLOBAL: XWA 0x91AD44
void* g_hInstance;
#define DINPUT_OUTPUT_DEBUG_STRING g_OutputDebugStringA
#define DINPUT_HINSTANCE g_hInstance
#define DINPUT_HWND hwnd
#else
static void OutputDebugStringA(const char* outputString) { DebugPrintf("%s", outputString); }
#define DINPUT_OUTPUT_DEBUG_STRING OutputDebugStringA
#define DINPUT_HINSTANCE NULL
#define DINPUT_HWND g_hWnd
#endif

/* Buffered-key translation tables: DIK scancode -> game key code. Codes 0xFD/0xFE/0xFF
 * are Shift/Ctrl/Alt markers handled specially by DInput_GetKey/HasKeyReady. */

// GLOBAL: XWA 0x5B2730
static const uint16_t g_dinputKeyCodeTable[256] = {
	0x0000, 0x001b, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x0030, 0x002d,
	0x003d, 0x0008, 0x0009, 0x0071, 0x0077, 0x0065, 0x0072, 0x0074, 0x0079, 0x0075, 0x0069, 0x006f, 0x0070,
	0x005b, 0x005d, 0x000d, 0x00fe, 0x0061, 0x0073, 0x0064, 0x0066, 0x0067, 0x0068, 0x006a, 0x006b, 0x006c,
	0x003b, 0x0027, 0x0060, 0x00fd, 0x005c, 0x007a, 0x0078, 0x0063, 0x0076, 0x0062, 0x006e, 0x006d, 0x002c,
	0x002e, 0x002f, 0x00fd, 0x00be, 0x00ff, 0x0020, 0x00b1, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7, 0x00c8,
	0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00bc, 0x00af, 0x00b9, 0x00ba, 0x00bb, 0x00bf, 0x00b6, 0x00b7, 0x00b8,
	0x00c0, 0x00b3, 0x00b4, 0x00b5, 0x00b2, 0x00c2, 0x0000, 0x0000, 0x0000, 0x00cd, 0x00ce, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x00c1, 0x00fe, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00bd,
	0x0000, 0x00ae, 0x00ff, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x00b0, 0x0000, 0x00aa, 0x00a6, 0x00ac, 0x0000, 0x00a4, 0x0000, 0x00a5, 0x0000, 0x00ab,
	0x00a7, 0x00ad, 0x00a8, 0x00a9, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// GLOBAL: XWA 0x5B2930
static const uint16_t g_dinputShiftKeyCodeTable[256] = {
	0x0000, 0x001b, 0x0021, 0x0040, 0x0023, 0x0024, 0x0025, 0x005e, 0x0026, 0x002a, 0x0028, 0x0029, 0x005f,
	0x002b, 0x0008, 0x000a, 0x0051, 0x0057, 0x0045, 0x0052, 0x0054, 0x0059, 0x0055, 0x0049, 0x004f, 0x0050,
	0x007b, 0x007d, 0x000d, 0x00fe, 0x0041, 0x0053, 0x0044, 0x0046, 0x0047, 0x0048, 0x004a, 0x004b, 0x004c,
	0x003a, 0x0022, 0x007e, 0x00fd, 0x007c, 0x005a, 0x0058, 0x0043, 0x0056, 0x0042, 0x004e, 0x004d, 0x003c,
	0x003e, 0x003f, 0x00fd, 0x00be, 0x00ff, 0x0020, 0x00b1, 0x00cf, 0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4,
	0x00d5, 0x00d6, 0x00d7, 0x00d8, 0x00bc, 0x00af, 0x00b9, 0x00ba, 0x00bb, 0x00bf, 0x00b6, 0x00b7, 0x00b8,
	0x00c0, 0x00b3, 0x00b4, 0x00b5, 0x00b2, 0x00c2, 0x0000, 0x0000, 0x0000, 0x00d9, 0x00da, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x00c1, 0x00fe, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00bd,
	0x0000, 0x00ae, 0x00ff, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x00eb, 0x0000, 0x00aa, 0x00a6, 0x00ac, 0x0000, 0x00a4, 0x0000, 0x00a5, 0x0000, 0x00ab,
	0x00a7, 0x00ad, 0x00a8, 0x00a9, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// GLOBAL: XWA 0x5B2B30
static const uint16_t g_dinputCtrlKeyCodeTable[256] = {
	0x0000, 0x001b, 0x0106, 0x0107, 0x0108, 0x0109, 0x010a, 0x010b, 0x010c, 0x010d, 0x010e, 0x0105, 0x005f,
	0x002b, 0x0008, 0x0009, 0x011f, 0x0125, 0x0113, 0x0120, 0x0122, 0x0127, 0x0123, 0x0117, 0x011d, 0x011e,
	0x001b, 0x001d, 0x000a, 0x00fe, 0x010f, 0x0121, 0x0112, 0x0114, 0x0115, 0x0116, 0x0118, 0x0119, 0x011a,
	0x003b, 0x0022, 0x0060, 0x00fd, 0x005c, 0x0128, 0x0126, 0x0111, 0x0124, 0x0110, 0x011c, 0x011b, 0x002c,
	0x002e, 0x002f, 0x00fd, 0x00be, 0x00ff, 0x0020, 0x00b1, 0x00cf, 0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4,
	0x00d5, 0x00d6, 0x00d7, 0x00d8, 0x00bc, 0x00e9, 0x00b9, 0x00ba, 0x00bb, 0x00bf, 0x00b6, 0x00b7, 0x00b8,
	0x00c0, 0x00b3, 0x00b4, 0x00b5, 0x00b2, 0x00c2, 0x0000, 0x0000, 0x0000, 0x00d9, 0x00da, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x00c1, 0x00fe, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00bd,
	0x0000, 0x00e8, 0x00ff, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x00aa, 0x00a6, 0x00ac, 0x0000, 0x00a4, 0x0000, 0x00a5, 0x0000, 0x012c,
	0x00a7, 0x00ad, 0x00a8, 0x00a9, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// GLOBAL: XWA 0x5B2D30
static const uint16_t g_dinputAltKeyCodeTable[256] = {
	0x0000, 0x001b, 0x009b, 0x009c, 0x009d, 0x009e, 0x009f, 0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x009a, 0x005f,
	0x002b, 0x0008, 0x0009, 0x0090, 0x0096, 0x0084, 0x0091, 0x0093, 0x0098, 0x0094, 0x0088, 0x008e, 0x008f,
	0x007b, 0x007d, 0x000d, 0x00fe, 0x0080, 0x0092, 0x0083, 0x0085, 0x0086, 0x0087, 0x0089, 0x008a, 0x008b,
	0x003a, 0x0022, 0x00ea, 0x00fd, 0x007c, 0x0099, 0x0097, 0x0082, 0x0095, 0x0081, 0x008d, 0x008c, 0x003c,
	0x012d, 0x003f, 0x00fd, 0x00be, 0x00ff, 0x0020, 0x00b1, 0x00cf, 0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4,
	0x00d5, 0x00d6, 0x00d7, 0x00d8, 0x00bc, 0x00af, 0x00b9, 0x00ba, 0x00bb, 0x00bf, 0x00b6, 0x00b7, 0x00f0,
	0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7, 0x00f8, 0x00f9, 0x00fa, 0x00fb, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x00c1, 0x00fe, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00bd,
	0x0000, 0x00e8, 0x00ff, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x00aa, 0x00a6, 0x00ac, 0x0000, 0x00a4, 0x0000, 0x00a5, 0x0000, 0x00ab,
	0x00a7, 0x00ad, 0x00a8, 0x00a9, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// GLOBAL: XWA 0x6343C8
IDirectInputA* g_directInput;
// GLOBAL: XWA 0x6343CC
IDirectInputDeviceA* g_dinputKeyboardDevice;
// GLOBAL: XWA 0x6343D0
IDirectInputDeviceA* g_dinputMouseDevice;
// GLOBAL: XWA 0x6343D4
unsigned char g_dinputKeyboardAcquired;
// GLOBAL: XWA 0x6343D8
unsigned char g_dinputMouseAcquired;

// GLOBAL: XWA 0x9E9520
unsigned char g_dinputKeyboardState[256];
// GLOBAL: XWA 0x6343DC
int g_dinputCtrlDown;
// GLOBAL: XWA 0x6343E0
int g_dinputShiftDown;
// GLOBAL: XWA 0x6343E4
int g_dinputAltDown;
// GLOBAL: XWA 0x9E9620
DIMOUSESTATE g_dinputMouseState;

// FUNCTION: XWA 0x42B4A0
void DInput_Shutdown(void) {
	if (g_dinputMouseDevice) {
		if (g_dinputMouseAcquired) {
			g_dinputMouseDevice->lpVtbl->Unacquire(g_dinputMouseDevice);
			g_dinputMouseAcquired = 0;
		}
	}
	if (g_dinputMouseDevice) {
		g_dinputMouseDevice->lpVtbl->Release(g_dinputMouseDevice);
		g_dinputMouseDevice = NULL;
	}
	if (g_dinputKeyboardDevice) {
		if (g_dinputKeyboardAcquired) {
			g_dinputKeyboardDevice->lpVtbl->Unacquire(g_dinputKeyboardDevice);
			g_dinputKeyboardAcquired = 0;
		}
	}
	if (g_dinputKeyboardDevice) {
		g_dinputKeyboardDevice->lpVtbl->Release(g_dinputKeyboardDevice);
		g_dinputKeyboardDevice = NULL;
	}
	if (g_directInput) {
		g_directInput->lpVtbl->Release(g_directInput);
		g_directInput = NULL;
	}
}

// FUNCTION: XWA 0x42B880
int DInput_UpdateKeyboardModifierState(void) {
	unsigned char state[256];
	HRESULT result;

	result = g_dinputKeyboardDevice->lpVtbl->GetDeviceState(g_dinputKeyboardDevice, 256, state);
	if (!result) {
		g_dinputShiftDown = (state[42] != 0 || state[54] != 0);
		g_dinputCtrlDown = (state[29] != 0 || state[157] != 0);
		g_dinputAltDown = (state[56] != 0 || state[184] != 0);
	}
	return result;
}

// FUNCTION: XWA 0x42AF80
char DInput_Init(void) {
	DxGuid keyboardGuid;
	DxGuid mouseGuid;
	DIPROPDWORD keyboardProp;
	DIPROPDWORD mouseProp;
	HRESULT hr;

	keyboardGuid = GUID_SysKeyboard;
	mouseGuid = GUID_SysMouse;

	if (g_directInput || g_dinputKeyboardDevice || g_dinputMouseDevice || g_dinputKeyboardAcquired ||
		g_dinputMouseAcquired) {
		DebugPrintf("InitInput() called but some objects already initialized");
	}

	if (DirectInputCreateA(DINPUT_HINSTANCE, 0x0500u, &g_directInput, NULL) &&
		DirectInputCreateA(DINPUT_HINSTANCE, 0x0300u, &g_directInput, NULL)) {
		DINPUT_OUTPUT_DEBUG_STRING(g_strDiStrings[0]);
		if (g_dinputMouseDevice) {
			if (g_dinputMouseAcquired) {
				g_dinputMouseDevice->lpVtbl->Unacquire(g_dinputMouseDevice);
				g_dinputMouseAcquired = 0;
			}
			if (g_dinputMouseDevice) {
				g_dinputMouseDevice->lpVtbl->Release(g_dinputMouseDevice);
				g_dinputMouseDevice = NULL;
			}
		}
		if (g_dinputKeyboardDevice) {
			if (g_dinputKeyboardAcquired) {
				g_dinputKeyboardDevice->lpVtbl->Unacquire(g_dinputKeyboardDevice);
				g_dinputKeyboardAcquired = 0;
			}
			if (g_dinputKeyboardDevice) {
				g_dinputKeyboardDevice->lpVtbl->Release(g_dinputKeyboardDevice);
				g_dinputKeyboardDevice = NULL;
			}
		}
		if (g_directInput) {
			g_directInput->lpVtbl->Release(g_directInput);
			g_directInput = NULL;
		}
		return 0;
	}

	if (g_directInput->lpVtbl->CreateDevice(g_directInput, &keyboardGuid, &g_dinputKeyboardDevice, NULL)) {
		DINPUT_OUTPUT_DEBUG_STRING(g_strDiStrings[1]);
		if (g_dinputMouseDevice) {
			if (g_dinputMouseAcquired) {
				g_dinputMouseDevice->lpVtbl->Unacquire(g_dinputMouseDevice);
				g_dinputMouseAcquired = 0;
			}
			if (g_dinputMouseDevice) {
				g_dinputMouseDevice->lpVtbl->Release(g_dinputMouseDevice);
				g_dinputMouseDevice = NULL;
			}
		}
		if (g_dinputKeyboardDevice) {
			if (g_dinputKeyboardAcquired) {
				g_dinputKeyboardDevice->lpVtbl->Unacquire(g_dinputKeyboardDevice);
				g_dinputKeyboardAcquired = 0;
			}
			if (g_dinputKeyboardDevice) {
				g_dinputKeyboardDevice->lpVtbl->Release(g_dinputKeyboardDevice);
				g_dinputKeyboardDevice = NULL;
			}
		}
		if (g_directInput) {
			g_directInput->lpVtbl->Release(g_directInput);
			g_directInput = NULL;
		}
		return 0;
	}
	if (g_dinputKeyboardDevice->lpVtbl->SetDataFormat(g_dinputKeyboardDevice, &c_dfDIKeyboard)) {
		DINPUT_OUTPUT_DEBUG_STRING(g_strDiStrings[2]);
		if (g_dinputMouseDevice) {
			if (g_dinputMouseAcquired) {
				g_dinputMouseDevice->lpVtbl->Unacquire(g_dinputMouseDevice);
				g_dinputMouseAcquired = 0;
			}
			if (g_dinputMouseDevice) {
				g_dinputMouseDevice->lpVtbl->Release(g_dinputMouseDevice);
				g_dinputMouseDevice = NULL;
			}
		}
		if (g_dinputKeyboardDevice) {
			if (g_dinputKeyboardAcquired) {
				g_dinputKeyboardDevice->lpVtbl->Unacquire(g_dinputKeyboardDevice);
				g_dinputKeyboardAcquired = 0;
			}
			if (g_dinputKeyboardDevice) {
				g_dinputKeyboardDevice->lpVtbl->Release(g_dinputKeyboardDevice);
				g_dinputKeyboardDevice = NULL;
			}
		}
		if (g_directInput) {
			g_directInput->lpVtbl->Release(g_directInput);
			g_directInput = NULL;
		}
		return 0;
	}
	if (g_dinputKeyboardDevice->lpVtbl->SetCooperativeLevel(g_dinputKeyboardDevice, DINPUT_HWND,
															DISCL_FOREGROUND | DISCL_NONEXCLUSIVE)) {
		DINPUT_OUTPUT_DEBUG_STRING(g_strDiStrings[3]);
		if (g_dinputMouseDevice) {
			if (g_dinputMouseAcquired) {
				g_dinputMouseDevice->lpVtbl->Unacquire(g_dinputMouseDevice);
				g_dinputMouseAcquired = 0;
			}
			if (g_dinputMouseDevice) {
				g_dinputMouseDevice->lpVtbl->Release(g_dinputMouseDevice);
				g_dinputMouseDevice = NULL;
			}
		}
		if (g_dinputKeyboardDevice) {
			if (g_dinputKeyboardAcquired) {
				g_dinputKeyboardDevice->lpVtbl->Unacquire(g_dinputKeyboardDevice);
				g_dinputKeyboardAcquired = 0;
			}
			if (g_dinputKeyboardDevice) {
				g_dinputKeyboardDevice->lpVtbl->Release(g_dinputKeyboardDevice);
				g_dinputKeyboardDevice = NULL;
			}
		}
		if (g_directInput) {
			g_directInput->lpVtbl->Release(g_directInput);
			g_directInput = NULL;
		}
		return 0;
	}
	keyboardProp.diph.dwSize = sizeof(DIPROPDWORD);
	keyboardProp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	keyboardProp.diph.dwObj = 0;
	keyboardProp.diph.dwHow = 0;
	keyboardProp.dwData = 32;
	hr = g_dinputKeyboardDevice->lpVtbl->SetProperty(g_dinputKeyboardDevice, DINPUT_DIPROP_BUFFERSIZE,
													 &keyboardProp.diph);
	if (hr < 0) {
		DINPUT_OUTPUT_DEBUG_STRING(g_strDiStrings[4]);
		if (g_dinputMouseDevice) {
			if (g_dinputMouseAcquired) {
				g_dinputMouseDevice->lpVtbl->Unacquire(g_dinputMouseDevice);
				g_dinputMouseAcquired = 0;
			}
			if (g_dinputMouseDevice) {
				g_dinputMouseDevice->lpVtbl->Release(g_dinputMouseDevice);
				g_dinputMouseDevice = NULL;
			}
		}
		if (g_dinputKeyboardDevice) {
			if (g_dinputKeyboardAcquired) {
				g_dinputKeyboardDevice->lpVtbl->Unacquire(g_dinputKeyboardDevice);
				g_dinputKeyboardAcquired = 0;
			}
			if (g_dinputKeyboardDevice) {
				g_dinputKeyboardDevice->lpVtbl->Release(g_dinputKeyboardDevice);
				g_dinputKeyboardDevice = NULL;
			}
		}
		if (g_directInput) {
			g_directInput->lpVtbl->Release(g_directInput);
			g_directInput = NULL;
		}
		return 0;
	}
	if (g_dinputKeyboardDevice->lpVtbl->Acquire(g_dinputKeyboardDevice)) {
		DINPUT_OUTPUT_DEBUG_STRING(g_strDiStrings[5]);
		if (g_dinputMouseDevice) {
			if (g_dinputMouseAcquired) {
				g_dinputMouseDevice->lpVtbl->Unacquire(g_dinputMouseDevice);
				g_dinputMouseAcquired = 0;
			}
			if (g_dinputMouseDevice) {
				g_dinputMouseDevice->lpVtbl->Release(g_dinputMouseDevice);
				g_dinputMouseDevice = NULL;
			}
		}
		if (g_dinputKeyboardDevice) {
			if (g_dinputKeyboardAcquired) {
				g_dinputKeyboardDevice->lpVtbl->Unacquire(g_dinputKeyboardDevice);
				g_dinputKeyboardAcquired = 0;
			}
			if (g_dinputKeyboardDevice) {
				g_dinputKeyboardDevice->lpVtbl->Release(g_dinputKeyboardDevice);
				g_dinputKeyboardDevice = NULL;
			}
		}
		if (g_directInput) {
			g_directInput->lpVtbl->Release(g_directInput);
			g_directInput = NULL;
		}
		return 0;
	}
	g_dinputKeyboardAcquired = 1;

	hr = g_directInput->lpVtbl->CreateDevice(g_directInput, &mouseGuid, &g_dinputMouseDevice, NULL);
	if (hr < 0) {
		DINPUT_OUTPUT_DEBUG_STRING(g_strDiStrings[6]);
		if (g_dinputMouseDevice) {
			if (g_dinputMouseAcquired) {
				g_dinputMouseDevice->lpVtbl->Unacquire(g_dinputMouseDevice);
				g_dinputMouseAcquired = 0;
			}
			if (g_dinputMouseDevice) {
				g_dinputMouseDevice->lpVtbl->Release(g_dinputMouseDevice);
				g_dinputMouseDevice = NULL;
			}
		}
		if (g_dinputKeyboardDevice) {
			if (g_dinputKeyboardAcquired) {
				g_dinputKeyboardDevice->lpVtbl->Unacquire(g_dinputKeyboardDevice);
				g_dinputKeyboardAcquired = 0;
			}
			if (g_dinputKeyboardDevice) {
				g_dinputKeyboardDevice->lpVtbl->Release(g_dinputKeyboardDevice);
				g_dinputKeyboardDevice = NULL;
			}
		}
		if (g_directInput) {
			g_directInput->lpVtbl->Release(g_directInput);
			g_directInput = NULL;
		}
		return 0;
	}
	hr = g_dinputMouseDevice->lpVtbl->SetDataFormat(g_dinputMouseDevice, &c_dfDIMouse);
	if (hr < 0) {
		DINPUT_OUTPUT_DEBUG_STRING(g_strDiStrings[7]);
		if (g_dinputMouseDevice) {
			if (g_dinputMouseAcquired) {
				g_dinputMouseDevice->lpVtbl->Unacquire(g_dinputMouseDevice);
				g_dinputMouseAcquired = 0;
			}
			if (g_dinputMouseDevice) {
				g_dinputMouseDevice->lpVtbl->Release(g_dinputMouseDevice);
				g_dinputMouseDevice = NULL;
			}
		}
		if (g_dinputKeyboardDevice) {
			if (g_dinputKeyboardAcquired) {
				g_dinputKeyboardDevice->lpVtbl->Unacquire(g_dinputKeyboardDevice);
				g_dinputKeyboardAcquired = 0;
			}
			if (g_dinputKeyboardDevice) {
				g_dinputKeyboardDevice->lpVtbl->Release(g_dinputKeyboardDevice);
				g_dinputKeyboardDevice = NULL;
			}
		}
		if (g_directInput) {
			g_directInput->lpVtbl->Release(g_directInput);
			g_directInput = NULL;
		}
		return 0;
	}
	hr = g_dinputMouseDevice->lpVtbl->SetCooperativeLevel(g_dinputMouseDevice, DINPUT_HWND,
														  DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	if (hr < 0) {
		DINPUT_OUTPUT_DEBUG_STRING(g_strDiStrings[8]);
		DInput_Shutdown();
		return 0;
	}
	mouseProp.diph.dwSize = sizeof(DIPROPDWORD);
	mouseProp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	mouseProp.diph.dwObj = 0;
	mouseProp.diph.dwHow = 0;
	mouseProp.dwData = 0;
	hr = g_dinputMouseDevice->lpVtbl->SetProperty(g_dinputMouseDevice, DINPUT_DIPROP_BUFFERSIZE,
												  &mouseProp.diph);
	if (hr < 0) {
		DINPUT_OUTPUT_DEBUG_STRING(g_strDiStrings[9]);
		DInput_Shutdown();
		return 0;
	}
	hr = g_dinputMouseDevice->lpVtbl->Acquire(g_dinputMouseDevice);
	if (hr < 0) {
		DINPUT_OUTPUT_DEBUG_STRING(g_strDiStrings[10]);
		DInput_Shutdown();
		return 0;
	}
	g_dinputMouseAcquired = 1;

	ForceFeedback_Init();
	DInput_UpdateKeyboardModifierState();
	return 1;
}

// FUNCTION: XWA 0x42B900
int DInput_ReadKeyboardState(void) {
	return g_dinputKeyboardDevice->lpVtbl->GetDeviceState(g_dinputKeyboardDevice, 256, g_dinputKeyboardState);
}

// FUNCTION: XWA 0x42B680
int DInput_DrainKeyboardEvents(void) {
	unsigned char state[256];
	uint32_t flushCount;
	int result;

	/* Flush the buffered keyboard events (INFINITE), then refresh the immediate state
	 * to recompute the cached Ctrl/Shift/Alt flags. */
	flushCount = 0xFFFFFFFFu;
	result = g_dinputKeyboardDevice->lpVtbl->GetDeviceData(
		g_dinputKeyboardDevice, (uint32_t)sizeof(DIDEVICEOBJECTDATA), NULL, &flushCount, 0);
	if (!g_dinputKeyboardDevice->lpVtbl->GetDeviceState(g_dinputKeyboardDevice, 256, state)) {
		g_dinputShiftDown = (state[42] != 0 || state[54] != 0);
		g_dinputCtrlDown = (state[29] != 0 || state[157] != 0);
		g_dinputAltDown = (state[56] != 0 || state[184] != 0);
	}
	return result >= 0;
}

// FUNCTION: XWA 0x42B740
uint16_t DInput_GetKey(void) {
	/* The event buffer holds two elements even though only one event is
	 * requested at a time; only keyEvent[0] is ever read. */
	DIDEVICEOBJECTDATA keyEvent[2];
	uint32_t eventCount;
	int diStatus;
	uint16_t code;
	int pressed;

	while (1) {
		/* Read one buffered event, reacquiring on input loss. */
		while (1) {
			eventCount = 1;
			diStatus = g_dinputKeyboardDevice->lpVtbl->GetDeviceData(
				g_dinputKeyboardDevice, (uint32_t)sizeof(DIDEVICEOBJECTDATA), keyEvent, &eventCount, 0);
			if (diStatus != DIERR_INPUTLOST) {
				break;
			}
			if (!g_dinputKeyboardDevice || !g_dinputMouseDevice ||
				((diStatus = g_dinputKeyboardDevice->lpVtbl->Acquire(g_dinputKeyboardDevice)) != DI_OK &&
				 diStatus != DI_NOEFFECT) ||
				((diStatus = g_dinputMouseDevice->lpVtbl->Acquire(g_dinputMouseDevice)) != DI_OK &&
				 diStatus != DI_NOEFFECT)) {
				return 0;
			}
		}
		if (diStatus < 0) {
			return 0;
		}
		if (eventCount == 0) {
			continue; /* no event dequeued: retry */
		}

		pressed = keyEvent[0].dwData & 0x80;
		code = g_dinputKeyCodeTable[keyEvent[0].dwOfs & 0xFF];
		if (code == 0x00fd) {
			g_dinputShiftDown = pressed;
			continue;
		}
		if (code == 0x00fe) {
			g_dinputCtrlDown = pressed;
			continue;
		}
		if (code == 0x00ff) {
			g_dinputAltDown = pressed;
			continue;
		}
		if (pressed == 0) {
			continue; /* key-up: keep scanning */
		}

		if (g_dinputShiftDown) {
			return g_dinputShiftKeyCodeTable[keyEvent[0].dwOfs & 0xFF];
		}
		if (g_dinputCtrlDown) {
			return g_dinputCtrlKeyCodeTable[keyEvent[0].dwOfs & 0xFF];
		}
		if (g_dinputAltDown) {
			return g_dinputAltKeyCodeTable[keyEvent[0].dwOfs & 0xFF];
		}
		return g_dinputKeyCodeTable[keyEvent[0].dwOfs & 0xFF];
	}
}

// FUNCTION: XWA 0x42B520
int DInput_HasKeyReady(void) {
	DIDEVICEOBJECTDATA keyEvent[2];
	uint32_t eventCount;
	int diStatus;
	uint16_t code;
	int pressed;

	while (1) {
		/* Peek the front event, reacquiring on input loss. */
		while (1) {
			eventCount = 1;
			diStatus = g_dinputKeyboardDevice->lpVtbl->GetDeviceData(g_dinputKeyboardDevice,
																	 (uint32_t)sizeof(DIDEVICEOBJECTDATA),
																	 keyEvent, &eventCount, DIGDD_PEEK);
			if (diStatus != DIERR_INPUTLOST) {
				break;
			}
			if (!g_dinputKeyboardDevice || !g_dinputMouseDevice ||
				((diStatus = g_dinputKeyboardDevice->lpVtbl->Acquire(g_dinputKeyboardDevice)) != DI_OK &&
				 diStatus != DI_NOEFFECT) ||
				((diStatus = g_dinputMouseDevice->lpVtbl->Acquire(g_dinputMouseDevice)) != DI_OK &&
				 diStatus != DI_NOEFFECT)) {
				return 0;
			}
		}
		if (diStatus < 0 || eventCount == 0) {
			return 0;
		}

		pressed = keyEvent[0].dwData & 0x80;
		code = g_dinputKeyCodeTable[keyEvent[0].dwOfs & 0xFF];
		if (code == 0x00fd) {
			g_dinputShiftDown = pressed;
			eventCount = 1;
			g_dinputKeyboardDevice->lpVtbl->GetDeviceData(
				g_dinputKeyboardDevice, (uint32_t)sizeof(DIDEVICEOBJECTDATA), keyEvent, &eventCount, 0);
			continue;
		}
		if (code == 0x00fe) {
			g_dinputCtrlDown = pressed;
			eventCount = 1;
			g_dinputKeyboardDevice->lpVtbl->GetDeviceData(
				g_dinputKeyboardDevice, (uint32_t)sizeof(DIDEVICEOBJECTDATA), keyEvent, &eventCount, 0);
			continue;
		}
		if (code == 0x00ff) {
			g_dinputAltDown = pressed;
			eventCount = 1;
			g_dinputKeyboardDevice->lpVtbl->GetDeviceData(
				g_dinputKeyboardDevice, (uint32_t)sizeof(DIDEVICEOBJECTDATA), keyEvent, &eventCount, 0);
			continue;
		}
		if (pressed) {
			break; /* a non-modifier key-down is queued; leave it for DInput_GetKey */
		}

		/* Modifier make/break or key-up: consume it and keep scanning. */
		eventCount = 1;
		g_dinputKeyboardDevice->lpVtbl->GetDeviceData(
			g_dinputKeyboardDevice, (uint32_t)sizeof(DIDEVICEOBJECTDATA), keyEvent, &eventCount, 0);
	}
	return 1;
}

// FUNCTION: XWA 0x42B920
void DInput_PollMouseState(void) {
	HRESULT result;
	unsigned char acquired;

	while (1) {
		result = g_dinputMouseDevice->lpVtbl->GetDeviceState(
			g_dinputMouseDevice, (uint32_t)sizeof(DIMOUSESTATE), &g_dinputMouseState);
		if (result != DIERR_INPUTLOST) {
			break;
		}
		/* Input lost: reacquire both devices and retry. */
		acquired = g_dinputKeyboardDevice && g_dinputMouseDevice &&
				   ((result = g_dinputKeyboardDevice->lpVtbl->Acquire(g_dinputKeyboardDevice)) == DI_OK ||
					result == DI_NOEFFECT) &&
				   ((result = g_dinputMouseDevice->lpVtbl->Acquire(g_dinputMouseDevice)) == DI_OK ||
					result == DI_NOEFFECT);
		if (!acquired) {
			break;
		}
	}
}
