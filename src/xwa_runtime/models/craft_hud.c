#include "xwa_runtime/models/craft_hud.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa_runtime/models/model_settings.h"

#include <stdlib.h>
#include <string.h>

/* Modern port module; no original counterpart. */

enum {
	CRAFT_HUD_HAS_INNER = 1,
	CRAFT_HUD_HAS_BORDER = 2,
};

typedef struct CraftHudType {
	uint32_t inner;
	uint32_t border;
	uint8_t colorFlags;
	uint16_t reticle[XWA_CRAFT_HUD_RETICLE_SLOTS];
} CraftHudType;

typedef struct CraftHudList {
	uint16_t* values;
	size_t count;
	int present;
} CraftHudList;

static const char s_iconsPath[] = "flight/hud.yaml";
static CraftHudType s_types[OBJ_Count];
static CraftHudList s_hullIcons;
static CraftHudList s_mapIcons;

/* Loads a file from the mod folder and checks that it has "version: 1".
   Returns NULL when the file is missing. Other errors are logged. */
static AeronConfigFile* CraftHud_LoadFile(const char* path) {
	AeronConfigError error;
	AeronConfigFile* file = NULL;
	const AeronConfigNode* root;

	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, path, &file, &error)) {
		if (error.code != AERON_CONFIG_ERROR_NOT_FOUND) {
			Aeron_LogError("xwa.hud", "%s:%d: %s. The file is ignored", path, error.line, error.message);
		}
		return NULL;
	}
	root = AeronConfigFile_Root(file);
	if (AeronConfigNode_Type(root) != AERON_CONFIG_MAP ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) != 1) {
		Aeron_LogError("xwa.hud", "%s: the file needs 'version: 1'. It is ignored", path);
		AeronConfigFile_Destroy(file);
		return NULL;
	}
	return file;
}

static void CraftHud_ReadList(const AeronConfigNode* root, const char* key, CraftHudList* list) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(root, key);
	size_t index;

	if (node == NULL) {
		return;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE) {
		Aeron_LogError("xwa.hud", "%s: '%s' isn't a list. The game uses its own table", s_iconsPath, key);
		return;
	}
	list->count = AeronConfigNode_SequenceCount(node);
	list->values = (uint16_t*)calloc(list->count != 0 ? list->count : 1, sizeof *list->values);
	if (list->values == NULL) {
		list->count = 0;
		return;
	}
	for (index = 0; index < list->count; ++index) {
		const int64_t value = AeronConfigNode_Int(AeronConfigNode_SequenceGet(node, index), -1);

		if (value < 0 || value > UINT16_MAX) {
			Aeron_LogWarn("xwa.hud", "%s: entry %zu of '%s' isn't an image id. It is set to 0", s_iconsPath,
						  index, key);
			continue;
		}
		list->values[index] = (uint16_t)value;
	}
	list->present = 1;
}

static void CraftHud_ReadIcons(void) {
	AeronConfigFile* file = CraftHud_LoadFile(s_iconsPath);
	const AeronConfigNode* root;

	if (file == NULL) {
		return;
	}
	root = AeronConfigFile_Root(file);
	CraftHud_ReadList(root, "hull_icons", &s_hullIcons);
	CraftHud_ReadList(root, "map_icons", &s_mapIcons);
	AeronConfigFile_Destroy(file);
	Aeron_LogInfo("xwa.hud", "%s: %zu hull icons, %zu map icons", s_iconsPath, s_hullIcons.count,
				  s_mapIcons.count);
}

static int CraftHud_ReadColor(const char* path, const AeronConfigNode* colors, const char* key,
							  uint32_t* out) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(colors, key);
	const int64_t value = AeronConfigNode_Int(node, -1);

	if (node == NULL) {
		return 0;
	}
	if (value < 0 || value > UINT32_MAX) {
		Aeron_LogWarn("xwa.hud", "%s: colors.%s isn't an ARGB colour. It is ignored", path, key);
		return 0;
	}
	*out = (uint32_t)value;
	return 1;
}

static void CraftHud_ReadCraft(const char* path, const AeronConfigNode* hud, CraftHudType* type) {
	const AeronConfigNode* reticle;
	const AeronConfigNode* colors;
	size_t index;

	reticle = AeronConfigNode_MapGet(hud, "reticle");
	for (index = 0; index < AeronConfigNode_MapCount(reticle); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(reticle, index);
		const int64_t value = AeronConfigNode_Int(AeronConfigNode_MapValueAt(reticle, index), 0);
		char* end;
		const long slot = strtol(key, &end, 10);

		if (*key == '\0' || *end != '\0' || slot < 0 || slot >= XWA_CRAFT_HUD_RETICLE_SLOTS || value <= 0 ||
			value > UINT16_MAX) {
			Aeron_LogWarn(
				"xwa.hud",
				"%s: reticle.%s needs a slot from 0 to 22 and an image position above 0. It is ignored", path,
				key);
			continue;
		}
		type->reticle[slot] = (uint16_t)value;
	}
	colors = AeronConfigNode_MapGet(hud, "colors");
	if (CraftHud_ReadColor(path, colors, "inner", &type->inner)) {
		type->colorFlags |= CRAFT_HUD_HAS_INNER;
	}
	if (CraftHud_ReadColor(path, colors, "border", &type->border)) {
		type->colorFlags |= CRAFT_HUD_HAS_BORDER;
	}
}

void XwaCraftHud_Init(void) {
	free(s_hullIcons.values);
	free(s_mapIcons.values);
	memset(&s_hullIcons, 0, sizeof s_hullIcons);
	memset(&s_mapIcons, 0, sizeof s_mapIcons);
	CraftHud_ReadIcons();
}

void XwaCraftHud_ClearModelSettings(void) { memset(s_types, 0, sizeof s_types); }

void XwaCraftHud_ReadModelSettings(unsigned int objectType, const char* path, const AeronConfigNode* group) {
	if (objectType < OBJ_Count && group != NULL) {
		CraftHud_ReadCraft(path, group, &s_types[objectType]);
	}
}

static uint16_t CraftHud_Icon(const CraftHudList* list, unsigned int objectType, uint16_t stockId) {
	if (!list->present) {
		return stockId;
	}
	return objectType < list->count ? list->values[objectType] : 0;
}

uint16_t XwaCraftHud_HullIcon(unsigned int objectType, uint16_t stockId) {
	return CraftHud_Icon(&s_hullIcons, objectType, stockId);
}

uint16_t XwaCraftHud_MapIcon(unsigned int objectType, uint16_t stockId) {
	return CraftHud_Icon(&s_mapIcons, objectType, stockId);
}

void XwaCraftHud_Colors(unsigned int objectType, uint32_t colors[2]) {
	const CraftHudType* type;

	if (objectType >= OBJ_Count) {
		return;
	}
	XwaModelSettings_Ensure(objectType);
	type = &s_types[objectType];
	if (type->colorFlags & CRAFT_HUD_HAS_INNER) {
		colors[0] = type->inner;
	}
	if (type->colorFlags & CRAFT_HUD_HAS_BORDER) {
		colors[1] = type->border;
	}
}

int XwaCraftHud_Reticle(unsigned int objectType, int slot) {
	if (objectType >= OBJ_Count || slot < 0 || slot >= XWA_CRAFT_HUD_RETICLE_SLOTS) {
		return 0;
	}
	XwaModelSettings_Ensure(objectType);
	return s_types[objectType].reticle[slot];
}
