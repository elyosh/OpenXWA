#include "xwa_runtime/frontend/room_anims.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "aeron/random.h"
#include "xwa/frontend/frontend_resources.h"
#include "xwa_runtime/frontend/frontend_geometry.h"
#include "xwa_runtime/frontend/frontend_video.h"
#include "xwa_runtime/timing/host_clock.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XWA_ANIMS_PATH "frontend/animations.yaml"
#define XWA_ANIMS_DIR "frontend/animations"

/* The rooms that draw their animations with XwaRoomAnims_Draw. */
static const char* const k_animRooms[] = { "concourse" };

typedef enum XwaAnimMissions {
	ANIM_MISSIONS_ALL,
	ANIM_MISSIONS_ONLY,   /* only the listed missions */
	ANIM_MISSIONS_EXCEPT, /* every mission but the listed ones */
} XwaAnimMissions;

typedef struct XwaRoomAnim {
	char path[160]; /* in the mod folder */
	float x, y;     /* 640x480 units */
	int has_size;
	float width, height; /* 640x480 units, with has_size */
	float scale;         /* without has_size: video pixels per design pixel */
	float fps;           /* frames per second, or 0 for the file's rate */
	XwaAnimMissions missions;
	int first_mission, mission_count; /* in g_animMissionIds */
} XwaRoomAnim;

typedef struct XwaRoomAnimGroup {
	const char* room; /* an entry of k_animRooms */
	int repeat_same;
	float delay_min, delay_max; /* seconds before each play */
	int first, count;           /* the group's animations in g_anims */
	/* Playback. */
	int active;  /* the room has been drawn since it was entered */
	int current; /* the animation that plays or waits to play, or -1 */
	int slot;    /* its video slot, or -1 */
	int waiting; /* the play starts at start_us, on the host clock */
	uint64_t start_us;
} XwaRoomAnimGroup;

static XwaRoomAnim* g_anims;
static int g_animCount;
static int* g_animMissionIds;
static int g_animMissionIdCount;
static XwaRoomAnimGroup* g_animGroups;
static int g_animGroupCount;
static int g_animsLoaded;
static uint32_t g_animRandom;

/* ---- the file ---------------------------------------------------------- */

static int anims_error(const AeronConfigNode* node, const char* format, ...) {
	char message[256];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	Aeron_LogError("xwa.frontend", "%s:%d: %s", XWA_ANIMS_PATH, AeronConfigNode_Line(node), message);
	return 0;
}

static int anims_is_number(const AeronConfigNode* node) {
	const AeronConfigNodeType type = AeronConfigNode_Type(node);
	return type == AERON_CONFIG_INT || type == AERON_CONFIG_FLOAT;
}

static float anims_number(const AeronConfigNode* node) { return (float)AeronConfigNode_Float(node, 0.0); }

/* Reads [a, b] into a and b. */
static int anims_pair(const AeronConfigNode* node, float* a, float* b) {
	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE || AeronConfigNode_SequenceCount(node) != 2 ||
		!anims_is_number(AeronConfigNode_SequenceGet(node, 0)) ||
		!anims_is_number(AeronConfigNode_SequenceGet(node, 1))) {
		return 0;
	}
	*a = anims_number(AeronConfigNode_SequenceGet(node, 0));
	*b = anims_number(AeronConfigNode_SequenceGet(node, 1));
	return 1;
}

/* Checks that every key of the map `node` is one of `keys`. */
static int anims_check_keys(const AeronConfigNode* node, const char* const* keys, const char* what) {
	for (size_t i = 0; i < AeronConfigNode_MapCount(node); i++) {
		const char* key = AeronConfigNode_MapKeyAt(node, i);
		int k = 0;
		while (keys[k] && strcmp(keys[k], key) != 0) {
			k++;
		}
		if (!keys[k]) {
			return anims_error(AeronConfigNode_MapValueAt(node, i), "unknown key '%s' in %s", key, what);
		}
	}
	return 1;
}

