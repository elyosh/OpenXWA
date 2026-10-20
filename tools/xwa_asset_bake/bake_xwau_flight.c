/*
 * xwa_asset_bake xwau-flight: converts XWAU's flight art for the mod folder.
 *
 * Each image group of the DAT files that XWAU's RESDATA.TXT lists becomes an
 * atlas with game geometry, so the whole game uses XWAU's frame counts, sizes
 * and anchors (frontend_geometry.h). Each image is packed at the resolution
 * XWAU ships it; nothing is upscaled. Each image's classic size is the size
 * XWAU draws it:
 *   - most images: their own size;
 *   - backdrop images: their size times their scale / 256, as XWAU's
 *     backdrop hook draws them. The scale comes from
 *     Resdata/BackdropScales.txt. A group that the file doesn't list gets the
 *     game's default backdrop scale, read from XWINGALLIANCE.EXE: 256 in the
 *     stock game, 128 in XWAU's executable;
 *   - hull icons (26000) and map icons (14800): their size times
 *     HullIconScale or MapIconScale / 256, as XWAU's hull icon hook draws them.
 *
 * Output:
 *   <out>/resdata/atlas/group_<g>.yaml        with group_<g>.ktx2, group_<g>_p<N>.ktx2
 *   <out>/resdata/atlas/group_<g>_alt<n>.yaml the alternative explosion sets of
 *   <out>/resdata/atlas/group_<g>_ds2.yaml    Materials/dat-<g>.mat, with their pages
 *   <out>/flight/fonts/font_tier_<n>.{ktx2,fnt}  XWAU's flight font
 *   <out>/flight/fonts/fonts.yaml             the TrueType font that XWAU's ddraw
 *                                             draws flight text with
 *
 * Left out: the frontend groups (15000-15999) and the craft pictures
 * (20001-20232), which xwau-frontend converts; the font group 16000, which
 * the game copies glyphs out of, so it gets font atlases instead; and the
 * groups below 1000 (the blast mark animations of BLASTMARKS.DAT), which
 * opt2gltf effects imports. PlanetCustom.dat and BACKDROP_SPECIAL.DAT are
 * left out too: missions add them for XWAU's own backdrops.
 *
 * The XWAU install is read with host file I/O, and file names are matched in
 * any case.
 */

#include "bake_xwau_flight.h"

#include "bake_flight_fonts.h"
#include "xwa_2d.h"
#include "xwau_config.h"
#include "xwau_dat.h"

#include "aeron/image.h"

#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define FLIGHT_FONT_GROUP 16000
#define FLIGHT_HULL_ICON_GROUP 26000
#define FLIGHT_MAP_ICON_GROUP 14800
/* XWAU's ddraw wrapper picks alternative explosions only for these groups
 * (Direct3DDevice.cpp: DATGroupId - 2000, MAX_XWA_EXPLOSIONS). */
#define FLIGHT_FIRST_EXPLOSION_GROUP 2000
#define FLIGHT_LAST_EXPLOSION_GROUP 2006
/* The mission in which XWAU's wrapper always uses ds2_frames
 * (DEATH_STAR_MISSION_INDEX, compared with the mission index at 0x9F5E74). */
#define FLIGHT_DEATH_STAR_MISSION_INDEX 52
/* XWAU's wrapper picks new explosion sets every 300th frame it presents:
 * 5 seconds at 60 fps. */
#define FLIGHT_REROLL_SECONDS 5
/* alt_frames_1 to alt_frames_4, then ds2_frames. */
#define FLIGHT_SET_COUNT 5
/* Addresses in XWINGALLIANCE.EXE. Mission_Init multiplies a backdrop's
 * mission scale by the float at FLIGHT_EXE_BACKDROP_SCALE. The backdrop table
 * has FLIGHT_BACKDROP_COUNT entries of 3 bytes: a model type (16 bits) and
 * flags (8 bits). The model type table has 24-byte entries, with the texture
 * group at byte 20. */
#define FLIGHT_EXE_IMAGE_BASE 0x400000u
#define FLIGHT_EXE_BACKDROP_SCALE 0x5A93ACu
#define FLIGHT_EXE_BACKDROP_TABLE 0x5B1140u
#define FLIGHT_EXE_MODEL_TYPES 0x5FB240u
#define FLIGHT_EXE_MODEL_TYPE_SIZE 24u
#define FLIGHT_EXE_MODEL_TYPE_GROUP 20u
#define FLIGHT_BACKDROP_COUNT 104
/* Backdrop flag 1: the backdrop picks its frame in the mission. */
#define FLIGHT_BACKDROP_PICKS_FRAME 1
/* The Death Star backdrop never picks a frame. Its beam copies its scale. */
#define FLIGHT_DEATH_STAR_GROUP 6250
#define FLIGHT_DEATH_STAR_BEAM_GROUP 6251
/* XWAU's backdrop hook adds backdrops 104 to 255, with groups 6304 to 6455
 * that pick their frame. */
#define FLIGHT_EXTRA_BACKDROP_GROUP 6304
#define FLIGHT_EXTRA_BACKDROP_COUNT 152

typedef struct FlightDat {
	char name[256];
	XwauDat* dat;
} FlightDat;

/* One BackdropScales.txt entry. `image` is the image's position in its
 * group, or -1 for the whole group. */
typedef struct FlightScale {
	int group;
	int image;
	int scale;
} FlightScale;

/* A group that missions use as a backdrop. */
typedef struct FlightBackdrop {
	int group;
	int picks_frame;
} FlightBackdrop;

/* A DAT material file: Materials/dat-<group>.mat or dat-<group>-<id>.mat. */
typedef struct FlightMaterialFile {
	int group;
	int image; /* the image id, or -1 for the whole group */
	char path[1100];
} FlightMaterialFile;

