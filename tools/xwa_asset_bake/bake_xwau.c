/*
 * xwa_asset_bake xwau-frontend: converts XWAU's frontend art for the mod
 * folder (docs/xwau_hd_concourse_plan.md, 4.0-4.4 and 4.12).
 *
 * XWAU replaces standard frontend files to change the frontend's layout, and
 * draws HD images in the rectangles they give. Here, each replaced image gets
 * an atlas with `game_geometry: true`: its pixels come from XWAU's HD image
 * (or from XWAU's standard image, upscaled, when there is none), and its frame
 * count, sizes, offsets and bounds come from XWAU's standard files. Values are
 * written in design pixels, for a 2560x1440 design screen: 640x480 units
 * times 4 across and 3 down.
 *
 * Output:
 *   <out>/frontend/frontend.yaml                    design screen and paint look
 *   <out>/frontend/frontres/<dir>/atlas/<base>.yaml with <base>.ktx2, <base>_p<N>.ktx2
 *   <out>/resdata/atlas/group_<g>.yaml              with group_<g>.ktx2, group_<g>_p<N>.ktx2
 *   <out>/frontend/concourse/starfield_mask.png     where the concourse stars can show
 *
 * Standard files equal to stock that have no HD image are left to the stock
 * bake. The XWAU install is read with host file I/O; names match in any case.
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

/* A standard image this large, with an HD image of another shape, is a room
 * background, and sets the room's layout (plan 4.4). TECHGLOBE is 575x464. */
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
	const char* layout;
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

/* Finds `name` in `dir` in any case, and writes its path. */
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

/* Reads `key = value` from a hook text file; `;` starts a comment. */
static int read_key_int(const char* path, const char* key, int fallback) {
	FILE* fp = fopen(path, "r");
	if (!fp)
		return fallback;
	char line[512];
	int value = fallback;
	while (fgets(line, sizeof line, fp)) {
		char* comment = strchr(line, ';');
		if (comment)
			*comment = '\0';
		char* eq = strchr(line, '=');
		if (!eq)
			continue;
		*eq = '\0';
		char* name = line;
		while (isspace((unsigned char)*name))
			++name;
		char* end = name + strlen(name);
		while (end > name && isspace((unsigned char)end[-1]))
			*--end = '\0';
		if (strcasecmp(name, key) == 0)
			value = atoi(eq + 1);
	}
	fclose(fp);
	return value;
}

/* The map icon rectangle of each craft type in SHIPLIST.TXT: the last four
 * numbers of a line. Lines are numbered and skipped as ShipList_Load does
 * (src/xwa/assets/ship_list.c): from 1, without `*` lines or lines whose
 * category the game doesn't know. */
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
		if (f->layout)
			fprintf(fp, ", layout: %s", f->layout);
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

/* Upscales a standard image like the stock bake. Takes ownership of `rgba`. */
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

/* The bounds that FrontImage_ComputeImageBounds (src/xwa/frontend/
 * frontend_image.c) gives a BMP or FLC frame, with the same scan and early
 * exits. Transparent texels are the game's palette index 0. */
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