static int anims_add_mission_ids(const AeronConfigNode* list, XwaRoomAnim* anim) {
	if (AeronConfigNode_Type(list) != AERON_CONFIG_SEQUENCE) {
		return 0;
	}
	anim->first_mission = g_animMissionIdCount;
	for (size_t i = 0; i < AeronConfigNode_SequenceCount(list); i++) {
		const AeronConfigNode* id = AeronConfigNode_SequenceGet(list, i);
		int* grown;
		if (AeronConfigNode_Type(id) != AERON_CONFIG_INT) {
			return 0;
		}
		grown = (int*)realloc(g_animMissionIds, (size_t)(g_animMissionIdCount + 1) * sizeof(int));
		if (!grown) {
			return 0;
		}
		g_animMissionIds = grown;
		g_animMissionIds[g_animMissionIdCount++] = (int)AeronConfigNode_Int(id, 0);
	}
	anim->mission_count = g_animMissionIdCount - anim->first_mission;
	return 1;
}

static int anims_read_missions(const AeronConfigNode* node, XwaRoomAnim* anim) {
	static const char* const keys[] = { "except", NULL };
	anim->missions = ANIM_MISSIONS_ALL;
	if (!node) {
		return 1;
	}
	switch (AeronConfigNode_Type(node)) {
		case AERON_CONFIG_STRING:
			if (strcmp(AeronConfigNode_String(node, ""), "all") == 0) {
				return 1;
			}
			break;
		case AERON_CONFIG_SEQUENCE:
			anim->missions = ANIM_MISSIONS_ONLY;
			if (anims_add_mission_ids(node, anim)) {
				return 1;
			}
			break;
		case AERON_CONFIG_MAP:
			anim->missions = ANIM_MISSIONS_EXCEPT;
			if (anims_check_keys(node, keys, "missions") &&
				anims_add_mission_ids(AeronConfigNode_MapGet(node, "except"), anim)) {
				return 1;
			}
			break;
		default:
			break;
	}
	return anims_error(node, "'missions' must be all, a list of mission numbers, or {except: [...]}");
}

static int anims_read_animation(const AeronConfigNode* node) {
	static const char* const keys[] = { "video", "at", "size", "scale", "fps", "missions", NULL };
	const AeronConfigNode* video = AeronConfigNode_MapGet(node, "video");
	const AeronConfigNode* size = AeronConfigNode_MapGet(node, "size");
	const AeronConfigNode* scale = AeronConfigNode_MapGet(node, "scale");
	const AeronConfigNode* fps = AeronConfigNode_MapGet(node, "fps");
	XwaRoomAnim anim;
	XwaRoomAnim* grown;
	float x, y;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		return anims_error(node, "an animation must be a map");
	}
	if (!anims_check_keys(node, keys, "an animation")) {
		return 0;
	}
	memset(&anim, 0, sizeof(anim));
	if (AeronConfigNode_Type(video) != AERON_CONFIG_STRING || !AeronConfigNode_String(video, "")[0]) {
		return anims_error(video ? video : node, "an animation needs 'video', the name of its file");
	}
	snprintf(anim.path, sizeof(anim.path), "%s/%s", XWA_ANIMS_DIR, AeronConfigNode_String(video, ""));
	if (!anims_pair(AeronConfigNode_MapGet(node, "at"), &x, &y)) {
		return anims_error(node, "an animation needs 'at', its position as [x, y]");
	}
	anim.x = XwaFrontendGeometry_UnitsX(x);
	anim.y = XwaFrontendGeometry_UnitsY(y);
	if (size) {
		if (!anims_pair(size, &x, &y) || x <= 0.0f || y <= 0.0f) {
			return anims_error(size, "'size' must be [width, height], larger than 0");
		}
		anim.has_size = 1;
		anim.width = XwaFrontendGeometry_UnitsX(x);
		anim.height = XwaFrontendGeometry_UnitsY(y);
	}
	anim.scale = 1.0f;
	if (scale) {
		if (!anims_is_number(scale) || anims_number(scale) <= 0.0f) {
			return anims_error(scale, "'scale' must be a number larger than 0");
		}
		anim.scale = anims_number(scale);
	}
	if (fps) {
		if (!anims_is_number(fps) || anims_number(fps) <= 0.0f) {
			return anims_error(fps, "'fps' must be a number larger than 0");
		}
		anim.fps = anims_number(fps);
	}
	if (!anims_read_missions(AeronConfigNode_MapGet(node, "missions"), &anim)) {
		return 0;
	}
	grown = (XwaRoomAnim*)realloc(g_anims, (size_t)(g_animCount + 1) * sizeof(*g_anims));
	if (!grown) {
		return anims_error(node, "out of memory");
	}
	g_anims = grown;
	g_anims[g_animCount++] = anim;
	return 1;
}

