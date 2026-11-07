/*
 * xwa_image_cook — builds an atlas of the game's 2D images from PNG files.
 *
 *     xwa_image_cook <folder> --mod <mod folder> [--quality fast|med|uber]
 *
 * The folder holds the PNG files and image.yaml, which names the image that
 * the atlas replaces and gives every key of the atlas file except the pixel
 * rectangles:
 *
 *     frontres: concourse/tourdoor   # a Frontres file, <dir>/<base>
 *     # group: 2000                  # or a DAT group
 *     units: design                  # or game; design by default
 *     game_geometry: true            # the game takes the geometry from the atlas
 *     classic_frames: 60             # the other keys are optional
 *     fps: 24
 *     tint: all
 *     draw_adjust: [0, 0, 0, 0]
 *     bounds: [0, 0, 388, 333]
 *     regions:
 *       - {type: 1, rect: [0, 0, 24, 24]}
 *     frames:
 *       - {image: door_00.png, size: [388, 333], offset: [0, 0]}
 *       - {image: door_01.png, size: [388, 333], offset: [0, 0], bounds: [0, 0, 387, 332]}
 *
 * A frame can also have `id` (required in a DAT group), `keep_aspect`,
 * `fit` and `light`. A frame without `size` gets the size of its PNG.
 *
 * The tool packs the frames into pages, and writes the pages as KTX2 files
 * with premultiplied alpha, next to the atlas file:
 *   <mod>/frontend/frontres/<dir>/atlas/<base>.yaml  for `frontres`
 *   <mod>/resdata/atlas/group_<g>.yaml               for `group`
 */

#include "bake_sprites.h"

#include "aeron/config_file.h"
#include "aeron/vfs.h"
#include "ldr_read.h"
#include "tool_log.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COOK_TAG "xwa_image_cook"
#define COOK_MAX_FRAMES 4096

static int usage(void) {
	fprintf(stderr, "usage: xwa_image_cook <folder> --mod <mod folder> [--quality fast|med|uber]\n"
					"\n"
					"Builds the atlas that image.yaml in <folder> describes from the PNG files\n"
					"there, and writes it into the mod folder.\n");
	return 2;
}

/* Logs an error about `node` of image.yaml and returns false. */
static bool report_error(const char* path, const AeronConfigNode* node, const char* message) {
	ToolLog_Write(TOOL_LOG_ERROR, "%s: %s:%d: %s", COOK_TAG, path, AeronConfigNode_Line(node), message);
	return false;
}

/* Checks that every key of `map` is in `keys`. */
static bool check_known_keys(const char* path, const AeronConfigNode* map, const char* const* keys,
							 size_t key_count) {
	for (size_t i = 0; i < AeronConfigNode_MapCount(map); ++i) {
		const char* key = AeronConfigNode_MapKeyAt(map, i);
		bool known = false;
		for (size_t k = 0; k < key_count && !known; ++k)
			known = strcmp(key, keys[k]) == 0;
		if (!known) {
			char message[300];
			snprintf(message, sizeof message, "%s isn't a key of this map", key);
			return report_error(path, AeronConfigNode_MapValueAt(map, i), message);
		}
	}
	return true;
}

static bool is_number(const AeronConfigNode* node) {
	const AeronConfigNodeType type = AeronConfigNode_Type(node);
	return type == AERON_CONFIG_INT || type == AERON_CONFIG_FLOAT;
}

/* Reads `key` of `map`, a list of `count` numbers, into `out`. Returns false
 * when the key is there but isn't such a list. */
static bool read_number_list(const char* path, const AeronConfigNode* map, const char* key, int count,
							 double* out, int* found) {
	const AeronConfigNode* list = AeronConfigNode_MapGet(map, key);
	if (found)
		*found = list != NULL;
	if (!list)
		return true;
	bool ok = AeronConfigNode_Type(list) == AERON_CONFIG_SEQUENCE &&
			  AeronConfigNode_SequenceCount(list) == (size_t)count;
	for (int i = 0; ok && i < count; ++i) {
		const AeronConfigNode* value = AeronConfigNode_SequenceGet(list, (size_t)i);
		ok = is_number(value);
		out[i] = AeronConfigNode_Float(value, 0.0);
	}
	if (!ok) {
		char message[160];
		snprintf(message, sizeof message, "%s must be a list of %d numbers", key, count);
		return report_error(path, list, message);
	}
	return true;
}

