#include "xwa_runtime/config/modern_speed_effect_options.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"

#include <math.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define SPEED_EFFECT_FLIGHT_YAML "flight/flight.yaml"

#define SPEED_EFFECT_SWITCH(field) { #field, 1, 0.0f, 1.0f, offsetof(XwaModernSpeedEffectOptions, field) }
#define SPEED_EFFECT_NUMBER(field, min, max)                                                                 \
	{ #field, 0, min, max, offsetof(XwaModernSpeedEffectOptions, field) }

static const XwaModernSpeedEffectOptionKey g_speedEffectOptionKeys[] = {
	SPEED_EFFECT_SWITCH(enabled),
	SPEED_EFFECT_NUMBER(brightness, 0.0f, 1.0f),
	SPEED_EFFECT_NUMBER(particles, 0.0f, 1024.0f),
};

#define SPEED_EFFECT_KEY_COUNT (sizeof g_speedEffectOptionKeys / sizeof g_speedEffectOptionKeys[0])

static struct {
	XwaModernSpeedEffectOptions shipped;
	XwaModernSpeedEffectOptions mod;
	XwaModernSpeedEffectOptions player;
	unsigned int override_mask;
	int has_mod;
} g_modernSpeedEffectOptions;

const XwaModernSpeedEffectOptionKey* XwaModernSpeedEffectOptions_Keys(size_t* count) {
	if (count) {
		*count = SPEED_EFFECT_KEY_COUNT;
	}
	return g_speedEffectOptionKeys;
}

void XwaModernSpeedEffectOptions_Configure(const XwaModernSpeedEffectOptions* shipped,
										   const XwaModernSpeedEffectOptions* player,
										   unsigned int override_mask) {
	memset(&g_modernSpeedEffectOptions, 0, sizeof g_modernSpeedEffectOptions);
	if (!shipped || !player) {
		Aeron_LogError("xwa.config", "cannot configure the speed effect options without values");
		return;
	}
	g_modernSpeedEffectOptions.shipped = *shipped;
	g_modernSpeedEffectOptions.player = *player;
	g_modernSpeedEffectOptions.override_mask = override_mask;
}

/* Reads one key of the mod's `speed_effect` section into `options`. A wrong
   value is logged and ignored. */
static void SpeedEffectOptions_ReadKey(const XwaModernSpeedEffectOptionKey* key, const AeronConfigNode* node,
									   XwaModernSpeedEffectOptions* options) {
	char* field = (char*)options + key->offset;
	const AeronConfigNodeType type = AeronConfigNode_Type(node);

	if (key->is_switch) {
		int value;

		if (type != AERON_CONFIG_BOOL) {
			Aeron_LogWarn("xwa.flight", "%s:%d: speed_effect.%s must be true or false; it is ignored",
						  SPEED_EFFECT_FLIGHT_YAML, AeronConfigNode_Line(node), key->name);
			return;
		}
		value = AeronConfigNode_Bool(node, 0) != 0;
		memcpy(field, &value, sizeof value);
	} else {
		const double number = AeronConfigNode_Float(node, NAN);
		float value;

		if ((type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT) ||
			!(number >= key->min && number <= key->max)) {
			Aeron_LogWarn("xwa.flight",
						  "%s:%d: speed_effect.%s must be a number from %g to %g; it is ignored",
						  SPEED_EFFECT_FLIGHT_YAML, AeronConfigNode_Line(node), key->name, (double)key->min,
						  (double)key->max);
			return;
		}
		value = (float)number;
		memcpy(field, &value, sizeof value);
	}
}

void XwaModernSpeedEffectOptions_LoadMod(void) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* section;
	XwaModernSpeedEffectOptions options;
	size_t i;

	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, SPEED_EFFECT_FLIGHT_YAML, &file,
									&error)) {
		/* The backdrops code already logs this error. */
		return;
	}
	section = AeronConfigNode_MapGet(AeronConfigFile_Root(file), "speed_effect");
	if (section == NULL) {
		AeronConfigFile_Destroy(file);
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.flight", "%s:%d: speed_effect must be a map; it is ignored",
					   SPEED_EFFECT_FLIGHT_YAML, AeronConfigNode_Line(section));
		AeronConfigFile_Destroy(file);
		return;
	}

	options = g_modernSpeedEffectOptions.shipped;
	for (i = 0; i < AeronConfigNode_MapCount(section); ++i) {
		const char* name = AeronConfigNode_MapKeyAt(section, i);
		const AeronConfigNode* node = AeronConfigNode_MapValueAt(section, i);
		size_t k;

		for (k = 0; k < SPEED_EFFECT_KEY_COUNT && strcmp(g_speedEffectOptionKeys[k].name, name) != 0; ++k) {
		}
		if (k == SPEED_EFFECT_KEY_COUNT) {
			Aeron_LogWarn("xwa.flight", "%s:%d: speed_effect.%s isn't a speed effect setting; it is ignored",
						  SPEED_EFFECT_FLIGHT_YAML, AeronConfigNode_Line(node), name);
			continue;
		}
		SpeedEffectOptions_ReadKey(&g_speedEffectOptionKeys[k], node, &options);
	}
	g_modernSpeedEffectOptions.mod = options;
	g_modernSpeedEffectOptions.has_mod = 1;
	AeronConfigFile_Destroy(file);
}

void XwaModernSpeedEffectOptions_Get(XwaModernSpeedEffectOptions* out) {
	size_t i;

	if (!out) {
		return;
	}
	*out = g_modernSpeedEffectOptions.has_mod ? g_modernSpeedEffectOptions.mod
											  : g_modernSpeedEffectOptions.shipped;
	for (i = 0; i < SPEED_EFFECT_KEY_COUNT; ++i) {
		if (g_modernSpeedEffectOptions.override_mask & (1u << i)) {
			const size_t offset = g_speedEffectOptionKeys[i].offset;
			const size_t size = g_speedEffectOptionKeys[i].is_switch ? sizeof(int) : sizeof(float);
			memcpy((char*)out + offset, (const char*)&g_modernSpeedEffectOptions.player + offset, size);
		}
	}
}
