/*
 * xwa_asset_bake xwau-frontend: converts XWAU's frontend art for the mod
 * folder.
 *
 * XWAU replaces standard frontend files to change the frontend's layout, and
 * draws HD images in the rectangles they give. This tool gives each replaced
 * image an atlas with `game_geometry: true`. Its pixels come from XWAU's HD
 * image, or from XWAU's standard image, upscaled, when there is no HD image.
 * Its frame count, sizes, offsets and bounds come from XWAU's standard files.
 * Values are in design pixels of a 2560x1440 screen: one 640x480 unit is 4
 * pixels across and 3 down.
 *
 * Output:
 *   <out>/frontend/frontend.yaml                    design screen and paint look
 *   <out>/frontend/frontres/<dir>/atlas/<base>.yaml with <base>.ktx2, <base>_p<N>.ktx2
 *   <out>/resdata/atlas/group_<g>.yaml              with group_<g>.ktx2, group_<g>_p<N>.ktx2
 *   <out>/frontend/concourse/starfield_mask.png     where the concourse stars can show
 *   <out>/frontend/concourse/layout.yaml            room positions
 *   <out>/frontend/animations.yaml                  room animations; copy their videos
 *                                                   by hand from XWAU's Resdata
 *   <out>/frontend/frontres/<dir>/video/<base>.yaml videos that replace images; copy
 *                                                   these videos by hand too
 *   <out>/frontend/fonts/fonts.yaml                 the text font; copy the font file
 *                                                   by hand from Windows' Fonts folder
 *
 * Standard files that match stock and have no HD image are left to the stock
 * bake. The XWAU install is read with host file I/O, and file names are
 * matched in any case.
 */

#include "bake_xwau.h"

#include "png_write.h"
#include "upscale.h"
#include "xwa_2d.h"
#include "xwau_dat.h"

#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

/* Design pixels per 640x480 unit on the 2560x1440 design screen. */
#define DESIGN_X 4.0
#define DESIGN_Y 3.0

/* A standard image this large is a screen background: its HD image sets the
 * layout of the screens that draw it (XWAU's IsBackgroundWide). TECHGLOBE is
 * 575x464. */
#define BACKGROUND_MIN_W 560
#define BACKGROUND_MIN_H 440

#define CBM_DESCRIPTOR_BYTES 36
#define CBM_FRAME_HEADER_BYTES 2084

typedef struct XwauFrame {
	uint8_t* rgba; /* straight alpha */
	int w, h;
	int id; /* DAT image id; -1 in a Frontres atlas */
	double size[2];
	double offset[2];
	int has_bounds;
	double bounds[4]; /* left, top, right, bottom, as the game stores them */
	const char* keep_aspect;
	const char* fit;
} XwauFrame;

typedef struct XwauRegion {
	int type;
	double rect[4];
} XwauRegion;

typedef struct XwauAtlas {
	XwauFrame* frames;
	int count;
	int classic_frames; /* 0 when equal to count */
	int fps;
	const char* tint;
	int has_draw_adjust;
	double draw_adjust[4];
	int has_area;
	double area[4];
	const XwauRegion* regions;
	int region_count;
} XwauAtlas;

/* A background and the layout XWAU gives the screens that show it. Files
 * that aren't here have no HD image, so XWAU shows them in 4:3. */
typedef struct XwauBackground {
	char key[300];      /* "<dir>/<base>" or "group/<g>/<id>", in lower case */
	const char* layout; /* "wide", "fill" or "classic" */
} XwauBackground;

typedef struct XwauContext {
	const BakeXwauOptions* opt;
	BakeXwauStats* stats;
	char frontres[1024];
	char resdata[1024];
	int monitor_fps;
	int rating_rect[4];  /* familyroom_rating_left, top, width, height */
	XwauRegion* regions; /* map icon rectangles from SHIPLIST.TXT */
	int region_count;
	char** wide_keys; /* ConcourseBackgroundsWide.txt entries set to 0 */
	int wide_key_count;
	XwauBackground* backgrounds; /* the layout XWAU gives each background */
	int background_count;
	struct XwauVideo* videos; /* the images that XWAU replaces with a video */
	int video_count;
} XwauContext;

/* ---- host files ------------------------------------------------------- */

static uint8_t* read_host_file(const char* path, size_t* size) {
	FILE* fp = fopen(path, "rb");
	if (!fp)
		return NULL;
	uint8_t* bytes = NULL;
	if (fseek(fp, 0, SEEK_END) == 0) {
		const long length = ftell(fp);
		if (length >= 0 && fseek(fp, 0, SEEK_SET) == 0) {
			bytes = (uint8_t*)malloc(length ? (size_t)length : 1);
			if (bytes && fread(bytes, 1, (size_t)length, fp) != (size_t)length) {
				free(bytes);
				bytes = NULL;
			}
			*size = (size_t)length;
		}
	}
	fclose(fp);
	return bytes;
}

/* Finds `name` in `dir`, ignoring case, and writes its path to `out`. */
static int find_ci(const char* dir, const char* name, char* out, size_t out_size) {
	DIR* d = opendir(dir);
	if (!d)
		return 0;
	int found = 0;
	struct dirent* e;
	while (!found && (e = readdir(d)) != NULL) {
		if (strcasecmp(e->d_name, name) == 0) {
			snprintf(out, out_size, "%s/%s", dir, e->d_name);
			found = 1;
		}
	}
	closedir(d);
	return found;
}

