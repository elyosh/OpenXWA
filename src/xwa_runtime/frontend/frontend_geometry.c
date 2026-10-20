#include "xwa_runtime/frontend/frontend_geometry.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/image.h"
#include "aeron/log.h"
#include "xwa/assets/model_texture.h"
#include "xwa/assets/object_type_info.h"
#include "xwa/util/byte_order.h"
#include "xwa/util/memory.h"
#include "xwa_runtime/models/mission_art.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XWA_GEOMETRY_DESIGN_PATH "frontend/frontend.yaml"
#define XWA_GEOMETRY_STARFIELD_MASK_PATH "frontend/concourse/starfield_mask.png"
#define XWA_GEOMETRY_KEY_SIZE 64

/* The rows the concourse starfield picks from (concourse.c, Concourse_InitStarfield). */
#define XWA_GEOMETRY_STARFIELD_TOP 25
#define XWA_GEOMETRY_STARFIELD_ROWS 100

typedef struct XwaGeometryEntry {
	char key[XWA_GEOMETRY_KEY_SIZE]; /* "<dir>/<base>", or "group_<g>" */
	XwaGeometryAtlas* atlas;         /* NULL when there is none */
} XwaGeometryEntry;

static XwaGeometryEntry* g_geometryEntries;
static int g_geometryEntryCount;
static int g_geometryEntryCapacity;

static int g_geometryDesignLoaded;
static float g_geometryDesignW = 640.0f;
static float g_geometryDesignH = 480.0f;
static XwaFrontendPaint g_geometryPaint = { 0.5f, 0, 0, 0, 0 };

static const ResourceDescriptor** g_geometryDescriptors;
static int g_geometryDescriptorCount;
static int g_geometryDescriptorCapacity;

static int g_geometryMaskState; /* 0 not read yet, 1 usable, -1 missing or unusable */
static uint8_t* g_geometryMask;
static int g_geometryMaskW;
static int g_geometryMaskH;

/* Reads `count` numbers from a sequence such as `[704, 357]`. */
static int geometry_read_numbers(const AeronConfigNode* node, float* out, size_t count) {
	if (AeronConfigNode_Type(node) != AERON_CONFIG_SEQUENCE || AeronConfigNode_SequenceCount(node) != count) {
		return 0;
	}
	for (size_t i = 0; i < count; ++i) {
		const AeronConfigNode* value = AeronConfigNode_SequenceGet(node, i);
		const AeronConfigNodeType type = AeronConfigNode_Type(value);
		if (type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT) {
			return 0;
		}
		out[i] = (float)AeronConfigNode_Float(value, 0.0);
	}
	return 1;
}

/* Reads the `paint` section. An invalid value is logged and that key keeps
 * the original look. */
static void geometry_read_paint(const AeronConfigNode* paint) {
	const AeronConfigNode* value;
	const char* text;

	if (paint == NULL) {
		return;
	}
	value = AeronConfigNode_MapGet(paint, "fill_alpha");
	if (value != NULL) {
		const float alpha = (float)AeronConfigNode_Float(value, -1.0);
		if (alpha >= 0.0f && alpha <= 1.0f) {
			g_geometryPaint.fill_alpha = alpha;
		} else {
			Aeron_LogError("xwa.frontend", "%s:%d: fill_alpha must be between 0 and 1",
						   XWA_GEOMETRY_DESIGN_PATH, AeronConfigNode_Line(value));
		}
	}
	value = AeronConfigNode_MapGet(paint, "line_width");
	if (value != NULL) {
		text = AeronConfigNode_String(value, "");
		if (strcmp(text, "pixel") == 0 || strcmp(text, "unit") == 0) {
			g_geometryPaint.pixel_lines = strcmp(text, "pixel") == 0;
		} else {
			Aeron_LogError("xwa.frontend", "%s:%d: line_width must be unit or pixel",
						   XWA_GEOMETRY_DESIGN_PATH, AeronConfigNode_Line(value));
		}
	}
	value = AeronConfigNode_MapGet(paint, "grid_colours");
	if (value != NULL) {
		const AeronConfigNode* major = AeronConfigNode_SequenceGet(value, 0);
		const AeronConfigNode* minor = AeronConfigNode_SequenceGet(value, 1);
		if (AeronConfigNode_SequenceCount(value) == 2 && AeronConfigNode_Type(major) == AERON_CONFIG_INT &&
			AeronConfigNode_Type(minor) == AERON_CONFIG_INT) {
			g_geometryPaint.has_grid_colours = 1;
			g_geometryPaint.grid_major = (uint32_t)AeronConfigNode_Int(major, 0) & 0xffffffu;
			g_geometryPaint.grid_minor = (uint32_t)AeronConfigNode_Int(minor, 0) & 0xffffffu;
		} else {
			Aeron_LogError("xwa.frontend",
						   "%s:%d: grid_colours must be [major, minor], such as [0xFF0000, 0x800000]",
						   XWA_GEOMETRY_DESIGN_PATH, AeronConfigNode_Line(value));
		}
	}
}

static void geometry_load_design(void) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* design;
	float size[2];

	if (g_geometryDesignLoaded) {
		return;
	}
	g_geometryDesignLoaded = 1;
	memset(&error, 0, sizeof(error));
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, XWA_GEOMETRY_DESIGN_PATH, &file,
									&error)) {
		if (error.code != AERON_CONFIG_ERROR_NOT_FOUND) {
			Aeron_LogError("xwa.frontend", "%s:%d: %s; the design screen is 640x480",
						   XWA_GEOMETRY_DESIGN_PATH, error.line,
						   error.message[0] ? error.message : "cannot be read");
		}
		return;
	}
	design = AeronConfigNode_MapGet(AeronConfigFile_Root(file), "design");
	if (design != NULL) {
		if (geometry_read_numbers(design, size, 2) && size[0] >= 1.0f && size[1] >= 1.0f) {
			g_geometryDesignW = size[0];
			g_geometryDesignH = size[1];
		} else {
			Aeron_LogError("xwa.frontend", "%s:%d: design must be [width, height] in pixels; using 640x480",
						   XWA_GEOMETRY_DESIGN_PATH, AeronConfigNode_Line(design));
		}
	}
	geometry_read_paint(AeronConfigNode_MapGet(AeronConfigFile_Root(file), "paint"));
	AeronConfigFile_Destroy(file);
}