/* XWAU's HD images for a Frontres file: the images of <base>_hd.dat in group
 * and image order, or the frames of <base>_hd.cbm. */
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
				f->anchor_x = groups[g]; /* the group, to read the image's offset back */
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
		/* Palette index 0 is transparent, as XWAU makes it when its colour is
		 * black (NET/ResItemImage.cs). XU's only _hd.cbm, EYE_HD, has it black. */
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
		/* The game's BMP and FLC loaders read the stock formats only, so it
		 * doesn't use this file (XU's 32-bit MAPICONS/LICON.BMP is source art). */
		printf("  note: frontres/%s/%s is skipped: %s, which the game can't read either\n", dir_lower,
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
		/* The game computes the bounds of a BMP or FLC from its pixels, and
		 * the image's bounds are the union of its frames' bounds. */
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
	 * play them by time at familyroom_monitor_fps (plan 4.3). Elsewhere, HD
	 * frames beyond the standard count are never drawn. */
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
			if (i == 0 && !map_icons && s->width >= BACKGROUND_MIN_W && s->height >= BACKGROUND_MIN_H &&
				!same_aspect(s->width, s->height, f->w, f->h)) {
				char key[600];
				snprintf(key, sizeof key, "frontres/%s/%s.cbm", dir_lower, base_lower);
				f->layout = strcmp(base_lower, "dpmed") == 0 || strcmp(base_lower, "configb") == 0 ||
									listed_not_wide(ctx, key)
								? "fill"
								: "wide";
			}
			if (map_icons) {
				for (size_t t = 0; t < (size_t)f->w * f->h; ++t) {
					uint8_t* px = f->rgba + t * 4;
					if (px[0] == 0 && px[1] == 0 && px[2] == 0)
						px[3] = 0;
				}
			}
			if (hd_dat && (icons || map_icons)) {
				XwauDatImageInfo info;
				if (XwauDat_GetImageInfo(hd_dat, h->anchor_x, h->sprite_id, &info) &&
					(info.offset_x || info.offset_y))
					printf("  note: frontres/%s/%s frame %d has HD offset %d, %d, which XWAU adds "
						   "only on tinted draws\n",
						   dir_lower, base_lower, i, info.offset_x, info.offset_y);
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
		/* Drawing-only adjustments of XWAU's HD draw path (NET/Main.cs:835-849,
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
		/* One image per base name; the CBM, the game's cache, wins. */
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
	/* XWAU's HD starfield puts stars where the HD concourse background is
	 * transparent, scaled down to 640x480 (HC/concourse.cpp:3413-3557). */
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
	/* Each image comes from the first DAT of RESDATA.TXT that has it, and its
	 * HD image from that DAT's _hd.dat, as XWAU's .NET code finds them. */
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
		/* XWAU draws an HD image at its own offset, in place of the standard one. */
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
			if (!same_aspect((int)sd_info.width, (int)sd_info.height, f->w, f->h))
				f->keep_aspect = "height";
			if ((int)sd_info.width >= BACKGROUND_MIN_W && (int)sd_info.height >= BACKGROUND_MIN_H &&
				!same_aspect((int)sd_info.width, (int)sd_info.height, f->w, f->h)) {
				char key[300];
				snprintf(key, sizeof key, "resdata/%s-%d-%d", file->name, group, id);
				lower(key);
				f->layout = listed_not_wide(ctx, key) ? "fill" : "wide";
			}
			if (group == 15001 && id == 3100)
				write_starfield_mask(ctx, f->rgba, f->w, f->h);
			if (group != 15900 &&
				(hd_info.offset_x != sd_info.offset_x || hd_info.offset_y != sd_info.offset_y))
				printf("  note: %d-%d: HD offset %g, %g, standard offset %d, %d; clicks follow the HD art\n",
					   group, id, xwau_value(hd_info.offset_x), xwau_value(hd_info.offset_y),
					   sd_info.offset_x, sd_info.offset_y);
		} else if (!upscale_standard(ctx, &f->rgba, &f->w, &f->h)) {
			ok = 0;
			break;
		}
		/* The rating tooltips' area: XWAU always widens it, or takes
		 * familyroom_rating_* (HC/concourse.cpp:5642-5658). The ratings are
		 * images 300 + 10 * n to 303 + 10 * n of group 15900. */
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
		/* New battle images need new mission content and an exe patch
		 * (plan section 8). */
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

/* ---- entry ----------------------------------------------------------------- */

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

	write_frontend_yaml(&ctx);
	bake_frontres(&ctx);
	bake_resdata(&ctx);

	free(ctx.regions);
	for (int i = 0; i < ctx.wide_key_count; ++i)
		free(ctx.wide_keys[i]);
	free(ctx.wide_keys);
	return stats->failures == 0;
}