static int is_directory(const char* path) {
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void lower(char* s) {
	for (; *s; ++s)
		*s = (char)tolower((unsigned char)*s);
}

static int same_files(const char* a, const char* b) {
	size_t size_a = 0, size_b = 0;
	uint8_t* bytes_a = read_host_file(a, &size_a);
	uint8_t* bytes_b = read_host_file(b, &size_b);
	const int same = bytes_a && bytes_b && size_a == size_b && memcmp(bytes_a, bytes_b, size_a) == 0;
	free(bytes_a);
	free(bytes_b);
	return same;
}

/* ---- XWAU text files ---------------------------------------------------- */

/* Reads `key = value` from a hook text file into `out`; `;` starts a
 * comment. With a `section`, only the keys in its `[section]` count. The last
 * value wins. Returns 0 when the key isn't set. */
static int read_key_string(const char* path, const char* section, const char* key, char* out, size_t size) {
	FILE* fp = fopen(path, "r");
	if (!fp)
		return 0;
	char line[512];
	int found = 0;
	int inside = section == NULL;
	while (fgets(line, sizeof line, fp)) {
		char* comment = strchr(line, ';');
		if (comment)
			*comment = '\0';
		char* name = line;
		while (isspace((unsigned char)*name))
			++name;
		if (section && *name == '[') {
			char* close = strchr(name, ']');
			if (close)
				*close = '\0';
			inside = strcasecmp(name + 1, section) == 0;
			continue;
		}
		char* eq = strchr(name, '=');
		if (!inside || !eq)
			continue;
		*eq = '\0';
		char* end = name + strlen(name);
		while (end > name && isspace((unsigned char)end[-1]))
			*--end = '\0';
		if (strcasecmp(name, key) != 0)
			continue;
		char* value = eq + 1;
		while (isspace((unsigned char)*value))
			++value;
		end = value + strlen(value);
		while (end > value && isspace((unsigned char)end[-1]))
			*--end = '\0';
		snprintf(out, size, "%s", value);
		found = 1;
	}
	fclose(fp);
	return found;
}

static int read_key_int(const char* path, const char* key, int fallback) {
	char value[512];
	return read_key_string(path, NULL, key, value, sizeof value) ? atoi(value) : fallback;
}

static int read_section_key_int(const char* path, const char* section, const char* key, int fallback) {
	char value[512];
	return read_key_string(path, section, key, value, sizeof value) ? (int)strtol(value, NULL, 0) : fallback;
}

/* Finds the hook_concourse settings: hook_concourse.cfg wins over the
 * [hook_concourse] section of Hooks.ini, as in XWAU. Sets *section to NULL
 * for hook_concourse.cfg. Returns 0, with an empty path, when there are
 * none. */
static int find_hook_settings(const char* root, char* path, size_t size, const char** section) {
	*section = NULL;
	if (find_ci(root, "hook_concourse.cfg", path, size))
		return 1;
	*section = "hook_concourse";
	if (find_ci(root, "Hooks.ini", path, size))
		return 1;
	path[0] = '\0';
	return 0;
}

/* Reads the map icon rectangle of each craft type from SHIPLIST.TXT: the last
 * four numbers of a line. Lines are numbered from 1, as in ShipList_Load
 * (src/xwa/assets/ship_list.c). Lines that start with `*` or have a category
 * the game doesn't know are skipped, but still counted. */
static void read_ship_regions(XwauContext* ctx, const char* xwau_root) {
	static const char* const categories[] = { "Fighter",
											  "Shuttle/Light Transport",
											  "Utility Craft",
											  "Container",
											  "Freighter/Heavy Transport",
											  "Starship",
											  "Station",
											  "Weapon emplacement",
											  "Mine",
											  "Satellite/Buoy",
											  "Droid" };
	char path[1100];
	if (!find_ci(xwau_root, "SHIPLIST.TXT", path, sizeof path))
		return;
	FILE* fp = fopen(path, "r");
	if (!fp)
		return;
	ctx->regions = (XwauRegion*)calloc(256, sizeof *ctx->regions);
	char line[1024];
	for (int type = 1; type < 255 && ctx->regions && fgets(line, sizeof line, fp); ++type) {
		char* tokens[32];
		int count = 0;
		for (char* t = strtok(line, ",\n\r"); t && count < 32; t = strtok(NULL, ",\n\r"))
			tokens[count++] = t;
		if (count < 9 || tokens[0][0] == '*')
			continue;
		int known = 0;
		for (size_t c = 0; c < sizeof categories / sizeof categories[0]; ++c)
			known |= strcmp(tokens[1], categories[c]) == 0;
		if (!known)
			continue;
		XwauRegion* r = &ctx->regions[ctx->region_count++];
		r->type = type;
		r->rect[0] = atoi(tokens[count - 4]) * DESIGN_X;
		r->rect[1] = atoi(tokens[count - 3]) * DESIGN_Y;
		r->rect[2] = atoi(tokens[count - 2]) * DESIGN_X;
		r->rect[3] = atoi(tokens[count - 1]) * DESIGN_Y;
	}
	fclose(fp);
}

/* Backgrounds that TFTC's ConcourseBackgroundsWide.txt sets to 0: they fill
 * the screen, but the room keeps a 4:3 layout. XU doesn't ship the file. */
static void read_wide_list(XwauContext* ctx, const char* xwau_root) {
	char path[1100];
	if (!find_ci(xwau_root, "ConcourseBackgroundsWide.txt", path, sizeof path))
		return;
	FILE* fp = fopen(path, "r");
	if (!fp)
		return;
	char line[512];
	while (fgets(line, sizeof line, fp)) {
		char* comment = strchr(line, ';');
		if (comment)
			*comment = '\0';
		char* eq = strchr(line, '=');
		if (!eq || atoi(eq + 1) != 0)
			continue;
		*eq = '\0';
		char* key = line;
		while (isspace((unsigned char)*key))
			++key;
		char* end = key + strlen(key);
		while (end > key && isspace((unsigned char)end[-1]))
			*--end = '\0';
		for (char* c = key; *c; ++c)
			if (*c == '\\')
				*c = '/';
		lower(key);
		char** keys = (char**)realloc(ctx->wide_keys, (size_t)(ctx->wide_key_count + 1) * sizeof *keys);
		if (!keys)
			break;
		ctx->wide_keys = keys;
		ctx->wide_keys[ctx->wide_key_count++] = strdup(key);
	}
	fclose(fp);
}

/* `key` is "frontres/<dir>/<file>" in lower case. */
static int listed_not_wide(const XwauContext* ctx, const char* key) {
	for (int i = 0; i < ctx->wide_key_count; ++i)
		if (strcmp(ctx->wide_keys[i], key) == 0)
			return 1;
	return 0;
}

static int is_screen_background(const char* key, int width, int height);

/* Records the layout XWAU gives a background. It is classic (4:3) when the
 * HD image has the same shape as the standard image. Otherwise it is fill
 * when ConcourseBackgroundsWide.txt sets it to 0 or it is DPMED or CONFIGB
 * (the hook always sets those to 0), and wide in every other case. */
static void note_background(XwauContext* ctx, const char* key, const char* list_key, int same_shape) {
	XwauBackground* grown = (XwauBackground*)realloc(ctx->backgrounds, (size_t)(ctx->background_count + 1) *
																		   sizeof *grown);
	if (!grown)
		return;
	ctx->backgrounds = grown;
	XwauBackground* b = &ctx->backgrounds[ctx->background_count++];
	snprintf(b->key, sizeof b->key, "%s", key);
	if (same_shape)
		b->layout = "classic";
	else if (strcmp(key, "config/dpmed") == 0 || strcmp(key, "config/configb") == 0 ||
			 listed_not_wide(ctx, list_key))
		b->layout = "fill";
	else
		b->layout = "wide";
}

static const char* background_layout(const XwauContext* ctx, const char* key) {
	for (int i = 0; i < ctx->background_count; ++i)
		if (strcmp(ctx->backgrounds[i].key, key) == 0)
			return ctx->backgrounds[i].layout;
	return "classic";
}

/* XWAU's tools store tenths of a unit as 8000 + 10 * value. */
static double xwau_value(int32_t v) { return v > 8000 ? (v - 8000) / 10.0 : (double)v; }

static int same_aspect(int w0, int h0, int w1, int h1) {
	return fabs((double)w0 / h0 - (double)w1 / h1) < 0.1;
}

/* ---- output -------------------------------------------------------------- */

static void write_number(FILE* fp, double v) {
	if (v == floor(v))
		fprintf(fp, "%.0f", v);
	else
		fprintf(fp, "%.2f", v);
}

static void write_list(FILE* fp, const double* values, int count) {
	fputc('[', fp);
	for (int i = 0; i < count; ++i) {
		if (i)
			fputs(", ", fp);
		write_number(fp, values[i]);
	}
	fputc(']', fp);
}

static int write_atlas_yaml(const char* path, const XwauAtlas* atlas, const AeronAtlasImage* placed,
							const AeronCpuAtlas* pages) {
	FILE* fp = fopen(path, "w");
	if (!fp)
		return 0;
	fprintf(fp, "# Made by xwa_asset_bake xwau-frontend. Sizes, offsets and bounds are in\n"
				"# design pixels; the game takes its geometry from this file.\n");
	fprintf(fp, "game_geometry: true\n");
	fprintf(fp, "atlas: {w: %d, h: %d}\n", pages->pages[0].width, pages->pages[0].height);
	if (atlas->classic_frames)
		fprintf(fp, "classic_frames: %d\n", atlas->classic_frames);
	if (atlas->fps)
		fprintf(fp, "fps: %d\n", atlas->fps);
	if (atlas->tint)
		fprintf(fp, "tint: %s\n", atlas->tint);
	if (atlas->has_draw_adjust) {
		fprintf(fp, "draw_adjust: ");
		write_list(fp, atlas->draw_adjust, 4);
		fputc('\n', fp);
	}
	if (atlas->has_area) {
		fprintf(fp, "bounds: ");
		write_list(fp, atlas->area, 4);
		fputc('\n', fp);
	}
	if (atlas->region_count) {
		fprintf(fp, "regions:\n");
		for (int i = 0; i < atlas->region_count; ++i) {
			fprintf(fp, "  - {type: %d, rect: ", atlas->regions[i].type);
			write_list(fp, atlas->regions[i].rect, 4);
			fprintf(fp, "}\n");
		}
	}
	fprintf(fp, "frames:\n");
	for (int i = 0; i < atlas->count; ++i) {
		const XwauFrame* f = &atlas->frames[i];
		fprintf(fp, "  - {");
		if (f->id >= 0)
			fprintf(fp, "id: %d, ", f->id);
		if (pages->page_count > 1)
			fprintf(fp, "page: %d, ", placed[i].page);
		fprintf(fp, "x: %d, y: %d, w: %d, h: %d, size: ", placed[i].x, placed[i].y, f->w, f->h);
		write_list(fp, f->size, 2);
		if (f->offset[0] != 0.0 || f->offset[1] != 0.0) {
			fprintf(fp, ", offset: ");
			write_list(fp, f->offset, 2);
		}
		if (f->has_bounds) {
			fprintf(fp, ", bounds: ");
			write_list(fp, f->bounds, 4);
		}
		if (f->keep_aspect)
			fprintf(fp, ", keep_aspect: %s", f->keep_aspect);
		if (f->fit)
			fprintf(fp, ", fit: %s", f->fit);
		fprintf(fp, "}\n");
	}
	return fclose(fp) == 0;
}

/* Writes `dir`/`base`.yaml and its pages: `base`, then `base`_p<N>. */
static void emit_atlas(XwauContext* ctx, const char* dir, const char* base, const XwauAtlas* atlas) {
	BakeFrame* frames = (BakeFrame*)calloc((size_t)atlas->count, sizeof *frames);
	if (!frames) {
		ctx->stats->failures++;
		return;
	}
	for (int i = 0; i < atlas->count; ++i) {
		frames[i].rgba = atlas->frames[i].rgba;
		frames[i].w = atlas->frames[i].w;
		frames[i].h = atlas->frames[i].h;
	}
	AeronCpuAtlas pages = { 0 };
	AeronAtlasImage* placed = NULL;
	if (!BakeSprites_BuildAtlas(frames, atlas->count, 0, &placed, &pages)) {
		fprintf(stderr, "xwau: %s/%s could not be packed\n", dir, base);
		free(frames);
		ctx->stats->failures++;
		return;
	}
	BakeSprites_MakeDirs(dir);
	int ok = 1;
	for (int page = 0; page < pages.page_count; ++page) {
		char path[1200];
		if (page == 0)
			snprintf(path, sizeof path, "%s/%s", dir, base);
		else
			snprintf(path, sizeof path, "%s/%s_p%d", dir, base, page);
		ok &= BakeSprites_EmitImage(ctx->opt->sprites, path, pages.pages[page].rgba, pages.pages[page].width,
									pages.pages[page].height);
	}
	char yaml[1200];
	snprintf(yaml, sizeof yaml, "%s/%s.yaml", dir, base);
	ok &= write_atlas_yaml(yaml, atlas, placed, &pages);
	free(placed);
	Aeron_AtlasBuildFree(&pages);
	free(frames);
	if (ok)
		ctx->stats->atlases++;
	else
		ctx->stats->failures++;
}

static void free_atlas_frames(XwauAtlas* atlas) {
	for (int i = 0; i < atlas->count; ++i)
		free(atlas->frames[i].rgba);
	free(atlas->frames);
	atlas->frames = NULL;
	atlas->count = 0;
}

/* Upscales a standard image the same way as the stock bake. Takes ownership of `rgba`. */
static int upscale_standard(XwauContext* ctx, uint8_t** rgba, int* w, int* h) {
	if (!atlas_svga_to_4k(rgba, w, h)) {
		ctx->stats->failures++;
		return 0;
	}
	ctx->stats->sd_frames++;
	return 1;
}

/* ---- Frontres ------------------------------------------------------------ */

typedef struct CbmGeometry {
	int count;
	int area[4];
	int* bounds; /* 4 per frame */
} CbmGeometry;

static int read_cbm_geometry(const uint8_t* bytes, size_t size, CbmGeometry* out) {
	memset(out, 0, sizeof *out);
	if (size < CBM_DESCRIPTOR_BYTES)
		return 0;
	int32_t v[4];
	memcpy(&out->count, bytes, 4);
	if (out->count <= 0 || out->count > 4096)
		return 0;
	memcpy(v, bytes + 8, 16);
	memcpy(out->area, v, sizeof v);
	out->bounds = (int*)calloc((size_t)out->count * 4, sizeof *out->bounds);
	if (!out->bounds)
		return 0;
	size_t pos = CBM_DESCRIPTOR_BYTES;
	for (int i = 0; i < out->count; ++i) {
		int32_t header[8];
		if (pos + CBM_FRAME_HEADER_BYTES > size)
			return 0;
		memcpy(header, bytes + pos, sizeof header);
		memcpy(&out->bounds[i * 4], &header[4], 4 * sizeof(int32_t));
		pos += CBM_FRAME_HEADER_BYTES + (size_t)(uint32_t)header[3];
	}
	return 1;
}

/* Computes the bounds of a BMP or FLC frame like FrontImage_ComputeImageBounds
 * (src/xwa/frontend/frontend_image.c), with the same scan order and early
 * exits. Texels with alpha 0 stand for the game's palette index 0. */
static void compute_bounds_like_game(const uint8_t* rgba, int width, int height, int* out) {
	int scan_y = height - 1, right = 0, bottom = 0;
	if (scan_y > 0) {
		const int max_right = width - 1;
		do {
			const uint8_t* row = rgba + (size_t)scan_y * width * 4;
			for (int col = max_right; col > right; --col) {
				if (row[col * 4 + 3] != 0) {
					if (scan_y > bottom)
						bottom = scan_y;
					right = col;
					break;
				}
			}
			if (right >= max_right)
				break;
			--scan_y;
		} while (scan_y > 0);
	}
	out[3] = bottom++;
	out[2] = right;
	int top = bottom, left = right + 1;
	for (int row = 0; row < bottom; ++row) {
		const uint8_t* pixels = rgba + (size_t)row * width * 4;
		for (int col = 0; col < left; ++col) {
			if (pixels[col * 4 + 3] != 0) {
				if (row < top)
					top = row;
				left = col;
				break;
			}
		}
		if (left == 0)
			break;
	}
	out[1] = top;
	out[0] = left;
}

/* Loads XWAU's HD images for a Frontres file: the images of <base>_hd.dat,
 * sorted by group and then by image id, or the frames of <base>_hd.cbm.
 * Returns 1 when found, 0 when there are none, and -1 on error. */
static int load_frontres_hd(const char* dir, const char* base, Xwa2dFrameSet* out, XwauDat** out_dat) {
	char name[300], path[1400], error[256];
	memset(out, 0, sizeof *out);
	*out_dat = NULL;
	snprintf(name, sizeof name, "%s_hd.dat", base);
	if (find_ci(dir, name, path, sizeof path)) {
		XwauDat* dat = XwauDat_Open(path, error, sizeof error);
		if (!dat) {
			fprintf(stderr, "xwau: %s\n", error);
			return -1;
		}
		const int* group_ids;
		const int group_count = (int)XwauDat_ListGroups(dat, &group_ids);
		int groups[64];
		int gn = 0;
		for (int g = 0; g < group_count && gn < 64; ++g)
			groups[gn++] = group_ids[g];
		for (int a = 0; a < gn; ++a)
			for (int b = a + 1; b < gn; ++b)
				if (groups[b] < groups[a]) {
					const int t = groups[a];
					groups[a] = groups[b];
					groups[b] = t;
				}
		for (int g = 0; g < gn; ++g) {
			const int* ids;
			const int n = (int)XwauDat_ListGroup(dat, groups[g], &ids);
			int* sorted = (int*)malloc((size_t)(n ? n : 1) * sizeof *sorted);
			memcpy(sorted, ids, (size_t)n * sizeof *sorted);
			for (int a = 0; a < n; ++a)
				for (int b = a + 1; b < n; ++b)
					if (sorted[b] < sorted[a]) {
						const int t = sorted[a];
						sorted[a] = sorted[b];
						sorted[b] = t;
					}
			for (int i = 0; i < n; ++i) {
				XwauDatImage image;
				if (!XwauDat_LoadImageFrom(dat, groups[g], sorted[i], &image, error, sizeof error)) {
					fprintf(stderr, "xwau: %s\n", error);
					free(sorted);
					Xwa2dFrameSet_Free(out);
					XwauDat_Close(dat);
					return -1;
				}
				Xwa2dFrame* frames =
					(Xwa2dFrame*)realloc(out->frames, (size_t)(out->count + 1) * sizeof *frames);
				out->frames = frames;
				Xwa2dFrame* f = &out->frames[out->count++];
				memset(f, 0, sizeof *f);
				f->rgba = image.rgba;
				f->width = (int)image.width;
				f->height = (int)image.height;
				f->sprite_id = sorted[i];
				f->anchor_x = groups[g]; /* the group, used later to look up the image's offset */
			}
			free(sorted);
		}
		*out_dat = dat;
		return out->count > 0;
	}
	snprintf(name, sizeof name, "%s_hd.cbm", base);
	if (find_ci(dir, name, path, sizeof path)) {
		size_t size = 0;
		uint8_t* bytes = read_host_file(path, &size);
		/* Palette index 0 is transparent. XWAU does this when that colour is
		 * black (NET/ResItemImage.cs), and EYE_HD, XU's only _hd.cbm, has it black. */
		const int ok = bytes && Xwa2d_DecodeCbm(bytes, size, out, error, sizeof error);
		free(bytes);
		if (!ok) {
			fprintf(stderr, "xwau: %s: %s\n", path, error);
			return -1;
		}
		return 1;
	}
	return 0;
}

/* ---- videos in place of images -------------------------------------------- */

/* An image that XWAU replaces with a video from Resdata (HC/concourse.cpp, the
 * TechDoor, DSBrief and Squadlogo hooks). */
typedef struct XwauVideo {
	char dir[32], base[32]; /* the image, in lower case */
	char webm[64];          /* the video's name in Resdata */
	double size[2];         /* design pixels; 0 means the standard image's size */
	double offset[2];
} XwauVideo;

static double door_value(const char* doors, const char* key, int fallback);

static const XwauVideo* find_video(const XwauContext* ctx, const char* dir, const char* base) {
	for (int i = 0; i < ctx->video_count; ++i) {
		if (strcmp(ctx->videos[i].dir, dir) == 0 && strcmp(ctx->videos[i].base, base) == 0)
			return &ctx->videos[i];
	}
	return NULL;
}

static void add_video(XwauContext* ctx, const char* dir, const char* base, const char* webm, double w,
					  double h, double x, double y) {
	XwauVideo* grown = (XwauVideo*)realloc(ctx->videos, (size_t)(ctx->video_count + 1) * sizeof *grown);
	if (!grown)
		return;
	ctx->videos = grown;
	XwauVideo* v = &ctx->videos[ctx->video_count++];
	snprintf(v->dir, sizeof v->dir, "%s", dir);
	snprintf(v->base, sizeof v->base, "%s", base);
	snprintf(v->webm, sizeof v->webm, "%s", webm);
	v->size[0] = w;
	v->size[1] = h;
	v->offset[0] = x;
	v->offset[1] = y;
}

/* Lists the images XWAU replaces with videos, and the size and offset of each video. */
static void find_videos(XwauContext* ctx) {
	char path[1200], doors[1100] = "";
	if (!find_ci(ctx->opt->xwau_root, "ConcourseDoors.txt", doors, sizeof doors))
		doors[0] = '\0';
	if (find_ci(ctx->resdata, "techdoor.webm", path, sizeof path)) {
		/* Drawn at the door's position, at techdoor_posWidth x techdoor_posHeight. */
		add_video(ctx, "concourse", "techdoor", strrchr(path, '/') + 1,
				  door_value(doors, "techdoor_posWidth", 92) * DESIGN_X,
				  door_value(doors, "techdoor_posHeight", 93) * DESIGN_Y, 0, 0);
	}
	if (find_ci(ctx->resdata, "dsbrief.webm", path, sizeof path)) {
		/* 272x270 units, 5 units right and 3 down from the image's position.
		   The campaign image and the other image use the same video. */
		add_video(ctx, "dsbrief", "dsbrief", strrchr(path, '/') + 1, 272 * DESIGN_X, 270 * DESIGN_Y,
				  5 * DESIGN_X, 3 * DESIGN_Y);
		add_video(ctx, "dsbrief", "dsbriefc", strrchr(path, '/') + 1, 272 * DESIGN_X, 270 * DESIGN_Y,
				  5 * DESIGN_X, 3 * DESIGN_Y);
	}
	for (int n = 1; n <= 99; ++n) {
		char name[32], txt[1200], base[32];
		snprintf(name, sizeof name, "Squad%d.webm", n);
		if (!find_ci(ctx->resdata, name, path, sizeof path))
			continue;
		snprintf(base, sizeof base, "squad%d", n);
		snprintf(name, sizeof name, "Squad%d.txt", n);
		int w = 0, h = 0;
		if (find_ci(ctx->resdata, name, txt, sizeof txt)) {
			w = read_key_int(txt, "Width", 0);
			h = read_key_int(txt, "Height", 0);
		}
		/* With Squad<N>.txt, XWAU draws the logo as a square on screen,
		   Height units tall. Without it, the logo has the standard image's
		   size. The position goes in layout.yaml. */
		if (w && h)
			add_video(ctx, "tour", base, strrchr(path, '/') + 1, h * DESIGN_Y, h * DESIGN_Y, 0, 0);
		else
			add_video(ctx, "tour", base, strrchr(path, '/') + 1, 0, 0, 0, 0);
	}
}

/* Gets the frame count and first frame size of the file the game reads for
 * an image: XWAU's file if there is one, else the stock one. Returns 0 when
 * neither exists. */
static int standard_image(const XwauContext* ctx, const char* dir, const char* base, int* count, int* w,
						  int* h) {
	static const char* const exts[] = { "cbm", "flc", "bmp" };
	const char* roots[2] = { ctx->frontres, NULL };
	char stock[1100];
	if (find_ci(ctx->opt->stock_root, "FRONTRES", stock, sizeof stock))
		roots[1] = stock;
	for (int r = 0; r < 2; ++r) {
		char folder[1200];
		if (!roots[r] || !find_ci(roots[r], dir, folder, sizeof folder))
			continue;
		for (int e = 0; e < 3; ++e) {
			char name[64], path[1400], error[256];
			snprintf(name, sizeof name, "%s.%s", base, exts[e]);
			if (!find_ci(folder, name, path, sizeof path))
				continue;
			size_t size = 0;
			uint8_t* bytes = read_host_file(path, &size);
			Xwa2dFrameSet set = { 0 };
			const int ok = bytes && (e == 0   ? Xwa2d_DecodeCbm(bytes, size, &set, error, sizeof error)
									 : e == 1 ? Xwa2d_DecodeFlc(bytes, size, &set, error, sizeof error)
											  : Xwa2d_DecodeBmp(bytes, size, &set, error, sizeof error));
			free(bytes);
			if (ok && set.count > 0) {
				*count = set.count;
				*w = set.frames[0].width;
				*h = set.frames[0].height;
			}
			Xwa2dFrameSet_Free(&set);
			if (ok && *count > 0)
				return 1;
		}
	}
	return 0;
}

/* Writes frontend/frontres/<dir>/video/<base>.yaml for each image that XWAU
 * replaces with a video. The game keeps the standard image's frame count.
 * XWAU shows one video frame per frontend frame, at 24 frames a second. */
static void write_video_yamls(XwauContext* ctx) {
	for (int i = 0; i < ctx->video_count; ++i) {
		const XwauVideo* v = &ctx->videos[i];
		int count = 1, w = 0, h = 0;
		const int has_image = standard_image(ctx, v->dir, v->base, &count, &w, &h);
		double size[2] = { v->size[0], v->size[1] };
		if (size[0] == 0 && !has_image) {
			fprintf(stderr, "xwau: skipping %s: it has no size and %s/%s has no standard image\n",
					v->webm, v->dir, v->base);
			continue;
		}
		if (size[0] == 0) {
			size[0] = w * DESIGN_X;
			size[1] = h * DESIGN_Y;
		}
		char dir[1200], path[1300];
		snprintf(dir, sizeof dir, "%s/frontend/frontres/%s/video", ctx->opt->sprites->out_root, v->dir);
		BakeSprites_MakeDirs(dir);
		snprintf(path, sizeof path, "%s/%s.yaml", dir, v->base);
		FILE* fp = fopen(path, "w");
		if (!fp) {
			ctx->stats->failures++;
			continue;
		}
		fprintf(fp,
				"# Made by xwa_asset_bake xwau-frontend. Sizes are in design pixels. The video\n"
				"# comes from XWAU's Resdata folder.\n"
				"video: %s\nsize: ",
				v->webm);
		write_list(fp, size, 2);
		fprintf(fp, "\noffset: ");
		write_list(fp, v->offset, 2);
		fprintf(fp, "\nclassic_frames: %d\nfps: 24\n", count);
		if (fclose(fp) != 0)
			ctx->stats->failures++;
	}
}

static void bake_frontres_file(XwauContext* ctx, const char* dir_name, const char* file_name) {
	char dir[1100], path[1400], error[256];
	snprintf(dir, sizeof dir, "%s/%s", ctx->frontres, dir_name);
	snprintf(path, sizeof path, "%s/%s", dir, file_name);
	char base[256];
	snprintf(base, sizeof base, "%s", file_name);
	char* dot = strrchr(base, '.');
	const char* ext = dot ? dot + 1 : "";
	char ext_copy[8];
	snprintf(ext_copy, sizeof ext_copy, "%s", ext);
	if (dot)
		*dot = '\0';
	char dir_lower[256], base_lower[256];
	snprintf(dir_lower, sizeof dir_lower, "%s", dir_name);
	snprintf(base_lower, sizeof base_lower, "%s", base);
	lower(dir_lower);
	lower(base_lower);
	if (find_video(ctx, dir_lower, base_lower))
		return; /* write_video_yamls writes its file */

	Xwa2dFrameSet hd = { 0 };
	XwauDat* hd_dat = NULL;
	const int has_hd = load_frontres_hd(dir, base, &hd, &hd_dat);
	if (has_hd < 0) {
		ctx->stats->failures++;
		return;
	}
	if (!has_hd) {
		char stock_frontres[1100], stock_dir[1200], stock_path[1400];
		const int in_stock =
			find_ci(ctx->opt->stock_root, "FRONTRES", stock_frontres, sizeof stock_frontres) &&
			find_ci(stock_frontres, dir_name, stock_dir, sizeof stock_dir) &&
			find_ci(stock_dir, file_name, stock_path, sizeof stock_path);
		if (in_stock && same_files(path, stock_path)) {
			ctx->stats->unchanged++;
			return;
		}
	}

	size_t size = 0;
	uint8_t* bytes = read_host_file(path, &size);
	Xwa2dFrameSet sd = { 0 };
	CbmGeometry cbm = { 0 };
	const int is_cbm = strcasecmp(ext_copy, "cbm") == 0;
	int ok = bytes != NULL;
	if (ok && is_cbm)
		ok = Xwa2d_DecodeCbm(bytes, size, &sd, error, sizeof error) && read_cbm_geometry(bytes, size, &cbm);
	else if (ok && strcasecmp(ext_copy, "flc") == 0)
		ok = Xwa2d_DecodeFlc(bytes, size, &sd, error, sizeof error);
	else if (ok)
		ok = Xwa2d_DecodeBmp(bytes, size, &sd, error, sizeof error);
	free(bytes);
	if (!ok && bytes && !is_cbm && !has_hd) {
		/* The game's BMP and FLC loaders only read the stock formats, so the game
		 * can't use this file either. XU's 32-bit MAPICONS/LICON.BMP is source art. */
		printf("  note: skipping frontres/%s/%s: %s. The game can't read it either\n", dir_lower,
			   file_name, error);
		Xwa2dFrameSet_Free(&sd);
		return;
	}
	if (!ok || sd.count <= 0) {
		fprintf(stderr, "xwau: %s: %s\n", path, error[0] ? error : "cannot read");
		Xwa2dFrameSet_Free(&sd);
		Xwa2dFrameSet_Free(&hd);
		XwauDat_Close(hd_dat);
		free(cbm.bounds);
		ctx->stats->failures++;
		return;
	}
	if (!is_cbm) {
		/* The game computes the bounds of a BMP or FLC from its pixels. The
		 * image's bounds are the union of its frames' bounds. */
		cbm.count = sd.count;
		cbm.bounds = (int*)calloc((size_t)sd.count * 4, sizeof *cbm.bounds);
		for (int i = 0; i < sd.count && cbm.bounds; ++i) {
			int* b = &cbm.bounds[i * 4];
			compute_bounds_like_game(sd.frames[i].rgba, sd.frames[i].width, sd.frames[i].height, b);
			if (i == 0 || b[0] < cbm.area[0])
				cbm.area[0] = b[0];
			if (i == 0 || b[1] < cbm.area[1])
				cbm.area[1] = b[1];
			if (i == 0 || b[2] > cbm.area[2])
				cbm.area[2] = b[2];
			if (i == 0 || b[3] > cbm.area[3])
				cbm.area[3] = b[3];
		}
	}

	/* The family room monitors have more HD frames than standard ones, and
	 * play them by time at familyroom_monitor_fps. Elsewhere, HD frames
	 * beyond the standard count are never drawn. */
	const int by_time = strcasecmp(dir_name, "family") == 0 &&
						(strcasecmp(base, "lightmon") == 0 || strcasecmp(base, "emailmon") == 0) &&
						hd.count > sd.count;
	const int map_icons = strcasecmp(dir_name, "mapicons") == 0;
	const int icons = strcasecmp(dir_name, "icons") == 0;
	XwauAtlas atlas = { 0 };
	atlas.count = by_time ? hd.count : sd.count;
	atlas.frames = (XwauFrame*)calloc((size_t)atlas.count, sizeof *atlas.frames);
	int new_image = 0;
	for (int i = 0; i < atlas.count && atlas.frames; ++i) {
		XwauFrame* f = &atlas.frames[i];
		const int si = i < sd.count ? i : sd.count - 1;
		const Xwa2dFrame* s = &sd.frames[si];
		f->id = -1;
		f->size[0] = s->width * DESIGN_X;
		f->size[1] = s->height * DESIGN_Y;
		if (cbm.bounds) {
			const int* b = &cbm.bounds[si * 4];
			f->has_bounds = 1;
			f->bounds[0] = b[0] * DESIGN_X;
			f->bounds[1] = b[1] * DESIGN_Y;
			f->bounds[2] = b[2] * DESIGN_X;
			f->bounds[3] = b[3] * DESIGN_Y;
		}
		if (i < hd.count) {
			Xwa2dFrame* h = &hd.frames[i];
			f->rgba = h->rgba;
			f->w = h->width;
			f->h = h->height;
			h->rgba = NULL;
			ctx->stats->hd_frames++;
			new_image |= f->w != s->width || f->h != s->height;
			if (icons && !same_aspect(s->width, s->height, f->w, f->h))
				f->keep_aspect = "width";
			{
				/* A background sets the layout of the screens that show it.
				 * Large images are backgrounds, and so are the images that XWAU
				 * treats as backgrounds at any size, such as MEDBACK (481x474). */
				char key[600], list_key[600];
				snprintf(key, sizeof key, "%s/%s", dir_lower, base_lower);
				snprintf(list_key, sizeof list_key, "frontres/%s/%s.cbm", dir_lower, base_lower);
				if (i == 0 && !map_icons &&
					((s->width >= BACKGROUND_MIN_W && s->height >= BACKGROUND_MIN_H) ||
					 is_screen_background(key, s->width, s->height)))
					note_background(ctx, key, list_key, same_aspect(s->width, s->height, f->w, f->h));
			}
			{
				/* XWAU draws some images as backgrounds: as tall as the screen
				 * and centred, instead of where the game puts them. It does this
				 * when the game loads the image under certain names and the HD
				 * image has a different shape from the standard image
				 * (NET/Main.cs DrawItem, IsBackgroundName). */
				char key[600];
				snprintf(key, sizeof key, "%s/%s", dir_lower, base_lower);
				if (is_screen_background(key, s->width, s->height) && !same_aspect(s->width, s->height, f->w, f->h))
					f->fit = "screen_height";
			}
			if (map_icons) {
				for (size_t t = 0; t < (size_t)f->w * f->h; ++t) {
					uint8_t* px = f->rgba + t * 4;
					if (px[0] == 0 && px[1] == 0 && px[2] == 0)
						px[3] = 0;
				}
			}
			if (hd_dat && (icons || map_icons)) {
				/* On tinted draws, XWAU adds an icon's HD offset to its position
				 * (NET/Main.cs FrontResDrawBlendColor). XWAU draws the HD image
				 * over the standard image's bounds, and the offset moves it back
				 * onto the button. Map icons don't get the offset. */
				XwauDatImageInfo info;
				if (XwauDat_GetImageInfo(hd_dat, h->anchor_x, h->sprite_id, &info) &&
					(info.offset_x || info.offset_y)) {
					if (icons) {
						f->offset[0] = info.offset_x * DESIGN_X;
						f->offset[1] = info.offset_y * DESIGN_Y;
					} else {
						printf("  note: frontres/%s/%s frame %d has HD offset %d, %d, which XWAU ignores "
							   "for map icons\n",
							   dir_lower, base_lower, i, info.offset_x, info.offset_y);
					}
				}
			}
		} else {
			f->rgba = sd.frames[i].rgba;
			f->w = sd.frames[i].width;
			f->h = sd.frames[i].height;
			sd.frames[i].rgba = NULL;
			if (!upscale_standard(ctx, &f->rgba, &f->w, &f->h)) {
				free_atlas_frames(&atlas);
				break;
			}
		}
	}
	if (atlas.frames) {
		if (atlas.count != sd.count)
			atlas.classic_frames = sd.count;
		if (by_time)
			atlas.fps = ctx->monitor_fps;
		if (new_image && !(strcmp(dir_lower, "combat") == 0 && strcmp(base_lower, "hud") == 0))
			atlas.tint = "highlights";
		atlas.has_area = 1;
		atlas.area[0] = cbm.area[0] * DESIGN_X;
		atlas.area[1] = cbm.area[1] * DESIGN_Y;
		atlas.area[2] = cbm.area[2] * DESIGN_X;
		atlas.area[3] = cbm.area[3] * DESIGN_Y;
		if (strcmp(dir_lower, "tour") == 0 && strncmp(base_lower, "squad", 5) == 0) {
			/* XWAU always gives "squadlogo" this area (HC/concourse.cpp:5592-5671). */
			atlas.area[0] = 0;
			atlas.area[1] = 0;
			atlas.area[2] = (cbm.area[2] > 180 ? cbm.area[2] : 180) * DESIGN_X;
			atlas.area[3] = 180 * DESIGN_Y;
		}
		/* Draw-only adjustments from XWAU's HD draw path (NET/Main.cs:835-849,
		 * 1033-1036), in design pixels. */
		static const struct {
			const char* dir;
			const char* base;
			double adjust[4];
		} adjustments[] = {
			{ "combat", "topframe01", { 0, 1 * DESIGN_Y, 0, 0 } },
			{ "combat", "leftframe01", { -5 * DESIGN_X, 0, 7 * DESIGN_X, 0 } },
			{ "config", "sparkcenter", { 0, 2 * DESIGN_Y, 0, 0 } },
		};
		for (size_t a = 0; a < sizeof adjustments / sizeof adjustments[0]; ++a) {
			if (strcmp(dir_lower, adjustments[a].dir) == 0 && strcmp(base_lower, adjustments[a].base) == 0) {
				atlas.has_draw_adjust = 1;
				memcpy(atlas.draw_adjust, adjustments[a].adjust, sizeof atlas.draw_adjust);
			}
		}
		if (map_icons) {
			atlas.regions = ctx->regions;
			atlas.region_count = ctx->region_count;
		}
		char out_dir[1200];
		snprintf(out_dir, sizeof out_dir, "%s/frontend/frontres/%s/atlas", ctx->opt->sprites->out_root,
				 dir_lower);
		emit_atlas(ctx, out_dir, base_lower, &atlas);
		free_atlas_frames(&atlas);
	} else {
		ctx->stats->failures++;
	}
	Xwa2dFrameSet_Free(&sd);
	Xwa2dFrameSet_Free(&hd);
	XwauDat_Close(hd_dat);
	free(cbm.bounds);
}

static int is_standard_image(const char* name) {
	const char* dot = strrchr(name, '.');
	if (!dot || (strcasecmp(dot, ".cbm") && strcasecmp(dot, ".bmp") && strcasecmp(dot, ".flc")))
		return 0;
	const size_t base = (size_t)(dot - name);
	return !(base >= 3 && strncasecmp(dot - 3, "_hd", 3) == 0);
}

static void bake_frontres(XwauContext* ctx) {
	DIR* top = opendir(ctx->frontres);
	if (!top) {
		fprintf(stderr, "xwau: no Frontres folder in %s\n", ctx->opt->xwau_root);
		ctx->stats->failures++;
		return;
	}
	struct dirent* d;
	while ((d = readdir(top)) != NULL) {
		char dir[1300];
		snprintf(dir, sizeof dir, "%s/%s", ctx->frontres, d->d_name);
		if (d->d_name[0] == '.' || !is_directory(dir))
			continue;
		/* Keep one image per base name. The CBM wins because it is the game's cache. */
		char (*files)[256] = calloc(1024, sizeof *files);
		int count = 0;
		DIR* sub = opendir(dir);
		struct dirent* e;
		while (files && sub && (e = readdir(sub)) != NULL && count < 1024) {
			if (!is_standard_image(e->d_name))
				continue;
			const size_t base = (size_t)(strrchr(e->d_name, '.') - e->d_name);
			int k = 0;
			while (k < count && !(strncasecmp(files[k], e->d_name, base) == 0 && files[k][base] == '.'))
				++k;
			if (k == count)
				snprintf(files[count++], sizeof files[0], "%s", e->d_name);
			else if (strcasecmp(strrchr(e->d_name, '.'), ".cbm") == 0)
				snprintf(files[k], sizeof files[0], "%s", e->d_name);
		}
		if (sub)
			closedir(sub);
		for (int i = 0; i < count; ++i)
			bake_frontres_file(ctx, d->d_name, files[i]);
		free(files);
	}
	closedir(top);
}

/* ---- Resdata --------------------------------------------------------------- */

typedef struct XwauDatFile {
	XwauDat* sd;
	XwauDat* hd;
	char name[256];
} XwauDatFile;

/* The frontend's DAT groups: 15000-15999. */
static int is_frontend_group(int group) { return group >= 15000 && group <= 15999; }

static void write_starfield_mask(XwauContext* ctx, const uint8_t* rgba, int w, int h) {
	/* XWAU's HD starfield shows stars where the HD concourse background is
	 * transparent (HC/concourse.cpp:3413-3557). A mask texel is white when all
	 * the HD texels it covers are transparent. */
	uint8_t* mask = (uint8_t*)calloc(640 * 480, 4);
	if (!mask)
		return;
	for (int y = 0; y < 480; ++y) {
		for (int x = 0; x < 640; ++x) {
			const int x0 = x * w / 640, x1 = (x + 1) * w / 640, y0 = y * h / 480, y1 = (y + 1) * h / 480;
			int open = 1;
			for (int sy = y0; sy < y1 && open; ++sy)
				for (int sx = x0; sx < x1 && open; ++sx)
					open = rgba[((size_t)sy * w + sx) * 4 + 3] == 0;
			uint8_t* px = mask + ((size_t)y * 640 + x) * 4;
			px[0] = px[1] = px[2] = open ? 255 : 0;
			px[3] = 255;
		}
	}
	char dir[1100], path[1200], error[256];
	snprintf(dir, sizeof dir, "%s/frontend/concourse", ctx->opt->sprites->out_root);
	BakeSprites_MakeDirs(dir);
	snprintf(path, sizeof path, "%s/starfield_mask.png", dir);
	if (!write_png_rgba_atomic(path, 640, 480, mask, error, sizeof error)) {
		fprintf(stderr, "xwau: %s: %s\n", path, error);
		ctx->stats->failures++;
	}
	free(mask);
}

static int compare_ints(const void* a, const void* b) { return *(const int*)a - *(const int*)b; }

static void bake_group(XwauContext* ctx, const XwauDatFile* files, int file_count, int group) {
	/* Each image comes from the first DAT in RESDATA.TXT that has it, and its
	 * HD image from that DAT's _hd.dat. XWAU's .NET code finds them the same way. */
	int* ids = NULL;
	int* owner = NULL;
	int count = 0, any_hd = 0;
	for (int f = 0; f < file_count; ++f) {
		const int* list;
		const int n = (int)XwauDat_ListGroup(files[f].sd, group, &list);
		for (int i = 0; i < n; ++i) {
			int k = 0;
			while (k < count && ids[k] != list[i])
				++k;
			if (k < count)
				continue;
			ids = (int*)realloc(ids, (size_t)(count + 1) * sizeof *ids);
			owner = (int*)realloc(owner, (size_t)(count + 1) * sizeof *owner);
			XwauDatImageInfo info;
			any_hd |= files[f].hd && XwauDat_GetImageInfo(files[f].hd, group, list[i], &info);
			ids[count] = list[i];
			owner[count++] = f;
		}
	}
	if (!any_hd) {
		free(ids);
		free(owner);
		return;
	}
	/* Sort by id, keeping each id's owner. */
	int* order = (int*)malloc((size_t)count * 2 * sizeof *order);
	for (int i = 0; i < count; ++i) {
		order[i * 2] = ids[i];
		order[i * 2 + 1] = owner[i];
	}
	qsort(order, (size_t)count, 2 * sizeof *order, compare_ints);

	XwauAtlas atlas = { 0 };
	atlas.frames = (XwauFrame*)calloc((size_t)count, sizeof *atlas.frames);
	char error[256];
	int ok = atlas.frames != NULL;
	for (int i = 0; i < count && ok; ++i) {
		const int id = order[i * 2];
		const XwauDatFile* file = &files[order[i * 2 + 1]];
		XwauDatImageInfo sd_info, hd_info;
		XwauDatImage image;
		XwauFrame* f = &atlas.frames[atlas.count++];
		XwauDat_GetImageInfo(file->sd, group, id, &sd_info);
		const int has_hd = file->hd && XwauDat_GetImageInfo(file->hd, group, id, &hd_info);
		f->id = id;
		f->size[0] = sd_info.width * DESIGN_X;
		f->size[1] = sd_info.height * DESIGN_Y;
		/* XWAU draws an HD image at the HD offset instead of the standard one. */
		const XwauDatImageInfo* placed = has_hd ? &hd_info : &sd_info;
		f->offset[0] = xwau_value(placed->offset_x) * DESIGN_X;
		f->offset[1] = xwau_value(placed->offset_y) * DESIGN_Y;
		if (!XwauDat_LoadImageFrom(has_hd ? file->hd : file->sd, group, id, &image, error, sizeof error)) {
			fprintf(stderr, "xwau: %s\n", error);
			ok = 0;
			break;
		}
		f->rgba = image.rgba;
		f->w = (int)image.width;
		f->h = (int)image.height;
		if (has_hd) {
			ctx->stats->hd_frames++;
			/* When a DAT image's HD version has a different aspect ratio, XWAU
			 * keeps the HD image's shape. Backgrounds still fill the screen
			 * (NET/Main.cs:911-999, 1037-1041). */
			const int background =
				(int)sd_info.width >= BACKGROUND_MIN_W && (int)sd_info.height >= BACKGROUND_MIN_H;
			if (!background && !same_aspect((int)sd_info.width, (int)sd_info.height, f->w, f->h))
				f->keep_aspect = "height";
			{
				char key[64];
				snprintf(key, sizeof key, "group/%d/%d", group, id);
				if (is_screen_background(key, (int)sd_info.width, (int)sd_info.height) &&
					!same_aspect((int)sd_info.width, (int)sd_info.height, f->w, f->h))
					f->fit = "screen_height";
			}
			if (background) {
				char key[300], list_key[300];
				snprintf(key, sizeof key, "group/%d/%d", group, id);
				snprintf(list_key, sizeof list_key, "resdata/%s-%d-%d", file->name, group, id);
				lower(list_key);
				note_background(ctx, key, list_key,
								same_aspect((int)sd_info.width, (int)sd_info.height, f->w, f->h));
			}
			if (group == 15001 && id == 3100)
				write_starfield_mask(ctx, f->rgba, f->w, f->h);
			/* XWAU draws an HD image at its HD offset, but the game uses the
			 * standard offset for the image's bounds. The game places some
			 * images from these bounds (the side bars are right-aligned to
			 * them) and uses them as hit areas. So the bounds hold the standard
			 * frame's rectangle, built as in FrontImage_GetAtlasSpriteBounds.
			 * In group 15900, XWAU takes the areas from the HD images
			 * (NET/Main.cs:565-633). */
			if (group != 15900 && !f->has_bounds &&
				(hd_info.offset_x != sd_info.offset_x || hd_info.offset_y != sd_info.offset_y)) {
				f->has_bounds = 1;
				f->bounds[0] = sd_info.offset_x * DESIGN_X;
				f->bounds[1] = sd_info.offset_y * DESIGN_Y;
				f->bounds[2] = (sd_info.offset_x + (int)sd_info.width - 1) * DESIGN_X;
				f->bounds[3] = (sd_info.offset_y + (int)sd_info.height - 1) * DESIGN_Y;
			}
		} else if (!upscale_standard(ctx, &f->rgba, &f->w, &f->h)) {
			ok = 0;
			break;
		}
		/* The area of the rating tooltips: XWAU widens it, or uses
		 * familyroom_rating_* when they are set (HC/concourse.cpp:5642-5658).
		 * The ratings are images 300 + 10 * n to 303 + 10 * n in group 15900. */
		if (group == 15900 && id >= 300 && id < 370 && id % 10 < 4) {
			const int left = (int)lround(f->offset[0] / DESIGN_X), top = (int)lround(f->offset[1] / DESIGN_Y);
			int r[4] = { left, top, left + (int)sd_info.width - 1, top + (int)sd_info.height - 1 };
			const int width = r[2] - r[0], height = r[3] - r[1];
			r[0] -= width * 3 / 2;
			r[2] += width;
			r[1] -= height / 2;
			if (ctx->rating_rect[2] != 0 && ctx->rating_rect[3] != 0) {
				r[0] = ctx->rating_rect[0];
				r[1] = ctx->rating_rect[1];
				r[2] = ctx->rating_rect[0] + ctx->rating_rect[2];
				r[3] = ctx->rating_rect[1] + ctx->rating_rect[3];
			}
			f->has_bounds = 1;
			f->bounds[0] = r[0] * DESIGN_X;
			f->bounds[1] = r[1] * DESIGN_Y;
			f->bounds[2] = r[2] * DESIGN_X;
			f->bounds[3] = r[3] * DESIGN_Y;
		}
	}
	if (ok) {
		char out_dir[1100], base[64];
		snprintf(out_dir, sizeof out_dir, "%s/resdata/atlas", ctx->opt->sprites->out_root);
		snprintf(base, sizeof base, "group_%d", group);
		printf("resdata group %d: %d images\n", group, atlas.count);
		emit_atlas(ctx, out_dir, base, &atlas);
	} else {
		ctx->stats->failures++;
	}
	free_atlas_frames(&atlas);
	free(order);
	free(ids);
	free(owner);
}

static void bake_resdata(XwauContext* ctx) {
	char list_path[1100];
	if (!find_ci(ctx->opt->xwau_root, "RESDATA.TXT", list_path, sizeof list_path)) {
		fprintf(stderr, "xwau: no RESDATA.TXT in %s\n", ctx->opt->xwau_root);
		ctx->stats->failures++;
		return;
	}
	FILE* fp = fopen(list_path, "r");
	if (!fp) {
		ctx->stats->failures++;
		return;
	}
	XwauDatFile files[64];
	int file_count = 0;
	char line[512], error[256];
	while (file_count < 64 && fgets(line, sizeof line, fp)) {
		char* end = line + strlen(line);
		while (end > line && isspace((unsigned char)end[-1]))
			*--end = '\0';
		const char* name = strrchr(line, '\\');
		name = name ? name + 1 : line;
		/* New battle images need new mission content and an exe patch,
		 * which OpenXWA doesn't support. */
		if (!name[0] || strcasecmp(name, "FrontEnd2_Battles.dat") == 0)
			continue;
		char path[1200];
		if (!find_ci(ctx->resdata, name, path, sizeof path))
			continue;
		XwauDat* sd = XwauDat_Open(path, error, sizeof error);
		if (!sd)
			continue;
		const int* groups;
		const int group_count = (int)XwauDat_ListGroups(sd, &groups);
		int frontend = 0;
		for (int g = 0; g < group_count; ++g)
			frontend |= is_frontend_group(groups[g]);
		if (!frontend) {
			XwauDat_Close(sd);
			continue;
		}
		XwauDatFile* file = &files[file_count++];
		memset(file, 0, sizeof *file);
		file->sd = sd;
		snprintf(file->name, sizeof file->name, "%s", name);
		char hd_name[300];
		snprintf(hd_name, sizeof hd_name, "%.*s_hd.dat", (int)(strlen(name) > 4 ? strlen(name) - 4 : 0),
				 name);
		if (find_ci(ctx->resdata, hd_name, path, sizeof path)) {
			file->hd = XwauDat_Open(path, error, sizeof error);
			if (!file->hd) {
				fprintf(stderr, "xwau: %s\n", error);
				ctx->stats->failures++;
			}
		}
	}
	fclose(fp);

	int groups[1000];
	int group_count = 0;
	for (int f = 0; f < file_count; ++f) {
		const int* list;
		const int n = (int)XwauDat_ListGroups(files[f].sd, &list);
		for (int g = 0; g < n; ++g) {
			int k = 0;
			while (k < group_count && groups[k] != list[g])
				++k;
			if (k == group_count && is_frontend_group(list[g]) && group_count < 1000)
				groups[group_count++] = list[g];
		}
	}
	for (int g = 0; g < group_count; ++g)
		bake_group(ctx, files, file_count, groups[g]);
	for (int f = 0; f < file_count; ++f) {
		XwauDat_Close(files[f].sd);
		XwauDat_Close(files[f].hd);
	}
}

/* ---- room positions ------------------------------------------------------ */

/* Reads one ConcourseDoors.txt value the way XWAU does: the file's value,
 * else the hook's default (HC/concourse.cpp:1929-2030). A value above 8000
 * holds tenths of a unit. */
static double door_value(const char* doors, const char* key, int fallback) {
	const int v = doors[0] ? read_key_int(doors, key, fallback) : fallback;
	return v > 8000 ? (v - 8000) / 10.0 : v;
}

static void write_design_point(FILE* fp, double x, double y) {
	const double values[2] = { x * DESIGN_X, y * DESIGN_Y };
	write_list(fp, values, 2);
}

static void write_design_rect(FILE* fp, double x, double y, double w, double h) {
	const double values[4] = { x * DESIGN_X, y * DESIGN_Y, w * DESIGN_X, h * DESIGN_Y };
	write_list(fp, values, 4);
}

/* Writes frontend/concourse/layout.yaml with every position that XWAU moves.
 * Values come from ConcourseDoors.txt, MedalsPositions.txt, the
 * [hook_concourse] settings and the squadron logo files, or from XWAU's
 * default when a file doesn't set one. Keys with stock values are written
 * too, so the game behaves like XWAU, for example by starting Marko's hit box
 * at his position. */
static void write_layout_yaml(XwauContext* ctx) {
	const char* root = ctx->opt->xwau_root;
	char doors[1100] = "", hooks[1100] = "", medals[1100] = "";
	if (!find_ci(root, "ConcourseDoors.txt", doors, sizeof doors))
		doors[0] = '\0';
	const char* hook_section;
	find_hook_settings(root, hooks, sizeof hooks, &hook_section);
#define HOOK_INT(key, fallback)                                                                              \
	(!hooks[0] ? (fallback) : read_section_key_int(hooks, hook_section, key, fallback))
#define DOOR(key, fallback) door_value(doors, key, fallback)

	char dir[1100], path[1200];
	snprintf(dir, sizeof dir, "%s/frontend/concourse", ctx->opt->sprites->out_root);
	BakeSprites_MakeDirs(dir);
	snprintf(path, sizeof path, "%s/layout.yaml", dir);
	FILE* fp = fopen(path, "w");
	if (!fp) {
		ctx->stats->failures++;
		return;
	}
	fprintf(fp, "# Made by xwa_asset_bake xwau-frontend from ConcourseDoors.txt, MedalsPositions.txt,\n"
				"# the [hook_concourse] settings and the squadron logo files. Positions and sizes\n"
				"# are in design pixels.\n"
				"version: 1\n");
	fprintf(fp, "starfield: %s\n", HOOK_INT("DrawStarfield", 1) ? "true" : "false");
	fprintf(fp, "planet: {left: %.0f, right: %.0f, top: %.0f, bottom: %.0f}\n",
			DOOR("planet_position_left", 0) * DESIGN_X, DOOR("planet_position_right", 640) * DESIGN_X,
			DOOR("planet_position_top", 55) * DESIGN_Y, DOOR("planet_position_bottom", 125) * DESIGN_Y);
	static const struct {
		const char* yaml;
		const char* x;
		const char* y;
		int dx, dy;
	} k_doors[] = {
		{ "tour", "tourdoor_posX", "tourdoor_posY", 231, 174 },
		{ "marko", "mkanim_posX", "mkanim_posY", 110, 358 },
		{ "training", "traindoor_posX", "traindoor_posY", 536, 174 },
		{ "combat", "combatdoor_poxX", "combatdoor_poxY", 35, 174 },
		{ "film", "filmdoor_posX", "filmdoor_posY", 536, 286 },
		{ "tech", "techdoor_posX", "techdoor_posY", 426, 219 },
	};
	fprintf(fp, "doors:\n");
	for (size_t i = 0; i < sizeof k_doors / sizeof k_doors[0]; ++i) {
		fprintf(fp, "  %s: ", k_doors[i].yaml);
		write_design_point(fp, DOOR(k_doors[i].x, k_doors[i].dx), DOOR(k_doors[i].y, k_doors[i].dy));
		fprintf(fp, "\n");
	}
	fprintf(fp, "combat_room:\n  multiplayer: {door: ");
	write_design_point(fp, DOOR("multidoor_posX", 24), DOOR("multidoor_posY", 140));
	fprintf(fp, ", selection: ");
	write_design_rect(fp, DOOR("multidoor_posSelectionX", 36), DOOR("multidoor_posSelectionY", 148),
					  DOOR("multidoor_posSelectionWidth", 282), DOOR("multidoor_posSelectionHeight", 108));
	fprintf(fp, "}\n  single_player: {door: ");
	write_design_point(fp, DOOR("singledoor_posX", 373), DOOR("singledoor_posY", 313));
	fprintf(fp, ", selection: ");
	write_design_rect(fp, DOOR("singledoor_posSelectionX", 384), DOOR("singledoor_posSelectionY", 252),
					  DOOR("singledoor_posSelectionWidth", 246), DOOR("singledoor_posSelectionHeight", 172));
	fprintf(fp, "}\n  back_door: ");
	write_design_point(fp, DOOR("combatbackdoor_posX", 299), DOOR("combatbackdoor_posY", 90));
	fprintf(fp, "\nfamily_room:\n  back: ");
	write_design_rect(
		fp, DOOR("familyroom_backtoconcourse_posX", 0), DOOR("familyroom_backtoconcourse_posY", 150),
		DOOR("familyroom_backtoconcourse_posWidth", 120), DOOR("familyroom_backtoconcourse_posHeight", 137));
	fprintf(fp, "\n  statistics: ");
	write_design_rect(fp, DOOR("familyroom_statistics_posX", 403), DOOR("familyroom_statistics_posY", 228),
					  DOOR("familyroom_statistics_posWidth", 96),
					  DOOR("familyroom_statistics_posHeight", 79));
	fprintf(fp, "\n  play_mission: ");
	write_design_rect(fp, DOOR("familyroom_playmission_posX", 103), DOOR("familyroom_playmission_posY", 288),
					  DOOR("familyroom_playmission_posWidth", 131),
					  DOOR("familyroom_playmission_posHeight", 128));
	fprintf(fp, "\n  marko_eye: ");
	write_design_point(fp, DOOR("familyroom_mkeye_posX", 160), DOOR("familyroom_mkeye_posY", 200));
	fprintf(fp, "\n  monitor: ");
	write_design_point(fp, DOOR("familyroom_monitor_posX", 426), DOOR("familyroom_monitor_posY", 243));
	fprintf(fp, "\n  lady_blue_mission: %d\n  cologne_mission: %d\n",
			doors[0] ? read_key_int(doors, "familyroom_ladyblue_mission", 23) : 23,
			doors[0] ? read_key_int(doors, "familyroom_cologne_mission", 9) : 9);
	/* XWAU always draws its own linear progress bar. */
	fprintf(fp, "loading_screen:\n  item: {left: %.0f, right: %.0f, height: %.0f}\n  progress: linear\n",
			DOOR("briefing_review_item_left", 64) * DESIGN_X,
			DOOR("briefing_review_item_right", 573) * DESIGN_X,
			DOOR("briefing_review_item_height", 41) * DESIGN_Y);
	fprintf(fp, "mission_selection_x: %.0f\n", DOOR("mission_selection_screen_posX", 230) * DESIGN_X);

	/* A squadron logo moves when it has a video and a position file
	 * (Resdata\Squad<N>.txt) with a Width and Height. Its number in the
	 * mission is N + 3. */
	int logos = 0;
	for (int n = 1; n <= 99; ++n) {
		char name[32], txt[1200], webm[1200];
		snprintf(name, sizeof name, "Squad%d.txt", n);
		if (!find_ci(ctx->resdata, name, txt, sizeof txt))
			continue;
		snprintf(name, sizeof name, "Squad%d.webm", n);
		if (!find_ci(ctx->resdata, name, webm, sizeof webm) || read_key_int(txt, "Width", 0) == 0 ||
			read_key_int(txt, "Height", 0) == 0)
			continue;
		fprintf(fp, logos++ ? ", " : "briefing:\n  squad_logos: {");
		fprintf(fp, "%d: ", n + 3);
		write_design_point(fp, read_key_int(txt, "PositionX", 0), read_key_int(txt, "PositionY", 0));
	}
	if (logos)
		fprintf(fp, "}\n");

	/* Fixed positions from XWAU's pilot creation and cutscene viewer hooks
	 * (HC:5361-5395, 7958-7976). */
	fprintf(fp, "create_pilot:\n  name_field: ");
	write_design_rect(fp, 305, 260, 120, 20);
	fprintf(fp, "\ncutscene_viewer:\n  scroll_bar_x: 0\n");

	fprintf(fp, "medals:\n");
	if (find_ci(root, "MedalsPositions.txt", medals, sizeof medals)) {
		FILE* mp = fopen(medals, "r");
		char line[256];
		int count = 0;
		fprintf(fp, "  positions: [");
		while (mp && fgets(line, sizeof line, mp)) {
			char* comma = strchr(line, ',');
			if (!comma)
				continue;
			fprintf(fp, count++ ? ", " : "");
			write_design_point(fp, (double)strtol(line, NULL, 0), (double)strtol(comma + 1, NULL, 0));
		}
		if (mp)
			fclose(mp);
		fprintf(fp, "]\n");
	}
	fprintf(fp, "  kalidor: ");
	write_design_point(fp, HOOK_INT("KalidorCrescentPositionX", 224),
					   HOOK_INT("KalidorCrescentPositionY", 168));
	/* Award ceremony: images are placed by their centre, and texts are
	 * offsets from where the stock game puts them. */
	fprintf(fp, "\naward_ceremony:\n  zoom_texts: %s\n",
			doors[0] && read_key_int(doors, "draw_medalzoom_texts", 1) == 0 ? "false" : "true");
	fprintf(fp, "  battle: {images: [");
	write_design_point(fp, DOOR("battlezoom_image1_posX", 320), DOOR("battlezoom_image1_posY", 240));
	fprintf(fp, ", ");
	write_design_point(fp, DOOR("battlezoom_image2_posX", 320), DOOR("battlezoom_image2_posY", 360));
	fprintf(fp, "], text_offsets: [");
	write_design_point(fp, DOOR("battlezoom_text1_posX", 300) - 300,
					   DOOR("battlezoom_text1_posY", 340) - 400);
	fprintf(fp, ", ");
	write_design_point(fp, DOOR("battlezoom_text2_posX", 182) - 182,
					   DOOR("battlezoom_text2_posY", 345) - 345);
	fprintf(fp, "]}\n  kalidor: {images: [");
	write_design_point(fp, DOOR("kalidorzoom_image1_posX", 320), DOOR("kalidorzoom_image1_posY", 240));
	fprintf(fp, ", ");
	write_design_point(fp, DOOR("kalidorzoom_image2_posX", 320), DOOR("kalidorzoom_image2_posY", 120));
	fprintf(fp, "], text_offsets: [");
	write_design_point(fp, DOOR("kalidorzoom_text1_posX", 300) - 300,
					   DOOR("kalidorzoom_text1_posY", 340) - 400);
	fprintf(fp, ", ");
	/* XWAU moves this one from (72, 105) (HC: DrawKalidorZoomText2Hook). */
	write_design_point(fp, DOOR("kalidorzoom_text2_posX", 182) - 72,
					   DOOR("kalidorzoom_text2_posY", 105) - 105);
	fprintf(fp, "]}\n");
#undef HOOK_INT
#undef DOOR
	if (fclose(fp) != 0)
		ctx->stats->failures++;
}

/* ---- room animations ---------------------------------------------------- */

/* XWAU counts animation delays and plays animations in concourse frames, at
 * 24 frames a second. */
#define CONCOURSE_FPS 24.0

typedef struct XwauAnimLine {
	int line;       /* in ConcourseAnimations.txt */
	int x, y, w, h; /* 640x480 units; w and h are 0 when the line has no size */
	int id;
	int delay_min, delay_max; /* concourse frames */
	char group[64];
	char missions[512];
	char video[256]; /* the webm file's name */
	char sound[256]; /* the WAV file's name, or empty */
} XwauAnimLine;

static char* trim(char* s) {
	while (isspace((unsigned char)*s))
		s++;
	char* end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1]))
		*--end = '\0';
	return s;
}

