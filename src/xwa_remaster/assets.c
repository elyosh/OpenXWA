/*
 * XWA remaster snapshot-resident asset resolver — see xwa_remaster/assets.h.
 * Resolves authored remaster assets or independently decoded original files.
 * Recovered XWA resource state is used only for snapshot identities/lifetimes.
 */

#include "xwa_remaster/assets.h"

#include "aeron/aeron.h"
#include "aeron/image.h"
#include "aeron/log.h"
#include "aeron/time.h"
#include "aeron/scene/image_cache.h"
#include "aeron/scene/sprite_atlas.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_remaster/original_2d.h"
#include "xwa_remaster/runtime_atlas.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XWA_ASSETS_MAX_FONTS 16
#define XWA_ORIGINAL_FONT_SCALE 4
#define XWA_ASSETS_MAX_FILES 1024 /* == engine resource cap */
#define XWA_ASSETS_MAX_GROUPS XWA_SNAP_MAX_TEXTURE_ASSETS
#define XWA_ASSETS_KEY_MAX 40

static uint32_t assets_next_generation;

typedef struct FontSlot {
	int font_size;
	uint8_t source;       /* AssetSource */
	AeronFontAtlas atlas; /* loaded == 0 when the load failed */
} FontSlot;

typedef struct FlightFontSlot {
	uint8_t tried;
	uint8_t loaded;
	uint8_t source; /* AssetSource */
	XwaFlightFontRef ref;
} FlightFontSlot;

/* Classified layout form of one source-file key. Probed once (fopen),
 * negative results cached — per-frame lookups never touch the disk. */
typedef enum FileKind {
	FILE_UNPROBED = 0,
	FILE_MISSING,
	FILE_FAILED,
	FILE_SINGLE,   /* frontres/<dir>/sprites/<base>.ktx2 */
	FILE_ATLAS,    /* frontres/<dir>/atlas/<base>.{ktx2,yaml} */
	FILE_FRAMEDIR, /* frontres/<dir>/atlas/<base>/frame_NN.ktx2 */
} FileKind;

typedef enum AssetSource {
	ASSET_SOURCE_MISSING = 0,
	ASSET_SOURCE_REMASTERED,
	ASSET_SOURCE_ORIGINAL,
} AssetSource;

typedef enum AssetLoadStatus {
	ASSET_LOAD_SUCCESS = 0,
	ASSET_LOAD_MISSING,
	ASSET_LOAD_FAILED,
} AssetLoadStatus;

typedef struct FileSlot {
	char key[XWA_ASSETS_KEY_MAX];
	char source_file[XWA_SNAP_FRONTEND_SOURCE_MAX];
	uint8_t kind;            /* FileKind */
	uint8_t resident_source; /* AssetSource */
	uint16_t frontend_part_count;
	uint64_t frontend_seen_generation;
	AeronSpriteAtlas atlas; /* FILE_ATLAS only */
	XwaRuntimeAtlas original_atlas;
} FileSlot;

typedef struct GroupSlot {
	int16_t group;
	uint8_t remastered_status; /* AssetLoadStatus */
	uint8_t frontend_resident;
	uint8_t flight_resident;
	uint8_t frontend_source; /* AssetSource */
	uint8_t flight_source;   /* AssetSource */
	uint64_t frontend_seen_generation;
	uint64_t flight_seen_generation;
	AeronSpriteAtlas atlas;
	XwaRuntimeAtlas frontend_original_atlas;
	XwaRuntimeAtlas flight_original_atlas;
} GroupSlot;

struct XwaRemasterAssets {
	char root[512];
	int prefer_original_2d;
	XwaRemasterOriginal2d* original_reader;
	AeronImageCache* frontend_images;
	AeronImageCache* flight_images;
	FontSlot fonts[XWA_ASSETS_MAX_FONTS];
	int font_count;
	FileSlot files[XWA_ASSETS_MAX_FILES];
	int file_count;
	GroupSlot groups[XWA_ASSETS_MAX_GROUPS];
	int group_count;
	uint64_t frontend_asset_generation;
	uint8_t frontend_fonts_prepared;
	FlightFontSlot flight_fonts[3];
	uint64_t texture_asset_generation;
	uint32_t generation;
};

XwaRemasterAssets* XwaRemasterAssets_Create(const char* root, int prefer_original_2d) {
	XwaRemasterAssets* a = (XwaRemasterAssets*)calloc(1, sizeof *a);
	if (!a) {
		return NULL;
	}
	snprintf(a->root, sizeof a->root, "%s", root ? root : "");
	a->prefer_original_2d = prefer_original_2d != 0;
	a->original_reader = XwaRemasterOriginal2d_Create(Aeron_GetVfs());
	a->frontend_images = Aeron_ImageCacheCreate();
	a->flight_images = Aeron_ImageCacheCreate();
	if (!a->frontend_images || !a->flight_images || !a->original_reader) {
		Aeron_ImageCacheDestroy(a->frontend_images);
		Aeron_ImageCacheDestroy(a->flight_images);
		XwaRemasterOriginal2d_Destroy(a->original_reader);
		free(a);
		return NULL;
	}
	a->frontend_asset_generation = UINT64_MAX;
	a->texture_asset_generation = UINT64_MAX;
	a->generation = ++assets_next_generation;
	if (!a->generation)
		a->generation = ++assets_next_generation;
	Aeron_Log("xwa.remaster", "2D asset policy: prefer=%s alternate=%s",
			  a->prefer_original_2d ? "original" : "remastered",
			  a->prefer_original_2d ? "remastered" : "original");
	return a;
}

void XwaRemasterAssets_Destroy(XwaRemasterAssets* a) {
	if (!a) {
		return;
	}
	for (int i = 0; i < a->font_count; i++) {
		if (a->fonts[i].atlas.loaded) {
			AeronFontAtlas_Release(&a->fonts[i].atlas);
		}
	}
	for (int i = 0; i < a->file_count; i++) {
		XwaRuntimeAtlas_Free(&a->files[i].original_atlas);
		if (a->files[i].kind == FILE_ATLAS) {
			Aeron_SpriteAtlasFree(&a->files[i].atlas);
		}
	}
	for (int i = 0; i < a->group_count; i++) {
		XwaRuntimeAtlas_Free(&a->groups[i].frontend_original_atlas);
		XwaRuntimeAtlas_Free(&a->groups[i].flight_original_atlas);
		if (a->groups[i].remastered_status == ASSET_LOAD_SUCCESS) {
			Aeron_SpriteAtlasFree(&a->groups[i].atlas);
		}
	}
	for (int i = 0; i < 3; i++) {
		if (a->flight_fonts[i].source == ASSET_SOURCE_ORIGINAL) {
			Aeron_DestroyTexture(a->flight_fonts[i].ref.texture);
		}
		free(a->flight_fonts[i].ref.glyphs);
	}
	if (a->frontend_images) {
		Aeron_ImageCacheDestroy(a->frontend_images);
	}
	if (a->flight_images) {
		Aeron_ImageCacheDestroy(a->flight_images);
	}
	XwaRemasterOriginal2d_Destroy(a->original_reader);
	free(a);
}

