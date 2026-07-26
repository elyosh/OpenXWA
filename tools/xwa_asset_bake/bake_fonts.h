#ifndef XWA_BAKE_FONTS_H
#define XWA_BAKE_FONTS_H

/* xwa_asset_bake — frontend font pass: classic TIMES<size>.ABP bitmap
 * fonts -> HD .png + .fnt (TFNT v2) atlas pairs consumable by
 * AeronFontAtlas and tunable with TIE's font_tune GUI. */

#include "aeron/vfs.h"

typedef struct BakeFontsOptions {
	const char* out_root; /* bake output root (fonts/ appended) */
	int         scale;    /* integer NN upscale factor (RM_SCALE; 1 = classic) */
	/* Optional TTF: when set, fonts/font<size> is built by
	 * rasterizing this face at HD cell metrics matched to the classic
	 * font (the classic ABPs are cached GDI "Times New Roman"
	 * renders, so a Times-family TTF is the faithful source). The
	 * NN-upscaled classic atlas is then emitted as
	 * fonts/reference/font<size> for font_tune comparison. */
	const char* ttf_path;
} BakeFontsOptions;

/* Returns the number of font sizes baked (0 = nothing found/failed). */
int BakeFonts_Run(const BakeFontsOptions* opt, AeronVfs* vfs);

#endif /* XWA_BAKE_FONTS_H */
