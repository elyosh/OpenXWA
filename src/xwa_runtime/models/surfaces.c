#include "xwa_runtime/models/surfaces.h"

#include "aeron/asset/flight_tags.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/flight/object/object.h"
#include "xwa_runtime/models/model_settings.h"

#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* The largest MGLT limit, as in engine_glows.c: a speed is at most 65535 raw
   units, so a larger limit would change nothing. */
#define SURFACES_MAX_MGLT 100000000

typedef struct SurfaceRule {
	char tag[AERON_FLIGHT_TAG_MAX];
	uint8_t hiddenWhenDisabled;
	/* 0: no limit. */
	int32_t minSpeedPercent;
	int32_t minSpeedMglt;
	int32_t minThrottlePercent;
	int32_t minOrderedSpeedMglt;
} SurfaceRule;

typedef struct SurfaceType {
	SurfaceRule* rules;
	uint16_t count;
} SurfaceType;

static SurfaceType s_types[OBJ_Count];

/* Reads key `key` of `entry` into `out`. Returns 0, logged, when the value
   is outside 1 to `max`. */
static int Surfaces_ReadLimit(const char* path, const char* tag, const AeronConfigNode* entry,
							  const char* key, int64_t max, int32_t* out) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(entry, key);
	const int64_t value = AeronConfigNode_Int(node, 0);

	if (node == NULL) {
		return 1;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < 1 || value > max) {
		Aeron_LogWarn("xwa.models", "%s: surfaces.%s.%s needs a number from 1 to %lld. The rule is ignored",
					  path, tag, key, (long long)max);
		return 0;
	}
	*out = (int32_t)value;
	return 1;
}

void XwaSurfaces_ClearModelSettings(void) {
	unsigned int objectType;

	for (objectType = 0; objectType < OBJ_Count; ++objectType) {
		free(s_types[objectType].rules);
	}
	memset(s_types, 0, sizeof s_types);
}

void XwaSurfaces_ReadModelSettings(unsigned int objectType, const char* path, const AeronConfigNode* group) {
	SurfaceType* type;
	size_t count;
	size_t index;

	if (objectType >= OBJ_Count || group == NULL) {
		return;
	}
	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: surfaces isn't a map. It is ignored", path);
		return;
	}
	type = &s_types[objectType];
	count = AeronConfigNode_MapCount(group);
	type->rules = (SurfaceRule*)calloc(count != 0 ? count : 1, sizeof *type->rules);
	if (type->rules == NULL) {
		return;
	}
	for (index = 0; index < count; ++index) {
		const char* tag = AeronConfigNode_MapKeyAt(group, index);
		const AeronConfigNode* entry = AeronConfigNode_MapValueAt(group, index);
		const AeronConfigNode* disabled = AeronConfigNode_MapGet(entry, "hidden_when_disabled");
		SurfaceRule rule;

		if (!Aeron_FlightTagValid(tag) || AeronConfigNode_Type(entry) != AERON_CONFIG_MAP) {
			Aeron_LogWarn("xwa.models", "%s: surfaces.%s needs a tag and a map. It is ignored", path, tag);
			continue;
		}
		if (disabled != NULL && AeronConfigNode_Type(disabled) != AERON_CONFIG_BOOL) {
			Aeron_LogWarn("xwa.models",
						  "%s: surfaces.%s.hidden_when_disabled needs true or false. The rule is ignored",
						  path, tag);
			continue;
		}
		memset(&rule, 0, sizeof rule);
		strcpy(rule.tag, tag);
		rule.hiddenWhenDisabled = (uint8_t)(AeronConfigNode_Bool(disabled, 0) != 0);
		if (!Surfaces_ReadLimit(path, tag, entry, "min_speed_percent", INT32_MAX, &rule.minSpeedPercent) ||
			!Surfaces_ReadLimit(path, tag, entry, "min_speed_mglt", SURFACES_MAX_MGLT, &rule.minSpeedMglt) ||
			!Surfaces_ReadLimit(path, tag, entry, "min_throttle_percent", 100, &rule.minThrottlePercent) ||
			!Surfaces_ReadLimit(path, tag, entry, "min_ordered_speed_mglt", SURFACES_MAX_MGLT,
								&rule.minOrderedSpeedMglt)) {
			continue;
		}
		if (type->count == XWA_SURFACES_MAX_RULES) {
			Aeron_LogWarn("xwa.models", "%s: surfaces has more than %d rules. surfaces.%s is ignored", path,
						  XWA_SURFACES_MAX_RULES, tag);
			continue;
		}
		type->rules[type->count++] = rule;
	}
}

/* A speed in raw units (2.25 per MGLT) is under `mglt` MGLT when 4 * speed
   < 9 * mglt, which is (int)(speed / 2.25f) < mglt without floats. */
static int Surfaces_UnderMglt(int speed, int32_t mglt) { return 4 * (int64_t)speed < 9 * (int64_t)mglt; }

static int Surfaces_RuleHides(const SurfaceRule* rule, const CraftData* craft, int speed) {
	const int maxSpeed = craft->aiFlight.maxSpeedCache;

	if (rule->hiddenWhenDisabled && craft->workingSubsystems == 0) {
		return 1;
	}
	if (rule->minSpeedPercent != 0 && maxSpeed != 0 && speed * 100 / maxSpeed < rule->minSpeedPercent) {
		return 1;
	}
	if (rule->minSpeedMglt != 0 && Surfaces_UnderMglt(speed, rule->minSpeedMglt)) {
		return 1;
	}
	if (rule->minThrottlePercent != 0 &&
		100 * (int64_t)craft->throttleSpeed < (int64_t)rule->minThrottlePercent * 65535) {
		return 1;
	}
	return rule->minOrderedSpeedMglt != 0 &&
		   Surfaces_UnderMglt(craft->commandedSpeed, rule->minOrderedSpeedMglt);
}

uint32_t XwaSurfaces_HiddenMask(unsigned int objectType, const CraftData* craft, uint16_t speed) {
	const SurfaceType* type;
	uint32_t mask = 0;
	uint16_t index;

	if (objectType >= OBJ_Count || craft == NULL) {
		return 0;
	}
	XwaModelSettings_Ensure(objectType);
	type = &s_types[objectType];
	for (index = 0; index < type->count; ++index) {
		/* The speed is read as a signed 16-bit value, as in engine_glows.c. */
		if (Surfaces_RuleHides(&type->rules[index], craft, (int16_t)speed)) {
			mask |= 1u << index;
		}
	}
	return mask;
}

const char* XwaSurfaces_RuleTag(unsigned int objectType, unsigned int rule) {
	if (objectType >= OBJ_Count || rule >= s_types[objectType].count) {
		return NULL;
	}
	return s_types[objectType].rules[rule].tag;
}
