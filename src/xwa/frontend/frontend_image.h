#ifndef XWA_FRONTEND_FRONTEND_IMAGE_H
#define XWA_FRONTEND_FRONTEND_IMAGE_H

#include "xwa/assets/file_io.h"
#include "xwa/frontend/frontend_rect.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	FRONT_IMAGE_MAX_RESOURCES = 1024,
};

typedef struct ImageResource {
	int width;
	int height;
	int isCompressed;
	int pixelCount;
	int boundsLeft;
	int boundsTop;
	int boundsRight;
	int boundsBottom;
	unsigned char* pixels;
	int colorLUT[256];
	unsigned char palette[1024];
} ImageResource;

typedef struct ResourceDescriptor {
	int frameCount;
	int currentFrame;
	FrontendRect bounds;
	int atlasBaseIndex;
	int atlasGroupId;
	ImageResource* image;
} ResourceDescriptor;

typedef struct ResourceEntry {
	char name[64];
	ResourceDescriptor* desc;
} ResourceEntry;

typedef struct FrontImageBmpFileHeader FrontImageBmpFileHeader;

extern ResourceEntry* g_resourceTable;
extern int g_resourceCount;
extern int g_displayBpp;
extern unsigned char g_pixelFormat555;
extern unsigned char* g_drawSurfacePtr;
extern int g_drawSurfacePitch;
extern unsigned char g_rleRowBuffer[0x10004];
extern int g_clipMinX;
extern int g_clipMaxX;
extern int g_clipMinY;
extern int g_clipMaxY;
extern int g_bmpSaveEnabled;

int FrontImage_RegisterResourceDefault(const char* fileName, const char* name);
int FrontImage_RegisterResource(const char* fileName, const char* name, int flags, int id);
void FrontImage_FreeResourceByName(const char* name);
void FrontImage_FreeAllResources(void);
int FrontImage_ResourceExists(const char* name);
int FrontImage_GetResourceRect(const char* name, FrontendRect* out);
int FrontImage_GetSpriteFrame(const char* name);
int FrontImage_AdvanceSpriteFrame(const char* name, int loop);
int FrontImage_RewindSpriteFrame(const char* name, int wrap);
int FrontImage_SetSpriteFrame(const char* name, int frame);

int FrontImage_LoadResourceList(char* fileName);
int FrontImage_UnloadResourceList(char* fileName);

int FrontImage_LoadCbmResource(const char* srcFile, const char* name);
int FrontImage_WriteCbmResourceCache(const char* srcFile, ResourceDescriptor* desc);

int FrontImage_InsertResourceSorted(ResourceEntry* entry);
void FrontImage_RemoveResourceAt(int index);
int FrontImage_FindResourceByName(const char* name);
int FrontImage_BSearchResource(ResourceEntry* table, int hi, const char* key);

int FrontImage_RegisterFlicResource(char* fileName, char* name, int remapPalette, int compress);
int FlicChunk_Color(XwaFile* stream, unsigned char* paletteRgbx);
int FlicChunk_DeltaFlc(XwaFile* stream, const unsigned char* chunkHeader, ImageResource* image);
int FlicChunk_ByteRun(XwaFile* stream, const unsigned char* chunkHeader, ImageResource* image);
void FrontImage_ReadBmpPalette(XwaFile* stream, char* dest, int count);
int FrontImage_DecodeBmp4bpp(XwaFile* stream, void* dstPixels, const FrontImageBmpFileHeader* fileHeader,
							 int* infoHeader);
int FrontImage_DecodeBmp8bpp(XwaFile* stream, void* dstPixels, void* fileHeader, int* infoHeader);
int FrontImage_LoadBmpFile(char* fileName, ImageResource* image, int makePalette, int upload);
int FrontImage_ComputeImageBounds(ImageResource* image, int* outBounds);
int FrontImage_CompressRLE(ImageResource* image);
void FrontImage_RemapPalette(unsigned char* pixels, unsigned char* srcPalette, int width, int height);
char FrontImage_RemapPaletteIndex(unsigned char* srcRgb, int srcIndex);
int FrontImage_RebuildPaletteCache(void);
int FrontImage_SaveBmpFile(char* fileName, const void* pixels, int width, int height, int pitch, int bpp,
						   int is555, const void* palette);

