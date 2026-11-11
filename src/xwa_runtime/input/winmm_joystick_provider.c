/* XWA controller source for the Aeron WinMM joystick shim. */

#include "xwa_runtime/input/winmm_joystick_provider.h"

#include "aeron/compat/host.h"
#include "xwa_runtime/input/controller_mapping.h"

#include <string.h>

static int XwaWinmmJoystick_Source(AeronWinmmJoystickState* out, void* user) {
	int axis;

	(void)user;
	if (!XwaControllerMapping_Present()) {
		return 0;
	}
	memset(out, 0, sizeof(*out));
	for (axis = 0; axis < 4; ++axis) {
		out->axes[axis] = 32768;
	}
	out->pov_direction = -1;
	out->name = "OpenXWA Controllers";
	return 1;
}

void XwaWinmmJoystick_RegisterSource(void) { AeronCompat_SetJoystickSource(XwaWinmmJoystick_Source, NULL); }
