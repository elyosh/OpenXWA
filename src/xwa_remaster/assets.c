/*
 * XWA remaster snapshot-resident asset resolver — see xwa_remaster/assets.h.
 * Resolves assets from the mod folder or independently decoded original files.
 * Recovered XWA resource state is used only for snapshot identities/lifetimes.
 */

#include "xwa_remaster/assets.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/image.h"
#include "aeron/log.h"
#include "aeron/random.h"
#include "aeron/time.h"
#include "aeron/scene/image_cache.h"
#include "aeron/scene/runtime_atlas.h"
#include "aeron/scene/sprite_atlas.h"
#include "xwa_runtime/frontend/frontend_fonts.h"
#include "xwa_runtime/frontend/frontend_geometry.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_remaster/original_2d.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XWA_ASSETS_MAX_FONTS 16
#define XWA_ASSETS_MAX_FACE_FONTS 16
#define XWA_ORIGINAL_FONT_SCALE 4
#define XWA_ASSETS_MAX_FILES 1024 /* == engine resource cap */
#define XWA_ASSETS_MAX_GROUPS XWA_SNAP_MAX_TEXTURE_ASSETS
#define XWA_ASSETS_KEY_MAX 40
#define XWA_ASSETS_MAX_FRAME_SETS 8
#define XWA_ASSETS_MAX_SET_MISSIONS 8
#define XWA_ASSETS_SET_NAME_MAX 64
#define XWA_ASSETS_SHUFFLE_NAME_MAX 32

static uint32_t assets_next_generation;

typedef struct FontSlot {
	int font_size;
	uint8_t source;       /* AssetSource */
	AeronFontAtlas atlas; /* loaded == 0 when the load failed */
} FontSlot;

/* An atlas of a font from frontend/fonts/fonts.yaml at one pixel size. */
typedef struct FaceFontSlot {
	int font_size;
	float em_pixels;
	uint32_t used; /* face_font_clock at the last prepare */
	AeronFontAtlas atlas;
} FaceFontSlot;

typedef struct FlightFontSlot {
	uint8_t tried;
	uint8_t source; /* AssetSource */
	float atlas_scale;
	AeronFontAtlas atlas;
} FlightFontSlot;

/* Classified layout form of one source-file key. Probed once (fopen),
 * negative results cached — per-frame lookups never touch the disk. */
typedef enum FileKind {
	FILE_UNPROBED = 0,
	FILE_MISSING,
	FILE_FAILED,
	FILE_SINGLE,   /* frontend/frontres/<dir>/sprites/<base>.ktx2 */
	FILE_ATLAS,    /* frontend/frontres/<dir>/atlas/<base>.{ktx2,yaml} */
	FILE_FRAMEDIR, /* frontend/frontres/<dir>/atlas/<base>/frame_NN.ktx2 */
} FileKind;

typedef enum AssetSource {
	ASSET_SOURCE_MISSING = 0,
	ASSET_SOURCE_MOD,
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
	AeronRuntimeAtlas original_atlas;
} FileSlot;

/* An alternative frame set of a group. It is an atlas in resdata/atlas, and
 * its images replace the group's images by position. */
typedef struct FrameSet {
	char name[XWA_ASSETS_SET_NAME_MAX];
	AeronSpriteAtlas atlas;
	uint8_t atlas_ok;
	uint8_t loaded; /* its pages are in the flight image cache */
	int32_t missions[XWA_ASSETS_MAX_SET_MISSIONS];
	int mission_count;
} FrameSet;

/* Holds the `frame_sets` key of a group's atlas, and the group's current
 * choice. */
typedef struct FrameSets {
	char shuffle[XWA_ASSETS_SHUFFLE_NAME_MAX]; /* empty when the group picks alone */
	uint64_t reroll_us;
	FrameSet sets[XWA_ASSETS_MAX_FRAME_SETS];
	int set_count;
	int choice;             /* -1 for the group's own images, else an index in sets */
	uint8_t drawn;          /* an image of the group was looked up since the last update */
	uint64_t next_check_us; /* 0 before the first update */
	uint32_t update_stamp;
} FrameSets;

typedef struct GroupSlot {
	int16_t group;
	uint8_t mod_status; /* AssetLoadStatus */
	uint8_t frontend_resident;
	uint8_t flight_resident;
	uint8_t frontend_source; /* AssetSource */
	uint8_t flight_source;   /* AssetSource */
	uint64_t frontend_seen_generation;
	uint64_t flight_seen_generation;
	AeronSpriteAtlas atlas;
	AeronRuntimeAtlas frontend_original_atlas;
	AeronRuntimeAtlas flight_original_atlas;
	FrameSets* frame_sets; /* NULL when the mod atlas has no `frame_sets` */
} GroupSlot;

struct XwaRemasterAssets {
	char root[512];
	int prefer_original_2d;
	XwaRemasterOriginal2d* original_reader;
	AeronImageCache* frontend_images;
	AeronImageCache* flight_images;
	FontSlot fonts[XWA_ASSETS_MAX_FONTS];
	int font_count;
	FaceFontSlot face_fonts[XWA_ASSETS_MAX_FACE_FONTS];
	uint32_t face_font_clock;
	FileSlot files[XWA_ASSETS_MAX_FILES];
	int file_count;
	GroupSlot groups[XWA_ASSETS_MAX_GROUPS];
	int group_count;
	uint64_t frontend_asset_generation;
	uint8_t frontend_fonts_prepared;
	FlightFontSlot flight_fonts[3];
	uint64_t texture_asset_generation;
	uint32_t generation;
	int32_t mission_index;      /* from the snapshot, for the frame sets' `missions` */
	uint32_t frame_set_random;  /* xorshift32 state */
	uint32_t frame_set_updates; /* counts XwaRemasterAssets_UpdateFrameSets calls */
};

