#include "xwa_runtime/models/model_source.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/file_io.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_type.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

typedef char ModelSourceKeyText[XWA_MODEL_SOURCE_KEY_MAX];

typedef struct ModelSourceCraftKey {
	ModelSourceKeyText key;
	uint8_t hasGlb;
	uint8_t hasExteriorGlb;
} ModelSourceCraftKey;

typedef struct ModelSourceCockpitKey {
	ModelSourceKeyText key;
	uint8_t hasGlb;
} ModelSourceCockpitKey;

static int s_replacementsEnabled;
static int s_initialized;
/* Spacecraft0.LST keys by row; row r (1-based, as the loaders count it) is
   s_craftRows[r - 1]. Rows whose name cannot form a key are empty. */
static ModelSourceKeyText* s_craftRows;
static int s_craftRowCount;
static ModelSourceCraftKey* s_craftKeys;
static int s_craftKeyCount;
static ModelSourceCockpitKey* s_cockpitKeys;
static int s_cockpitKeyCount;
/* Equipment0.LST keys by row, like s_craftRows, and the replaceable ones
   (every row). */
static ModelSourceKeyText* s_equipmentRows;
static int s_equipmentRowCount;
static ModelSourceCockpitKey* s_equipmentKeys;
static int s_equipmentKeyCount;

static const char s_exteriorSuffix[] = "exterior";

/* models/model_map.yaml in the mod folder: stock key -> GLB key, for model sets whose
   file names differ from the stock ones. Without the file the mapping is the
   identity. */
typedef struct ModelSourceMapEntry {
	ModelSourceKeyText stockKey;
	ModelSourceKeyText glbKey;
} ModelSourceMapEntry;

static const char s_modelMapPath[] = "models/model_map.yaml";
static ModelSourceMapEntry* s_mapEntries;
static int s_mapEntryCount;

/* GLB keys no model list names, added by the settings that load them
   (XwaModelSource_AddKey); they map to themselves. */
static ModelSourceKeyText* s_addedKeys;
static int s_addedKeyCount;

