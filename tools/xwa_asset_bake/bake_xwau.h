#ifndef XWA_BAKE_XWAU_H
#define XWA_BAKE_XWAU_H

/* xwa_asset_bake xwau-frontend: converts XWAU's frontend art into atlases
 * that also give the game its frame counts, sizes and offsets. See
 * bake_xwau.c and docs/xwau_hd_concourse_plan.md (4.0-4.4, 4.12). */

#include "bake_sprites.h"

typedef struct BakeXwauOptions {
	const BakeSpriteOptions* sprites; /* out_root, formats and BC7 quality */
	const char* xwau_root;            /* the XWAU install */
	const char* stock_root;           /* the stock ALLIANCE folder */
} BakeXwauOptions;

typedef struct BakeXwauStats {
	int atlases;   /* atlases written */
	int hd_frames; /* frames taken from XWAU's HD images */
	int sd_frames; /* frames upscaled from XWAU's standard images */
	int unchanged; /* standard files equal to stock, without HD: left to the stock bake */
	int failures;
} BakeXwauStats;

/* Returns nonzero when no failure occurred. */
int BakeXwau_RunFrontend(const BakeXwauOptions* opt, BakeXwauStats* stats);

#endif
