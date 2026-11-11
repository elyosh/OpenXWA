/* XWA rumble provider for the Aeron DirectInput force-feedback shim. */

#include "xwa_runtime/input/rumble_provider.h"

#include "aeron/compat/host.h"
#include "xwa_runtime/input/controller_mapping.h"

static int XwaRumble_HasRumble(void* user) {
	(void)user;
	return XwaControllerMapping_RumbleInstance() != 0;
}

static uint32_t XwaRumble_ControllerInstanceId(void* user) {
	(void)user;
	return XwaControllerMapping_RumbleInstance();
}

static int XwaRumble_Rumble(uint16_t low, uint16_t high, uint32_t duration_ms, void* user) {
	(void)user;
	return XwaControllerMapping_Rumble(low, high, duration_ms);
}

void XwaRumble_RegisterProvider(void) {
	AeronCompatRumbleProvider provider = { 0 };

	provider.has_rumble = XwaRumble_HasRumble;
	provider.controller_instance_id = XwaRumble_ControllerInstanceId;
	provider.rumble = XwaRumble_Rumble;
	AeronCompat_SetRumbleProvider(&provider);
}