XwaRemasterAssets* XwaRemasterAssets_Create(const char* root, int prefer_original_2d) {
	XwaRemasterAssets* a = (XwaRemasterAssets*)calloc(1, sizeof *a);
	if (!a) {
		return NULL;
	}
	snprintf(a->root, sizeof a->root, "%s", root ? root : "");
	a->prefer_original_2d = prefer_original_2d != 0;
	a->mission_index = -1;
	/* The frame sets use their own random numbers, so they don't change the
	   game's rand() sequence. */
	if (!Aeron_RandomBytes(&a->frame_set_random, sizeof a->frame_set_random) || !a->frame_set_random) {
		a->frame_set_random = 0x2545f491u;
	}
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
	Aeron_LogInfo("xwa.remaster", "2D asset policy: prefer=%s alternate=%s",
				  a->prefer_original_2d ? "original" : "mod", a->prefer_original_2d ? "mod" : "original");
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
	for (int i = 0; i < XWA_ASSETS_MAX_FACE_FONTS; i++) {
		AeronFontAtlas_Release(&a->face_fonts[i].atlas);
	}
	for (int i = 0; i < a->file_count; i++) {
		Aeron_RuntimeAtlasRelease(&a->files[i].original_atlas);
		if (a->files[i].kind == FILE_ATLAS) {
			Aeron_SpriteAtlasFree(&a->files[i].atlas);
		}
	}
	for (int i = 0; i < a->group_count; i++) {
		Aeron_RuntimeAtlasRelease(&a->groups[i].frontend_original_atlas);
		Aeron_RuntimeAtlasRelease(&a->groups[i].flight_original_atlas);
		if (a->groups[i].mod_status == ASSET_LOAD_SUCCESS) {
			Aeron_SpriteAtlasFree(&a->groups[i].atlas);
		}
		if (a->groups[i].frame_sets) {
			for (int k = 0; k < a->groups[i].frame_sets->set_count; k++)
				Aeron_SpriteAtlasFree(&a->groups[i].frame_sets->sets[k].atlas);
			free(a->groups[i].frame_sets);
		}
	}
	for (int i = 0; i < 3; i++) {
		AeronFontAtlas_Release(&a->flight_fonts[i].atlas);
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
	return source == ASSET_SOURCE_MOD ? "mod" : source == ASSET_SOURCE_ORIGINAL ? "original" : "missing";
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

static int assets_runtime_frame(const AeronRuntimeAtlas* atlas, int frame, XwaAssetRef* out) {
	if (!atlas || !out || frame < 0 || frame >= atlas->layout.frame_count)
		return 0;
	const int page_index = atlas->layout.pages ? atlas->layout.pages[frame] : 0;
	if (page_index < 0 || page_index >= atlas->layout.page_count)
		return 0;
	const AeronRuntimeAtlasPage* page = &atlas->pages[page_index];
	if (!page->texture || page->width <= 0 || page->height <= 0)
		return 0;
	out->texture = page->texture;
	out->u0 = out->v0 = 0.0f;
	out->u1 = out->v1 = 1.0f;
	out->w = page->width;
	out->h = page->height;
	return apply_atlas_frame(&atlas->layout, frame, out);
}

static int assets_build_runtime_atlas(AeronRuntimeAtlas* atlas,
		AeronCommandBuffer* cmd, const Xwa2dFrameSet* source,
		int generate_mips, const char* debug_name) {
	if (!atlas || !cmd || !source || !source->frames || source->count <= 0)
		return 0;
	AeronRuntimeAtlasFrame* frames =
		(AeronRuntimeAtlasFrame*)calloc((size_t)source->count, sizeof *frames);
	if (!frames)
		return 0;
	for (int index = 0; index < source->count; ++index) {
		const Xwa2dFrame* source_frame = &source->frames[index];
		frames[index] = (AeronRuntimeAtlasFrame) {
			.rgba = source_frame->rgba,
			.width = source_frame->width,
			.height = source_frame->height,
			.id = source_frame->sprite_id,
			.anchor_x = source_frame->anchor_x,
			.anchor_y = source_frame->anchor_y,
		};
	}
	const AeronRuntimeAtlasOptions options = {
		.format = AERON_TEXTURE_FORMAT_RGBA8_SRGB,
		.color_space = AERON_COLOR_SPACE_SRGB,
		.alpha_mode = AERON_IMAGE_ALPHA_STRAIGHT,
		.generate_mips = generate_mips != 0,
		.debug_name = debug_name,
	};
	const int built = Aeron_RuntimeAtlasBuild(atlas, cmd, frames,
			source->count, &options) ? 1 : 0;
	free(frames);
	return built;
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
	snprintf(path, sizeof path, "%s/frontend/frontres/", a->root);
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
		snprintf(path, 768, "%s/frontend/frontres/%.*s/sprites/%s.ktx2", a->root, dlen, slot->key, base);
	} else if (slot->kind == FILE_ATLAS && part > 0) {
		snprintf(path, 768, "%s/frontend/frontres/%.*s/atlas/%s_p%d.ktx2", a->root, dlen, slot->key, base, part);
	} else if (slot->kind == FILE_ATLAS) {
		snprintf(path, 768, "%s/frontend/frontres/%.*s/atlas/%s.ktx2", a->root, dlen, slot->key, base);
	} else {
		snprintf(path, 768, "%s/frontend/frontres/%.*s/atlas/%s/frame_%02d.ktx2", a->root, dlen, slot->key,
				 base, part);
	}
}

static FrameSets* assets_read_frame_sets(const XwaRemasterAssets* a, int group);

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
		s->mod_status = Aeron_SpriteAtlasLoad(&s->atlas, probe) ? ASSET_LOAD_SUCCESS : ASSET_LOAD_FAILED;
		if (s->mod_status == ASSET_LOAD_SUCCESS)
			s->frame_sets = assets_read_frame_sets(a, group);
	} else {
		s->mod_status = (uint8_t)status;
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

/* ---- alternative frame sets ---------------------------------------------- */

static int assets_frame_sets_error(const char* path, const AeronConfigNode* node, const char* message) {
	Aeron_LogError("xwa.remaster", "%s:%d: %s. The group draws only its own images", path,
				   AeronConfigNode_Line(node), message);
	return 0;
}

/* A set name is used as a file name, so it may only contain letters, digits,
   '_' and '-'. */
static int assets_valid_set_name(const char* name) {
	if (!name || !name[0] || strlen(name) >= XWA_ASSETS_SET_NAME_MAX)
		return 0;
	for (const char* c = name; *c; c++) {
		if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_' ||
			  *c == '-'))
			return 0;
	}
	return 1;
}