const char* XwaRemasterAssets_Root(const XwaRemasterAssets* a) { return a ? a->root : ""; }

static AssetLoadStatus assets_probe_file(const char* path) {
	FILE* f = fopen(path, "rb");
	if (!f) {
		return errno == ENOENT || errno == ENOTDIR ? ASSET_LOAD_MISSING
												   : ASSET_LOAD_FAILED;
	}
	fclose(f);
	return ASSET_LOAD_SUCCESS;
}

static int assets_texture_ref(const AeronImageCacheEntry* e, XwaAssetRef* out) {
	if (!e || !e->tex) {
		return 0;
	}
	out->texture = e->tex;
	out->u0 = 0.0f;
	out->v0 = 0.0f;
	out->u1 = 1.0f;
	out->v1 = 1.0f;
	out->w = e->w;
	out->h = e->h;
	return 1;
}

static AssetLoadStatus assets_load_texture(AeronImageCache* images, AeronCommandBuffer* cmd,
										   const char* path, XwaAssetRef* out) {
	return assets_texture_ref(Aeron_ImageCacheLoad(images, cmd, path), out)
			   ? ASSET_LOAD_SUCCESS
			   : ASSET_LOAD_FAILED;
}

static int assets_find_texture(AeronImageCache* images, const char* path, XwaAssetRef* out) {
	return assets_texture_ref(Aeron_ImageCacheLoad(images, NULL, path), out);
}

static const char* assets_source_name(int source) {
	return source == ASSET_SOURCE_REMASTERED ? "remastered"
		   : source == ASSET_SOURCE_ORIGINAL ? "original"
											 : "missing";
}

static AssetLoadStatus
assets_original_status(XwaRemasterOriginal2dLoadStatus status) {
	switch (status) {
		case XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS:
			return ASSET_LOAD_SUCCESS;
		case XWA_REMASTER_ORIGINAL_2D_LOAD_MISSING:
			return ASSET_LOAD_MISSING;
		default:
			return ASSET_LOAD_FAILED;
	}
}

/* Narrow *out (a loaded page texture with full UVs and texture dims
 * in w/h) to one frame of the atlas layout. Frame rects are in the
 * containing PAGE's pixel space, so normalization uses the loaded
 * texture's own dims — correct for every page of a multi-page atlas
 * (the YAML `atlas:` dims only describe page 0). */
static int apply_atlas_frame(const AeronSpriteAtlas* atlas, int frame, XwaAssetRef* out) {
	if (frame < 0 || frame >= atlas->frame_count || out->w <= 0 || out->h <= 0) {
		return 0;
	}
	const AeronSpriteRect* r = &atlas->frames[frame];
	const float aw = (float)out->w;
	const float ah = (float)out->h;
	out->u0 = r->x / aw;
	out->v0 = r->y / ah;
	out->u1 = (r->x + r->w) / aw;
	out->v1 = (r->y + r->h) / ah;
	out->w = (int)(r->w + 0.5f);
	out->h = (int)(r->h + 0.5f);
	/* Classic dims: the per-frame YAML key is authoritative (each
	 * frame's true original size — required once hand-authored frames
	 * of arbitrary resolution mix into an atlas); the atlas-level
	 * classic ratio covers older YAMLs (uniform machine upscale). */
	if (atlas->classic_w && atlas->classic_h && atlas->classic_w[frame] > 0) {
		out->classic_w = atlas->classic_w[frame];
		out->classic_h = atlas->classic_h[frame];
	} else if (atlas->classic_atlas_w > 0 && atlas->atlas_w > 0) {
		const float inv = (float)atlas->classic_atlas_w / (float)atlas->atlas_w;
		out->classic_w = (int)(r->w * inv + 0.5f);
		out->classic_h = (int)(r->h * inv + 0.5f);
	} else {
		out->classic_w = out->w;
		out->classic_h = out->h;
	}
	return 1;
}

static int assets_runtime_frame(const XwaRuntimeAtlas* atlas, int frame, XwaAssetRef* out) {
	if (!atlas || !out || frame < 0 || frame >= atlas->layout.frame_count)
		return 0;
	const int page_index = atlas->layout.pages ? atlas->layout.pages[frame] : 0;
	if (page_index < 0 || page_index >= atlas->layout.page_count)
		return 0;
	const XwaRuntimeAtlasPage* page = &atlas->pages[page_index];
	if (!page->texture || page->width <= 0 || page->height <= 0)
		return 0;
	out->texture = page->texture;
	out->u0 = out->v0 = 0.0f;
	out->u1 = out->v1 = 1.0f;
	out->w = page->width;
	out->h = page->height;
	return apply_atlas_frame(&atlas->layout, frame, out);
}

static FileSlot* file_classify(XwaRemasterAssets* a, const char* key) {
	for (int i = 0; i < a->file_count; i++) {
		if (strncmp(a->files[i].key, key, sizeof a->files[i].key) == 0) {
			return &a->files[i];
		}
	}
	if (a->file_count >= XWA_ASSETS_MAX_FILES) {
		return NULL;
	}
	FileSlot* s = &a->files[a->file_count++];
	memset(s, 0, sizeof *s);
	snprintf(s->key, sizeof s->key, "%s", key);

	/* Split "<dir>/<base>" for path building. */
	char path[768];
	snprintf(path, sizeof path, "%s/frontres/", a->root);
	size_t pfx = strlen(path);
	const char* slash = strchr(key, '/');
	const char* base = slash ? slash + 1 : key;
	const int dlen = slash ? (int)(slash - key) : 0;
	snprintf(path + pfx, sizeof path - pfx, "%.*s/sprites/%s.ktx2", dlen, key, base);
	AssetLoadStatus probe = assets_probe_file(path);
	if (probe == ASSET_LOAD_SUCCESS) {
		s->kind = FILE_SINGLE;
		return s;
	}
	if (probe == ASSET_LOAD_FAILED) {
		s->kind = FILE_FAILED;
		return s;
	}
	snprintf(path + pfx, sizeof path - pfx, "%.*s/atlas/%s.yaml", dlen, key, base);
	probe = assets_probe_file(path);
	if (probe == ASSET_LOAD_SUCCESS) {
		s->kind = Aeron_SpriteAtlasLoad(&s->atlas, path) ? FILE_ATLAS : FILE_FAILED;
		return s;
	}
	if (probe == ASSET_LOAD_FAILED) {
		s->kind = FILE_FAILED;
		return s;
	}
	snprintf(path + pfx, sizeof path - pfx, "%.*s/atlas/%s/frame_00.ktx2", dlen, key, base);
	probe = assets_probe_file(path);
	if (probe == ASSET_LOAD_SUCCESS) {
		s->kind = FILE_FRAMEDIR;
		return s;
	}
	s->kind = probe == ASSET_LOAD_MISSING ? FILE_MISSING : FILE_FAILED;
	return s;
}

