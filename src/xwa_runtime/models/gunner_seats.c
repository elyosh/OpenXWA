#include "xwa_runtime/models/gunner_seats.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/model_mesh.h"
#include "xwa/assets/object_type.h"
#include "xwa_runtime/models/craft_definitions.h"
#include "xwa_runtime/models/model_settings.h"
#include "xwa_runtime/models/model_source.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Modern port module; no original counterpart. */

/* Game units per metre. */
static const double s_unitsPerMetre = 40.96;

/* The keys that a seat can have. */
static const char* const s_seatKeys[] = {
	"position_m",
	"mount_pitch_degrees",
	"mount_yaw_degrees",
	"aim_limit_pitch_degrees",
	"aim_limit_yaw_degrees",
	"cockpit",
	"inverted",
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

/* Reads the angle in degrees at `key` in `node`, and converts it to a signed
   16-bit angle in `out`. Angles wrap around, so 270 degrees gives the same
   value as -90 degrees. A missing key gives 0. If the value isn't a number
   from `min` to `max`, logs a warning and returns 0. */
static int GunnerSeats_ReadAngle(const char* path, const char* what, const AeronConfigNode* node,
								 const char* key, double min, double max, int32_t* out) {
	const AeronConfigNode* item = AeronConfigNode_MapGet(node, key);
	const AeronConfigNodeType type = AeronConfigNode_Type(item);
	const double degrees = AeronConfigNode_Float(item, 0.0);
	long steps;

	*out = 0;
	if (item == NULL) {
		return 1;
	}
	if ((type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT) || !(degrees >= min && degrees <= max)) {
		Aeron_LogWarn("xwa.models", "%s: %s.%s must be a number of degrees from %g to %g", path, what, key,
					  min, max);
		return 0;
	}
	steps = lround(degrees * 65536.0 / 360.0);
	*out = (int16_t)(uint16_t)(steps & 0xFFFF);
	return 1;
}

/* Reads the aim limit in degrees at `key` in `node`, from 0 to 180. The game
   reads a limit as a signed 16-bit angle, so 180 degrees becomes the largest
   value, one step less. */
static int GunnerSeats_ReadAimLimit(const char* path, const char* what, const AeronConfigNode* node,
									const char* key, int32_t* out) {
	if (!GunnerSeats_ReadAngle(path, what, node, key, 0.0, 180.0, out)) {
		return 0;
	}
	if (*out < 0) {
		*out = INT16_MAX;
	}
	return 1;
}

int XwaGunnerSeats_ReadPosition(const char* path, const char* what, const AeronConfigNode* position,
								int32_t* out) {
	int axis;

	if (AeronConfigNode_Type(position) != AERON_CONFIG_SEQUENCE ||
		AeronConfigNode_SequenceCount(position) != 3) {
		Aeron_LogWarn("xwa.models", "%s: %s must be [right, forward, up] in metres", path, what);
		return 0;
	}
	for (axis = 0; axis < 3; ++axis) {
		const AeronConfigNode* item = AeronConfigNode_SequenceGet(position, (size_t)axis);
		const AeronConfigNodeType type = AeronConfigNode_Type(item);
		const long units = lround(AeronConfigNode_Float(item, 0.0) * s_unitsPerMetre);

		if ((type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT) || units < INT16_MIN ||
			units > INT16_MAX) {
			Aeron_LogWarn("xwa.models", "%s: %s must be three numbers from %.3f to %.3f metres", path, what,
						  INT16_MIN / s_unitsPerMetre, INT16_MAX / s_unitsPerMetre);
			return 0;
		}
		out[axis] = (int32_t)units;
	}
	return 1;
}

int XwaGunnerSeats_ReadPlacement(const char* path, const char* what, const AeronConfigNode* node,
								 int32_t* value) {
	const AeronConfigNode* position = AeronConfigNode_MapGet(node, "position_m");
	int axis;

	for (axis = 0; axis < 3; ++axis) {
		value[XWA_GUNNER_SEAT_X + axis] = 0;
	}
	if (position != NULL) {
		char where[96];

		snprintf(where, sizeof where, "%s.position_m", what);
		if (!XwaGunnerSeats_ReadPosition(path, where, position, &value[XWA_GUNNER_SEAT_X])) {
			return 0;
		}
	}
	return GunnerSeats_ReadAngle(path, what, node, "mount_pitch_degrees", -360.0, 360.0,
								 &value[XWA_GUNNER_SEAT_MOUNT_ANGLE_A]) &&
		   GunnerSeats_ReadAngle(path, what, node, "mount_yaw_degrees", -360.0, 360.0,
								 &value[XWA_GUNNER_SEAT_MOUNT_ANGLE_B]) &&
		   GunnerSeats_ReadAimLimit(path, what, node, "aim_limit_pitch_degrees",
									&value[XWA_GUNNER_SEAT_AIM_LIMIT_A]) &&
		   GunnerSeats_ReadAimLimit(path, what, node, "aim_limit_yaw_degrees",
									&value[XWA_GUNNER_SEAT_AIM_LIMIT_B]);
}

/* Reads seat `node` into `value`. If the seat can't be used, logs a warning
   and returns 0. */
static int GunnerSeats_ReadSeat(const char* path, size_t index, const AeronConfigNode* node, int32_t* value) {
	const AeronConfigNode* item;
	char what[48];
	size_t i;

	snprintf(what, sizeof what, "gunner_seats.seats[%zu]", index);
	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: %s isn't a map. The seats are ignored", path, what);
		return 0;
	}
	for (i = 0; i < AeronConfigNode_MapCount(node); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(node, i);
		size_t known;

		for (known = 0;
			 known < sizeof s_seatKeys / sizeof s_seatKeys[0] && strcmp(key, s_seatKeys[known]) != 0;
			 ++known) {
		}
		if (known == sizeof s_seatKeys / sizeof s_seatKeys[0]) {
			Aeron_LogWarn("xwa.models", "%s: %s.%s isn't a valid seat key. It is ignored", path, what, key);
		}
	}
	if (!XwaGunnerSeats_ReadPlacement(path, what, node, value)) {
		Aeron_LogWarn("xwa.models", "%s: the seats are ignored", path);
		return 0;
	}
	item = AeronConfigNode_MapGet(node, "cockpit");
	value[XWA_GUNNER_SEAT_COCKPIT] =
		AeronConfigNode_Type(item) == AERON_CONFIG_STRING
			? (int32_t)GunnerSeats_CockpitType(AeronConfigNode_String(item, NULL))
			: 0;
	if (value[XWA_GUNNER_SEAT_COCKPIT] == 0) {
		Aeron_LogWarn("xwa.models", "%s: %s.cockpit isn't a gunner cockpit model. The seats are ignored",
					  path, what);
		return 0;
	}
	item = AeronConfigNode_MapGet(node, "inverted");
	if (item != NULL && AeronConfigNode_Type(item) != AERON_CONFIG_BOOL) {
		Aeron_LogWarn("xwa.models", "%s: %s.inverted must be true or false", path, what);
	}
	value[XWA_GUNNER_SEAT_INVERTED] = AeronConfigNode_Bool(item, 0) != 0;
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
