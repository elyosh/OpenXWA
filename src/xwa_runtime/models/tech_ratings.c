#include "xwa_runtime/models/tech_ratings.h"

#include "aeron/aeron.h"
#include "aeron/asset/flight_model.h"
#include "aeron/asset/opt_model.h"
#include "aeron/asset/opt_sim_model.h"
#include "aeron/vfs.h"
#include "xwa/assets/file_io.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/object_type_info.h"
#include "xwa/assets/ship_list.h"
#include "xwa/flight/fediskio.h"
#include "xwa/util/memory.h"
#include "xwa_runtime/models/model_source.h"
#include "xwa_runtime/models/model_swaps.h"
#include "xwa_runtime/models/stats_profiles.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

enum {
	GLB_MAGIC = 0x46546C67,      /* "glTF" */
	GLB_JSON_CHUNK = 0x4E4F534A, /* "JSON" */
	/* The model loader halves the sizes until each fits in this. */
	BOUND_SIZE_LIMIT = 640,
};

/* The size of a model file, read once for the session. */
typedef struct TechRatingsSize {
	char path[256];
	int found;
	int boundSize;
} TechRatingsSize;

static TechRatingsSize* s_sizes;
static int s_sizeCount;

/* Reads the bounds of the GLB `path`, in OPT units, from its JSON part. A GLB
   whose components don't all have a descriptor needs its meshes, so it is
   then read whole. */
static int TechRatings_GlbBounds(const char* path, float boundsMin[3], float boundsMax[3]) {
	AeronFile* file = NULL;
	uint32_t header[5];
	size_t read = 0;
	char* json = NULL;
	AeronFlightBounds bounds;
	int found = 0;
	int axis;

	if (AeronVfs_Open(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, path, AERON_VFS_READ, &file)) {
		if (AeronVfs_Read(file, header, sizeof header, &read) && read == sizeof header &&
			header[0] == GLB_MAGIC && header[4] == GLB_JSON_CHUNK && header[3] > 0 &&
			(json = (char*)malloc(header[3])) != NULL && AeronVfs_Read(file, json, header[3], &read) &&
			read == header[3]) {
			found = Aeron_FlightModelReadDescriptorBounds(json, header[3], &bounds);
		}
		free(json);
		AeronVfs_Close(file);
	}
	if (!found) {
		uint8_t* glb = NULL;
		size_t glbSize = 0;
		AeronFlightModel model;
		uint32_t index;

		if (!AeronVfs_ReadAll(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, path, 0, &glb, &glbSize)) {
			return 0;
		}
		if (Aeron_FlightModelBuildSemanticsMemory(glb, glbSize, path, &model)) {
			for (index = 0; index < model.component_count; ++index) {
				const AeronFlightBounds* box = &model.components[index].descriptor_bounds;

				if (index == 0) {
					bounds = *box;
					continue;
				}
				bounds.min.x = box->min.x < bounds.min.x ? box->min.x : bounds.min.x;
				bounds.min.y = box->min.y < bounds.min.y ? box->min.y : bounds.min.y;
				bounds.min.z = box->min.z < bounds.min.z ? box->min.z : bounds.min.z;
				bounds.max.x = box->max.x > bounds.max.x ? box->max.x : bounds.max.x;
				bounds.max.y = box->max.y > bounds.max.y ? box->max.y : bounds.max.y;
				bounds.max.z = box->max.z > bounds.max.z ? box->max.z : bounds.max.z;
			}
			found = model.component_count > 0;
			Aeron_FlightModelFreeSemantics(&model);
		}
		free(glb);
		if (!found) {
			return 0;
		}
	}
	/* In OPT units, as the synthesized model's descriptors have them. */
	boundsMin[0] = bounds.min.x;
	boundsMin[1] = bounds.min.y;
	boundsMin[2] = bounds.min.z;
	boundsMax[0] = bounds.max.x;
	boundsMax[1] = bounds.max.y;
	boundsMax[2] = bounds.max.z;
	for (axis = 0; axis < 3; ++axis) {
		boundsMin[axis] = (float)((double)boundsMin[axis] * AERON_OPT_SIM_UNITS_PER_METER);
		boundsMax[axis] = (float)((double)boundsMax[axis] * AERON_OPT_SIM_UNITS_PER_METER);
	}
	return 1;
}

