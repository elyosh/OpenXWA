#include "xwa_runtime/frontend/concourse_layout.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/frontend/mission_setup.h"
#include "xwa_runtime/frontend/frontend_geometry.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XWA_LAYOUT_PATH "frontend/concourse/layout.yaml"

typedef enum XwaLayoutKind {
	LAYOUT_SECTION, /* a map of other keys */
	LAYOUT_INT,
	LAYOUT_BOOL,
	LAYOUT_X,         /* a position across, in design pixels */
	LAYOUT_Y,         /* a position down */
	LAYOUT_POINT,     /* [x, y] */
	LAYOUT_RECT,      /* [x, y, width, height] */
	LAYOUT_POINTS,    /* [[x, y], ...] */
	LAYOUT_POINT_MAP, /* {number: [x, y], ...} */
	LAYOUT_PROGRESS,  /* linear or stock */
} XwaLayoutKind;

/* The keys the file can have. */
static const struct {
	const char* key;
	XwaLayoutKind kind;
} k_layoutKeys[] = {
	{ "version", LAYOUT_INT },
	{ "starfield", LAYOUT_BOOL },
	{ "planet", LAYOUT_SECTION },
	{ "planet.left", LAYOUT_X },
	{ "planet.right", LAYOUT_X },
	{ "planet.top", LAYOUT_Y },
	{ "planet.bottom", LAYOUT_Y },
	{ "doors", LAYOUT_SECTION },
	{ "doors.tour", LAYOUT_POINT },
	{ "doors.marko", LAYOUT_POINT },
	{ "doors.training", LAYOUT_POINT },
	{ "doors.combat", LAYOUT_POINT },
	{ "doors.film", LAYOUT_POINT },
	{ "doors.tech", LAYOUT_POINT },
	{ "combat_room", LAYOUT_SECTION },
	{ "combat_room.multiplayer", LAYOUT_SECTION },
	{ "combat_room.multiplayer.door", LAYOUT_POINT },
	{ "combat_room.multiplayer.selection", LAYOUT_RECT },
	{ "combat_room.single_player", LAYOUT_SECTION },
	{ "combat_room.single_player.door", LAYOUT_POINT },
	{ "combat_room.single_player.selection", LAYOUT_RECT },
	{ "combat_room.back_door", LAYOUT_POINT },
	{ "family_room", LAYOUT_SECTION },
	{ "family_room.back", LAYOUT_RECT },
	{ "family_room.statistics", LAYOUT_RECT },
	{ "family_room.play_mission", LAYOUT_RECT },
	{ "family_room.marko_eye", LAYOUT_POINT },
	{ "family_room.monitor", LAYOUT_POINT },
	{ "family_room.lady_blue_mission", LAYOUT_INT },
	{ "family_room.cologne_mission", LAYOUT_INT },
	{ "loading_screen", LAYOUT_SECTION },
	{ "loading_screen.item", LAYOUT_SECTION },
	{ "loading_screen.item.left", LAYOUT_X },
	{ "loading_screen.item.right", LAYOUT_X },
	{ "loading_screen.item.height", LAYOUT_Y },
	{ "loading_screen.progress", LAYOUT_PROGRESS },
	{ "mission_selection_x", LAYOUT_X },
	{ "briefing", LAYOUT_SECTION },
	{ "briefing.squad_logos", LAYOUT_POINT_MAP },
	{ "create_pilot", LAYOUT_SECTION },
	{ "create_pilot.name_field", LAYOUT_RECT },
	{ "cutscene_viewer", LAYOUT_SECTION },
	{ "cutscene_viewer.scroll_bar_x", LAYOUT_X },
	{ "medals", LAYOUT_SECTION },
	{ "medals.positions", LAYOUT_POINTS },
	{ "medals.kalidor", LAYOUT_POINT },
	{ "award_ceremony", LAYOUT_SECTION },
	{ "award_ceremony.zoom_texts", LAYOUT_BOOL },
	{ "award_ceremony.battle", LAYOUT_SECTION },
	{ "award_ceremony.battle.images", LAYOUT_POINTS },
	{ "award_ceremony.battle.text_offsets", LAYOUT_POINTS },
	{ "award_ceremony.kalidor", LAYOUT_SECTION },
	{ "award_ceremony.kalidor.images", LAYOUT_POINTS },
	{ "award_ceremony.kalidor.text_offsets", LAYOUT_POINTS },
};