int XwaModelSource_NormalizeKey(const char* fileName, char* key) {
	const char* base;
	const char* end;
	const char* cursor;
	size_t length;
	size_t i;

	if (fileName == NULL) {
		return 0;
	}
	base = fileName;
	for (cursor = fileName; *cursor != '\0'; ++cursor) {
		if (*cursor == '\\' || *cursor == '/') {
			base = cursor + 1;
		}
	}
	end = strrchr(base, '.');
	length = end != NULL ? (size_t)(end - base) : strlen(base);
	if (length == 0 || length >= XWA_MODEL_SOURCE_KEY_MAX) {
		return 0;
	}
	for (i = 0; i < length; ++i) {
		char c = base[i];
		key[i] = (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
	}
	key[length] = '\0';
	return 1;
}

static const char* ModelSource_GlbKeyFor(const char* key) {
	int i;

	for (i = 0; i < s_mapEntryCount; ++i) {
		if (strcmp(s_mapEntries[i].stockKey, key) == 0) {
			return s_mapEntries[i].glbKey;
		}
	}
	return key;
}

int XwaModelSource_GlbPath(const char* key, char* path, size_t pathSize) {
	int written = snprintf(path, pathSize, "models/%s.glb", ModelSource_GlbKeyFor(key));
	return written > 0 && (size_t)written < pathSize;
}

static int ModelSource_FileExists(const char* path) {
	XwaFile* stream = File_Open(AERON_VFS_ROOT_MOD, path, "rb");

	if (stream == NULL) {
		return 0;
	}
	File_Close(stream);
	return 1;
}

static int ModelSource_GlbExists(const char* key) {
	char path[XWA_MODEL_SOURCE_KEY_MAX + 32];

	return XwaModelSource_GlbPath(key, path, sizeof path) && ModelSource_FileExists(path);
}

static int ModelSource_Grow(void** items, int count, size_t itemSize) {
	void* grown;

	/* Capacity is 16, then doubles each time count reaches it. */
	if (count != 0 && (count < 16 || (count & (count - 1)) != 0)) {
		return 1;
	}
	grown = realloc(*items, (size_t)(count == 0 ? 16 : count * 2) * itemSize);
	if (grown == NULL) {
		return 0;
	}
	*items = grown;
	return 1;
}

/* Reads a FlightModels list the way FeDiskIo_LoadResources does: line ends
   trimmed, empty lines skipped, rows numbered from 1. Each row's key goes to
   `rows`. Returns 0 if the list cannot be read. */
static int ModelSource_ReadList(const char* path, ModelSourceKeyText** rows, int* rowCount) {
	char line[256];
	XwaFile* stream;

	*rows = NULL;
	*rowCount = 0;
	stream = File_Open(AERON_VFS_ROOT_ASSET, path, "rb");
	if (stream == NULL) {
		return 0;
	}
	while (File_ReadLine(stream, line, sizeof line)) {
		char* cursor;

		for (cursor = line; *cursor != '\0' && *cursor != '\r' && *cursor != '\n'; ++cursor) {
		}
		*cursor = '\0';
		if (line[0] == '\0') {
			continue;
		}
		if (!ModelSource_Grow((void**)rows, *rowCount, sizeof **rows)) {
			File_Close(stream);
			free(*rows);
			*rows = NULL;
			*rowCount = 0;
			return 0;
		}
		if (!XwaModelSource_NormalizeKey(line, (*rows)[*rowCount])) {
			(*rows)[*rowCount][0] = '\0';
		}
		++*rowCount;
	}
	File_Close(stream);
	return 1;
}

static ModelSourceCraftKey* ModelSource_FindCraftKey(const char* key) {
	int i;

	for (i = 0; i < s_craftKeyCount; ++i) {
		if (strcmp(s_craftKeys[i].key, key) == 0) {
			return &s_craftKeys[i];
		}
	}
	return NULL;
}

static ModelSourceCockpitKey* ModelSource_FindCockpitKey(const char* key) {
	int i;

	for (i = 0; i < s_cockpitKeyCount; ++i) {
		if (strcmp(s_cockpitKeys[i].key, key) == 0) {
			return &s_cockpitKeys[i];
		}
	}
	return NULL;
}

static int ModelSource_AddCraftKey(const char* key) {
	ModelSourceCraftKey* entry;

	if (key[0] == '\0' || ModelSource_FindCraftKey(key) != NULL) {
		return 1;
	}
	if (!ModelSource_Grow((void**)&s_craftKeys, s_craftKeyCount, sizeof *s_craftKeys)) {
		return 0;
	}
	entry = &s_craftKeys[s_craftKeyCount++];
	memset(entry, 0, sizeof *entry);
	strcpy(entry->key, key);
	return 1;
}

static int ModelSource_AddCockpitKey(const char* key) {
	ModelSourceCockpitKey* entry;

	if (key[0] == '\0' || ModelSource_FindCockpitKey(key) != NULL) {
		return 1;
	}
	if (!ModelSource_Grow((void**)&s_cockpitKeys, s_cockpitKeyCount, sizeof *s_cockpitKeys)) {
		return 0;
	}
	entry = &s_cockpitKeys[s_cockpitKeyCount++];
	memset(entry, 0, sizeof *entry);
	strcpy(entry->key, key);
	return 1;
}

/* `<name>cockpit` for every model definition name, the fallbacks
   FeDiskIo_LoadCockpitModel tries, and the Equipment0.LST key of every
   turret-seat model type. */
static int ModelSource_BuildCockpitKeys(void) {
	int modelIndex;
	int ok;

	ok = ModelSource_AddCockpitKey("tiecockpit") && ModelSource_AddCockpitKey("combatsimcockpit");
	for (modelIndex = 0; ok && modelIndex < XWA_MODEL_DEF_COUNT; ++modelIndex) {
		char fileName[XWA_MODEL_SOURCE_KEY_MAX + 16];
		ModelSourceKeyText key;
		int written;

		if (g_modelDefs[modelIndex].name[0] == '\0') {
			continue;
		}
		written = snprintf(fileName, sizeof fileName, "%sCockpit", g_modelDefs[modelIndex].name);
		if (written > 0 && (size_t)written < sizeof fileName && XwaModelSource_NormalizeKey(fileName, key)) {
			ok = ModelSource_AddCockpitKey(key);
		}
	}
	if (!ok) {
		return 0;
	}

	for (modelIndex = 0; ok && modelIndex < XWA_MODEL_DEF_COUNT; ++modelIndex) {
		int seat;

		for (seat = 0; ok && seat < 2; ++seat) {
			uint16_t objectType = g_modelDefs[modelIndex].turretModelIndex[seat];
			const ModelTypeInfo* info;

			if (objectType == 0 || objectType >= OBJ_Count) {
				continue;
			}
			info = &g_modelTypeTable[objectType];
			if ((info->recordFlags & MODEL_TYPE_RECORD_TEXTURE_BACKED) != 0 && info->textureGroup == 1 &&
				(int)info->frameCount < s_equipmentRowCount) {
				ok = ModelSource_AddCockpitKey(s_equipmentRows[info->frameCount]);
			}
		}
	}
	return ok;
}

/* Every Equipment0.LST model. Recovered code reaches the components of some
   of them by number; those sites resolve component roles
   (xwa_runtime/models/model_roles.h), so their layout is free too. */
static int ModelSource_BuildEquipmentKeys(void) {
	int row;

	for (row = 0; row < s_equipmentRowCount; ++row) {
		const char* key = s_equipmentRows[row];
		ModelSourceCockpitKey* entry;
		int known;
		int i;

		if (key[0] == '\0') {
			continue;
		}
		known = 0;
		for (i = 0; i < s_equipmentKeyCount && !known; ++i) {
			known = strcmp(s_equipmentKeys[i].key, key) == 0;
		}
		if (known) {
			continue;
		}
		if (!ModelSource_Grow((void**)&s_equipmentKeys, s_equipmentKeyCount, sizeof *s_equipmentKeys)) {
			return 0;
		}
		entry = &s_equipmentKeys[s_equipmentKeyCount++];
		memset(entry, 0, sizeof *entry);
		strcpy(entry->key, key);
	}
	return 1;
}

static void ModelSource_Reset(void) {
	free(s_craftRows);
	free(s_craftKeys);
	free(s_cockpitKeys);
	free(s_equipmentRows);
	free(s_equipmentKeys);
	free(s_mapEntries);
	s_mapEntries = NULL;
	s_mapEntryCount = 0;
	free(s_addedKeys);
	s_addedKeys = NULL;
	s_addedKeyCount = 0;
	s_equipmentRows = NULL;
	s_equipmentRowCount = 0;
	s_equipmentKeys = NULL;
	s_equipmentKeyCount = 0;
	s_craftRows = NULL;
	s_craftRowCount = 0;
	s_craftKeys = NULL;
	s_craftKeyCount = 0;
	s_cockpitKeys = NULL;
	s_cockpitKeyCount = 0;
	s_initialized = 0;
}

static int ModelSource_IsValidKeyText(const char* text) {
	size_t length;

	if (text == NULL) {
		return 0;
	}
	for (length = 0; text[length] != '\0'; ++length) {
		const char c = text[length];

		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) {
			return 0;
		}
	}
	return length >= 1 && length < XWA_MODEL_SOURCE_KEY_MAX;
}

