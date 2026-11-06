#include "xwa_remaster/speed_effect.h"

#include "aeron/asset/opt_model.h"
#include "aeron/log.h"
#include "aeron/render.h"
#include "aeron/scene/billboard.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/scene_lights.h"
#include "xwa_runtime/config/modern_speed_effect_options.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
	SPEED_EFFECT_MAX_PARTICLES = 1024,
	SPEED_EFFECT_TEXTURE_W = 32,
	SPEED_EFFECT_TEXTURE_H = 16,
};

#define SPEED_EFFECT_Q16_TO_RAD (6.28318530718f / 65536.0f)
/* The craft speed at which the streaks reach full brightness. */
#define SPEED_EFFECT_FULL_SPEED 35.0f
/* The game moves a craft by speed * 4660 / 256 / 236 world units each game
   tick (Object_UpdateLifetimeAndMovement). */
#define SPEED_EFFECT_UNITS_PER_SECOND_PER_SPEED                                                              \
	(4660.0f / 256.0f / 236.0f / (float)XWA_SNAP_GAME_TICK_SECONDS)
/* How fast the dust seems to pass, in metres per second for each unit of
   craft speed, at the sizes below. It is slower than the craft. */
#define SPEED_EFFECT_SHOWN_M_PER_SECOND_PER_SPEED 0.1423f
/* The dust stays in place, so it passes at the craft's real speed. All its
   distances are this many times the sizes below, so it crosses the screen at
   the shown speed, with the same size on the screen. */
#define SPEED_EFFECT_SCALE                                                                                   \
	(SPEED_EFFECT_UNITS_PER_SECOND_PER_SPEED / AERON_OPT_UNITS_PER_METER /                                   \
	 SPEED_EFFECT_SHOWN_M_PER_SECOND_PER_SPEED)
/* The size of the box of dust, before the scale. */
#define SPEED_EFFECT_BOX_M 50.0f
/* A streak is as long as the distance the dust moves in this time. */
#define SPEED_EFFECT_TRAIL_SECONDS 0.02f
/* Before the scale. */
#define SPEED_EFFECT_HALF_WIDTH_M 0.0075f
/* Thinner streaks are drawn this wide and dimmed to match, so they don't
   flicker. */
#define SPEED_EFFECT_MIN_HALF_WIDTH_PX 0.75f
/* The streaks fade in between these distances from the camera, before the
   scale. */
#define SPEED_EFFECT_NEAR_FADE_START_M 1.0f
#define SPEED_EFFECT_NEAR_FADE_END_M 2.0f
/* The streaks fade out from this fraction of the half box to the half box,
   so they don't pop when they wrap to the other side of the box. */
#define SPEED_EFFECT_FAR_FADE_START 0.6f

struct XwaRemasterSpeedEffect {
	AeronTexture* texture;
	/* The size of the box of dust, in whole world units. */
	int32_t box;
	/* Each streak's place in the box, in world units from 0 to the box
	   size. */
	float seeds[SPEED_EFFECT_MAX_PARTICLES][3];
};

static float speed_effect_smoothstep(float edge0, float edge1, float x) {
	float t = (x - edge0) / (edge1 - edge0);
	t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	return t * t * (3.0f - 2.0f * t);
}

/* A soft streak: U runs from the head to the tail, V across. It is bright
   near the head and fades out towards the tail and the sides. */
static AeronTexture* speed_effect_create_texture(void) {
	uint8_t texels[SPEED_EFFECT_TEXTURE_H][SPEED_EFFECT_TEXTURE_W][4];
	for (int y = 0; y < SPEED_EFFECT_TEXTURE_H; y++) {
		const float v = ((float)y + 0.5f) / (float)SPEED_EFFECT_TEXTURE_H;
		const float across = 1.0f - speed_effect_smoothstep(0.2f, 1.0f, fabsf(2.0f * v - 1.0f));
		for (int x = 0; x < SPEED_EFFECT_TEXTURE_W; x++) {
			const float u = ((float)x + 0.5f) / (float)SPEED_EFFECT_TEXTURE_W;
			const float along =
				speed_effect_smoothstep(0.0f, 0.25f, u) * (1.0f - speed_effect_smoothstep(0.4f, 1.0f, u));
			const uint8_t value = (uint8_t)lroundf(along * across * 255.0f);
			texels[y][x][0] = texels[y][x][1] = texels[y][x][2] = texels[y][x][3] = value;
		}
	}
	AeronTexture* texture = Aeron_CreateTexture(&(AeronTextureDesc) {
		.width = SPEED_EFFECT_TEXTURE_W,
		.height = SPEED_EFFECT_TEXTURE_H,
		.format = AERON_TEXTURE_FORMAT_RGBA8_UNORM,
		.usage = AERON_TEXTURE_USAGE_SAMPLED | AERON_TEXTURE_USAGE_TRANSFER_DST,
		.debug_name = "xwa.speed_effect",
	});
	if (!texture)
		return NULL;
	if (!Aeron_UploadTextureData(&(AeronTextureUploadDesc) { .texture = texture,
															 .width = SPEED_EFFECT_TEXTURE_W,
															 .height = SPEED_EFFECT_TEXTURE_H,
															 .raw_data = texels,
															 .raw_size = sizeof texels })) {
		Aeron_DestroyTexture(texture);
		return NULL;
	}
	return texture;
}