/* Splits `text` at any of `delimiters` and trims each piece, like XWAU.
 * Several delimiters in a row count as one. Returns the number of pieces. */
static int split(char* text, const char* delimiters, char** pieces, int max) {
	int count = 0;
	for (char* piece = strtok(text, delimiters); piece && count < max; piece = strtok(NULL, delimiters))
		pieces[count++] = trim(piece);
	return count;
}

/* Gets the shortest and longest delay before each play, in seconds. XWAU
 * waits delayMin + rand() % (delayMax - delayMin) frames, and doesn't wait
 * when delayMax <= delayMin. Returns 0 for no delay. */
static int anim_delay(const XwauAnimLine* a, double* shortest, double* longest) {
	if (a->delay_max <= a->delay_min || a->delay_max <= 1)
		return 0;
	*shortest = (a->delay_min > 0 ? a->delay_min : 0) / CONCOURSE_FPS;
	*longest = (a->delay_max - 1) / CONCOURSE_FPS;
	return 1;
}

static int same_delay(const XwauAnimLine* a, const XwauAnimLine* b) {
	double a0 = 0, a1 = 0, b0 = 0, b1 = 0;
	const int delayed = anim_delay(a, &a0, &a1);
	return delayed == anim_delay(b, &b0, &b1) && a0 == b0 && a1 == b1;
}

