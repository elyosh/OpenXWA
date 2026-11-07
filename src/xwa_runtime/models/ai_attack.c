#include "xwa_runtime/models/ai_attack.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/flight/object/object.h"
#include "xwa_runtime/models/flight_config.h"
#include "xwa_runtime/models/model_settings.h"
#include "xwa_runtime/models/model_source.h"
#include "xwa_runtime/models/model_swaps.h"
#include "xwa_runtime/models/names.h"
#include "xwa_runtime/models/units.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* The game doubles the break range of small targets when the two craft face
   apart, so a larger value would overflow. */
#define AI_ATTACK_SMALL_MAX (INT32_MAX / 2)

/* The stock laser range of each rank: the range of the skill tier that the
   rank gives (g_aiSkillValueQ16ByLevel, then
   g_aiFighterShootMaxRangeBySkill). */
static const uint32_t s_stockLaserRangeByRank[XWA_AI_ATTACK_RANKS] = { 0x6000, 0x6000, 0x8000,
																	   0xa000, 0xa000, 0xa000 };

/* The break offset for targets that load the model `key`. */
typedef struct AiAttackOffset {
	char key[XWA_MODEL_SOURCE_KEY_MAX];
	int32_t offset;
} AiAttackOffset;

typedef struct AiAttackSet {
	int32_t value[XWA_AI_ATTACK_VALUES];
	uint8_t set[XWA_AI_ATTACK_VALUES];
	AiAttackOffset* offsets;
	size_t offsetCount;
} AiAttackSet;

/* The values of flight.yaml. */
static AiAttackSet s_defaults;
static AiAttackSet s_models[OBJ_Count];

static void AiAttack_FreeSet(AiAttackSet* set) {
	free(set->offsets);
	memset(set, 0, sizeof *set);
}

/* Reads `node`, the key `name`, as a distance in metres from `min` to `max`
   game units, into `value` of `set`. */
static void AiAttack_ReadRange(const char* path, const char* name, const AeronConfigNode* node, int64_t min,
							   int64_t max, AiAttackSet* set, int value) {
	int32_t game;

	if (!XwaUnits_Read(node, XWA_UNIT_METRES, min, max, &game)) {
		Aeron_LogWarn("xwa.models",
					  "%s:%d: ai_attack.%s must be a number of metres from %.2f to %.2f. It is ignored", path,
					  AeronConfigNode_Line(node), name, XwaUnits_FromGame(XWA_UNIT_METRES, (double)min),
					  XwaUnits_FromGame(XWA_UNIT_METRES, (double)max));
		return;
	}
	set->value[value] = game;
	set->set[value] = 1;
}

/* Reads the map `node`, the key `name`, which has a distance for each rank,
   into the values of `set` from `first`. */
static void AiAttack_ReadRanks(const char* path, const char* name, const AeronConfigNode* node, int first,
							   int64_t min, int64_t max, AiAttackSet* set) {
	size_t index;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s:%d: ai_attack.%s must be a map of ranks. It is ignored", path,
					  AeronConfigNode_Line(node), name);
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* child = AeronConfigNode_MapValueAt(node, index);
		const int rank = XwaNames_Find(&g_xwaRankNames, key);
		char full[128];

		snprintf(full, sizeof full, "%s.%s", name, key);
		if (rank < 0 || rank >= XWA_AI_ATTACK_RANKS) {
			Aeron_LogWarn("xwa.models", "%s:%d: ai_attack.%s isn't a rank. It is ignored", path,
						  AeronConfigNode_Line(child), full);
			continue;
		}
		AiAttack_ReadRange(path, full, child, min, max, set, first + rank);
	}
}