/* The keys of a DAT material that this bake uses, from its [Default]
 * section. */
typedef struct FlightMaterial {
	int has_light;
	float light[3];
	int blend_mode; /* ExplosionBlendMode; 0 when not set */
	int has_set[FLIGHT_SET_COUNT];
	char set_dat[FLIGHT_SET_COUNT][512]; /* the DAT path as written, such as ResData\ExplosionsAlt3.dat */
	int set_group[FLIGHT_SET_COUNT];
} FlightMaterial;

typedef struct FlightContext {
	const BakeXwauFlightOptions* opt;
	BakeXwauFlightStats* stats;
	char atlas_dir[1100];
	FlightDat* dats;
	int dat_count;
	FlightScale* scales;
	int scale_count;
	float backdrop_scale;      /* the default backdrop scale, 256 = 1.0 */
	FlightBackdrop* backdrops; /* empty when the executable couldn't be read */
	int backdrop_count;
	int hull_icon_scale;
	int map_icon_scale;
	FlightMaterialFile* materials;
	int material_count;
} FlightContext;

typedef struct FlightImage {
	int id;
	int dat; /* index in FlightContext.dats */
} FlightImage;

static const char* const g_flightSetKeys[FLIGHT_SET_COUNT] = {
	"alt_frames_1", "alt_frames_2", "alt_frames_3", "alt_frames_4", "ds2_frames",
};
static const char* const g_flightSetNames[FLIGHT_SET_COUNT] = { "alt1", "alt2", "alt3", "alt4", "ds2" };

/* ---- XWAU files --------------------------------------------------------- */

/* Opens a DAT once: a file listed twice keeps its first place. */
static void flight_open_dat(FlightContext* ctx, const char* rel) {
	char path[1100], error[256];
	if (!Xwau_FindFile(ctx->opt->xwau_root, rel, path, sizeof path)) {
		fprintf(stderr, "xwau: %s isn't in %s\n", rel, ctx->opt->xwau_root);
		return;
	}
	const char* name = strrchr(path, '/');
	name = name ? name + 1 : path;
	for (int i = 0; i < ctx->dat_count; ++i) {
		if (strcasecmp(ctx->dats[i].name, name) == 0)
			return;
	}
	XwauDat* dat = XwauDat_Open(path, error, sizeof error);
	if (!dat) {
		fprintf(stderr, "xwau: %s\n", error);
		ctx->stats->failures++;
		return;
	}
	FlightDat* grown = (FlightDat*)realloc(ctx->dats, (size_t)(ctx->dat_count + 1) * sizeof *grown);
	if (!grown) {
		XwauDat_Close(dat);
		ctx->stats->failures++;
		return;
	}
	ctx->dats = grown;
	FlightDat* file = &ctx->dats[ctx->dat_count++];
	snprintf(file->name, sizeof file->name, "%s", name);
	file->dat = dat;
}

/* Opens Planet2.dat, then the files of RESDATA.TXT in order. Every mission
 * lists Planet2.dat first in its [Resdata] section, so its images win, as
 * in XWAU. */
static int flight_open_dats(FlightContext* ctx) {
	char list_path[1100];
	if (!Xwau_FindFile(ctx->opt->xwau_root, "RESDATA.TXT", list_path, sizeof list_path)) {
		fprintf(stderr, "xwau: no RESDATA.TXT in %s\n", ctx->opt->xwau_root);
		return 0;
	}
	char planet2[1100];
	if (Xwau_FindFile(ctx->opt->xwau_root, "Resdata/Planet2.dat", planet2, sizeof planet2))
		flight_open_dat(ctx, "Resdata/Planet2.dat");
	FILE* fp = fopen(list_path, "r");
	if (!fp)
		return 0;
	char line[512];
	while (fgets(line, sizeof line, fp)) {
		char* end = line + strlen(line);
		while (end > line && isspace((unsigned char)end[-1]))
			*--end = '\0';
		if (line[0])
			flight_open_dat(ctx, line);
	}
	fclose(fp);
	return ctx->dat_count > 0;
}

/* Reads one of the hook_hull_icon settings: hook_hull_icon.cfg wins over the
 * [hook_hull_icon] section of Hooks.ini, as in the hook. */
static int flight_hull_icon_setting(const FlightContext* ctx, const char* key, int fallback) {
	char path[1100], value[64];
	XwauLines lines = { 0 };
	if (Xwau_FindFile(ctx->opt->xwau_root, "hook_hull_icon.cfg", path, sizeof path))
		XwauLines_Read(path, "", XWAU_RULES_CPP, &lines);
	else if (Xwau_FindFile(ctx->opt->xwau_root, "Hooks.ini", path, sizeof path))
		XwauLines_Read(path, "hook_hull_icon", XWAU_RULES_CPP, &lines);
	XwauLines_Value(&lines, key, XWAU_RULES_CPP, value, sizeof value);
	XwauLines_Free(&lines);
	long long number;
	return value[0] && Xwau_StoiAuto(value, &number) && number > 0 ? (int)number : fallback;
}

/* Reads Resdata/BackdropScales.txt as XWAU's backdrop hook does: each line is
 * `group, scale` or `group, image, scale`, and the first entry for a key
 * wins. Missions can add their own scales; no XWAU mission does, and this
 * bake only writes the shared atlases. */
