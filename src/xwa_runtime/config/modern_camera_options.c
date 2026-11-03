#include "xwa_runtime/config/modern_camera_options.h"

#include "aeron/log.h"

#include <string.h>

#define CAMERA_SWITCH(field) { #field, 1, 0.0f, 1.0f, offsetof(XwaModernCameraOptions, field) }
#define CAMERA_NUMBER(field, min, max) { #field, 0, min, max, offsetof(XwaModernCameraOptions, field) }

static const XwaModernCameraOptionKey g_cameraOptionKeys[] = {
	CAMERA_SWITCH(enabled),
	CAMERA_SWITCH(exterior_lag),
	CAMERA_NUMBER(exterior_lag_strength, 0.0f, 2.0f),
	CAMERA_NUMBER(exterior_lag_max, 0.0f, 15.0f),
	CAMERA_NUMBER(exterior_tilt, -30.0f, 30.0f),
	CAMERA_SWITCH(cockpit_sway),
	CAMERA_NUMBER(cockpit_sway_strength_m, 0.0f, 0.5f),
	CAMERA_NUMBER(cockpit_speed_sway_m, 0.0f, 0.05f),
	CAMERA_NUMBER(cockpit_sway_max_m, 0.0f, 0.25f),
	CAMERA_NUMBER(lag_time, 0.05f, 2.0f),
	CAMERA_SWITCH(exterior_hud),
};

#define CAMERA_KEY_COUNT (sizeof g_cameraOptionKeys / sizeof g_cameraOptionKeys[0])

static struct {
	XwaModernCameraOptions shipped;
	XwaModernCameraOptions mod;
	XwaModernCameraOptions player;
	unsigned int override_mask;
	XwaModernCameraOptionsPersistFn persist;
	int has_mod;
	int configured;
	int dirty;
} g_modernCameraOptions;

const XwaModernCameraOptionKey* XwaModernCameraOptions_Keys(size_t* count) {
	if (count) {
		*count = CAMERA_KEY_COUNT;
	}
	return g_cameraOptionKeys;
}

unsigned int XwaModernCameraOptions_EnabledBit(void) { return 1u; }

void XwaModernCameraOptions_Configure(const XwaModernCameraOptions* shipped,
									  const XwaModernCameraOptions* player, unsigned int override_mask,
									  XwaModernCameraOptionsPersistFn persist) {
	memset(&g_modernCameraOptions, 0, sizeof g_modernCameraOptions);
	if (!shipped || !player) {
		Aeron_LogError("xwa.config", "cannot configure the camera options without values");
		return;
	}
	g_modernCameraOptions.shipped = *shipped;
	g_modernCameraOptions.player = *player;
	g_modernCameraOptions.override_mask = override_mask;
	g_modernCameraOptions.persist = persist;
	g_modernCameraOptions.configured = 1;
}

void XwaModernCameraOptions_GetShipped(XwaModernCameraOptions* out) {
	if (out) {
		*out = g_modernCameraOptions.shipped;
	}
}

void XwaModernCameraOptions_SetMod(const XwaModernCameraOptions* options) {
	if (options) {
		g_modernCameraOptions.mod = *options;
		g_modernCameraOptions.has_mod = 1;
	}
}

void XwaModernCameraOptions_Get(XwaModernCameraOptions* out) {
	size_t i;

	if (!out) {
		return;
	}
	*out = g_modernCameraOptions.has_mod ? g_modernCameraOptions.mod : g_modernCameraOptions.shipped;
	for (i = 0; i < CAMERA_KEY_COUNT; ++i) {
		if (g_modernCameraOptions.override_mask & (1u << i)) {
			const size_t offset = g_cameraOptionKeys[i].offset;
			const size_t size = g_cameraOptionKeys[i].is_switch ? sizeof(int) : sizeof(float);
			memcpy((char*)out + offset, (const char*)&g_modernCameraOptions.player + offset, size);
		}
	}
}

void XwaModernCameraOptions_SetEnabled(int enabled) {
	XwaModernCameraOptions current;

	if (!g_modernCameraOptions.configured) {
		return;
	}
	XwaModernCameraOptions_Get(&current);
	if (current.enabled == (enabled != 0)) {
		return;
	}
	g_modernCameraOptions.player.enabled = enabled != 0;
	g_modernCameraOptions.override_mask |= XwaModernCameraOptions_EnabledBit();
	g_modernCameraOptions.dirty = 1;
}

int XwaModernCameraOptions_Flush(void) {
	char error[512];

	if (!g_modernCameraOptions.configured || !g_modernCameraOptions.dirty) {
		return 1;
	}
	if (!g_modernCameraOptions.persist) {
		Aeron_LogWarn("xwa.config", "the camera switch changed, but nothing saves it");
		return 0;
	}
	error[0] = '\0';
	if (!g_modernCameraOptions.persist(g_modernCameraOptions.player.enabled, error, sizeof error)) {
		Aeron_LogError("xwa.config", "%s",
					   error[0] ? error : "could not save the camera switch to config.yaml");
		return 0;
	}
	g_modernCameraOptions.dirty = 0;
	return 1;
}

int XwaModernCameraOptions_IsDirty(void) { return g_modernCameraOptions.dirty; }
