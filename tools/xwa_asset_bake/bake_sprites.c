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
 *   <out>/frontend/frontres/<dir>/sprites/<base>.{png,ktx2}        single frame
 *   <out>/frontend/frontres/<dir>/atlas/<base>.{png,ktx2,yaml}     packed frames
 *   <out>/frontend/frontres/<dir>/atlas/<base>/frame_NN.{png,ktx2} frames too
 *                                                                  big to pack (FLIC)
 *   <out>/resdata/atlas/group_<g>.{png,ktx2,yaml}                  DAT group
 *   <out>/resdata/atlas/group_<g>/frame_NN.{png,ktx2}              oversized
 *
 * The atlas files are in game units. Each frame's `size` is its classic
 * size, and its `offset` its DAT anchor. The frame index is the animation
 * frame, and a DAT group's frames also have their sprite `id`.
 */

#include "bake_sprites.h"
#include "bake_source.h"
#include "aeron/atlas_pack.h"
#include "xwa_2d.h"

#include "bc7_codec.h"
#include "ktx2_writer.h"
#include "png_write.h"
#include "tool_log.h"
#include "upscale.h"

#include "aeron/vfs.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

/* ---- small utilities ------------------------------------------------- */

static int make_dir(const char* path) {
#ifdef _WIN32
	return _mkdir(path);
#else
	return mkdir(path, 0755);
#endif
}