static int assets_read_frame_set(const XwaRemasterAssets* a, const char* path, const AeronConfigNode* node,
								 FrameSet* set) {
	const AeronConfigNode* atlas = AeronConfigNode_MapGet(node, "atlas");
	const AeronConfigNode* missions = AeronConfigNode_MapGet(node, "missions");
	const char* name = AeronConfigNode_String(atlas, NULL);
	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP ||
		AeronConfigNode_Type(atlas) != AERON_CONFIG_STRING || !assets_valid_set_name(name))
		return assets_frame_sets_error(path, node,
									   "each set needs an atlas name, with only letters, digits, _ and -");
	snprintf(set->name, sizeof set->name, "%s", name);
	if (missions) {
		const size_t count = AeronConfigNode_SequenceCount(missions);
		if (AeronConfigNode_Type(missions) != AERON_CONFIG_SEQUENCE || count > XWA_ASSETS_MAX_SET_MISSIONS)
			return assets_frame_sets_error(path, missions,
										   "missions must be a list of up to 8 mission indexes");
		for (size_t i = 0; i < count; i++) {
			const AeronConfigNode* mission = AeronConfigNode_SequenceGet(missions, i);
			const int64_t index = AeronConfigNode_Int(mission, -1);
			if (AeronConfigNode_Type(mission) != AERON_CONFIG_INT || index < 0 || index > INT32_MAX)
				return assets_frame_sets_error(path, mission,
											   "a mission index must be a whole number, 0 or more");
			set->missions[set->mission_count++] = (int32_t)index;
		}
	}
	char atlas_path[768];
	snprintf(atlas_path, sizeof atlas_path, "%s/resdata/atlas/%s.yaml", a->root, set->name);
	set->atlas_ok = Aeron_SpriteAtlasLoad(&set->atlas, atlas_path);
	if (!set->atlas_ok)
		Aeron_LogError("xwa.remaster", "%s: the set %s can't be read. It isn't used", path, set->name);
	return 1;
}

/* Reads the `frame_sets` key of group `group`'s atlas. Returns NULL when the
   atlas has no such key, or when the key has an error (logged). */
static FrameSets* assets_read_frame_sets(const XwaRemasterAssets* a, int group) {
	char path[64];
	AeronConfigFile* file = NULL;
	AeronConfigError error = { 0 };
	snprintf(path, sizeof path, "resdata/atlas/group_%d.yaml", group);
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, path, &file, &error))
		return NULL; /* the atlas reader already reported it */
	const AeronConfigNode* node = AeronConfigNode_MapGet(AeronConfigFile_Root(file), "frame_sets");
	const AeronConfigNode* shuffle = AeronConfigNode_MapGet(node, "shuffle");
	const AeronConfigNode* reroll = AeronConfigNode_MapGet(node, "reroll_seconds");
	const AeronConfigNode* sets = AeronConfigNode_MapGet(node, "sets");
	const size_t set_count = AeronConfigNode_SequenceCount(sets);
	const AeronConfigNodeType reroll_type = AeronConfigNode_Type(reroll);
	const double reroll_seconds = AeronConfigNode_Float(reroll, 0.0);
	FrameSets* fs = NULL;
	int ok = 1;
	if (!node) {
		AeronConfigFile_Destroy(file);
		return NULL;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP ||
		AeronConfigNode_Type(sets) != AERON_CONFIG_SEQUENCE || set_count == 0 ||
		set_count > XWA_ASSETS_MAX_FRAME_SETS)
		ok = assets_frame_sets_error(path, node, "frame_sets needs a list of 1 to 8 sets");
	else if ((reroll_type != AERON_CONFIG_INT && reroll_type != AERON_CONFIG_FLOAT) ||
			 !(reroll_seconds > 0.0 && reroll_seconds <= 3600.0))
		ok = assets_frame_sets_error(path, node, "frame_sets needs reroll_seconds, from above 0 to 3600");
	else if (shuffle && (AeronConfigNode_Type(shuffle) != AERON_CONFIG_STRING ||
						 strlen(AeronConfigNode_String(shuffle, "")) >= XWA_ASSETS_SHUFFLE_NAME_MAX))
		ok = assets_frame_sets_error(path, shuffle, "shuffle must be a name of up to 31 characters");
	if (ok) {
		fs = (FrameSets*)calloc(1, sizeof *fs);
		ok = fs != NULL;
	}
	if (ok) {
		snprintf(fs->shuffle, sizeof fs->shuffle, "%s", AeronConfigNode_String(shuffle, ""));
		fs->reroll_us = (uint64_t)(reroll_seconds * 1000000.0);
		fs->choice = -1;
		for (size_t i = 0; ok && i < set_count; i++) {
			ok = assets_read_frame_set(a, path, AeronConfigNode_SequenceGet(sets, i), &fs->sets[i]);
			fs->set_count = (int)i + 1;
		}
	}
	if (!ok && fs) {
		for (int k = 0; k < fs->set_count; k++)
			Aeron_SpriteAtlasFree(&fs->sets[k].atlas);
		free(fs);
		fs = NULL;
	}
	AeronConfigFile_Destroy(file);
	return fs;
}

static void assets_set_page_path(const XwaRemasterAssets* a, const FrameSet* set, int page, char path[768]) {
	if (page == 0) {
		snprintf(path, 768, "%s/resdata/atlas/%s.ktx2", a->root, set->name);
	} else {
		snprintf(path, 768, "%s/resdata/atlas/%s_p%d.ktx2", a->root, set->name, page);
	}
}

