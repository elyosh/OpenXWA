#include "xwa_runtime/models/mag_pulse.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/flight/object/object.h"
#include "xwa_runtime/models/flight_config.h"
#include "xwa_runtime/models/model_settings.h"
#include "xwa_runtime/models/units.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* The fire timer is 16-bit. */
#define MAG_PULSE_MAX_TICKS 65535

enum { MAG_PULSE_SMALL, MAG_PULSE_LARGE, MAG_PULSE_SIZES };

typedef struct MagPulseModel {
	uint8_t hasDrainOnly;
	uint8_t drainOnly;
	uint8_t hasTicks;
	uint16_t ticks;
} MagPulseModel;

/* The values of flight.yaml. */
static uint8_t s_hasDrainOnly;
static uint8_t s_drainOnly;
static uint8_t s_hasTicks[MAG_PULSE_SIZES];
static uint16_t s_ticks[MAG_PULSE_SIZES];

static MagPulseModel s_models[OBJ_Count];

/* Reads `node`, the key `name`, as true or false. Returns 0 when it isn't. */
static int MagPulse_ReadBool(const char* path, const char* name, const AeronConfigNode* node, uint8_t* out) {
	if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		Aeron_LogWarn("xwa.models", "%s:%d: mag_pulse.%s must be true or false. It is ignored", path,
					  AeronConfigNode_Line(node), name);
		return 0;
	}
	*out = AeronConfigNode_Bool(node, 0) != 0;
	return 1;
}

/* Reads `node`, the key `name`, as a time in milliseconds. Returns 0 when it
   isn't one, or when it doesn't fit in the fire timer. */
static int MagPulse_ReadTicks(const char* path, const char* name, const AeronConfigNode* node,
							  uint16_t* out) {
	int32_t ticks;

	if (!XwaUnits_Read(node, XWA_UNIT_MILLISECONDS, 0, MAG_PULSE_MAX_TICKS, &ticks)) {
		Aeron_LogWarn("xwa.models",
					  "%s:%d: mag_pulse.%s must be a number of milliseconds from 0 to %.0f. It is ignored",
					  path, AeronConfigNode_Line(node), name,
					  XwaUnits_FromGame(XWA_UNIT_MILLISECONDS, (double)MAG_PULSE_MAX_TICKS));
		return 0;
	}
	*out = (uint16_t)ticks;
	return 1;
}

/* Reads the `disable_time_ms` map of flight.yaml, by size. */
static void MagPulse_ReadSizes(const AeronConfigNode* node) {
	static const char* const sizes[MAG_PULSE_SIZES] = { "small", "large" };
	size_t index;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models",
					  "%s:%d: mag_pulse.disable_time_ms must be a map of small and large. It is ignored",
					  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(node));
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(node); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(node, index);
		const AeronConfigNode* child = AeronConfigNode_MapValueAt(node, index);
		int size;
		char name[64];

		size = 0;
		while (size < MAG_PULSE_SIZES && strcmp(key, sizes[size]) != 0) {
			++size;
		}
		snprintf(name, sizeof name, "disable_time_ms.%s", key);
		if (size == MAG_PULSE_SIZES) {
			Aeron_LogWarn("xwa.models", "%s:%d: mag_pulse.%s isn't a setting. It is ignored",
						  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(child), name);
			continue;
		}
		s_hasTicks[size] = (uint8_t)MagPulse_ReadTicks(XWA_FLIGHT_CONFIG_PATH, name, child, &s_ticks[size]);
	}
}

void XwaMagPulse_Init(void) {
	const AeronConfigNode* section = AeronConfigNode_MapGet(XwaFlightConfig_Root(), "mag_pulse");
	size_t index;

	s_hasDrainOnly = 0;
	s_drainOnly = 0;
	memset(s_hasTicks, 0, sizeof s_hasTicks);
	memset(s_ticks, 0, sizeof s_ticks);
	if (section != NULL && AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s:%d: mag_pulse isn't a map. It is ignored", XWA_FLIGHT_CONFIG_PATH,
					  AeronConfigNode_Line(section));
		section = NULL;
	}
	for (index = 0; index < AeronConfigNode_MapCount(section); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(section, index);
		const AeronConfigNode* child = AeronConfigNode_MapValueAt(section, index);

		if (strcmp(key, "drain_only") == 0) {
			s_hasDrainOnly = (uint8_t)MagPulse_ReadBool(XWA_FLIGHT_CONFIG_PATH, key, child, &s_drainOnly);
		} else if (strcmp(key, "disable_time_ms") == 0) {
			MagPulse_ReadSizes(child);
		} else {
			Aeron_LogWarn("xwa.models", "%s:%d: mag_pulse.%s isn't a setting. It is ignored",
						  XWA_FLIGHT_CONFIG_PATH, AeronConfigNode_Line(child), key);
		}
	}
}

void XwaMagPulse_ClearModelSettings(void) { memset(s_models, 0, sizeof s_models); }

void XwaMagPulse_ReadModelSettings(unsigned int objectType, const char* path, const AeronConfigNode* group) {
	MagPulseModel* model;
	size_t index;

	if (objectType >= OBJ_Count || group == NULL) {
		return;
	}
	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s:%d: mag_pulse isn't a map. It is ignored", path,
					  AeronConfigNode_Line(group));
		return;
	}
	model = &s_models[objectType];
	for (index = 0; index < AeronConfigNode_MapCount(group); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(group, index);
		const AeronConfigNode* child = AeronConfigNode_MapValueAt(group, index);

		if (strcmp(key, "drain_only") == 0) {
			model->hasDrainOnly = (uint8_t)MagPulse_ReadBool(path, key, child, &model->drainOnly);
		} else if (strcmp(key, "disable_time_ms") == 0) {
			model->hasTicks = (uint8_t)MagPulse_ReadTicks(path, key, child, &model->ticks);
		} else {
			Aeron_LogWarn("xwa.models", "%s:%d: mag_pulse.%s isn't a setting. It is ignored", path,
						  AeronConfigNode_Line(child), key);
		}
	}
}

/* The values of the model that object `objIdx` loads, or NULL. */
static const MagPulseModel* MagPulse_Model(unsigned int objIdx) {
	const unsigned int objectType = g_objectTable[(uint16_t)objIdx].objectType;

	XwaModelSettings_Ensure(objectType);
	return objectType < OBJ_Count ? &s_models[objectType] : NULL;
}

int XwaMagPulse_DrainOnly(unsigned int objIdx) {
	const MagPulseModel* model = MagPulse_Model(objIdx);

	if (model != NULL && model->hasDrainOnly) {
		return model->drainOnly;
	}
	return s_hasDrainOnly ? s_drainOnly : 0;
}

uint16_t XwaMagPulse_DisableTicks(unsigned int objIdx, int small, uint16_t stock) {
	const MagPulseModel* model = MagPulse_Model(objIdx);
	const int size = small ? MAG_PULSE_SMALL : MAG_PULSE_LARGE;

	if (model != NULL && model->hasTicks) {
		return model->ticks;
	}
	return s_hasTicks[size] ? s_ticks[size] : stock;
}
