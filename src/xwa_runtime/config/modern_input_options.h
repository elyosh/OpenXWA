#ifndef XWA_RUNTIME_MODERN_INPUT_OPTIONS_H
#define XWA_RUNTIME_MODERN_INPUT_OPTIONS_H

#include "xwa_runtime/input/controller_options.h"
#include "xwa_runtime/input/keyboard_mapping.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	XWA_MODERN_MOUSE_SENSITIVITY_MIN = 1,
	XWA_MODERN_MOUSE_SENSITIVITY_MAX = 9,
	XWA_MODERN_RUMBLE_STRENGTH_MIN = 0,
	XWA_MODERN_RUMBLE_STRENGTH_MAX = 8,
};

#define XWA_MODERN_VR_STICK_RANGE_MIN 0.5f
#define XWA_MODERN_VR_STICK_RANGE_MAX 2.0f
#define XWA_MODERN_VR_BUTTON_RAY_MIN_M 0.05f
#define XWA_MODERN_VR_BUTTON_RAY_MAX_M 5.0f

/* Position: mouse displacement sets a held virtual-stick deflection.
 * Rate: per-frame mouse velocity is the deflection (the TIE Fighter
 * scheme). */
typedef enum XwaModernMouseMode {
	XWA_MODERN_MOUSE_MODE_POSITION = 0,
	XWA_MODERN_MOUSE_MODE_RATE,
} XwaModernMouseMode;

/* The virtual stick of the VR controllers: while the squeeze is held, the
 * hand's movement steers the craft. Tilt reads the hand's rotation, and
 * Slide its movement to the sides and forward. */
typedef enum XwaModernVrStickMode {
	XWA_MODERN_VR_STICK_OFF = 0,
	XWA_MODERN_VR_STICK_TILT,
	XWA_MODERN_VR_STICK_SLIDE,
	XWA_MODERN_VR_STICK_MODE_COUNT,
} XwaModernVrStickMode;

/* The input section of config.yaml: mouse flight, rumble, the saved
 * controller models and the keyboard bindings. */
typedef struct XwaModernInputOptions {
	int mouse_flight_enabled;
	XwaModernMouseMode mouse_mode;
	/* 1..9 doubling steps. Position mode: mouse travel for full deflection
	 * (~256 px at notch 5, halved per notch up). Rate mode: velocity gain
	 * (notch 5 = TIE parity). */
	int mouse_sensitivity;
	int mouse_invert_y;
	int rumble_enabled;
	int rumble_strength;
	/* Draw gloves in the cockpit where the VR controllers are. */
	int vr_show_hands;
	/* Use the mod's gloves when it has some, and not the game's own. */
	int vr_mod_hands;
	XwaModernVrStickMode vr_stick;
	/* The hand that holds the virtual stick: 0 is the left one, 1 the right
	 * one. */
	int vr_stick_hand;
	/* The other hand's squeeze holds a virtual throttle, while the virtual
	 * stick is on. */
	int vr_throttle;
	/* Multiplies the hand travel that gives the stick's full deflection. */
	float vr_stick_range;
	/* A VR hand can point at the cockpit's buttons and press them, and how
	 * far from the fingertip a button can be, in metres. */
	int vr_cockpit_buttons;
	float vr_button_ray_m;
	XwaControllerOptions controllers;
	/* Sorted with XwaKeyboardMapping_Sort. */
	XwaKeyboardBindings keyboard;
} XwaModernInputOptions;

typedef void (*XwaModernInputOptionsApplyFn)(const XwaModernInputOptions* options);
typedef int (*XwaModernInputOptionsPersistFn)(const XwaModernInputOptions* options, char* error,
											  size_t error_size);

/* `controller_defaults` holds the profiles that new gamepad and VR hand
 * models get (input.gamepad_defaults, input.vr_left_defaults and
 * input.vr_right_defaults in the shipped file). */
void XwaModernInputOptions_Configure(const XwaModernInputOptions* defaults,
									 const XwaControllerDefaults* controller_defaults,
									 const XwaModernInputOptions* options, XwaModernInputOptionsApplyFn apply,
									 XwaModernInputOptionsPersistFn persist);
int XwaModernInputOptions_Validate(const XwaModernInputOptions* options, char* error, size_t error_size);
/* Whether two option sets are the same. Both keyboards must be sorted. */
int XwaModernInputOptions_Equal(const XwaModernInputOptions* a, const XwaModernInputOptions* b);
void XwaModernInputOptions_Get(XwaModernInputOptions* out);
void XwaModernInputOptions_GetDefaults(XwaModernInputOptions* out);
const XwaControllerDefaults* XwaModernInputOptions_ControllerDefaults(void);
int XwaModernInputOptions_Set(const XwaModernInputOptions* options);
/* The options must be saved even if they don't change, such as after the
 * configuration upgrade. */
void XwaModernInputOptions_MarkDirty(void);
int XwaModernInputOptions_Flush(void);
/* As Flush, but also gives the error. */
int XwaModernInputOptions_FlushWithError(char* error, size_t error_size);
int XwaModernInputOptions_IsDirty(void);

#ifdef __cplusplus
}
#endif

#endif