/* Loads the pages of the group's sets into the flight image cache. A set
   that can't be loaded isn't used. */
static void assets_load_frame_sets(XwaRemasterAssets* a, AeronCommandBuffer* cmd, GroupSlot* slot) {
	FrameSets* fs = slot->frame_sets;
	for (int k = 0; fs && k < fs->set_count; k++) {
		FrameSet* set = &fs->sets[k];
		if (!set->atlas_ok || set->loaded)
			continue;
		const int page_count = set->atlas.page_count > 0 ? set->atlas.page_count : 1;
		int page = 0;
		for (; page < page_count; page++) {
			char path[768];
			XwaAssetRef ref;
			assets_set_page_path(a, set, page, path);
			if (assets_load_texture(a->flight_images, cmd, path, &ref) != ASSET_LOAD_SUCCESS)
				break;
		}
		if (page == page_count) {
			set->loaded = 1;
			continue;
		}
		for (int loaded = 0; loaded < page; loaded++) {
			char path[768];
			assets_set_page_path(a, set, loaded, path);
			Aeron_ImageCacheInvalidate(a->flight_images, path);
		}
		Aeron_LogError("xwa.remaster", "flight group %d: the set %s can't be loaded. It isn't used",
					   slot->group, set->name);
	}
}

static void assets_release_frame_sets(XwaRemasterAssets* a, GroupSlot* slot) {
	FrameSets* fs = slot->frame_sets;
	for (int k = 0; fs && k < fs->set_count; k++) {
		FrameSet* set = &fs->sets[k];
		if (!set->loaded)
			continue;
		const int page_count = set->atlas.page_count > 0 ? set->atlas.page_count : 1;
		for (int page = 0; page < page_count; page++) {
			char path[768];
			assets_set_page_path(a, set, page, path);
			Aeron_ImageCacheInvalidate(a->flight_images, path);
		}
		set->loaded = 0;
	}
}

/* Returns the set that the group draws now, or NULL when it draws its own
   images. In a mission that a set lists, that set is drawn instead of any
   other set. The group's own images stay a choice. */
static const FrameSet* assets_chosen_set(const XwaRemasterAssets* a, const FrameSets* fs) {
	if (fs->choice < 0 || fs->choice >= fs->set_count)
		return NULL;
	for (int k = 0; k < fs->set_count; k++) {
		for (int m = 0; fs->sets[k].loaded && m < fs->sets[k].mission_count; m++) {
			if (fs->sets[k].missions[m] == a->mission_index)
				return &fs->sets[k];
		}
	}
	return fs->sets[fs->choice].loaded ? &fs->sets[fs->choice] : NULL;
}

static uint32_t assets_frame_set_random(XwaRemasterAssets* a) {
	uint32_t x = a->frame_set_random; /* xorshift32 */
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	a->frame_set_random = x;
	return x;
}

/* Picks the group's own images or one of its loaded sets, each with the same
   odds. */
static void assets_pick_frame_set(XwaRemasterAssets* a, FrameSets* fs) {
	int loaded[XWA_ASSETS_MAX_FRAME_SETS];
	int count = 0;
	for (int k = 0; k < fs->set_count; k++) {
		if (fs->sets[k].loaded)
			loaded[count++] = k;
	}
	const uint32_t pick = assets_frame_set_random(a) % (uint32_t)(count + 1);
	fs->choice = pick == 0 ? -1 : loaded[pick - 1];
}

static int assets_same_shuffle(const FrameSets* lhs, const FrameSets* rhs) {
	return lhs == rhs || (lhs->shuffle[0] && strcmp(lhs->shuffle, rhs->shuffle) == 0);
}

void XwaRemasterAssets_UpdateFrameSets(XwaRemasterAssets* a, const XwaSnapshot* snapshot) {
	if (!a)
		return;
	a->mission_index = snapshot ? snapshot->mission_index : -1;
	const uint32_t stamp = ++a->frame_set_updates;
	const uint64_t now = Aeron_NowUs();
	for (int i = 0; i < a->group_count; i++) {
		FrameSets* first = a->groups[i].frame_sets;
		if (!first || first->update_stamp == stamp)
			continue;
		/* The groups that share this group's shuffle name use its timer. */
		int drawn = 0;
		for (int j = i; j < a->group_count; j++) {
			const FrameSets* fs = a->groups[j].frame_sets;
			if (fs && assets_same_shuffle(first, fs))
				drawn |= fs->drawn;
		}
		const int start = first->next_check_us == 0;
		const int check = !start && now >= first->next_check_us;
		for (int j = i; j < a->group_count; j++) {
			FrameSets* fs = a->groups[j].frame_sets;
			if (!fs || !assets_same_shuffle(first, fs))
				continue;
			/* Pick new sets only when none of the groups was drawn in the
			   last frame, so that no animation changes while on screen. */
			if (check && !drawn)
				assets_pick_frame_set(a, fs);
			if (start || check)
				fs->next_check_us = now + first->reroll_us;
			fs->drawn = 0;
			fs->update_stamp = stamp;
		}
	}
}

/* Looks up frame `frame` of a group with frame sets. It returns image
   `frame` of the chosen set, with the classic size of the group's own frame,
   so the image is drawn on the group's quad. When the set has fewer images,
   the extra frames return nothing. */
static int assets_frame_set_frame(XwaRemasterAssets* a, GroupSlot* slot, int frame, XwaAssetRef* out);

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

static int assets_frame_set_frame(XwaRemasterAssets* a, GroupSlot* slot, int frame, XwaAssetRef* out) {
	FrameSets* fs = slot->frame_sets;
	fs->drawn = 1;
	const FrameSet* set = assets_chosen_set(a, fs);
	if (!set)
		return assets_flight_group_frame(a, slot, frame, out);
	XwaAssetRef own;
	if (!assets_flight_group_frame(a, slot, frame, &own) || frame >= set->atlas.frame_count)
		return 0;
	const int page = set->atlas.pages ? set->atlas.pages[frame] : 0;
	char path[768];
	assets_set_page_path(a, set, page, path);
	if (!assets_find_texture(a->flight_images, path, out) || !apply_atlas_frame(&set->atlas, frame, out))
		return 0;
	out->classic_w = own.classic_w;
	out->classic_h = own.classic_h;
	return 1;
}

