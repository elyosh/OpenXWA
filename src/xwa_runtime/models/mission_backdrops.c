#include "xwa_runtime/models/mission_backdrops.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type_info.h"

#include <string.h>

/* Modern port module; no original counterpart. */

#define MISSION_BACKDROPS_PATH "flight/flight.yaml"
#define MISSION_BACKDROPS_EXTRA_COUNT (XWA_EXTRA_BACKDROP_LAST_ID - XWA_EXTRA_BACKDROP_FIRST_ID + 1)
/* The stock planet that the extra planets copy. */
#define MISSION_BACKDROPS_TEMPLATE_ID 1
/* 0xFFFF means no image group. */
#define MISSION_BACKDROPS_LAST_GROUP 0xFFFE

typedef char mission_backdrops_extra_types[(OBJ_FirstExtraPlanet + MISSION_BACKDROPS_EXTRA_COUNT == OBJ_Count)
											   ? 1
											   : -1];

static ObjectTypeBackdropDescriptor s_extraDescriptors[MISSION_BACKDROPS_EXTRA_COUNT];

static int MissionBackdrops_ReadInt(const AeronConfigNode* node, int64_t* value) {
	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT) {
		return 0;
	}
	*value = AeronConfigNode_Int(node, 0);
	return 1;
}

/* Reads one entry of extra_planets. Returns 0, logged, when it is invalid. */
static int MissionBackdrops_ReadEntry(const AeronConfigNode* entry, int* firstId, int* lastId,
									  int* firstGroup) {
	const AeronConfigNode* ids = AeronConfigNode_MapGet(entry, "ids");
	int64_t first;
	int64_t last;
	int64_t group;
	const int line = AeronConfigNode_Line(entry);

	if (AeronConfigNode_Type(entry) != AERON_CONFIG_MAP ||
		AeronConfigNode_Type(ids) != AERON_CONFIG_SEQUENCE || AeronConfigNode_SequenceCount(ids) != 2 ||
		!MissionBackdrops_ReadInt(AeronConfigNode_SequenceGet(ids, 0), &first) ||
		!MissionBackdrops_ReadInt(AeronConfigNode_SequenceGet(ids, 1), &last) ||
		!MissionBackdrops_ReadInt(AeronConfigNode_MapGet(entry, "first_group"), &group)) {
		Aeron_LogError("xwa.mission",
					   "%s:%d: an extra planet needs ids: [first, last] and first_group; it is ignored",
					   MISSION_BACKDROPS_PATH, line);
		return 0;
	}
	if (first < XWA_EXTRA_BACKDROP_FIRST_ID || last > XWA_EXTRA_BACKDROP_LAST_ID || first > last) {
		Aeron_LogError("xwa.mission",
					   "%s:%d: ids [%lld, %lld] must be from %d to %d, first to last; they are ignored",
					   MISSION_BACKDROPS_PATH, line, (long long)first, (long long)last,
					   XWA_EXTRA_BACKDROP_FIRST_ID, XWA_EXTRA_BACKDROP_LAST_ID);
		return 0;
	}
	if (group < 0 || group + (last - first) > MISSION_BACKDROPS_LAST_GROUP) {
		Aeron_LogError(
			"xwa.mission", "%s:%d: image groups must be from 0 to %d; ids [%lld, %lld] are ignored",
			MISSION_BACKDROPS_PATH, line, MISSION_BACKDROPS_LAST_GROUP, (long long)first, (long long)last);
		return 0;
	}
	*firstId = (int)first;
	*lastId = (int)last;
	*firstGroup = (int)group;
	return 1;
}

/* Makes backdrop id `id` a planet of image group `group`. */
static void MissionBackdrops_AddPlanet(int id, int group) {
	const ObjectTypeBackdropDescriptor* stock =
		&g_objectTypeTables.missionBackdropDescriptors[MISSION_BACKDROPS_TEMPLATE_ID];
	const int objectType = OBJ_FirstExtraPlanet + id - XWA_EXTRA_BACKDROP_FIRST_ID;
	ObjectTypeInfo* row = &g_objectTypes[objectType];

	*row = g_objectTypes[stock->objectType];
	row->curTexLevel = NULL;
	row->texLevels = NULL;
	row->textureGroup = (uint16_t)group;
	row->frameCount = 0;
	s_extraDescriptors[id - XWA_EXTRA_BACKDROP_FIRST_ID].objectType = (uint16_t)objectType;
	s_extraDescriptors[id - XWA_EXTRA_BACKDROP_FIRST_ID].flags = stock->flags;
}