/* Reads `break_range_m.target_offset`, a map of model keys to distances. */
static void AiAttack_ReadOffsets(const char* path, const AeronConfigNode* node, AiAttackSet* set) {
	size_t index;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models",
					  "%s:%d: ai_attack.break_range_m.target_offset must be a map of models. It is ignored",
					  path, AeronConfigNode_Line(node));
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* child = AeronConfigNode_MapValueAt(node, index);
		AiAttackOffset* grown;
		int32_t game;

		if (strlen(key) >= XWA_MODEL_SOURCE_KEY_MAX) {
			Aeron_LogWarn(
				"xwa.models",
				"%s:%d: ai_attack.break_range_m.target_offset.%s is too long for a model name. It is ignored",
				path, AeronConfigNode_Line(child), key);
			continue;
		}
		if (!XwaUnits_Read(child, XWA_UNIT_METRES, INT32_MIN, INT32_MAX, &game)) {
			Aeron_LogWarn(
				"xwa.models",
				"%s:%d: ai_attack.break_range_m.target_offset.%s must be a number of metres. It is ignored",
				path, AeronConfigNode_Line(child), key);
			continue;
		}
		grown = (AiAttackOffset*)realloc(set->offsets, (set->offsetCount + 1) * sizeof *set->offsets);
		if (grown == NULL) {
			Aeron_LogError("xwa.models", "%s: out of memory for ai_attack.break_range_m.target_offset", path);
			return;
		}
		set->offsets = grown;
		memset(&set->offsets[set->offsetCount], 0, sizeof *set->offsets);
		memcpy(set->offsets[set->offsetCount].key, key, strlen(key) + 1);
		set->offsets[set->offsetCount].offset = game;
		set->offsetCount++;
	}
}

static void AiAttack_ReadBreak(const char* path, const AeronConfigNode* node, AiAttackSet* set) {
	size_t index;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s:%d: ai_attack.break_range_m isn't a map. It is ignored", path,
					  AeronConfigNode_Line(node));
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* child = AeronConfigNode_MapValueAt(node, index);

		if (strcmp(key, "warhead") == 0) {
			AiAttack_ReadRange(path, "break_range_m.warhead", child, INT32_MIN, INT32_MAX, set,
							   XWA_AI_ATTACK_BREAK_WARHEAD);
		} else if (strcmp(key, "small") == 0) {
			AiAttack_ReadRanks(path, "break_range_m.small", child, XWA_AI_ATTACK_BREAK_SMALL, 0,
							   AI_ATTACK_SMALL_MAX, set);
		} else if (strcmp(key, "big_max") == 0) {
			AiAttack_ReadRange(path, "break_range_m.big_max", child, INT32_MIN, INT32_MAX, set,
							   XWA_AI_ATTACK_BREAK_BIG_MAX);
		} else if (strcmp(key, "target_offset") == 0) {
			AiAttack_ReadOffsets(path, child, set);
		} else {
			Aeron_LogWarn("xwa.models", "%s:%d: ai_attack.break_range_m.%s isn't a setting. It is ignored",
						  path, AeronConfigNode_Line(child), key);
		}
	}
}

/* Reads the `ai_attack` map `group` into `set`. */
static void AiAttack_ReadGroup(const char* path, const AeronConfigNode* group, AiAttackSet* set) {
	size_t index;

	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s:%d: ai_attack isn't a map. It is ignored", path,
					  AeronConfigNode_Line(group));
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(group); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(group, index);
		const AeronConfigNode* child = AeronConfigNode_MapValueAt(group, index);

		if (strcmp(key, "break_range_m") == 0) {
			AiAttack_ReadBreak(path, child, set);
		} else if (strcmp(key, "laser_range_m") == 0) {
			AiAttack_ReadRanks(path, "laser_range_m", child, XWA_AI_ATTACK_LASER_RANGE, 0, INT32_MAX, set);
		} else if (strcmp(key, "avoid_hits") == 0) {
			if (AeronConfigNode_Type(child) != AERON_CONFIG_BOOL) {
				Aeron_LogWarn("xwa.models",
							  "%s:%d: ai_attack.avoid_hits must be true or false. It is ignored", path,
							  AeronConfigNode_Line(child));
				continue;
			}
			set->value[XWA_AI_ATTACK_AVOID_HITS] = AeronConfigNode_Bool(child, 1) != 0;
			set->set[XWA_AI_ATTACK_AVOID_HITS] = 1;
		} else {
			Aeron_LogWarn("xwa.models", "%s:%d: ai_attack.%s isn't a setting. It is ignored", path,
						  AeronConfigNode_Line(child), key);
		}
	}
}

