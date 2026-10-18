#ifndef XWA_BAKE_XWAU_H
#define XWA_BAKE_XWAU_H

/* xwa_asset_bake xwau-frontend: converts XWAU's frontend art into atlases.
 * The atlases also hold the frame counts, sizes and offsets that the game
 * uses. See bake_xwau.c. */

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
	int unchanged; /* files that match stock and have no HD image; the stock bake handles them */
	int failures;
} BakeXwauStats;

/* Returns nonzero when no failure occurred. */
int BakeXwau_RunFrontend(const BakeXwauOptions* opt, BakeXwauStats* stats);

#endif