/* Reads `key` of `map`, a whole number from `min` to `max`. */
static bool read_whole_number(const char* path, const AeronConfigNode* map, const char* key, int min, int max,
							  int* out, int* found) {
	const AeronConfigNode* value = AeronConfigNode_MapGet(map, key);
	if (found)
		*found = value != NULL;
	if (!value)
		return true;
	const int64_t number = AeronConfigNode_Int(value, min - 1);
	if (AeronConfigNode_Type(value) != AERON_CONFIG_INT || number < min || number > max) {
		char message[160];
		snprintf(message, sizeof message, "%s must be a whole number from %d to %d", key, min, max);
		return report_error(path, value, message);
	}
	*out = (int)number;
	return true;
}

/* Reads `key` of `map`, one of `choices`, into `out`, which points into the
 * file. */
static bool read_choice(const char* path, const AeronConfigNode* map, const char* key,
						const char* const* choices, size_t choice_count, const char** out) {
	const AeronConfigNode* value = AeronConfigNode_MapGet(map, key);
	if (!value)
		return true;
	const char* text = AeronConfigNode_String(value, NULL);
	for (size_t i = 0; text && i < choice_count; ++i) {
		if (strcmp(text, choices[i]) == 0) {
			*out = text;
			return true;
		}
	}
	char message[200];
	snprintf(message, sizeof message, "%s must be %s%s%s", key, choices[0], choice_count > 1 ? " or " : "",
			 choice_count > 1 ? choices[choice_count - 1] : "");
	return report_error(path, value, message);
}

/* Whether `name` is a lower-case `<dir>/<base>` of letters, digits, '_' and
 * '-'. */
static bool is_frontres_name(const char* name) {
	int slashes = 0;
	if (!name[0] || name[0] == '/' || name[strlen(name) - 1] == '/')
		return false;
	for (const char* c = name; *c; ++c) {
		if (*c == '/')
			++slashes;
		else if (!islower((unsigned char)*c) && !isdigit((unsigned char)*c) && *c != '_' && *c != '-')
			return false;
	}
	return slashes == 1;
}

/* Reads one frame of image.yaml and loads its PNG from `folder`. */
static bool read_frame(const char* path, const char* folder, const AeronConfigNode* node, bool needs_id,
					   BakeFrame* frame) {
	static const char* const keys[] = { "image",  "id",          "size", "offset",
										"bounds", "keep_aspect", "fit",  "light" };
	static const char* const aspects[] = { "height", "width" };
	static const char* const fits[] = { "screen_height" };
	const AeronConfigNode* image = AeronConfigNode_MapGet(node, "image");
	const char* image_name = AeronConfigNode_String(image, NULL);
	int has_size = 0;
	double light[3] = { 0.0, 0.0, 0.0 };
	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP)
		return report_error(path, node, "a frame must be a map");
	if (!check_known_keys(path, node, keys, sizeof keys / sizeof keys[0]))
		return false;
	if (!image_name || !image_name[0] || strchr(image_name, '/') || strchr(image_name, '\\'))
		return report_error(path, image ? image : node,
							"a frame needs `image`, the name of a PNG file in the folder");
	if (!read_whole_number(path, node, "id", 0, 65535, &frame->id, &frame->has_id) ||
		!read_number_list(path, node, "size", 2, frame->size, &has_size) ||
		!read_number_list(path, node, "offset", 2, frame->offset, NULL) ||
		!read_number_list(path, node, "bounds", 4, frame->bounds, &frame->has_bounds) ||
		!read_choice(path, node, "keep_aspect", aspects, 2, &frame->keep_aspect) ||
		!read_choice(path, node, "fit", fits, 1, &frame->fit) ||
		!read_number_list(path, node, "light", 3, light, &frame->has_light))
		return false;
	if (needs_id && !frame->has_id)
		return report_error(path, node, "a frame of a DAT group needs an id");
	if (has_size && (frame->size[0] <= 0.0 || frame->size[1] <= 0.0))
		return report_error(path, AeronConfigNode_MapGet(node, "size"), "size must be above 0");
	for (int c = 0; c < 3; ++c)
		frame->light[c] = (float)light[c];

	char png[1300], reason[512];
	snprintf(png, sizeof png, "%s/%s", folder, image_name);
	if (!read_ldr_rgba(png, &frame->rgba, &frame->w, &frame->h, reason, sizeof reason)) {
		ToolLog_Write(TOOL_LOG_ERROR, "%s: %s", COOK_TAG, reason);
		return false;
	}
	if (!has_size) {
		frame->size[0] = frame->w;
		frame->size[1] = frame->h;
	}
	return true;
}