static AssetLoadStatus assets_load_mod_group(XwaRemasterAssets* a, AeronImageCache* images,
											 AeronCommandBuffer* cmd, GroupSlot* slot) {
	if (slot->mod_status != ASSET_LOAD_SUCCESS)
		return (AssetLoadStatus)slot->mod_status;
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
			Aeron_LogWarn("xwa.remaster", "2D group %d: original load failed: %s", slot->group, error);
		return status;
	}
	AeronRuntimeAtlas* atlas = flight ? &slot->flight_original_atlas : &slot->frontend_original_atlas;
	const int loaded = assets_build_runtime_atlas(atlas, cmd, &frames, flight,
			"XWA original DAT group");
	Xwa2dFrameSet_Free(&frames);
	if (loaded) {
		Aeron_LogInfo("xwa.remaster", "2D group %d: source=original pages=%d time_us=%llu", slot->group,
				  atlas->layout.page_count, (unsigned long long)(Aeron_NowUs() - start_us));
	}
	return loaded ? ASSET_LOAD_SUCCESS : ASSET_LOAD_FAILED;
}

static AssetLoadStatus assets_choose_group(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
										   GroupSlot* slot, int flight, uint8_t* out_source) {
	*out_source = ASSET_SOURCE_MISSING;
	/* As for files: if an atlas sets a group's geometry, only the mod's
	   texture is used, in the frontend and in flight. The stock DAT can have
	   other frame counts and sizes than the ones the game uses. */
	if (XwaFrontendGeometry_FindGroup(slot->group)) {
		const AssetLoadStatus status =
			assets_load_mod_group(a, flight ? a->flight_images : a->frontend_images, cmd, slot);
		if (status == ASSET_LOAD_SUCCESS)
			*out_source = ASSET_SOURCE_MOD;
		else if (status == ASSET_LOAD_MISSING)
			Aeron_LogError("xwa.remaster",
						   "%s group %d: the geometry comes from an atlas but the texture "
						   "is missing, so the group isn't drawn",
						   flight ? "flight" : "frontend", slot->group);
		return status == ASSET_LOAD_MISSING ? ASSET_LOAD_SUCCESS : status;
	}
	const int first = a->prefer_original_2d ? ASSET_SOURCE_ORIGINAL : ASSET_SOURCE_MOD;
	for (int attempt = 0; attempt < 2; attempt++) {
		const int source = attempt == 0
							   ? first
							   : (first == ASSET_SOURCE_ORIGINAL ? ASSET_SOURCE_MOD : ASSET_SOURCE_ORIGINAL);
		const AssetLoadStatus status =
			source == ASSET_SOURCE_ORIGINAL
				? assets_load_original_group(a, cmd, slot, flight)
				: assets_load_mod_group(a, flight ? a->flight_images : a->frontend_images, cmd, slot);
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
	if (*source == ASSET_SOURCE_MOD) {
		AeronImageCache* images = flight ? a->flight_images : a->frontend_images;
		if (flight)
			assets_release_frame_sets(a, slot);
		const int page_count = slot->atlas.page_count > 0 ? slot->atlas.page_count : 1;
		for (int page = 0; page < page_count; page++) {
			char path[768];
			assets_group_page_path(a, slot->group, page, path);
			Aeron_ImageCacheInvalidate(images, path);
		}
	} else if (*source == ASSET_SOURCE_ORIGINAL) {
		Aeron_RuntimeAtlasRelease(flight ? &slot->flight_original_atlas :
				&slot->frontend_original_atlas);
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
			Aeron_LogWarn("xwa.remaster", "2D file '%s': original load failed: %s", slot->key, error);
		return status;
	}
	const int loaded = assets_build_runtime_atlas(&slot->original_atlas, cmd,
			&frames, 0, slot->source_file);
	Xwa2dFrameSet_Free(&frames);
	if (loaded) {
		Aeron_LogInfo("xwa.remaster", "2D file '%s': source=original pages=%d time_us=%llu", slot->key,
				  slot->original_atlas.layout.page_count, (unsigned long long)(Aeron_NowUs() - start_us));
	}
	return loaded ? ASSET_LOAD_SUCCESS : ASSET_LOAD_FAILED;
}

static AssetLoadStatus assets_load_mod_file(XwaRemasterAssets* a, AeronCommandBuffer* cmd, FileSlot* slot,
											uint16_t frame_count) {
	if (slot->kind == FILE_MISSING || slot->kind == FILE_UNPROBED)
		return ASSET_LOAD_MISSING;
	if (slot->kind == FILE_FAILED)
		return ASSET_LOAD_FAILED;
	const uint16_t parts = slot->kind == FILE_FRAMEDIR ? frame_count
						   : slot->kind == FILE_ATLAS && slot->atlas.page_count > 1 ? (uint16_t)slot->atlas.page_count
																				   : 1;
	const AssetLoadStatus status = assets_sync_frontend_file(a, cmd, slot, parts);
	return status == ASSET_LOAD_SUCCESS ? ASSET_LOAD_SUCCESS : ASSET_LOAD_FAILED;
}

static AssetLoadStatus assets_choose_frontend_file(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
												   FileSlot* slot, uint16_t frame_count,
												   uint8_t* out_source) {
	*out_source = ASSET_SOURCE_MISSING;
	/* If an atlas sets an image's geometry, the game's own image doesn't fit
	   it, so only the mod's texture is used. An image replaced by a video has
	   no texture; the snapshot draws the video instead. */
	const XwaGeometryAtlas* atlas = XwaFrontendGeometry_FindFile(slot->source_file);
	if (atlas && atlas->video[0])
		return ASSET_LOAD_SUCCESS;
	if (atlas) {
		const AssetLoadStatus status = assets_load_mod_file(a, cmd, slot, frame_count);
		if (status == ASSET_LOAD_SUCCESS)
			*out_source = ASSET_SOURCE_MOD;
		else if (status == ASSET_LOAD_MISSING)
			Aeron_LogError("xwa.remaster",
						   "2D file '%s': the geometry comes from an atlas but the texture is "
						   "missing, so the file isn't drawn",
						   slot->key);
		return status == ASSET_LOAD_MISSING ? ASSET_LOAD_SUCCESS : status;
	}
	const int first = a->prefer_original_2d ? ASSET_SOURCE_ORIGINAL : ASSET_SOURCE_MOD;
	for (int attempt = 0; attempt < 2; attempt++) {
		const int source = attempt == 0
							   ? first
							   : (first == ASSET_SOURCE_ORIGINAL ? ASSET_SOURCE_MOD : ASSET_SOURCE_ORIGINAL);
		const AssetLoadStatus status = source == ASSET_SOURCE_ORIGINAL
										   ? assets_load_original_file(a, cmd, slot)
										   : assets_load_mod_file(a, cmd, slot, frame_count);
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
			Aeron_RuntimeAtlasRelease(&slot->original_atlas);
			slot->resident_source = ASSET_SOURCE_MISSING;
			snprintf(slot->source_file, sizeof slot->source_file, "%s", asset->source_file);
		}
		if (slot->resident_source == ASSET_SOURCE_MISSING) {
			uint8_t source;
			const AssetLoadStatus status =
				assets_choose_frontend_file(a, cmd, slot, asset->frame_count, &source);
			if (status != ASSET_LOAD_SUCCESS && status != ASSET_LOAD_MISSING) {
				Aeron_LogError("xwa.remaster", "2D file '%s': load failed", slot->key);
				return 0;
			}
			slot->resident_source = source;
			Aeron_LogInfo("xwa.remaster", "2D file '%s': source=%s", slot->key,
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
				Aeron_LogError("xwa.remaster", "frontend group %d: load failed", slot->group);
				return 0;
			}
			slot->frontend_resident = 1;
			Aeron_LogInfo("xwa.remaster", "frontend group %d: source=%s", slot->group,
					  assets_source_name(slot->frontend_source));
		}
	}

	uint32_t texture_count = 0;
	for (int i = 0; i < a->file_count; i++) {
		FileSlot* slot = &a->files[i];
		if (slot->resident_source != ASSET_SOURCE_MISSING &&
			slot->frontend_seen_generation != snapshot->frontend_asset_generation) {
			(void)assets_sync_frontend_file(a, cmd, slot, 0);
			Aeron_RuntimeAtlasRelease(&slot->original_atlas);
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
								 : (slot->frontend_source == ASSET_SOURCE_MOD
										? (slot->atlas.page_count > 0 ? (uint32_t)slot->atlas.page_count : 1)
										: 0);
		}
	}

	if (!assets_prepare_frontend_fonts(a, cmd))
		return 0;
	Aeron_LogInfo("xwa.remaster", "frontend assets prepared: generation=%llu files=%u groups=%u textures=%u",
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
	int part = slot->kind == FILE_FRAMEDIR && frame >= 0 ? frame : 0;
	if (slot->kind == FILE_ATLAS && slot->atlas.pages && frame >= 0 && frame < slot->atlas.frame_count)
		part = slot->atlas.pages[frame];
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
				Aeron_LogError("xwa.remaster", "flight group %d: load failed", slot->group);
				return 0;
			}
			slot->flight_resident = 1;
			if (slot->flight_source == ASSET_SOURCE_MOD)
				assets_load_frame_sets(a, cmd, slot);
			Aeron_LogInfo("xwa.remaster", "flight group %d: source=%s", slot->group,
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
		} else if (slot->flight_source == ASSET_SOURCE_MOD) {
			page_count += slot->atlas.page_count > 0 ? (uint32_t)slot->atlas.page_count : 1;
		}
	}

	Aeron_LogInfo("xwa.remaster", "flight texture assets prepared: generation=%llu live_types=%u pages=%u",
			  (unsigned long long)snapshot->texture_asset_generation, live_type_count, page_count);
	return 1;
}