static FileSlot* file_find(XwaRemasterAssets* a, const char* key) {
	for (int i = 0; i < a->file_count; i++) {
		if (strncmp(a->files[i].key, key, sizeof a->files[i].key) == 0) {
			return &a->files[i];
		}
	}
	return NULL;
}

static void assets_file_texture_path(const XwaRemasterAssets* a, const FileSlot* slot, int part,
									 char path[768]) {
	const char* slash = strchr(slot->key, '/');
	const char* base = slash ? slash + 1 : slot->key;
	const int dlen = slash ? (int)(slash - slot->key) : 0;
	if (slot->kind == FILE_SINGLE) {
		snprintf(path, 768, "%s/frontres/%.*s/sprites/%s.ktx2", a->root, dlen, slot->key, base);
	} else if (slot->kind == FILE_ATLAS) {
		snprintf(path, 768, "%s/frontres/%.*s/atlas/%s.ktx2", a->root, dlen, slot->key, base);
	} else {
		snprintf(path, 768, "%s/frontres/%.*s/atlas/%s/frame_%02d.ktx2", a->root, dlen, slot->key, base,
				 part);
	}
}

static GroupSlot* assets_group_slot(XwaRemasterAssets* a, int group) {
	for (int i = 0; i < a->group_count; i++) {
		if (a->groups[i].group == (int16_t)group) {
			return &a->groups[i];
		}
	}
	if (a->group_count >= XWA_ASSETS_MAX_GROUPS) {
		return NULL;
	}
	GroupSlot* s = &a->groups[a->group_count++];
	memset(s, 0, sizeof *s);
	s->group = (int16_t)group;
	char probe[768];
	snprintf(probe, sizeof probe, "%s/resdata/atlas/group_%d.yaml", a->root, group);
	const AssetLoadStatus status = assets_probe_file(probe);
	if (status == ASSET_LOAD_SUCCESS) {
		s->remastered_status =
			Aeron_SpriteAtlasLoad(&s->atlas, probe) ? ASSET_LOAD_SUCCESS : ASSET_LOAD_FAILED;
	} else {
		s->remastered_status = (uint8_t)status;
	}
	return s;
}

static GroupSlot* assets_group_find(XwaRemasterAssets* a, int group) {
	for (int i = 0; i < a->group_count; i++) {
		if (a->groups[i].group == (int16_t)group) {
			return &a->groups[i];
		}
	}
	return NULL;
}

static void assets_group_page_path(const XwaRemasterAssets* a, int group, int page, char path[768]) {
	if (page == 0) {
		snprintf(path, 768, "%s/resdata/atlas/group_%d.ktx2", a->root, group);
	} else {
		snprintf(path, 768, "%s/resdata/atlas/group_%d_p%d.ktx2", a->root, group, page);
	}
}

static int assets_frontend_group_frame(XwaRemasterAssets* a, GroupSlot* s, int frame, XwaAssetRef* out) {
	const int page = s->atlas.pages ? s->atlas.pages[frame] : 0;
	char path[768];
	assets_group_page_path(a, s->group, page, path);
	return assets_find_texture(a->frontend_images, path, out) && apply_atlas_frame(&s->atlas, frame, out);
}

static int assets_flight_group_frame(XwaRemasterAssets* a, GroupSlot* s, int frame, XwaAssetRef* out) {
	const int page = s->atlas.pages ? s->atlas.pages[frame] : 0;
	char path[768];
	assets_group_page_path(a, s->group, page, path);
	return assets_find_texture(a->flight_images, path, out) && apply_atlas_frame(&s->atlas, frame, out);
}

static AssetLoadStatus assets_load_remastered_group(XwaRemasterAssets* a, AeronImageCache* images,
													 AeronCommandBuffer* cmd, GroupSlot* slot) {
	if (slot->remastered_status != ASSET_LOAD_SUCCESS)
		return (AssetLoadStatus)slot->remastered_status;
	const int page_count = slot->atlas.page_count > 0 ? slot->atlas.page_count : 1;
	for (int page = 0; page < page_count; page++) {
		char path[768];
		XwaAssetRef ref;
		assets_group_page_path(a, slot->group, page, path);
		AssetLoadStatus status = assets_load_texture(images, cmd, path, &ref);
		if (status == ASSET_LOAD_SUCCESS)
			continue;
		for (int loaded = 0; loaded < page; loaded++) {
			assets_group_page_path(a, slot->group, loaded, path);
			Aeron_ImageCacheInvalidate(images, path);
		}
		return ASSET_LOAD_FAILED;
	}
	return ASSET_LOAD_SUCCESS;
}

static AssetLoadStatus assets_load_original_group(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
												  GroupSlot* slot, int flight) {
	Xwa2dFrameSet frames = { 0 };
	char error[256] = { 0 };
	const uint64_t start_us = Aeron_NowUs();
	const AssetLoadStatus status =
		assets_original_status(XwaRemasterOriginal2d_LoadDatGroup(
			a->original_reader, slot->group, &frames, error, sizeof error));
	if (status != ASSET_LOAD_SUCCESS) {
		if (status != ASSET_LOAD_MISSING)
			Aeron_Log("xwa.remaster", "2D group %d: original load failed: %s", slot->group, error);
		return status;
	}
	XwaRuntimeAtlas* atlas = flight ? &slot->flight_original_atlas : &slot->frontend_original_atlas;
	const int loaded = XwaRuntimeAtlas_Build(atlas, cmd, &frames, flight, "XWA original DAT group");
	Xwa2dFrameSet_Free(&frames);
	if (loaded) {
		Aeron_Log("xwa.remaster", "2D group %d: source=original pages=%d time_us=%llu", slot->group,
				  atlas->layout.page_count, (unsigned long long)(Aeron_NowUs() - start_us));
	}
	return loaded ? ASSET_LOAD_SUCCESS : ASSET_LOAD_FAILED;
}

static AssetLoadStatus assets_choose_group(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
										   GroupSlot* slot, int flight, uint8_t* out_source) {
	*out_source = ASSET_SOURCE_MISSING;
	const int first = a->prefer_original_2d ? ASSET_SOURCE_ORIGINAL : ASSET_SOURCE_REMASTERED;
	for (int attempt = 0; attempt < 2; attempt++) {
		const int source =
			attempt == 0 ? first
						 : (first == ASSET_SOURCE_ORIGINAL ? ASSET_SOURCE_REMASTERED : ASSET_SOURCE_ORIGINAL);
		const AssetLoadStatus status =
			source == ASSET_SOURCE_ORIGINAL
				? assets_load_original_group(a, cmd, slot, flight)
				: assets_load_remastered_group(a, flight ? a->flight_images : a->frontend_images, cmd, slot);
		if (status == ASSET_LOAD_SUCCESS) {
			*out_source = (uint8_t)source;
			return ASSET_LOAD_SUCCESS;
		}
		if (status != ASSET_LOAD_MISSING)
			return status;
	}
	return ASSET_LOAD_MISSING;
}