/* Reads `regions` into a new array. */
static bool read_regions(const char* path, const AeronConfigNode* list, BakeRegion** out, int* count) {
	static const char* const keys[] = { "type", "rect" };
	*out = NULL;
	*count = 0;
	if (!list)
		return true;
	const size_t n = AeronConfigNode_SequenceCount(list);
	if (AeronConfigNode_Type(list) != AERON_CONFIG_SEQUENCE || n == 0 || n > 255)
		return report_error(path, list, "regions must be a list of 1 to 255 regions");
	*out = (BakeRegion*)calloc(n, sizeof **out);
	if (!*out)
		return report_error(path, list, "out of memory");
	for (size_t i = 0; i < n; ++i) {
		const AeronConfigNode* region = AeronConfigNode_SequenceGet(list, i);
		int found = 0;
		if (AeronConfigNode_Type(region) != AERON_CONFIG_MAP)
			return report_error(path, region, "a region must be a map");
		if (!check_known_keys(path, region, keys, 2) ||
			!read_whole_number(path, region, "type", 1, 254, &(*out)[i].type, &found))
			return false;
		if (!found)
			return report_error(path, region, "a region needs a type");
		if (!read_number_list(path, region, "rect", 4, (*out)[i].rect, &found))
			return false;
		if (!found)
			return report_error(path, region, "a region needs a rect");
		++*count;
	}
	return true;
}