static void flight_read_backdrop_scales(FlightContext* ctx) {
	char path[1100];
	if (!Xwau_FindFile(ctx->opt->xwau_root, "Resdata/BackdropScales.txt", path, sizeof path))
		return;
	XwauLines lines = { 0 };
	XwauLines_Read(path, "", XWAU_RULES_CPP, &lines);
	for (size_t i = 0; i < lines.count; ++i) {
		XwauLines tokens = { 0 };
		long long values[3] = { 0, -1, 0 };
		int ok = Xwau_Tokenize(lines.items[i], XWAU_RULES_CPP, &tokens) && tokens.count >= 2;
		if (ok && tokens.count == 2) {
			ok = Xwau_Stoi(tokens.items[0], &values[0]) && Xwau_Stoi(tokens.items[1], &values[2]);
		} else if (ok) {
			ok = Xwau_Stoi(tokens.items[0], &values[0]) && Xwau_Stoi(tokens.items[1], &values[1]) &&
				 Xwau_Stoi(tokens.items[2], &values[2]);
		}
		XwauLines_Free(&tokens);
		if (!ok) {
			fprintf(stderr, "xwau: %s: line '%s' isn't 'group, scale' or 'group, image, scale'\n", path,
					lines.items[i]);
			continue;
		}
		int known = 0;
		for (int k = 0; k < ctx->scale_count && !known; ++k)
			known = ctx->scales[k].group == (int)values[0] && ctx->scales[k].image == (int)values[1];
		if (known)
			continue;
		FlightScale* grown =
			(FlightScale*)realloc(ctx->scales, (size_t)(ctx->scale_count + 1) * sizeof *grown);
		if (!grown)
			break;
		ctx->scales = grown;
		ctx->scales[ctx->scale_count++] = (FlightScale) { (int)values[0], (int)values[1], (int)values[2] };
	}
	XwauLines_Free(&lines);
}

/* Returns the `size` bytes of the executable at address `va`, or NULL when
 * no section holds them. */
static const uint8_t* flight_exe_at(const uint8_t* exe, size_t exe_size, uint32_t va, uint32_t size) {
	if (exe_size < 0x40 || exe[0] != 'M' || exe[1] != 'Z')
		return NULL;
	const uint32_t pe = (uint32_t)exe[0x3C] | (uint32_t)exe[0x3D] << 8 | (uint32_t)exe[0x3E] << 16 |
						(uint32_t)exe[0x3F] << 24;
	if ((size_t)pe + 24 > exe_size || memcmp(exe + pe, "PE\0\0", 4) != 0)
		return NULL;
	const uint16_t section_count = (uint16_t)(exe[pe + 6] | exe[pe + 7] << 8);
	const uint16_t optional_size = (uint16_t)(exe[pe + 20] | exe[pe + 21] << 8);
	const uint8_t* section = exe + pe + 24 + optional_size;
	if (va < FLIGHT_EXE_IMAGE_BASE)
		return NULL;
	const uint32_t rva = va - FLIGHT_EXE_IMAGE_BASE;
	for (uint16_t i = 0; i < section_count; ++i, section += 40) {
		if ((size_t)(section + 40 - exe) > exe_size)
			return NULL;
		uint32_t fields[4]; /* virtual size, virtual address, raw size, raw offset */
		for (int f = 0; f < 4; ++f) {
			const uint8_t* p = section + 8 + 4 * f;
			fields[f] = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
		}
		if (rva >= fields[1] && rva - fields[1] + size <= fields[2]) {
			const size_t offset = (size_t)fields[3] + (rva - fields[1]);
			return offset + size <= exe_size ? exe + offset : NULL;
		}
	}
	return NULL;
}

static void flight_add_backdrop(FlightContext* ctx, int group, int picks_frame) {
	for (int i = 0; i < ctx->backdrop_count; ++i) {
		if (ctx->backdrops[i].group == group) {
			ctx->backdrops[i].picks_frame |= picks_frame;
			return;
		}
	}
	FlightBackdrop* grown =
		(FlightBackdrop*)realloc(ctx->backdrops, (size_t)(ctx->backdrop_count + 1) * sizeof *grown);
	if (!grown)
		return;
	ctx->backdrops = grown;
	ctx->backdrops[ctx->backdrop_count++] = (FlightBackdrop) { group, picks_frame };
}

/* Reads the default backdrop scale and the backdrop groups from the XWAU
 * executable. Without them, every group keeps the stock default of 256 and
 * the scales of BackdropScales.txt apply to any group. */
static void flight_read_exe(FlightContext* ctx) {
	ctx->backdrop_scale = 256.0f;
	char path[1100];
	if (!Xwau_FindFile(ctx->opt->xwau_root, "XWINGALLIANCE.EXE", path, sizeof path)) {
		fprintf(stderr, "xwau: no XWINGALLIANCE.EXE in %s. Backdrops get the stock scale of 256\n",
				ctx->opt->xwau_root);
		return;
	}
	FILE* fp = fopen(path, "rb");
	uint8_t* exe = NULL;
	long size = -1;
	if (fp && fseek(fp, 0, SEEK_END) == 0 && (size = ftell(fp)) > 0 && fseek(fp, 0, SEEK_SET) == 0 &&
		(exe = (uint8_t*)malloc((size_t)size)) != NULL && fread(exe, 1, (size_t)size, fp) != (size_t)size) {
		free(exe);
		exe = NULL;
	}
	if (fp)
		fclose(fp);
	const uint8_t* scale = exe ? flight_exe_at(exe, (size_t)size, FLIGHT_EXE_BACKDROP_SCALE, 4) : NULL;
	const uint8_t* table =
		exe ? flight_exe_at(exe, (size_t)size, FLIGHT_EXE_BACKDROP_TABLE, 3 * FLIGHT_BACKDROP_COUNT) : NULL;
	float value = 0.0f;
	if (scale)
		memcpy(&value, scale, sizeof value);
	if (!scale || !table || !(value > 0.0f && value <= 65536.0f)) {
		fprintf(stderr, "xwau: %s isn't the expected game executable. Backdrops get the stock scale of 256\n",
				path);
		free(exe);
		return;
	}
	ctx->backdrop_scale = value;
	for (int i = 0; i < FLIGHT_BACKDROP_COUNT; ++i) {
		const uint16_t type = (uint16_t)(table[3 * i] | table[3 * i + 1] << 8);
		const uint8_t flags = table[3 * i + 2];
		const uint8_t* entry =
			type != 0
				? flight_exe_at(exe, (size_t)size, FLIGHT_EXE_MODEL_TYPES + FLIGHT_EXE_MODEL_TYPE_SIZE * type,
								FLIGHT_EXE_MODEL_TYPE_SIZE)
				: NULL;
		if (!entry)
			continue;
		const int group =
			(int16_t)(entry[FLIGHT_EXE_MODEL_TYPE_GROUP] | entry[FLIGHT_EXE_MODEL_TYPE_GROUP + 1] << 8);
		if (group > 0)
			flight_add_backdrop(
				ctx, group, (flags & FLIGHT_BACKDROP_PICKS_FRAME) != 0 && group != FLIGHT_DEATH_STAR_GROUP);
	}
	for (int i = 0; i < FLIGHT_EXTRA_BACKDROP_COUNT; ++i)
		flight_add_backdrop(ctx, FLIGHT_EXTRA_BACKDROP_GROUP + i, 1);
	free(exe);
}

