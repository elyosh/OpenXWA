#ifndef XWA_BAKE_FONTS_H
#define XWA_BAKE_FONTS_H

/* xwa_asset_bake frontend font pass: converts the TIMES<size>.ABP bitmap
 * fonts to .png and .fnt (TFNT v2) atlases for AeronFontAtlas. */

#include "aeron/vfs.h"

typedef struct BakeFontsOptions {
	const char* out_root; /* bake output root (frontend/fonts/ appended) */
	int         scale;    /* integer NN upscale factor (RM_SCALE; 1 = classic) */
} BakeFontsOptions;

/* Returns the number of font sizes baked (0 = nothing found/failed). */
int BakeFonts_Run(const BakeFontsOptions* opt, AeronVfs* vfs);

#endif /* XWA_BAKE_FONTS_H */