/* Cooks the atlas that `folder`/image.yaml describes into `mod`. */
static bool cook_folder(const char* folder, const char* mod, const BakeSpriteOptions* options) {
	static const char* const keys[] = { "frontres",       "group",   "units", "game_geometry",
										"classic_frames", "fps",     "tint",  "draw_adjust",
										"bounds",         "regions", "frames" };
	static const char* const units[] = { "design", "game" };
	static const char* const tints[] = { "all", "none", "highlights" };
	const char* path = "image.yaml";
	AeronVfs* vfs = AeronVfs_Create(NULL);
	AeronConfigFile* file = NULL;
	AeronConfigError error;
	memset(&error, 0, sizeof error);
	if (!vfs || !AeronVfs_SetRoot(vfs, AERON_VFS_ROOT_ASSET, folder) ||
		!AeronConfigFile_LoadYamlEx(vfs, AERON_VFS_ROOT_ASSET, path, &file, &error)) {
		ToolLog_Write(TOOL_LOG_ERROR, "%s: %s/%s:%d: %s", COOK_TAG, folder, path, error.line,
					  error.message[0] ? error.message : "can't be read");
		AeronVfs_Destroy(vfs);
		return false;
	}
	const AeronConfigNode* root = AeronConfigFile_Root(file);
	const AeronConfigNode* frontres = AeronConfigNode_MapGet(root, "frontres");
	const AeronConfigNode* frames = AeronConfigNode_MapGet(root, "frames");
	const char* unit_name = "design";
	BakeAtlasHeader header;
	BakeRegion* regions = NULL;
	BakeFrame* baked = NULL;
	int group = 0, has_group = 0, frame_count = 0;
	memset(&header, 0, sizeof header);
	bool ok =
		AeronConfigNode_Type(root) == AERON_CONFIG_MAP || report_error(path, root, "the file must be a map");
	ok = ok && check_known_keys(path, root, keys, sizeof keys / sizeof keys[0]) &&
		 read_whole_number(path, root, "group", 1, 65535, &group, &has_group) &&
		 read_choice(path, root, "units", units, 2, &unit_name) &&
		 read_whole_number(path, root, "classic_frames", 1, 65535, &header.classic_frames, NULL) &&
		 read_whole_number(path, root, "fps", 1, 1000, &header.fps, NULL) &&
		 read_choice(path, root, "tint", tints, 3, &header.tint) &&
		 read_number_list(path, root, "draw_adjust", 4, header.draw_adjust, &header.has_draw_adjust) &&
		 read_number_list(path, root, "bounds", 4, header.bounds, &header.has_bounds) &&
		 read_regions(path, AeronConfigNode_MapGet(root, "regions"), &regions, &header.region_count);
	if (ok && (frontres != NULL) == (has_group != 0))
		ok = report_error(path, root, "the file needs either `frontres` or `group`");
	if (ok && frontres && !is_frontres_name(AeronConfigNode_String(frontres, "")))
		ok = report_error(path, frontres, "frontres must be <dir>/<base>, in lower case");
	if (ok && AeronConfigNode_MapGet(root, "game_geometry")) {
		const AeronConfigNode* value = AeronConfigNode_MapGet(root, "game_geometry");
		if (AeronConfigNode_Type(value) != AERON_CONFIG_BOOL)
			ok = report_error(path, value, "game_geometry must be true or false");
		header.game_geometry = AeronConfigNode_Bool(value, 0);
	}
	if (ok) {
		frame_count = (int)AeronConfigNode_SequenceCount(frames);
		if (AeronConfigNode_Type(frames) != AERON_CONFIG_SEQUENCE || frame_count < 1 ||
			frame_count > COOK_MAX_FRAMES)
			ok = report_error(path, frames ? frames : root, "frames must be a list of 1 to 4096 frames");
	}
	header.game_units = strcmp(unit_name, "game") == 0;
	header.regions = regions;
	baked = ok ? (BakeFrame*)calloc((size_t)frame_count, sizeof *baked) : NULL;
	ok = ok && baked;
	for (int i = 0; ok && i < frame_count; ++i)
		ok = read_frame(path, folder, AeronConfigNode_SequenceGet(frames, (size_t)i), has_group != 0,
						&baked[i]);

	char dir[1300], base[300];
	if (ok && frontres) {
		const char* name = AeronConfigNode_String(frontres, "");
		const char* slash = strchr(name, '/');
		snprintf(dir, sizeof dir, "%s/frontend/frontres/%.*s/atlas", mod, (int)(slash - name), name);
		snprintf(base, sizeof base, "%s", slash + 1);
	} else if (ok) {
		snprintf(dir, sizeof dir, "%s/resdata/atlas", mod);
		snprintf(base, sizeof base, "group_%d", group);
	}
	ok = ok && BakeSprites_PackAndWriteAtlas(options, dir, base, &header, baked, frame_count);
	if (ok)
		ToolLog_Write(TOOL_LOG_INFO, "%s: %d frames -> %s/%s.yaml", COOK_TAG, frame_count, dir, base);
	for (int i = 0; baked && i < frame_count; ++i)
		free(baked[i].rgba);
	free(baked);
	free(regions);
	AeronConfigFile_Destroy(file);
	AeronVfs_Destroy(vfs);
	return ok;
}

int main(int argc, char** argv) {
	const char* folder = NULL;
	const char* mod = NULL;
	BakeSpriteOptions options;
	memset(&options, 0, sizeof options);
	options.write_ktx2 = true;
	options.zstd = true;
	options.bc7_quality = KTX2_BC7_QUALITY_MED;
	options.group_filter = -1;
	for (int i = 1; i < argc; ++i) {
		const char* arg = argv[i];
		if (strcmp(arg, "--mod") == 0 && i + 1 < argc) {
			mod = argv[++i];
		} else if (strcmp(arg, "--quality") == 0 && i + 1 < argc) {
			const char* quality = argv[++i];
			if (strcmp(quality, "fast") == 0)
				options.bc7_quality = KTX2_BC7_QUALITY_FAST;
			else if (strcmp(quality, "med") == 0)
				options.bc7_quality = KTX2_BC7_QUALITY_MED;
			else if (strcmp(quality, "uber") == 0)
				options.bc7_quality = KTX2_BC7_QUALITY_UBER;
			else
				return usage();
		} else if (arg[0] != '-' && !folder) {
			folder = arg;
		} else {
			return usage();
		}
	}
	if (!folder || !mod)
		return usage();
	options.out_root = mod;
	return cook_folder(folder, mod, &options) ? 0 : 1;
}
