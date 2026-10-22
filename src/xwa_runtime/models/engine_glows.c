#include "xwa_runtime/models/engine_glows.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa_runtime/models/model_settings.h"

#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* The largest `min_speed_mglt`, so that the limit in raw units fits in an
   int. A speed is at most 65535, so a larger limit would change nothing. */
#define ENGINE_GLOWS_MAX_MGLT 100000000

typedef struct EngineGlowRule {
	uint8_t glow;
	/* 0: no limit. */
	int32_t minPercent;
	/* The MGLT limit in raw speed units. 0: no limit. */
	int32_t minSpeed;
} EngineGlowRule;

typedef struct EngineGlowType {
	EngineGlowRule* rules;
	uint16_t count;
} EngineGlowType;

static EngineGlowType s_types[OBJ_Count];

/* Reads key `key` of `entry` into `out`. Returns 0, logged, when the value
   is outside 1 to `max`. */
static int EngineGlows_ReadLimit(const char* path, const char* glow, const AeronConfigNode* entry,
								 const char* key, int64_t max, int32_t* out) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(entry, key);
	const int64_t value = AeronConfigNode_Int(node, 0);

	if (node == NULL) {
		return 1;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < 1 || value > max) {
		Aeron_LogWarn("xwa.models",
					  "%s: engine_glows.%s.%s needs a number from 1 to %lld. The glow is ignored", path, glow,
					  key, (long long)max);
		return 0;
	}
	*out = (int32_t)value;
	return 1;
}

void XwaEngineGlows_ClearModelSettings(void) {
	unsigned int objectType;

	for (objectType = 0; objectType < OBJ_Count; ++objectType) {
		free(s_types[objectType].rules);
	}
	memset(s_types, 0, sizeof s_types);
}

void XwaEngineGlows_ReadModelSettings(unsigned int objectType, const char* path,
									  const AeronConfigNode* group) {
	EngineGlowType* type;
	size_t count;
	size_t index;

	if (objectType >= OBJ_Count || group == NULL) {
		return;
	}
	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: engine_glows isn't a map. It is ignored", path);
		return;
	}
	type = &s_types[objectType];
	count = AeronConfigNode_MapCount(group);
	type->rules = (EngineGlowRule*)calloc(count != 0 ? count : 1, sizeof *type->rules);
	if (type->rules == NULL) {
		return;
	}
	for (index = 0; index < count; ++index) {
		const char* key = AeronConfigNode_MapKeyAt(group, index);
		const AeronConfigNode* entry = AeronConfigNode_MapValueAt(group, index);
		EngineGlowRule rule;
		int32_t mglt = 0;
		char* end;
		const long glow = strtol(key, &end, 10);

		if (*key == '\0' || *end != '\0' || glow < 0 || glow > 255 ||
			AeronConfigNode_Type(entry) != AERON_CONFIG_MAP) {
			Aeron_LogWarn("xwa.models",
						  "%s: engine_glows.%s needs a glow from 0 to 255 and a map. It is ignored", path,
						  key);
			continue;
		}
		memset(&rule, 0, sizeof rule);
		rule.glow = (uint8_t)glow;
		if (!EngineGlows_ReadLimit(path, key, entry, "min_speed_percent", INT32_MAX, &rule.minPercent) ||
			!EngineGlows_ReadLimit(path, key, entry, "min_speed_mglt", ENGINE_GLOWS_MAX_MGLT, &mglt)) {
			continue;
		}
		/* The limit is (int)(mglt * 2.25f + 0.5f) raw units, in integers. */
		rule.minSpeed = mglt != 0 ? (9 * mglt + 2) / 4 : 0;
		if (rule.minPercent != 0 || rule.minSpeed != 0) {
			type->rules[type->count++] = rule;
		}
	}
}

/* Both speeds are read as signed 16-bit values. */
static int EngineGlows_RuleHides(const EngineGlowRule* rule, int speed, int maxSpeed) {
	const int speedPercent = speed * 100 / maxSpeed;

	if (rule->minPercent != 0 && speedPercent < rule->minPercent) {
		return 1;
	}
	return rule->minSpeed != 0 && speed < rule->minSpeed;
}

int XwaEngineGlows_Hidden(unsigned int objectType, unsigned int glowIndex, uint16_t speed,
						  uint16_t maxSpeed) {
	const EngineGlowType* type;
	uint16_t index;

	if (objectType >= OBJ_Count || (int16_t)maxSpeed == 0) {
		return 0;
	}
	XwaModelSettings_Ensure(objectType);
	type = &s_types[objectType];
	for (index = 0; index < type->count; ++index) {
		if (type->rules[index].glow == glowIndex) {
			return EngineGlows_RuleHides(&type->rules[index], (int16_t)speed, (int16_t)maxSpeed);
		}
	}
	return 0;
}

void XwaEngineGlows_HiddenMask(unsigned int objectType, uint16_t speed, uint16_t maxSpeed,
							   uint32_t mask[XWA_ENGINE_GLOWS_MASK_WORDS]) {
	const EngineGlowType* type;
	uint16_t index;

	memset(mask, 0, XWA_ENGINE_GLOWS_MASK_WORDS * sizeof mask[0]);
	if (objectType >= OBJ_Count || (int16_t)maxSpeed == 0) {
		return;
	}
	XwaModelSettings_Ensure(objectType);
	type = &s_types[objectType];
	for (index = 0; index < type->count; ++index) {
		const EngineGlowRule* rule = &type->rules[index];

		if (EngineGlows_RuleHides(rule, (int16_t)speed, (int16_t)maxSpeed)) {
			mask[rule->glow >> 5] |= 1u << (rule->glow & 31);
		}
	}
}
