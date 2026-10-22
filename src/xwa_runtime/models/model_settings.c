#include "xwa_runtime/models/model_settings.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa_runtime/models/craft_hud.h"
#include "xwa_runtime/models/craft_sounds.h"
#include "xwa_runtime/models/craft_weapons.h"
#include "xwa_runtime/models/engine_glows.h"
#include "xwa_runtime/models/model_source.h"
#include "xwa_runtime/models/model_swaps.h"
#include "xwa_runtime/models/shield_recharge.h"
#include "xwa_runtime/models/stats_profiles.h"
#include "xwa_runtime/models/weapon_rates.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* One module's group of keys in the model settings files. */
typedef struct ModelSettingsGroup {
	const char* key;
	/* Drops the previous mission's values. */
	void (*clear)(void);
	/* Reads the group of `objectType`'s model. `group` is NULL when the file
	   doesn't have the group. */
	void (*read)(unsigned int objectType, const char* path, const AeronConfigNode* group);
} ModelSettingsGroup;

static const ModelSettingsGroup s_groups[] = {
	{ "hud", XwaCraftHud_ClearModelSettings, XwaCraftHud_ReadModelSettings },
	{ "engine_glows", XwaEngineGlows_ClearModelSettings, XwaEngineGlows_ReadModelSettings },
	{ "weapons", XwaCraftWeapons_ClearModelSettings, XwaCraftWeapons_ReadModelSettings },
	{ "weapons", XwaWeaponRates_ClearModelSettings, XwaWeaponRates_ReadModelSettings },
	{ "stats_profiles", XwaStatsProfiles_ClearModelSettings, XwaStatsProfiles_ReadModelSettings },
	{ "shield", XwaShieldRecharge_ClearModelSettings, XwaShieldRecharge_ReadModelSettings },
	{ "sound", XwaCraftSounds_ClearModelSettings, XwaCraftSounds_ReadModelSettings },
};

/* A model file that this mission has looked for. `file` is NULL when the
   model has no file, or when the file has errors. */
typedef struct ModelSettingsFile {
	char key[XWA_MODEL_SOURCE_KEY_MAX];
	AeronConfigFile* file;
} ModelSettingsFile;

static uint8_t s_read[OBJ_Count];
static ModelSettingsFile* s_files;
static size_t s_fileCount;
static size_t s_fileCapacity;

static void ModelSettings_Path(const char* key, char* path, size_t pathSize) {
	snprintf(path, pathSize, "flight/models/%s.yaml", key);
}

/* Loads a model settings file and checks that it has "version: 1". Returns
   NULL when the file is missing. Other errors are logged. */
static AeronConfigFile* ModelSettings_LoadFile(const char* path) {
	AeronConfigError error;
	AeronConfigFile* file = NULL;
	const AeronConfigNode* root;

	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, path, &file, &error)) {
		if (error.code != AERON_CONFIG_ERROR_NOT_FOUND) {
			Aeron_LogError("xwa.models", "%s:%d: %s. The file is ignored", path, error.line, error.message);
		}
		return NULL;
	}
	root = AeronConfigFile_Root(file);
	if (AeronConfigNode_Type(root) != AERON_CONFIG_MAP ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) != 1) {
		Aeron_LogError("xwa.models", "%s: the file needs 'version: 1'. It is ignored", path);
		AeronConfigFile_Destroy(file);
		return NULL;
	}
	return file;
}

/* Returns the parsed file of model `key`, loading it the first time this
   mission asks for it. Returns NULL when the model has no usable file. */
static const AeronConfigFile* ModelSettings_File(const char* key) {
	char path[XWA_MODEL_SOURCE_KEY_MAX + 32];
	ModelSettingsFile* entry;
	size_t index;

	for (index = 0; index < s_fileCount; ++index) {
		if (strcmp(s_files[index].key, key) == 0) {
			return s_files[index].file;
		}
	}
	if (s_fileCount == s_fileCapacity) {
		const size_t capacity = s_fileCapacity != 0 ? s_fileCapacity * 2 : 32;
		ModelSettingsFile* files = (ModelSettingsFile*)realloc(s_files, capacity * sizeof *files);

		if (files == NULL) {
			return NULL;
		}
		s_files = files;
		s_fileCapacity = capacity;
	}
	ModelSettings_Path(key, path, sizeof path);
	entry = &s_files[s_fileCount++];
	strcpy(entry->key, key);
	entry->file = ModelSettings_LoadFile(path);
	return entry->file;
}

/* Reads `objectType`'s settings. Returns 1 when its model has a file. */
static int ModelSettings_Read(unsigned int objectType) {
	char key[XWA_MODEL_SOURCE_KEY_MAX];
	char path[XWA_MODEL_SOURCE_KEY_MAX + 32];
	const AeronConfigFile* file;
	const AeronConfigNode* root;
	size_t index;

	s_read[objectType] = 1;
	/* The model that the type loads in this mission: a swap's target has its
	   own settings. */
	if (!XwaModelSwaps_TypeKey(objectType, key)) {
		return 0;
	}
	file = ModelSettings_File(key);
	if (file == NULL) {
		return 0;
	}
	ModelSettings_Path(key, path, sizeof path);
	root = AeronConfigFile_Root(file);
	for (index = 0; index < sizeof s_groups / sizeof s_groups[0]; ++index) {
		s_groups[index].read(objectType, path, AeronConfigNode_MapGet(root, s_groups[index].key));
	}
	return 1;
}

static void ModelSettings_Clear(void) {
	size_t index;

	for (index = 0; index < s_fileCount; ++index) {
		AeronConfigFile_Destroy(s_files[index].file);
	}
	s_fileCount = 0;
	memset(s_read, 0, sizeof s_read);
	for (index = 0; index < sizeof s_groups / sizeof s_groups[0]; ++index) {
		s_groups[index].clear();
	}
}

void XwaModelSettings_LoadMission(const struct XwaFlightGroup* flightGroups, size_t stride, int count) {
	int index;
	int typeCount = 0;

	ModelSettings_Clear();
	for (index = 0; index < count; ++index) {
		const struct XwaFlightGroup* fg =
			(const struct XwaFlightGroup*)((const unsigned char*)flightGroups + (size_t)index * stride);
		const unsigned int objectType = fg->craftType < XWA_CRAFT_TYPE_TO_OBJECT_TYPE_COUNT
											? g_objectTypeTables.craftTypeToObjectType[fg->craftType]
											: OBJ_None;

		if (objectType >= OBJ_Count || s_read[objectType]) {
			continue;
		}
		typeCount += ModelSettings_Read(objectType);
	}
	if (typeCount != 0) {
		Aeron_LogInfo("xwa.models", "flight/models: settings for %d of the mission's object types",
					  typeCount);
	}
}

void XwaModelSettings_Ensure(unsigned int objectType) {
	if (objectType < OBJ_Count && !s_read[objectType]) {
		ModelSettings_Read(objectType);
	}
}
