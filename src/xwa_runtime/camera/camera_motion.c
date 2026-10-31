#include "xwa_runtime/camera/camera_motion.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/flight/hangar.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/player/player.h"
#include "xwa/render/renderer.h"
#include "xwa/util/time.h"
#include "xwa_runtime/config/modern_camera_options.h"

#include <math.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define CAMERA_FLIGHT_YAML "flight/flight.yaml"

/* A tick of g_gameTime is 4 ms. */
#define CAMERA_SECONDS_PER_TICK 0.004f
/* A longer jump of the game time starts the lag again. */
#define CAMERA_MAX_STEP_TICKS (250 / 4)
/* The second speed filter is this many times as fast as the first one, as in
   XWAU: its steps of 10% and 5% per frame give ln 0.9 / ln 0.95. */
#define CAMERA_SLOW_SPEED_RATE 2.05f

#define CAMERA_PI 3.14159265358979323846f
#define CAMERA_Q16_TO_RADIANS (2.0f * CAMERA_PI / 65536.0f)
#define CAMERA_RADIANS_TO_Q16 (65536.0f / (2.0f * CAMERA_PI))
#define CAMERA_DEGREES_TO_Q16 (65536.0f / 360.0f)

static struct {
	/* Whether the state below follows a craft. */
	int valid;
	int objectIdx;
	int lastTime;
	/* A unit vector that trails the craft's forward axis. */
	float slowForward[3];
	/* smoothSpeed trails the craft's speed, and slowSpeed trails
	   smoothSpeed. */
	float smoothSpeed;
	float slowSpeed;
	/* The lag angles, as sines, which are close to radians for small
	   angles. They are positive when the craft turns right or up. */
	float lagSide;
	float lagUp;
	/* smoothSpeed - slowSpeed, in the unit of MobileObject.speed. It is
	   positive while the craft speeds up. */
	float speedChange;
	/* The craft's axes at the last update, in world axes. */
	float right[3];
	float up[3];
	float forward[3];
} s_cameraMotion;

/* Reads one key of the mod's `camera` section into `options`. Returns 0
   (logged) when the value is wrong. */
static int CameraMotion_ReadKey(const XwaModernCameraOptionKey* key, const AeronConfigNode* node,
								XwaModernCameraOptions* options) {
	char* field = (char*)options + key->offset;
	const AeronConfigNodeType type = AeronConfigNode_Type(node);

	if (key->is_switch) {
		int value;

		if (type != AERON_CONFIG_BOOL) {
			Aeron_LogWarn("xwa.flight", "%s:%d: camera.%s must be true or false; it is ignored",
						  CAMERA_FLIGHT_YAML, AeronConfigNode_Line(node), key->name);
			return 0;
		}
		value = AeronConfigNode_Bool(node, 0) != 0;
		memcpy(field, &value, sizeof value);
	} else {
		const double number = AeronConfigNode_Float(node, NAN);
		float value;

		if ((type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT) ||
			!(number >= key->min && number <= key->max)) {
			Aeron_LogWarn("xwa.flight", "%s:%d: camera.%s must be a number from %g to %g; it is ignored",
						  CAMERA_FLIGHT_YAML, AeronConfigNode_Line(node), key->name, (double)key->min,
						  (double)key->max);
			return 0;
		}
		value = (float)number;
		memcpy(field, &value, sizeof value);
	}
	return 1;
}