void XwaRemasterAssets_CommitFrontendAssets(XwaRemasterAssets* a, uint64_t generation) {
	if (!a) {
		return;
	}
	a->frontend_asset_generation = generation;
	Aeron_LogInfo("xwa.remaster", "frontend assets committed: generation=%llu",
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
	Aeron_LogInfo("xwa.remaster", "flight texture assets committed: generation=%llu",
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
	if (slot->frame_sets)
		return assets_frame_set_frame(a, slot, frame, out);
	return assets_flight_group_frame(a, slot, frame, out);
}

int XwaRemasterAssets_FlightModelImageId(XwaRemasterAssets* a, int object_type, int classic_frame_1based,
										 int* out_group, int* out_image) {
	int group = 0;
	int frames_mode = 0;
	int sprite_id = 0;
	if (!a || !out_group || !out_image ||
		!XwaSnapshotExport_ModelTextureBinding(object_type, &group, &frames_mode, &sprite_id)) {
		return 0;
	}
	*out_group = group;
	if (!frames_mode) {
		*out_image = sprite_id;
		return 1;
	}
	const int index = classic_frame_1based - 1;
	if (index < 0) {
		return 0;
	}
	const GroupSlot* slot = assets_group_find(a, group);
	const AeronSpriteAtlas* atlas = !slot ? NULL
									: slot->flight_source == ASSET_SOURCE_ORIGINAL
										? &slot->flight_original_atlas.layout
										: &slot->atlas;
	*out_image = atlas && atlas->ids && index < atlas->frame_count ? atlas->ids[index] : index;
	return 1;
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
			source->glyphs[i].height > UINT16_MAX / atlas_scale ||
			source->glyphs[i].advance > UINT16_MAX / atlas_scale) {
			free(glyphs);
			return NULL;
		}
		glyphs[i].atlas_x = (uint16_t)(source->glyphs[i].x * atlas_scale);
		glyphs[i].atlas_y = (uint16_t)(source->glyphs[i].y * atlas_scale);
		glyphs[i].atlas_w = (uint16_t)(source->glyphs[i].width * atlas_scale);
		glyphs[i].atlas_h = (uint16_t)(source->glyphs[i].height * atlas_scale);
		glyphs[i].advance = (uint16_t)(source->glyphs[i].advance * atlas_scale);
	}
	return glyphs;
}