static void geometry_free_atlas(XwaGeometryAtlas* atlas) {
	if (atlas != NULL) {
		free(atlas->frames);
		free(atlas->regions);
		free(atlas);
	}
}

/* Reads a frame's size and anchor in game units. The size comes from
 * `classic_w` and `classic_h`, which are required. The anchor comes from
 * `origin_x` and `origin_y`, which are 0 when missing. The keys in design
 * pixels are an error here, so that no value is read in the wrong units. */
static int geometry_read_game_frame(const char* path, const AeronConfigNode* node, XwaGeometryFrame* frame) {
	static const char* const design_keys[] = { "size", "offset", "bounds" };
	const AeronConfigNode* width = AeronConfigNode_MapGet(node, "classic_w");
	const AeronConfigNode* height = AeronConfigNode_MapGet(node, "classic_h");
	const AeronConfigNode* origin[2] = { AeronConfigNode_MapGet(node, "origin_x"),
										 AeronConfigNode_MapGet(node, "origin_y") };
	const int line = AeronConfigNode_Line(node);

	for (size_t i = 0; i < sizeof(design_keys) / sizeof(design_keys[0]); ++i) {
		const AeronConfigNode* value = AeronConfigNode_MapGet(node, design_keys[i]);
		if (value != NULL) {
			Aeron_LogError("xwa.frontend", "%s:%d: %s is in design pixels, but this atlas uses game units",
						   path, AeronConfigNode_Line(value), design_keys[i]);
			return 0;
		}
	}
	if (AeronConfigNode_Type(width) != AERON_CONFIG_INT || AeronConfigNode_Type(height) != AERON_CONFIG_INT ||
		AeronConfigNode_Int(width, 0) <= 0 || AeronConfigNode_Int(height, 0) <= 0) {
		Aeron_LogError("xwa.frontend", "%s:%d: a frame needs classic_w and classic_h, both above 0", path,
					   line);
		return 0;
	}
	frame->size[0] = (float)AeronConfigNode_Int(width, 0);
	frame->size[1] = (float)AeronConfigNode_Int(height, 0);
	for (int axis = 0; axis < 2; ++axis) {
		if (origin[axis] == NULL) {
			continue;
		}
		if (AeronConfigNode_Type(origin[axis]) != AERON_CONFIG_INT) {
			Aeron_LogError("xwa.frontend", "%s:%d: %s must be a whole number", path,
						   AeronConfigNode_Line(origin[axis]), axis == 0 ? "origin_x" : "origin_y");
			return 0;
		}
		frame->offset[axis] = (float)AeronConfigNode_Int(origin[axis], 0);
	}
	return 1;
}

/* Reads one frame. Other keys, such as the texture rectangle, are for the
 * renderer and are skipped here. */
static int geometry_read_frame(const char* path, const AeronConfigNode* node, float scale, int needs_id,
							   uint8_t units, XwaGeometryFrame* frame) {
	const AeronConfigNode* value;
	const char* text;
	float pixels[2];
	const int line = AeronConfigNode_Line(node);

	memset(frame, 0, sizeof(*frame));
	frame->id = -1;
	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.frontend", "%s:%d: a frame must be a map", path, line);
		return 0;
	}
	value = AeronConfigNode_MapGet(node, "id");
	if (value != NULL) {
		if (AeronConfigNode_Type(value) != AERON_CONFIG_INT) {
			Aeron_LogError("xwa.frontend", "%s:%d: id must be a number", path, AeronConfigNode_Line(value));
			return 0;
		}
		frame->id = (int)AeronConfigNode_Int(value, -1);
	} else if (needs_id) {
		Aeron_LogError("xwa.frontend", "%s:%d: a frame of a DAT group needs an id", path, line);
		return 0;
	}
	if (units == XWA_GEOMETRY_UNITS_GAME) {
		return geometry_read_game_frame(path, node, frame);
	}
	value = AeronConfigNode_MapGet(node, "size");
	if (value != NULL) {
		if (!geometry_read_numbers(value, frame->size, 2) || frame->size[0] <= 0.0f ||
			frame->size[1] <= 0.0f) {
			Aeron_LogError("xwa.frontend", "%s:%d: size must be [width, height]", path,
						   AeronConfigNode_Line(value));
			return 0;
		}
	} else {
		pixels[0] = (float)AeronConfigNode_Float(AeronConfigNode_MapGet(node, "w"), 0.0);
		pixels[1] = (float)AeronConfigNode_Float(AeronConfigNode_MapGet(node, "h"), 0.0);
		if (pixels[0] <= 0.0f || pixels[1] <= 0.0f) {
			Aeron_LogError("xwa.frontend", "%s:%d: a frame needs w and h, or a size", path, line);
			return 0;
		}
		frame->size[0] = pixels[0] / scale;
		frame->size[1] = pixels[1] / scale;
	}
	value = AeronConfigNode_MapGet(node, "offset");
	if (value != NULL && !geometry_read_numbers(value, frame->offset, 2)) {
		Aeron_LogError("xwa.frontend", "%s:%d: offset must be [x, y]", path, AeronConfigNode_Line(value));
		return 0;
	}
	value = AeronConfigNode_MapGet(node, "bounds");
	if (value != NULL) {
		if (!geometry_read_numbers(value, frame->bounds, 4)) {
			Aeron_LogError("xwa.frontend", "%s:%d: bounds must be [left, top, right, bottom]", path,
						   AeronConfigNode_Line(value));
			return 0;
		}
		frame->has_bounds = 1;
	}
	value = AeronConfigNode_MapGet(node, "fit");
	if (value != NULL) {
		text = AeronConfigNode_String(value, "");
		if (strcmp(text, "screen_height") == 0) {
			frame->fit = XWA_GEOMETRY_FIT_SCREEN_HEIGHT;
		} else {
			Aeron_LogError("xwa.frontend", "%s:%d: fit must be screen_height", path,
						   AeronConfigNode_Line(value));
			return 0;
		}
	}
	value = AeronConfigNode_MapGet(node, "keep_aspect");
	if (value != NULL) {
		text = AeronConfigNode_String(value, "");
		if (strcmp(text, "height") == 0) {
			frame->keep_aspect = XWA_GEOMETRY_KEEP_ASPECT_HEIGHT;
		} else if (strcmp(text, "width") == 0) {
			frame->keep_aspect = XWA_GEOMETRY_KEEP_ASPECT_WIDTH;
		} else {
			Aeron_LogError("xwa.frontend", "%s:%d: keep_aspect must be height or width", path,
						   AeronConfigNode_Line(value));
			return 0;
		}
	}
	return 1;
}