void XwaCameraMotion_Init(void) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* section;
	const XwaModernCameraOptionKey* keys;
	XwaModernCameraOptions options;
	size_t keyCount;
	size_t i;

	memset(&s_cameraMotion, 0, sizeof s_cameraMotion);
	memset(&error, 0, sizeof error);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, CAMERA_FLIGHT_YAML, &file, &error)) {
		/* The backdrops code already logs this error. */
		return;
	}
	root = AeronConfigFile_Root(file);
	section = AeronConfigNode_MapGet(root, "camera");
	if (AeronConfigNode_Type(AeronConfigNode_MapGet(root, "version")) != AERON_CONFIG_INT ||
		AeronConfigNode_Int(AeronConfigNode_MapGet(root, "version"), 0) != 1 || section == NULL) {
		AeronConfigFile_Destroy(file);
		return;
	}
	if (AeronConfigNode_Type(section) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.flight", "%s:%d: camera must be a map; it is ignored", CAMERA_FLIGHT_YAML,
					   AeronConfigNode_Line(section));
		AeronConfigFile_Destroy(file);
		return;
	}

	XwaModernCameraOptions_GetShipped(&options);
	options.enabled = 1;
	keys = XwaModernCameraOptions_Keys(&keyCount);
	for (i = 0; i < AeronConfigNode_MapCount(section); ++i) {
		const char* name = AeronConfigNode_MapKeyAt(section, i);
		const AeronConfigNode* node = AeronConfigNode_MapValueAt(section, i);
		size_t k;

		for (k = 0; k < keyCount && strcmp(keys[k].name, name) != 0; ++k) {
		}
		if (k == keyCount) {
			Aeron_LogWarn("xwa.flight", "%s:%d: camera.%s isn't a camera setting; it is ignored",
						  CAMERA_FLIGHT_YAML, AeronConfigNode_Line(node), name);
			continue;
		}
		CameraMotion_ReadKey(&keys[k], node, &options);
	}
	XwaModernCameraOptions_SetMod(&options);
	AeronConfigFile_Destroy(file);
}

int XwaCameraMotion_Enabled(void) {
	XwaModernCameraOptions options;

	XwaModernCameraOptions_Get(&options);
	return options.enabled;
}

/* Turns the rows of `rows` around the unit vector `axis` by a Q16 angle. This
   is the float version of FVIEW_transformaxes. */
static void CameraMotion_TurnRows(float rows[3][3], const float axis[3], int16_t angleQ16) {
	const float x = axis[0];
	const float y = axis[1];
	const float z = axis[2];
	const float angle = (float)angleQ16 * CAMERA_Q16_TO_RADIANS;
	const float c = cosf(angle);
	const float s = sinf(angle);
	int r;

	if (angleQ16 == 0) {
		return;
	}
	for (r = 0; r < 3; ++r) {
		const float vx = rows[r][0];
		const float vy = rows[r][1];
		const float vz = rows[r][2];
		const float along = (x * vx + y * vy + z * vz) * (1.0f - c);

		rows[r][0] = vx * c + (y * vz - z * vy) * s + x * along;
		rows[r][1] = vy * c + (z * vx - x * vz) * s + y * along;
		rows[r][2] = vz * c + (x * vy - y * vx) * s + z * along;
	}
}

/* The craft's right, up and forward axes, in world axes, from its angles.
   This is the float version of FVIEW_calcrotatemove and
   FVIEW_calcrotateorient. The axes that those functions keep in the mobile
   object can be older than the angles. */
static void CameraMotion_CraftAxes(const ObjectRecord* object, float right[3], float up[3],
								   float forward[3]) {
	const float a = (float)(int16_t)(0xc000 - object->pitch) * CAMERA_Q16_TO_RADIANS;
	const float b = (float)(int16_t)(-(int16_t)object->yaw) * CAMERA_Q16_TO_RADIANS;
	const float cosA = cosf(a);
	const float sinA = sinf(a);
	const float cosB = cosf(b);
	const float sinB = sinf(b);
	float rows[3][3];
	float axis[3];

	/* The rows of the game's curMat: right, up and backward. */
	rows[0][0] = cosB;
	rows[0][1] = sinB;
	rows[0][2] = 0.0f;
	rows[1][0] = -sinB * sinA;
	rows[1][1] = cosB * sinA;
	rows[1][2] = -cosA;
	rows[2][0] = -sinB * cosA;
	rows[2][1] = cosB * cosA;
	rows[2][2] = sinA;
	memcpy(axis, rows[1], sizeof axis);
	CameraMotion_TurnRows(rows, axis, (int16_t)object->angleD);
	memcpy(axis, rows[2], sizeof axis);
	CameraMotion_TurnRows(rows, axis, (int16_t)object->roll);
	if (object->mobj->spinAngleQ16 != 0) {
		axis[0] = object->mobj->spinAxisX;
		axis[1] = object->mobj->spinAxisY;
		axis[2] = object->mobj->spinAxisZ;
		CameraMotion_TurnRows(rows, axis, object->mobj->spinAngleQ16);
	}
	memcpy(right, rows[0], sizeof rows[0]);
	memcpy(up, rows[1], sizeof rows[1]);
	forward[0] = -rows[2][0];
	forward[1] = -rows[2][1];
	forward[2] = -rows[2][2];
}

