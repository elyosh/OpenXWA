/*
 * xwa_asset_bake — sprite bake pass.
 *
 * Frontend source images (every CBM/BMP/FLC under FRONTRES — assets
 * are keyed by source FILE because the engine binds resource names to
 * files at runtime, e.g. "background" is a different file per room)
 * and the global DAT atlas sprites (per group). Decode via
 * the shared classic decoder, HD upscale + pack + encode via imgbake
 * backend.
 *
 * Output layout (per user: PNG for artists, KTX2 for the game):
 *   <out>/frontres/<dir>/sprites/<base>.{png,ktx2}        single frame
 *   <out>/frontres/<dir>/atlas/<base>.{png,ktx2,yaml}     packed frames
 *   <out>/frontres/<dir>/atlas/<base>/frame_NN.{png,ktx2} frames too
 *                                                    big to pack (FLIC)
 *   <out>/resdata/atlas/group_<g>.{png,ktx2,yaml}         DAT group
 *   <out>/resdata/atlas/group_<g>/frame_NN.{png,ktx2}     oversized
 *
 * Atlas layout YAML matches AeronSpriteAtlas: atlas{w,h,classic_w,
 * classic_h} + frames[{x,y,w,h,origin_x,origin_y}] (frame index is
 * positional: animation frame / DAT sprite id). origin_x/y carry the
 * DAT anchor in CLASSIC units.
 */

#include "bake_sprites.h"
#include "bake_source.h"
#include "aeron/atlas_pack.h"
#include "xwa_2d.h"

#include "ktx2_writer.h"
#include "png_write.h"
#include "upscale.h"

#include "aeron/vfs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define BAKE_MAX_ATLAS_DIM 8192
#define BAKE_HD_PAD        8 /* HD-texel gutter between packed frames */

/* ---- small utilities ------------------------------------------------- */

static int mkdir_p(const char* path) {
	char tmp[1024];
	snprintf(tmp, sizeof tmp, "%s", path);
	for (char* p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
				return 0;
			}
			*p = '/';
		}
	}
	return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

static void lower_str(char* s) {
	for (; *s; s++) {
		if (*s >= 'A' && *s <= 'Z') {
			*s += 'a' - 'A';
		}
	}
}

typedef struct BakeFrame {
	uint8_t* rgba; /* HD (or classic when !scale) straight-alpha */
	int      w, h;
	int      classic_w, classic_h;
	int      origin_x, origin_y; /* classic units (DAT anchors) */
} BakeFrame;

static void frames_free(BakeFrame* f, int n) {
	for (int i = 0; i < n; i++) {
		free(f[i].rgba);
	}
	free(f);
}

/* Write one RGBA image as PNG and/or KTX2 per options. */
static int emit_image(const BakeSpriteOptions* opt, const char* base_path, const uint8_t* rgba,
					  int w, int h) {
	char path[1100];
	char err[256];
	int  ok = 1;
	if (opt->write_png) {
		snprintf(path, sizeof path, "%s.png", base_path);
		if (!write_png_rgba_atomic(path, w, h, rgba, err, sizeof err)) {
			fprintf(stderr, "  PNG write failed: %s (%s)\n", path, err);
			ok = 0;
		}
	}
	if (opt->write_ktx2) {
		snprintf(path, sizeof path, "%s.ktx2", base_path);
		if (!write_ktx2_bc7_with_generated_mips(path, w, h, rgba, opt->bc7_quality, KTX2_TF_SRGB,
												opt->zstd)) {
			fprintf(stderr, "  KTX2 write failed: %s\n", path);
			ok = 0;
		}
	}
	return ok;
}

static int build_atlas(const BakeFrame* frames, int count, int max_pages,
					   AeronAtlasImage** out_images, AeronCpuAtlas* out) {
	AeronAtlasImage* images = (AeronAtlasImage*)calloc((size_t)count, sizeof *images);
	if (!images)
		return 0;
	for (int i = 0; i < count; i++) {
		images[i] = (AeronAtlasImage) {
			.rgba = frames[i].rgba,
			.width = frames[i].w,
			.height = frames[i].h,
		};
	}
	const AeronAtlasBuildOptions options = {
		.gutter = BAKE_HD_PAD,
		.max_dimension = BAKE_MAX_ATLAS_DIM,
		.max_pages = max_pages,
	};
	const int built = Aeron_AtlasBuildRgba8(images, count, &options, out);
	if (built)
		*out_images = images;
	else
		free(images);
	return built;
}

