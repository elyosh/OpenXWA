#ifndef XWA_BAKE_SPRITES_H
#define XWA_BAKE_SPRITES_H

/* xwa_asset_bake — sprite bake pass (named .LST resources + DAT
 * atlas groups). See bake_sprites.c for the output layout. */

#include "aeron/vfs.h"
#include "ktx2_writer.h"

#include <stdbool.h>

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

typedef struct BakeStats {
	int sprites;    /* single-frame outputs */
	int atlases;    /* packed multi-frame atlases */
	int frame_dirs; /* oversized resources emitted frame-per-file */
	int skipped;    /* source files the engine's own loader rejects */
	int failures;
} BakeStats;

/* Run both passes. Returns nonzero when no failure occurred. */
int BakeSprites_Run(const BakeSpriteOptions* opt, AeronVfs* vfs, BakeStats* stats);

#endif /* XWA_BAKE_SPRITES_H */