static int is_dir(const char* path) {
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* Creates the folder and its missing parents, and logs an error when it
 * can't. Paths can use '/' or '\'. The parents are made from the deepest one
 * that exists, so a drive such as "C:" is never made or checked: Windows
 * reports that "C:" isn't a folder. */
int BakeSprites_MakeDirs(const char* path) {
	if (is_dir(path)) {
		return 1;
	}
	char parent[1024];
	const int length = snprintf(parent, sizeof parent, "%s", path);
	if (length < 0 || length >= (int)sizeof parent) {
		ToolLog_Write(TOOL_LOG_ERROR, "The folder %s can't be created: its path is too long.", path);
		return 0;
	}
	/* The last separator, unless it follows a drive such as "C:". */
	char* last = NULL;
	for (char* p = parent + 1; *p; p++) {
		if ((*p == '/' || *p == '\\') && p[-1] != ':') {
			last = p;
		}
	}
	if (last) {
		*last = '\0';
		if (!BakeSprites_MakeDirs(parent)) {
			return 0;
		}
	}
	if (make_dir(path) != 0) {
		const int error = errno;
		if (!is_dir(path)) {
			ToolLog_Write(TOOL_LOG_ERROR, "The folder %s can't be created: %s.", path, strerror(error));
			return 0;
		}
	}
	return 1;
}

static void lower_str(char* s) {
	for (; *s; s++) {
		if (*s >= 'A' && *s <= 'Z') {
			*s += 'a' - 'A';
		}
	}
}

static void frames_free(BakeFrame* f, int n) {
	for (int i = 0; i < n; i++) {
		free(f[i].rgba);
	}
	free(f);
}

/* Write one RGBA image as PNG and/or KTX2 per options. */
int BakeSprites_EmitImage(const BakeSpriteOptions* opt, const char* base_path, const uint8_t* rgba,
					  int w, int h) {
	char path[1100];
	char err[256];
	int  ok = 1;
	if (opt->write_png) {
		snprintf(path, sizeof path, "%s.png", base_path);
		if (!write_png_rgba_atomic(path, w, h, rgba, err, sizeof err)) {
			ToolLog_Write(TOOL_LOG_ERROR, "  PNG write failed: %s (%s)", path, err);
			ok = 0;
		}
	}
	if (opt->write_ktx2) {
		snprintf(path, sizeof path, "%s.ktx2", base_path);
		if (!write_ktx2_bc7_with_generated_mips(path, w, h, rgba, opt->bc7_quality, KTX2_TF_SRGB,
												opt->zstd)) {
			ToolLog_Write(TOOL_LOG_ERROR, "  KTX2 write failed: %s", path);
			ok = 0;
		}
		/* Images rarely share blocks, and the thread's cache would keep the
		   blocks of every image until the program ends. */
		bc7_codec_clear_cache();
	}
	return ok;
}

int BakeSprites_BuildAtlas(const BakeFrame* frames, int count, int max_pages,
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

void BakeSprites_WriteNumberList(FILE* fp, const double* values, int count) {
	fputc('[', fp);
	for (int i = 0; i < count; ++i) {
		if (i)
			fputs(", ", fp);
		if (values[i] == floor(values[i]))
			fprintf(fp, "%.0f", values[i]);
		else
			fprintf(fp, "%.2f", values[i]);
	}
	fputc(']', fp);
}

/* Writes the atlas file `path`, which the game reads with
 * Aeron_SpriteAtlasLoad and, with `game_geometry`, with its geometry
 * reader. */
static int write_atlas_yaml(const char* path, const BakeAtlasHeader* header, const BakeFrame* frames,
							int count, const AeronAtlasImage* placed, const AeronCpuAtlas* pages) {
	FILE* fp = fopen(path, "w");
	if (!fp)
		return 0;
	if (header->comment)
		fputs(header->comment, fp);
	if (header->game_geometry)
		fputs("game_geometry: true\n", fp);
	if (header->game_units)
		fputs("units: game\n", fp);
	fprintf(fp, "atlas: {w: %d, h: %d}\n", pages->pages[0].width, pages->pages[0].height);
	if (header->classic_frames)
		fprintf(fp, "classic_frames: %d\n", header->classic_frames);
	if (header->fps)
		fprintf(fp, "fps: %d\n", header->fps);
	if (header->tint)
		fprintf(fp, "tint: %s\n", header->tint);
	if (header->has_draw_adjust) {
		fputs("draw_adjust: ", fp);
		BakeSprites_WriteNumberList(fp, header->draw_adjust, 4);
		fputc('\n', fp);
	}
	if (header->has_bounds) {
		fputs("bounds: ", fp);
		BakeSprites_WriteNumberList(fp, header->bounds, 4);
		fputc('\n', fp);
	}
	if (header->region_count) {
		fputs("regions:\n", fp);
		for (int i = 0; i < header->region_count; ++i) {
			fprintf(fp, "  - {type: %d, rect: ", header->regions[i].type);
			BakeSprites_WriteNumberList(fp, header->regions[i].rect, 4);
			fputs("}\n", fp);
		}
	}
	if (header->extra)
		fputs(header->extra, fp);
	fputs("frames:\n", fp);
	for (int i = 0; i < count; ++i) {
		const BakeFrame* f = &frames[i];
		fputs("  - {", fp);
		if (f->has_id)
			fprintf(fp, "id: %d, ", f->id);
		if (pages->page_count > 1)
			fprintf(fp, "page: %d, ", placed[i].page);
		fprintf(fp, "x: %d, y: %d, w: %d, h: %d, size: ", placed[i].x, placed[i].y, f->w, f->h);
		BakeSprites_WriteNumberList(fp, f->size, 2);
		if (f->offset[0] != 0.0 || f->offset[1] != 0.0) {
			fputs(", offset: ", fp);
			BakeSprites_WriteNumberList(fp, f->offset, 2);
		}
		if (f->has_bounds) {
			fputs(", bounds: ", fp);
			BakeSprites_WriteNumberList(fp, f->bounds, 4);
		}
		if (f->keep_aspect)
			fprintf(fp, ", keep_aspect: %s", f->keep_aspect);
		if (f->fit)
			fprintf(fp, ", fit: %s", f->fit);
		if (f->has_light)
			fprintf(fp, ", light: [%g, %g, %g]", f->light[0], f->light[1], f->light[2]);
		fputs("}\n", fp);
	}
	return fclose(fp) == 0;
}

int BakeSprites_WriteAtlasFiles(const BakeSpriteOptions* opt, const char* dir, const char* base,
								const BakeAtlasHeader* header, const BakeFrame* frames, int count,
								const AeronAtlasImage* placed, const AeronCpuAtlas* pages) {
	if (!BakeSprites_MakeDirs(dir))
		return 0;
	int ok = 1;
	for (int page = 0; page < pages->page_count; ++page) {
		char path[1200];
		if (page == 0)
			snprintf(path, sizeof path, "%s/%s", dir, base);
		else
			snprintf(path, sizeof path, "%s/%s_p%d", dir, base, page);
		ok &= BakeSprites_EmitImage(opt, path, pages->pages[page].rgba, pages->pages[page].width,
									pages->pages[page].height);
	}
	char yaml[1200];
	snprintf(yaml, sizeof yaml, "%s/%s.yaml", dir, base);
	return ok && write_atlas_yaml(yaml, header, frames, count, placed, pages);
}

int BakeSprites_PackAndWriteAtlas(const BakeSpriteOptions* opt, const char* dir, const char* base,
								  const BakeAtlasHeader* header, const BakeFrame* frames, int count) {
	AeronCpuAtlas pages = { 0 };
	AeronAtlasImage* placed = NULL;
	if (!BakeSprites_BuildAtlas(frames, count, 0, &placed, &pages)) {
		ToolLog_Write(TOOL_LOG_ERROR, "bake: %s/%s could not be packed", dir, base);
		return 0;
	}
	const int ok = BakeSprites_WriteAtlasFiles(opt, dir, base, header, frames, count, placed, &pages);
	if (pages.page_count > 1)
		ToolLog_Write(TOOL_LOG_DETAIL, "  %s split across %d pages", base, pages.page_count);
	free(placed);
	Aeron_AtlasBuildFree(&pages);
	return ok;
}

/* HD-upscale a decoded classic frame in place (unless disabled). */
static int frame_upscale(const BakeSpriteOptions* opt, BakeFrame* f) {
	f->size[0] = f->w;
	f->size[1] = f->h;
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
		if (!BakeSprites_MakeDirs(dir)) {
			stats->failures++;
			return;
		}
		snprintf(base, sizeof base, "%s/%s", dir, name);
		if (BakeSprites_EmitImage(opt, base, frames[0].rgba, frames[0].w, frames[0].h)) {
			stats->sprites++;
		} else {
			stats->failures++;
		}
		return;
	}

	AeronCpuAtlas atlas = { 0 };
	AeronAtlasImage* images = NULL;
	if (BakeSprites_BuildAtlas(frames, n, 1, &images, &atlas)) {
		const BakeAtlasHeader header = { .game_units = 1 };
		char dir[1040];
		snprintf(dir, sizeof dir, "%s/atlas", bundle_dir);
		const int ok = BakeSprites_WriteAtlasFiles(opt, dir, name, &header, frames, n, images, &atlas);
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
	if (!BakeSprites_MakeDirs(dir)) {
		stats->failures++;
		return;
	}
	int ok = 1;
	for (int i = 0; i < n; i++) {
		snprintf(base, sizeof base, "%s/frame_%02d", dir, i);
		ok &= BakeSprites_EmitImage(opt, base, frames[i].rgba, frames[i].w, frames[i].h);
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
	snprintf(src, sizeof src, "FRONTRES/%s/%s", dir_name, file_name);

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
		ToolLog_Write(TOOL_LOG_ERROR, "bake: decode failed: %s (%s)", src,
					  error[0] ? error : "file read failed");
		Xwa2dFrameSet_Free(&decoded);
		stats->failures++;
		return;
	}

	char dir_lower[128];
	snprintf(dir_lower, sizeof dir_lower, "%s", dir_name);
	lower_str(dir_lower);
	char bundle_dir[1024];
	snprintf(bundle_dir, sizeof bundle_dir, "%s/frontend/frontres/%s", opt->out_root, dir_lower);

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
				frames[f].size[0] = frames[f].size[1] = 1;
			}
		}
		emit_frames(opt, bundle_dir, base, frames, decoded.count, 0, stats);
	}
	frames_free(frames, decoded.count);
	Xwa2dFrameSet_Free(&decoded);
}