/* Reads an atlas. Returns NULL when the file is missing, has no
 * `game_geometry: true`, or has an error, which is logged. */
static XwaGeometryAtlas* geometry_read_atlas(const char* path, int needs_id) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* frames;
	const AeronConfigNode* value;
	XwaGeometryAtlas* atlas;
	float scale = 1.0f;
	int ok = 1;

	memset(&error, 0, sizeof(error));
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, path, &file, &error)) {
		if (error.code != AERON_CONFIG_ERROR_NOT_FOUND) {
			Aeron_LogError("xwa.frontend", "%s:%d: %s", path, error.line,
						   error.message[0] ? error.message : "cannot be read");
		}
		return NULL;
	}
	root = AeronConfigFile_Root(file);
	if (!AeronConfigNode_Bool(AeronConfigNode_MapGet(root, "game_geometry"), 0)) {
		AeronConfigFile_Destroy(file);
		return NULL;
	}
	frames = AeronConfigNode_MapGet(root, "frames");
	atlas = (XwaGeometryAtlas*)calloc(1, sizeof(*atlas));
	if (atlas == NULL || AeronConfigNode_Type(frames) != AERON_CONFIG_SEQUENCE ||
		AeronConfigNode_SequenceCount(frames) == 0) {
		Aeron_LogError("xwa.frontend", "%s: an atlas with game geometry needs frames", path);
		AeronConfigFile_Destroy(file);
		free(atlas);
		return NULL;
	}
	value = AeronConfigNode_MapGet(root, "units");
	if (value != NULL) {
		const char* units = AeronConfigNode_String(value, "");
		if (strcmp(units, "game") == 0) {
			atlas->units = XWA_GEOMETRY_UNITS_GAME;
		} else if (strcmp(units, "design") != 0) {
			Aeron_LogError("xwa.frontend", "%s:%d: units must be design or game", path,
						   AeronConfigNode_Line(value));
			ok = 0;
		}
	}
	value = AeronConfigNode_MapGet(root, "scale");
	if (value != NULL) {
		scale = (float)AeronConfigNode_Float(value, 0.0);
		if (atlas->units == XWA_GEOMETRY_UNITS_GAME) {
			Aeron_LogError("xwa.frontend",
						   "%s:%d: scale only applies to design pixels, but this atlas uses game units", path,
						   AeronConfigNode_Line(value));
			ok = 0;
		} else if (scale <= 0.0f) {
			Aeron_LogError("xwa.frontend", "%s:%d: scale must be a positive number", path,
						   AeronConfigNode_Line(value));
			ok = 0;
		}
	}
	atlas->frame_count = (int)AeronConfigNode_SequenceCount(frames);
	atlas->frames = (XwaGeometryFrame*)calloc((size_t)atlas->frame_count, sizeof(*atlas->frames));
	ok = ok && atlas->frames != NULL;
	for (int i = 0; ok && i < atlas->frame_count; ++i) {
		ok = geometry_read_frame(path, AeronConfigNode_SequenceGet(frames, (size_t)i), scale, needs_id,
								 atlas->units, &atlas->frames[i]);
	}
	atlas->classic_frames = atlas->frame_count;
	value = AeronConfigNode_MapGet(root, "classic_frames");
	if (ok && value != NULL) {
		atlas->classic_frames = (int)AeronConfigNode_Int(value, 0);
		if (AeronConfigNode_Type(value) != AERON_CONFIG_INT || atlas->classic_frames < 1 ||
			atlas->classic_frames > atlas->frame_count) {
			Aeron_LogError("xwa.frontend", "%s:%d: classic_frames must be between 1 and the frame count",
						   path, AeronConfigNode_Line(value));
			ok = 0;
		}
	}
	value = AeronConfigNode_MapGet(root, "fps");
	if (ok && value != NULL) {
		atlas->fps = (float)AeronConfigNode_Float(value, 0.0);
		if (atlas->fps <= 0.0f) {
			Aeron_LogError("xwa.frontend", "%s:%d: fps must be a positive number", path,
						   AeronConfigNode_Line(value));
			ok = 0;
		}
	}
	value = AeronConfigNode_MapGet(root, "tint");
	if (ok && value != NULL) {
		const char* tint = AeronConfigNode_String(value, "");
		if (strcmp(tint, "all") == 0) {
			atlas->tint = XWA_GEOMETRY_TINT_ALL;
		} else if (strcmp(tint, "none") == 0) {
			atlas->tint = XWA_GEOMETRY_TINT_NONE;
		} else if (strcmp(tint, "highlights") == 0) {
			atlas->tint = XWA_GEOMETRY_TINT_HIGHLIGHTS;
		} else {
			Aeron_LogError("xwa.frontend", "%s:%d: tint must be all, none or highlights", path,
						   AeronConfigNode_Line(value));
			ok = 0;
		}
	}
	value = AeronConfigNode_MapGet(root, "draw_adjust");
	if (ok && value != NULL) {
		atlas->has_draw_adjust = geometry_read_numbers(value, atlas->draw_adjust, 4);
		if (!atlas->has_draw_adjust) {
			Aeron_LogError("xwa.frontend", "%s:%d: draw_adjust must be [x, y, width, height]", path,
						   AeronConfigNode_Line(value));
			ok = 0;
		}
	}
	value = AeronConfigNode_MapGet(root, "bounds");
	if (ok && value != NULL) {
		atlas->has_bounds = geometry_read_numbers(value, atlas->bounds, 4);
		if (!atlas->has_bounds) {
			Aeron_LogError("xwa.frontend", "%s:%d: bounds must be [left, top, right, bottom]", path,
						   AeronConfigNode_Line(value));
			ok = 0;
		}
	}
	value = AeronConfigNode_MapGet(root, "regions");
	if (ok && value != NULL) {
		atlas->region_count = (int)AeronConfigNode_SequenceCount(value);
		atlas->regions = (XwaGeometryRegion*)calloc((size_t)atlas->region_count + 1, sizeof(*atlas->regions));
		ok = atlas->regions != NULL;
		for (int i = 0; ok && i < atlas->region_count; ++i) {
			const AeronConfigNode* region = AeronConfigNode_SequenceGet(value, (size_t)i);
			const AeronConfigNode* type = AeronConfigNode_MapGet(region, "type");
			atlas->regions[i].type = (int)AeronConfigNode_Int(type, 0);
			if (AeronConfigNode_Type(type) != AERON_CONFIG_INT ||
				!geometry_read_numbers(AeronConfigNode_MapGet(region, "rect"), atlas->regions[i].rect, 4)) {
				Aeron_LogError("xwa.frontend",
							   "%s:%d: a region needs a type and a rect [left, top, right, bottom]", path,
							   AeronConfigNode_Line(region));
				ok = 0;
			}
		}
	}
	AeronConfigFile_Destroy(file);
	if (!ok) {
		Aeron_LogError("xwa.frontend", "%s is ignored", path);
		geometry_free_atlas(atlas);
		return NULL;
	}
	return atlas;
}