/* A key the resolver can produce: a craft, its exterior, a cockpit-class or
   an equipment key. */
static int ModelSource_IsKnownKey(const char* key) {
	const size_t suffixLength = sizeof s_exteriorSuffix - 1;
	const size_t length = strlen(key);
	int i;

	if (ModelSource_FindCraftKey(key) != NULL || ModelSource_FindCockpitKey(key) != NULL) {
		return 1;
	}
	for (i = 0; i < s_equipmentKeyCount; ++i) {
		if (strcmp(s_equipmentKeys[i].key, key) == 0) {
			return 1;
		}
	}
	if (length > suffixLength && strcmp(key + length - suffixLength, s_exteriorSuffix) == 0) {
		ModelSourceKeyText base;

		memcpy(base, key, length - suffixLength);
		base[length - suffixLength] = '\0';
		return ModelSource_FindCraftKey(base) != NULL;
	}
	return 0;
}

/* Loads the model map once the key sets are built. Returns 0 after writing the
   reason to `message` when the map is invalid; an absent map is valid. */
static int ModelSource_LoadMap(char* message, size_t messageSize) {
	AeronConfigFile* config = NULL;
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* models;
	size_t count;
	size_t index;

	if (!ModelSource_FileExists(s_modelMapPath)) {
		return 1;
	}
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, s_modelMapPath, &config, &error)) {
		snprintf(message, messageSize, "%s:%d: %s", s_modelMapPath, error.line, error.message);
		return 0;
	}
	root = AeronConfigFile_Root(config);
	models = AeronConfigNode_MapGet(root, "models");
	if (AeronConfigNode_Type(root) != AERON_CONFIG_MAP ||
		AeronConfigNode_Type(AeronConfigNode_MapGet(root, "version")) != AERON_CONFIG_INT ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) != 1 ||
		AeronConfigNode_Type(models) != AERON_CONFIG_MAP) {
		snprintf(message, messageSize, "%s: expected 'version: 1' and a 'models' map", s_modelMapPath);
		AeronConfigFile_Destroy(config);
		return 0;
	}
	count = AeronConfigNode_MapCount(models);
	s_mapEntries = count ? (ModelSourceMapEntry*)calloc(count, sizeof *s_mapEntries) : NULL;
	if (count && s_mapEntries == NULL) {
		snprintf(message, messageSize, "%s: out of memory", s_modelMapPath);
		AeronConfigFile_Destroy(config);
		return 0;
	}
	for (index = 0; index < count; ++index) {
		const char* key = AeronConfigNode_MapKeyAt(models, index);
		const char* value = AeronConfigNode_String(AeronConfigNode_MapValueAt(models, index), NULL);
		char path[XWA_MODEL_SOURCE_KEY_MAX + 32];
		int i;

		if (!ModelSource_IsValidKeyText(key) || !ModelSource_IsValidKeyText(value)) {
			snprintf(message, messageSize,
					 "%s: '%s: %s' is not a pair of [a-z0-9._-] keys of 1 to %d characters", s_modelMapPath,
					 key ? key : "", value ? value : "", XWA_MODEL_SOURCE_KEY_MAX - 1);
			AeronConfigFile_Destroy(config);
			return 0;
		}
		for (i = 0; i < s_mapEntryCount; ++i) {
			if (strcmp(s_mapEntries[i].stockKey, key) == 0) {
				snprintf(message, messageSize, "%s: '%s' is mapped twice", s_modelMapPath, key);
				AeronConfigFile_Destroy(config);
				return 0;
			}
		}
		if (!ModelSource_IsKnownKey(key)) {
			snprintf(message, messageSize,
					 "%s: '%s' is not a craft, exterior, cockpit or equipment model key", s_modelMapPath,
					 key);
			AeronConfigFile_Destroy(config);
			return 0;
		}
		snprintf(path, sizeof path, "models/%s.glb", value);
		if (!ModelSource_FileExists(path)) {
			snprintf(message, messageSize, "%s: '%s' maps to missing '%s'", s_modelMapPath, key, path);
			AeronConfigFile_Destroy(config);
			return 0;
		}
		strcpy(s_mapEntries[s_mapEntryCount].stockKey, key);
		strcpy(s_mapEntries[s_mapEntryCount].glbKey, value);
		++s_mapEntryCount;
	}
	AeronConfigFile_Destroy(config);
	Aeron_LogInfo("xwa.models", "model map: %d entries", s_mapEntryCount);
	return 1;
}