XwaRemasterSpeedEffect* XwaRemasterSpeedEffect_Create(void) {
	XwaRemasterSpeedEffect* effect = (XwaRemasterSpeedEffect*)calloc(1, sizeof *effect);
	if (!effect)
		return NULL;
	effect->texture = speed_effect_create_texture();
	if (!effect->texture) {
		Aeron_LogError("xwa.remaster", "speed effect: texture create failed");
		free(effect);
		return NULL;
	}
	effect->box = (int32_t)lroundf(SPEED_EFFECT_BOX_M * SPEED_EFFECT_SCALE * AERON_OPT_UNITS_PER_METER);
	uint32_t state = 0x9e3779b9u;
	for (int i = 0; i < SPEED_EFFECT_MAX_PARTICLES; i++) {
		for (int axis = 0; axis < 3; axis++) {
			state ^= state << 13;
			state ^= state >> 17;
			state ^= state << 5;
			effect->seeds[i][axis] = (float)(state >> 8) * ((float)effect->box / 16777216.0f);
		}
	}
	return effect;
}

void XwaRemasterSpeedEffect_Destroy(XwaRemasterSpeedEffect* effect) {
	if (!effect)
		return;
	if (effect->texture)
		Aeron_DestroyTexture(effect->texture);
	free(effect);
}

/* Moves a point `eye` by `px` pixels on the screen, at its own depth. */
static void speed_effect_offset(const XwaRemasterEffectView* view, const float eye[3], float px, float py,
								float out[3]) {
	out[0] = eye[0] + px * eye[2] / view->focal_x_px;
	out[1] = eye[1] + py * eye[2] / view->focal_y_px;
	out[2] = eye[2];
}