static const FlightBackdrop* flight_find_backdrop(const FlightContext* ctx, int group) {
	for (int i = 0; i < ctx->backdrop_count; ++i) {
		if (ctx->backdrops[i].group == group)
			return &ctx->backdrops[i];
	}
	return NULL;
}

/* Lists the DAT material files of Materials: dat-<group>.mat and
 * dat-<group>-<id>.mat. */
static void flight_list_materials(FlightContext* ctx) {
	char dir_path[1100];
	if (!Xwau_FindDir(ctx->opt->xwau_root, "Materials", dir_path, sizeof dir_path))
		return;
	DIR* dir = opendir(dir_path);
	if (!dir)
		return;
	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL) {
		const char* name = entry->d_name;
		const size_t length = strlen(name);
		if (length < 9 || strncasecmp(name, "dat-", 4) != 0 || strcasecmp(name + length - 4, ".mat") != 0)
			continue;
		char* end;
		const long group = strtol(name + 4, &end, 10);
		long image = -1;
		if (end == name + 4)
			continue;
		if (*end == '-') {
			char* image_end;
			image = strtol(end + 1, &image_end, 10);
			if (image_end == end + 1)
				continue;
			end = image_end;
		}
		if (strcasecmp(end, ".mat") != 0)
			continue;
		FlightMaterialFile* grown =
			(FlightMaterialFile*)realloc(ctx->materials, (size_t)(ctx->material_count + 1) * sizeof *grown);
		if (!grown)
			break;
		ctx->materials = grown;
		FlightMaterialFile* file = &ctx->materials[ctx->material_count++];
		file->group = (int)group;
		file->image = (int)image;
		snprintf(file->path, sizeof file->path, "%s/%s", dir_path, name);
	}
	closedir(dir);
}

/* Whether a section header line such as "[Default]" names the Default
 * section. Its names are split on ',' without trimming, as in XWAU. */
static int flight_is_default_section(const char* line) {
	char names[512];
	snprintf(names, sizeof names, "%s", line + 1);
	char* close = strchr(names, ']');
	if (close)
		*close = '\0';
	for (char* name = strtok(names, ","); name; name = strtok(NULL, ",")) {
		if (strcasecmp(name, "Default") == 0)
			return 1;
	}
	return 0;
}

/* Reads a DAT material with the rules of XWAU's ddraw wrapper
 * (ReadMaterialLine, LoadSimpleFrames and LoadLightColor in Materials.cpp):
 * a comment starts with ';' or '#' in column 0, a section starts with '[' in
 * column 0, and a key line is "key = value". Only the [Default] section is
 * read: a DAT image has no section of its own. */
static int flight_read_material(const char* path, FlightMaterial* out) {
	memset(out, 0, sizeof *out);
	FILE* fp = fopen(path, "r");
	if (!fp)
		return 0;
	char line[1024];
	int in_default = 0;
	while (fgets(line, sizeof line, fp)) {
		if (line[0] == ';' || line[0] == '#')
			continue;
		if (line[0] == '[') {
			in_default = flight_is_default_section(line);
			continue;
		}
		char param[128], svalue[512];
		const char* equals = strchr(line, '=');
		if (!in_default || !equals || sscanf(line, "%127s = %511s", param, svalue) < 2)
			continue;
		if (strcasecmp(param, "ExplosionBlendMode") == 0) {
			out->blend_mode = (int)atof(svalue);
		} else if (strcasecmp(param, "Light") == 0) {
			float rgb[3];
			if (sscanf(equals + 1, "%f, %f, %f", &rgb[0], &rgb[1], &rgb[2]) == 3) {
				out->has_light = 1;
				memcpy(out->light, rgb, sizeof rgb);
			} else {
				fprintf(stderr, "xwau: %s: Light needs three numbers\n", path);
			}
		} else {
			for (int set = 0; set < FLIGHT_SET_COUNT; ++set) {
				if (strcasecmp(param, g_flightSetKeys[set]) != 0)
					continue;
				/* The value is "<path>.dat-<group>", up to the first comma; the
				   rest of the line isn't used. */
				char value[512], written[512];
				snprintf(value, sizeof value, "%s", equals + 1);
				char* comma = strchr(value, ',');
				if (comma)
					*comma = '\0';
				char* split = NULL;
				if (sscanf(value, "%511s", written) == 1) {
					for (char* c = written; *c && !split; ++c) {
						if (strncasecmp(c, ".dat-", 5) == 0)
							split = c;
					}
				}
				char* group_end = NULL;
				const long group = split ? strtol(split + 5, &group_end, 10) : 0;
				if (!split || group_end == split + 5) {
					fprintf(stderr, "xwau: %s: %s must be <file>.dat-<group>\n", path, param);
					break;
				}
				split[4] = '\0';
				out->has_set[set] = 1;
				snprintf(out->set_dat[set], sizeof out->set_dat[set], "%s", written);
				out->set_group[set] = (int)group;
				break;
			}
		}
	}
	fclose(fp);
	return 1;
}