static int assets_init_original_font(AeronCommandBuffer* cmd,
		const Xwa2dFontAtlas* font, int atlas_scale, int generate_mips,
		const char* debug_name, AeronFontAtlas* out) {
	if (!cmd || !font || !out || atlas_scale <= 0 || font->cell_width <= 0 ||
		font->cell_height <= 0 || font->baseline < 0 ||
		font->cell_width > UINT16_MAX / atlas_scale ||
		font->cell_height > UINT16_MAX / atlas_scale ||
		font->baseline > UINT16_MAX / atlas_scale)
		return 0;
	AeronFontGlyph* glyphs = assets_copy_font_glyphs(font, atlas_scale);
	int atlas_width = 0, atlas_height = 0;
	uint8_t* atlas_rgba = glyphs ? Aeron_ImageUpscaleNearestRgba8(font->rgba, font->width, font->height,
														  atlas_scale, &atlas_width, &atlas_height)
								 : NULL;
	const int initialized = atlas_rgba && AeronFontAtlas_InitRgba8(
			out, cmd,
			&(AeronFontAtlasRgba8Desc) {
				.pixels = atlas_rgba,
				.width = atlas_width,
				.height = atlas_height,
				.pitch = (size_t)atlas_width * 4,
				.first_char = font->first_char,
				.cell_w = (uint16_t)(font->cell_width * atlas_scale),
				.cell_h = (uint16_t)(font->cell_height * atlas_scale),
				.baseline = (uint16_t)(font->baseline * atlas_scale),
				.glyphs = glyphs,
				.glyph_count = font->glyph_count,
				.format = AERON_TEXTURE_FORMAT_RGBA8_UNORM,
				.color_space = AERON_COLOR_SPACE_LINEAR_SRGB,
				.alpha_mode = AERON_IMAGE_ALPHA_STRAIGHT,
				.generate_mips = generate_mips != 0,
				.debug_name = debug_name,
			});
	free(atlas_rgba);
	free(glyphs);
	return initialized;
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
			Aeron_LogWarn("xwa.remaster", "flight font tier %d: original load failed: %s", tier, error);
		return status;
	}
	if (!Xwa2d_BuildFlightFontTier(&group, tier, &font, error, sizeof error)) {
		Aeron_LogWarn("xwa.remaster", "flight font tier %d: original data invalid: %s", tier, error);
		Xwa2dFrameSet_Free(&group);
		Xwa2dFontAtlas_Free(&font);
		return ASSET_LOAD_FAILED;
	}
	Xwa2dFrameSet_Free(&group);
	if (!assets_init_original_font(cmd, &font, XWA_ORIGINAL_FONT_SCALE, 1,
			"XWA original flight font", &slot->atlas)) {
		Xwa2dFontAtlas_Free(&font);
		return ASSET_LOAD_FAILED;
	}
	slot->atlas_scale = (float)XWA_ORIGINAL_FONT_SCALE;
	Xwa2dFontAtlas_Free(&font);
	return ASSET_LOAD_SUCCESS;
}

static AssetLoadStatus assets_load_mod_flight_font(XwaRemasterAssets* a, AeronCommandBuffer* cmd, int tier) {
	if (!a || tier < 0 || tier >= 3)
		return ASSET_LOAD_FAILED;
	FlightFontSlot* slot = &a->flight_fonts[tier];
	if (!cmd)
		return ASSET_LOAD_FAILED;
	char base[768], path[800];
	if (snprintf(base, sizeof base, "%s/flight/fonts/font_tier_%d",
			a->root, tier) >= (int)sizeof base ||
		snprintf(path, sizeof path, "%s.fnt", base) >= (int)sizeof path)
		return ASSET_LOAD_FAILED;
	AssetLoadStatus status = assets_probe_file(path);
	if (status != ASSET_LOAD_SUCCESS)
		return status;
	if (snprintf(path, sizeof path, "%s.ktx2", base) >= (int)sizeof path)
		return ASSET_LOAD_FAILED;
	status = assets_probe_file(path);
	if (status != ASSET_LOAD_SUCCESS)
		return status;
	if (!AeronFontAtlas_LoadKtx2(&slot->atlas, cmd, base))
		return ASSET_LOAD_FAILED;
	slot->atlas_scale = (float)XWA_ORIGINAL_FONT_SCALE;
	return ASSET_LOAD_SUCCESS;
}