static void assets_release_group(XwaRemasterAssets* a, GroupSlot* slot, int flight) {
	uint8_t* source = flight ? &slot->flight_source : &slot->frontend_source;
	const int is_resident = flight ? slot->flight_resident : slot->frontend_resident;
	if (!is_resident)
		return;
	if (*source == ASSET_SOURCE_REMASTERED) {
		AeronImageCache* images = flight ? a->flight_images : a->frontend_images;
		const int page_count = slot->atlas.page_count > 0 ? slot->atlas.page_count : 1;
		for (int page = 0; page < page_count; page++) {
			char path[768];
			assets_group_page_path(a, slot->group, page, path);
			Aeron_ImageCacheInvalidate(images, path);
		}
	} else if (*source == ASSET_SOURCE_ORIGINAL) {
		XwaRuntimeAtlas_Free(flight ? &slot->flight_original_atlas : &slot->frontend_original_atlas);
	}
	if (flight)
		slot->flight_resident = 0;
	else
		slot->frontend_resident = 0;
	*source = ASSET_SOURCE_MISSING;
}

static AssetLoadStatus assets_sync_frontend_file(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
												 FileSlot* slot, uint16_t desired_parts) {
	char path[768];
	while (slot->frontend_part_count > desired_parts) {
		slot->frontend_part_count--;
		assets_file_texture_path(a, slot, slot->frontend_part_count, path);
		Aeron_ImageCacheInvalidate(a->frontend_images, path);
	}
	while (slot->frontend_part_count < desired_parts) {
		assets_file_texture_path(a, slot, slot->frontend_part_count, path);
		XwaAssetRef ref;
		const AssetLoadStatus status =
			assets_load_texture(a->frontend_images, cmd, path, &ref);
		if (status != ASSET_LOAD_SUCCESS) {
			while (slot->frontend_part_count) {
				slot->frontend_part_count--;
				assets_file_texture_path(a, slot, slot->frontend_part_count, path);
				Aeron_ImageCacheInvalidate(a->frontend_images, path);
			}
			return status;
		}
		slot->frontend_part_count++;
	}
	return ASSET_LOAD_SUCCESS;
}

static AssetLoadStatus assets_load_original_file(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
												 FileSlot* slot) {
	Xwa2dFrameSet frames = { 0 };
	char error[256] = { 0 };
	const uint64_t start_us = Aeron_NowUs();
	const AssetLoadStatus status =
		assets_original_status(XwaRemasterOriginal2d_LoadFrontend(
			a->original_reader, slot->source_file, &frames, error, sizeof error));
	if (status != ASSET_LOAD_SUCCESS) {
		if (status != ASSET_LOAD_MISSING)
			Aeron_Log("xwa.remaster", "2D file '%s': original load failed: %s", slot->key, error);
		return status;
	}
	const int loaded = XwaRuntimeAtlas_Build(&slot->original_atlas, cmd, &frames, 0, slot->source_file);
	Xwa2dFrameSet_Free(&frames);
	if (loaded) {
		Aeron_Log("xwa.remaster", "2D file '%s': source=original pages=%d time_us=%llu", slot->key,
				  slot->original_atlas.layout.page_count, (unsigned long long)(Aeron_NowUs() - start_us));
	}
	return loaded ? ASSET_LOAD_SUCCESS : ASSET_LOAD_FAILED;
}

static AssetLoadStatus assets_load_remastered_file(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
												   FileSlot* slot, uint16_t frame_count) {
	if (slot->kind == FILE_MISSING || slot->kind == FILE_UNPROBED)
		return ASSET_LOAD_MISSING;
	if (slot->kind == FILE_FAILED)
		return ASSET_LOAD_FAILED;
	const uint16_t parts = slot->kind == FILE_FRAMEDIR ? frame_count : 1;
	const AssetLoadStatus status = assets_sync_frontend_file(a, cmd, slot, parts);
	return status == ASSET_LOAD_SUCCESS ? ASSET_LOAD_SUCCESS : ASSET_LOAD_FAILED;
}

static AssetLoadStatus assets_choose_frontend_file(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
												   FileSlot* slot, uint16_t frame_count,
												   uint8_t* out_source) {
	*out_source = ASSET_SOURCE_MISSING;
	const int first = a->prefer_original_2d ? ASSET_SOURCE_ORIGINAL : ASSET_SOURCE_REMASTERED;
	for (int attempt = 0; attempt < 2; attempt++) {
		const int source =
			attempt == 0 ? first
						 : (first == ASSET_SOURCE_ORIGINAL ? ASSET_SOURCE_REMASTERED : ASSET_SOURCE_ORIGINAL);
		const AssetLoadStatus status =
			source == ASSET_SOURCE_ORIGINAL ? assets_load_original_file(a, cmd, slot)
											: assets_load_remastered_file(a, cmd, slot, frame_count);
		if (status == ASSET_LOAD_SUCCESS) {
			*out_source = (uint8_t)source;
			return ASSET_LOAD_SUCCESS;
		}
		if (status != ASSET_LOAD_MISSING)
			return status;
	}
	return ASSET_LOAD_MISSING;
}

static void assets_release_frontend_group(XwaRemasterAssets* a, GroupSlot* slot) {
	assets_release_group(a, slot, 0);
}

static int assets_prepare_frontend_fonts(XwaRemasterAssets* a, AeronCommandBuffer* cmd);

int XwaRemasterAssets_FrontendAssetsNeedSync(const XwaRemasterAssets* a, const XwaSnapshot* snapshot) {
	return a && snapshot && a->frontend_asset_generation != snapshot->frontend_asset_generation;
}

