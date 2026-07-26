#include "xwa/frontend/frontend_input.h"

#include "xwa_runtime/compat/winmm/joystick.h"
#include "xwa/config/game_config.h"

#include <string.h>

typedef MMRESULT(XWA_WINMMAPI* JoyGetPosExFunc)(uint32_t deviceId, JOYINFOEX* joyInfo);
typedef MMRESULT(XWA_WINMMAPI* JoyGetDevCapsFunc)(uint32_t deviceId, JOYCAPSA* joyCaps, uint32_t joyCapsSize);
typedef uint32_t(XWA_WINMMAPI* JoyGetNumDevsFunc)(void);

// GLOBAL: XWA 0x5A92A4
JoyGetPosExFunc g_joyGetPosEx = joyGetPosEx;
// GLOBAL: XWA 0x5A92A8
JoyGetDevCapsFunc g_joyGetDevCaps = joyGetDevCapsA;
// GLOBAL: XWA 0x5A92B4
JoyGetNumDevsFunc g_joyGetNumDevs = joyGetNumDevs;

// GLOBAL: XWA 0x9F6888
int g_mouseInputGate = 0;
// GLOBAL: XWA 0x9F6884
MouseClickLatch g_mouseClickLatch;
// GLOBAL: XWA 0x9F6F83
int g_charReadIdx = 0;
// GLOBAL: XWA 0x9F6F7F
int g_charWriteIdx = 0;
// GLOBAL: XWA 0x9F6B7F
unsigned char g_charRingBuffer[1024];
// GLOBAL: XWA 0x9F697F
unsigned char KeyState[256];
// GLOBAL: XWA 0x9F688C
JoystickFrontendState g_joystickState;
// GLOBAL: XWA 0x781E68
int g_joystickDetectionCached;
// GLOBAL: XWA 0x781E6C
int g_joystickDetectionProbeInProgress;
// GLOBAL: XWA 0x781E70
int g_joystickActive;
// GLOBAL: XWA 0x781E74
int g_joyDeviceIndex;

/* Frontend joystick calibration/state, populated by Joystick_InitDevices and polled
 * by Joystick_UpdateState (WinMM slots 0/1). */
/* Flight joystick calibration, populated/polled by Joystick_PollRawAxes. */
// GLOBAL: XWA 0x7733A0
int g_joyCalibrated[2];
// GLOBAL: XWA 0x7733C8
int g_joyHasPOV;
// GLOBAL: XWA 0x5FFDF0
int g_joyRangeX;
// GLOBAL: XWA 0x5FFDF4
int g_joyRangeY;
// GLOBAL: XWA 0x5FFDF8
int g_joyRangeZ;
// GLOBAL: XWA 0x5FFDFC
int g_joyRangeR;
// GLOBAL: XWA 0x7733A8
int g_joyOffsetX;
// GLOBAL: XWA 0x7733AC
int g_joyOffsetY;
// GLOBAL: XWA 0x7733B0
int g_joyOffsetZ;
// GLOBAL: XWA 0x7733B4
int g_joyOffsetR;
// GLOBAL: XWA 0x7733B8
int g_joyDeadzoneX;
// GLOBAL: XWA 0x7733BC
int g_joyDeadzoneY;
// GLOBAL: XWA 0x7733C0
int g_joyDeadzoneZ;
// GLOBAL: XWA 0x7733C4
int g_joyDeadzoneR;
// GLOBAL: XWA 0x7712C0
unsigned int g_joyDeviceId[2];
// GLOBAL: XWA 0x7732E0
int g_joyCenterX;
// GLOBAL: XWA 0x7732E4
int g_joyCenterY;
// GLOBAL: XWA 0x7712B8
int g_joyCenterZ;
// GLOBAL: XWA 0x7712C8
int g_joyCenterR;
// GLOBAL: XWA 0x5FFDA0
unsigned char g_joyRudderEnabled;
// GLOBAL: XWA 0x77332C
unsigned char g_joyInvertR;
// GLOBAL: XWA 0x773330
unsigned char g_joyInvertY;

// FUNCTION: XWA 0x558170
int FrontendMouse_SetInputGate(int gateId) {
	g_mouseInputGate = gateId;
	return 1;
}