static const FlightMaterialFile* flight_find_material(const FlightContext* ctx, int group, int image) {
	for (int i = 0; i < ctx->material_count; ++i) {
		if (ctx->materials[i].group == group && ctx->materials[i].image == image)
			return &ctx->materials[i];
	}
	return NULL;
}

/* ---- sizes -------------------------------------------------------------- */

static int flight_is_baked_group(int group) {
	if (group >= 15000 && group <= 15999)
		return 0; /* frontend */
	if (group >= 20001 && group <= 20232)
		return 0; /* craft pictures, frontend */
	if (group == FLIGHT_FONT_GROUP)
		return 0;         /* font atlases instead */
	return group >= 1000; /* below: BLASTMARKS.DAT's animations, which opt2gltf effects imports */
}

static int flight_is_planet_group(int group) { return group >= 6000 && group <= 6999; }

/* The scale, where 256 means 1.0, at which XWAU draws the image at
 * `position` in `group`. */
static double flight_scale(const FlightContext* ctx, int group, int position) {
	if (group == FLIGHT_HULL_ICON_GROUP)
		return ctx->hull_icon_scale;
	if (group == FLIGHT_MAP_ICON_GROUP)
		return ctx->map_icon_scale;
	/* The beam copies the scale of the Death Star backdrop. */
	if (group == FLIGHT_DEATH_STAR_BEAM_GROUP)
		return flight_scale(ctx, FLIGHT_DEATH_STAR_GROUP, -1);
	if (ctx->backdrop_count > 0) {
		const FlightBackdrop* backdrop = flight_find_backdrop(ctx, group);
		if (!backdrop)
			return 256;
		/* The hook looks for the image `frame - 1`. A backdrop that doesn't
		   pick its frame has frame 0, so only the group's scale applies. */
		if (!backdrop->picks_frame)
			position = -1;
	}
	/* XWAU's backdrop hook looks for the image first, then the group. */
	for (int i = 0; i < ctx->scale_count; ++i) {
		if (ctx->scales[i].group == group && ctx->scales[i].image == position)
			return ctx->scales[i].scale;
	}
	for (int i = 0; i < ctx->scale_count; ++i) {
		if (ctx->scales[i].group == group && ctx->scales[i].image == -1)
			return ctx->scales[i].scale;
	}
	return ctx->backdrop_scale;
}

/* XWAU's tools store an offset that isn't whole as 8000 + 10 * value. */
static double flight_offset(int32_t value) { return value > 8000 ? (value - 8000) / 10.0 : (double)value; }

static int flight_round(double value) { return (int)floor(value + 0.5); }

/* ---- atlases ------------------------------------------------------------ */

static void flight_free_frames(BakeFrame* frames, int count) {
	for (int i = 0; i < count; ++i)
		free(frames[i].rgba);
	free(frames);
}

/* Packs `frames` and writes <atlas_dir>/<base>.yaml with its pages. */
static int flight_write_atlas(FlightContext* ctx, const char* base, const BakeFrame* frames,
							  const uint16_t* ids, int count, const char* header) {
	AeronCpuAtlas pages = { 0 };
	AeronAtlasImage* placed = NULL;
	if (!BakeSprites_BuildAtlas(frames, count, 0, &placed, &pages)) {
		fprintf(stderr, "xwau: %s could not be packed\n", base);
		return 0;
	}
	BakeSprites_MakeDirs(ctx->atlas_dir);
	int ok = 1;
	for (int page = 0; page < pages.page_count; ++page) {
		char path[1300];
		if (page == 0)
			snprintf(path, sizeof path, "%s/%s", ctx->atlas_dir, base);
		else
			snprintf(path, sizeof path, "%s/%s_p%d", ctx->atlas_dir, base, page);
		ok &= BakeSprites_EmitImage(ctx->opt->sprites, path, pages.pages[page].rgba, pages.pages[page].width,
									pages.pages[page].height);
	}
	char yaml[1300];
	snprintf(yaml, sizeof yaml, "%s/%s.yaml", ctx->atlas_dir, base);
	ok &= BakeSprites_WriteAtlasYaml(yaml, placed, frames, ids, count, &pages, pages.pages[0].width,
									 pages.pages[0].height, header);
	free(placed);
	Aeron_AtlasBuildFree(&pages);
	if (ok) {
		ctx->stats->atlases++;
		ctx->stats->images += count;
	}
	return ok;
}

/* The images of `group`: each id from the first file listed that has it,
 * sorted by id, as the game loads a group (SpriteResource_LoadGroup). */
static int flight_compare_images(const void* lhs, const void* rhs) {
	const int a = ((const FlightImage*)lhs)->id;
	const int b = ((const FlightImage*)rhs)->id;
	return (a > b) - (a < b);
}

static FlightImage* flight_group_images(const FlightContext* ctx, int group, int* out_count) {
	FlightImage* images = NULL;
	int count = 0;
	for (int f = 0; f < ctx->dat_count; ++f) {
		const int* ids;
		const size_t n = XwauDat_ListGroup(ctx->dats[f].dat, group, &ids);
		for (size_t i = 0; i < n; ++i) {
			int known = 0;
			for (int k = 0; k < count && !known; ++k)
				known = images[k].id == ids[i];
			if (known)
				continue;
			FlightImage* grown = (FlightImage*)realloc(images, (size_t)(count + 1) * sizeof *grown);
			if (!grown) {
				free(images);
				*out_count = 0;
				return NULL;
			}
			images = grown;
			images[count++] = (FlightImage) { ids[i], f };
		}
	}
	if (count > 1)
		qsort(images, (size_t)count, sizeof *images, flight_compare_images);
	*out_count = count;
	return images;
}

