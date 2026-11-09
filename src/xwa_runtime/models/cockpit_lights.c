#include "xwa_runtime/models/cockpit_lights.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa_runtime/models/flight_config.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <math.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* The names of the cockpit lights, by slot. */
static const char* const kCockpitLightNames[XWA_SNAP_COCKPIT_LIGHT_COUNT] = {
	"hull_critical",
	"warhead_lock",
	"beam_warning",
};

/* The values that the mod sets for a light. The renderer uses its
   defaults for the others. */
typedef struct ModCockpitLight {
	int hasPosition;
	int hasColor;
	int hasPower;
	float position[3];
	float color[3];
	float power;
} ModCockpitLight;

static ModCockpitLight s_lights[XWA_SNAP_COCKPIT_LIGHT_COUNT];

int XwaCockpitLights_Find(const char* name) {
	int slot;

	for (slot = 0; name != NULL && slot < XWA_SNAP_COCKPIT_LIGHT_COUNT; ++slot) {
		if (strcmp(name, kCockpitLightNames[slot]) == 0) {
			return slot;
		}
	}
	return -1;
}

const char* XwaCockpitLights_Name(int slot) {
	return slot >= 0 && slot < XWA_SNAP_COCKPIT_LIGHT_COUNT ? kCockpitLightNames[slot] : NULL;
}

/* Reads the list of three numbers at `key` of light `name`, if there is
   one. Returns 0 (logged) when it isn't a list of three numbers, or when
   `nonNegative` and one is below 0. */
static int CockpitLights_ReadVector(const AeronConfigNode* entry, const char* name, const char* key,
									int nonNegative, float out[3], int* found) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(entry, key);
	float values[3];
	int i;

	if (node == NULL) {
		return 1;
	}
	for (i = 0; i < 3; ++i) {
		const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, (size_t)i);
		const AeronConfigNodeType type = AeronConfigNode_Type(item);

		if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE || AeronConfigNode_SequenceCount(node) != 3 ||
			(type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT) ||
			!isfinite(AeronConfigNode_Float(item, 0.0)) ||
			(nonNegative && AeronConfigNode_Float(item, 0.0) < 0.0)) {
			Aeron_LogError("xwa.flight", "%s:%d: cockpit_lights.%s: %s must be a list of three %snumbers",
						   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node), name, key,
						   nonNegative ? "non-negative " : "");
			return 0;
		}
		values[i] = (float)AeronConfigNode_Float(item, 0.0);
	}
	memcpy(out, values, sizeof values);
	if (found != NULL) {
		*found = 1;
	}
	return 1;
}

/* Reads light `name` from `entry` into `light`. Returns 0 (logged) when
   the entry is invalid. */
static int CockpitLights_ReadLight(const AeronConfigNode* entry, const char* name, ModCockpitLight* light) {
	const AeronConfigNode* node;
	int ok = 1;

	if (AeronConfigNode_Type(entry) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.flight", "%s:%d: cockpit_lights.%s must be a map", XWA_FLIGHT_CONFIG_PATH,
					   AeronConfigNode_Line(entry), name);
		return 0;
	}
	ok &= CockpitLights_ReadVector(entry, name, "position_m", 0, light->position, &light->hasPosition);
	ok &= CockpitLights_ReadVector(entry, name, "color", 1, light->color, &light->hasColor);
	node = AeronConfigNode_MapGet(entry, "power_w");
	if (node != NULL) {
		const AeronConfigNodeType type = AeronConfigNode_Type(node);

		light->power = (float)AeronConfigNode_Float(node, -1.0);
		light->hasPower = 1;
		if ((type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT) || !(light->power >= 0.0f) ||
			!isfinite(light->power)) {
			Aeron_LogError("xwa.flight",
						   "%s:%d: cockpit_lights.%s: power_w must be a number of watts, 0 or more",
						   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node), name);
			ok = 0;
		}
	}
	return ok;
}

void XwaCockpitLights_Init(void) {
	const AeronConfigNode* root;
	const AeronConfigNode* section;
	size_t i;
	int slot;

	memset(s_lights, 0, sizeof s_lights);
	root = XwaFlightConfig_Root();
	section = AeronConfigNode_MapGet(root, "cockpit_lights");
	if (section == NULL) {
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.flight", "%s:%d: cockpit_lights must be a map. The default lights are used",
					   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(section));
		return;
	}
	for (i = 0; i < AeronConfigNode_MapCount(section); ++i) {
		const char* name = AeronConfigNode_MapKeyAt(section, i);

		slot = XwaCockpitLights_Find(name);
		if (slot < 0) {
			Aeron_LogWarn("xwa.flight", "%s:%d: cockpit_lights: unknown light %s; it is ignored",
						  XWA_FLIGHT_CONFIG_PATH,
						  AeronConfigNode_Line(AeronConfigNode_MapValueAt(section, i)),
						  name != NULL ? name : "");
			continue;
		}
		if (!CockpitLights_ReadLight(AeronConfigNode_MapValueAt(section, i), name, &s_lights[slot])) {
			memset(&s_lights[slot], 0, sizeof s_lights[slot]);
			Aeron_LogError("xwa.flight", "%s: cockpit_lights.%s: the default light is used",
						   XWA_FLIGHT_CONFIG_PATH, name);
		}
	}
}

void XwaCockpitLights_Capture(XwaSnapshot* snapshot) {
	int slot;

	for (slot = 0; slot < XWA_SNAP_COCKPIT_LIGHT_COUNT; ++slot) {
		const ModCockpitLight* light = &s_lights[slot];
		XwaCockpitLight* out = &snapshot->cockpit_lights[slot];

		out->mod_has_position = (uint8_t)light->hasPosition;
		out->mod_has_color = (uint8_t)light->hasColor;
		out->mod_has_power = (uint8_t)light->hasPower;
		memcpy(out->mod_position_m, light->position, sizeof out->mod_position_m);
		memcpy(out->mod_color, light->color, sizeof out->mod_color);
		out->mod_power_w = light->power;
	}
}