/* Writes XWAU's mission list. An empty list, or one with -1, means all
 * missions. Otherwise, if the list has plain numbers, only those count.
 * If it only has ~ numbers, those are the missions to leave out. */
static void write_missions(FILE* fp, const char* list) {
	char text[512];
	char* ids[128];
	snprintf(text, sizeof text, "%s", list);
	const int count = split(text, " \t", ids, 128);
	int plain = 0;
	for (int i = 0; i < count; ++i) {
		if (strcmp(ids[i], "-1") == 0)
			return;
		plain += ids[i][0] != '~';
	}
	if (count == 0)
		return;
	fprintf(fp, plain ? ", missions: [" : ", missions: {except: [");
	int written = 0;
	for (int i = 0; i < count; ++i) {
		if ((ids[i][0] != '~') == (plain != 0))
			fprintf(fp, "%s%ld", written++ ? ", " : "", strtol(ids[i] + (plain ? 0 : 1), NULL, 0));
	}
	fprintf(fp, plain ? "]" : "]}");
}

/* Finds the WAV file that Sfx\Sfx.lst lists as `concourse_anim_<id>`. XWAU
 * plays it at the start of each play of that animation. Writes the file's
 * name to `out`, or returns 0 when there is none. */
static int find_anim_sound(const XwauContext* ctx, int id, char* out, size_t out_size) {
	char dir[1100], list[1200], name[64], line[600], file[256], sound[256];
	snprintf(name, sizeof name, "concourse_anim_%d", id);
	if (!find_ci(ctx->opt->xwau_root, "Sfx", dir, sizeof dir) || !find_ci(dir, "Sfx.lst", list, sizeof list))
		return 0;
	FILE* fp = fopen(list, "r");
	if (!fp)
		return 0;
	int found = 0;
	/* The game skips the first line. */
	if (fgets(line, sizeof line, fp)) {
		while (!found && fgets(line, sizeof line, fp)) {
			if (sscanf(line, "%255s %255s", file, sound) == 2 && strcasecmp(sound, name) == 0)
				found = 1;
		}
	}
	fclose(fp);
	if (!found)
		return 0;
	/* The path is relative to the install, with backslashes. */
	char path[1400], listed[256];
	snprintf(path, sizeof path, "%s", ctx->opt->xwau_root);
	snprintf(listed, sizeof listed, "%s", file);
	for (char* part = strtok(file, "\\/"); part; part = strtok(NULL, "\\/")) {
		char next[1400];
		if (!find_ci(path, part, next, sizeof next)) {
			fprintf(stderr, "xwau: Sfx.lst lists %s for %s, but the file is missing\n", listed, name);
			return 0;
		}
		snprintf(path, sizeof path, "%s", next);
	}
	snprintf(out, out_size, "%s", strrchr(path, '/') + 1);
	return 1;
}