/* AeronSpriteAtlas-compatible layout YAML. */
static int emit_atlas_yaml(const char* path, const AeronAtlasImage* images, const BakeFrame* frames,
						   const uint16_t* ids, int count, const AeronCpuAtlas* atlas,
						   int classic_atlas_w, int classic_atlas_h) {
	FILE* fp = fopen(path, "w");
	if (!fp) {
		return 0;
	}
	fprintf(fp, "atlas: {w: %d, h: %d, classic_w: %d, classic_h: %d}\n", atlas->pages[0].width,
			atlas->pages[0].height, classic_atlas_w, classic_atlas_h);
	fprintf(fp, "frames:\n");
	for (int i = 0; i < count; i++) {
		const AeronAtlasImage* placement = &images[i];
		const BakeFrame* frame = &frames[i];
		fprintf(fp, "  - {");
		if (ids)
			fprintf(fp, "id: %u, ", ids[i]);
		if (atlas->page_count > 1)
			fprintf(fp, "page: %d, ", placement->page);
		fprintf(fp,
				"x: %d, y: %d, w: %d, h: %d, origin_x: %d, origin_y: %d, "
				"classic_w: %d, classic_h: %d}\n",
				placement->x, placement->y, frame->w, frame->h, frame->origin_x, frame->origin_y,
				frame->classic_w, frame->classic_h);
	}
	fclose(fp);
	return 1;
}

/* HD-upscale a decoded classic frame in place (unless disabled). */
static int frame_upscale(const BakeSpriteOptions* opt, BakeFrame* f) {
	f->classic_w = f->w;
	f->classic_h = f->h;
	if (!opt->scale) {
		return 1;
	}
	return atlas_svga_to_4k(&f->rgba, &f->w, &f->h) ? 1 : 0;
}

/* Emit a resource: single sprite, packed atlas, or frames-dir
 * fallback. force_atlas keeps single-frame resources in atlas+YAML
 * form (DAT groups: the runtime addresses group+index and needs the
 * per-frame anchors from the YAML). */
static void emit_frames(const BakeSpriteOptions* opt, const char* bundle_dir, const char* name,
						BakeFrame* frames, int n, int force_atlas, BakeStats* stats) {
	char base[1060];
	if (n == 1 && !force_atlas) {
		char dir[1040];
		snprintf(dir, sizeof dir, "%s/sprites", bundle_dir);
		mkdir_p(dir);
		snprintf(base, sizeof base, "%s/%s", dir, name);
		if (emit_image(opt, base, frames[0].rgba, frames[0].w, frames[0].h)) {
			stats->sprites++;
		} else {
			stats->failures++;
		}
		return;
	}

	AeronCpuAtlas atlas = { 0 };
	AeronAtlasImage* images = NULL;
	if (build_atlas(frames, n, 1, &images, &atlas)) {
		char dir[1040];
		snprintf(dir, sizeof dir, "%s/atlas", bundle_dir);
		mkdir_p(dir);
		snprintf(base, sizeof base, "%s/%s", dir, name);
		int ok = emit_image(opt, base, atlas.pages[0].rgba, atlas.pages[0].width, atlas.pages[0].height);
		char yaml[1100];
		snprintf(yaml, sizeof yaml, "%s.yaml", base);
		const float inv_scale = opt->scale ? (1.0f / 4.5f) : 1.0f;
		ok &= emit_atlas_yaml(yaml, images, frames, NULL, n, &atlas,
							  (int)(atlas.pages[0].width * inv_scale + 0.5f),
							  (int)(atlas.pages[0].height * inv_scale + 0.5f));
		free(images);
		Aeron_AtlasBuildFree(&atlas);
		if (ok) {
			stats->atlases++;
		} else {
			stats->failures++;
		}
		return;
	}

	/* Frames too large to pack (room FLICs) — one file per frame. */
	char dir[1040];
	snprintf(dir, sizeof dir, "%s/atlas/%s", bundle_dir, name);
	mkdir_p(dir);
	int ok = 1;
	for (int i = 0; i < n; i++) {
		snprintf(base, sizeof base, "%s/frame_%02d", dir, i);
		ok &= emit_image(opt, base, frames[i].rgba, frames[i].w, frames[i].h);
	}
	if (ok) {
		stats->frame_dirs++;
	} else {
		stats->failures++;
	}
}