static AeronConfigFile* g_layoutFile;
static int g_layoutLoaded;

static int layout_is_number(const AeronConfigNode* node) {
	const AeronConfigNodeType type = AeronConfigNode_Type(node);
	return type == AERON_CONFIG_INT || type == AERON_CONFIG_FLOAT;
}

static int layout_is_numbers(const AeronConfigNode* node, size_t count) {
	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE || AeronConfigNode_SequenceCount(node) != count) {
		return 0;
	}
	for (size_t i = 0; i < count; i++) {
		if (!layout_is_number(AeronConfigNode_SequenceGet(node, i))) {
			return 0;
		}
	}
	return 1;
}

static int layout_check_value(const AeronConfigNode* node, XwaLayoutKind kind) {
	switch (kind) {
		case LAYOUT_INT:
			return AeronConfigNode_Type(node) == AERON_CONFIG_INT;
		case LAYOUT_BOOL:
			return AeronConfigNode_Type(node) == AERON_CONFIG_BOOL;
		case LAYOUT_X:
		case LAYOUT_Y:
			return layout_is_number(node);
		case LAYOUT_POINT:
			return layout_is_numbers(node, 2);
		case LAYOUT_RECT:
			return layout_is_numbers(node, 4);
		case LAYOUT_POINTS:
			if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE) {
				return 0;
			}
			for (size_t i = 0; i < AeronConfigNode_SequenceCount(node); i++) {
				if (!layout_is_numbers(AeronConfigNode_SequenceGet(node, i), 2)) {
					return 0;
				}
			}
			return 1;
		case LAYOUT_POINT_MAP:
			if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
				return 0;
			}
			for (size_t i = 0; i < AeronConfigNode_MapCount(node); i++) {
				char* end = NULL;
				(void)strtol(AeronConfigNode_MapKeyAt(node, i), &end, 10);
				if (!end || *end != '\0' || !layout_is_numbers(AeronConfigNode_MapValueAt(node, i), 2)) {
					return 0;
				}
			}
			return 1;
		case LAYOUT_PROGRESS: {
			const char* text = AeronConfigNode_String(node, "");
			return strcmp(text, "linear") == 0 || strcmp(text, "stock") == 0;
		}
		case LAYOUT_SECTION:
			return AeronConfigNode_Type(node) == AERON_CONFIG_MAP;
	}
	return 0;
}

static const char* const k_layoutKindText[] = {
	"a map",
	"a whole number",
	"true or false",
	"a number",
	"a number",
	"[x, y]",
	"[x, y, width, height]",
	"a list of [x, y]",
	"a map of numbers to [x, y]",
	"linear or stock",
};

/* Checks every key of the map `node`, whose path is `prefix`. */
static int layout_check_map(const AeronConfigNode* node, const char* prefix) {
	for (size_t i = 0; i < AeronConfigNode_MapCount(node); i++) {
		const char* name = AeronConfigNode_MapKeyAt(node, i);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(node, i);
		char key[128];
		size_t k = 0;

		snprintf(key, sizeof(key), "%s%s%s", prefix, prefix[0] ? "." : "", name);
		while (k < sizeof(k_layoutKeys) / sizeof(k_layoutKeys[0]) && strcmp(k_layoutKeys[k].key, key) != 0) {
			k++;
		}
		if (k == sizeof(k_layoutKeys) / sizeof(k_layoutKeys[0])) {
			Aeron_LogError("xwa.frontend", "%s:%d: unknown key '%s'", XWA_LAYOUT_PATH,
						   AeronConfigNode_Line(value), key);
			return 0;
		}
		if (!layout_check_value(value, k_layoutKeys[k].kind)) {
			Aeron_LogError("xwa.frontend", "%s:%d: '%s' must be %s", XWA_LAYOUT_PATH,
						   AeronConfigNode_Line(value), key, k_layoutKindText[k_layoutKeys[k].kind]);
			return 0;
		}
		if (k_layoutKeys[k].kind == LAYOUT_SECTION && !layout_check_map(value, key)) {
			return 0;
		}
	}
	return 1;
}