int FrontImage_DrawSprite(const char* name, int x, int y);
int FrontImage_DrawAtlasSprite(int16_t groupId, int16_t index, int16_t x, int16_t y);
void FrontImage_BuildAtlasBlendLut(void);
int FrontImage_InitAtlasSprites(void);
int FrontImage_RegisterAtlasSprites(void);
int FrontImage_RegisterAtlasSprite(const char* name, uint16_t groupId, uint16_t baseIndex,
								   uint16_t frameCount);
int FrontImage_CreateAtlasResource(const char* name, int groupId, int baseIndex, int frameCount,
								   const FrontendRect* bounds);
int FrontImage_GetAtlasSpriteBounds(FrontendRect* out, int groupId, uint16_t index);
void FrontImage_BlitAtlasSprite(int16_t x, int16_t y);
int FrontImage_UnloadAtlasSpriteGroups(void);
int FrontImage_FreeAtlasResources(void);
int FrontImage_BlitClipped(ImageResource* image, int x, int y);
void FrontImage_BlitRLE8(ImageResource* image, int destX, int destY, int srcLeft, int srcTop,
						 int visibleWidth, int visibleHeight);
void FrontImage_BlitRLE16(ImageResource* image, int destX, int destY, int srcLeft, int srcTop,
						  int visibleWidth, int visibleHeight);

int FrontImage_DrawSpriteOpaque(const char* name, int x, int y);
int FrontImage_BlitOpaque(ImageResource* image, int x, int y);
void FrontImage_BlitRLE8Opaque(ImageResource* image, int destX, int destY, int srcLeft, int srcTop,
							   int visibleWidth, int visibleHeight);
void FrontImage_BlitRLE16Opaque(ImageResource* image, int destX, int destY, int srcLeft, int srcTop,
								int visibleWidth, int visibleHeight);

int FrontImage_DrawSpriteTranslucent(const char* name, int x, int y);
int FrontImage_BlitTranslucent(ImageResource* image, int x, int y);
int FrontImage_BlitRLE16Translucent(ImageResource* image, int dstX, int dstY, int srcX, int srcY, int width,
									int height);
int FrontImage_DrawSpriteRectTransparent(const char* name, FrontendRect* srcRect, int dstX, int dstY);
int FrontImage_BlitRectTransparent(ImageResource* image, FrontendRect* srcRect, int dstX, int dstY);
int FrontImage_DrawSpriteRectTinted(const char* name, FrontendRect* srcRect, int dstX, int dstY,
									unsigned int tintColor);
int FrontImage_BlitRectTinted(ImageResource* image, FrontendRect* srcRect, int dstX, int dstY,
							  unsigned int tintColor);
int FrontImage_DrawSpriteRectBlendMode(const char* name, FrontendRect* srcRect, int dstX, int dstY,
									   int blendMode);
int FrontImage_BlitRectBlendMode(ImageResource* image, FrontendRect* srcRect, int dstX, int dstY,
								 int blendMode);
int FrontImage_DrawSpriteRectTintedBlendMode(const char* name, FrontendRect* srcRect, int dstX, int dstY,
											 unsigned int tintColor, int blendMode);
int FrontImage_BlitRectTintedBlendMode(ImageResource* image, FrontendRect* srcRect, int dstX, int dstY,
									   unsigned int tintColor, int blendMode);
int FrontImage_DrawGlyph(intptr_t* glyph, int x, int y, unsigned int color, int allowColorRemap);
void FrontImage_BlitGlyphRLE_8bpp(intptr_t* glyph, int destX, int destY, int clipLeftSkip, int clipTopSkip,
								  int visibleWidth, int visibleRows, unsigned char color);
void FrontImage_BlitGlyphRLE_16bpp(intptr_t* glyph, int destX, int destY, int clipLeftSkip, int clipTopSkip,
								   int visibleWidth, int visibleRows, unsigned int color);
int FrontImage_EncodeGlyphRow(int* rowRecord, unsigned char* srcPixels, int width);
unsigned int FrontImage_GetFadedGlyphColor16(uint16_t color16);

#ifdef __cplusplus
}
#endif

#endif