/* Reads the bounds of the OPT `fileName` from its mesh descriptors. */
static int TechRatings_OptBounds(const char* fileName, float boundsMin[3], float boundsMax[3]) {
	XwaFile* stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
	uint8_t* bytes;
	size_t size;
	AeronFlightBounds bounds;
	int found = 0;

	if (stream == NULL) {
		return 0;
	}
	size = (size_t)File_GetSize(stream);
	bytes = size > 0 ? (uint8_t*)malloc(size) : NULL;
	if (bytes != NULL && File_ReadCount(stream, bytes, size)) {
		found = Aeron_OptModelReadBounds(bytes, size, &bounds);
	}
	free(bytes);
	File_Close(stream);
	if (found) {
		boundsMin[0] = bounds.min.x;
		boundsMin[1] = bounds.min.y;
		boundsMin[2] = bounds.min.z;
		boundsMax[0] = bounds.max.x;
		boundsMax[1] = bounds.max.y;
		boundsMax[2] = bounds.max.z;
	}
	return found;
}

/* The largest size of the model, as FeDiskIo stores it in the record (each
   size halved until all fit in 640, then the shift kept) and
   BuildCraftTechStats reads it back. */
static int TechRatings_RoundedSize(const float boundsMin[3], const float boundsMax[3]) {
	int size[3];
	int shift = 0;
	int largest = 0;
	int axis;

	for (axis = 0; axis < 3; ++axis) {
		size[axis] = (int)(boundsMax[axis] - boundsMin[axis]);
		if (size[axis] < 0) {
			size[axis] = -size[axis];
		}
	}
	while (size[0] > BOUND_SIZE_LIMIT || size[1] > BOUND_SIZE_LIMIT || size[2] > BOUND_SIZE_LIMIT) {
		for (axis = 0; axis < 3; ++axis) {
			size[axis] >>= 1;
		}
		++shift;
	}
	for (axis = 0; axis < 3; ++axis) {
		if ((size[axis] << shift) > largest) {
			largest = size[axis] << shift;
		}
	}
	return largest;
}

int XwaTechRatings_BoundSize(int craftType, int* boundSize) {
	const char* fileName = FeDiskIo_GetCraftModelName((unsigned int)craftType);
	char swapped[256];
	char glbPath[256];
	const char* path;
	XwaModelSource source;
	float boundsMin[3];
	float boundsMax[3];
	TechRatingsSize* entry;
	int found;
	int size = 0;
	int i;

	if (fileName == NULL) {
		return 0;
	}
	if (XwaModelSwaps_FileName(fileName, swapped, sizeof swapped)) {
		fileName = swapped;
	}
	source = XwaModelSource_Resolve(fileName, glbPath, sizeof glbPath);
	if (source == XWA_MODEL_SOURCE_SUPPRESSED) {
		return 0;
	}
	path = source == XWA_MODEL_SOURCE_GLB ? glbPath : fileName;
	for (i = 0; i < s_sizeCount; ++i) {
		if (strcmp(s_sizes[i].path, path) == 0) {
			if (s_sizes[i].found) {
				*boundSize = s_sizes[i].boundSize;
			}
			return s_sizes[i].found;
		}
	}

	found = source == XWA_MODEL_SOURCE_GLB ? TechRatings_GlbBounds(path, boundsMin, boundsMax)
										   : TechRatings_OptBounds(path, boundsMin, boundsMax);
	if (found) {
		size = TechRatings_RoundedSize(boundsMin, boundsMax);
		*boundSize = size;
	}
	entry = (TechRatingsSize*)realloc(s_sizes, sizeof *s_sizes * (size_t)(s_sizeCount + 1));
	if (entry != NULL) {
		s_sizes = entry;
		entry = &s_sizes[s_sizeCount++];
		snprintf(entry->path, sizeof entry->path, "%s", path);
		entry->found = found;
		entry->boundSize = size;
	}
	return found;
}

/* The factor of the shield and hull ratings of a genus, as
   BuildCraftTechStats scales them. */
static int TechRatings_GenusFactor(int genusId) {
	if (genusId == 4 || genusId == 5) {
		return 16;
	}
	if (genusId == 3 || genusId == 17) {
		return 4;
	}
	return 1;
}