static int anims_read_group(const AeronConfigNode* node) {
	static const char* const keys[] = { "room", "repeat", "delay", "animations", NULL };
	const AeronConfigNode* room = AeronConfigNode_MapGet(node, "room");
	const AeronConfigNode* repeat = AeronConfigNode_MapGet(node, "repeat");
	const AeronConfigNode* delay = AeronConfigNode_MapGet(node, "delay");
	const AeronConfigNode* animations = AeronConfigNode_MapGet(node, "animations");
	XwaRoomAnimGroup group;
	XwaRoomAnimGroup* grown;
	size_t r = 0;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		return anims_error(node, "a group must be a map");
	}
	if (!anims_check_keys(node, keys, "a group")) {
		return 0;
	}
	memset(&group, 0, sizeof(group));
	while (r < sizeof(k_animRooms) / sizeof(k_animRooms[0]) &&
		   strcmp(k_animRooms[r], AeronConfigNode_String(room, "")) != 0) {
		r++;
	}
	if (r == sizeof(k_animRooms) / sizeof(k_animRooms[0])) {
		return anims_error(room ? room : node, "a group needs 'room', one of: concourse");
	}
	group.room = k_animRooms[r];
	if (repeat) {
		const char* text = AeronConfigNode_String(repeat, "");
		if (strcmp(text, "same") == 0) {
			group.repeat_same = 1;
		} else if (strcmp(text, "random") != 0) {
			return anims_error(repeat, "'repeat' must be random or same");
		}
	}
	if (delay) {
		int valid = 1;
		if (anims_is_number(delay)) {
			group.delay_min = group.delay_max = anims_number(delay);
		} else {
			valid = anims_pair(delay, &group.delay_min, &group.delay_max);
		}
		if (!valid || group.delay_min < 0.0f || group.delay_max < group.delay_min) {
			return anims_error(delay, "'delay' must be a number of seconds, or [shortest, longest]");
		}
	}
	if (AeronConfigNode_Type(animations) != AERON_CONFIG_SEQUENCE ||
		AeronConfigNode_SequenceCount(animations) == 0) {
		return anims_error(animations ? animations : node, "a group needs 'animations', a list");
	}
	group.first = g_animCount;
	for (size_t i = 0; i < AeronConfigNode_SequenceCount(animations); i++) {
		if (!anims_read_animation(AeronConfigNode_SequenceGet(animations, i))) {
			return 0;
		}
	}
	group.count = g_animCount - group.first;
	group.current = -1;
	group.slot = -1;
	grown = (XwaRoomAnimGroup*)realloc(g_animGroups, (size_t)(g_animGroupCount + 1) * sizeof(*g_animGroups));
	if (!grown) {
		return anims_error(node, "out of memory");
	}
	g_animGroups = grown;
	g_animGroups[g_animGroupCount++] = group;
	return 1;
}

static void anims_free(void) {
	free(g_anims);
	free(g_animMissionIds);
	free(g_animGroups);
	g_anims = NULL;
	g_animMissionIds = NULL;
	g_animGroups = NULL;
	g_animCount = g_animMissionIdCount = g_animGroupCount = 0;
}

