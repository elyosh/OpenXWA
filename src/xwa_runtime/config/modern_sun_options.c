#include "xwa_runtime/config/modern_sun_options.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"

#include <math.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define SUN_FLIGHT_YAML "flight/flight.yaml"

#define SUN_SWITCH(field) { #field, 1, 0.0f, 1.0f, offsetof(XwaModernSunOptions, field) }
#define SUN_NUMBER(field, min, max) { #field, 0, min, max, offsetof(XwaModernSunOptions, field) }

static const XwaModernSunOptionKey g_sunOptionKeys[] = {
	SUN_SWITCH(procedural_sun),
	SUN_SWITCH(procedural_flare),
	SUN_NUMBER(flare_intensity, 0.0f, 10.0f),
};

#define SUN_KEY_COUNT (sizeof g_sunOptionKeys / sizeof g_sunOptionKeys[0])

static struct {
	XwaModernSunOptions shipped;
	XwaModernSunOptions mod;
	XwaModernSunOptions player;
	unsigned int override_mask;
	int has_mod;
} g_modernSunOptions;

const XwaModernSunOptionKey* XwaModernSunOptions_Keys(size_t* count) {
	if (count) {
		*count = SUN_KEY_COUNT;
	}
	return g_sunOptionKeys;
}

void XwaModernSunOptions_Configure(const XwaModernSunOptions* shipped, const XwaModernSunOptions* player,
								   unsigned int override_mask) {
	memset(&g_modernSunOptions, 0, sizeof g_modernSunOptions);
	if (!shipped || !player) {
		Aeron_LogError("xwa.config", "cannot configure the sun options without values");
		return;
	}
	g_modernSunOptions.shipped = *shipped;
	g_modernSunOptions.player = *player;
	g_modernSunOptions.override_mask = override_mask;
}

/* Reads one key of the mod's `suns` section into `options`. Returns 0
   (logged) when the value is wrong. */
static int SunOptions_ReadKey(const XwaModernSunOptionKey* key, const AeronConfigNode* node,
							  XwaModernSunOptions* options) {
	char* field = (char*)options + key->offset;
	const AeronConfigNodeType type = AeronConfigNode_Type(node);

	if (key->is_switch) {
		int value;

		if (type != AERON_CONFIG_BOOL) {
			Aeron_LogWarn("xwa.flight", "%s:%d: suns.%s must be true or false; it is ignored",
						  SUN_FLIGHT_YAML, AeronConfigNode_Line(node), key->name);
			return 0;
		}
		value = AeronConfigNode_Bool(node, 0) != 0;
		memcpy(field, &value, sizeof value);
	} else {
		const double number = AeronConfigNode_Float(node, NAN);
		float value;

		if ((type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT) ||
			!(number >= key->min && number <= key->max)) {
			Aeron_LogWarn("xwa.flight", "%s:%d: suns.%s must be a number from %g to %g; it is ignored",
						  SUN_FLIGHT_YAML, AeronConfigNode_Line(node), key->name, (double)key->min,
						  (double)key->max);
			return 0;
		}
		value = (float)number;
		memcpy(field, &value, sizeof value);
	}
	return 1;
}

void XwaModernSunOptions_LoadMod(void) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* section;
	XwaModernSunOptions options;
	size_t i;

	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, SUN_FLIGHT_YAML, &file, &error)) {
		/* The backdrops code already logs this error. */
		return;
	}
	root = AeronConfigFile_Root(file);
	section = AeronConfigNode_MapGet(root, "suns");
	if (AeronConfigNode_Type(AeronConfigNode_MapGet(root, "version")) != AERON_CONFIG_INT ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) != 1 || section == NULL) {
		AeronConfigFile_Destroy(file);
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.flight", "%s:%d: suns must be a map; it is ignored", SUN_FLIGHT_YAML,
					   AeronConfigNode_Line(section));
		AeronConfigFile_Destroy(file);
		return;
	}

	options = g_modernSunOptions.shipped;
	for (i = 0; i < AeronConfigNode_MapCount(section); ++i) {
		const char* name = AeronConfigNode_MapKeyAt(section, i);
		const AeronConfigNode* node = AeronConfigNode_MapValueAt(section, i);
		size_t k;

		for (k = 0; k < SUN_KEY_COUNT && strcmp(g_sunOptionKeys[k].name, name) != 0; ++k) {
		}
		if (k == SUN_KEY_COUNT) {
			Aeron_LogWarn("xwa.flight", "%s:%d: suns.%s isn't a sun setting; it is ignored", SUN_FLIGHT_YAML,
						  AeronConfigNode_Line(node), name);
			continue;
		}
		SunOptions_ReadKey(&g_sunOptionKeys[k], node, &options);
	}
	g_modernSunOptions.mod = options;
	g_modernSunOptions.has_mod = 1;
	AeronConfigFile_Destroy(file);
}

void XwaModernSunOptions_Get(XwaModernSunOptions* out) {
	size_t i;

	if (!out) {
		return;
	}
	*out = g_modernSunOptions.has_mod ? g_modernSunOptions.mod : g_modernSunOptions.shipped;
	for (i = 0; i < SUN_KEY_COUNT; ++i) {
		if (g_modernSunOptions.override_mask & (1u << i)) {
			const size_t offset = g_sunOptionKeys[i].offset;
			const size_t size = g_sunOptionKeys[i].is_switch ? sizeof(int) : sizeof(float);
			memcpy((char*)out + offset, (const char*)&g_modernSunOptions.player + offset, size);
		}
	}
}