/* Reads a video's YAML file. Returns NULL when the file is missing or has an
 * error, which is logged. */
static XwaGeometryAtlas* geometry_read_video(const char* path) {
	static const char* const keys[] = { "video", "size", "offset", "classic_frames", "fps", NULL };
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	const AeronConfigNode* root;
	const AeronConfigNode* value;
	const char* slash = strrchr(path, '/');
	XwaGeometryAtlas* atlas;
	XwaGeometryFrame frame;
	int ok = 1;

	memset(&error, 0, sizeof(error));
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, path, &file, &error)) {
		if (error.code != AERON_CONFIG_ERROR_NOT_FOUND) {
			Aeron_LogError("xwa.frontend", "%s:%d: %s", path, error.line,
						   error.message[0] ? error.message : "cannot be read");
		}
		return NULL;
	}
	root = AeronConfigFile_Root(file);
	atlas = (XwaGeometryAtlas*)calloc(1, sizeof(*atlas));
	memset(&frame, 0, sizeof(frame));
	frame.id = -1;
	if (atlas == NULL || AeronConfigNode_Type(root) != AERON_CONFIG_MAP) {
		Aeron_LogError("xwa.frontend", "%s: the file must be a map", path);
		ok = 0;
	}
	for (size_t i = 0; ok && i < AeronConfigNode_MapCount(root); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(root, i);
		int k = 0;
		while (keys[k] != NULL && strcmp(keys[k], key) != 0) {
			k++;
		}
		if (keys[k] == NULL) {
			Aeron_LogError("xwa.frontend", "%s:%d: unknown key '%s'", path,
						   AeronConfigNode_Line(AeronConfigNode_MapValueAt(root, i)), key);
			ok = 0;
		}
	}
	value = AeronConfigNode_MapGet(root, "video");
	if (ok && (AeronConfigNode_Type(value) != AERON_CONFIG_STRING || !AeronConfigNode_String(value, "")[0])) {
		Aeron_LogError("xwa.frontend", "%s: a video needs 'video', the name of its file", path);
		ok = 0;
	}
	if (ok) {
		snprintf(atlas->video, sizeof(atlas->video), "%.*s/%s", (int)(slash - path), path,
				 AeronConfigNode_String(value, ""));
	}
	value = AeronConfigNode_MapGet(root, "size");
	if (ok &&
		(!geometry_read_numbers(value, frame.size, 2) || frame.size[0] <= 0.0f || frame.size[1] <= 0.0f)) {
		Aeron_LogError("xwa.frontend", "%s: a video needs 'size', [width, height] larger than 0", path);
		ok = 0;
	}
	value = AeronConfigNode_MapGet(root, "offset");
	if (ok && value != NULL && !geometry_read_numbers(value, frame.offset, 2)) {
		Aeron_LogError("xwa.frontend", "%s:%d: offset must be [x, y]", path, AeronConfigNode_Line(value));
		ok = 0;
	}
	value = AeronConfigNode_MapGet(root, "classic_frames");
	if (ok) {
		atlas->classic_frames = 1;
	}
	if (ok && value != NULL) {
		atlas->classic_frames =
			AeronConfigNode_Type(value) == AERON_CONFIG_INT ? (int)AeronConfigNode_Int(value, 0) : 0;
		if (atlas->classic_frames < 1 || atlas->classic_frames > 4096) {
			Aeron_LogError("xwa.frontend", "%s:%d: classic_frames must be a number from 1 to 4096", path,
						   AeronConfigNode_Line(value));
			ok = 0;
		}
	}
	value = AeronConfigNode_MapGet(root, "fps");
	if (ok && value != NULL) {
		atlas->fps = (float)AeronConfigNode_Float(value, 0.0);
		if (atlas->fps <= 0.0f) {
			Aeron_LogError("xwa.frontend", "%s:%d: fps must be a positive number", path,
						   AeronConfigNode_Line(value));
			ok = 0;
		}
	}
	if (ok) {
		atlas->frame_count = atlas->classic_frames;
		atlas->frames = (XwaGeometryFrame*)calloc((size_t)atlas->frame_count, sizeof(*atlas->frames));
		ok = atlas->frames != NULL;
		for (int i = 0; ok && i < atlas->frame_count; ++i) {
			atlas->frames[i] = frame;
		}
	}
	AeronConfigFile_Destroy(file);
	if (!ok) {
		Aeron_LogError("xwa.frontend", "%s is ignored", path);
		geometry_free_atlas(atlas);
		return NULL;
	}
	return atlas;
}