static int anims_read(const AeronConfigNode* root) {
	static const char* const keys[] = { "version", "groups", NULL };
	const AeronConfigNode* version = AeronConfigNode_MapGet(root, "version");
	const AeronConfigNode* groups = AeronConfigNode_MapGet(root, "groups");

	if (AeronConfigNode_Type(root) != AERON_CONFIG_MAP) {
		return anims_error(root, "the file must be a map");
	}
	if (!anims_check_keys(root, keys, "the file")) {
		return 0;
	}
	if (version &&
		(AeronConfigNode_Type(version) != AERON_CONFIG_INT || AeronConfigNode_Int(version, 0) != 1)) {
		return anims_error(version, "'version' must be 1");
	}
	if (AeronConfigNode_Type(groups) != AERON_CONFIG_SEQUENCE) {
		return anims_error(groups ? groups : root, "the file needs 'groups', a list");
	}
	for (size_t i = 0; i < AeronConfigNode_SequenceCount(groups); i++) {
		if (!anims_read_group(AeronConfigNode_SequenceGet(groups, i))) {
			return 0;
		}
	}
	return 1;
}

static void anims_load(void) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;

	if (g_animsLoaded) {
		return;
	}
	g_animsLoaded = 1;
	/* The animations use their own random generator, so they don't change the
	   game's rand() sequence. */
	if (!Aeron_RandomBytes(&g_animRandom, sizeof(g_animRandom)) || !g_animRandom) {
		g_animRandom = 0x2545f491u;
	}
	memset(&error, 0, sizeof(error));
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, XWA_ANIMS_PATH, &file, &error)) {
		if (error.code != AERON_CONFIG_ERROR_NOT_FOUND) {
			Aeron_LogError("xwa.frontend", "%s:%d: %s", XWA_ANIMS_PATH, error.line,
						   error.message[0] ? error.message : "cannot be read");
			Aeron_LogError("xwa.frontend", "%s is ignored", XWA_ANIMS_PATH);
		}
		return;
	}
	if (!anims_read(AeronConfigFile_Root(file))) {
		Aeron_LogError("xwa.frontend", "%s is ignored", XWA_ANIMS_PATH);
		anims_free();
	} else {
		Aeron_LogInfo("xwa.frontend", "%s: %d animations in %d groups", XWA_ANIMS_PATH, g_animCount,
					  g_animGroupCount);
	}
	AeronConfigFile_Destroy(file);
}

/* ---- playback ------------------------------------------------------------- */

static uint32_t anims_random(void) {
	uint32_t x = g_animRandom; /* xorshift32 */
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	g_animRandom = x;
	return x;
}

static int anim_in_mission(const XwaRoomAnim* anim) {
	int listed = 0;
	if (anim->missions == ANIM_MISSIONS_ALL) {
		return 1;
	}
	for (int i = 0; i < anim->mission_count; i++) {
		if (g_animMissionIds[anim->first_mission + i] == g_currentMissionId) {
			listed = 1;
		}
	}
	return anim->missions == ANIM_MISSIONS_ONLY ? listed : !listed;
}

/* A random animation of the group for the current mission, or -1. Sets
 * *candidates to the number of animations it picked from. */
static int group_pick(const XwaRoomAnimGroup* group, int* candidates) {
	int pick;
	*candidates = 0;
	for (int i = 0; i < group->count; i++) {
		*candidates += anim_in_mission(&g_anims[group->first + i]);
	}
	if (*candidates == 0) {
		return -1;
	}
	pick = (int)(anims_random() % (uint32_t)*candidates);
	for (int i = 0; i < group->count; i++) {
		if (anim_in_mission(&g_anims[group->first + i]) && pick-- == 0) {
			return group->first + i;
		}
	}
	return -1;
}

static void group_stop(XwaRoomAnimGroup* group) {
	XwaFrontendVideo_Close(group->slot);
	group->slot = -1;
	group->current = -1;
	group->waiting = 0;
}