int XwaRemasterAssets_SyncFrontendAssets(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
										 const XwaSnapshot* snapshot) {
	if (!a || !cmd || !snapshot) {
		return 0;
	}
	if (!XwaRemasterAssets_FrontendAssetsNeedSync(a, snapshot))
		return 1;

	uint32_t file_count = snapshot->frontend_file_count;
	if (file_count > XWA_SNAP_MAX_FRONTEND_FILES) {
		file_count = XWA_SNAP_MAX_FRONTEND_FILES;
	}
	for (uint32_t i = 0; i < file_count; i++) {
		const XwaFrontendFileAsset* asset = &snapshot->frontend_files[i];
		FileSlot* slot = file_classify(a, asset->file);
		if (!slot)
			return 0;
		slot->frontend_seen_generation = snapshot->frontend_asset_generation;
		if (strcmp(slot->source_file, asset->source_file) != 0) {
			assets_sync_frontend_file(a, cmd, slot, 0);
			XwaRuntimeAtlas_Free(&slot->original_atlas);
			slot->resident_source = ASSET_SOURCE_MISSING;
			snprintf(slot->source_file, sizeof slot->source_file, "%s", asset->source_file);
		}
		if (slot->resident_source == ASSET_SOURCE_MISSING) {
			uint8_t source;
			const AssetLoadStatus status =
				assets_choose_frontend_file(a, cmd, slot, asset->frame_count, &source);
			if (status != ASSET_LOAD_SUCCESS && status != ASSET_LOAD_MISSING) {
				Aeron_Log("xwa.remaster", "2D file '%s': load failed", slot->key);
				return 0;
			}
			slot->resident_source = source;
			Aeron_Log("xwa.remaster", "2D file '%s': source=%s", slot->key,
					  assets_source_name(slot->resident_source));
		}
	}

	uint32_t group_count = snapshot->frontend_group_count;
	if (group_count > XWA_SNAP_MAX_SPRITE_GROUPS) {
		group_count = XWA_SNAP_MAX_SPRITE_GROUPS;
	}
	for (uint32_t i = 0; i < group_count; i++) {
		GroupSlot* slot = assets_group_slot(a, snapshot->frontend_groups[i].group);
		if (!slot)
			return 0;
		slot->frontend_seen_generation = snapshot->frontend_asset_generation;
		if (!slot->frontend_resident) {
			const AssetLoadStatus status =
				assets_choose_group(a, cmd, slot, 0, &slot->frontend_source);
			if (status != ASSET_LOAD_SUCCESS && status != ASSET_LOAD_MISSING) {
				Aeron_Log("xwa.remaster", "frontend group %d: load failed", slot->group);
				return 0;
			}
			slot->frontend_resident = 1;
			Aeron_Log("xwa.remaster", "frontend group %d: source=%s", slot->group,
					  assets_source_name(slot->frontend_source));
		}
	}

	uint32_t texture_count = 0;
	for (int i = 0; i < a->file_count; i++) {
		FileSlot* slot = &a->files[i];
		if (slot->resident_source != ASSET_SOURCE_MISSING &&
			slot->frontend_seen_generation != snapshot->frontend_asset_generation) {
			(void)assets_sync_frontend_file(a, cmd, slot, 0);
			XwaRuntimeAtlas_Free(&slot->original_atlas);
			slot->resident_source = ASSET_SOURCE_MISSING;
		}
		texture_count += slot->resident_source == ASSET_SOURCE_ORIGINAL
							 ? (uint32_t)slot->original_atlas.layout.page_count
							 : slot->frontend_part_count;
	}
	for (int i = 0; i < a->group_count; i++) {
		GroupSlot* slot = &a->groups[i];
		if (slot->frontend_resident &&
			slot->frontend_seen_generation != snapshot->frontend_asset_generation) {
			assets_release_frontend_group(a, slot);
		}
		if (slot->frontend_resident) {
			texture_count += slot->frontend_source == ASSET_SOURCE_ORIGINAL
								 ? (uint32_t)slot->frontend_original_atlas.layout.page_count
								 : (slot->frontend_source == ASSET_SOURCE_REMASTERED
										? (slot->atlas.page_count > 0 ? (uint32_t)slot->atlas.page_count : 1)
										: 0);
		}
	}

	if (!assets_prepare_frontend_fonts(a, cmd))
		return 0;
	Aeron_Log("xwa.remaster", "frontend assets prepared: generation=%llu files=%u groups=%u textures=%u",
			  (unsigned long long)snapshot->frontend_asset_generation, file_count, group_count,
			  texture_count);
	return 1;
}

int XwaRemasterAssets_FrontendSprite(XwaRemasterAssets* a, const char* file_key, int frame,
									 XwaAssetRef* out) {
	if (!a || !out || !file_key || !file_key[0]) {
		return 0;
	}
	FileSlot* slot = file_find(a, file_key);
	if (!slot || slot->resident_source == ASSET_SOURCE_MISSING) {
		return 0;
	}
	if (slot->resident_source == ASSET_SOURCE_ORIGINAL) {
		return assets_runtime_frame(&slot->original_atlas, frame, out);
	}
	const int part = slot->kind == FILE_FRAMEDIR && frame >= 0 ? frame : 0;
	char path[768];
	assets_file_texture_path(a, slot, part, path);
	if (!assets_find_texture(a->frontend_images, path, out)) {
		return 0;
	}
	return slot->kind != FILE_ATLAS || apply_atlas_frame(&slot->atlas, frame, out);
}

int XwaRemasterAssets_FrontendAtlasSprite(XwaRemasterAssets* a, int group, int index, XwaAssetRef* out) {
	if (!a || !out) {
		return 0;
	}
	GroupSlot* slot = assets_group_find(a, group);
	if (!slot || !slot->frontend_resident || slot->frontend_source == ASSET_SOURCE_MISSING) {
		return 0;
	}
	if (slot->frontend_source == ASSET_SOURCE_ORIGINAL) {
		const int frame = Aeron_SpriteAtlasFindById(&slot->frontend_original_atlas.layout, index);
		return assets_runtime_frame(&slot->frontend_original_atlas, frame, out);
	}
	const int frame = Aeron_SpriteAtlasFindById(&slot->atlas, index);
	return frame >= 0 && assets_frontend_group_frame(a, slot, frame, out);
}

int XwaRemasterAssets_FlightTexturesNeedSync(const XwaRemasterAssets* a, const XwaSnapshot* snapshot) {
	return a && snapshot && a->texture_asset_generation != snapshot->texture_asset_generation;
}

int XwaRemasterAssets_SyncFlightTextures(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
										 const XwaSnapshot* snapshot) {
	if (!a || !cmd || !snapshot) {
		return 0;
	}
	if (!XwaRemasterAssets_FlightTexturesNeedSync(a, snapshot))
		return 1;

	uint32_t live_type_count = snapshot->texture_asset_count;
	if (live_type_count > XWA_SNAP_MAX_TEXTURE_ASSETS) {
		live_type_count = XWA_SNAP_MAX_TEXTURE_ASSETS;
	}
	for (uint32_t i = 0; i < live_type_count; i++) {
		int group = 0;
		int frames_mode = 0;
		int sprite_id = 0;
		if (!XwaSnapshotExport_ModelTextureBinding(snapshot->texture_assets[i].model_type, &group,
												   &frames_mode, &sprite_id)) {
			continue;
		}
		GroupSlot* slot = assets_group_slot(a, group);
		(void)frames_mode;
		(void)sprite_id;
		if (!slot)
			return 0;
		slot->flight_seen_generation = snapshot->texture_asset_generation;
		if (!slot->flight_resident) {
			const AssetLoadStatus status =
				assets_choose_group(a, cmd, slot, 1, &slot->flight_source);
			if (status != ASSET_LOAD_SUCCESS && status != ASSET_LOAD_MISSING) {
				Aeron_Log("xwa.remaster", "flight group %d: load failed", slot->group);
				return 0;
			}
			slot->flight_resident = 1;
			Aeron_Log("xwa.remaster", "flight group %d: source=%s", slot->group,
					  assets_source_name(slot->flight_source));
		}
	}

	uint32_t page_count = 0;
	for (int i = 0; i < a->group_count; i++) {
		GroupSlot* slot = &a->groups[i];
		if (slot->flight_resident && slot->flight_seen_generation != snapshot->texture_asset_generation) {
			assets_release_group(a, slot, 1);
		}
		if (slot->flight_source == ASSET_SOURCE_ORIGINAL) {
			page_count += (uint32_t)slot->flight_original_atlas.layout.page_count;
		} else if (slot->flight_source == ASSET_SOURCE_REMASTERED) {
			page_count += slot->atlas.page_count > 0 ? (uint32_t)slot->atlas.page_count : 1;
		}
	}

	Aeron_Log("xwa.remaster", "flight texture assets prepared: generation=%llu live_types=%u pages=%u",
			  (unsigned long long)snapshot->texture_asset_generation, live_type_count, page_count);
	return 1;
}

