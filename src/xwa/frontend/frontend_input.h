#ifndef XWA_FRONTEND_FRONTEND_INPUT_H
#define XWA_FRONTEND_FRONTEND_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Per-frame mouse click latches, one byte per event, packed at 0x9F6884.
// Set when the matching button event fires, read by the FrontendMouse_Get*
// accessors, and cleared each frame.
typedef struct {
	unsigned char leftClick;
	unsigned char rightClick;
	unsigned char leftDblClick;
	unsigned char rightDblClick;
} MouseClickLatch;

typedef struct {
	unsigned char held[64];
	unsigned char released[64];
} JoystickButtonState;

typedef struct {
	unsigned int xMin[2];
	unsigned int xMax[2];
	unsigned int yMin[2];
	unsigned int yMax[2];
	unsigned int xCenter[2];
	unsigned int yCenter[2];
	int xNegativeScale[2];
	int xPositiveScale[2];
	int yNegativeScale[2];
	int yPositiveScale[2];
} JoystickCalibrationState;

#pragma pack(push, 1)
typedef struct {
	unsigned int deviceIds[2];
	unsigned char initFlags[2];
	unsigned char present[2];
	unsigned char hasPov[2];
	unsigned char buttonCount[2];
	JoystickButtonState buttons;
	unsigned char povDirection[2];
	unsigned char preferredId;
	int axisX[2];
	int axisY[2];
	JoystickCalibrationState calibration;
} JoystickFrontendState;
#pragma pack(pop)

extern int g_mouseInputGate;
extern MouseClickLatch g_mouseClickLatch;
extern unsigned char g_mouseLeftDown;
extern unsigned char g_mouseRightDown;
extern int g_charReadIdx;
extern int g_charWriteIdx;
extern unsigned char g_charRingBuffer[1024];
extern JoystickFrontendState g_joystickState;
extern unsigned char KeyState[256];
extern int g_joystickDetectionCached;
extern int g_joystickDetectionProbeInProgress;
extern int g_joystickActive;
extern int g_joyDeviceIndex;

int FrontendMouse_SetInputGate(int gateId);
int FrontendMouse_ClearInputGate(void);
int FrontendMouse_GetLeftDown(void);
int FrontendMouse_GetRightDown(void);
int FrontendMouse_GetLeftClick(void);
int FrontendMouse_GetRightClick(void);
int FrontendMouse_GetLeftClickFor(int gateId);
int FrontendMouse_GetRightClickFor(int gateId);
int FrontendMouse_IsGateOwner(int gateId);
int FrontendMouse_IsGateOpen(void);
int FrontendMouse_ClearClicks(void);
int FrontendMouse_GetLeftDblClick(void);
int FrontendMouse_GetRightDblClick(void);
char Keyboard_PeekChar(void);
char Keyboard_DequeueChar(void);
int Keyboard_DiscardChar(void);
int Keyboard_FlushCharBuffer(void);
int Keyboard_BufferContains(char ch);
int Keyboard_IsKeyDown(unsigned char virtualKey);
int Joystick_InitDevices(void);
void Joystick_ReinitializeDevices(void);
unsigned int Joystick_GetDeviceId(int joySlot);
void Joystick_UpdateState(int joySlot);
int Joystick_GetCount(void);
int Joystick_GetButtonCount(int joySlot);
int Joystick_HasPov(int joySlot);
int Joystick_GetPovDirection(int joySlot);
int Joystick_GetFirstPressedButton(int joySlot);
void Joystick_PollRawAxes(int deviceId, int* pAxisX, int* pAxisY, int* pAxisZ, int* pAxisR, int* pButtons);
int Input_DetectActiveJoystick(void);
int Joystick_PollRawAxesIfEnabled(int* pAxisX, int* pAxisY, int* pAxisZ, int* pAxisR, int unused);

#ifdef __cplusplus
}
#endif

#endif
