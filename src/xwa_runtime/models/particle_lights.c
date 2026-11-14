#include "xwa_runtime/models/particle_lights.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa_runtime/models/flight_config.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <math.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* The names of the groups, by XwaParticleLightGroup. */
static const char* const kParticleLightNames[XWA_PARTICLE_LIGHT_GROUP_COUNT] = {
	"cockpit_sparks",
	"cockpit_smoke",
	"craft_damage",
	"player_craft_damage",
};

static XwaParticleLight s_lights[XWA_PARTICLE_LIGHT_GROUP_COUNT];

int XwaParticleLights_Find(const char* name) {
	int group;

	for (group = 0; name != NULL && group < XWA_PARTICLE_LIGHT_GROUP_COUNT; ++group) {
		if (strcmp(name, kParticleLightNames[group]) == 0) {
			return group;
		}
	}
	return -1;
}

const char* XwaParticleLights_Name(int group) {
	return group >= 0 && group < XWA_PARTICLE_LIGHT_GROUP_COUNT ? kParticleLightNames[group] : NULL;
}

/* Reads group `name` from `entry` into `light`. Returns 0 (logged) when the
   entry is invalid. */
static int ParticleLights_ReadLight(const AeronConfigNode* entry, const char* name, XwaParticleLight* light) {
	const AeronConfigNode* node;
	int ok = 1;
	int i;

	if (AeronConfigNode_Type(entry) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.flight", "%s:%d: particle_lights.%s must be a map", XWA_FLIGHT_CONFIG_PATH,
					   AeronConfigNode_Line(entry), name);
		return 0;
	}
	node = AeronConfigNode_MapGet(entry, "color");
	if (node != NULL) {
		for (i = 0; i < 3; ++i) {
			const AeronConfigNode* item = AeronConfigNode_SequenceGet(node, (size_t)i);
			const AeronConfigNodeType type = AeronConfigNode_Type(item);
			const double value = AeronConfigNode_Float(item, -1.0);

			if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE ||
				AeronConfigNode_SequenceCount(node) != 3 ||
				(type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT) || !isfinite(value) || value < 0.0) {
				Aeron_LogError(
					"xwa.flight",
					"%s:%d: particle_lights.%s: color must be a list of three non-negative numbers",
					XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node), name);
				ok = 0;
				break;
			}
			light->color[i] = (float)value;
		}
		light->has_color = 1;
	}
	node = AeronConfigNode_MapGet(entry, "power_w");
	if (node != NULL) {
		const AeronConfigNodeType type = AeronConfigNode_Type(node);

		light->power_w = (float)AeronConfigNode_Float(node, -1.0);
		light->has_power = 1;
		if ((type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT) || !(light->power_w >= 0.0f) ||
			!isfinite(light->power_w)) {
			Aeron_LogError("xwa.flight",
						   "%s:%d: particle_lights.%s: power_w must be a number of watts, 0 or more",
						   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node), name);
			ok = 0;
		}
	}
	node = AeronConfigNode_MapGet(entry, "fade");
	if (node != NULL) {
		light->has_fade = 1;
		light->fade = (uint8_t)AeronConfigNode_Bool(node, 0);
		if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
			Aeron_LogError("xwa.flight", "%s:%d: particle_lights.%s: fade must be true or false",
						   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node), name);
			ok = 0;
		}
	}
	return ok;
}

void XwaParticleLights_Init(void) {
	const AeronConfigNode* root;
	const AeronConfigNode* section;
	size_t i;
	int group;

	memset(s_lights, 0, sizeof s_lights);
	root = XwaFlightConfig_Root();
	section = AeronConfigNode_MapGet(root, "particle_lights");
	if (section == NULL) {
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.flight", "%s:%d: particle_lights must be a map. The default lights are used",
					   XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(section));
		return;
	}
	for (i = 0; i < AeronConfigNode_MapCount(section); ++i) {
		const char* name = AeronConfigNode_MapKeyAt(section, i);

		group = XwaParticleLights_Find(name);
		if (group < 0) {
			Aeron_LogWarn("xwa.flight", "%s:%d: particle_lights: unknown group %s; it is ignored",
						  XWA_FLIGHT_CONFIG_PATH,
						  AeronConfigNode_Line(AeronConfigNode_MapValueAt(section, i)),
						  name != NULL ? name : "");
			continue;
		}
		if (!ParticleLights_ReadLight(AeronConfigNode_MapValueAt(section, i), name, &s_lights[group])) {
			memset(&s_lights[group], 0, sizeof s_lights[group]);
			Aeron_LogError("xwa.flight", "%s: particle_lights.%s: the default light is used",
						   XWA_FLIGHT_CONFIG_PATH, name);
		}
	}
}

void XwaParticleLights_Capture(XwaSnapshot* snapshot) {
	memcpy(snapshot->particle_lights, s_lights, sizeof snapshot->particle_lights);
}
