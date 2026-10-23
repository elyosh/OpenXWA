#include "xwa_runtime/models/warhead_lock.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/flight/object/object.h"
#include "xwa_runtime/models/model_settings.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* Each value's group (NULL for a key of `warhead_lock` itself), key and
   limits. */
static const struct {
	const char* group;
	const char* key;
	int64_t min;
	int64_t max;
} s_values[XWA_WARHEAD_LOCK_VALUES] = {
	{ "lock_range", "small", 0, INT32_MAX },
	{ "lock_range", "big", 0, INT32_MAX },
	{ "lock_range", "transport", 0, INT32_MAX },
	/* The HUD divides by lock_ticks / 200, and the lock ticks are 16-bit. */
	{ NULL, "lock_ticks", 200, 32767 },
	{ NULL, "ticks_per_homing_level", 1, 32767 },
	{ "ai_lock_range", "small", 0, INT32_MAX },
	{ "ai_lock_range", "big", 0, INT32_MAX },
	{ "ai_lock_range", "big_top_tier", 0, INT32_MAX },
	{ NULL, "ai_lock_ticks_per_tier", 0, 65535 },
};

typedef struct WarheadLockModel {
	int32_t value[XWA_WARHEAD_LOCK_VALUES];
	uint8_t set[XWA_WARHEAD_LOCK_VALUES];
} WarheadLockModel;

static WarheadLockModel s_models[OBJ_Count];

void XwaWarheadLock_ClearModelSettings(void) { memset(s_models, 0, sizeof s_models); }

/* Reads `value` from `node`, the key `name` of `group`. */
static void WarheadLock_ReadValue(unsigned int objectType, const char* path, const char* name,
								  XwaWarheadLockValue value, const AeronConfigNode* node) {
	const int64_t number = AeronConfigNode_Int(node, -1);

	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || number < s_values[value].min ||
		number > s_values[value].max) {
		Aeron_LogWarn("xwa.models", "%s: warhead_lock.%s must be a number from %lld to %lld. It is ignored",
					  path, name, (long long)s_values[value].min, (long long)s_values[value].max);
		return;
	}
	s_models[objectType].value[value] = (int32_t)number;
	s_models[objectType].set[value] = 1;
}

/* Reads the keys of map `node`, which is `group`, or `warhead_lock` itself
   when `group` is NULL. */
static void WarheadLock_ReadMap(unsigned int objectType, const char* path, const char* group,
								const AeronConfigNode* node) {
	size_t index;

	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* child = AeronConfigNode_MapValueAt(node, index);
		char name[64];
		int value;

		if (group == NULL && (strcmp(key, "lock_range") == 0 || strcmp(key, "ai_lock_range") == 0)) {
			if (AeronConfigNode_Type(child) != AERON_CONFIG_MAP) {
				Aeron_LogWarn("xwa.models", "%s: warhead_lock.%s isn't a map. It is ignored", path, key);
				continue;
			}
			WarheadLock_ReadMap(objectType, path, key, child);
			continue;
		}
		for (value = 0; value < XWA_WARHEAD_LOCK_VALUES; ++value) {
			if ((group == NULL
					 ? s_values[value].group == NULL
					 : s_values[value].group != NULL && strcmp(s_values[value].group, group) == 0) &&
				strcmp(s_values[value].key, key) == 0) {
				break;
			}
		}
		snprintf(name, sizeof name, "%s%s%s", group != NULL ? group : "", group != NULL ? "." : "", key);
		if (value == XWA_WARHEAD_LOCK_VALUES) {
			Aeron_LogWarn("xwa.models", "%s: warhead_lock.%s isn't a setting. It is ignored", path, name);
			continue;
		}
		WarheadLock_ReadValue(objectType, path, name, (XwaWarheadLockValue)value, child);
	}
}

void XwaWarheadLock_ReadModelSettings(unsigned int objectType, const char* path,
									  const AeronConfigNode* group) {
	if (objectType >= OBJ_Count || group == NULL) {
		return;
	}
	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: warhead_lock isn't a map. It is ignored", path);
		return;
	}
	WarheadLock_ReadMap(objectType, path, NULL, group);
}

int XwaWarheadLock_Value(unsigned int objIdx, XwaWarheadLockValue value, int stock) {
	const unsigned int objectType = g_objectTable[(uint16_t)objIdx].objectType;

	XwaModelSettings_Ensure(objectType);
	if (objectType >= OBJ_Count || (unsigned int)value >= XWA_WARHEAD_LOCK_VALUES ||
		!s_models[objectType].set[value]) {
		return stock;
	}
	return s_models[objectType].value[value];
}