void XwaRemasterSpeedEffect_Submit(const XwaRemasterSpeedEffect* effect, AeronScene3D* scene,
								   const XwaSnapshot* snapshot, const XwaRemasterEffectView* view) {
	if (!effect || !scene || !snapshot || !view || !snapshot->flight_camera_valid)
		return;
	XwaModernSpeedEffectOptions options;
	XwaModernSpeedEffectOptions_Get(&options);
	const int count = (int)fminf(lroundf(options.particles), (float)SPEED_EFFECT_MAX_PARTICLES);
	if (!options.enabled || count <= 0)
		return;
	const XwaFlightCamera* camera = &snapshot->flight_camera;
	if (camera->in_hangar || camera->player_obj_idx < 0 || camera->focus_obj_idx != camera->player_obj_idx)
		return;
	const XwaFlightObject* craft = XwaSnapshot_FlightObjectAtSlot(snapshot, (uint32_t)camera->player_obj_idx);
	if (!craft || !craft->has_mobj)
		return;

	/* The streaks show from just above speed 0, and reach full brightness at
	   full speed. The brightness also scales the light they add. */
	const float speed = (float)craft->speed;
	const float level = options.brightness * fminf(speed / SPEED_EFFECT_FULL_SPEED, 1.0f) - 0.1f;
	if (level <= 0.0f)
		return;
	const float radiance =
		XwaRemaster_SrgbToLinear(level * options.brightness) * XwaRemaster_ClassicRadiance();

	/* The craft moves along this direction (FVIEW_calcrotatemove). The dust
	   comes towards the camera, so a streak's tail is ahead of its head. */
	const float pitch = (float)(int16_t)(0xc000 - craft->pitch) * SPEED_EFFECT_Q16_TO_RAD;
	const float yaw = (float)(int16_t)(-(int16_t)craft->yaw) * SPEED_EFFECT_Q16_TO_RAD;
	const float trail_length = speed * SPEED_EFFECT_UNITS_PER_SECOND_PER_SPEED * SPEED_EFFECT_TRAIL_SECONDS;
	const float trail_world[3] = { sinf(yaw) * cosf(pitch) * trail_length,
								   -cosf(yaw) * cosf(pitch) * trail_length, -sinf(pitch) * trail_length };
	float trail_eye[3];
	for (int r = 0; r < 3; r++)
		trail_eye[r] = view->world_to_eye[r * 3] * trail_world[0] +
					   view->world_to_eye[r * 3 + 1] * trail_world[1] +
					   view->world_to_eye[r * 3 + 2] * trail_world[2];

	/* The camera's place in the box. The render origin is whole units, so
	   its part below the box size is exact. */
	float camera_in_box[3];
	for (int r = 0; r < 3; r++) {
		const int64_t part = (int64_t)view->origin_world[r] % effect->box;
		camera_in_box[r] = (float)(part < 0 ? part + effect->box : part) + view->scene_origin[r];
	}

	const float box = (float)effect->box;
	const float half_box = 0.5f * box;
	const float scale = SPEED_EFFECT_SCALE * AERON_OPT_UNITS_PER_METER;
	const float near_start = SPEED_EFFECT_NEAR_FADE_START_M * scale;
	const float near_end = SPEED_EFFECT_NEAR_FADE_END_M * scale;
	const float half_width = SPEED_EFFECT_HALF_WIDTH_M * scale;
	static const float uvs[4][2] = { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 0 } };

	for (int i = 0; i < count; i++) {
		float offset[3];
		for (int r = 0; r < 3; r++) {
			const float d = effect->seeds[i][r] - camera_in_box[r];
			offset[r] = d - box * floorf(d / box + 0.5f);
		}
		const float distance = sqrtf(offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2]);
		const float fade =
			speed_effect_smoothstep(near_start, near_end, distance) *
			(1.0f - speed_effect_smoothstep(SPEED_EFFECT_FAR_FADE_START * half_box, half_box, distance));
		if (fade <= 0.0f)
			continue;
		float head[3], tail[3];
		for (int r = 0; r < 3; r++) {
			head[r] = view->world_to_eye[r * 3] * offset[0] + view->world_to_eye[r * 3 + 1] * offset[1] +
					  view->world_to_eye[r * 3 + 2] * offset[2];
			tail[r] = head[r] + trail_eye[r];
		}
		if (head[2] < near_start || tail[2] < near_start)
			continue;

		/* The streak's direction on the screen, in pixels. A streak seen end
		   on is a dot. */
		float dir_x = view->focal_x_px * (tail[0] / tail[2] - head[0] / head[2]);
		float dir_y = view->focal_y_px * (tail[1] / tail[2] - head[1] / head[2]);
		const float dir_length = sqrtf(dir_x * dir_x + dir_y * dir_y);
		if (dir_length > 1e-4f) {
			dir_x /= dir_length;
			dir_y /= dir_length;
		} else {
			dir_x = 1.0f;
			dir_y = 0.0f;
		}
		const float head_px = half_width * view->focal_y_px / head[2];
		const float tail_px = half_width * view->focal_y_px / tail[2];
		const float head_drawn_px = fmaxf(head_px, SPEED_EFFECT_MIN_HALF_WIDTH_PX);
		const float tail_drawn_px = fmaxf(tail_px, SPEED_EFFECT_MIN_HALF_WIDTH_PX);

		/* Each end reaches half a width past the streak, so the ends are
		   round and a dot has a size. */
		float eye[4][3];
		speed_effect_offset(view, head, (-dir_x - dir_y) * head_drawn_px, (-dir_y + dir_x) * head_drawn_px,
							eye[0]);
		speed_effect_offset(view, head, (-dir_x + dir_y) * head_drawn_px, (-dir_y - dir_x) * head_drawn_px,
							eye[1]);
		speed_effect_offset(view, tail, (dir_x + dir_y) * tail_drawn_px, (dir_y - dir_x) * tail_drawn_px,
							eye[2]);
		speed_effect_offset(view, tail, (dir_x - dir_y) * tail_drawn_px, (dir_y + dir_x) * tail_drawn_px,
							eye[3]);

		AeronSceneBillboardDesc desc;
		memset(&desc, 0, sizeof desc);
		desc.texture = effect->texture;
		desc.blend = AERON_SCENE_BILLBOARD_BLEND_ADDITIVE;
		desc.stage = AERON_SCENE_BILLBOARD_STAGE_OVERLAY;
		const float head_value = radiance * fade * head_px / head_drawn_px;
		const float tail_value = radiance * fade * tail_px / tail_drawn_px;
		for (int c = 0; c < 4; c++) {
			for (int r = 0; r < 3; r++)
				desc.corners[c][r] = view->scene_origin[r] + view->eye_to_scene[r * 3] * eye[c][0] +
									 view->eye_to_scene[r * 3 + 1] * eye[c][1] +
									 view->eye_to_scene[r * 3 + 2] * eye[c][2];
			const float value = c < 2 ? head_value : tail_value;
			desc.colors[c][0] = desc.colors[c][1] = desc.colors[c][2] = value;
			desc.colors[c][3] = 1.0f;
			desc.uv[c][0] = uvs[c][0];
			desc.uv[c][1] = uvs[c][1];
		}
		AeronScene_AddBillboard(scene, &desc);
	}
}
