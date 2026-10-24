#include "xwa_runtime/models/shield_recharge.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/flight_model.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/object_type.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa_runtime/models/model_settings.h"

#include <stdint.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define SHIELD_RECHARGE_FLIGHT_YAML "flight/flight.yaml"

typedef struct ShieldRechargeModel {
	uint8_t set;
	uint8_t perGenerator;
	int32_t rate;
	int32_t playerRate;
	int32_t delaySeconds;
} ShieldRechargeModel;

static int s_active;
static int s_starships;
static int s_starshipsOnEasy;
static int s_doubleStarfighterShields;
static int32_t s_ratePercent;
static ShieldRechargeModel s_models[OBJ_Count];
/* The mission time until which a flight group doesn't recharge, or 0. */
static int s_delayUntil[256];

static void ShieldRecharge_ReadBool(const AeronConfigNode* section, const char* key, int* out) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(section, key);

	if (node == NULL) {
		return;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		Aeron_LogWarn("xwa.models", "%s:%d: shield_recharge.%s must be true or false; it is ignored",
					  SHIELD_RECHARGE_FLIGHT_YAML, AeronConfigNode_Line(node), key);
		return;
	}
	*out = AeronConfigNode_Bool(node, 0) != 0;
}

void XwaShieldRecharge_Init(void) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* section;
	const AeronConfigNode* percent;

	s_active = 0;
	s_starships = 1;
	s_starshipsOnEasy = 0;
	s_doubleStarfighterShields = 0;
	s_ratePercent = 100;
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, SHIELD_RECHARGE_FLIGHT_YAML, &file,
									&error)) {
		/* The backdrops code already logs this error. */
		return;
	}
	root = AeronConfigFile_Root(file);
	section = AeronConfigNode_MapGet(root, "shield_recharge");
	if (AeronConfigNode_Type(AeronConfigNode_MapGet(root, "version")) != AERON_CONFIG_INT ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) != 1 || section == NULL) {
		AeronConfigFile_Destroy(file);
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.models", "%s:%d: shield_recharge must be a map; it is ignored",
					   SHIELD_RECHARGE_FLIGHT_YAML, AeronConfigNode_Line(section));
		AeronConfigFile_Destroy(file);
		return;
	}
	s_active = 1;
	ShieldRecharge_ReadBool(section, "starships", &s_starships);
	ShieldRecharge_ReadBool(section, "starships_on_easy", &s_starshipsOnEasy);
	ShieldRecharge_ReadBool(section, "double_starfighter_shields", &s_doubleStarfighterShields);
	percent = AeronConfigNode_MapGet(section, "rate_percent");
	if (percent != NULL) {
		const int64_t value = AeronConfigNode_Int(percent, -1);

		if (AeronConfigNode_Type(percent) != AERON_CONFIG_INT || value < 0 || value > INT32_MAX) {
			Aeron_LogWarn("xwa.models",
						  "%s:%d: shield_recharge.rate_percent must be 0 or more; it is ignored",
						  SHIELD_RECHARGE_FLIGHT_YAML, AeronConfigNode_Line(percent));
		} else {
			s_ratePercent = (int32_t)value;
		}
	}
	AeronConfigFile_Destroy(file);
}

void XwaShieldRecharge_ClearModelSettings(void) { memset(s_models, 0, sizeof s_models); }

/* Reads `key` into `out`. Returns 0, logged, when it isn't a number of 0
   or more. */
static int ShieldRecharge_ReadInt(const char* path, const AeronConfigNode* group, const char* key,
								  int32_t* out) {
	const AeronConfigNode* node = AeronConfigNode_MapGet(group, key);
	const int64_t value = AeronConfigNode_Int(node, -1);

	if (node == NULL) {
		return 1;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT || value < 0 || value > INT32_MAX) {
		Aeron_LogWarn("xwa.models", "%s: shield.%s must be a number of 0 or more. It is ignored", path, key);
		return 0;
	}
	*out = (int32_t)value;
	return 1;
}

void XwaShieldRecharge_ReadModelSettings(unsigned int objectType, const char* path,
										 const AeronConfigNode* group) {
	ShieldRechargeModel* model;

	if (objectType >= OBJ_Count || group == NULL) {
		return;
	}
	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: shield isn't a map. It is ignored", path);
		return;
	}
	model = &s_models[objectType];
	memset(model, 0, sizeof *model);
	model->set = 1;
	if (AeronConfigNode_MapGet(group, "per_generator_rate") != NULL) {
		model->perGenerator = 1;
		ShieldRecharge_ReadInt(path, group, "per_generator_rate", &model->rate);
	} else {
		ShieldRecharge_ReadInt(path, group, "rate", &model->rate);
	}
	ShieldRecharge_ReadInt(path, group, "player_rate", &model->playerRate);
	ShieldRecharge_ReadInt(path, group, "delay_seconds", &model->delaySeconds);
}