/* Sets up `anim` to play next, after the group's delay. */
static void group_start(XwaRoomAnimGroup* group, int anim, int candidates, uint64_t now_us) {
	const float delay = group->delay_min +
						(group->delay_max - group->delay_min) * (float)(anims_random() % 10001u) / 10000.0f;
	if (anim != group->current || !XwaFrontendVideo_Player(group->slot)) {
		/* Without a delay, an animation that can only be followed by itself is
		   opened looping, so there is no empty frame between plays. */
		const int loop = group->delay_max <= 0.0f && (group->repeat_same || candidates == 1);
		group_stop(group);
		group->slot =
			XwaFrontendVideo_Open(AERON_VFS_ROOT_MOD, g_anims[anim].path, loop, 0, g_anims[anim].fps);
		if (group->slot < 0) {
			return;
		}
	}
	group->current = anim;
	group->waiting = 1;
	group->start_us = now_us + (uint64_t)(delay * 1000000.0f);
}

static void group_enter(XwaRoomAnimGroup* group, uint64_t now_us) {
	int candidates;
	int anim;
	group_stop(group);
	group->active = 1;
	anim = group_pick(group, &candidates);
	if (anim >= 0) {
		group_start(group, anim, candidates, now_us);
	}
}

/* After a play: the same animation with `repeat: same`, else a new pick. */
static void group_next(XwaRoomAnimGroup* group, uint64_t now_us) {
	int candidates;
	int anim = group_pick(group, &candidates);
	if (group->repeat_same) {
		anim = group->current;
	}
	if (anim < 0) {
		group_stop(group);
	} else {
		group_start(group, anim, candidates, now_us);
	}
}

static void anim_draw(const XwaRoomAnim* anim, int slot, AeronVideoPlayer* player) {
	float width = anim->width;
	float height = anim->height;
	if (!anim->has_size) {
		AeronVideoInfo info;
		if (!Aeron_VideoGetInfo(player, &info)) {
			return; /* the video isn't open yet, so it has no frame */
		}
		width = XwaFrontendGeometry_UnitsX((float)info.display_width / anim->scale);
		height = XwaFrontendGeometry_UnitsY((float)info.display_height / anim->scale);
	}
	XwaFrontendVideo_Draw(slot, anim->x, anim->y, width, height);
}

void XwaRoomAnims_Draw(const char* room, int frame) {
	const uint64_t now_us = XwaTime_GetElapsedUs();
	anims_load();
	for (int i = 0; i < g_animGroupCount; i++) {
		XwaRoomAnimGroup* group = &g_animGroups[i];
		AeronVideoPlayer* player;
		AeronVideoState state;

		if (strcmp(group->room, room) != 0) {
			continue;
		}
		if (frame == 0 || !group->active) {
			group_enter(group, now_us);
		}
		player = XwaFrontendVideo_Player(group->slot);
		if (!player) {
			continue;
		}
		if (group->waiting) {
			if (now_us < group->start_us) {
				continue;
			}
			Aeron_VideoPlay(player);
			group->waiting = 0;
		}
		Aeron_VideoUpdate(player);
		state = Aeron_VideoGetState(player);
		if (state == AERON_VIDEO_ERROR) {
			group_stop(group); /* Aeron has logged the error */
			continue;
		}
		anim_draw(&g_anims[group->current], group->slot, player);
		if (state == AERON_VIDEO_ENDED) {
			group_next(group, now_us);
		}
	}
}

void XwaRoomAnims_Leave(const char* room) {
	for (int i = 0; i < g_animGroupCount; i++) {
		if (strcmp(g_animGroups[i].room, room) == 0) {
			group_stop(&g_animGroups[i]);
			g_animGroups[i].active = 0;
		}
	}
}

void XwaRoomAnims_Shutdown(void) {
	for (int i = 0; i < g_animGroupCount; i++) {
		group_stop(&g_animGroups[i]);
	}
	anims_free();
	g_animsLoaded = 0;
}