// FUNCTION: XWA 0x558180
int FrontendMouse_ClearInputGate(void) {
	g_mouseInputGate = 0;
	return 1;
}

// FUNCTION: XWA 0x558190
int FrontendMouse_GetLeftDown(void) {
	if (g_mouseInputGate) {
		return 0;
	}

	return g_mouseLeftDown;
}

// FUNCTION: XWA 0x5581B0
int FrontendMouse_GetRightDown(void) {
	if (g_mouseInputGate) {
		return 0;
	}

	return g_mouseRightDown;
}

// FUNCTION: XWA 0x5581D0
int FrontendMouse_GetLeftClick(void) {
	if (g_mouseInputGate) {
		return 0;
	}

	return g_mouseClickLatch.leftClick;
}

// FUNCTION: XWA 0x5581F0
int FrontendMouse_GetRightClick(void) {
	if (g_mouseInputGate) {
		return 0;
	}

	return g_mouseClickLatch.rightClick;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x558210
int FrontendMouse_GetLeftClickFor(int gateId) {
	if (g_mouseInputGate == gateId || !g_mouseInputGate) {
		return g_mouseClickLatch.leftClick;
	}

	return 0;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x558230
int FrontendMouse_GetRightClickFor(int gateId) {
	if (g_mouseInputGate == gateId || !g_mouseInputGate) {
		return g_mouseClickLatch.rightClick;
	}

	return 0;
}

// FUNCTION: XWA 0x558250
int FrontendMouse_IsGateOwner(int gateId) { return g_mouseInputGate == gateId; }

// FUNCTION: XWA 0x558270
int FrontendMouse_IsGateOpen(void) { return g_mouseInputGate == 0; }

// FUNCTION: XWA 0x558280
int FrontendMouse_ClearClicks(void) {
	g_mouseClickLatch.leftClick = 0;
	g_mouseClickLatch.rightClick = 0;
	return 1;
}

// FUNCTION: XWA 0x5582A0
int FrontendMouse_GetLeftDblClick(void) {
	if (g_mouseInputGate) {
		return 0;
	}

	return g_mouseClickLatch.leftDblClick;
}

// FUNCTION: XWA 0x5582C0
int FrontendMouse_GetRightDblClick(void) {
	if (g_mouseInputGate) {
		return 0;
	}

	return g_mouseClickLatch.rightDblClick;
}

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x55B570
char Keyboard_PeekChar(void) {
	if (g_charReadIdx == g_charWriteIdx) {
		return 0;
	}

	return (char)g_charRingBuffer[g_charReadIdx];
}

// FUNCTION: XWA 0x55B4F0
char Keyboard_DequeueChar(void) {
	char result;

	if (g_charReadIdx == g_charWriteIdx) {
		return 0;
	}

	result = (char)g_charRingBuffer[g_charReadIdx++];
	if (g_charReadIdx == 1024) {
		g_charReadIdx = 0;
	}

	return result;
}

// FUNCTION: XWA 0x55B5B0
int Keyboard_DiscardChar(void) {
	int nextIndex;

	if (g_charReadIdx == g_charWriteIdx) {
		return 0;
	}

	nextIndex = g_charReadIdx + 1;
	g_charReadIdx = nextIndex;
	if (nextIndex == 1024) {
		g_charReadIdx = 0;
	}

	return 1;
}

// FUNCTION: XWA 0x55B590
int Keyboard_FlushCharBuffer(void) {
	g_charReadIdx = 0;
	g_charWriteIdx = 0;
	return 1;
}

// FUNCTION: XWA 0x541240
unsigned int Joystick_GetDeviceId(int joySlot) {
	if (joySlot > 2) {
		return 0;
	}
	return g_joystickState.deviceIds[joySlot];
}

// FUNCTION: XWA 0x540D40
int Joystick_InitDevices(void) {
	JOYINFOEX joyInfo;
	JOYCAPSA joyCaps;
	int numDevs;
	int initializedCount;
	int slot;
	int matchIdx;
	int dev;

	numDevs = (int)g_joyGetNumDevs();
	if (!numDevs) {
		return 0;
	}
	g_joystickState.initFlags[1] = 1;
	g_joystickState.initFlags[0] = 1;
	g_joystickState.present[1] = 0;
	g_joystickState.present[0] = 0;

	/* First pass: honor g_joystickState.preferredId (use the Nth detected device). */
	initializedCount = 0;
	slot = 0;
	matchIdx = 0;
	for (dev = 0; dev < numDevs; ++dev) {
		if (!g_joyGetDevCaps((uint32_t)dev, &joyCaps, sizeof(joyCaps))) {
			++matchIdx;
			if (!g_joystickState.preferredId || matchIdx == g_joystickState.preferredId) {
				g_joystickState.buttonCount[slot] = (unsigned char)joyCaps.wNumButtons;
				g_joystickState.hasPov[slot] = (joyCaps.wCaps & JOYCAPS_HASPOV) != 0;
				joyInfo.dwSize = sizeof(joyInfo);
				joyInfo.dwFlags = JOY_RETURNX | JOY_RETURNY | JOY_RETURNBUTTONS | JOY_RETURNCENTERED;
				if (!g_joyGetPosEx((uint32_t)dev, &joyInfo)) {
					g_joystickState.calibration.xMin[slot] = joyCaps.wXmin;
					g_joystickState.calibration.xMax[slot] = joyCaps.wXmax;
					g_joystickState.calibration.yMin[slot] = joyCaps.wYmin;
					g_joystickState.calibration.yMax[slot] = joyCaps.wYmax;
					g_joystickState.calibration.xCenter[slot] = joyInfo.dwXpos;
					g_joystickState.calibration.yCenter[slot] = joyInfo.dwYpos;
					g_joystickState.calibration.xNegativeScale[slot] = (joyInfo.dwXpos - joyCaps.wXmin) / 255;
					g_joystickState.calibration.xPositiveScale[slot] = (joyCaps.wXmax - joyInfo.dwXpos) / 255;
					g_joystickState.calibration.yNegativeScale[slot] = (joyInfo.dwYpos - joyCaps.wYmin) / 255;
					g_joystickState.calibration.yPositiveScale[slot] = (joyCaps.wYmax - joyInfo.dwYpos) / 255;
					g_joystickState.deviceIds[slot] = (unsigned int)dev;
					g_joystickState.present[slot] = 1;
					++initializedCount;
					if (++slot >= 2) {
						break;
					}
				}
			}
		}
	}

	/* Fallback pass: no preferred match found -- take the first available devices. */
	if (!initializedCount) {
		initializedCount = 0;
		slot = 0;
		for (dev = 0; dev < numDevs; ++dev) {
			if (!g_joyGetDevCaps((uint32_t)dev, &joyCaps, sizeof(joyCaps))) {
				g_joystickState.buttonCount[slot] = (unsigned char)joyCaps.wNumButtons;
				g_joystickState.hasPov[slot] = (joyCaps.wCaps & JOYCAPS_HASPOV) != 0;
				joyInfo.dwSize = sizeof(joyInfo);
				joyInfo.dwFlags = JOY_RETURNX | JOY_RETURNY | JOY_RETURNBUTTONS | JOY_RETURNCENTERED;
				if (!g_joyGetPosEx((uint32_t)dev, &joyInfo)) {
					g_joystickState.calibration.xMin[slot] = joyCaps.wXmin;
					g_joystickState.calibration.xMax[slot] = joyCaps.wXmax;
					g_joystickState.calibration.yMin[slot] = joyCaps.wYmin;
					g_joystickState.calibration.yMax[slot] = joyCaps.wYmax;
					g_joystickState.calibration.xCenter[slot] = joyInfo.dwXpos;
					g_joystickState.calibration.yCenter[slot] = joyInfo.dwYpos;
					g_joystickState.calibration.xNegativeScale[slot] = (joyInfo.dwXpos - joyCaps.wXmin) / 255;
					g_joystickState.calibration.xPositiveScale[slot] = (joyCaps.wXmax - joyInfo.dwXpos) / 255;
					g_joystickState.calibration.yNegativeScale[slot] = (joyInfo.dwYpos - joyCaps.wYmin) / 255;
					g_joystickState.calibration.yPositiveScale[slot] = (joyCaps.wYmax - joyInfo.dwYpos) / 255;
					g_joystickState.deviceIds[slot] = (unsigned int)dev;
					g_joystickState.present[slot] = 1;
					++initializedCount;
					if (++slot >= 2) {
						break;
					}
				}
			}
		}
	}
	return initializedCount != 0;
}

// FUNCTION: XWA 0x541050
void Joystick_UpdateState(int joySlot) {
	JOYINFOEX pji;
	unsigned int deviceId;
	int axisDeltaX;
	int axisDeltaY;
	unsigned int dwButtons;
	unsigned int buttonMask;
	int buttonIdx;

	if (joySlot > 1 || !g_joystickState.present[joySlot]) {
		return;
	}
	deviceId = g_joystickState.deviceIds[joySlot];
	pji.dwSize = sizeof(pji);
	pji.dwFlags = JOY_RETURNX | JOY_RETURNY | JOY_RETURNBUTTONS | JOY_RETURNPOV | JOY_RETURNCENTERED;
	g_joyGetPosEx(deviceId, &pji);

	axisDeltaX = (int)pji.dwXpos - (int)g_joystickState.calibration.xCenter[joySlot];
	axisDeltaY = (int)pji.dwYpos - (int)g_joystickState.calibration.yCenter[joySlot];
	if (axisDeltaX > 1000 || axisDeltaX < -1000) {
		g_joystickState.axisX[joySlot] =
			axisDeltaX < 0 ? axisDeltaX / g_joystickState.calibration.xNegativeScale[joySlot]
						   : axisDeltaX / g_joystickState.calibration.xPositiveScale[joySlot];
	} else {
		g_joystickState.axisX[joySlot] = 0;
	}
	if (axisDeltaY > 1000 || axisDeltaY < -1000) {
		g_joystickState.axisY[joySlot] =
			axisDeltaY < 0 ? axisDeltaY / g_joystickState.calibration.yNegativeScale[joySlot]
						   : axisDeltaY / g_joystickState.calibration.yPositiveScale[joySlot];
	} else {
		g_joystickState.axisY[joySlot] = 0;
	}

	dwButtons = pji.dwButtons;
	buttonMask = 1;
	for (buttonIdx = 0; buttonIdx < 32; ++buttonIdx) {
		g_joystickState.buttons.released[32 * joySlot + buttonIdx] =
			(dwButtons & buttonMask) == 0 && g_joystickState.buttons.held[32 * joySlot + buttonIdx] == 1;
		g_joystickState.buttons.held[32 * joySlot + buttonIdx] = (dwButtons & buttonMask) != 0;
		buttonMask <<= 1;
	}

	if (g_joystickState.hasPov[joySlot]) {
		if (pji.dwPOV == JOY_POVCENTERED) {
			g_joystickState.povDirection[joySlot] = 0;
		} else {
			g_joystickState.povDirection[joySlot] = (unsigned char)(pji.dwPOV / 0x2328u) + 1;
		}
	}
}

// FUNCTION: XWA 0x50B790
void Joystick_PollRawAxes(int deviceId, int* pAxisX, int* pAxisY, int* pAxisZ, int* pAxisR, int* pButtons) {
	JOYINFOEX pji;
	JOYCAPSA pjc;
	int absDelta;

	deviceId = deviceId != 0;

	/* One-time calibration from the device caps. */
	if (!g_joyCalibrated[deviceId]) {
		memset(&pjc, 0, sizeof(pjc));
		g_joyCalibrated[deviceId] = 1;
		if (!joyGetDevCapsA(Joystick_GetDeviceId(0), &pjc, sizeof(pjc))) {
			g_joyDeviceId[deviceId] = Joystick_GetDeviceId(0);
			g_joyHasPOV = (pjc.wCaps >> 4) & 1;
		} else {
			if (!joyGetDevCapsA(Joystick_GetDeviceId(1), &pjc, sizeof(pjc))) {
				g_joyDeviceId[deviceId] = Joystick_GetDeviceId(1);
				g_joyHasPOV = (pjc.wCaps >> 4) & 1;
			} else {
				goto noDevice;
			}
		}
		g_joyRangeX = (int)(pjc.wXmax - pjc.wXmin);
		g_joyRangeY = (int)(pjc.wYmax - pjc.wYmin);
		g_joyRangeZ = (int)(pjc.wZmax - pjc.wZmin);
		if (pjc.wCaps & JOYCAPS_HASR) {
			g_joyRangeR = (int)(pjc.wRmax - pjc.wRmin);
		} else {
			g_joyRangeR = 0;
			pjc.wRmin = 0;
			pjc.wRmax = 0;
		}
		g_joyOffsetX = (g_joyRangeX >> 1) - (int)pjc.wXmax;
		g_joyOffsetY = (g_joyRangeY >> 1) - (int)pjc.wYmax;
		g_joyOffsetZ = (g_joyRangeZ >> 1) - (int)pjc.wZmax;
		g_joyOffsetR = (g_joyRangeR >> 1) - (int)pjc.wRmax;
		g_joyDeadzoneX = g_joyRangeX / 20;
		g_joyDeadzoneY = g_joyRangeY / 20;
		g_joyDeadzoneZ = g_joyRangeZ / 20;
		g_joyDeadzoneR = g_joyRangeR / 10;
		g_joyCenterX = (g_joyRangeX >> 1) + (int)pjc.wXmin;
		g_joyCenterY = (g_joyRangeY >> 1) + (int)pjc.wYmin;
		g_joyCenterZ = (g_joyRangeZ >> 1) + (int)pjc.wZmin;
		g_joyCenterR = (g_joyRangeR >> 1) + (int)pjc.wRmin;
		goto poll;

	noDevice:
		g_joyHasPOV = 0;
		g_joyRangeX = 1;
		g_joyRangeY = 1;
		g_joyRangeZ = 1;
		g_joyRangeR = 1;
		g_joyOffsetX = 0;
		g_joyOffsetY = 0;
		g_joyOffsetZ = 0;
		g_joyOffsetR = 0;
		g_joyDeadzoneX = 0;
		g_joyDeadzoneY = 0;
		g_joyDeadzoneZ = 0;
		g_joyDeadzoneR = 0;
	}

poll:
	g_joyRudderEnabled = g_gameConfig.rudderEnabled;
	g_joyInvertR = g_gameConfig.flipRudder;
	g_joyInvertY = g_gameConfig.flipY;
	*pButtons = 0;
	memset(&pji, 0, sizeof(pji));
	pji.dwSize = sizeof(pji);
	pji.dwFlags = JOY_RETURNX | JOY_RETURNY | JOY_RETURNZ | JOY_RETURNR | JOY_RETURNBUTTONS | JOY_RETURNPOV |
				  JOY_RETURNCENTERED;
	if (!joyGetPosEx(g_joyDeviceId[deviceId], &pji)) {
		absDelta = (int)pji.dwXpos - g_joyCenterX;
		if (absDelta < 0) {
			absDelta = -absDelta;
		}
		if (absDelta > g_joyDeadzoneX) {
			*pAxisX = (int)(255u * (g_joyOffsetX + pji.dwXpos) / g_joyRangeX);
		} else {
			*pAxisX = 0;
		}

		absDelta = (int)pji.dwYpos - g_joyCenterY;
		if (absDelta < 0) {
			absDelta = -absDelta;
		}
		if (absDelta > g_joyDeadzoneY) {
			*pAxisY = (int)(255u * (g_joyOffsetY + pji.dwYpos) / g_joyRangeY);
			if (g_joyInvertY) {
				*pAxisY = -*pAxisY;
			}
		} else {
			*pAxisY = 0;
		}

		absDelta = (int)pji.dwZpos - g_joyCenterZ;
		if (absDelta < 0) {
			absDelta = -absDelta;
		}
		if (absDelta > g_joyDeadzoneZ && g_joyRangeZ > 0) {
			*pAxisZ = (int)(255u * (g_joyOffsetZ + pji.dwZpos) / g_joyRangeZ);
		} else {
			*pAxisZ = 0;
		}

		absDelta = (int)pji.dwRpos - g_joyCenterR;
		if (absDelta < 0) {
			absDelta = -absDelta;
		}
		if (absDelta > g_joyDeadzoneR && g_joyRangeR > 0 && g_joyRudderEnabled) {
			*pAxisR = (int)(255u * (g_joyOffsetR + pji.dwRpos) / g_joyRangeR);
			if (g_joyInvertR) {
				*pAxisR = -*pAxisR;
			}
		} else {
			*pAxisR = 0;
		}

		*pButtons = (int)(pji.dwButtons & 0xFFFF);
		if (g_joyHasPOV && pji.dwPOV != 0xFFFF) {
			*pButtons |= 0x10000 << (pji.dwPOV / 0x2328u);
		}
	} else {
		*pAxisX = 32000;
		*pAxisY = 32000;
		*pAxisZ = 32000;
		*pAxisR = 32000;
	}
}

// FUNCTION: XWA 0x50E480
int Input_DetectActiveJoystick(void) {
	int axisX;
	int axisY;
	int axisZ;
	int axisR;
	int buttons;

	if (!g_joystickDetectionCached) {
		g_joystickDetectionProbeInProgress = 1;
		Joystick_PollRawAxes(0, &axisX, &axisY, &axisZ, &axisR, &buttons);
		if (axisX == 32000 && axisY == 32000) {
			Joystick_PollRawAxes(1, &axisX, &axisY, &axisZ, &axisR, &buttons);
			if (axisX == 32000 && axisY == 32000) {
				g_joystickActive = 0;
			} else {
				g_joyDeviceIndex = 1;
				g_joystickActive = 1;
			}
		} else {
			g_joyDeviceIndex = 0;
			g_joystickActive = 1;
		}
		g_joystickDetectionCached = 1;
	}

	return g_joystickActive;
}

// FUNCTION: XWA 0x50E540
int Joystick_PollRawAxesIfEnabled(int* pAxisX, int* pAxisY, int* pAxisZ, int* pAxisR, int unused) {
	int buttons;

	(void)unused;
	if (!g_joystickActive) {
		*pAxisX = 0;
		*pAxisY = 0;
		*pAxisZ = 0;
		*pAxisR = 0;
		return 0;
	}

	Joystick_PollRawAxes(g_joyDeviceIndex, pAxisX, pAxisY, pAxisZ, pAxisR, &buttons);
	return buttons;
}

// FUNCTION: XWA 0x55B530
int Keyboard_BufferContains(char ch) {
	int index;
	int writeIdx;

	index = g_charReadIdx;
	writeIdx = g_charWriteIdx;
	while (index != writeIdx) {
		if (g_charRingBuffer[index] == (unsigned char)ch) {
			return 1;
		}
		++index;
		if (index == 1024) {
			index = 0;
		}
	}
	return 0;
}

// FUNCTION: XWA 0x55B4D0
int Keyboard_IsKeyDown(unsigned char virtualKey) { return KeyState[virtualKey] >> 7; }

// FUNCTION: XWA 0x541030
int Joystick_GetCount(void) {
	int result = 0;

	if (g_joystickState.present[0]) {
		result = 1;
	}
	if (g_joystickState.present[1]) {
		++result;
	}
	return result;
}

// FUNCTION: XWA 0x541220
int Joystick_GetButtonCount(int joySlot) {
	if (joySlot > 1) {
		return 0;
	}

	return g_joystickState.buttonCount[joySlot];
}

// FUNCTION: XWA 0x541200
int Joystick_HasPov(int joySlot) {
	if (joySlot > 1) {
		return 0;
	}

	return g_joystickState.hasPov[joySlot];
}

// FUNCTION: XWA 0x5411E0
int Joystick_GetPovDirection(int joySlot) {
	if (joySlot > 1) {
		return 0;
	}

	return g_joystickState.povDirection[joySlot];
}

// FUNCTION: XWA 0x5411B0
int Joystick_GetFirstPressedButton(int joySlot) {
	int buttonIdx;

	if (joySlot > 1 || !g_joystickState.present[joySlot]) {
		return -1;
	}
	for (buttonIdx = 0; buttonIdx < 32; ++buttonIdx) {
		if (g_joystickState.buttons.held[32 * joySlot + buttonIdx]) {
			return buttonIdx;
		}
	}
	return -1;
}