void XwaModelSource_Configure(int replacementsEnabled) { s_replacementsEnabled = replacementsEnabled != 0; }

void XwaModelSource_Init(void) {
	int glbCount;
	int i;

	ModelSource_Reset();
	if (!s_replacementsEnabled) {
		Aeron_LogInfo("xwa.models", "model replacements: disabled");
		return;
	}
	if (!ModelSource_ReadList("FlightModels\\Spacecraft0.LST", &s_craftRows, &s_craftRowCount)) {
		Aeron_LogError("xwa.models",
					   "cannot read FlightModels\\Spacecraft0.LST; model replacements disabled");
		return;
	}
	for (i = 0; i < s_craftRowCount; ++i) {
		if (!ModelSource_AddCraftKey(s_craftRows[i])) {
			Aeron_LogError("xwa.models", "out of memory building the model replacement set");
			ModelSource_Reset();
			return;
		}
	}
	if (!ModelSource_ReadList("FlightModels\\Equipment0.LST", &s_equipmentRows, &s_equipmentRowCount)) {
		Aeron_LogWarn("xwa.models",
					  "cannot read FlightModels\\Equipment0.LST; equipment and turret seats keep their OPT");
	}
	if (!ModelSource_BuildCockpitKeys() || !ModelSource_BuildEquipmentKeys()) {
		Aeron_LogError("xwa.models", "out of memory building the model replacement set");
		ModelSource_Reset();
		return;
	}
	{
		char message[256];

		if (!ModelSource_LoadMap(message, sizeof message)) {
			/* Like an invalid GLB: the installed model set is broken. */
			Aeron_LogError("xwa.models", "invalid model map: %s", message);
			ModelSource_Reset();
			Aeron_RequestFatalError("OpenXWA", message);
			return;
		}
	}

	glbCount = 0;
	for (i = 0; i < s_craftKeyCount; ++i) {
		ModelSourceCraftKey* entry = &s_craftKeys[i];
		char exteriorKey[XWA_MODEL_SOURCE_KEY_MAX + sizeof s_exteriorSuffix];

		entry->hasGlb = (uint8_t)ModelSource_GlbExists(entry->key);
		snprintf(exteriorKey, sizeof exteriorKey, "%s%s", entry->key, s_exteriorSuffix);
		entry->hasExteriorGlb =
			(uint8_t)(strlen(exteriorKey) < XWA_MODEL_SOURCE_KEY_MAX && ModelSource_GlbExists(exteriorKey));
		if (entry->hasGlb) {
			Aeron_LogInfo("xwa.models", "replacement: %s (%s.glb)", entry->key,
						  ModelSource_GlbKeyFor(entry->key));
			++glbCount;
		}
		if (entry->hasExteriorGlb) {
			Aeron_LogInfo("xwa.models", "replacement: %s (%s.glb)", exteriorKey,
						  ModelSource_GlbKeyFor(exteriorKey));
			++glbCount;
		}
	}
	for (i = 0; i < s_cockpitKeyCount; ++i) {
		ModelSourceCockpitKey* entry = &s_cockpitKeys[i];

		entry->hasGlb = (uint8_t)ModelSource_GlbExists(entry->key);
		if (entry->hasGlb) {
			Aeron_LogInfo("xwa.models", "replacement: %s (%s.glb)", entry->key,
						  ModelSource_GlbKeyFor(entry->key));
			++glbCount;
		}
	}
	for (i = 0; i < s_equipmentKeyCount; ++i) {
		ModelSourceCockpitKey* entry = &s_equipmentKeys[i];

		entry->hasGlb =
			(uint8_t)(ModelSource_FindCockpitKey(entry->key) == NULL && ModelSource_GlbExists(entry->key));
		if (entry->hasGlb) {
			Aeron_LogInfo("xwa.models", "replacement: %s (%s.glb)", entry->key,
						  ModelSource_GlbKeyFor(entry->key));
			++glbCount;
		}
	}
	s_initialized = 1;
	Aeron_LogInfo("xwa.models",
				  "model replacements: %d GLB (%d craft keys, %d cockpit-class keys, %d equipment keys)",
				  glbCount, s_craftKeyCount, s_cockpitKeyCount, s_equipmentKeyCount);
}

