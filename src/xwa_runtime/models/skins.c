#include "xwa_runtime/models/skins.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa_runtime/models/mission_file.h"
#include "xwa_runtime/models/model_source.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* g_missionFlightGroups and FrontendMission.flightGroups hold 192. */
#define SKINS_FLIGHT_GROUPS 192

typedef struct SkinsFlightGroup {
	uint16_t objectType;
	uint8_t markings;
	uint8_t player;
} SkinsFlightGroup;

/* A use registered during the mission: objects of `objectType` show
   `markings`, or every markings value (`all`). */
typedef struct SkinsUse {
	uint16_t objectType;
	uint8_t markings;
	uint8_t all;
} SkinsUse;

static XwaSkinSelection s_selections[XWA_SNAP_MAX_SKIN_SELECTIONS];
static uint32_t s_selectionCount;
static SkinsFlightGroup s_flightGroups[SKINS_FLIGHT_GROUPS];
static int s_flightGroupCount;
static SkinsUse s_uses[XWA_SNAP_MAX_SKIN_USES];
static int s_useCount;
static int s_usesFullLogged;
static uint64_t s_generation;
static uint64_t s_hash;

void XwaSkins_LoadMission(const char* missionPath) {
	char path[300];
	AeronConfigFile* config = NULL;
	const AeronConfigNode* models;
	size_t model;

	s_selectionCount = 0;
	s_flightGroupCount = 0;
	s_useCount = 0;
	s_usesFullLogged = 0;
	if (missionPath == NULL || XwaMissionFile_Load(missionPath, &config, path, sizeof path) <= 0) {
		return;
	}
	models = AeronConfigNode_MapGet(AeronConfigFile_Root(config), "skins");
	if (models != NULL && AeronConfigNode_Type(models) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.models", "%s: 'skins' is not a map of models", path);
		models = NULL;
	}
	for (model = 0; models != NULL && model < AeronConfigNode_MapCount(models); ++model) {
		const char* name = AeronConfigNode_MapKeyAt(models, model);
		const AeronConfigNode* markings = AeronConfigNode_MapValueAt(models, model);
		size_t entry;

		if (AeronConfigNode_Type(markings) != AERON_CONFIG_MAP) {
			Aeron_LogWarn("xwa.models", "%s: skins of '%s' are not a map of markings values; ignored", path,
						  name);
			continue;
		}
		for (entry = 0; entry < AeronConfigNode_MapCount(markings); ++entry) {
			const char* key = AeronConfigNode_MapKeyAt(markings, entry);
			const AeronConfigNode* node = AeronConfigNode_MapValueAt(markings, entry);
			/* null: the model's own materials. */
			const char* variant =
				AeronConfigNode_Type(node) == AERON_CONFIG_NULL ? "" : AeronConfigNode_String(node, NULL);
			char* end = NULL;
			const long value = key != NULL ? strtol(key, &end, 10) : -1;
			XwaSkinSelection* selection;

			if (key == NULL || end == key || *end != '\0' || value < 0 || value > 255 || variant == NULL ||
				strlen(name) >= sizeof selection->model || strlen(variant) >= sizeof selection->variant) {
				Aeron_LogWarn("xwa.models",
							  "%s: skin '%s' of '%s' is not a markings value and a variant name or null; "
							  "ignored",
							  path, key != NULL ? key : "", name);
				continue;
			}
			if (s_selectionCount == XWA_SNAP_MAX_SKIN_SELECTIONS) {
				Aeron_LogWarn("xwa.models", "%s: more than %d skin selections; the rest are ignored", path,
							  XWA_SNAP_MAX_SKIN_SELECTIONS);
				break;
			}
			/* Zeroed, as the snapshot's change hash reads every byte. */
			selection = &s_selections[s_selectionCount++];
			memset(selection, 0, sizeof *selection);
			snprintf(selection->model, sizeof selection->model, "%s", name);
			snprintf(selection->variant, sizeof selection->variant, "%s", variant);
			selection->markings = (uint8_t)value;
		}
	}
	AeronConfigFile_Destroy(config);
	Aeron_LogInfo("xwa.models", "%s: %u skin selections", path, s_selectionCount);
}

