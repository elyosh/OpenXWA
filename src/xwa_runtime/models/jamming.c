#include "xwa_runtime/models/jamming.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa_runtime/models/model_settings.h"

#include <stdint.h>
#include <string.h>

/* Modern port module; no original counterpart. */

static const char* const s_directionNames[] = { "all", "up", "down" };
static const char* const s_ruleNames[] = { "none", "hangar_mesh_center", "hangar_point",
										   "hangar_and_jamming_points" };

/* The keys that a model's group sets. */
enum {
	JAMMING_SET_ENABLED = 1 << 0,
	JAMMING_SET_DIRECTION = 1 << 1,
	JAMMING_SET_RANGE = 1 << 2,
	JAMMING_SET_RULE = 1 << 3,
};

typedef struct JammingModel {
	uint8_t has;
	uint8_t set;
	XwaJammingRule rule;
} JammingModel;

static JammingModel s_models[OBJ_Count];

void XwaJamming_ClearModelSettings(void) { memset(s_models, 0, sizeof s_models); }

/* Returns the index of `node`'s text in `names`, or -1. */
static int Jamming_ReadName(const AeronConfigNode* node, const char* const* names, int count) {
	const char* text = AeronConfigNode_String(node, NULL);
	int index;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_STRING || text == NULL) {
		return -1;
	}
	for (index = 0; index < count; ++index) {
		if (strcmp(text, names[index]) == 0) {
			return index;
		}
	}
	return -1;
}

void XwaJamming_ReadModelSettings(unsigned int objectType, const char* path, const AeronConfigNode* group) {
	JammingModel* model;
	size_t index;

	if (objectType >= OBJ_Count || group == NULL) {
		return;
	}
	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: jamming isn't a map. It is ignored", path);
		return;
	}
	model = &s_models[objectType];
	model->has = 1;
	for (index = 0; index < AeronConfigNode_MapCount(group); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(group, index);
		const AeronConfigNode* node = AeronConfigNode_MapValueAt(group, index);
		int value;

		if (strcmp(key, "enabled") == 0) {
			if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
				Aeron_LogWarn("xwa.models", "%s: jamming.enabled must be true or false. It is ignored", path);
				continue;
			}
			model->rule.enabled = AeronConfigNode_Bool(node, 1);
			model->set |= JAMMING_SET_ENABLED;
		} else if (strcmp(key, "direction") == 0) {
			if ((value = Jamming_ReadName(node, s_directionNames, 3)) < 0) {
				Aeron_LogWarn("xwa.models", "%s: jamming.direction must be all, up or down. It is ignored",
							  path);
				continue;
			}
			model->rule.direction = (XwaJammingDirection)value;
			model->set |= JAMMING_SET_DIRECTION;
		} else if (strcmp(key, "range") == 0) {
			const int64_t number = AeronConfigNode_Int(node, -1);

			if (AeronConfigNode_Type(node) == AERON_CONFIG_STRING &&
				strcmp(AeronConfigNode_String(node, ""), "none") == 0) {
				model->rule.range = -1;
			} else if (AeronConfigNode_Type(node) == AERON_CONFIG_INT && number >= 0 && number <= INT32_MAX) {
				model->rule.range = (int)number;
			} else {
				Aeron_LogWarn("xwa.models",
							  "%s: jamming.range must be none or a number from 0 to %d. It is ignored", path,
							  INT32_MAX);
				continue;
			}
			model->set |= JAMMING_SET_RANGE;
		} else if (strcmp(key, "rule") == 0) {
			if ((value = Jamming_ReadName(node, s_ruleNames, 4)) < 0) {
				Aeron_LogWarn("xwa.models",
							  "%s: jamming.rule must be none, hangar_mesh_center, hangar_point or "
							  "hangar_and_jamming_points. It is ignored",
							  path);
				continue;
			}
			model->rule.behavior = (XwaJammingBehavior)value;
			model->set |= JAMMING_SET_RULE;
		} else {
			Aeron_LogWarn("xwa.models", "%s: jamming.%s isn't a setting. It is ignored", path, key);
		}
	}
}

/* The stock rule of `objectType`, as collide_ApplyHostileProximityWeaponDisruption
   chooses it. */
static void Jamming_StockRule(unsigned int objectType, XwaJammingRule* rule) {
	rule->enabled = 1;
	rule->direction = XWA_JAMMING_ALL;
	rule->range = -1;
	rule->behavior = XWA_JAMMING_RULE_HANGAR_AND_JAMMING_POINTS;
	switch (objectType) {
		case OBJ_ImperialResearchCenter:
			rule->enabled = 0;
			rule->behavior = XWA_JAMMING_RULE_NONE;
			break;
		case OBJ_Factory:
			rule->range = 4096;
			rule->behavior = XWA_JAMMING_RULE_NONE;
			break;
		case OBJ_ImperialStarDestroyer2:
		case OBJ_Interdictor2:
		case OBJ_VictoryStarDestroyer2:
			rule->direction = XWA_JAMMING_DOWN;
			break;
		case OBJ_SuperStarDestroyer:
			rule->behavior = XWA_JAMMING_RULE_HANGAR_POINT;
			break;
		case OBJ_RepairYard:
			rule->behavior = XWA_JAMMING_RULE_HANGAR_MESH_CENTER;
			break;
		default:
			break;
	}
}

int XwaJamming_Rule(unsigned int objectType, XwaJammingRule* rule) {
	const JammingModel* model;

	Jamming_StockRule(objectType, rule);
	XwaModelSettings_Ensure(objectType);
	if (objectType >= OBJ_Count || !s_models[objectType].has) {
		return 0;
	}
	model = &s_models[objectType];
	if (model->set & JAMMING_SET_ENABLED) {
		rule->enabled = model->rule.enabled;
	}
	if (model->set & JAMMING_SET_DIRECTION) {
		rule->direction = model->rule.direction;
	}
	if (model->set & JAMMING_SET_RANGE) {
		rule->range = model->rule.range;
	}
	if (model->set & JAMMING_SET_RULE) {
		rule->behavior = model->rule.behavior;
	}
	return 1;
}