/* Halves `frame` until it is at most `max_width` wide. Its classic size
 * doesn't change. */
static int flight_shrink(BakeFrame* frame, int max_width) {
	while (max_width > 0 && frame->w > max_width) {
		int w = 0, h = 0;
		uint8_t* smaller = Aeron_ImageDownsampleStraightAlphaRgba8(frame->rgba, frame->w, frame->h, &w, &h);
		if (!smaller)
			return 0;
		free(frame->rgba);
		frame->rgba = smaller;
		frame->w = w;
		frame->h = h;
	}
	return 1;
}

/* Converts the alternative sets of an explosion group's material into
 * atlases, and writes the `frame_sets` key that lists them into `header`.
 * A set's images keep their order in the DAT file: XWAU finds the image for
 * a frame by its position. */
static void flight_bake_frame_sets(FlightContext* ctx, int group, char* header, size_t header_size) {
	const FlightMaterialFile* file = flight_find_material(ctx, group, -1);
	FlightMaterial material;
	if (!file || !flight_read_material(file->path, &material))
		return;
	int any = 0;
	for (int set = 0; set < FLIGHT_SET_COUNT; ++set)
		any |= material.has_set[set];
	if (!any)
		return;
	if (material.blend_mode != 0) {
		fprintf(stderr,
				"xwau: %s: ExplosionBlendMode %d uses XWAU's procedural explosions, which OpenXWA doesn't "
				"have; its sets aren't converted\n",
				file->path, material.blend_mode);
		return;
	}
	size_t used = strlen(header);
	used += (size_t)snprintf(header + used, header_size - used,
							 "frame_sets:\n  shuffle: explosions\n  reroll_seconds: %d\n  sets:\n",
							 FLIGHT_REROLL_SECONDS);
	for (int set = 0; set < FLIGHT_SET_COUNT; ++set) {
		if (!material.has_set[set])
			continue;
		char path[1100], error[256];
		XwauDat* dat = NULL;
		if (!Xwau_FindFile(ctx->opt->xwau_root, material.set_dat[set], path, sizeof path) ||
			!(dat = XwauDat_Open(path, error, sizeof error))) {
			fprintf(stderr, "xwau: %s: %s: %s can't be read\n", file->path, g_flightSetKeys[set],
					material.set_dat[set]);
			ctx->stats->failures++;
			continue;
		}
		const int* listed;
		const int count = (int)XwauDat_ListGroup(dat, material.set_group[set], &listed);
		BakeFrame* frames = (BakeFrame*)calloc(count > 0 ? (size_t)count : 1, sizeof *frames);
		uint16_t* frame_ids = (uint16_t*)calloc(count > 0 ? (size_t)count : 1, sizeof *frame_ids);
		int ok = count > 0 && frames && frame_ids;
		for (int i = 0; ok && i < count; ++i)
			frame_ids[i] = (uint16_t)listed[i];
		for (int i = 0; ok && i < count; ++i) {
			XwauDatImage image;
			ok = XwauDat_LoadImageFrom(dat, material.set_group[set], frame_ids[i], &image, error,
									   sizeof error);
			if (!ok) {
				fprintf(stderr, "xwau: %s: %s\n", path, error);
				break;
			}
			frames[i].rgba = image.rgba;
			frames[i].w = frames[i].classic_w = (int)image.width;
			frames[i].h = frames[i].classic_h = (int)image.height;
		}
		char base[64];
		snprintf(base, sizeof base, "group_%d_%s", group, g_flightSetNames[set]);
		if (ok && flight_write_atlas(ctx, base, frames, frame_ids, count, NULL)) {
			ctx->stats->frame_sets++;
			used += (size_t)snprintf(header + used, header_size - used, "    - {atlas: %s", base);
			if (set == FLIGHT_SET_COUNT - 1)
				used += (size_t)snprintf(header + used, header_size - used, ", missions: [%d]",
										 FLIGHT_DEATH_STAR_MISSION_INDEX);
			used += (size_t)snprintf(header + used, header_size - used, "}\n");
			printf("resdata group %d: %s set, %d images\n", group, g_flightSetNames[set], count);
		} else {
			fprintf(stderr, "xwau: group %d: the %s set failed\n", group, g_flightSetNames[set]);
			ctx->stats->failures++;
		}
		flight_free_frames(frames, frames && count > 0 ? count : 0);
		free(frame_ids);
		XwauDat_Close(dat);
	}
}

/* The `light` colour of an image: its own material, else its group's. */
static void flight_image_light(const FlightContext* ctx, int group, int id, BakeFrame* frame) {
	const FlightMaterialFile* file = flight_find_material(ctx, group, id);
	if (!file)
		file = flight_find_material(ctx, group, -1);
	FlightMaterial material;
	if (file && flight_read_material(file->path, &material) && material.has_light) {
		frame->has_light = 1;
		memcpy(frame->light, material.light, sizeof frame->light);
	}
}

