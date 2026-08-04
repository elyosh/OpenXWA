#ifndef XWA_RUNTIME_MODERN_INPUT_OPTIONS_H
#define XWA_RUNTIME_MODERN_INPUT_OPTIONS_H

#include "aeron/input.h"

#include <stddef.h>
#include <stdint.h>

#define XWA_CONTROLLER_DIGITAL_THRESHOLD_DEFAULT 0.5f

#ifdef __cplusplus
extern "C" {
#endif

enum {
	XWA_MODERN_MOUSE_SENSITIVITY_MIN = 1,
	XWA_MODERN_MOUSE_SENSITIVITY_MAX = 9,
	XWA_CONTROLLER_LOGICAL_AXIS_COUNT = 4,
	XWA_CONTROLLER_LOGICAL_BUTTON_COUNT = 16,
	XWA_CONTROLLER_ACTION_COUNT = 20,
	XWA_CONTROLLER_RUMBLE_STRENGTH_MIN = 0,
	XWA_CONTROLLER_RUMBLE_STRENGTH_MAX = 8,
	XWA_CONTROLLER_DEVICE_ORDINAL_MAX = 15,
};

/* Position: mouse displacement sets a held virtual-stick deflection.
 * Rate: per-frame mouse velocity is the deflection (the TIE Fighter
 * scheme). */
typedef enum XwaModernMouseMode {
	XWA_MODERN_MOUSE_MODE_POSITION = 0,
	XWA_MODERN_MOUSE_MODE_RATE,
} XwaModernMouseMode;

typedef enum XwaControllerLogicalAxis {
	XWA_CONTROLLER_AXIS_YAW = 0,
	XWA_CONTROLLER_AXIS_PITCH,
	XWA_CONTROLLER_AXIS_THROTTLE,
	XWA_CONTROLLER_AXIS_ROLL,
} XwaControllerLogicalAxis;

typedef struct XwaControllerAxisBinding {
	/* Standardized AeronGamepadAxis or raw joystick axis index. -1 is
	 * unbound; the profile containing the binding determines the namespace. */
	int source;
	int invert;
	float deadzone;
} XwaControllerAxisBinding;

typedef struct XwaControllerProfile {
	XwaControllerAxisBinding axes[XWA_CONTROLLER_LOGICAL_AXIS_COUNT];
	/* Physical digital sources for logical WinMM buttons 1..16. */
	AeronControllerDigitalSource buttons[XWA_CONTROLLER_LOGICAL_BUTTON_COUNT];
	/* Gamepad profile: boolean D-pad-to-POV. Joystick profile: raw hat index,
	 * or -1 for no POV. */
	int pov_source;
	/* Actions paired with buttons 1..16 and POV up/right/down/left. Logical
	 * indexes remain an internal WinMM compatibility detail. */
	uint16_t actions[XWA_CONTROLLER_ACTION_COUNT];
} XwaControllerProfile;

typedef struct XwaControllerOptions {
	AeronControllerSelector device;
	int roll_enabled;
	int rumble_enabled;
	int rumble_strength;
	XwaControllerProfile gamepad;
	XwaControllerProfile joystick;
} XwaControllerOptions;

typedef struct XwaModernInputOptions {
	int mouse_flight_enabled;
	XwaModernMouseMode mouse_mode;
	/* 1..9 doubling steps. Position mode: mouse travel for full deflection
	 * (~256 px at notch 5, halved per notch up). Rate mode: velocity gain
	 * (notch 5 = TIE parity). */
	int mouse_sensitivity;
	int mouse_invert_y;
	XwaControllerOptions controller;
} XwaModernInputOptions;

typedef void (*XwaModernInputOptionsApplyFn)(const XwaModernInputOptions* options);
typedef int (*XwaModernInputOptionsPersistFn)(const XwaModernInputOptions* options, char* error,
											  size_t error_size);

void XwaModernInputOptions_Configure(const XwaModernInputOptions* defaults,
									 const XwaModernInputOptions* options, XwaModernInputOptionsApplyFn apply,
									 XwaModernInputOptionsPersistFn persist);
int XwaModernInputOptions_Validate(const XwaModernInputOptions* options);
void XwaModernInputOptions_Get(XwaModernInputOptions* out);
void XwaModernInputOptions_GetDefaults(XwaModernInputOptions* out);
int XwaModernInputOptions_Set(const XwaModernInputOptions* options);
int XwaModernInputOptions_RestoreControllerDefaults(void);
int XwaModernInputOptions_Flush(void);
int XwaModernInputOptions_IsDirty(void);

#ifdef __cplusplus
}
#endif

#endif
