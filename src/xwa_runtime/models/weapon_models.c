#include "xwa_runtime/models/weapon_models.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/file_io.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/opt_model.h"
#include "xwa_runtime/models/model_source.h"
#include "xwa_runtime/models/names.h"

#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

enum {
	WEAPON_MODELS_FIRST_WEAPON = 280,
	WEAPON_MODELS_WEAPONS = 28,
	/* The different models that the settings of one mission can name. */
	WEAPON_MODELS_MAX = 64,
};

/* The models named in the settings. Their handles are set when
   XwaWeaponModels_LoadMission loads them. s_keySources holds where each
   model is first named, such as "flight/models/x.yaml:12:
   weapons.models.torpedo", for the error message. */
static char s_keys[WEAPON_MODELS_MAX][XWA_MODEL_SOURCE_KEY_MAX];
static char s_keySources[WEAPON_MODELS_MAX][192];
static uint16_t s_keyHandles[WEAPON_MODELS_MAX];
static int s_keyCount;
/* For each object type and weapon, the index of the model in s_keys plus 1,
   or 0 for none. */
static uint8_t s_models[OBJ_Count][WEAPON_MODELS_WEAPONS];
/* The handles to free. They are kept separately from s_keyHandles, because
   the settings can be cleared while the models are still loaded. */
static uint16_t s_loadedHandles[WEAPON_MODELS_MAX];
static int s_loadedCount;

/* Returns the index of `key` in s_keys, adding it with `source` if needed,
   or -1 when the list is full. */
static int WeaponModels_KeyIndex(const char* key, const char* source) {
	int index;

	for (index = 0; index < s_keyCount; ++index) {
		if (strcmp(s_keys[index], key) == 0) {
			return index;
		}
	}
	if (s_keyCount == WEAPON_MODELS_MAX) {
		return -1;
	}
	snprintf(s_keys[s_keyCount], sizeof s_keys[s_keyCount], "%s", key);
	snprintf(s_keySources[s_keyCount], sizeof s_keySources[s_keyCount], "%s", source);
	s_keyHandles[s_keyCount] = 0;
	return s_keyCount++;
}

void XwaWeaponModels_ClearModelSettings(void) {
	memset(s_models, 0, sizeof s_models);
	memset(s_keyHandles, 0, sizeof s_keyHandles);
	s_keyCount = 0;
}

void XwaWeaponModels_ReadModelSettings(unsigned int objectType, const char* path,
									   const struct AeronConfigNode* group) {
	const AeronConfigNode* models;
	size_t index;

	/* The weapon colours code already logs a `weapons` group that isn't a
	   map. */
	if (objectType >= OBJ_Count || AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		return;
	}
	models = AeronConfigNode_MapGet(group, "models");
	if (models == NULL) {
		return;
	}
	if (AeronConfigNode_Type(models) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s:%d: weapons.models must be a map. It is ignored", path,
					  AeronConfigNode_Line(models));
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(models); ++index) {
		const char* weaponName = AeronConfigNode_MapKeyAt(models, index);
		const AeronConfigNode* node = AeronConfigNode_MapValueAt(models, index);
		const char* model = AeronConfigNode_String(node, NULL);
		const int weapon = XwaNames_Find(&g_xwaWeaponTypeNames, weaponName) - WEAPON_MODELS_FIRST_WEAPON;
		char key[XWA_MODEL_SOURCE_KEY_MAX];
		char source[192];
		int keyIndex;

		if (weapon < 0 || weapon >= WEAPON_MODELS_WEAPONS) {
			Aeron_LogWarn("xwa.models", "%s:%d: weapons.models: %s isn't a weapon type. It is ignored", path,
						  AeronConfigNode_Line(node), weaponName);
			continue;
		}
		if (AeronConfigNode_Type(node) != AERON_CONFIG_STRING || model == NULL ||
			!XwaModelSource_NormalizeKey(model, key) || strcmp(key, model) != 0) {
			Aeron_LogWarn("xwa.models",
						  "%s:%d: weapons.models.%s must be a model key in lower case. It is ignored", path,
						  AeronConfigNode_Line(node), weaponName);
			continue;
		}
		snprintf(source, sizeof source, "%s:%d: weapons.models.%s", path, AeronConfigNode_Line(node),
				 weaponName);
		keyIndex = WeaponModels_KeyIndex(key, source);
		if (keyIndex < 0) {
			Aeron_LogWarn("xwa.models",
						  "%s:%d: weapons.models.%s: the mission's craft already use %d weapon models. "
						  "This one is ignored",
						  path, AeronConfigNode_Line(node), weaponName, WEAPON_MODELS_MAX);
			continue;
		}
		s_models[objectType][weapon] = (uint8_t)(keyIndex + 1);
	}
}

int XwaWeaponModels_LoadMission(void) {
	int index;

	XwaWeaponModels_FreeMission();
	for (index = 0; index < s_keyCount; ++index) {
		char fileName[XWA_MODEL_SOURCE_KEY_MAX + 32];
		uint16_t handle;

		/* A model that isn't in any list must be added by its key before it
		   can load. Without a GLB, the game's own OPT with that name loads, if
		   there is one. */
		XwaModelSource_AddKey(s_keys[index]);
		snprintf(fileName, sizeof fileName, "FlightModels\\%s.opt", s_keys[index]);
		handle = OptModel_LoadHandle(fileName);
		if (handle == 0) {
			char message[512];

			/* The shots would look like the weapon's own model, and the mod
			   wouldn't play as its author made it. */
			snprintf(message, sizeof message,
					 "%s: the model %s can't be loaded. Check that the mod has flight/models/%s.glb.",
					 s_keySources[index], s_keys[index], s_keys[index]);
			File_PrintFatalMessageAndExit(message, 1);
			return 0;
		}
		s_keyHandles[index] = handle;
		s_loadedHandles[s_loadedCount++] = handle;
	}
	if (s_loadedCount != 0) {
		Aeron_LogInfo("xwa.models", "weapons.models: loaded %d weapon models", s_loadedCount);
	}
	return 1;
}

void XwaWeaponModels_FreeMission(void) {
	int index;

	for (index = 0; index < s_loadedCount; ++index) {
		OptModel_FreeHandle(s_loadedHandles[index]);
	}
	s_loadedCount = 0;
	memset(s_keyHandles, 0, sizeof s_keyHandles);
}

uint16_t XwaWeaponModels_Handle(unsigned int sourceObjectType, unsigned int weaponType) {
	const unsigned int weapon = weaponType - WEAPON_MODELS_FIRST_WEAPON;
	unsigned int keyIndex;

	if (sourceObjectType >= OBJ_Count || weapon >= WEAPON_MODELS_WEAPONS) {
		return 0;
	}
	keyIndex = s_models[sourceObjectType][weapon];
	return keyIndex != 0 ? s_keyHandles[keyIndex - 1] : 0;
}

int XwaWeaponModels_IsModel(uint16_t handle) {
	int index;

	if (handle == 0) {
		return 0;
	}
	for (index = 0; index < s_loadedCount; ++index) {
		if (s_loadedHandles[index] == handle) {
			return 1;
		}
	}
	return 0;
}