static void flight_bake_group(FlightContext* ctx, int group) {
	int count = 0;
	FlightImage* images = flight_group_images(ctx, group, &count);
	if (count == 0) {
		free(images);
		return;
	}
	char header[4096];
	snprintf(header, sizeof header, "game_geometry: true\nunits: game\n");
	if (group >= FLIGHT_FIRST_EXPLOSION_GROUP && group <= FLIGHT_LAST_EXPLOSION_GROUP)
		flight_bake_frame_sets(ctx, group, header, sizeof header);

	BakeFrame* frames = (BakeFrame*)calloc((size_t)count, sizeof *frames);
	uint16_t* ids = (uint16_t*)calloc((size_t)count, sizeof *ids);
	int ok = frames && ids;
	int resized = 0;
	char error[256];
	for (int i = 0; ok && i < count; ++i) {
		const XwauDat* dat = ctx->dats[images[i].dat].dat;
		XwauDatImageInfo info;
		XwauDatImage image;
		ok = XwauDat_GetImageInfo(dat, group, images[i].id, &info) &&
			 XwauDat_LoadImageFrom(dat, group, images[i].id, &image, error, sizeof error);
		if (!ok) {
			fprintf(stderr, "xwau: %s: group %d image %d can't be decoded: %s\n",
					ctx->dats[images[i].dat].name, group, images[i].id, error);
			break;
		}
		BakeFrame* frame = &frames[i];
		const double scale = flight_scale(ctx, group, i) / 256.0;
		frame->rgba = image.rgba;
		frame->w = (int)image.width;
		frame->h = (int)image.height;
		frame->classic_w = flight_round(image.width * scale);
		frame->classic_h = flight_round(image.height * scale);
		if (frame->classic_w < 1)
			frame->classic_w = 1;
		if (frame->classic_h < 1)
			frame->classic_h = 1;
		frame->origin_x = flight_round(flight_offset(info.offset_x) * scale);
		frame->origin_y = flight_round(flight_offset(info.offset_y) * scale);
		ids[i] = (uint16_t)images[i].id;
		flight_image_light(ctx, group, images[i].id, frame);
		if (flight_is_planet_group(group) && ctx->opt->planet_max > 0 && frame->w > ctx->opt->planet_max) {
			ok = flight_shrink(frame, ctx->opt->planet_max);
			resized++;
		}
	}
	char base[64];
	snprintf(base, sizeof base, "group_%d", group);
	if (ok && flight_write_atlas(ctx, base, frames, ids, count, header)) {
		printf("resdata group %d: %d images%s\n", group, count, resized ? ", planets shrunk" : "");
	} else {
		fprintf(stderr, "xwau: group %d failed\n", group);
		ctx->stats->failures++;
	}
	flight_free_frames(frames, frames ? count : 0);
	free(ids);
	free(images);
}

/* XWAU's flight font: the font tiers that flight/fonts holds, built from
 * group 16000 as the stock flight-fonts bake does. */
static void flight_bake_fonts(FlightContext* ctx) {
	int count = 0;
	FlightImage* images = flight_group_images(ctx, FLIGHT_FONT_GROUP, &count);
	if (count == 0) {
		free(images);
		return;
	}
	Xwa2dFrameSet set = { 0 };
	set.frames = (Xwa2dFrame*)calloc((size_t)count, sizeof *set.frames);
	int ok = set.frames != NULL;
	char error[256];
	for (int i = 0; ok && i < count; ++i) {
		const XwauDat* dat = ctx->dats[images[i].dat].dat;
		XwauDatImageInfo info;
		XwauDatImage image;
		ok = XwauDat_GetImageInfo(dat, FLIGHT_FONT_GROUP, images[i].id, &info) &&
			 XwauDat_LoadImageFrom(dat, FLIGHT_FONT_GROUP, images[i].id, &image, error, sizeof error);
		if (!ok)
			break;
		Xwa2dFrame* frame = &set.frames[set.count++];
		frame->rgba = image.rgba;
		frame->width = (int)image.width;
		frame->height = (int)image.height;
		frame->frame_index = i;
		frame->sprite_id = images[i].id;
		frame->anchor_x = info.offset_x;
		frame->anchor_y = info.offset_y;
	}
	if (ok) {
		const BakeSpriteOptions* sprites = ctx->opt->sprites;
		/* The stock flight-fonts bake's 4x scale, which the HD HUD's font
		   atlases expect. */
		const BakeFlightFontsOptions font_options = {
			.out_root = sprites->out_root,
			.scale = 4,
			.zstd = sprites->zstd,
			.bc7_quality = sprites->bc7_quality,
		};
		ctx->stats->font_tiers = BakeFlightFonts_WriteTiers(&font_options, &set);
	}
	if (!ok || ctx->stats->font_tiers != 3) {
		fprintf(stderr, "xwau: the flight font failed%s%s\n", ok ? "" : ": ", ok ? "" : error);
		ctx->stats->failures++;
	}
	Xwa2dFrameSet_Free(&set);
	free(images);
}

/* The TrueType font files of the font families that XWAU's ddraw can name
 * in TextFontFamily. The ddraw asks Windows for the family at the normal
 * weight, so the file is the family's regular file. */
static const struct {
	const char* family;
	const char* file;
} g_flightFontFiles[] = {
	{ "Verdana", "verdana.ttf" },
};

/* Reads the text settings of DDraw.cfg as XWAU's ddraw does (config.cpp):
 * lines starting with '#', ';' or "//" are comments, the name is the text
 * before the first '=' without its spaces, and the last line with a name
 * wins. Settings that the file doesn't set keep the ddraw's defaults. */