/* ---- named-resource pass (per source file) ---------------------------- */

/* Resources are keyed by source file because names such as "background" are
 * rebound by the frontend. Prefer the CBM cache, matching the game. */
static void bake_one_file(const BakeSpriteOptions* opt, AeronVfs* vfs, const char* dir_name,
						  const char* file_name, BakeStats* stats) {
	char src[512];
	snprintf(src, sizeof src, "ALLIANCE/FRONTRES/%s/%s", dir_name, file_name);

	char base[128];
	snprintf(base, sizeof base, "%s", file_name);
	char* dot = strrchr(base, '.');
	if (dot) {
		*dot = '\0';
	}
	lower_str(base);

	Xwa2dFrameSet decoded = { 0 };
	char error[256] = { 0 };
	const int ok = BakeSource_LoadFrontend(vfs, src, &decoded, error, sizeof error);
	if (!ok || decoded.count <= 0) {
		fprintf(stderr, "bake: decode failed: %s (%s)\n", src,
				error[0] ? error : "file read failed");
		Xwa2dFrameSet_Free(&decoded);
		stats->failures++;
		return;
	}

	char dir_lower[128];
	snprintf(dir_lower, sizeof dir_lower, "%s", dir_name);
	lower_str(dir_lower);
	char bundle_dir[1024];
	snprintf(bundle_dir, sizeof bundle_dir, "%s/frontres/%s", opt->out_root, dir_lower);

	BakeFrame* frames = (BakeFrame*)calloc((size_t)decoded.count, sizeof *frames);
	int        n      = 0;
	for (int f = 0; f < decoded.count; f++) {
		BakeFrame* bf = &frames[f];
		bf->rgba = decoded.frames[f].rgba;
		bf->w = decoded.frames[f].width;
		bf->h = decoded.frames[f].height;
		decoded.frames[f].rgba = NULL;
		if (!frame_upscale(opt, bf)) {
			free(bf->rgba);
			bf->rgba = NULL;
			stats->failures++;
			continue;
		}
		n++;
	}
	if (n > 0) {
		for (int f = 0; f < decoded.count; f++) {
			if (!frames[f].rgba) {
				frames[f].rgba = (uint8_t*)calloc(1, 4);
				frames[f].w = frames[f].h = 1;
				frames[f].classic_w = frames[f].classic_h = 1;
			}
		}
		emit_frames(opt, bundle_dir, base, frames, decoded.count, 0, stats);
	}
	frames_free(frames, decoded.count);
	Xwa2dFrameSet_Free(&decoded);
}

/* ---- DAT (resdata) pass ---------------------------------------------- */

/* Emit one DAT group as (possibly multi-page) atlas + id-keyed YAML.
 * Frames are DENSE (one per real sprite), each carrying its SPRITE ID
 * via the schema's optional `id` key — ids are sparse (frontend atlas
 * resources use base offsets like 4002); consumers look frames up
 * with Aeron_SpriteAtlasFindById. Content exceeding one atlas splits
 * across pages via the optional `page` key: page 0 = group_<g>.ktx2,
 * page N = group_<g>_p<N>.ktx2. */
