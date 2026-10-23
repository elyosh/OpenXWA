#include "xwa_runtime/models/gunner_seats.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/object_type.h"
#include "xwa_runtime/models/craft_definitions.h"
#include "xwa_runtime/models/model_settings.h"
#include "xwa_runtime/models/model_source.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

static const char* const s_keys[XWA_GUNNER_SEAT_FIELDS] = {
	"x", "y", "z", "mount_angle_a", "mount_angle_b", "aim_limit_a", "aim_limit_b", "cockpit", "inverted",
};

typedef struct GunnerSeatsModel {
	uint8_t set;
	uint8_t count;
	uint8_t forwardFire;
	int32_t value[XWA_GUNNER_SEAT_MAX][XWA_GUNNER_SEAT_FIELDS];
} GunnerSeatsModel;

static GunnerSeatsModel s_models[OBJ_Count];

void XwaGunnerSeats_ClearModelSettings(void) { memset(s_models, 0, sizeof s_models); }

/* Returns the object type of the gunner cockpit model `key`, or 0 when no
   gunner cockpit type loads it. */
static unsigned int GunnerSeats_CockpitType(const char* key) {
	char normalized[XWA_MODEL_SOURCE_KEY_MAX];
	char typeKey[XWA_MODEL_SOURCE_KEY_MAX];
	unsigned int objectType;

	if (!XwaModelSource_NormalizeKey(key, normalized)) {
		return 0;
	}
	for (objectType = 1; objectType < OBJ_Count; ++objectType) {
		if ((objectType == OBJ_CorellianTransportGunner || XwaCraftDefinitions_GunnerCockpit(objectType)) &&
			XwaModelSource_TypeKey(objectType, typeKey) && strcmp(typeKey, normalized) == 0) {
			return objectType;
		}
	}
	return 0;
}

/* Reads seat `node` into `value`. Returns 0, logged, when it can't be
   used. */
static int GunnerSeats_ReadSeat(const char* path, size_t index, const AeronConfigNode* node, int32_t* value) {
	int field;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: gunner_seats.seats[%zu] isn't a map. The seats are ignored", path,
					  index);
		return 0;
	}
	for (field = 0; field < XWA_GUNNER_SEAT_FIELDS; ++field) {
		const AeronConfigNode* item = AeronConfigNode_MapGet(node, s_keys[field]);
		const int64_t number = AeronConfigNode_Int(item, 0);

		value[field] = 0;
		if (field == XWA_GUNNER_SEAT_COCKPIT) {
			const char* key = AeronConfigNode_String(item, NULL);

			value[field] =
				AeronConfigNode_Type(item) == AERON_CONFIG_STRING ? (int32_t)GunnerSeats_CockpitType(key) : 0;
			if (value[field] == 0) {
				Aeron_LogWarn("xwa.models",
							  "%s: gunner_seats.seats[%zu].cockpit isn't the model of a gunner cockpit. The "
							  "seats are ignored",
							  path, index);
				return 0;
			}
		} else if (field == XWA_GUNNER_SEAT_INVERTED) {
			if (item != NULL && AeronConfigNode_Type(item) != AERON_CONFIG_BOOL) {
				Aeron_LogWarn("xwa.models", "%s: gunner_seats.seats[%zu].inverted must be true or false",
							  path, index);
			}
			value[field] = AeronConfigNode_Bool(item, 0) != 0;
		} else if (item != NULL) {
			/* The record's fields are 16 bits. Angles can be written from 0 to
			   65535 too. */
			if (AeronConfigNode_Type(item) != AERON_CONFIG_INT || number < INT16_MIN || number > UINT16_MAX) {
				Aeron_LogWarn(
					"xwa.models",
					"%s: gunner_seats.seats[%zu].%s must be a number from -32768 to 65535. The seats "
					"are ignored",
					path, index, s_keys[field]);
				return 0;
			}
			value[field] = (int16_t)number;
		}
	}
	return 1;
}

