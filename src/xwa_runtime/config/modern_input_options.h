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

/* Position: mouse displacement sets a held virtual-stick deflection.
 * Rate: per-frame mouse velocity is the deflection (the TIE Fighter
 * scheme). */
typedef enum XwaModernMouseMode {
	XWA_MODERN_MOUSE_MODE_POSITION = 0,
	XWA_MODERN_MOUSE_MODE_RATE,
} XwaModernMouseMode;

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
	XwaControllerOptions controllers;
	/* Sorted with XwaKeyboardMapping_Sort. */
	XwaKeyboardBindings keyboard;
} XwaModernInputOptions;

typedef void (*XwaModernInputOptionsApplyFn)(const XwaModernInputOptions* options);
typedef int (*XwaModernInputOptionsPersistFn)(const XwaModernInputOptions* options, char* error,
											  size_t error_size);

/* `gamepad_defaults` is the profile that new gamepad models get
 * (input.gamepad_defaults in the shipped file). */
void XwaModernInputOptions_Configure(const XwaModernInputOptions* defaults,
									 const XwaControllerProfile* gamepad_defaults,
									 const XwaModernInputOptions* options, XwaModernInputOptionsApplyFn apply,
									 XwaModernInputOptionsPersistFn persist);
int XwaModernInputOptions_Validate(const XwaModernInputOptions* options, char* error, size_t error_size);
/* Whether two option sets are the same. Both keyboards must be sorted. */
int XwaModernInputOptions_Equal(const XwaModernInputOptions* a, const XwaModernInputOptions* b);
void XwaModernInputOptions_Get(XwaModernInputOptions* out);
void XwaModernInputOptions_GetDefaults(XwaModernInputOptions* out);
const XwaControllerProfile* XwaModernInputOptions_GamepadDefaults(void);
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
