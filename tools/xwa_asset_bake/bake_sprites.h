#ifndef XWA_BAKE_SPRITES_H
#define XWA_BAKE_SPRITES_H

/* xwa_asset_bake — sprite bake pass (named .LST resources + DAT
 * atlas groups). See bake_sprites.c for the output layout. */

#include "aeron/atlas_pack.h"
#include "aeron/vfs.h"
#include "ktx2_writer.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct BakeSpriteOptions {
	const char*    out_root;     /* output directory root */
	bool           write_png;    /* straight-alpha PNG (artist source) */
	bool           write_ktx2;   /* BC7 KTX2 (game input) */
	bool           zstd;         /* KTX2 zstd supercompression */
	bool           scale;        /* SVGA→4K HD upscale (off = classic 1:1) */
	Ktx2Bc7Quality bc7_quality;
	const char*    list_filter;  /* only frontres files matching this */
	int            group_filter; /* only this DAT group (-1 = all) */
	bool           dat_only;     /* skip the frontres pass */
} BakeSpriteOptions;

/* A frame of an atlas. `size`, `offset` and `bounds` are in the atlas's
 * units (BakeAtlasHeader). */
typedef struct BakeFrame {
	uint8_t* rgba; /* straight alpha */
	int w, h;      /* in pixels */
	int has_id;
	int id;           /* the DAT image id */
	double size[2];   /* the draw size */
	double offset[2]; /* the anchor */
	int has_bounds;
	double bounds[4];        /* left, top, right, bottom */
	const char* keep_aspect; /* NULL, "height" or "width" */
	const char* fit;         /* NULL or "screen_height" */
	int has_light;
	float light[3];
} BakeFrame;

typedef struct BakeRegion {
	int type;
	double rect[4];
} BakeRegion;

/* The keys of an atlas file apart from `atlas` and `frames`. */
typedef struct BakeAtlasHeader {
	const char* comment; /* lines that start with '#', or NULL */
	int game_units;      /* `units: game`; else design units */
	int game_geometry;
	int classic_frames; /* 0: not written */
	int fps;            /* 0: not written */
	const char* tint;   /* NULL: not written */
	const char* layout; /* the screen's layout while the image is its background; NULL: not written */
	int has_draw_adjust;
	double draw_adjust[4];
	int has_bounds;
	double bounds[4];
	const BakeRegion* regions;
	int region_count;
	const char* extra; /* more keys, written as they are, such as `frame_sets` */
} BakeAtlasHeader;

typedef struct BakeStats {
	int sprites;    /* single-frame outputs */
	int atlases;    /* packed multi-frame atlases */
	int frame_dirs; /* oversized resources emitted frame-per-file */
	int skipped;    /* source files the engine's own loader rejects */
	int failures;
} BakeStats;

/* Run both passes. Returns nonzero when no failure occurred. */
int BakeSprites_Run(const BakeSpriteOptions* opt, AeronVfs* vfs, BakeStats* stats);

/* Helpers shared with the font bakes and the mod importer's frontend art. */
int BakeSprites_MakeDirs(const char* path);
/* Writes `base_path`.png and/or .ktx2 (BC7, sRGB, premultiplied) per `opt`.
 * While the encode queue is on, it copies the image for the queue's threads
 * and returns 1 at once. BakeSprites_EndQueue then reports the images that
 * failed. */
int BakeSprites_EmitImage(const BakeSpriteOptions* opt, const char* base_path, const uint8_t* rgba, int w,
						  int h);
/* Turns the encode queue on, with `threads` threads. Each image is then
 * encoded and written on one of them. With fewer than 2 threads, the images
 * are still written at once. Call it and BakeSprites_EndQueue from one
 * thread, which is the only one that emits images in between. */
void BakeSprites_BeginQueue(int threads);
/* The number of queued images that have failed so far. A bake that stops at
 * its first failure checks it. */
int BakeSprites_QueueFailures(void);
/* Waits for the queued images, turns the queue off, and returns the number
 * of images that failed. */
int BakeSprites_EndQueue(void);
/* The size of an atlas page, and the gutter between its frames. */
#define BAKE_MAX_ATLAS_DIM 8192
#define BAKE_HD_PAD        8
/* The largest page that a frame too large for a BAKE_MAX_ATLAS_DIM page gets
 * on its own. Every GPU that SDL's GPU API supports on D3D12 and Metal can
 * create a texture of this size. */
#define BAKE_MAX_OVERSIZE_DIM 16384

/* Packs frames into pages of at most BAKE_MAX_ATLAS_DIM texels square, with a
 * BAKE_HD_PAD gutter. A frame too large for such a page gets a page of its
 * own, of at most BAKE_MAX_OVERSIZE_DIM. `max_pages` 0 allows any number of
 * pages. */
int BakeSprites_BuildAtlas(const BakeFrame* frames, int count, int max_pages,
						   AeronAtlasImage** out_images, AeronCpuAtlas* out);
/* Writes `dir`/`base`.yaml and the pages of an atlas that
 * BakeSprites_BuildAtlas packed: `base`, then `base`_p<N>. */
int BakeSprites_WriteAtlasFiles(const BakeSpriteOptions* opt, const char* dir, const char* base,
								const BakeAtlasHeader* header, const BakeFrame* frames, int count,
								const AeronAtlasImage* placed, const AeronCpuAtlas* pages);
/* Packs `frames` into any number of pages and writes them with
 * BakeSprites_WriteAtlasFiles. Returns 0 and logs the reason on failure. */
int BakeSprites_PackAndWriteAtlas(const BakeSpriteOptions* opt, const char* dir, const char* base,
								  const BakeAtlasHeader* header, const BakeFrame* frames, int count);
/* Writes `[a, b, ...]`, with whole numbers without decimals. */
void BakeSprites_WriteNumberList(FILE* fp, const double* values, int count);

#endif /* XWA_BAKE_SPRITES_H */