void XwaGunnerSeats_ReadModelSettings(unsigned int objectType, const char* path,
									  const AeronConfigNode* group) {
	GunnerSeatsModel model;
	const AeronConfigNode* seats;
	const AeronConfigNode* forwardFire;
	size_t index;

	if (objectType >= OBJ_Count || group == NULL) {
		return;
	}
	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: gunner_seats isn't a map. It is ignored", path);
		return;
	}
	memset(&model, 0, sizeof model);
	model.set = 1;
	model.forwardFire = 1;
	forwardFire = AeronConfigNode_MapGet(group, "forward_fire");
	if (forwardFire != NULL) {
		if (AeronConfigNode_Type(forwardFire) != AERON_CONFIG_BOOL) {
			Aeron_LogWarn("xwa.models", "%s: gunner_seats.forward_fire must be true or false", path);
		} else {
			model.forwardFire = AeronConfigNode_Bool(forwardFire, 1) != 0;
		}
	}
	seats = AeronConfigNode_MapGet(group, "seats");
	if (AeronConfigNode_Type(seats) != AERON_CONFIG_SEQUENCE) {
		Aeron_LogWarn("xwa.models", "%s: gunner_seats.seats must be a list. The group is ignored", path);
		return;
	}
	if (AeronConfigNode_SequenceCount(seats) > XWA_GUNNER_SEAT_MAX) {
		Aeron_LogWarn("xwa.models",
					  "%s: gunner_seats has %zu seats; the game has room for %d. The rest are "
					  "ignored",
					  path, AeronConfigNode_SequenceCount(seats), XWA_GUNNER_SEAT_MAX);
	}
	for (index = 0; index < AeronConfigNode_SequenceCount(seats) && index < XWA_GUNNER_SEAT_MAX; ++index) {
		if (!GunnerSeats_ReadSeat(path, index, AeronConfigNode_SequenceGet(seats, index),
								  model.value[index])) {
			return;
		}
		++model.count;
	}
	s_models[objectType] = model;
}

static const GunnerSeatsModel* GunnerSeats_Model(unsigned int objectType) {
	XwaModelSettings_Ensure(objectType);
	return objectType < OBJ_Count && s_models[objectType].set ? &s_models[objectType] : NULL;
}

int XwaGunnerSeats_Count(unsigned int objectType) {
	const GunnerSeatsModel* model = GunnerSeats_Model(objectType);

	return model != NULL ? model->count : -1;
}

int XwaGunnerSeats_Value(unsigned int objectType, int seat, XwaGunnerSeatField field, int stock) {
	const GunnerSeatsModel* model = GunnerSeats_Model(objectType);

	if (model == NULL || seat < 0 || seat >= model->count || field < 0 || field >= XWA_GUNNER_SEAT_FIELDS) {
		return stock;
	}
	return model->value[seat][field];
}

int XwaGunnerSeats_ForwardFire(unsigned int objectType) {
	const GunnerSeatsModel* model = GunnerSeats_Model(objectType);

	return model == NULL || model->forwardFire;
}

int XwaGunnerSeats_CockpitHasHardpoint(unsigned int cockpitType, XwaGunnerHardpoint kind) {
	const int meshCount = ModelMesh_GetObjectTypeMeshCount((int)cockpitType);
	int mesh;

	for (mesh = 0; mesh < meshCount; ++mesh) {
		const int count = ModelMesh_CountHardpoints((int)cockpitType, mesh);
		int index;

		for (index = 0; index < count; ++index) {
			OptHardpointType type;
			int x;
			int y;
			int z;

			ModelMesh_GetHardpoint((int)cockpitType, mesh, index, &type, &x, &y, &z);
			switch (type) {
				case OPT_HARDPOINT_RebelLaser:
				case OPT_HARDPOINT_TurboRebelLaser:
				case OPT_HARDPOINT_EmpireLaser:
				case OPT_HARDPOINT_TurboEmpireLaser:
				case OPT_HARDPOINT_SuperRebelLaser:
				case OPT_HARDPOINT_SuperEmpireLaser:
					if (kind == XWA_GUNNER_HARDPOINT_LASER) {
						return 1;
					}
					break;
				case OPT_HARDPOINT_IonCannon:
				case OPT_HARDPOINT_TurboIonCannon:
				case OPT_HARDPOINT_SuperIonCannon:
					if (kind == XWA_GUNNER_HARDPOINT_ION) {
						return 1;
					}
					break;
				case OPT_HARDPOINT_Torpedo:
				case OPT_HARDPOINT_Missile:
				case OPT_HARDPOINT_SuperTorpedo:
				case OPT_HARDPOINT_SuperMissile:
				case OPT_HARDPOINT_DumbBomb:
				case OPT_HARDPOINT_FiredBomb:
				case OPT_HARDPOINT_MagPulse:
				case OPT_HARDPOINT_TurboMagPulse:
				case OPT_HARDPOINT_SuperMagPulse:
					if (kind == XWA_GUNNER_HARDPOINT_WARHEAD) {
						return 1;
					}
					break;
				default:
					break;
			}
		}
	}
	return 0;
}

int XwaGunnerSeats_CockpitFileName(unsigned int objectType, char* fileName, size_t fileNameSize) {
	char key[XWA_MODEL_SOURCE_KEY_MAX];
	int written;

	if (!XwaCraftDefinitions_GunnerCockpit(objectType) || !XwaModelSource_TypeKey(objectType, key)) {
		return 0;
	}
	written = snprintf(fileName, fileNameSize, "%s.opt", key);
	return written > 0 && (size_t)written < fileNameSize;
}