void XwaTechRatings_ApplyProfile(unsigned int objectType, CraftTechStats* stats) {
	const ModelDef* record;
	const int factor = TechRatings_GenusFactor(stats->genusId);
	int32_t value;
	int32_t rollRate;
	int32_t pitchRate;
	int32_t hasShields;
	int hasShieldsSet;
	int rates;

	if (objectType >= OBJ_Count || (uint16_t)g_objectTypes[objectType].modelDefIndex == 0xFFFF) {
		return;
	}
	record = &g_modelDefs[(uint16_t)g_objectTypes[objectType].modelDefIndex];

	if (XwaStatsProfiles_DefaultValue(objectType, XWA_STATS_SPEED, &value)) {
		stats->speedRating =
			(int)((double)(uint16_t)value * g_techLibraryRatingScale - g_techLibraryNegativeRoundingOffset);
	}
	if (XwaStatsProfiles_DefaultValue(objectType, XWA_STATS_ACCELERATION, &value)) {
		stats->accelerationRating =
			(int)((double)(uint16_t)value * g_techLibraryRatingScale - g_techLibraryNegativeRoundingOffset);
	}
	rollRate = record->rollRate;
	pitchRate = record->pitchRate;
	rates = XwaStatsProfiles_DefaultValue(objectType, XWA_STATS_ROLL_RATE, &rollRate);
	rates |= XwaStatsProfiles_DefaultValue(objectType, XWA_STATS_PITCH_RATE, &pitchRate);
	if (rates) {
		stats->maneuverRating =
			(int)((double)((uint16_t)rollRate + (uint16_t)pitchRate) * g_techLibraryManeuverScale -
				  g_techLibraryNegativeRoundingOffset);
	}

	/* The profile's values are in the record's units: its shields and hull
	   are the profile's numbers times 50 and 105, divided by the factor.
	   Rounding these back gives the profile's numbers, where the stock
	   division first would lose up to one factor. */
	hasShields = record->hasShields;
	hasShieldsSet = XwaStatsProfiles_DefaultValue(objectType, XWA_STATS_HAS_SHIELDS, &hasShields);
	if (XwaStatsProfiles_DefaultValue(objectType, XWA_STATS_SHIELDS, &value)) {
		stats->shieldRating = hasShields ? (int)(((int64_t)value * factor + 25) / 50) : 0;
	} else if (hasShieldsSet) {
		stats->shieldRating = hasShields ? record->shieldStrength / 50 * factor : 0;
	}
	if (XwaStatsProfiles_DefaultValue(objectType, XWA_STATS_HULL, &value)) {
		stats->hullRating = (int)(((int64_t)value * factor + 52) / 105);
	}
}

void XwaTechRatings_ProfileRecord(unsigned int objectType, ModelDef* record) {
	int32_t value;

	if (XwaStatsProfiles_DefaultValue(objectType, XWA_STATS_SPEED, &value)) {
		record->maxSpeed = (uint16_t)value;
	}
	if (XwaStatsProfiles_DefaultValue(objectType, XWA_STATS_ACCELERATION, &value)) {
		record->accelRate = (uint16_t)value;
	}
	if (XwaStatsProfiles_DefaultValue(objectType, XWA_STATS_PITCH_RATE, &value)) {
		record->pitchRate = (int16_t)value;
	}
	if (XwaStatsProfiles_DefaultValue(objectType, XWA_STATS_ROLL_RATE, &value)) {
		record->rollRate = (int16_t)value;
	}
	if (XwaStatsProfiles_DefaultValue(objectType, XWA_STATS_YAW_RATE, &value)) {
		record->yawRate = (int16_t)value;
	}
	if (XwaStatsProfiles_DefaultValue(objectType, XWA_STATS_HAS_SHIELDS, &value)) {
		record->hasShields = value != 0;
	}
	if (XwaStatsProfiles_DefaultValue(objectType, XWA_STATS_SHIELDS, &value)) {
		record->shieldStrength = value;
	}
	if (XwaStatsProfiles_DefaultValue(objectType, XWA_STATS_HULL, &value)) {
		record->hullStrength = value;
	}
}

void XwaTechRatings_BuildTable(CraftTechStats** table, int* count) {
	int shipIndex;
	int built = 0;

	if (*table != NULL) {
		Mem_Free(*table);
		*table = NULL;
	}
	*count = 0;
	if (g_shipCount <= 1) {
		return;
	}
	*table = (CraftTechStats*)Mem_Alloc(sizeof(CraftTechStats) * (size_t)(g_shipCount - 1));
	if (*table == NULL) {
		return;
	}
	/* The ship list as TechLibrary_GenerateCraftSpecCache walks it. */
	for (shipIndex = 1; shipIndex < g_shipCount; ++shipIndex) {
		CraftTechStats* stats = &(*table)[built];

		memset(stats, 0, sizeof *stats);
		stats->craftType = g_shipList[shipIndex].craftType;
		if (BuildCraftTechStats(stats)) {
			++built;
		}
	}
	*count = built;
}