/* The geometry for `key`: the video at `video_path` when there is one (not
 * for DAT groups), else the atlas at `path`. */
static const XwaGeometryAtlas* geometry_find(const char* key, const char* video_path, const char* path,
											 int needs_id) {
	XwaGeometryEntry* entry;

	for (int i = 0; i < g_geometryEntryCount; ++i) {
		if (strcmp(g_geometryEntries[i].key, key) == 0) {
			return g_geometryEntries[i].atlas;
		}
	}
	if (g_geometryEntryCount == g_geometryEntryCapacity) {
		const int capacity = g_geometryEntryCapacity ? g_geometryEntryCapacity * 2 : 256;
		XwaGeometryEntry* grown =
			(XwaGeometryEntry*)realloc(g_geometryEntries, (size_t)capacity * sizeof(*grown));
		if (grown == NULL) {
			return NULL;
		}
		g_geometryEntries = grown;
		g_geometryEntryCapacity = capacity;
	}
	geometry_load_design();
	entry = &g_geometryEntries[g_geometryEntryCount++];
	snprintf(entry->key, sizeof(entry->key), "%s", key);
	entry->atlas = video_path != NULL ? geometry_read_video(video_path) : NULL;
	if (entry->atlas != NULL) {
		Aeron_LogInfo("xwa.frontend", "%s: video for %s, %d frames", video_path, key,
					  entry->atlas->classic_frames);
		return entry->atlas;
	}
	entry->atlas = geometry_read_atlas(path, needs_id);
	if (entry->atlas != NULL) {
		Aeron_LogInfo("xwa.frontend", "%s: game geometry, %d frames", path, entry->atlas->classic_frames);
	}
	return entry->atlas;
}

const XwaGeometryAtlas* XwaFrontendGeometry_FindFile(const char* fileName) {
	char name[XWA_GEOMETRY_KEY_SIZE + 16];
	char path[160];
	char video_path[160];
	const char* key;
	const char* slash;
	char* dot;
	size_t length = 0;

	if (fileName == NULL) {
		return NULL;
	}
	/* The same key the snapshot records: lower case, forward slashes, no
	   extension and no leading "frontres/". */
	for (const char* c = fileName; *c != '\0' && length + 1 < sizeof(name); ++c) {
		char ch = *c;
		if (ch == '\\') {
			ch = '/';
		} else if (ch >= 'A' && ch <= 'Z') {
			ch = (char)(ch + ('a' - 'A'));
		}
		name[length++] = ch;
	}
	name[length] = '\0';
	dot = strrchr(name, '.');
	if (dot != NULL && strchr(dot, '/') == NULL) {
		*dot = '\0';
	}
	key = strncmp(name, "frontres/", 9) == 0 ? name + 9 : name;
	slash = strchr(key, '/');
	if (strlen(key) >= XWA_GEOMETRY_KEY_SIZE || slash == NULL || strchr(slash + 1, '/') != NULL) {
		return NULL;
	}
	snprintf(path, sizeof(path), "frontend/frontres/%.*s/atlas/%s.yaml", (int)(slash - key), key, slash + 1);
	snprintf(video_path, sizeof(video_path), "frontend/frontres/%.*s/video/%s.yaml", (int)(slash - key), key,
			 slash + 1);
	return geometry_find(key, video_path, path, 0);
}

const XwaGeometryAtlas* XwaFrontendGeometry_FindGroup(int group) {
	static int fontReported;
	char key[XWA_GEOMETRY_KEY_SIZE];
	char path[96];
	const XwaGeometryAtlas* atlas;
	const char* art;

	art = XwaMissionArt_GroupSource(group);
	if (art != NULL) {
		/* The mission's own art (mission_art.h). */
		snprintf(key, sizeof(key), "%s/group_%d", art, group);
		snprintf(path, sizeof(path), "resdata/mission_art/%s/group_%d.yaml", art, group);
	} else {
		snprintf(key, sizeof(key), "group_%d", group);
		snprintf(path, sizeof(path), "resdata/atlas/group_%d.yaml", group);
	}
	atlas = geometry_find(key, NULL, path, 1);
	/* The game copies the flight font's glyph pixels (flight_text.c), so the
	   font group keeps its DAT images. The HD fonts come from flight/fonts. */
	if (atlas != NULL && group == g_objectTypes[MODEL_TEXTURE_FONT_OBJECT_TYPE].textureGroup) {
		if (!fontReported) {
			Aeron_LogError("xwa.frontend",
						   "%s: the flight font group can't use an atlas, so the game "
						   "uses its DAT images",
						   path);
			fontReported = 1;
		}
		return NULL;
	}
	return atlas;
}

