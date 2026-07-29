#ifndef XWA_RUNTIME_MODERN_INPUT_OPTIONS_H
#define XWA_RUNTIME_MODERN_INPUT_OPTIONS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	XWA_MODERN_MOUSE_SENSITIVITY_MIN = 1,
	XWA_MODERN_MOUSE_SENSITIVITY_MAX = 9,
};

/* Position: mouse displacement sets a held virtual-stick deflection.
 * Rate: per-frame mouse velocity is the deflection (the TIE Fighter
 * scheme). */
typedef enum XwaModernMouseMode {
	XWA_MODERN_MOUSE_MODE_POSITION = 0,
	XWA_MODERN_MOUSE_MODE_RATE,
} XwaModernMouseMode;

typedef struct XwaModernInputOptions {
	int mouse_flight_enabled;
	XwaModernMouseMode mouse_mode;
	/* 1..9 doubling steps. Position mode: mouse travel for full deflection
	 * (~256 px at notch 5, halved per notch up). Rate mode: velocity gain
	 * (notch 5 = TIE parity). */
	int mouse_sensitivity;
	int mouse_invert_y;
} XwaModernInputOptions;

typedef void (*XwaModernInputOptionsApplyFn)(const XwaModernInputOptions* options);
typedef int (*XwaModernInputOptionsPersistFn)(const XwaModernInputOptions* options, char* error,
											  size_t error_size);

void XwaModernInputOptions_Configure(const XwaModernInputOptions* options, XwaModernInputOptionsApplyFn apply,
									 XwaModernInputOptionsPersistFn persist);
void XwaModernInputOptions_Get(XwaModernInputOptions* out);
int XwaModernInputOptions_Set(const XwaModernInputOptions* options);
int XwaModernInputOptions_Flush(void);
int XwaModernInputOptions_IsDirty(void);

#ifdef __cplusplus
}
#endif

#endif