static void flight_read_ddraw_text(const FlightContext* ctx, int* enabled, char* family, size_t family_size,
								   int* width_delta) {
	char path[1100], line[512];
	*enabled = 1;
	snprintf(family, family_size, "Verdana");
	*width_delta = 0;
	FILE* f = Xwau_FindFile(ctx->opt->xwau_root, "DDraw.cfg", path, sizeof path) ? fopen(path, "r") : NULL;
	while (f && fgets(line, sizeof line, f)) {
		char name[128];
		size_t length = 0;
		const char* equals = strchr(line, '=');
		if (line[0] == '#' || line[0] == ';' || (line[0] == '/' && line[1] == '/') || !equals)
			continue;
		for (const char* c = line; c < equals && length + 1 < sizeof name; ++c) {
			if (!isspace((unsigned char)*c))
				name[length++] = *c;
		}
		name[length] = '\0';
		const char* value = equals + 1;
		while (isspace((unsigned char)*value))
			++value;
		size_t value_length = strlen(value);
		while (value_length > 0 && isspace((unsigned char)value[value_length - 1]))
			--value_length;
		if (!length || !value_length)
			continue;
		if (strcmp(name, "Text2DRendererEnabled") == 0)
			*enabled = atoi(value) != 0;
		else if (strcmp(name, "TextFontFamily") == 0)
			snprintf(family, family_size, "%.*s", (int)value_length, value);
		else if (strcmp(name, "TextWidthDelta") == 0)
			*width_delta = atoi(value);
	}
	if (f)
		fclose(f);
}

/* Writes flight/fonts/fonts.yaml for the TrueType font that XWAU's ddraw
 * draws flight text with. The game lays out the text with the font's
 * widths, as with XWAU's ddraw, so the file sets `metrics: font`. */
static void flight_write_fonts_yaml(FlightContext* ctx) {
	int enabled, width_delta;
	char family[128], dir[1100], path[1200];
	const char* file = NULL;
	flight_read_ddraw_text(ctx, &enabled, family, sizeof family, &width_delta);
	if (!enabled)
		return;
	for (size_t i = 0; i < sizeof g_flightFontFiles / sizeof g_flightFontFiles[0]; ++i) {
		if (strcasecmp(family, g_flightFontFiles[i].family) == 0)
			file = g_flightFontFiles[i].file;
	}
	if (!file) {
		fprintf(stderr,
				"xwau: TextFontFamily = %s: the font's file isn't known, so flight/fonts/fonts.yaml isn't "
				"written\n",
				family);
		return;
	}
	if (width_delta != 0)
		fprintf(stderr,
				"xwau: TextWidthDelta = %d isn't converted. The flight text uses the font's own widths\n",
				width_delta);
	snprintf(dir, sizeof dir, "%s/flight/fonts", ctx->opt->sprites->out_root);
	BakeSprites_MakeDirs(dir);
	snprintf(path, sizeof path, "%s/fonts.yaml", dir);
	FILE* f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "xwau: %s can't be written\n", path);
		ctx->stats->failures++;
		return;
	}
	fprintf(f,
			"# Made by xwa_asset_bake xwau-flight from TextFontFamily in DDraw.cfg.\n"
			"version: 1\n"
			"default: {file: %s, metrics: font}\n",
			file);
	fclose(f);
	printf("xwau: copy %s from Windows' Fonts folder to flight/fonts/\n", file);
}

static int flight_compare_ints(const void* lhs, const void* rhs) {
	const int a = *(const int*)lhs;
	const int b = *(const int*)rhs;
	return (a > b) - (a < b);
}

int BakeXwau_RunFlight(const BakeXwauFlightOptions* opt, BakeXwauFlightStats* stats) {
	memset(stats, 0, sizeof *stats);
	FlightContext ctx;
	memset(&ctx, 0, sizeof ctx);
	ctx.opt = opt;
	ctx.stats = stats;
	snprintf(ctx.atlas_dir, sizeof ctx.atlas_dir, "%s/resdata/atlas", opt->sprites->out_root);
	if (!flight_open_dats(&ctx)) {
		stats->failures++;
		return 0;
	}
	ctx.hull_icon_scale = flight_hull_icon_setting(&ctx, "HullIconScale", 256);
	ctx.map_icon_scale = flight_hull_icon_setting(&ctx, "MapIconScale", 256);
	flight_read_backdrop_scales(&ctx);
	flight_read_exe(&ctx);
	flight_list_materials(&ctx);
	printf("xwau: %d DAT files, %d backdrop scales, default backdrop scale %g for %d backdrop groups, %d DAT "
		   "materials, hull icon scale %d, map icon scale %d\n",
		   ctx.dat_count, ctx.scale_count, (double)ctx.backdrop_scale, ctx.backdrop_count, ctx.material_count,
		   ctx.hull_icon_scale, ctx.map_icon_scale);

	int* groups = NULL;
	int group_count = 0;
	for (int f = 0; f < ctx.dat_count; ++f) {
		const int* list;
		const size_t n = XwauDat_ListGroups(ctx.dats[f].dat, &list);
		for (size_t g = 0; g < n; ++g) {
			int known = 0;
			for (int k = 0; k < group_count && !known; ++k)
				known = groups[k] == list[g];
			if (known || !flight_is_baked_group(list[g]))
				continue;
			if (opt->sprites->group_filter >= 0 && list[g] != opt->sprites->group_filter)
				continue;
			int* grown = (int*)realloc(groups, (size_t)(group_count + 1) * sizeof *grown);
			if (!grown)
				break;
			groups = grown;
			groups[group_count++] = list[g];
		}
	}
	if (group_count > 1)
		qsort(groups, (size_t)group_count, sizeof *groups, flight_compare_ints);
	for (int g = 0; g < group_count; ++g)
		flight_bake_group(&ctx, groups[g]);
	if (opt->sprites->group_filter < 0 || opt->sprites->group_filter == FLIGHT_FONT_GROUP) {
		flight_bake_fonts(&ctx);
		flight_write_fonts_yaml(&ctx);
	}

	free(groups);
	for (int f = 0; f < ctx.dat_count; ++f)
		XwauDat_Close(ctx.dats[f].dat);
	free(ctx.dats);
	free(ctx.scales);
	free(ctx.backdrops);
	free(ctx.materials);
	return stats->failures == 0;
}