void XwaCameraMotion_Update(int playerIdx) {
	const PlayerData* player = &g_players[playerIdx];
	XwaModernCameraOptions options;
	const ObjectRecord* object;
	float right[3];
	float up[3];
	float forward[3];
	float speed;
	float length;
	float k;
	float k2;
	int dt;
	int i;

	if (playerIdx != g_localPlayer) {
		return;
	}
	XwaModernCameraOptions_Get(&options);
	if (!options.enabled || (unsigned int)player->objectIndex == 0xffffu ||
		g_objectTable[player->objectIndex].mobj == NULL || player->hyperspacePhase || g_inHangarReady) {
		s_cameraMotion.valid = 0;
		return;
	}

	object = &g_objectTable[player->objectIndex];
	CameraMotion_CraftAxes(object, right, up, forward);
	memcpy(s_cameraMotion.right, right, sizeof right);
	memcpy(s_cameraMotion.up, up, sizeof up);
	memcpy(s_cameraMotion.forward, forward, sizeof forward);
	speed = (float)object->mobj->speed;
	dt = g_gameTime - s_cameraMotion.lastTime;
	if (!s_cameraMotion.valid || s_cameraMotion.objectIdx != player->objectIndex || dt < 0 ||
		dt > CAMERA_MAX_STEP_TICKS) {
		memcpy(s_cameraMotion.slowForward, forward, sizeof forward);
		s_cameraMotion.smoothSpeed = speed;
		s_cameraMotion.slowSpeed = speed;
		s_cameraMotion.lagSide = 0.0f;
		s_cameraMotion.lagUp = 0.0f;
		s_cameraMotion.speedChange = 0.0f;
		s_cameraMotion.objectIdx = player->objectIndex;
		s_cameraMotion.lastTime = g_gameTime;
		s_cameraMotion.valid = 1;
		return;
	}
	/* The game time stops while the game is paused. */
	if (dt == 0) {
		return;
	}
	s_cameraMotion.lastTime = g_gameTime;

	k = 1.0f - expf(-(float)dt * CAMERA_SECONDS_PER_TICK / options.lag_time);
	k2 = 1.0f - expf(-(float)dt * CAMERA_SECONDS_PER_TICK * CAMERA_SLOW_SPEED_RATE / options.lag_time);
	for (i = 0; i < 3; ++i) {
		s_cameraMotion.slowForward[i] += k * (forward[i] - s_cameraMotion.slowForward[i]);
	}
	length = sqrtf(s_cameraMotion.slowForward[0] * s_cameraMotion.slowForward[0] +
				   s_cameraMotion.slowForward[1] * s_cameraMotion.slowForward[1] +
				   s_cameraMotion.slowForward[2] * s_cameraMotion.slowForward[2]);
	/* The average is zero when the craft turned around in one step. */
	if (length > 1e-6f) {
		for (i = 0; i < 3; ++i) {
			s_cameraMotion.slowForward[i] /= length;
		}
	} else {
		memcpy(s_cameraMotion.slowForward, forward, sizeof forward);
	}
	s_cameraMotion.smoothSpeed += k * (speed - s_cameraMotion.smoothSpeed);
	s_cameraMotion.slowSpeed += k2 * (s_cameraMotion.smoothSpeed - s_cameraMotion.slowSpeed);

	/* When the craft turns right, its forward axis moves to the right, so the
	   slow axis is now to its left. */
	s_cameraMotion.lagSide =
		-(s_cameraMotion.slowForward[0] * right[0] + s_cameraMotion.slowForward[1] * right[1] +
		  s_cameraMotion.slowForward[2] * right[2]);
	s_cameraMotion.lagUp = -(s_cameraMotion.slowForward[0] * up[0] + s_cameraMotion.slowForward[1] * up[1] +
							 s_cameraMotion.slowForward[2] * up[2]);
	s_cameraMotion.speedChange = s_cameraMotion.smoothSpeed - s_cameraMotion.slowSpeed;
}