void XwaSkins_SetFlightGroups(const struct XwaFlightGroup* flightGroups, size_t stride, int count) {
	int index;

	s_flightGroupCount = 0;
	for (index = 0; index < count && index < SKINS_FLIGHT_GROUPS; ++index) {
		const struct XwaFlightGroup* fg =
			(const struct XwaFlightGroup*)((const unsigned char*)flightGroups + (size_t)index * stride);
		SkinsFlightGroup* group = &s_flightGroups[s_flightGroupCount++];

		group->objectType = g_objectTypeTables.craftTypeToObjectType[fg->craftType];
		group->markings = fg->markings;
		group->player = fg->playerNumber != 0;
	}
}

/* Whether objects of `objectType` showing `markings` are registered: by a
   flight group or a use. */
static int Skins_Registered(uint16_t objectType, int markings) {
	int index;

	for (index = 0; index < s_flightGroupCount; ++index) {
		if (s_flightGroups[index].objectType == objectType && s_flightGroups[index].markings == markings) {
			return 1;
		}
	}
	for (index = 0; index < s_useCount; ++index) {
		if (s_uses[index].objectType == objectType && (s_uses[index].all || s_uses[index].markings == markings)) {
			return 1;
		}
	}
	return 0;
}

static void Skins_Register(uint16_t objectType, int markings, int all) {
	int index;

	for (index = 0; index < s_useCount; ++index) {
		if (s_uses[index].objectType == objectType && s_uses[index].all == all &&
			s_uses[index].markings == markings) {
			return;
		}
	}
	if (s_useCount == XWA_SNAP_MAX_SKIN_USES) {
		if (!s_usesFullLogged) {
			s_usesFullLogged = 1;
			Aeron_LogWarn("xwa.models", "skins: more than %d markings uses; the rest are ignored",
						  XWA_SNAP_MAX_SKIN_USES);
		}
		return;
	}
	s_uses[s_useCount].objectType = objectType;
	s_uses[s_useCount].markings = (uint8_t)markings;
	s_uses[s_useCount++].all = (uint8_t)(all != 0);
}

void XwaSkins_UseMarkings(uint16_t objectType, int markings) {
	if (markings >= 0 && markings <= 255) {
		Skins_Register(objectType, markings, 0);
	}
}

void XwaSkins_UseAllMarkings(uint16_t objectType) { Skins_Register(objectType, 0, 1); }

/* Whether the model loaded as `name` is a GLB. */
static int Skins_IsGlb(const XwaSnapshot* snapshot, const char* name) {
	char key[XWA_MODEL_SOURCE_KEY_MAX];
	char asset[XWA_MODEL_SOURCE_KEY_MAX];
	uint32_t index;

	if (name == NULL || !XwaModelSource_NormalizeKey(name, key)) {
		return 0;
	}
	for (index = 0; index < snapshot->opt_asset_count; ++index) {
		if (snapshot->opt_assets[index].source == XWA_MODEL_SOURCE_GLB &&
			XwaModelSource_NormalizeKey(snapshot->opt_assets[index].name, asset) && strcmp(asset, key) == 0) {
			return 1;
		}
	}
	return 0;
}

/* The safety net: registers the markings of every object shown, so that
   one nothing registered loads its look (its mesh is created again). */
static void Skins_RegisterShown(const XwaSnapshot* snapshot) {
	uint32_t index;

	if (snapshot->scene_kind != XWA_SCENE_FLIGHT) {
		return;
	}
	for (index = 0; index < snapshot->flight_object_count; ++index) {
		const XwaFlightObject* object = &snapshot->flight_objects[index];
		/* A debris chunk draws its source's model with its markings. */
		const uint16_t objectType =
			object->object_type == XWA_SNAP_TYPE_DEBRIS_CHUNK ? object->source_object_type : object->object_type;

		if (!object->has_mobj || Skins_Registered(objectType, object->node_switch)) {
			continue;
		}
		Skins_Register(objectType, object->node_switch, 0);
		if (Skins_IsGlb(snapshot, XwaSnapshotExport_ModelName(objectType))) {
			Aeron_LogInfo("xwa.models", "skins: '%s' is shown with markings %u, which was not registered",
						  XwaSnapshotExport_ModelName(objectType), (unsigned)object->node_switch);
		}
	}
}

