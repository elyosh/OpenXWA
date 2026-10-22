#include "xwa_runtime/models/component_strength.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/object_type.h"
#include "xwa_runtime/models/model_settings.h"

#include <stdint.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define COMPONENT_STRENGTH_MESH_TYPES 32

static const char* const s_meshTypes[COMPONENT_STRENGTH_MESH_TYPES] = {
	"default",
	"main_hull",
	"wing",
	"fuselage",
	"gun_turret",
	"small_gun",
	"engine",
	"bridge",
	"shield_generator",
	"energy_generator",
	"launcher",
	"communication_system",
	"beam_system",
	"command_system",
	"docking_platform",
	"landing_platform",
	"hangar",
	"cargo_pod",
	"misc_hull",
	"antenna",
	"rotary_wing",
	"rotary_gun_turret",
	"rotary_launcher",
	"rotary_comm_system",
	"rotary_beam_system",
	"rotary_command_system",
	"hatch",
	"custom",
	"weapon_system_1",
	"weapon_system_2",
	"power_regenerator",
	"reactor",
};

/* The strength of each mesh type of an object type, plus 1. 0 means not
   set. */
typedef struct ComponentStrengthModel {
	uint16_t value[COMPONENT_STRENGTH_MESH_TYPES];
} ComponentStrengthModel;

static ComponentStrengthModel s_models[OBJ_Count];

void XwaComponentStrength_ClearModelSettings(void) { memset(s_models, 0, sizeof s_models); }

void XwaComponentStrength_ReadModelSettings(unsigned int objectType, const char* path,
											const AeronConfigNode* group) {
	size_t index;

	if (objectType >= OBJ_Count || group == NULL) {
		return;
	}
	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: component_strength isn't a map. It is ignored", path);
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(group); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(group, index);
		const AeronConfigNode* node = AeronConfigNode_MapValueAt(group, index);
		const int64_t value = AeronConfigNode_Int(node, -1);
		int meshType;

		for (meshType = 0; meshType < COMPONENT_STRENGTH_MESH_TYPES; ++meshType) {
			if (strcmp(key, s_meshTypes[meshType]) == 0) {
				break;
			}
		}
		if (meshType == COMPONENT_STRENGTH_MESH_TYPES) {
			Aeron_LogWarn("xwa.models", "%s: component_strength: '%s' isn't a mesh type. It is ignored", path,
						  key);
			continue;
		}
		if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < 0 || value > 255) {
			Aeron_LogWarn("xwa.models",
						  "%s: component_strength.%s must be a number from 0 to 255. It is ignored", path,
						  key);
			continue;
		}
		s_models[objectType].value[meshType] = (uint16_t)(value + 1);
	}
}

int XwaComponentStrength_Value(unsigned int objectType, int meshType, int stock) {
	XwaModelSettings_Ensure(objectType);
	if (objectType >= OBJ_Count || meshType < 0 || meshType >= COMPONENT_STRENGTH_MESH_TYPES ||
		s_models[objectType].value[meshType] == 0) {
		return stock;
	}
	return s_models[objectType].value[meshType] - 1;
}