void XwaCameraMotion_ExteriorOffsets(int playerIdx, int16_t* pitchQ16, int16_t* yawQ16) {
	const PlayerData* player = &g_players[playerIdx];
	XwaModernCameraOptions options;
	float side = 0.0f;
	float up = 0.0f;
	float pitch;

	*pitchQ16 = 0;
	*yawQ16 = 0;
	XwaModernCameraOptions_Get(&options);
	/* In a Death Star mission, the game fires lasers from the camera
	   position, so moving the camera would change the mission. */
	if (!options.enabled || playerIdx != g_localPlayer || !player->viewState.externalCameraActive ||
		player->viewState.cameraFocusObjIdx != player->objectIndex || player->mapCameraState ||
		g_inHangarReady || (g_filmPlaybackMode && g_filmOverlayActive == 1) ||
		g_missionHeader.body.missionType == XWA_MISSION_TYPE_DEATH_STAR) {
		return;
	}

	if (options.exterior_lag && s_cameraMotion.valid) {
		const float limit = options.exterior_lag_max * (CAMERA_PI / 180.0f);
		float length;

		side = options.exterior_lag_strength * s_cameraMotion.lagSide;
		up = options.exterior_lag_strength * s_cameraMotion.lagUp;
		length = sqrtf(side * side + up * up);
		/* A soft limit: close to the length while it is small, and never
		   above `limit`. */
		if (limit <= 0.0f) {
			side = 0.0f;
			up = 0.0f;
		} else if (length > 0.0f) {
			const float scale = limit * tanhf(length / limit) / length;

			side *= scale;
			up *= scale;
		}
	}

	/* The camera turns toward the slow heading. A negative extra yaw turns it
	   to the craft's left, and a negative extra pitch turns it down, which
	   puts the camera above the craft. */
	pitch = -up * CAMERA_RADIANS_TO_Q16 - options.exterior_tilt * CAMERA_DEGREES_TO_Q16;
	*pitchQ16 = (int16_t)lroundf(pitch);
	*yawQ16 = (int16_t)lroundf(-side * CAMERA_RADIANS_TO_Q16);
}

void XwaCameraMotion_CockpitSway(float out[3]) {
	const PlayerData* player = &g_players[g_localPlayer];
	XwaModernCameraOptions options;
	float limit;
	float side;
	float up;
	float back;
	int i;

	out[0] = 0.0f;
	out[1] = 0.0f;
	out[2] = 0.0f;
	XwaModernCameraOptions_Get(&options);
	limit = options.cockpit_sway_max;
	if (!options.enabled || !options.cockpit_sway || limit <= 0.0f || !s_cameraMotion.valid ||
		s_cameraMotion.objectIdx != player->objectIndex || player->viewState.externalCameraActive ||
		player->viewState.cameraFocusObjIdx != player->objectIndex || player->mapCameraState ||
		g_inHangarReady || (g_filmPlaybackMode && g_filmOverlayActive == 1)) {
		return;
	}

	/* Each axis has a soft limit: close to the value while it is small, and
	   never above `limit`. */
	side = limit * tanhf(options.cockpit_sway_strength * s_cameraMotion.lagSide / limit);
	up = limit * tanhf(options.cockpit_sway_strength * s_cameraMotion.lagUp / limit);
	back = limit * tanhf(options.cockpit_speed_sway * s_cameraMotion.speedChange / limit);
	for (i = 0; i < 3; ++i) {
		out[i] =
			side * s_cameraMotion.right[i] + up * s_cameraMotion.up[i] - back * s_cameraMotion.forward[i];
	}
}