const XwaGeometryFrame* XwaFrontendGeometry_FindFrame(const XwaGeometryAtlas* atlas, int id) {
	if (atlas == NULL) {
		return NULL;
	}
	for (int i = 0; i < atlas->frame_count; ++i) {
		if (atlas->frames[i].id == id) {
			return &atlas->frames[i];
		}
	}
	return NULL;
}

const XwaGeometryRegion* XwaFrontendGeometry_FindRegion(const XwaGeometryAtlas* atlas, int type) {
	if (atlas == NULL) {
		return NULL;
	}
	for (int i = 0; i < atlas->region_count; ++i) {
		if (atlas->regions[i].type == type) {
			return &atlas->regions[i];
		}
	}
	return NULL;
}

int XwaFrontendGeometry_UnitX(float x) {
	geometry_load_design();
	return (int)floorf(x * 640.0f / g_geometryDesignW + 0.5f);
}

int XwaFrontendGeometry_UnitY(float y) {
	geometry_load_design();
	return (int)floorf(y * 480.0f / g_geometryDesignH + 0.5f);
}

float XwaFrontendGeometry_UnitsX(float x) {
	geometry_load_design();
	return x * 640.0f / g_geometryDesignW;
}

float XwaFrontendGeometry_UnitsY(float y) {
	geometry_load_design();
	return y * 480.0f / g_geometryDesignH;
}

void XwaFrontendGeometry_DesignSize(float* width, float* height) {
	geometry_load_design();
	*width = g_geometryDesignW;
	*height = g_geometryDesignH;
}

const XwaFrontendPaint* XwaFrontendGeometry_Paint(void) {
	geometry_load_design();
	return &g_geometryPaint;
}

static void geometry_unit_bounds(const float* bounds, int* left, int* top, int* right, int* bottom) {
	*left = XwaFrontendGeometry_UnitX(bounds[0]);
	*top = XwaFrontendGeometry_UnitY(bounds[1]);
	*right = XwaFrontendGeometry_UnitX(bounds[2]);
	*bottom = XwaFrontendGeometry_UnitY(bounds[3]);
}

ResourceDescriptor* XwaFrontendGeometry_BuildDescriptor(const XwaGeometryAtlas* atlas) {
	ResourceDescriptor* desc;
	int atlasRight = 0;
	int atlasBottom = 0;

	if (atlas == NULL) {
		return NULL;
	}
	if (g_geometryDescriptorCount == g_geometryDescriptorCapacity) {
		const int capacity = g_geometryDescriptorCapacity ? g_geometryDescriptorCapacity * 2 : 64;
		const ResourceDescriptor** grown = (const ResourceDescriptor**)realloc(
			(void*)g_geometryDescriptors, (size_t)capacity * sizeof(*grown));
		if (grown == NULL) {
			return NULL;
		}
		g_geometryDescriptors = grown;
		g_geometryDescriptorCapacity = capacity;
	}
	desc = (ResourceDescriptor*)Mem_Alloc(sizeof(*desc));
	if (desc == NULL) {
		return NULL;
	}
	memset(desc, 0, sizeof(*desc));
	desc->frameCount = atlas->classic_frames;
	desc->image = (ImageResource*)Mem_Alloc(sizeof(*desc->image) * (size_t)desc->frameCount);
	if (desc->image == NULL) {
		Mem_Free(desc);
		return NULL;
	}
	memset(desc->image, 0, sizeof(*desc->image) * (size_t)desc->frameCount);
	desc->bounds.left = 0x7fffffff;
	desc->bounds.top = 0x7fffffff;
	if (atlas->has_bounds) {
		atlasRight = XwaFrontendGeometry_UnitX(atlas->bounds[2]);
		atlasBottom = XwaFrontendGeometry_UnitY(atlas->bounds[3]);
	}
	for (int i = 0; i < desc->frameCount; ++i) {
		const XwaGeometryFrame* frame = &atlas->frames[i];
		ImageResource* image = &desc->image[i];
		int width = XwaFrontendGeometry_UnitX(frame->size[0]);
		int height = XwaFrontendGeometry_UnitY(frame->size[1]);

		image->width = width > 0 ? width : 1;
		image->height = height > 0 ? height : 1;
		/* Without `bounds`, a frame's bounds are its drawn rectangle, inclusive.
		   This is what FrontImage_ComputeImageBounds finds for an image with no
		   transparent border. */
		if (frame->has_bounds) {
			geometry_unit_bounds(frame->bounds, &image->boundsLeft, &image->boundsTop, &image->boundsRight,
								 &image->boundsBottom);
		} else {
			image->boundsRight = image->width - 1;
			image->boundsBottom = image->height - 1;
		}
		image->pixelCount = image->width * image->height;
		{
			/* Rectangle draws read the pixels inside the bounds, with the width
			   as the row stride. Some atlases give bounds that include the right
			   and bottom edges (24x24 for a 24x24 icon), which would read past the
			   last row. So the buffer covers both the frame's bounds and the
			   image's bounds. */
			int lastColumn = image->width - 1;
			int lastRow = image->height - 1;
			size_t size;

			lastColumn = image->boundsRight > lastColumn ? image->boundsRight : lastColumn;
			lastRow = image->boundsBottom > lastRow ? image->boundsBottom : lastRow;
			lastColumn = atlasRight > lastColumn ? atlasRight : lastColumn;
			lastRow = atlasBottom > lastRow ? atlasBottom : lastRow;
			size = (size_t)image->width * (size_t)lastRow + (size_t)lastColumn + 1;
			/* Every pixel is uncompressed index 0, so the classic draw code works
			   unchanged and code that reads the pixels sees an empty image. */
			image->pixels = (unsigned char*)Mem_Alloc(size);
			if (image->pixels == NULL) {
				for (int j = 0; j < i; ++j) {
					Mem_Free(desc->image[j].pixels);
				}
				Mem_Free(desc->image);
				Mem_Free(desc);
				return NULL;
			}
			memset(image->pixels, 0, size);
		}
		/* The image's bounds are the union of the frames' bounds, as in
		   FrontImage_RegisterFlicResource. */
		if (desc->bounds.left > image->boundsLeft) {
			desc->bounds.left = image->boundsLeft;
		}
		if (desc->bounds.top > image->boundsTop) {
			desc->bounds.top = image->boundsTop;
		}
		if (desc->bounds.right < image->boundsRight) {
			desc->bounds.right = image->boundsRight;
		}
		if (desc->bounds.bottom < image->boundsBottom) {
			desc->bounds.bottom = image->boundsBottom;
		}
	}
	if (atlas->has_bounds) {
		geometry_unit_bounds(atlas->bounds, &desc->bounds.left, &desc->bounds.top, &desc->bounds.right,
							 &desc->bounds.bottom);
	}
	g_geometryDescriptors[g_geometryDescriptorCount++] = desc;
	return desc;
}