static void emit_group_atlas(const BakeSpriteOptions* opt, const char* resdata_dir,
							 unsigned group, BakeFrame* frames, const uint16_t* ids, int n,
							 BakeStats* stats) {
	char dir[1040];
	snprintf(dir, sizeof dir, "%s/atlas", resdata_dir);
	mkdir_p(dir);
	AeronCpuAtlas atlas = { 0 };
	AeronAtlasImage* images = NULL;
	if (!build_atlas(frames, n, 0, &images, &atlas)) {
		fprintf(stderr, "bake: group %u could not be packed within the atlas cap\n", group);
		stats->failures++;
		return;
	}
	int ok = 1;
	for (int page = 0; page < atlas.page_count; page++) {
		char base[1100];
		if (page == 0) {
			snprintf(base, sizeof base, "%s/group_%u", dir, group);
		} else {
			snprintf(base, sizeof base, "%s/group_%u_p%d", dir, group, page);
		}
		ok &= emit_image(opt, base, atlas.pages[page].rgba, atlas.pages[page].width,
						 atlas.pages[page].height);
	}
	char yaml_path[1160];
	snprintf(yaml_path, sizeof yaml_path, "%s/group_%u.yaml", dir, group);
	const float inv_scale = opt->scale ? (1.0f / 4.5f) : 1.0f;
	ok &= emit_atlas_yaml(yaml_path, images, frames, ids, n, &atlas,
						  (int)(atlas.pages[0].width * inv_scale + 0.5f),
						  (int)(atlas.pages[0].height * inv_scale + 0.5f));
	const int page_count = atlas.page_count;
	free(images);
	Aeron_AtlasBuildFree(&atlas);
	if (ok) {
		stats->atlases++;
	} else {
		stats->failures++;
	}
	if (page_count > 1) {
		printf("  group %u split across %d pages\n", group, page_count);
	}
}

static void bake_dat_groups(const BakeSpriteOptions* opt, AeronVfs* vfs, BakeStats* stats) {
	char resdata_dir[1024];
	snprintf(resdata_dir, sizeof resdata_dir, "%s/resdata", opt->out_root);
	BakeDatCatalog catalog;
	char catalog_error[256] = { 0 };
	if (!BakeSource_InitDatCatalog(vfs, &catalog, catalog_error, sizeof catalog_error)) {
		fprintf(stderr, "bake: DAT catalog failed: %s\n", catalog_error);
		stats->failures++;
		return;
	}

	for (int group_index = 0; group_index < catalog.group_count; group_index++) {
		const uint16_t group = catalog.groups[group_index];
		if (opt->group_filter >= 0 && (int)group != opt->group_filter) {
			continue;
		}
		Xwa2dFrameSet decoded = { 0 };
		char error[256] = { 0 };
		const int decode_ok = BakeSource_LoadDatGroup(&catalog, group, &decoded, error, sizeof error);
		if (!decode_ok || decoded.count <= 0) {
			if (!decode_ok) {
				fprintf(stderr, "bake: DAT group %u decode failed: %s\n", group, error);
				stats->failures++;
			}
			Xwa2dFrameSet_Free(&decoded);
			continue;
		}

		BakeFrame* frames = (BakeFrame*)calloc((size_t)decoded.count, sizeof *frames);
		uint16_t* kept_ids = (uint16_t*)malloc((size_t)decoded.count * sizeof *kept_ids);
		if (!frames || !kept_ids) {
			free(frames);
			free(kept_ids);
			Xwa2dFrameSet_Free(&decoded);
			stats->failures++;
			continue;
		}
		int        n = 0;
		for (int i = 0; i < decoded.count; i++) {
			Xwa2dFrame* source = &decoded.frames[i];
			BakeFrame* bf = &frames[n];
			bf->rgba = source->rgba;
			bf->w = source->width;
			bf->h = source->height;
			bf->origin_x = source->anchor_x;
			bf->origin_y = source->anchor_y;
			source->rgba = NULL;
			if (!frame_upscale(opt, bf)) {
				free(bf->rgba);
				bf->rgba = NULL;
				stats->failures++;
				continue;
			}
			kept_ids[n] = (uint16_t)source->sprite_id;
			n++;
		}
		Xwa2dFrameSet_Free(&decoded);
		if (n == 0) {
			free(frames);
			free(kept_ids);
			continue;
		}

		printf("resdata group %u: %d sprites (ids %u..%u)\n", group, n, kept_ids[0],
			   kept_ids[n - 1]);
		emit_group_atlas(opt, resdata_dir, group, frames, kept_ids, n, stats);
		frames_free(frames, n);
		free(kept_ids);
	}
	BakeSource_FreeDatCatalog(&catalog);
}