void XwaShieldRecharge_LoadMission(void) { memset(s_delayUntil, 0, sizeof s_delayUntil); }

static const ShieldRechargeModel* ShieldRecharge_Model(unsigned int objectType) {
	XwaModelSettings_Ensure(objectType);
	return objectType < OBJ_Count && s_models[objectType].set ? &s_models[objectType] : NULL;
}

/* The live shield generator meshes of the object's craft. */
static int ShieldRecharge_Generators(unsigned int objIdx, const CraftData* craft) {
	const ObjectTypeId objectType = (ObjectTypeId)g_objectTable[objIdx].objectType;
	const int meshCount = ModelMesh_GetObjectTypeMeshCount(objectType);
	int count = 0;
	int meshIdx;

	for (meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
		if (ModelMesh_GetObjectTypeMeshType(objectType, meshIdx) == MESH_ShieldGenerator &&
			craft->componentHp[meshIdx] != 0) {
			++count;
		}
	}
	return count;
}

/* The model rate of the object's craft, generators included. */
static int ShieldRecharge_ModelRate(unsigned int objIdx, const CraftData* craft) {
	const ObjectRecord* object = &g_objectTable[objIdx];
	const ShieldRechargeModel* model = ShieldRecharge_Model(object->objectType);

	if (model != NULL) {
		return model->perGenerator ? ShieldRecharge_Generators(objIdx, craft) * model->rate : model->rate;
	}
	if (object->genusId == GENUS_Fighter) {
		return 20;
	}
	if (object->genusId != GENUS_Starship) {
		return 0;
	}
	switch (object->objectType) {
		case OBJ_Interdictor2:
		case OBJ_VictoryStarDestroyer2:
		case OBJ_VictoryStarDestroyer2_149:
		case OBJ_ImperialStarDestroyer2:
		case OBJ_ImperialStarDestroyer2_150:
		case OBJ_SuperStarDestroyer:
			return ShieldRecharge_Generators(objIdx, craft) * 5;
		default:
			return 5;
	}
}

void XwaShieldRecharge_Rate(unsigned int objIdx, int aiUpdate, int* rate) {
	const ObjectRecord* object = &g_objectTable[objIdx];
	CraftData* craft = object->mobj != NULL ? object->mobj->pCraft : NULL;
	const ShieldRechargeModel* model;
	const int missionTime = Mission_GetElapsedClockSeconds();
	const int canRecharge = g_flightDifficulty >= 1 || s_starshipsOnEasy;
	int shieldStrength;
	int belowMaximum;

	if (!s_active || craft == NULL) {
		return;
	}
	model = ShieldRecharge_Model(object->objectType);
	shieldStrength = craft->shieldFront + craft->shieldRear;
	belowMaximum =
		shieldStrength < XwaShieldRecharge_MaxShield(objIdx, Craft_GetObjectMaxShield((uint16_t)objIdx));

	/* After the shields fall to 0, the flight group waits. */
	if (s_delayUntil[object->flightGroupIdx] > missionTime) {
		*rate = 0;
		return;
	}
	if (shieldStrength != 0) {
		s_delayUntil[object->flightGroupIdx] = 0;
	} else if (s_delayUntil[object->flightGroupIdx] == 0) {
		s_delayUntil[object->flightGroupIdx] = missionTime + (model != NULL ? model->delaySeconds : 0);
	}

	if (object->playerOwnerIdx != -1) {
		*rate = model != NULL ? model->playerRate : 0;
		if (*rate == 0) {
			*rate = ShieldRecharge_ModelRate(objIdx, craft);
		}
		if (*rate == 0) {
			*rate = 20;
		}
	} else if (object->genusId == GENUS_Fighter) {
		*rate = ShieldRecharge_ModelRate(objIdx, craft);
		if (*rate != 0 && aiUpdate && belowMaximum && (craft->systemFlags & 1) != 0 &&
			craft->shieldRedirect == 2) {
			craft->shieldRedirect = 3;
		}
	} else if (object->genusId == GENUS_Starship) {
		*rate = s_starships && canRecharge ? ShieldRecharge_ModelRate(objIdx, craft) : 0;
		if (*rate != 0 && aiUpdate && belowMaximum) {
			craft->shieldRedirect = 3;
		}
	} else {
		*rate = ShieldRecharge_ModelRate(objIdx, craft);
		if (*rate != 0 && aiUpdate && belowMaximum && canRecharge) {
			craft->shieldRedirect = 3;
		}
	}
	*rate = *rate * s_ratePercent / 100;
}

int XwaShieldRecharge_MaxShield(unsigned int objIdx, int shields) {
	if (s_active && s_doubleStarfighterShields && g_objectTable[objIdx].genusId == GENUS_Fighter) {
		return shields * 2;
	}
	return shields;
}