/* Writes frontend/animations.yaml from ConcourseAnimations.txt. Each XWAU
 * group becomes a group. A group whose name starts with + repeats the same
 * animation. Lines that OpenXWA can't play the same way are reported. */
/* Writes frontend/fonts/fonts.yaml when XWAU draws the frontend text with a
 * system font (HDConcourseTextFont). XWAU draws it in bold, so the file names
 * the font's bold file, which must be copied by hand. */
static void write_fonts_yaml(XwauContext* ctx) {
	char hooks[1100], font[128] = "consolas";
	const char* section;
	if (!find_hook_settings(ctx->opt->xwau_root, hooks, sizeof hooks, &section) ||
		read_section_key_int(hooks, section, "HDConcourseEnabled", 0) == 0)
		return;
	read_key_string(hooks, section, "HDConcourseTextFont", font, sizeof font);
	if (strcasecmp(font, "consolas") != 0) {
		fprintf(stderr, "xwau: HDConcourseTextFont = %s: the font's bold file isn't known, fonts.yaml isn't written\n",
				font);
		return;
	}
	char dir[1100], path[1200];
	snprintf(dir, sizeof dir, "%s/frontend/fonts", ctx->opt->sprites->out_root);
	BakeSprites_MakeDirs(dir);
	snprintf(path, sizeof path, "%s/fonts.yaml", dir);
	FILE* fp = fopen(path, "w");
	if (!fp) {
		ctx->stats->failures++;
		return;
	}
	fprintf(fp, "# Made by xwa_asset_bake xwau-frontend from HDConcourseTextFont.\n"
				"version: 1\n"
				"default: {file: consolab.ttf, metrics: font}\n");
	fclose(fp);
	printf("xwau: copy consolab.ttf from Windows' Fonts folder to frontend/fonts/\n");
}

