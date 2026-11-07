#include "xwa_runtime/models/cockpit_alert_lights.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <math.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define ALERT_LIGHTS_FLIGHT_YAML "flight/flight.yaml"

/* The names of the game's light pulses, by slot. */
static const char* const kAlertLightNames[XWA_SNAP_MAX_LIGHT_PULSES] = {
	"hull_critical",   "warhead_lock",        "beam_warning",
	"hyperspace_jump", "hyperspace_transfer", "hyperspace_arrival",
};

typedef struct AlertLight {
	int defined;
	int hasColor;
	float position[3];
	float color[3];
	float power;
} AlertLight;

static AlertLight s_lights[XWA_SNAP_MAX_LIGHT_PULSES];

/* Reads the list of three numbers at `key` of light `name`, if there is
   one. Returns 0 (logged) when it isn't a list of three numbers, or when
   `nonNegative` and one is below 0. */
static int AlertLights_ReadVector(const AeronConfigNode* entry, const char* name, const char* key,
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
			Aeron_LogError("xwa.flight",
						   "%s:%d: cockpit_alert_lights.%s: %s must be a list of three %snumbers",
						   ALERT_LIGHTS_FLIGHT_YAML, AeronConfigNode_Line(node), name, key,
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
static int AlertLights_ReadLight(const AeronConfigNode* entry, const char* name, AlertLight* light) {
	const AeronConfigNode* node;
	int ok = 1;

	if (AeronConfigNode_Type(entry) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.flight", "%s:%d: cockpit_alert_lights.%s must be a map", ALERT_LIGHTS_FLIGHT_YAML,
					   AeronConfigNode_Line(entry), name);
		return 0;
	}
	ok &= AlertLights_ReadVector(entry, name, "position_m", 0, light->position, NULL);
	ok &= AlertLights_ReadVector(entry, name, "color", 1, light->color, &light->hasColor);
	node = AeronConfigNode_MapGet(entry, "power_w");
	light->power = (float)AeronConfigNode_Float(node, -1.0);
	if (!(light->power > 0.0f) || !isfinite(light->power)) {
		Aeron_LogError("xwa.flight",
					   "%s:%d: cockpit_alert_lights.%s: power_w must be a number of watts above 0",
					   ALERT_LIGHTS_FLIGHT_YAML, AeronConfigNode_Line(node != NULL ? node : entry), name);
		ok = 0;
	}
	return ok;
}

void XwaCockpitAlertLights_Init(void) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* section;
	size_t i;
	int slot;

	memset(s_lights, 0, sizeof s_lights);
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, ALERT_LIGHTS_FLIGHT_YAML, &file,
									&error)) {
		/* XwaFlightConfig_Root already logs this error. */
		return;
	}
	root = AeronConfigFile_Root(file);
	section = AeronConfigNode_MapGet(root, "cockpit_alert_lights");
	if (section == NULL) {
		AeronConfigFile_Destroy(file);
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.flight", "%s:%d: cockpit_alert_lights must be a map; the game's lights are kept",
					   ALERT_LIGHTS_FLIGHT_YAML, AeronConfigNode_Line(section));
		AeronConfigFile_Destroy(file);
		return;
	}
	for (i = 0; i < AeronConfigNode_MapCount(section); ++i) {
		const char* name = AeronConfigNode_MapKeyAt(section, i);

		for (slot = 0; slot < XWA_SNAP_MAX_LIGHT_PULSES; ++slot) {
			if (name != NULL && strcmp(name, kAlertLightNames[slot]) == 0) {
				break;
			}
		}
		if (slot == XWA_SNAP_MAX_LIGHT_PULSES) {
			Aeron_LogWarn("xwa.flight", "%s:%d: cockpit_alert_lights: unknown light %s; it is ignored",
						  ALERT_LIGHTS_FLIGHT_YAML,
						  AeronConfigNode_Line(AeronConfigNode_MapValueAt(section, i)),
						  name != NULL ? name : "");
			continue;
		}
		if (AlertLights_ReadLight(AeronConfigNode_MapValueAt(section, i), name, &s_lights[slot])) {
			s_lights[slot].defined = 1;
		} else {
			memset(&s_lights[slot], 0, sizeof s_lights[slot]);
			Aeron_LogError("xwa.flight", "%s: cockpit_alert_lights.%s: the game's light is kept",
						   ALERT_LIGHTS_FLIGHT_YAML, name);
		}
	}
	AeronConfigFile_Destroy(file);
}

void XwaCockpitAlertLights_Capture(XwaSnapshot* snapshot) {
	int slot;

	for (slot = 0; slot < XWA_SNAP_MAX_LIGHT_PULSES; ++slot) {
		const AlertLight* light = &s_lights[slot];
		XwaLightPulse* out = &snapshot->light_pulses[slot];

		out->mod_defined = (uint8_t)light->defined;
		out->mod_has_color = (uint8_t)light->hasColor;
		memcpy(out->mod_position_m, light->position, sizeof out->mod_position_m);
		memcpy(out->mod_color, light->color, sizeof out->mod_color);
		out->mod_power_w = light->power;
	}
}