void XwaRemasterAssets_CommitFrontendAssets(XwaRemasterAssets* a, uint64_t generation) {
	if (!a) {
		return;
	}
	a->frontend_asset_generation = generation;
	Aeron_Log("xwa.remaster", "frontend assets committed: generation=%llu",
			  (unsigned long long)a->frontend_asset_generation);
}

void XwaRemasterAssets_CommitFlightTextures(XwaRemasterAssets* a, uint64_t generation) {
	if (!a) {
		return;
	}
	a->texture_asset_generation = generation;
	a->generation++;
	if (!a->generation) {
		a->generation++;
	}
	Aeron_Log("xwa.remaster", "flight texture assets committed: generation=%llu",
			  (unsigned long long)a->texture_asset_generation);
}

int XwaRemasterAssets_FlightAtlasFrame(XwaRemasterAssets* a, int group, int frame_index0, XwaAssetRef* out) {
	if (!a || !out || frame_index0 < 0) {
		return 0;
	}
	GroupSlot* slot = assets_group_find(a, group);
	if (!slot || !slot->flight_resident || slot->flight_source == ASSET_SOURCE_MISSING) {
		return 0;
	}
	if (slot->flight_source == ASSET_SOURCE_ORIGINAL) {
		return assets_runtime_frame(&slot->flight_original_atlas, frame_index0, out);
	}
	if (frame_index0 >= slot->atlas.frame_count)
		return 0;
	return assets_flight_group_frame(a, slot, frame_index0, out);
}

int XwaRemasterAssets_FlightModelFrame(XwaRemasterAssets* a, int object_type, int classic_frame_1based,
									   XwaAssetRef* out) {
	int group = 0;
	int frames_mode = 0;
	int sprite_id = 0;
	if (!a || !out || !XwaSnapshotExport_ModelTextureBinding(object_type, &group, &frames_mode, &sprite_id)) {
		return 0;
	}
	GroupSlot* slot = assets_group_find(a, group);
	if (!slot || !slot->flight_resident || slot->flight_source == ASSET_SOURCE_MISSING) {
		return 0;
	}
	if (slot->flight_source == ASSET_SOURCE_ORIGINAL) {
		const int frame = frames_mode
						  ? classic_frame_1based - 1
						  : Aeron_SpriteAtlasFindById(&slot->flight_original_atlas.layout, sprite_id);
		return assets_runtime_frame(&slot->flight_original_atlas, frame, out);
	}
	const int frame =
		frames_mode ? classic_frame_1based - 1 : Aeron_SpriteAtlasFindById(&slot->atlas, sprite_id);
	if (frame < 0 || frame >= slot->atlas.frame_count) {
		return 0;
	}
	return assets_flight_group_frame(a, slot, frame, out);
}