XwaModelSource XwaModelSource_Resolve(const char* fileName, char* glbPath, size_t glbPathSize) {
	ModelSourceKeyText key;
	const char* glbKey;
	const ModelSourceCraftKey* craft;
	const ModelSourceCockpitKey* cockpit;
	size_t length;
	int i;

	if (!s_replacementsEnabled || !s_initialized || !XwaModelSource_NormalizeKey(fileName, key)) {
		return XWA_MODEL_SOURCE_OPT;
	}

	glbKey = NULL;
	craft = ModelSource_FindCraftKey(key);
	if (craft != NULL && craft->hasGlb) {
		glbKey = key;
	}
	length = strlen(key);
	if (glbKey == NULL && length > sizeof s_exteriorSuffix - 1 &&
		strcmp(key + length - (sizeof s_exteriorSuffix - 1), s_exteriorSuffix) == 0) {
		ModelSourceKeyText base;

		memcpy(base, key, length - (sizeof s_exteriorSuffix - 1));
		base[length - (sizeof s_exteriorSuffix - 1)] = '\0';
		craft = ModelSource_FindCraftKey(base);
		if (craft != NULL && craft->hasExteriorGlb) {
			glbKey = key;
		} else if (craft != NULL && craft->hasGlb) {
			return XWA_MODEL_SOURCE_SUPPRESSED;
		}
	}
	if (glbKey == NULL) {
		cockpit = ModelSource_FindCockpitKey(key);
		if (cockpit != NULL && cockpit->hasGlb) {
			glbKey = key;
		}
	}
	for (i = 0; glbKey == NULL && i < s_equipmentKeyCount; ++i) {
		if (s_equipmentKeys[i].hasGlb && strcmp(s_equipmentKeys[i].key, key) == 0) {
			glbKey = key;
		}
	}
	for (i = 0; glbKey == NULL && i < s_addedKeyCount; ++i) {
		if (strcmp(s_addedKeys[i], key) == 0) {
			glbKey = key;
		}
	}
	if (glbKey == NULL) {
		return XWA_MODEL_SOURCE_OPT;
	}
	if (glbPath != NULL && glbPathSize != 0 && !XwaModelSource_GlbPath(glbKey, glbPath, glbPathSize)) {
		glbPath[0] = '\0';
	}
	return XWA_MODEL_SOURCE_GLB;
}