static const AeronConfigNode* layout_node(const char* key) {
	if (!g_layoutLoaded) {
		AeronConfigError error;

		g_layoutLoaded = 1;
		memset(&error, 0, sizeof(error));
		if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, XWA_LAYOUT_PATH, &g_layoutFile,
										&error)) {
			if (error.code != AERON_CONFIG_ERROR_NOT_FOUND) {
				Aeron_LogError("xwa.frontend", "%s:%d: %s", XWA_LAYOUT_PATH, error.line,
							   error.message[0] ? error.message : "cannot be read");
				Aeron_LogError("xwa.frontend", "%s is ignored", XWA_LAYOUT_PATH);
			}
			g_layoutFile = NULL;
		} else if (AeronConfigNode_Type(AeronConfigFile_Root(g_layoutFile)) != AERON_CONFIG_MAP ||
				   !layout_check_map(AeronConfigFile_Root(g_layoutFile), "")) {
			Aeron_LogError("xwa.frontend", "%s is ignored", XWA_LAYOUT_PATH);
			AeronConfigFile_Destroy(g_layoutFile);
			g_layoutFile = NULL;
		} else {
			Aeron_LogInfo("xwa.frontend", "%s: loaded room positions", XWA_LAYOUT_PATH);
		}
	}
	return g_layoutFile ? AeronConfigFile_GetNode(g_layoutFile, key) : NULL;
}

static float layout_number(const AeronConfigNode* list, size_t index) {
	return (float)AeronConfigNode_Float(AeronConfigNode_SequenceGet(list, index), 0.0);
}

static int layout_point_node(const AeronConfigNode* node, float* x, float* y) {
	if (!layout_is_numbers(node, 2)) {
		return 0;
	}
	*x = XwaFrontendGeometry_UnitsX(layout_number(node, 0));
	*y = XwaFrontendGeometry_UnitsY(layout_number(node, 1));
	return 1;
}

int XwaConcourseLayout_Point(const char* key, float* x, float* y) {
	return layout_point_node(layout_node(key), x, y);
}

int XwaConcourseLayout_PointAt(const char* key, int index, float* x, float* y) {
	const AeronConfigNode* list = layout_node(key);
	if (index < 0 || (size_t)index >= AeronConfigNode_SequenceCount(list)) {
		return 0;
	}
	return layout_point_node(AeronConfigNode_SequenceGet(list, (size_t)index), x, y);
}

int XwaConcourseLayout_Rect(const char* key, float* x, float* y, float* width, float* height) {
	const AeronConfigNode* node = layout_node(key);
	if (!layout_is_numbers(node, 4)) {
		return 0;
	}
	*x = XwaFrontendGeometry_UnitsX(layout_number(node, 0));
	*y = XwaFrontendGeometry_UnitsY(layout_number(node, 1));
	*width = XwaFrontendGeometry_UnitsX(layout_number(node, 2));
	*height = XwaFrontendGeometry_UnitsY(layout_number(node, 3));
	return 1;
}

float XwaConcourseLayout_X(const char* key, float stock) {
	const AeronConfigNode* node = layout_node(key);
	return layout_is_number(node) ? XwaFrontendGeometry_UnitsX((float)AeronConfigNode_Float(node, 0.0))
								  : stock;
}

float XwaConcourseLayout_Y(const char* key, float stock) {
	const AeronConfigNode* node = layout_node(key);
	return layout_is_number(node) ? XwaFrontendGeometry_UnitsY((float)AeronConfigNode_Float(node, 0.0))
								  : stock;
}

int XwaConcourseLayout_Int(const char* key, int stock) {
	const AeronConfigNode* node = layout_node(key);
	return AeronConfigNode_Type(node) == AERON_CONFIG_INT ? (int)AeronConfigNode_Int(node, stock) : stock;
}

int XwaConcourseLayout_Bool(const char* key, int stock) {
	return AeronConfigNode_Bool(layout_node(key), stock) != 0;
}

int XwaConcourseLayout_LinearProgress(void) {
	return strcmp(AeronConfigNode_String(layout_node("loading_screen.progress"), "stock"), "linear") == 0;
}

int XwaConcourseLayout_SquadLogo(int id, float* x, float* y) {
	char key[16];
	snprintf(key, sizeof(key), "%d", id);
	return layout_point_node(AeronConfigNode_MapGet(layout_node("briefing.squad_logos"), key), x, y);
}

int XwaConcourseLayout_Round(float value) { return (int)floorf(value + 0.5f); }

int XwaConcourseLayout_DrawPoint(const char* key, int* x, int* y) {
	float px, py;
	if (!XwaConcourseLayout_Point(key, &px, &py)) {
		return 0;
	}
	*x = (int)floorf(px);
	*y = (int)floorf(py);
	XwaSnapshot_SetNextSpriteFraction((int)lroundf((px - (float)*x) * 256.0f),
									  (int)lroundf((py - (float)*y) * 256.0f));
	return 1;
}

