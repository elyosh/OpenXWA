#ifndef XWA_ASSETS_SPRITE_TEXTURE_H
#define XWA_ASSETS_SPRITE_TEXTURE_H

#include "xwa/assets/sprite_resource.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	SPRITE_TEXTURE_FORMAT_ARGB8888 = 1,
	SPRITE_TEXTURE_FORMAT_RGB555 = 2,
	SPRITE_TEXTURE_FORMAT_RGB565_CK = 3,
	SPRITE_TEXTURE_FORMAT_ARGB4444 = 4,
	SPRITE_TEXTURE_FORMAT_RGB555_OPAQUE = 5,
	SPRITE_TEXTURE_FORMAT_RGB565 = 6,
	SPRITE_TEXTURE_FORMAT_PAL555_CK = 9,
	SPRITE_TEXTURE_FORMAT_PAL565_CK = 10,
	SPRITE_TEXTURE_FORMAT_PAL555_OPAQUE = 12,
	SPRITE_TEXTURE_FORMAT_PAL565_OPAQUE = 13,
	SPRITE_TEXTURE_FORMAT_CK_BORDER = 0x1000,
};

typedef struct TexLevel {
	Sprite* image;
	uint16_t width;
	uint16_t height;
	int32_t argbColor;
	uint8_t shift;
} TexLevel;

extern uint8_t g_colorKeyAlphaThreshold;

void SpriteTexture_ConvertById(int16_t groupId, uint16_t spriteId, int format, TexLevel* texLevels,
							   char baseShift, uint16_t levelCount);
void SpriteTexture_ConvertByIndex(int16_t groupId, uint16_t spriteIndex, int format, TexLevel* texLevels,
								  char baseShift, uint16_t levelCount);
void SpriteTexture_ConvertSprite(Sprite* sprite, int format, TexLevel* texLevels, char baseShift,
								 uint16_t levelCount);

void SpriteTexture_EncodeARGB8888(Sprite* sprite, const uint8_t* colorTable24, const uint8_t* rows,
								  int format, TexLevel* texLevels, uint16_t levelCount);
void SpriteTexture_EncodeRGB555(Sprite* sprite, const uint8_t* colorTable24, unsigned int colorCount,
								const uint8_t* rows, int format, TexLevel* texLevels, uint16_t levelCount);
void SpriteTexture_EncodeRGB565(Sprite* sprite, const uint8_t* colorTable24, unsigned int colorCount,
								const uint8_t* rows, int format, TexLevel* texLevels, uint16_t levelCount);
void SpriteTexture_EncodePaletted(Sprite* sprite, const uint8_t* colorTable24, unsigned int colorCount,
								  const uint8_t* rows, int format, TexLevel* texLevels, uint16_t levelCount);
void SpriteTexture_EncodeARGB4444(Sprite* sprite, const uint8_t* colorTable24, unsigned int colorCount,
								  const uint8_t* rows, int format, TexLevel* texLevels, uint16_t levelCount);
void SpriteTexture_BuildPalette16FromRGB24(const uint8_t* colorTable24, unsigned int colorCount,
										   uint16_t* outPalette16, int format);
void SpriteTexture_DownsampleMip24(const uint8_t* rows, uint16_t width, uint16_t height,
								   const uint8_t* colorTable24, uint16_t shift, uint32_t* outRGBA);

void SpriteTexture_FreeTexLevelImages(TexLevel* texLevels, uint16_t levelCount);

#ifdef __cplusplus
}
#endif

#endif