static void write_animations_yaml(XwauContext* ctx) {
	char list[1100];
	if (!find_ci(ctx->opt->xwau_root, "ConcourseAnimations.txt", list, sizeof list))
		return;
	FILE* in = fopen(list, "r");
	if (!in) {
		ctx->stats->failures++;
		return;
	}
	XwauAnimLine* lines = NULL;
	int count = 0;
	char text[2048];
	for (int number = 1; fgets(text, sizeof text, in); ++number) {
		char* t = trim(text);
		if (!t[0] || t[0] == '#' || t[0] == ';' || (t[0] == '/' && t[1] == '/'))
			continue;
		/* XWAU reads the lines before the first [section]. */
		if (t[0] == '[' && t[strlen(t) - 1] == ']')
			break;
		char* v[16];
		const int n = split(t, ",;", v, 16);
		if (n < 8)
			continue;
		/* Lines with 8 pieces, or 9 with a comment, have no size. */
		const int k = n == 8 || n == 9 ? 0 : 2;
		XwauAnimLine a;
		memset(&a, 0, sizeof a);
		a.line = number;
		a.x = (int)strtol(v[1], NULL, 0);
		a.y = (int)strtol(v[2], NULL, 0);
		if (k) {
			a.w = (int)strtol(v[3], NULL, 0);
			a.h = (int)strtol(v[4], NULL, 0);
		}
		a.id = (int)strtol(v[3 + k], NULL, 0);
		a.delay_min = (int)strtol(v[4 + k], NULL, 0);
		a.delay_max = (int)strtol(v[5 + k], NULL, 0);
		snprintf(a.group, sizeof a.group, "%s", v[6 + k]);
		snprintf(a.missions, sizeof a.missions, "%s", v[7 + k]);
		if (strcasecmp(v[0], "concourse") != 0) {
			fprintf(stderr,
					"xwau: ConcourseAnimations.txt:%d: skipping this %s line: XWAU only draws concourse "
					"animations\n",
					number, v[0]);
			continue;
		}
		char name[64], webm[1200];
		snprintf(name, sizeof name, "concourse_anim_%d.webm", a.id);
		if (!find_ci(ctx->resdata, name, webm, sizeof webm)) {
			fprintf(stderr,
					"xwau: ConcourseAnimations.txt:%d: skipping concourse_anim_%d: it has no webm file, "
					"and OpenXWA doesn't play DAT animations\n",
					number, a.id);
			continue;
		}
		snprintf(a.video, sizeof a.video, "%s", strrchr(webm, '/') + 1);
		if (!find_anim_sound(ctx, a.id, a.sound, sizeof a.sound))
			a.sound[0] = '\0';
		if (a.w == 0 || a.h == 0)
			fprintf(stderr,
					"xwau: ConcourseAnimations.txt:%d: concourse_anim_%d has no size. XWAU draws one video "
					"pixel per 640x480 unit, OpenXWA one per design pixel\n",
					number, a.id);
		XwauAnimLine* grown = (XwauAnimLine*)realloc(lines, (size_t)(count + 1) * sizeof *lines);
		if (!grown)
			break;
		lines = grown;
		lines[count++] = a;
	}
	fclose(in);

	char path[1200];
	snprintf(path, sizeof path, "%s/frontend/animations.yaml", ctx->opt->sprites->out_root);
	FILE* fp = fopen(path, "w");
	if (!fp) {
		free(lines);
		ctx->stats->failures++;
		return;
	}
	fprintf(fp,
			"# Made by xwa_asset_bake xwau-frontend from ConcourseAnimations.txt. Positions and\n"
			"# sizes are in design pixels, delays in seconds. Copy the videos from\n"
			"# XWAU's Resdata folder, and the sounds from its Sfx folder, to\n"
			"# frontend/animations/.\n"
			"version: 1\n"
			"groups:%s\n",
			count ? "" : " []");
	/* Groups are written in the order of their first line. Group names are
	   compared ignoring case, like XWAU does. */
	for (int first = 0; first < count; ++first) {
		int seen = 0;
		for (int i = 0; i < first && !seen; ++i)
			seen = strcasecmp(lines[i].group, lines[first].group) == 0;
		if (seen)
			continue;
		const XwauAnimLine* a = &lines[first];
		double shortest, longest;
		fprintf(fp, "  - room: concourse\n    repeat: %s\n", a->group[0] == '+' ? "same" : "random");
		if (anim_delay(a, &shortest, &longest)) {
			const double delay[2] = { shortest, longest };
			fprintf(fp, "    delay: ");
			write_list(fp, delay, 2);
			fprintf(fp, "\n");
		}
		fprintf(fp, "    animations:\n");
		int apart = 0;
		for (int i = first; i < count; ++i) {
			const XwauAnimLine* b = &lines[i];
			if (strcasecmp(b->group, a->group) != 0) {
				apart = 1;
				continue;
			}
			if (!same_delay(a, b))
				fprintf(
					stderr,
					"xwau: ConcourseAnimations.txt:%d: this delay differs from the one on line %d. OpenXWA "
					"has one delay per group, so %s uses the delay from line %d\n",
					b->line, a->line, a->group, a->line);
			if (apart == 1) {
				fprintf(
					stderr,
					"xwau: ConcourseAnimations.txt:%d: the lines of %s are not together. OpenXWA draws the "
					"whole group at its first line, so the order with other groups can differ\n",
					b->line, a->group);
				apart = 2;
			}
			fprintf(fp, "      - {video: %s, at: ", b->video);
			write_design_point(fp, b->x, b->y);
			if (b->w && b->h) {
				fprintf(fp, ", size: ");
				write_design_point(fp, b->w, b->h);
			}
			/* XWAU shows one video frame per concourse frame, whatever the
			   file's frame rate. */
			fprintf(fp, ", fps: %.0f", CONCOURSE_FPS);
			if (b->sound[0])
				fprintf(fp, ", sound: %s", b->sound);
			write_missions(fp, b->missions);
			fprintf(fp, "}\n");
		}
	}
	free(lines);
	if (fclose(fp) != 0)
		ctx->stats->failures++;
}

