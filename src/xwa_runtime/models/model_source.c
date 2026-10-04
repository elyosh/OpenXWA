#include "xwa_runtime/models/model_source.h"

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

static const char s_exteriorSuffix[] = "exterior";

/* Basename without extension, lower-cased. Returns 0 for an empty or
   overlong name. */
static int ModelSource_NormalizeKey(const char* fileName, char* key) {
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

static int ModelSource_FormatGlbPath(const char* key, char* path, size_t pathSize) {
	int written = snprintf(path, pathSize, "remaster\\models\\%s.glb", key);
	return written > 0 && (size_t)written < pathSize;
}

static int ModelSource_GlbExists(const char* key) {
	char path[XWA_MODEL_SOURCE_KEY_MAX + 32];
	XwaFile* stream;

	if (!ModelSource_FormatGlbPath(key, path, sizeof path)) {
		return 0;
	}
	stream = File_Open(AERON_VFS_ROOT_ASSET, path, "rb");
	if (stream == NULL) {
		return 0;
	}
	File_Close(stream);
	return 1;
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
		if (!ModelSource_NormalizeKey(line, (*rows)[*rowCount])) {
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
	ModelSourceKeyText* equipmentRows;
	int equipmentRowCount;
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
		if (written > 0 && (size_t)written < sizeof fileName && ModelSource_NormalizeKey(fileName, key)) {
			ok = ModelSource_AddCockpitKey(key);
		}
	}
	if (!ok) {
		return 0;
	}

	if (!ModelSource_ReadList("FlightModels\\Equipment0.LST", &equipmentRows, &equipmentRowCount)) {
		Aeron_LogWarn("xwa.models", "cannot read FlightModels\\Equipment0.LST; turret seats keep their OPT");
		return 1;
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
				(int)info->frameCount < equipmentRowCount) {
				ok = ModelSource_AddCockpitKey(equipmentRows[info->frameCount]);
			}
		}
	}
	free(equipmentRows);
	return ok;
}

static void ModelSource_Reset(void) {
	free(s_craftRows);
	free(s_craftKeys);
	free(s_cockpitKeys);
	s_craftRows = NULL;
	s_craftRowCount = 0;
	s_craftKeys = NULL;
	s_craftKeyCount = 0;
	s_cockpitKeys = NULL;
	s_cockpitKeyCount = 0;
	s_initialized = 0;
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
	if (!ModelSource_BuildCockpitKeys()) {
		Aeron_LogError("xwa.models", "out of memory building the model replacement set");
		ModelSource_Reset();
		return;
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
			Aeron_LogInfo("xwa.models", "replacement: %s", entry->key);
			++glbCount;
		}
		if (entry->hasExteriorGlb) {
			Aeron_LogInfo("xwa.models", "replacement: %s", exteriorKey);
			++glbCount;
		}
	}
	for (i = 0; i < s_cockpitKeyCount; ++i) {
		ModelSourceCockpitKey* entry = &s_cockpitKeys[i];

		entry->hasGlb = (uint8_t)ModelSource_GlbExists(entry->key);
		if (entry->hasGlb) {
			Aeron_LogInfo("xwa.models", "replacement: %s", entry->key);
			++glbCount;
		}
	}
	s_initialized = 1;
	Aeron_LogInfo("xwa.models", "model replacements: %d GLB (%d craft keys, %d cockpit-class keys)", glbCount,
				  s_craftKeyCount, s_cockpitKeyCount);
}

XwaModelSource XwaModelSource_Resolve(const char* fileName, char* glbPath, size_t glbPathSize) {
	ModelSourceKeyText key;
	const char* glbKey;
	const ModelSourceCraftKey* craft;
	const ModelSourceCockpitKey* cockpit;
	size_t length;

	if (!s_replacementsEnabled || !s_initialized || !ModelSource_NormalizeKey(fileName, key)) {
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
	if (glbKey == NULL) {
		return XWA_MODEL_SOURCE_OPT;
	}
	if (glbPath != NULL && glbPathSize != 0 && !ModelSource_FormatGlbPath(glbKey, glbPath, glbPathSize)) {
		glbPath[0] = '\0';
	}
	return XWA_MODEL_SOURCE_GLB;
}

int XwaModelSource_ModelFileExists(const char* fileName) {
	XwaFile* stream;

	switch (XwaModelSource_Resolve(fileName, NULL, 0)) {
		case XWA_MODEL_SOURCE_GLB:
			return 1;
		case XWA_MODEL_SOURCE_SUPPRESSED:
			return 0;
		case XWA_MODEL_SOURCE_OPT:
		default:
			break;
	}
	stream = File_Open(AERON_VFS_ROOT_ASSET, fileName, "rb");
	if (stream == NULL) {
		return 0;
	}
	File_Close(stream);
	return 1;
}

int XwaModelSource_ReplacementsEnabled(void) { return s_replacementsEnabled && s_initialized; }

int XwaModelSource_IsCockpitClassKey(const char* key) {
	ModelSourceKeyText normalized;

	return ModelSource_NormalizeKey(key, normalized) && ModelSource_FindCockpitKey(normalized) != NULL;
}

int XwaModelSource_KeyRequiresFloatHardpoints(const char* key) {
	ModelSourceKeyText normalized;
	int row;

	if (!ModelSource_NormalizeKey(key, normalized)) {
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