void XwaAiAttack_Init(void) {
	const AeronConfigNode* section = AeronConfigNode_MapGet(XwaFlightConfig_Root(), "ai_attack");

	AiAttack_FreeSet(&s_defaults);
	if (section != NULL) {
		AiAttack_ReadGroup(XWA_FLIGHT_CONFIG_PATH, section, &s_defaults);
	}
}

void XwaAiAttack_ClearModelSettings(void) {
	size_t type;

	for (type = 0; type < OBJ_Count; ++type) {
		AiAttack_FreeSet(&s_models[type]);
	}
}

void XwaAiAttack_ReadModelSettings(unsigned int objectType, const char* path, const AeronConfigNode* group) {
	if (objectType >= OBJ_Count || group == NULL) {
		return;
	}
	AiAttack_ReadGroup(path, group, &s_models[objectType]);
}

/* The values of the model that object `objIdx` loads. */
static const AiAttackSet* AiAttack_Model(unsigned int objIdx) {
	const unsigned int objectType = g_objectTable[(uint16_t)objIdx].objectType;

	XwaModelSettings_Ensure(objectType);
	return objectType < OBJ_Count ? &s_models[objectType] : NULL;
}

int32_t XwaAiAttack_Value(unsigned int aiObjIdx, XwaAiAttackValue value, int32_t stock) {
	const AiAttackSet* model = AiAttack_Model(aiObjIdx);

	if ((unsigned int)value >= XWA_AI_ATTACK_VALUES) {
		return stock;
	}
	if (model != NULL && model->set[value]) {
		return model->value[value];
	}
	if (s_defaults.set[value]) {
		return s_defaults.value[value];
	}
	return stock;
}

/* Finds the offset for model `key` in `set`. */
static const AiAttackOffset* AiAttack_FindOffset(const AiAttackSet* set, const char* key) {
	size_t index;

	for (index = 0; set != NULL && index < set->offsetCount; ++index) {
		if (strcmp(set->offsets[index].key, key) == 0) {
			return &set->offsets[index];
		}
	}
	return NULL;
}

int32_t XwaAiAttack_TargetOffset(unsigned int aiObjIdx, unsigned int targetObjIdx, int32_t stock) {
	const AiAttackSet* model = AiAttack_Model(aiObjIdx);
	const AiAttackOffset* offset;
	char key[XWA_MODEL_SOURCE_KEY_MAX];

	if ((model == NULL || model->offsetCount == 0) && s_defaults.offsetCount == 0) {
		return stock;
	}
	if (!XwaModelSwaps_TypeKey(g_objectTable[(uint16_t)targetObjIdx].objectType, key)) {
		return stock;
	}
	offset = AiAttack_FindOffset(model, key);
	if (offset == NULL) {
		offset = AiAttack_FindOffset(&s_defaults, key);
	}
	return offset != NULL ? offset->offset : stock;
}

int XwaAiAttack_LaserRange(unsigned int aiObjIdx, unsigned int rank, uint32_t* range) {
	const AiAttackSet* model = AiAttack_Model(aiObjIdx);
	int any = 0;
	int index;

	for (index = 0; index < XWA_AI_ATTACK_RANKS; ++index) {
		any |= (model != NULL && model->set[XWA_AI_ATTACK_LASER_RANGE + index]) ||
			   s_defaults.set[XWA_AI_ATTACK_LASER_RANGE + index];
	}
	if (!any) {
		return 0;
	}
	/* A rank above super ace uses the novice range. */
	if (rank >= XWA_AI_ATTACK_RANKS) {
		rank = 0;
	}
	*range = (uint32_t)XwaAiAttack_Value(aiObjIdx, (XwaAiAttackValue)(XWA_AI_ATTACK_LASER_RANGE + rank),
										 (int32_t)s_stockLaserRangeByRank[rank]);
	return 1;
}