/* Adds the use of `markings` (or every value) by the model loaded as `name`,
   once. */
static void Skins_AddUse(XwaSnapshot* snapshot, const char* name, int markings, int all) {
	char key[XWA_MODEL_SOURCE_KEY_MAX];
	uint32_t index;

	if (name == NULL || markings < 0 || markings > 255 || !XwaModelSource_NormalizeKey(name, key)) {
		return;
	}
	for (index = 0; index < snapshot->skin_use_count; ++index) {
		if (snapshot->skin_uses[index].markings == markings && snapshot->skin_uses[index].all_markings == all &&
			strcmp(snapshot->skin_uses[index].key, key) == 0) {
			return;
		}
	}
	if (snapshot->skin_use_count == XWA_SNAP_MAX_SKIN_USES) {
		return;
	}
	/* Zeroed, as the change hash reads every byte. */
	memset(&snapshot->skin_uses[snapshot->skin_use_count], 0, sizeof snapshot->skin_uses[0]);
	snprintf(snapshot->skin_uses[snapshot->skin_use_count].key, sizeof snapshot->skin_uses[0].key, "%s", key);
	snapshot->skin_uses[snapshot->skin_use_count].all_markings = (uint8_t)(all != 0);
	snapshot->skin_uses[snapshot->skin_use_count++].markings = (uint8_t)markings;
}

static uint64_t Skins_Hash(uint64_t hash, const void* data, size_t size) {
	const unsigned char* bytes = (const unsigned char*)data;
	size_t index;

	for (index = 0; index < size; ++index) {
		hash = (hash ^ bytes[index]) * 1099511628211ull;
	}
	return hash;
}

void XwaSkins_Capture(XwaSnapshot* snapshot) {
	uint64_t hash;
	int index;
	uint32_t preview;

	memcpy(snapshot->skin_selections, s_selections, s_selectionCount * sizeof s_selections[0]);
	snapshot->skin_selection_count = s_selectionCount;
	snapshot->skin_use_count = 0;
	for (index = 0; index < s_flightGroupCount; ++index) {
		const SkinsFlightGroup* group = &s_flightGroups[index];

		Skins_AddUse(snapshot, XwaSnapshotExport_ModelName(group->objectType), group->markings, 0);
		/* The player cockpit and exterior show their craft's markings. */
		if (group->player) {
			Skins_AddUse(snapshot, snapshot->cockpit.model_name, group->markings, 0);
			Skins_AddUse(snapshot, snapshot->cockpit.exterior_name, group->markings, 0);
		}
	}
	Skins_RegisterShown(snapshot);
	for (index = 0; index < s_useCount; ++index) {
		Skins_AddUse(snapshot, XwaSnapshotExport_ModelName(s_uses[index].objectType), s_uses[index].markings,
					 s_uses[index].all);
	}
	for (preview = 0; preview < snapshot->model_preview_count; ++preview) {
		Skins_AddUse(snapshot, snapshot->model_previews[preview].opt_name,
					 snapshot->model_previews[preview].node_switch_index, 0);
	}

	hash = Skins_Hash(14695981039346656037ull, snapshot->skin_selections,
					  snapshot->skin_selection_count * sizeof snapshot->skin_selections[0]);
	hash = Skins_Hash(hash, snapshot->skin_uses, snapshot->skin_use_count * sizeof snapshot->skin_uses[0]);
	if (hash != s_hash) {
		s_hash = hash;
		++s_generation;
	}
	snapshot->skin_generation = s_generation;
}