XwaFile* XwaModelSource_OpenModelFile(const char* fileName) {
	char glbPath[XWA_MODEL_SOURCE_KEY_MAX + 32];

	switch (XwaModelSource_Resolve(fileName, glbPath, sizeof glbPath)) {
		case XWA_MODEL_SOURCE_GLB:
			return File_Open(AERON_VFS_ROOT_MOD, glbPath, "rb");
		case XWA_MODEL_SOURCE_SUPPRESSED:
			return NULL;
		case XWA_MODEL_SOURCE_OPT:
		default:
			return File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
	}
}

int XwaModelSource_ReplacementsEnabled(void) { return s_replacementsEnabled && s_initialized; }

int XwaModelSource_AddKey(const char* key) {
	ModelSourceKeyText normalized;
	int i;

	if (!s_replacementsEnabled || !s_initialized || !ModelSource_IsValidKeyText(key)) {
		return 0;
	}
	strcpy(normalized, key);
	if (ModelSource_IsKnownKey(normalized)) {
		return XwaModelSource_Resolve(normalized, NULL, 0) == XWA_MODEL_SOURCE_GLB;
	}
	for (i = 0; i < s_addedKeyCount; ++i) {
		if (strcmp(s_addedKeys[i], normalized) == 0) {
			return 1;
		}
	}
	if (!ModelSource_GlbExists(normalized) ||
		!ModelSource_Grow((void**)&s_addedKeys, s_addedKeyCount, sizeof *s_addedKeys)) {
		return 0;
	}
	strcpy(s_addedKeys[s_addedKeyCount++], normalized);
	Aeron_LogInfo("xwa.models", "replacement: %s (%s.glb, added)", normalized, normalized);
	return 1;
}

int XwaModelSource_TypeKey(unsigned int objectType, int mapped, char* key) {
	const ModelTypeInfo* info;
	const char* rowKey = NULL;

	if (!s_replacementsEnabled || !s_initialized || objectType >= OBJ_Count) {
		return 0;
	}
	info = &g_modelTypeTable[objectType];
	if ((info->recordFlags & MODEL_TYPE_RECORD_TEXTURE_BACKED) == 0) {
		return 0;
	}
	if (info->textureGroup == 0 && (int)info->frameCount < s_craftRowCount) {
		rowKey = s_craftRows[info->frameCount];
	} else if (info->textureGroup == 1 && (int)info->frameCount < s_equipmentRowCount) {
		rowKey = s_equipmentRows[info->frameCount];
	}
	if (rowKey == NULL || rowKey[0] == '\0') {
		return 0;
	}
	strcpy(key, mapped ? ModelSource_GlbKeyFor(rowKey) : rowKey);
	return 1;
}

int XwaModelSource_IsCockpitClassKey(const char* key) {
	ModelSourceKeyText normalized;

	return XwaModelSource_NormalizeKey(key, normalized) && ModelSource_FindCockpitKey(normalized) != NULL;
}

int XwaModelSource_KeyRequiresFloatHardpoints(const char* key) {
	ModelSourceKeyText normalized;
	int row;

	if (!XwaModelSource_NormalizeKey(key, normalized)) {
		return 0;
	}
	for (row = 0; row < s_craftRowCount; ++row) {
		unsigned int objectType;

		if (strcmp(s_craftRows[row], normalized) != 0) {
			continue;
		}
		for (objectType = 0; objectType < OBJ_Count; ++objectType) {
			const ModelTypeInfo* info = &g_modelTypeTable[objectType];

			if ((info->recordFlags & MODEL_TYPE_RECORD_TEXTURE_BACKED) != 0 && info->textureGroup == 0 &&
				info->frameCount == (uint16_t)row &&
				(info->flags & MODEL_TYPE_FLAG_EXPANDED_TARGET_PROBE) != 0) {
				return 1;
			}
		}
	}
	return 0;
}
