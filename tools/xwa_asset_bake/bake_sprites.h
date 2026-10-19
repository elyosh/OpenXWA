#ifndef XWA_BAKE_SPRITES_H
#define XWA_BAKE_SPRITES_H

/* xwa_asset_bake — sprite bake pass (named .LST resources + DAT
 * atlas groups). See bake_sprites.c for the output layout. */

#include "aeron/atlas_pack.h"
#include "aeron/vfs.h"
#include "ktx2_writer.h"

#include <stdbool.h>
#include <stdint.h>

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

typedef struct BakeFrame {
	uint8_t* rgba; /* HD (or classic when !scale) straight-alpha */
	int      w, h;
	int      classic_w, classic_h;
	int      origin_x, origin_y; /* classic units (DAT anchors) */
	int      has_light;          /* write `light: [r, g, b]` */
	float    light[3];
} BakeFrame;

typedef struct BakeStats {
	int sprites;    /* single-frame outputs */
	int atlases;    /* packed multi-frame atlases */
	int frame_dirs; /* oversized resources emitted frame-per-file */
	int skipped;    /* source files the engine's own loader rejects */
	int failures;
} BakeStats;

/* Run both passes. Returns nonzero when no failure occurred. */
int BakeSprites_Run(const BakeSpriteOptions* opt, AeronVfs* vfs, BakeStats* stats);

/* Helpers shared with the XWAU frontend import (bake_xwau.c). */
int BakeSprites_MakeDirs(const char* path);
/* Writes `base_path`.png and/or .ktx2 (BC7, sRGB, premultiplied) per `opt`. */
int BakeSprites_EmitImage(const BakeSpriteOptions* opt, const char* base_path, const uint8_t* rgba, int w,
						  int h);
/* Packs frames into pages of at most 8192x8192 texels with an 8-texel gutter.
 * `max_pages` 0 allows any number of pages. */
int BakeSprites_BuildAtlas(const BakeFrame* frames, int count, int max_pages,
						   AeronAtlasImage** out_images, AeronCpuAtlas* out);
/* Writes the layout YAML of an atlas that BakeSprites_BuildAtlas packed:
 * each frame's id (when `ids` isn't NULL), page, rectangle, anchor and classic
 * size, and `light` for a frame with `has_light`. `header`, when not NULL, is
 * written first, as is. */
int BakeSprites_WriteAtlasYaml(const char* path, const AeronAtlasImage* images, const BakeFrame* frames,
							   const uint16_t* ids, int count, const AeronCpuAtlas* atlas, int classic_atlas_w,
							   int classic_atlas_h, const char* header);

#endif /* XWA_BAKE_SPRITES_H */