/* The screens and views of the layout table, and the backgrounds each one
 * shows, taken from the recovered code. The family room's memento and rank
 * views show many backgrounds in medals/, so they are selected by prefix. */
typedef struct XwauScreen {
	const char* name;
	const char* backgrounds[4];
	const char* medal_prefix; /* "rankzoom", or "" for the memento views */
} XwauScreen;

static const XwauScreen k_screens[] = {
	{ "concourse", { "group/15001/3100" }, NULL },
	{ "combat_room", { "combat/combat" }, NULL },
	{ "combat_room.family", { "combat/podopen" }, NULL },
	{ "family_room", { "family/familyroom" }, NULL },
	{ "family_room.monitor", { "family/monitor" }, NULL },
	{ "family_room.detail", { NULL }, "" },
	{ "family_room.medal_case", { "medals/medback" }, NULL },
	{ "family_room.rank", { NULL }, "rankzoom" },
	{ "film_room", { "combat/multiplayer" }, NULL },
	{ "tech_room", { "techroom/techglobe" }, NULL },
	{ "briefing_room", { "combat/solo", "family/markoholo" }, NULL },
	{ "briefing_room.start", { "family/markostart", "combat/briefoff" }, NULL },
	{ "mission_briefing", { "combat/solo", "family/markoholo", "combat/multiplayer" }, NULL },
	{ "mission_setup", { "combat/multiplayer" }, NULL },
	{ "debrief", { "combat/solo", "family/markoholo", "combat/multiplayer" }, NULL },
	{ "datapad", { "config/configb" }, NULL },
	{ "create_pilot", { "concourse/create" }, NULL },
	{ "get_ready", { "config/configb" }, NULL },
	{ "loading", { "config/dpmed" }, NULL },
};