void XwaMissionBackdrops_Init(void) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* version;
	const AeronConfigNode* planets;
	size_t entry;
	int added = 0;
	int i;

	memset(s_extraDescriptors, 0, sizeof s_extraDescriptors);
	for (i = OBJ_FirstExtraPlanet; i < OBJ_Count; ++i) {
		g_objectTypes[i] = g_objectTypes[OBJ_None];
	}

	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, MISSION_BACKDROPS_PATH, &file,
									&error)) {
		if (error.code != AERON_CONFIG_ERROR_NOT_FOUND) {
			Aeron_LogError("xwa.mission", "%s:%d: %s; backdrop ids above %d place nothing",
						   MISSION_BACKDROPS_PATH, error.line,
						   error.message[0] ? error.message : "cannot be read",
						   XWA_EXTRA_BACKDROP_FIRST_ID - 1);
		}
		return;
	}
	root = AeronConfigFile_Root(file);
	version = AeronConfigNode_MapGet(root, "version");
	if (AeronConfigNode_Type(version) != AERON_CONFIG_INT || AeronConfigNode_Int(version, 0) != 1) {
		Aeron_LogError("xwa.mission", "%s: expected 'version: 1'; backdrop ids above %d place nothing",
					   MISSION_BACKDROPS_PATH, XWA_EXTRA_BACKDROP_FIRST_ID - 1);
		AeronConfigFile_Destroy(file);
		return;
	}
	planets = AeronConfigNode_MapGet(AeronConfigNode_MapGet(root, "backdrops"), "extra_planets");
	if (planets != NULL && AeronConfigNode_Type(planets) != AERON_CONFIG_SEQUENCE) {
		Aeron_LogError(
			"xwa.mission", "%s:%d: extra_planets must be a list; backdrop ids above %d place nothing",
			MISSION_BACKDROPS_PATH, AeronConfigNode_Line(planets), XWA_EXTRA_BACKDROP_FIRST_ID - 1);
		planets = NULL;
	}
	for (entry = 0; entry < AeronConfigNode_SequenceCount(planets); ++entry) {
		const AeronConfigNode* node = AeronConfigNode_SequenceGet(planets, entry);
		int firstId;
		int lastId;
		int firstGroup;
		int id;
		int taken = 0;

		if (!MissionBackdrops_ReadEntry(node, &firstId, &lastId, &firstGroup)) {
			continue;
		}
		for (id = firstId; id <= lastId; ++id) {
			taken = taken || s_extraDescriptors[id - XWA_EXTRA_BACKDROP_FIRST_ID].objectType != 0;
		}
		if (taken) {
			Aeron_LogError("xwa.mission",
						   "%s:%d: an earlier entry already gives one of ids [%d, %d]; they are ignored",
						   MISSION_BACKDROPS_PATH, AeronConfigNode_Line(node), firstId, lastId);
			continue;
		}
		for (id = firstId; id <= lastId; ++id) {
			MissionBackdrops_AddPlanet(id, firstGroup + id - firstId);
		}
		added += lastId - firstId + 1;
	}
	AeronConfigFile_Destroy(file);
	if (added != 0) {
		Aeron_LogInfo("xwa.mission", "%s: %d extra planet backdrop ids", MISSION_BACKDROPS_PATH, added);
	}
}

const ObjectTypeBackdropDescriptor* XwaMissionBackdrops_Descriptor(int id) {
	if (id >= 0 && id < XWA_MISSION_BACKDROP_DESCRIPTOR_COUNT) {
		return &g_objectTypeTables.missionBackdropDescriptors[id];
	}
	if (id >= XWA_EXTRA_BACKDROP_FIRST_ID && id <= XWA_EXTRA_BACKDROP_LAST_ID &&
		s_extraDescriptors[id - XWA_EXTRA_BACKDROP_FIRST_ID].objectType != 0) {
		return &s_extraDescriptors[id - XWA_EXTRA_BACKDROP_FIRST_ID];
	}
	return NULL;
}