static uint16_t assets_rd_u16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static uint32_t assets_rd_u32(const uint8_t* p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint8_t* assets_read_file(const char* path, size_t* out_size, AssetLoadStatus* status) {
	FILE* f = fopen(path, "rb");
	if (!f) {
		*status = errno == ENOENT || errno == ENOTDIR ? ASSET_LOAD_MISSING
													  : ASSET_LOAD_FAILED;
		return NULL;
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		*status = ASSET_LOAD_FAILED;
		fclose(f);
		return NULL;
	}
	const long length = ftell(f);
	if (length < 0) {
		*status = ASSET_LOAD_FAILED;
		fclose(f);
		return NULL;
	}
	rewind(f);
	uint8_t* data = (uint8_t*)malloc(length > 0 ? (size_t)length : 1u);
	if (!data) {
		*status = ASSET_LOAD_FAILED;
		fclose(f);
		return NULL;
	}
	if (fread(data, 1, (size_t)length, f) != (size_t)length) {
		*status = ASSET_LOAD_FAILED;
		free(data);
		fclose(f);
		return NULL;
	}
	fclose(f);
	*out_size = (size_t)length;
	*status = ASSET_LOAD_SUCCESS;
	return data;
}

static AeronFontGlyph* assets_copy_font_glyphs(const Xwa2dFontAtlas* source, int atlas_scale) {
	if (!source || !source->glyphs || !source->glyph_count || atlas_scale <= 0)
		return NULL;
	AeronFontGlyph* glyphs = (AeronFontGlyph*)calloc(source->glyph_count, sizeof *glyphs);
	if (!glyphs)
		return NULL;
	for (uint16_t i = 0; i < source->glyph_count; i++) {
		if (source->glyphs[i].x > UINT16_MAX / atlas_scale ||
			source->glyphs[i].y > UINT16_MAX / atlas_scale ||
			source->glyphs[i].width > UINT16_MAX / atlas_scale ||
			source->glyphs[i].height > UINT16_MAX / atlas_scale) {
			free(glyphs);
			return NULL;
		}
		glyphs[i].atlas_x = (uint16_t)(source->glyphs[i].x * atlas_scale);
		glyphs[i].atlas_y = (uint16_t)(source->glyphs[i].y * atlas_scale);
		glyphs[i].atlas_w = (uint16_t)(source->glyphs[i].width * atlas_scale);
		glyphs[i].atlas_h = (uint16_t)(source->glyphs[i].height * atlas_scale);
		glyphs[i].advance = source->glyphs[i].advance;
	}
	return glyphs;
}

static int assets_upload_original_font(AeronCommandBuffer* cmd, const Xwa2dFontAtlas* font, int atlas_scale,
									   int generate_mips, const char* debug_name, AeronTexture** out_texture,
									   AeronFontGlyph** out_glyphs, int* out_width, int* out_height) {
	if (!cmd || !font || !out_texture || !out_glyphs || !out_width || !out_height)
		return 0;
	AeronFontGlyph* glyphs = assets_copy_font_glyphs(font, atlas_scale);
	int atlas_width = 0, atlas_height = 0;
	uint8_t* atlas_rgba = glyphs ? Aeron_ImageUpscaleNearestRgba8(font->rgba, font->width, font->height,
																  atlas_scale, &atlas_width, &atlas_height)
								 : NULL;
	AeronTexture* texture = atlas_rgba
								? XwaRuntimeTexture_UploadLinearRgba(cmd, atlas_rgba, atlas_width,
																	 atlas_height, generate_mips, debug_name)
								: NULL;
	free(atlas_rgba);
	if (!texture) {
		free(glyphs);
		return 0;
	}
	*out_texture = texture;
	*out_glyphs = glyphs;
	*out_width = atlas_width;
	*out_height = atlas_height;
	return 1;
}

static AssetLoadStatus assets_load_original_flight_font(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
														FlightFontSlot* slot, int tier) {
	Xwa2dFrameSet group = { 0 };
	Xwa2dFontAtlas font = { 0 };
	char error[256] = { 0 };
	AssetLoadStatus status =
		assets_original_status(XwaRemasterOriginal2d_LoadDatGroup(
			a->original_reader, 16000, &group, error, sizeof error));
	if (status != ASSET_LOAD_SUCCESS) {
		if (status != ASSET_LOAD_MISSING)
			Aeron_Log("xwa.remaster", "flight font tier %d: original load failed: %s", tier, error);
		return status;
	}
	if (!Xwa2d_BuildFlightFontTier(&group, tier, &font, error, sizeof error)) {
		Aeron_Log("xwa.remaster", "flight font tier %d: original data invalid: %s", tier, error);
		Xwa2dFrameSet_Free(&group);
		Xwa2dFontAtlas_Free(&font);
		return ASSET_LOAD_FAILED;
	}
	Xwa2dFrameSet_Free(&group);
	AeronFontGlyph* glyphs = NULL;
	AeronTexture* texture = NULL;
	int atlas_width = 0, atlas_height = 0;
	if (!assets_upload_original_font(cmd, &font, XWA_ORIGINAL_FONT_SCALE, 1, "XWA original flight font",
									 &texture, &glyphs, &atlas_width, &atlas_height)) {
		Xwa2dFontAtlas_Free(&font);
		return ASSET_LOAD_FAILED;
	}
	slot->ref.texture = texture;
	slot->ref.atlas_w = atlas_width;
	slot->ref.atlas_h = atlas_height;
	slot->ref.cell_w = (uint16_t)font.cell_width;
	slot->ref.cell_h = (uint16_t)font.cell_height;
	slot->ref.baseline = (uint16_t)font.baseline;
	slot->ref.first_char = font.first_char;
	slot->ref.num_chars = font.glyph_count;
	slot->ref.glyphs = glyphs;
	Xwa2dFontAtlas_Free(&font);
	return ASSET_LOAD_SUCCESS;
}

static AssetLoadStatus assets_load_remastered_flight_font(XwaRemasterAssets* a,
														  AeronCommandBuffer* cmd, int tier) {
	if (!a || tier < 0 || tier >= 3)
		return ASSET_LOAD_FAILED;
	FlightFontSlot* slot = &a->flight_fonts[tier];
	if (!cmd)
		return ASSET_LOAD_FAILED;
	char base[768], path[800];
	snprintf(base, sizeof base, "%s/flight/fonts/font_tier_%d", a->root, tier);
	snprintf(path, sizeof path, "%s.fnt", base);
	size_t size = 0;
	AssetLoadStatus status;
	uint8_t* fnt = assets_read_file(path, &size, &status);
	if (!fnt)
		return status;
	if (size < 24 || assets_rd_u32(fnt) != 0x544e4654u || assets_rd_u16(fnt + 4) != 2) {
		free(fnt);
		return ASSET_LOAD_FAILED;
	}
	const uint16_t count = assets_rd_u16(fnt + 8);
	if (!count || size != 24u + (size_t)count * 10u) {
		free(fnt);
		return ASSET_LOAD_FAILED;
	}
	AeronFontGlyph* glyphs = (AeronFontGlyph*)calloc(count, sizeof *glyphs);
	if (!glyphs) {
		free(fnt);
		return ASSET_LOAD_FAILED;
	}
	for (uint16_t i = 0; i < count; i++) {
		const uint8_t* r = fnt + 24 + (size_t)i * 10;
		glyphs[i].atlas_x = assets_rd_u16(r);
		glyphs[i].atlas_y = assets_rd_u16(r + 2);
		glyphs[i].atlas_w = assets_rd_u16(r + 4);
		glyphs[i].atlas_h = assets_rd_u16(r + 6);
		glyphs[i].advance = assets_rd_u16(r + 8);
	}
	slot->ref.atlas_w = assets_rd_u16(fnt + 10);
	slot->ref.atlas_h = assets_rd_u16(fnt + 12);
	slot->ref.cell_w = assets_rd_u16(fnt + 14);
	slot->ref.cell_h = assets_rd_u16(fnt + 16);
	slot->ref.baseline = assets_rd_u16(fnt + 18);
	slot->ref.first_char = assets_rd_u16(fnt + 6);
	slot->ref.num_chars = count;
	for (uint16_t i = 0; i < count; i++) {
		if ((uint32_t)glyphs[i].atlas_x + glyphs[i].atlas_w > (uint32_t)slot->ref.atlas_w ||
			(uint32_t)glyphs[i].atlas_y + glyphs[i].atlas_h > (uint32_t)slot->ref.atlas_h) {
			free(glyphs);
			free(fnt);
			memset(&slot->ref, 0, sizeof slot->ref);
			return ASSET_LOAD_FAILED;
		}
	}
	free(fnt);
	snprintf(path, sizeof path, "%s.ktx2", base);
	const AeronImageCacheEntry* image = Aeron_ImageCacheLoad(a->flight_images, cmd, path);
	if (!image || !image->tex || image->w != slot->ref.atlas_w || image->h != slot->ref.atlas_h) {
		free(glyphs);
		memset(&slot->ref, 0, sizeof slot->ref);
		return ASSET_LOAD_FAILED;
	}
	slot->ref.texture = image->tex;
	slot->ref.glyphs = glyphs;
	slot->loaded = 1;
	return ASSET_LOAD_SUCCESS;
}

static AssetLoadStatus assets_load_flight_font(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
											   int tier) {
	if (!a || !cmd || tier < 0 || tier >= 3)
		return ASSET_LOAD_FAILED;
	FlightFontSlot* slot = &a->flight_fonts[tier];
	if (slot->tried)
		return slot->loaded ? ASSET_LOAD_SUCCESS : ASSET_LOAD_FAILED;
	slot->tried = 1;
	const int first = a->prefer_original_2d ? ASSET_SOURCE_ORIGINAL : ASSET_SOURCE_REMASTERED;
	for (int attempt = 0; attempt < 2; attempt++) {
		const int source =
			attempt == 0 ? first
						 : (first == ASSET_SOURCE_ORIGINAL ? ASSET_SOURCE_REMASTERED : ASSET_SOURCE_ORIGINAL);
		const AssetLoadStatus status =
			source == ASSET_SOURCE_ORIGINAL ? assets_load_original_flight_font(a, cmd, slot, tier)
											: assets_load_remastered_flight_font(a, cmd, tier);
		if (status == ASSET_LOAD_SUCCESS) {
			slot->source = (uint8_t)source;
			slot->loaded = 1;
			Aeron_Log("xwa.remaster", "flight font tier %d: source=%s", tier, assets_source_name(source));
			return ASSET_LOAD_SUCCESS;
		}
		if (status != ASSET_LOAD_MISSING) {
			Aeron_Log("xwa.remaster", "flight font tier %d: load failed", tier);
			return status;
		}
	}
	Aeron_Log("xwa.remaster", "flight font tier %d: source=missing", tier);
	return ASSET_LOAD_MISSING;
}

int XwaRemasterAssets_PrepareFlightFonts(XwaRemasterAssets* a, AeronCommandBuffer* cmd) {
	if (!a || !cmd) {
		return 0;
	}
	for (int tier = 0; tier < 3; tier++) {
		if (assets_load_flight_font(a, cmd, tier) != ASSET_LOAD_SUCCESS)
			return 0;
	}
	return 1;
}

const XwaFlightFontRef* XwaRemasterAssets_FlightFont(XwaRemasterAssets* a, int tier) {
	if (!a || tier < 0 || tier >= 3) {
		return NULL;
	}
	const FlightFontSlot* slot = &a->flight_fonts[tier];
	return slot->loaded ? &slot->ref : NULL;
}

uint32_t XwaRemasterAssets_Generation(const XwaRemasterAssets* a) { return a ? a->generation : 0; }

static AssetLoadStatus assets_load_original_frontend_font(XwaRemasterAssets* a,
														  AeronCommandBuffer* cmd, FontSlot* slot,
														  int font_size) {
	Xwa2dFontAtlas font = { 0 };
	char error[256] = { 0 };
	const AssetLoadStatus status =
		assets_original_status(XwaRemasterOriginal2d_LoadFrontendFont(
			a->original_reader, font_size, &font, error, sizeof error));
	if (status != ASSET_LOAD_SUCCESS) {
		if (status != ASSET_LOAD_MISSING)
			Aeron_Log("xwa.remaster", "frontend font %d: original load failed: %s", font_size, error);
		return status;
	}
	AeronFontGlyph* glyphs = NULL;
	AeronTexture* texture = NULL;
	int atlas_width = 0, atlas_height = 0;
	if (!assets_upload_original_font(cmd, &font, XWA_ORIGINAL_FONT_SCALE, 0, "XWA original frontend font",
									 &texture, &glyphs, &atlas_width, &atlas_height)) {
		Xwa2dFontAtlas_Free(&font);
		return ASSET_LOAD_FAILED;
	}
	slot->atlas.texture = texture;
	slot->atlas.atlas_w = atlas_width;
	slot->atlas.atlas_h = atlas_height;
	slot->atlas.first_char = font.first_char;
	slot->atlas.num_chars = font.glyph_count;
	slot->atlas.cell_w = (uint16_t)(font.cell_width * XWA_ORIGINAL_FONT_SCALE);
	slot->atlas.cell_h = (uint16_t)(font.cell_height * XWA_ORIGINAL_FONT_SCALE);
	slot->atlas.baseline = (uint16_t)(font.baseline * XWA_ORIGINAL_FONT_SCALE);
	slot->atlas.glyphs = glyphs;
	slot->atlas.loaded = 1;
	Xwa2dFontAtlas_Free(&font);
	return ASSET_LOAD_SUCCESS;
}

static AssetLoadStatus assets_load_frontend_font(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
												 int font_size, const AeronFontAtlas** out) {
	*out = NULL;
	if (!a || a->root[0] == '\0') {
		return ASSET_LOAD_FAILED;
	}
	for (int i = 0; i < a->font_count; i++) {
		if (a->fonts[i].font_size == font_size) {
			if (a->fonts[i].atlas.loaded)
				*out = &a->fonts[i].atlas;
			return a->fonts[i].atlas.loaded ? ASSET_LOAD_SUCCESS : ASSET_LOAD_MISSING;
		}
	}
	if (a->font_count >= XWA_ASSETS_MAX_FONTS) {
		return ASSET_LOAD_FAILED;
	}
	FontSlot* slot = &a->fonts[a->font_count++];
	slot->font_size = font_size;
	const int first = a->prefer_original_2d ? ASSET_SOURCE_ORIGINAL : ASSET_SOURCE_REMASTERED;
	for (int attempt = 0; attempt < 2; attempt++) {
		const int source =
			attempt == 0 ? first
						 : (first == ASSET_SOURCE_ORIGINAL ? ASSET_SOURCE_REMASTERED : ASSET_SOURCE_ORIGINAL);
		AssetLoadStatus status;
		if (source == ASSET_SOURCE_ORIGINAL) {
			status = assets_load_original_frontend_font(a, cmd, slot, font_size);
		} else {
			char basename[640], path[672];
			snprintf(basename, sizeof basename, "%s/fonts/font%d", a->root, font_size);
			snprintf(path, sizeof path, "%s.fnt", basename);
			status = assets_probe_file(path);
			if (status == ASSET_LOAD_SUCCESS &&
				!AeronFontAtlas_Load(&slot->atlas, cmd, basename))
				status = ASSET_LOAD_FAILED;
		}
		if (status == ASSET_LOAD_SUCCESS) {
			slot->source = (uint8_t)source;
			Aeron_Log("xwa.remaster", "frontend font %d: source=%s", font_size, assets_source_name(source));
			*out = &slot->atlas;
			return ASSET_LOAD_SUCCESS;
		}
		memset(&slot->atlas, 0, sizeof slot->atlas);
		if (status != ASSET_LOAD_MISSING) {
			Aeron_Log("xwa.remaster", "frontend font %d: load failed", font_size);
			return status;
		}
	}
	Aeron_Log("xwa.remaster", "frontend font %d: source=missing", font_size);
	return ASSET_LOAD_MISSING;
}

static int assets_prepare_frontend_fonts(XwaRemasterAssets* a, AeronCommandBuffer* cmd) {
	if (!a || a->frontend_fonts_prepared) {
		return a != NULL;
	}
	static const int font_sizes[] = { 10, 12, 15, 20 };
	int loaded = 0;
	for (int i = 0; i < (int)(sizeof font_sizes / sizeof font_sizes[0]); i++) {
		const AeronFontAtlas* font;
		const AssetLoadStatus status =
			assets_load_frontend_font(a, cmd, font_sizes[i], &font);
		if (status != ASSET_LOAD_SUCCESS && status != ASSET_LOAD_MISSING)
			return 0;
		loaded += font != NULL;
	}
	a->frontend_fonts_prepared = 1;
	Aeron_Log("xwa.remaster", "frontend fonts: %d/%u sizes loaded", loaded,
			  (unsigned)(sizeof font_sizes / sizeof font_sizes[0]));
	return 1;
}

const AeronFontAtlas* XwaRemasterAssets_FrontendFont(XwaRemasterAssets* a, int font_size,
													 float* out_atlas_scale) {
	if (!a) {
		return NULL;
	}
	for (int i = 0; i < a->font_count; i++) {
		if (a->fonts[i].font_size == font_size) {
			if (out_atlas_scale)
				*out_atlas_scale = 4.0f;
			return a->fonts[i].atlas.loaded ? &a->fonts[i].atlas : NULL;
		}
	}
	return NULL;
}