#define SCREEN_COUNT (sizeof k_screens / sizeof k_screens[0])

/* Returns whether `key` is one of the memento (prefix "") or rank zoom
 * backgrounds in medals/. The medal case's own images are neither. */
static int is_medal_view(const char* key, const char* prefix) {
	if (strncmp(key, "medals/", 7) != 0)
		return 0;
	const char* base = key + 7;
	if (prefix[0])
		return strncmp(base, prefix, strlen(prefix)) == 0;
	return strcmp(base, "medback") != 0 && strcmp(base, "medalcase") != 0 && strncmp(base, "rankzoom", 8) != 0 &&
		   strncmp(base, "lbattle", 7) != 0 && strncmp(base, "lkalidor", 8) != 0;
}

/* Returns the most common layout among a screen's backgrounds, and prints a
 * note for each background with another layout. Returns NULL when the screen
 * has no backgrounds. */
static const char* screen_layout(const XwauContext* ctx, const XwauScreen* screen, int* image_count) {
	static const char* const names[] = { "wide", "fill", "classic" };
	const char* keys[256];
	const char* layouts[256];
	int count = 0;
	if (screen->medal_prefix) {
		for (int i = 0; i < ctx->background_count && count < 256; ++i)
			if (is_medal_view(ctx->backgrounds[i].key, screen->medal_prefix)) {
				keys[count] = ctx->backgrounds[i].key;
				layouts[count++] = ctx->backgrounds[i].layout;
			}
	} else {
		for (int b = 0; b < 4 && screen->backgrounds[b]; ++b) {
			keys[count] = screen->backgrounds[b];
			layouts[count++] = background_layout(ctx, screen->backgrounds[b]);
		}
	}
	*image_count = count;
	if (count == 0)
		return NULL;
	int votes[3] = { 0 }, best = 0;
	for (int i = 0; i < count; ++i)
		for (int n = 0; n < 3; ++n)
			votes[n] += strcmp(layouts[i], names[n]) == 0;
	for (int n = 1; n < 3; ++n)
		if (votes[n] > votes[best])
			best = n;
	for (int i = 0; i < count; ++i)
		if (strcmp(layouts[i], names[best]) != 0)
			fprintf(stderr, "  note: %s is %s, but its background %s is %s\n", screen->name, names[best], keys[i],
					layouts[i]);
	return names[best];
}

/* Writes only the screens whose layout differs from their fallback. A view
 * falls back to its screen, and a screen falls back to `default: wide`. */
static void write_layouts(const XwauContext* ctx, FILE* fp) {
	const char* layouts[SCREEN_COUNT];
	int counts[SCREEN_COUNT];
	for (size_t i = 0; i < SCREEN_COUNT; ++i)
		layouts[i] = screen_layout(ctx, &k_screens[i], &counts[i]);
	fprintf(fp, "# The layout of each screen: wide (the whole screen), fill (a background\n"
				"# around the original screen's items), or classic (the original 4:3\n"
				"# screen). Views such as family_room.rank fall back to their screen.\n"
				"layouts:\n"
				"  default: wide\n");
	for (size_t i = 0; i < SCREEN_COUNT; ++i) {
		const char* name = k_screens[i].name;
		const char* dot = strchr(name, '.');
		const char* fallback = "wide";
		for (size_t j = 0; dot && j < SCREEN_COUNT; ++j)
			if (strlen(k_screens[j].name) == (size_t)(dot - name) && strncmp(k_screens[j].name, name, (size_t)(dot - name)) == 0 &&
				layouts[j])
				fallback = layouts[j];
		if (!layouts[i] || strcmp(layouts[i], fallback) == 0)
			continue;
		fprintf(fp, "  %s: %s  # ", name, layouts[i]);
		if (k_screens[i].medal_prefix)
			fprintf(fp, "%d images in medals/", counts[i]);
		for (int b = 0; !k_screens[i].medal_prefix && b < counts[i]; ++b)
			fprintf(fp, "%s%s", b ? ", " : "", k_screens[i].backgrounds[b]);
		fprintf(fp, "\n");
	}
}

/* Returns whether XWAU treats the image `key` ("<dir>/<base>" or
 * "group/<g>/<id>") as a background. XWAU decides from the name the game
 * loads the image under: backname, backconfig, concourse, background,
 * briefstart, globe, techroom or cerecase (NET/Main.cs _backgroundNames).
 * These names cover the images in the screen table, the family room's views,
 * the ceremony's medal case and the tech room. The family room's views are
 * found by folder, but that folder also holds the medal case's small medals.
 * So a view's standard image (`width` by `height`) must also be at least half
 * the screen in each direction: the views are 415x384 units or more, and the
 * medals 108x119 or less. */
static int is_screen_background(const char* key, int width, int height) {
	for (size_t i = 0; i < SCREEN_COUNT; ++i) {
		if (k_screens[i].medal_prefix && is_medal_view(key, k_screens[i].medal_prefix) && width >= 320 &&
			height >= 240)
			return 1;
		for (int b = 0; b < 4 && k_screens[i].backgrounds[b]; ++b)
			if (strcmp(k_screens[i].backgrounds[b], key) == 0)
				return 1;
	}
	return strcmp(key, "medals/medalcase") == 0 || strcmp(key, "group/15000/3000") == 0;
}

static void write_frontend_yaml(XwauContext* ctx) {
	char dir[1100], path[1200];
	snprintf(dir, sizeof dir, "%s/frontend", ctx->opt->sprites->out_root);
	BakeSprites_MakeDirs(dir);
	snprintf(path, sizeof path, "%s/frontend.yaml", dir);
	FILE* fp = fopen(path, "w");
	if (!fp) {
		ctx->stats->failures++;
		return;
	}
	fprintf(fp, "# Made by xwa_asset_bake xwau-frontend.\n"
				"version: 1\n"
				"# The screen the frontend art is drawn for. Positions and sizes in the\n"
				"# frontend's files are pixels of this screen.\n"
				"design: [2560, 1440]\n"
				"# XWAU's look: translucent fills at 0.7, lines one screen pixel thick, and\n"
				"# a pure red briefing grid.\n"
				"paint:\n"
				"  fill_alpha: 0.7\n"
				"  line_width: pixel\n"
				"  grid_colours: [0xFF0000, 0xFF0000]\n");
	write_layouts(ctx, fp);
	if (fclose(fp) != 0)
		ctx->stats->failures++;
}

int BakeXwau_RunFrontend(const BakeXwauOptions* opt, BakeXwauStats* stats) {
	memset(stats, 0, sizeof *stats);
	XwauContext ctx;
	memset(&ctx, 0, sizeof ctx);
	ctx.opt = opt;
	ctx.stats = stats;
	if (!find_ci(opt->xwau_root, "Frontres", ctx.frontres, sizeof ctx.frontres) ||
		!find_ci(opt->xwau_root, "Resdata", ctx.resdata, sizeof ctx.resdata)) {
		fprintf(stderr, "xwau: %s has no Frontres or Resdata folder\n", opt->xwau_root);
		stats->failures++;
		return 0;
	}
	char doors[1100];
	ctx.monitor_fps = 6; /* the hook's default */
	if (find_ci(opt->xwau_root, "ConcourseDoors.txt", doors, sizeof doors)) {
		ctx.monitor_fps = read_key_int(doors, "familyroom_monitor_fps", ctx.monitor_fps);
		ctx.rating_rect[0] = read_key_int(doors, "familyroom_rating_left", 0);
		ctx.rating_rect[1] = read_key_int(doors, "familyroom_rating_top", 0);
		ctx.rating_rect[2] = read_key_int(doors, "familyroom_rating_width", 0);
		ctx.rating_rect[3] = read_key_int(doors, "familyroom_rating_height", 0);
	}
	read_ship_regions(&ctx, opt->xwau_root);
	read_wide_list(&ctx, opt->xwau_root);
	find_videos(&ctx);

	bake_frontres(&ctx);
	bake_resdata(&ctx);
	write_frontend_yaml(&ctx);
	write_layout_yaml(&ctx);
	write_animations_yaml(&ctx);
	write_video_yamls(&ctx);
	write_fonts_yaml(&ctx);

	free(ctx.regions);
	for (int i = 0; i < ctx.wide_key_count; ++i)
		free(ctx.wide_keys[i]);
	free(ctx.wide_keys);
	free(ctx.backgrounds);
	free(ctx.videos);
	return stats->failures == 0;
}