/* ---- source-file discovery --------------------------------------------- */

#define BAKE_MAX_DIRS          64
#define BAKE_MAX_FILES_PER_DIR 512

typedef struct FileDiscovery {
	char dirs[BAKE_MAX_DIRS][256];
	int  dir_count;
	/* Files of the directory currently being scanned, deduped by base
	 * name (FOO.CBM is the cache of FOO.FLC/FOO.BMP — one asset). */
	char files[BAKE_MAX_FILES_PER_DIR][256];
	int  file_count;
} FileDiscovery;

static int glob_collect_dir(void* user, const AeronVfsEntry* e) {
	FileDiscovery* d = (FileDiscovery*)user;
	if (e->is_directory && d->dir_count < BAKE_MAX_DIRS) {
		snprintf(d->dirs[d->dir_count++], sizeof d->dirs[0], "%s", e->name);
	}
	return 1;
}

static int base_len(const char* name) {
	const char* dot = strrchr(name, '.');
	return dot ? (int)(dot - name) : (int)strlen(name);
}

static int glob_collect_image(void* user, const AeronVfsEntry* e) {
	FileDiscovery* d   = (FileDiscovery*)user;
	size_t         len = strlen(e->name);
	if (e->is_directory || len < 5 || d->file_count >= BAKE_MAX_FILES_PER_DIR) {
		return 1;
	}
	const char* ext = e->name + len - 4;
	if (strcasecmp(ext, ".CBM") != 0 && strcasecmp(ext, ".BMP") != 0 &&
		strcasecmp(ext, ".FLC") != 0) {
		return 1;
	}
	const int bl = base_len(e->name);
	for (int i = 0; i < d->file_count; i++) {
		if (base_len(d->files[i]) == bl && strncasecmp(d->files[i], e->name, (size_t)bl) == 0) {
			if (strcasecmp(ext, ".CBM") == 0)
				snprintf(d->files[i], sizeof d->files[i], "%s", e->name);
			return 1;
		}
	}
	snprintf(d->files[d->file_count++], sizeof d->files[0], "%s", e->name);
	return 1;
}

/* ---- entry ------------------------------------------------------------ */

int BakeSprites_Run(const BakeSpriteOptions* opt, AeronVfs* vfs, BakeStats* stats) {
	memset(stats, 0, sizeof *stats);

	FileDiscovery* disc = (FileDiscovery*)calloc(1, sizeof *disc);
	if (!disc) {
		stats->failures++;
		return 0;
	}
	if (!opt->dat_only) {
		AeronVfs_Glob(vfs, AERON_VFS_ROOT_ASSET, "ALLIANCE/FRONTRES", "*",
					  AERON_VFS_GLOB_DIRECTORIES | AERON_VFS_GLOB_CASE_INSENSITIVE,
					  glob_collect_dir, disc);
		printf("bake: %d frontres directories discovered\n", disc->dir_count);
	}

	char filter[256] = { 0 };
	if (opt->list_filter) {
		snprintf(filter, sizeof filter, "%s", opt->list_filter);
		lower_str(filter);
	}

	for (int i = 0; i < disc->dir_count; i++) {
		char sub[560];
		snprintf(sub, sizeof sub, "ALLIANCE/FRONTRES/%s", disc->dirs[i]);
		disc->file_count = 0;
		AeronVfs_Glob(vfs, AERON_VFS_ROOT_ASSET, sub, "*",
					  AERON_VFS_GLOB_FILES | AERON_VFS_GLOB_CASE_INSENSITIVE, glob_collect_image,
					  disc);
		printf("frontres/%s: %d source images\n", disc->dirs[i], disc->file_count);
		for (int f = 0; f < disc->file_count; f++) {
			if (filter[0]) {
				char key[560];
				snprintf(key, sizeof key, "%s/%s", disc->dirs[i], disc->files[f]);
				lower_str(key);
				if (!strstr(key, filter)) {
					continue;
				}
			}
			bake_one_file(opt, vfs, disc->dirs[i], disc->files[f], stats);
		}
	}
	free(disc);

	if (!opt->list_filter) {
		bake_dat_groups(opt, vfs, stats);
	}
	return stats->failures == 0;
}