int XwaFrontendGeometry_ResourceHasGeometry(const char* name) {
	const int index = FrontImage_FindResourceByName(name);
	const ResourceDescriptor* desc;

	if (index == -1) {
		return 0;
	}
	desc = g_resourceTable[index].desc;
	if (desc->atlasBaseIndex != 0) {
		return XwaFrontendGeometry_FindGroup(desc->atlasGroupId) != NULL;
	}
	return XwaFrontendGeometry_IsBuiltDescriptor(desc);
}

/* The DAT group layout of sprite_resource.c: an index of 8-byte entries
 * (a 16-bit id, then a 32-bit offset into the data), sorted by id, then the
 * sprites, each an 18-byte header and its payload. */
enum {
	GEOMETRY_INDEX_ENTRY_SIZE = 8,
	GEOMETRY_SPRITE_HEADER_SIZE = 18,
	GEOMETRY_SPRITE_TYPE = 25,
};

static const XwaGeometryAtlas* g_geometrySortAtlas;

/* Converts an atlas value to whole game units, rounded to the nearest unit. */
static int geometry_atlas_unit_x(const XwaGeometryAtlas* atlas, float x) {
	return atlas->units == XWA_GEOMETRY_UNITS_GAME ? (int)floorf(x + 0.5f) : XwaFrontendGeometry_UnitX(x);
}

static int geometry_atlas_unit_y(const XwaGeometryAtlas* atlas, float y) {
	return atlas->units == XWA_GEOMETRY_UNITS_GAME ? (int)floorf(y + 0.5f) : XwaFrontendGeometry_UnitY(y);
}

static int geometry_compare_ids(const void* lhs, const void* rhs) {
	const int a = g_geometrySortAtlas->frames[*(const int*)lhs].id;
	const int b = g_geometrySortAtlas->frames[*(const int*)rhs].id;
	return (a > b) - (a < b);
}

int XwaFrontendGeometry_BuildSpriteGroup(int16_t group_id, SpriteGroup* group,
										 const XwaGeometryAtlas* atlas) {
	int* order = (int*)malloc((size_t)atlas->frame_count * sizeof(*order));
	int32_t data_size = 0;
	uint32_t data_used = 0;
	int count = 0;

	if (order == NULL) {
		return 0;
	}
	for (int i = 0; i < atlas->frame_count; ++i) {
		const int height = geometry_atlas_unit_y(atlas, atlas->frames[i].size[1]);
		order[i] = i;
		data_size += GEOMETRY_SPRITE_HEADER_SIZE + (int32_t)sizeof(SpritePayload) + (height > 0 ? height : 1);
	}
	g_geometrySortAtlas = atlas;
	qsort(order, (size_t)atlas->frame_count, sizeof(*order), geometry_compare_ids);

	memset(group, 0, sizeof(*group));
	group->indexSize = GEOMETRY_INDEX_ENTRY_SIZE * atlas->frame_count;
	group->hGlobal = (unsigned char*)calloc(1, (size_t)group->indexSize + (size_t)data_size);
	if (group->hGlobal == NULL) {
		free(order);
		group->groupId = -1;
		Aeron_LogError("xwa.assets", "Failed to allocate sprite group %d (%d bytes)", group_id,
					   group->indexSize + data_size);
		return 0;
	}
	group->groupId = group_id;
	group->dataSize = group->indexSize + data_size;
	group->indexBase = group->hGlobal;
	group->dataBase = group->hGlobal + group->indexSize;
	for (int i = 0; i < atlas->frame_count; ++i) {
		const XwaGeometryFrame* frame = &atlas->frames[order[i]];
		const int width = geometry_atlas_unit_x(atlas, frame->size[0]);
		const int height = geometry_atlas_unit_y(atlas, frame->size[1]);
		const uint16_t rows = (uint16_t)(height > 0 ? height : 1);
		const uint32_t payload_size = (uint32_t)sizeof(SpritePayload) + rows;
		unsigned char* sprite = group->dataBase + data_used;
		unsigned char* payload = sprite + GEOMETRY_SPRITE_HEADER_SIZE;
		unsigned char* entry = group->indexBase + GEOMETRY_INDEX_ENTRY_SIZE * count;

		if (count > 0 && frame->id == atlas->frames[order[i - 1]].id) {
			continue; /* keep the first image with this id, as the DAT files do */
		}
		ByteOrder_WriteU16Le(sprite, GEOMETRY_SPRITE_TYPE);
		ByteOrder_WriteU16Le(sprite + 2, (uint16_t)(width > 0 ? width : 1));
		ByteOrder_WriteU16Le(sprite + 4, rows);
		ByteOrder_WriteU16Le(sprite + 10, (uint16_t)group_id);
		ByteOrder_WriteU16Le(sprite + 12, (uint16_t)frame->id);
		ByteOrder_WriteU32Le(sprite + 14, payload_size);
		ByteOrder_WriteU32Le(payload + offsetof(SpritePayload, payloadSize), payload_size);
		ByteOrder_WriteU32Le(payload + offsetof(SpritePayload, colorTable24Offset), sizeof(SpritePayload));
		ByteOrder_WriteU32Le(payload + offsetof(SpritePayload, rowDataOffset), sizeof(SpritePayload));
		ByteOrder_WriteU32Le(payload + offsetof(SpritePayload, palette16Offset), payload_size);
		ByteOrder_WriteU32Le(payload + offsetof(SpritePayload, anchorX),
							 (uint32_t)geometry_atlas_unit_x(atlas, frame->offset[0]));
		ByteOrder_WriteU32Le(payload + offsetof(SpritePayload, anchorY),
							 (uint32_t)geometry_atlas_unit_y(atlas, frame->offset[1]));
		/* Each row after the payload header is a run count of 0, already
		   zeroed by calloc. */
		ByteOrder_WriteU16Le(entry, (uint16_t)frame->id);
		ByteOrder_WriteU32Le(entry + 2, data_used);
		data_used += GEOMETRY_SPRITE_HEADER_SIZE + payload_size;
		count++;
	}
	free(order);
	group->spriteCount = (int16_t)count;
	group->lockState = 2;
	return 1;
}