/* ---- DAT (resdata) pass ---------------------------------------------- */

/* Writes one DAT group as resdata/atlas/group_<g>, with as many pages as
 * it needs. Each frame has its sprite id, which the game looks up with
 * Aeron_SpriteAtlasFindById. */
static void emit_group_atlas(const BakeSpriteOptions* opt, const char* resdata_dir, unsigned group,
							 const BakeFrame* frames, int n, BakeStats* stats) {
	const BakeAtlasHeader header = { .game_units = 1 };
	char dir[1040], base[64];
	snprintf(dir, sizeof dir, "%s/atlas", resdata_dir);
	snprintf(base, sizeof base, "group_%u", group);
	if (BakeSprites_PackAndWriteAtlas(opt, dir, base, &header, frames, n)) {
		stats->atlases++;
	} else {
		stats->failures++;
	}
}

static void bake_dat_groups(const BakeSpriteOptions* opt, AeronVfs* vfs, BakeStats* stats) {
	char resdata_dir[1024];
	snprintf(resdata_dir, sizeof resdata_dir, "%s/resdata", opt->out_root);
	BakeDatCatalog catalog;
	char catalog_error[256] = { 0 };
	if (!BakeSource_InitDatCatalog(vfs, &catalog, catalog_error, sizeof catalog_error)) {
		ToolLog_Write(TOOL_LOG_ERROR, "bake: DAT catalog failed: %s", catalog_error);
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
				ToolLog_Write(TOOL_LOG_ERROR, "bake: DAT group %u decode failed: %s", group, error);
				stats->failures++;
			}
			Xwa2dFrameSet_Free(&decoded);
			continue;
		}

		BakeFrame* frames = (BakeFrame*)calloc((size_t)decoded.count, sizeof *frames);
		if (!frames) {
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
			bf->has_id = 1;
			bf->id = source->sprite_id;
			bf->offset[0] = source->anchor_x;
			bf->offset[1] = source->anchor_y;
			source->rgba = NULL;
			if (!frame_upscale(opt, bf)) {
				free(bf->rgba);
				bf->rgba = NULL;
				stats->failures++;
				continue;
			}
			n++;
		}
		Xwa2dFrameSet_Free(&decoded);
		if (n == 0) {
			free(frames);
			continue;
		}

		ToolLog_Write(TOOL_LOG_INFO, "resdata group %u: %d sprites (ids %d..%d)", group, n, frames[0].id,
					  frames[n - 1].id);
		emit_group_atlas(opt, resdata_dir, group, frames, n, stats);
		frames_free(frames, n);
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
		AeronVfs_Glob(vfs, AERON_VFS_ROOT_ASSET, "FRONTRES", "*",
					  AERON_VFS_GLOB_DIRECTORIES | AERON_VFS_GLOB_CASE_INSENSITIVE,
					  glob_collect_dir, disc);
		ToolLog_Write(TOOL_LOG_INFO, "bake: %d frontres directories discovered", disc->dir_count);
	}

	char filter[256] = { 0 };
	if (opt->list_filter) {
		snprintf(filter, sizeof filter, "%s", opt->list_filter);
		lower_str(filter);
	}

	for (int i = 0; i < disc->dir_count; i++) {
		char sub[560];
		snprintf(sub, sizeof sub, "FRONTRES/%s", disc->dirs[i]);
		disc->file_count = 0;
		AeronVfs_Glob(vfs, AERON_VFS_ROOT_ASSET, sub, "*",
					  AERON_VFS_GLOB_FILES | AERON_VFS_GLOB_CASE_INSENSITIVE, glob_collect_image,
					  disc);
		ToolLog_Write(TOOL_LOG_INFO, "frontres/%s: %d source images", disc->dirs[i], disc->file_count);
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
