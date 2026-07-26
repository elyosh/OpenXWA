#ifndef XWA_REMASTER_RUNTIME_ATLAS_H
#define XWA_REMASTER_RUNTIME_ATLAS_H

#include "aeron/render.h"
#include "aeron/scene/sprite_atlas.h"
#include "xwa_2d.h"

typedef struct XwaRuntimeAtlasPage {
	AeronTexture* texture;
	int width, height;
} XwaRuntimeAtlasPage;

typedef struct XwaRuntimeAtlas {
	AeronSpriteAtlas layout;
	XwaRuntimeAtlasPage* pages;
} XwaRuntimeAtlas;

int XwaRuntimeAtlas_Build(XwaRuntimeAtlas* atlas, AeronCommandBuffer* cmd, const Xwa2dFrameSet* source,
						  int generate_mips, const char* debug_name);
/* Premultiplies and uploads linear coverage RGBA, such as decoded bitmap fonts, as UNORM. */
AeronTexture* XwaRuntimeTexture_UploadLinearRgba(AeronCommandBuffer* cmd, uint8_t* rgba, int width,
												 int height, int generate_mips, const char* debug_name);
void XwaRuntimeAtlas_Free(XwaRuntimeAtlas* atlas);

#endif