int XwaFrontendGeometry_IsBuiltDescriptor(const ResourceDescriptor* desc) {
	for (int i = 0; desc != NULL && i < g_geometryDescriptorCount; ++i) {
		if (g_geometryDescriptors[i] == desc) {
			return 1;
		}
	}
	return 0;
}

void XwaFrontendGeometry_ForgetDescriptor(const ResourceDescriptor* desc) {
	for (int i = 0; desc != NULL && i < g_geometryDescriptorCount; ++i) {
		if (g_geometryDescriptors[i] == desc) {
			g_geometryDescriptors[i] = g_geometryDescriptors[--g_geometryDescriptorCount];
			return;
		}
	}
}

static int geometry_mask_open(int x, int y) {
	const int mx = (2 * x + 1) * g_geometryMaskW / 1280;
	const int my = (2 * y + 1) * g_geometryMaskH / 960;
	return g_geometryMask[((size_t)my * (size_t)g_geometryMaskW + (size_t)mx) * 4] >= 128;
}

int XwaFrontendGeometry_StarfieldOpen(int x, int y) {
	if (g_geometryMaskState == 0) {
		int open = 0;

		g_geometryMaskState = -1;
		g_geometryMask =
			Aeron_ImageReadPngRgba8(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, XWA_GEOMETRY_STARFIELD_MASK_PATH,
									&g_geometryMaskW, &g_geometryMaskH);
		if (g_geometryMask == NULL) {
			Aeron_LogError("xwa.frontend", "%s: missing or unreadable; the stars show everywhere",
						   XWA_GEOMETRY_STARFIELD_MASK_PATH);
			return -1;
		}
		g_geometryMaskState = 1;
		/* The starfield picks random points until it has 128 open ones, so it
		   would never finish if the mask had no open point in its rows. */
		for (int row = XWA_GEOMETRY_STARFIELD_TOP;
			 !open && row < XWA_GEOMETRY_STARFIELD_TOP + XWA_GEOMETRY_STARFIELD_ROWS; ++row) {
			for (int column = 0; !open && column < 640; ++column) {
				open = geometry_mask_open(column, row);
			}
		}
		if (!open) {
			Aeron_LogError("xwa.frontend", "%s: no white pixel in rows %d-%d; the stars show everywhere",
						   XWA_GEOMETRY_STARFIELD_MASK_PATH, XWA_GEOMETRY_STARFIELD_TOP,
						   XWA_GEOMETRY_STARFIELD_TOP + XWA_GEOMETRY_STARFIELD_ROWS - 1);
			g_geometryMaskState = -1;
		}
	}
	if (g_geometryMaskState < 0 || x < 0 || y < 0 || x >= 640 || y >= 480) {
		return -1;
	}
	return geometry_mask_open(x, y);
}

void XwaFrontendGeometry_Shutdown(void) {
	for (int i = 0; i < g_geometryEntryCount; ++i) {
		geometry_free_atlas(g_geometryEntries[i].atlas);
	}
	free(g_geometryEntries);
	g_geometryEntries = NULL;
	g_geometryEntryCount = 0;
	g_geometryEntryCapacity = 0;
	free((void*)g_geometryDescriptors);
	g_geometryDescriptors = NULL;
	g_geometryDescriptorCount = 0;
	g_geometryDescriptorCapacity = 0;
	free(g_geometryMask);
	g_geometryMask = NULL;
	g_geometryMaskState = 0;
	g_geometryDesignLoaded = 0;
	g_geometryDesignW = 640.0f;
	g_geometryDesignH = 480.0f;
	g_geometryPaint.fill_alpha = 0.5f;
	g_geometryPaint.pixel_lines = 0;
	g_geometryPaint.has_grid_colours = 0;
}