int XwaConcourseLayout_HitPoint(const char* key, int* x, int* y) {
	float px, py;
	if (!XwaConcourseLayout_Point(key, &px, &py)) {
		return 0;
	}
	*x = XwaConcourseLayout_Round(px);
	*y = XwaConcourseLayout_Round(py);
	return 1;
}

void XwaConcourseLayout_DrawSprite(const char* name, const char* key, int x, int y) {
	XwaConcourseLayout_DrawPoint(key, &x, &y);
	FrontImage_DrawSprite(name, x, y);
	XwaSnapshot_SetNextSpriteFraction(0, 0); /* clear it in case nothing was drawn */
}

void XwaConcourseLayout_DrawSpriteOpaque(const char* name, const char* key, int x, int y) {
	XwaConcourseLayout_DrawPoint(key, &x, &y);
	FrontImage_DrawSpriteOpaque(name, x, y);
	XwaSnapshot_SetNextSpriteFraction(0, 0);
}

void XwaConcourseLayout_OffsetRect(FrontendRect* rect, const char* key, int x, int y) {
	XwaConcourseLayout_HitPoint(key, &x, &y);
	FrontendDraw_RectOffsetXY(rect, x, y);
}

void XwaConcourseLayout_AssignRect(FrontendRect* rect, const char* key, int left, int top, int right,
								   int bottom) {
	float x, y, width, height;
	if (XwaConcourseLayout_Rect(key, &x, &y, &width, &height)) {
		left = XwaConcourseLayout_Round(x);
		top = XwaConcourseLayout_Round(y);
		right = left + XwaConcourseLayout_Round(width);
		bottom = top + XwaConcourseLayout_Round(height);
	}
	FrontendDraw_RectAssign(rect, left, top, right, bottom);
}

void XwaConcourseLayout_AssignMovedRect(FrontendRect* rect, const char* key, int x, int y, int left, int top,
										int right, int bottom) {
	int moved_x = x;
	int moved_y = y;
	FrontendDraw_RectAssign(rect, left, top, right, bottom);
	if (XwaConcourseLayout_HitPoint(key, &moved_x, &moved_y)) {
		FrontendDraw_RectOffsetXY(rect, moved_x - x, moved_y - y);
	}
}

int XwaConcourseLayout_UnlockMission(const char* key, int mission) {
	const int value = XwaConcourseLayout_Int(key, mission);
	return value < 0 || value >= 255 ? 0 : value;
}

int XwaConcourseLayout_Unlocked(const char* key, int mission) {
	const int value = XwaConcourseLayout_UnlockMission(key, mission);
	return value <= 0 || g_pilotData.tourOfDutyMissions[value - 1].completedCount != 0;
}

void XwaConcourseLayout_MedalPosition(int index, int* x, int* y) {
	float px, py;
	if (AeronConfigNode_Type(layout_node("medals.positions")) != AERON_CONFIG_SEQUENCE) {
		return;
	}
	if (XwaConcourseLayout_PointAt("medals.positions", index, &px, &py)) {
		*x = XwaConcourseLayout_Round(px);
		*y = XwaConcourseLayout_Round(py);
	} else {
		*x = 0;
		*y = 0;
	}
}

void XwaConcourseLayout_DrawSpriteCentered(const char* name, const char* key, int index, int x, int y,
										   int width, int height) {
	float px, py;
	if (XwaConcourseLayout_PointAt(key, index, &px, &py)) {
		x = XwaConcourseLayout_Round(px);
		y = XwaConcourseLayout_Round(py);
	}
	FrontImage_DrawSprite(name, x - (width >> 1), y - (height >> 1));
}

void XwaConcourseLayout_MoveRect(FrontendRect* rect, const char* key, int index) {
	float dx, dy;
	if (XwaConcourseLayout_PointAt(key, index, &dx, &dy)) {
		FrontendDraw_RectOffsetXY(rect, XwaConcourseLayout_Round(dx), XwaConcourseLayout_Round(dy));
	}
}

void XwaConcourseLayout_Shutdown(void) {
	AeronConfigFile_Destroy(g_layoutFile);
	g_layoutFile = NULL;
	g_layoutLoaded = 0;
}