static AssetLoadStatus assets_load_flight_font(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
											   int tier) {
	if (!a || !cmd || tier < 0 || tier >= 3)
		return ASSET_LOAD_FAILED;
	FlightFontSlot* slot = &a->flight_fonts[tier];
	if (slot->tried)
		return slot->atlas.loaded ? ASSET_LOAD_SUCCESS : ASSET_LOAD_FAILED;
	slot->tried = 1;
	const int first = a->prefer_original_2d ? ASSET_SOURCE_ORIGINAL : ASSET_SOURCE_MOD;
	for (int attempt = 0; attempt < 2; attempt++) {
		const int source = attempt == 0
							   ? first
							   : (first == ASSET_SOURCE_ORIGINAL ? ASSET_SOURCE_MOD : ASSET_SOURCE_ORIGINAL);
		const AssetLoadStatus status = source == ASSET_SOURCE_ORIGINAL
										   ? assets_load_original_flight_font(a, cmd, slot, tier)
										   : assets_load_mod_flight_font(a, cmd, tier);
		if (status == ASSET_LOAD_SUCCESS) {
			slot->source = (uint8_t)source;
			Aeron_LogInfo("xwa.remaster", "flight font tier %d: source=%s", tier, assets_source_name(source));
			return ASSET_LOAD_SUCCESS;
		}
		if (status != ASSET_LOAD_MISSING) {
			Aeron_LogError("xwa.remaster", "flight font tier %d: load failed", tier);
			return status;
		}
	}
	Aeron_LogWarn("xwa.remaster", "flight font tier %d: source=missing", tier);
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

const AeronFontAtlas* XwaRemasterAssets_FlightFont(XwaRemasterAssets* a,
		int tier, float* out_atlas_scale) {
	if (!a || tier < 0 || tier >= 3) {
		return NULL;
	}
	const FlightFontSlot* slot = &a->flight_fonts[tier];
	if (out_atlas_scale)
		*out_atlas_scale = slot->atlas_scale;
	return slot->atlas.loaded ? &slot->atlas : NULL;
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
			Aeron_LogWarn("xwa.remaster", "frontend font %d: original load failed: %s", font_size, error);
		return status;
	}
	if (!assets_init_original_font(cmd, &font, XWA_ORIGINAL_FONT_SCALE, 0,
			"XWA original frontend font", &slot->atlas)) {
		Xwa2dFontAtlas_Free(&font);
		return ASSET_LOAD_FAILED;
	}
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
	const int first = a->prefer_original_2d ? ASSET_SOURCE_ORIGINAL : ASSET_SOURCE_MOD;
	for (int attempt = 0; attempt < 2; attempt++) {
		const int source = attempt == 0
							   ? first
							   : (first == ASSET_SOURCE_ORIGINAL ? ASSET_SOURCE_MOD : ASSET_SOURCE_ORIGINAL);
		AssetLoadStatus status;
		if (source == ASSET_SOURCE_ORIGINAL) {
			status = assets_load_original_frontend_font(a, cmd, slot, font_size);
		} else {
			char basename[640], path[672];
			snprintf(basename, sizeof basename, "%s/frontend/fonts/font%d", a->root, font_size);
			snprintf(path, sizeof path, "%s.fnt", basename);
			status = assets_probe_file(path);
			if (status == ASSET_LOAD_SUCCESS &&
				!AeronFontAtlas_Load(&slot->atlas, cmd, basename))
				status = ASSET_LOAD_FAILED;
		}
		if (status == ASSET_LOAD_SUCCESS) {
			slot->source = (uint8_t)source;
			Aeron_LogInfo("xwa.remaster", "frontend font %d: source=%s", font_size, assets_source_name(source));
			*out = &slot->atlas;
			return ASSET_LOAD_SUCCESS;
		}
		memset(&slot->atlas, 0, sizeof slot->atlas);
		if (status != ASSET_LOAD_MISSING) {
			Aeron_LogError("xwa.remaster", "frontend font %d: load failed", font_size);
			return status;
		}
	}
	Aeron_LogWarn("xwa.remaster", "frontend font %d: source=missing", font_size);
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
	Aeron_LogInfo("xwa.remaster", "frontend fonts: %d/%u sizes loaded", loaded,
			  (unsigned)(sizeof font_sizes / sizeof font_sizes[0]));
	return 1;
}

static FaceFontSlot* assets_find_face_font(XwaRemasterAssets* a, int font_size, float em_pixels) {
	for (int i = 0; i < XWA_ASSETS_MAX_FACE_FONTS; i++) {
		FaceFontSlot* slot = &a->face_fonts[i];
		if (slot->atlas.loaded && slot->font_size == font_size && fabsf(slot->em_pixels - em_pixels) < 0.01f)
			return slot;
	}
	return NULL;
}

void XwaRemasterAssets_PrepareFaceFont(XwaRemasterAssets* a, AeronCommandBuffer* cmd, int font_size,
									   float em_pixels) {
	const XwaFrontendFont* font = XwaFrontendFonts_Get(font_size);
	if (!a || !cmd || !font || a->prefer_original_2d || !(em_pixels > 0.0f))
		return;
	a->face_font_clock++;
	FaceFontSlot* slot = assets_find_face_font(a, font_size, em_pixels);
	if (slot) {
		slot->used = a->face_font_clock;
		return;
	}
	/* Replace the slot used longest ago. */
	slot = &a->face_fonts[0];
	for (int i = 1; i < XWA_ASSETS_MAX_FACE_FONTS && slot->atlas.loaded; i++) {
		if (!a->face_fonts[i].atlas.loaded || a->face_fonts[i].used < slot->used)
			slot = &a->face_fonts[i];
	}
	AeronFontAtlas_Release(&slot->atlas);
	char name[64];
	snprintf(name, sizeof name, "frontend font %d at %.2f px", font_size, em_pixels);
	const AeronFontFaceAtlasDesc desc = {
		.em_pixels = em_pixels,
		.first_char = 32,
		.char_count = 224,
		.codepoints = font->codepoints + 32,
		.debug_name = name,
	};
	if (!AeronFontFace_BuildAtlas(font->face, cmd, &desc, &slot->atlas)) {
		Aeron_LogError("xwa.remaster", "%s: the atlas can't be built", name);
		return;
	}
	slot->font_size = font_size;
	slot->em_pixels = em_pixels;
	slot->used = a->face_font_clock;
	Aeron_LogInfo("xwa.remaster", "%s: %dx%d atlas", name, slot->atlas.atlas_w, slot->atlas.atlas_h);
}

const AeronFontAtlas* XwaRemasterAssets_FaceFont(XwaRemasterAssets* a, int font_size, float em_pixels) {
	const FaceFontSlot* slot = a ? assets_find_face_font(a, font_size, em_pixels) : NULL;
	return slot ? &slot->atlas : NULL;
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
