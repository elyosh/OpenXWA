#include "xwa/assets/sprite_texture.h"

#include "xwa/util/byte_order.h"
#include "xwa/util/memory.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SPRITE_TEXTURE_HEADER_SIZE 18
#define SPRITE_TEXTURE_FORMAT_MASK 0xefff

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define SPRITE_TEXTURE_PACKED_STRUCT
#else
#define SPRITE_TEXTURE_PACKED_STRUCT __attribute__((packed))
#endif

typedef struct SPRITE_TEXTURE_PACKED_STRUCT SpriteTextureSpriteHeader {
	uint16_t type;
	uint16_t width;
	uint16_t height;
	uint16_t colorKey;
	uint16_t field8;
	uint16_t groupId;
	uint16_t spriteId;
	uint32_t pixelDataSize;
} SpriteTextureSpriteHeader;

typedef struct SPRITE_TEXTURE_PACKED_STRUCT SpriteTexturePayloadHeader {
	uint32_t payloadSize;
	uint32_t colorTable24Offset;
	uint32_t rowDataOffset;
	uint32_t palette16Offset;
	uint32_t field22;
	uint32_t field26;
	uint32_t anchorX;
	uint32_t anchorY;
	uint32_t field32;
	uint32_t field36;
	uint32_t colorCount;
} SpriteTexturePayloadHeader;

typedef struct SPRITE_TEXTURE_PACKED_STRUCT SpriteTextureIndexEntry {
	uint16_t spriteId;
	uint32_t dataOffset;
	uint16_t field6;
} SpriteTextureIndexEntry;

typedef struct SpriteTextureColor24 {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
} SpriteTextureColor24;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif
#undef SPRITE_TEXTURE_PACKED_STRUCT

enum {
	SPRITE_TEXTURE_TYPE_OFFSET = 0,
	SPRITE_TEXTURE_WIDTH_OFFSET = 2,
	SPRITE_TEXTURE_HEIGHT_OFFSET = 4,
	SPRITE_TEXTURE_COLOR_KEY_OFFSET = 6,
	SPRITE_TEXTURE_FIELD_8_OFFSET = 8,
	SPRITE_TEXTURE_PIXEL_DATA_SIZE_OFFSET = 14,
};

enum {
	SPRITE_TEXTURE_PAYLOAD_COLOR_TABLE_24_OFFSET_OFFSET = 4,
	SPRITE_TEXTURE_PAYLOAD_ROW_DATA_OFFSET_OFFSET = 8,
	SPRITE_TEXTURE_PAYLOAD_PALETTE_16_OFFSET_OFFSET = 12,
	SPRITE_TEXTURE_PAYLOAD_COLOR_COUNT_OFFSET = 40,
	SPRITE_TEXTURE_TYPE24_HEADER_SIZE = 44,
};

#define SPRITE_TEXTURE_CONVERT_ICON_TAG "CONVERTICONBUF"
#define SPRITE_TEXTURE_MIP_SCRATCH_TAG "GETMIPBITTEMP"
#define SPRITE_TEXTURE_RESOURCE_TAG "RESOURCEITEM"
#define SPRITE_TEXTURE_SAMPLE_DOWN_TAG "SAMPLEDOWN"

// GLOBAL: XWA 0x5BA4B0
uint8_t g_colorKeyAlphaThreshold = 0x4b;

// FUNCTION: XWA 0x4CE0F0
void SpriteTexture_ConvertById(int16_t groupId, uint16_t spriteId, int format, TexLevel* texLevels,
							   char baseShift, uint16_t levelCount) {
	Sprite* sprite = SpriteResource_ResolveSprite(groupId, spriteId);

	if (sprite != NULL) {
		SpriteTexture_ConvertSprite(sprite, format, texLevels, baseShift, levelCount);
	}
}

// FUNCTION: XWA 0x4CE130
void SpriteTexture_ConvertByIndex(int16_t groupId, uint16_t spriteIndex, int format, TexLevel* texLevels,
								  char baseShift, uint16_t levelCount) {
	uint16_t groupIndex = 0;
	uint16_t resolvedGroupIndex;

	while (groupIndex < SPRITE_RESOURCE_MAX_GROUPS) {
		if (g_spriteGroups[groupIndex].groupId == groupId) {
			break;
		}
		++groupIndex;
	}

	if (groupIndex < SPRITE_RESOURCE_MAX_GROUPS) {
		resolvedGroupIndex = groupIndex;
	} else {
		resolvedGroupIndex = (uint16_t)SpriteResource_LoadGroup(groupId);
	}

	if (resolvedGroupIndex < SPRITE_RESOURCE_MAX_GROUPS) {
		Sprite* sprite = NULL;

		if (spriteIndex < (uint16_t)g_spriteGroups[resolvedGroupIndex].spriteCount) {
			SpriteTextureIndexEntry* indexEntry =
				&((SpriteTextureIndexEntry*)g_spriteGroups[resolvedGroupIndex].indexBase)[spriteIndex];

			sprite = (Sprite*)(g_spriteGroups[resolvedGroupIndex].dataBase + indexEntry->dataOffset);
		}

		if (sprite != NULL) {
			SpriteTexture_ConvertSprite(sprite, format, texLevels, baseShift, levelCount);
		}
	}
}

// FUNCTION: XWA 0x4CE1D0
void SpriteTexture_ConvertSprite(Sprite* sprite, int format, TexLevel* texLevels, char baseShift,
								 uint16_t levelCount) {
	int spriteType;
	Sprite* convertedType7 = NULL;

	spriteType = sprite->type;
	if (spriteType != 7) {
		if (spriteType <= 23 || spriteType > 25) {
			return;
		}
	} else {
		if (sprite != NULL) {
			uint8_t* sourcePayload = SpriteResource_GetMutableSpritePayload(sprite);
			SpriteTexturePayloadHeader* sourceHeader = (SpriteTexturePayloadHeader*)sourcePayload;
			uint32_t colorCount = sourceHeader->colorCount;
			uint8_t* sourceRows = sourcePayload + sourceHeader->rowDataOffset;
			const uint8_t* colorTable24 = sourcePayload + sourceHeader->colorTable24Offset;

			if (colorCount <= 256) {
				uint32_t width = sprite->width;
				uint32_t height = sprite->height;
				uint32_t rowBytes = width * height;
				uint32_t colorBytes = 3 * colorCount;
				uint32_t totalSize;

				rowBytes <<= 1;
				totalSize =
					SPRITE_TEXTURE_HEADER_SIZE + SPRITE_TEXTURE_TYPE24_HEADER_SIZE + colorBytes + rowBytes;

				convertedType7 = (Sprite*)Memory_AllocTagged(SPRITE_TEXTURE_CONVERT_ICON_TAG, totalSize);
				if (convertedType7 != NULL) {
					uint8_t* convertedPayload;
					SpriteTexturePayloadHeader* convertedPayloadHeader;
					uint8_t* convertedRows;
					uint16_t row;

					*(SpriteTextureSpriteHeader*)convertedType7 = *(SpriteTextureSpriteHeader*)sprite;
					convertedType7->pixelDataSize = totalSize;
					convertedType7->type = 24;

					convertedPayload = SpriteResource_GetMutableSpritePayload(convertedType7);
					convertedPayloadHeader = (SpriteTexturePayloadHeader*)convertedPayload;
					memcpy(convertedPayloadHeader, sourcePayload, sizeof(*convertedPayloadHeader));
					convertedPayloadHeader->payloadSize = totalSize;
					convertedPayloadHeader->palette16Offset = 0;
					memcpy(convertedPayload + SPRITE_TEXTURE_TYPE24_HEADER_SIZE, colorTable24, colorBytes);
					convertedPayloadHeader->colorTable24Offset = SPRITE_TEXTURE_TYPE24_HEADER_SIZE;
					convertedPayloadHeader->rowDataOffset = SPRITE_TEXTURE_TYPE24_HEADER_SIZE + colorBytes;

					convertedRows = convertedPayload + convertedPayloadHeader->rowDataOffset;
					for (row = 0; row < convertedType7->height; ++row) {
						uint8_t segmentCount = *sourceRows++;

						while (segmentCount-- != 0) {
							uint16_t control = *sourceRows++;
							uint16_t count = (uint16_t)(control & 0x7f);

							if ((control & 0x80) != 0) {
								memset(convertedRows, 0, 2u * count);
								convertedRows += 2u * count;
							} else {
								while (count-- != 0) {
									*convertedRows++ = *sourceRows++;
									*convertedRows++ = 0xff;
								}
							}
						}
					}
				}
			}
		}

		sprite = convertedType7;
	}

	{
		uint8_t* imagePayload;
		const uint8_t* rows;
		const uint8_t* colorTable24;
		uint32_t colorCount;
		uint16_t convertLevelCount;
		uint16_t level;
		TexLevel* convertTexLevels;
		TexLevel* levelCursor;

		convertLevelCount = levelCount;
		convertTexLevels = texLevels;
		levelCursor = convertTexLevels;
		memset(convertTexLevels, 0, sizeof(*convertTexLevels) * convertLevelCount);
		for (level = 0; level < convertLevelCount; ++level, ++levelCursor) {
			uint8_t shift = (uint8_t)(baseShift + level);
			uint16_t width = (uint16_t)(sprite->width >> shift);
			uint16_t height = (uint16_t)(sprite->height >> shift);

			if (width < 8 || height < 8) {
				if (level != 0) {
					break;
				}

				shift = 0;
				width = sprite->width;
				height = sprite->height;
			}

			levelCursor->shift = shift;
			levelCursor->width = width;
			levelCursor->height = height;
			levelCursor->image = NULL;
			levelCursor->argbColor = -1;
		}

		imagePayload = SpriteResource_GetMutableSpritePayload(sprite);
		rows = imagePayload + ((SpriteTexturePayloadHeader*)imagePayload)->rowDataOffset;
		colorTable24 = imagePayload + ((SpriteTexturePayloadHeader*)imagePayload)->colorTable24Offset;
		colorCount = ((SpriteTexturePayloadHeader*)imagePayload)->colorCount;

		if (colorCount <= 256) {
			switch (format & SPRITE_TEXTURE_FORMAT_MASK) {
				case SPRITE_TEXTURE_FORMAT_ARGB8888:
					SpriteTexture_EncodeARGB8888(sprite, colorTable24, rows, format, convertTexLevels,
												 convertLevelCount);
					break;
				case SPRITE_TEXTURE_FORMAT_RGB555:
				case SPRITE_TEXTURE_FORMAT_RGB555_OPAQUE:
					SpriteTexture_EncodeRGB555(sprite, colorTable24, colorCount, rows, format,
											   convertTexLevels, convertLevelCount);
					break;
				case SPRITE_TEXTURE_FORMAT_RGB565_CK:
				case SPRITE_TEXTURE_FORMAT_RGB565:
					SpriteTexture_EncodeRGB565(sprite, colorTable24, colorCount, rows, format,
											   convertTexLevels, convertLevelCount);
					break;
				case SPRITE_TEXTURE_FORMAT_PAL555_CK:
				case SPRITE_TEXTURE_FORMAT_PAL565_CK:
				case SPRITE_TEXTURE_FORMAT_PAL555_OPAQUE:
				case SPRITE_TEXTURE_FORMAT_PAL565_OPAQUE:
					SpriteTexture_EncodePaletted(sprite, colorTable24, colorCount, rows, format,
												 convertTexLevels, convertLevelCount);
					break;
				case SPRITE_TEXTURE_FORMAT_ARGB4444:
					SpriteTexture_EncodeARGB4444(sprite, colorTable24, colorCount, rows, format,
												 convertTexLevels, convertLevelCount);
					break;
				default:
					break;
			}

			if (convertedType7 != NULL) {
				Memory_FreeTagged(SPRITE_TEXTURE_CONVERT_ICON_TAG, convertedType7);
			}
		}
	}
}

// FUNCTION: XWA 0x4CE4F0
void SpriteTexture_EncodeARGB8888(Sprite* sprite, const uint8_t* colorTable24, const uint8_t* rows,
								  int format, TexLevel* texLevels, uint16_t levelCount) {
	unsigned int level;
	unsigned int levelLimit;
	uint32_t pixelCount;
	uint32_t count;
	uint8_t* out;
	const SpriteTextureColor24* colors;

	if (sprite != NULL && colorTable24 != NULL && rows != NULL && texLevels != NULL && levelCount != 0) {
		colors = (const SpriteTextureColor24*)colorTable24;
		level = 0;
		levelLimit = levelCount;
		while (level < levelLimit) {
			Sprite* texture;

			if (texLevels->width == 0) {
				break;
			}

			pixelCount = sprite->width;
			pixelCount *= sprite->height;
			pixelCount >>= 2 * texLevels->shift;
			texture = (Sprite*)Memory_AllocTagged(SPRITE_TEXTURE_RESOURCE_TAG,
												  SPRITE_TEXTURE_HEADER_SIZE + 4u * pixelCount);

			if (texture != NULL) {
				Sprite* source = sprite;

				memcpy(texture, source, SPRITE_TEXTURE_HEADER_SIZE);
				texture->type = SPRITE_TEXTURE_FORMAT_ARGB8888;
				out = (uint8_t*)texture + SPRITE_TEXTURE_HEADER_SIZE;

				if (texLevels->shift == 0) {
					const uint8_t* src = rows;

					count = pixelCount - 1;
					if (pixelCount != 0) {
						++count;
						do {
							unsigned int index = *src++;
							uint32_t alpha = (uint32_t)*src++ << 24;
							uint32_t argb = alpha | ((uint32_t)colors[index].red << 16) |
											((uint32_t)colors[index].green << 8) | colors[index].blue;

							memcpy(out, &argb, sizeof(argb));
							out += sizeof(argb);
							--count;
						} while (count != 0);
					}
				} else {
					uint32_t pixelDataSize = texture->pixelDataSize;

					texture->width = (uint16_t)(texture->width >> texLevels->shift);
					texture->height = (uint16_t)(texture->height >> texLevels->shift);
					texture->colorKey = (uint16_t)(texture->colorKey >> texLevels->shift);
					texture->field8 = (uint16_t)(texture->field8 >> texLevels->shift);
					texture->pixelDataSize = pixelDataSize >> (2 * texLevels->shift);
					SpriteTexture_DownsampleMip24(rows, source->width, source->height, colorTable24,
												  texLevels->shift, (uint32_t*)out);
				}

				if ((format & SPRITE_TEXTURE_FORMAT_CK_BORDER) != 0) {
					unsigned int width = texture->width;
					uint32_t* top = (uint32_t*)((uint8_t*)texture + SPRITE_TEXTURE_HEADER_SIZE);
					uint32_t* bottom = top + width * (texture->height - 1);
					unsigned int x = 0;

					if (width > 0) {
						do {
							*top++ &= 0x00ffffffu;
							*bottom++ &= 0x00ffffffu;
							++x;
						} while (x < texture->width);
					}

					{
						unsigned int y = 1;
						uint32_t* row =
							(uint32_t*)((uint8_t*)texture + SPRITE_TEXTURE_HEADER_SIZE) + texture->width;

						if ((uint32_t)texture->height - 1 > y) {
							do {
								row[0] &= 0x00ffffffu;
								row[texture->width - 1] &= 0x00ffffffu;
								row += texture->width;
								++y;
							} while (y < (uint32_t)texture->height - 1);
						}
					}
				}
			}

			texLevels->image = texture;
			++texLevels;
			++level;
		}
	}
}

// FUNCTION: XWA 0x4CE750
void SpriteTexture_EncodeRGB555(Sprite* sprite, const uint8_t* colorTable24, unsigned int colorCount,
								const uint8_t* rows, int format, TexLevel* texLevels, uint16_t levelCount) {
	uint16_t palette[256];
	unsigned int alphaValue;
	uint32_t count;
	unsigned int level;
	unsigned int levelLimit;
	int opaque;

	if (sprite == NULL) {
		return;
	}
	if (colorTable24 == NULL) {
		return;
	}
	if (rows == NULL) {
		return;
	}
	if (texLevels == NULL) {
		return;
	}
	if (levelCount == 0) {
		return;
	}

	opaque = (format & ~SPRITE_TEXTURE_FORMAT_CK_BORDER) == SPRITE_TEXTURE_FORMAT_RGB555_OPAQUE;

	level = 0;
	levelLimit = levelCount;
	if (levelLimit != 0) {
		do {
			uint32_t pixelCount;
			Sprite* texture;

			if (texLevels->width == 0) {
				break;
			}

			pixelCount = ((uint32_t)sprite->width * sprite->height) >> (2 * texLevels->shift);
			texture = (Sprite*)Memory_AllocTagged(SPRITE_TEXTURE_RESOURCE_TAG,
												  SPRITE_TEXTURE_HEADER_SIZE + 2u * pixelCount);

			if (texture != NULL) {
				uint16_t* out;

				*(SpriteTextureSpriteHeader*)texture = *(SpriteTextureSpriteHeader*)sprite;
				texture->type = (uint16_t)(format & ~SPRITE_TEXTURE_FORMAT_CK_BORDER);
				out = (uint16_t*)((uint8_t*)texture + SPRITE_TEXTURE_HEADER_SIZE);

				if (texLevels->shift == 0) {
					const uint8_t* src = rows;

					SpriteTexture_BuildPalette16FromRGB24(colorTable24, (int)colorCount, palette,
														  SPRITE_TEXTURE_FORMAT_RGB555);
					count = pixelCount;
					if (count != 0) {
						do {
							unsigned int index = *src++;
							unsigned int alphaFlag;
							uint16_t value;

							alphaValue = *src++;
							if (opaque || alphaValue >= g_colorKeyAlphaThreshold) {
								alphaFlag = 0x80u;
							} else {
								alphaFlag = 0;
							}
							value = palette[index];
							value |= (uint16_t)alphaFlag;
							*out++ = value;
							--count;
						} while (count != 0);
					}
				} else {
					uint32_t* rgba = (uint32_t*)Memory_AllocTagged(SPRITE_TEXTURE_SAMPLE_DOWN_TAG,
																   sizeof(uint32_t) * pixelCount);

					if (rgba != NULL) {
						uint32_t* samples = rgba;
						uint32_t pixelDataSize = texture->pixelDataSize;

						texture->width = (uint16_t)(texture->width >> texLevels->shift);
						texture->height = (uint16_t)(texture->height >> texLevels->shift);
						texture->colorKey = (uint16_t)(texture->colorKey >> texLevels->shift);
						texture->field8 = (uint16_t)(texture->field8 >> texLevels->shift);
						texture->pixelDataSize = pixelDataSize >> (2 * texLevels->shift);
						SpriteTexture_DownsampleMip24(rows, sprite->width, sprite->height, colorTable24,
													  texLevels->shift, rgba);

						count = pixelCount;
						while (count != 0) {
							uint32_t argb = *samples++;
							uint8_t alpha = (uint8_t)(argb >> 24);
							unsigned int alphaFlag;
							uint16_t value = (uint16_t)(((argb >> 9) & 0x7c00u) | ((argb >> 6) & 0x03e0u) |
														((argb >> 3) & 0x001fu));

							if (opaque || alpha >= g_colorKeyAlphaThreshold) {
								alphaFlag = 0x80u;
							} else {
								alphaFlag = 0;
							}
							value |= (uint16_t)alphaFlag;
							*out++ = value;
							--count;
						}
						Memory_FreeTagged(SPRITE_TEXTURE_SAMPLE_DOWN_TAG, rgba);
					} else {
						Memory_FreeTagged(SPRITE_TEXTURE_RESOURCE_TAG, texture);
						texture = NULL;
					}
				}

				if ((format & SPRITE_TEXTURE_FORMAT_CK_BORDER) != 0) {
					uint16_t width = texture->width;
					uint16_t* top = (uint16_t*)((uint8_t*)texture + SPRITE_TEXTURE_HEADER_SIZE);
					uint16_t* bottom = top + width * (texture->height - 1);
					uint16_t x = 0;

					if (width != 0) {
						do {
							*top++ &= 0x7fffu;
							*bottom++ &= 0x7fffu;
							++x;
						} while (x < texture->width);
					}

					if ((uint32_t)texture->height - 1 > 1) {
						uint16_t* row =
							(uint16_t*)((uint8_t*)texture + SPRITE_TEXTURE_HEADER_SIZE) + texture->width;
						uint16_t y = 1;

						do {
							row[0] &= 0x7fffu;
							row[texture->width - 1] &= 0x7fffu;
							row += texture->width;
							++y;
						} while (y < (uint16_t)(texture->height - 1));
					}
				}
			}

			texLevels->image = texture;
			++texLevels;
			++level;
		} while (level < levelLimit);
	}
}

// FUNCTION: XWA 0x4CEAC0
void SpriteTexture_EncodeRGB565(Sprite* sprite, const uint8_t* colorTable24, unsigned int colorCount,
								const uint8_t* rows, int format, TexLevel* texLevels, uint16_t levelCount) {
	uint16_t palette[256];
	uint32_t* rgba;
	uint16_t* out;
	uint16_t key;
	unsigned int level;
	uint16_t opaque;

	if (sprite == NULL) {
		return;
	}

	if (colorTable24 == NULL || rows == NULL || texLevels == NULL || levelCount == 0) {
		return;
	}

	opaque = (format & ~SPRITE_TEXTURE_FORMAT_CK_BORDER) == SPRITE_TEXTURE_FORMAT_RGB565;
	SpriteTexture_BuildPalette16FromRGB24(colorTable24, (int)colorCount, palette,
										  SPRITE_TEXTURE_FORMAT_RGB565_CK);
	if (!opaque) {
		uint16_t candidate;

		for (candidate = 0; candidate <= colorCount; ++candidate) {
			uint16_t shifted;
			uint16_t middle;
			uint16_t i;

			key = (uint16_t)((uint32_t)(candidate & 0x20u) | ((uint32_t)(candidate & 4u) << 2));
			key <<= 2;
			key |= candidate & 0x100u;
			shifted = candidate;
			key <<= 1;
			key |= candidate & 2u;
			key <<= 2;
			key |= candidate & 0x10u;
			middle = candidate & 8u;
			shifted >>= 2;
			middle |= shifted & 0x10u;
			key <<= 2;
			middle >>= 2;
			key |= middle;
			key |= candidate & 0x81u;

			for (i = 0; i < colorCount; ++i) {
				if (key == palette[i]) {
					break;
				}
			}
			if (i == colorCount) {
				break;
			}
		}
	} else {
		key = 0;
	}

	level = 0;
	while (level < levelCount) {
		uint32_t pixelCount;
		Sprite* texture;

		if (texLevels->width == 0) {
			break;
		}
		pixelCount = ((uint32_t)sprite->width * sprite->height) >> (2 * texLevels->shift);
		texture = (Sprite*)Memory_AllocTagged(SPRITE_TEXTURE_RESOURCE_TAG,
											  SPRITE_TEXTURE_HEADER_SIZE + 2u * pixelCount);

		if (texture != NULL) {
			const uint8_t* src;

			*(SpriteTextureSpriteHeader*)texture = *(SpriteTextureSpriteHeader*)sprite;
			texture->colorKey = key;
			texture->type = (uint16_t)(format & SPRITE_TEXTURE_FORMAT_MASK);
			src = rows;
			out = (uint16_t*)((uint8_t*)texture + SPRITE_TEXTURE_HEADER_SIZE);

			if (texLevels->shift == 0) {
				uint16_t* dest = out;
				uint32_t count;

				if (pixelCount-- != 0) {
					count = ++pixelCount;
					do {
						const uint8_t* next;
						unsigned int index;
						uint16_t value;
						unsigned int alpha;

						next = src + 1;
						index = *src;
						value = palette[index];
						alpha = *next;
						src = next + 1;
						if (!opaque && alpha < g_colorKeyAlphaThreshold) {
							value = key;
						}
						*dest++ = value;
						--count;
					} while (count != 0);
				}
			} else {
				uint32_t* samples = (uint32_t*)Memory_AllocTagged(SPRITE_TEXTURE_SAMPLE_DOWN_TAG,
																  sizeof(uint32_t) * pixelCount);

				rgba = samples;
				if (samples != NULL) {
					uint32_t count;
					uint32_t pixelDataSize = texture->pixelDataSize;

					texture->width = (uint16_t)(texture->width >> texLevels->shift);
					texture->height = (uint16_t)(texture->height >> texLevels->shift);
					texture->colorKey = (uint16_t)(texture->colorKey >> texLevels->shift);
					texture->field8 = (uint16_t)(texture->field8 >> texLevels->shift);
					texture->pixelDataSize = pixelDataSize >> (2 * texLevels->shift);
					SpriteTexture_DownsampleMip24(rows, sprite->width, sprite->height, colorTable24,
												  texLevels->shift, rgba);

					if (pixelCount-- != 0) {
						count = ++pixelCount;
						do {
							uint32_t argb;
							unsigned int alpha;
							uint16_t value;

							argb = *samples;
							++samples;
							alpha = argb >> 24;
							value = (uint16_t)(((argb >> 8) & 0xf800u) | ((argb >> 5) & 0x07e0u) |
											   (((uint8_t)argb) >> 3));

							if (!opaque && alpha < g_colorKeyAlphaThreshold) {
								value = key;
							} else if (value == key) {
								value ^= 0x20u;
							}
							*out++ = value;
							--count;
						} while (count != 0);
					}
					Memory_FreeTagged(SPRITE_TEXTURE_SAMPLE_DOWN_TAG, rgba);
				} else {
					Memory_FreeTagged(SPRITE_TEXTURE_RESOURCE_TAG, texture);
					texture = NULL;
				}
			}

			if ((format & SPRITE_TEXTURE_FORMAT_CK_BORDER) != 0) {
				uint16_t width = texture->width;
				uint16_t* top = (uint16_t*)((uint8_t*)texture + SPRITE_TEXTURE_HEADER_SIZE);
				uint16_t* bottom = top + width * (texture->height - 1);
				uint16_t x = 0;

				if (width > 0) {
					do {
						*top++ = key;
						*bottom++ = key;
						++x;
					} while (x < texture->width);
				}

				{
					uint16_t* row =
						(uint16_t*)((uint8_t*)texture + SPRITE_TEXTURE_HEADER_SIZE) + texture->width;
					uint16_t y = 1;

					if ((uint32_t)texture->height - 1 > y) {
						do {
							*row = key;
							row += texture->width - 1;
							*row++ = key;
							++y;
						} while (y < (uint32_t)texture->height - 1);
					}
				}
			}
		}

		texLevels->image = texture;
		++texLevels;
		++level;
	}
}

// FUNCTION: XWA 0x4CEEC0
void SpriteTexture_EncodePaletted(Sprite* sprite, const uint8_t* colorTable24, unsigned int colorCount,
								  const uint8_t* rows, int format, TexLevel* texLevels, uint16_t levelCount) {
	Sprite* source;
	uint16_t* out;
	uint8_t* pixels;
	uint32_t pixelCount;
	uint16_t colorKey;
	unsigned int level;
	unsigned int levelLimit;
	char hasColorKey;
	char opaque;
	uint16_t palette[256];

	if (sprite == NULL) {
		return;
	}
	if (colorTable24 == NULL) {
		return;
	}
	if (rows == NULL) {
		return;
	}
	if (texLevels == NULL) {
		return;
	}
	if (levelCount == 0) {
		return;
	}

	source = sprite;
	format &= ~SPRITE_TEXTURE_FORMAT_CK_BORDER;
	opaque = format == SPRITE_TEXTURE_FORMAT_PAL555_OPAQUE || format == SPRITE_TEXTURE_FORMAT_PAL565_OPAQUE;
	hasColorKey = format == SPRITE_TEXTURE_FORMAT_PAL565_CK || format == SPRITE_TEXTURE_FORMAT_PAL565_OPAQUE;
	SpriteTexture_BuildPalette16FromRGB24(colorTable24, colorCount, palette, format);

	if (hasColorKey && !opaque) {
		uint16_t candidate;
		uint16_t key;

		for (candidate = 0; candidate <= colorCount; ++candidate) {
			uint32_t packed;
			uint16_t middle;
			uint16_t i;

			packed = (candidate & 0x20u) | ((candidate & 4u) << 2);
			packed <<= 2;
			packed |= candidate & 0x100u;
			packed <<= 1;
			packed |= candidate & 2u;
			packed <<= 2;
			packed |= candidate & 0x10u;
			middle = (uint16_t)((candidate & 8u) | (((uint8_t)candidate >> 2) & 0x10u));
			packed <<= 2;
			middle >>= 2;
			packed |= middle;
			packed |= candidate & 0x81u;
			key = (uint16_t)packed;

			for (i = 0; i < colorCount; ++i) {
				if (key == palette[i]) {
					break;
				}
			}
			if (i == colorCount) {
				break;
			}
		}
		colorKey = key;
	} else {
		colorKey = 0;
	}

	level = 0;
	levelLimit = levelCount;
	if (levelLimit != 0) {
		do {
			Sprite* texture;

			if (texLevels->width == 0) {
				break;
			}

			pixelCount = ((uint32_t)source->width * source->height) >> (2 * texLevels->shift);
			texture = (Sprite*)Memory_AllocTagged(
				SPRITE_TEXTURE_RESOURCE_TAG, SPRITE_TEXTURE_HEADER_SIZE + sizeof(palette) + 2u * pixelCount);

			if (texture != NULL) {
				*(SpriteTextureSpriteHeader*)texture = *(SpriteTextureSpriteHeader*)source;
				texture->type = (uint16_t)format;
				if (hasColorKey) {
					texture->colorKey = colorKey;
				}
				out = (uint16_t*)((uint8_t*)texture + SPRITE_TEXTURE_HEADER_SIZE);
				memcpy(out, palette, sizeof(palette));
				out += sizeof(palette) / sizeof(*out);
				pixels = (uint8_t*)out;

				if (texLevels->shift == 0) {
					unsigned int rowBytes = 2u * sprite->width;
					int32_t rowOffset = (int32_t)(rowBytes * (sprite->height - 1));

					if (rowOffset >= 0) {
						do {
							memcpy(out, rows + rowOffset, rowBytes);
							out += sprite->width;
							rowOffset -= (int32_t)rowBytes;
						} while (rowOffset >= 0);
					}
				} else {
					const uint8_t* row;
					uint16_t step;

					texture->width = (uint16_t)(texture->width >> texLevels->shift);
					texture->height = (uint16_t)(texture->height >> texLevels->shift);
					texture->colorKey = (uint16_t)(texture->colorKey >> texLevels->shift);
					texture->field8 = (uint16_t)(texture->field8 >> texLevels->shift);
					texture->pixelDataSize >>= 2 * texLevels->shift;

					step = (uint16_t)(1u << texLevels->shift);
					row = rows + 2u * sprite->width * (sprite->height - 1);
#ifdef XWA_MODERN
					{
						int32_t rowOffset = (int32_t)(row - rows);
						int32_t rowStep = (int32_t)(2u * sprite->width * step);

						while (rowOffset >= 0) {
#else
					while (row >= rows) {
#endif
							uint16_t x = 0;
							const uint16_t* sample = (const uint16_t*)row;

							if (x < texture->width) {
								do {
									*out++ = *sample;
									sample += step;
									++x;
								} while (x < texture->width);
							}
#ifdef XWA_MODERN
							rowOffset -= rowStep;
							if (rowOffset >= 0) {
								row = rows + rowOffset;
							}
						}
					}
#else
						row -= 2u * sprite->width * step;
					}
#endif
				}

				{
					uint8_t* alpha = pixels + 1;
					uint32_t count = pixelCount;

					--count;
					if (pixelCount != 0) {
						++count;
						do {
							if (opaque || *alpha >= g_colorKeyAlphaThreshold) {
								*alpha = 0xff;
							} else {
								*alpha = 0;
							}
							alpha += 2;
							--count;
						} while (count != 0);
					}
				}
				source = sprite;
			}

			texLevels->image = texture;
			++texLevels;
			++level;
		} while (level < levelLimit);
	}
}

// FUNCTION: XWA 0x4CF290
void SpriteTexture_EncodeARGB4444(Sprite* sprite, const uint8_t* colorTable24, unsigned int colorCount,
								  const uint8_t* rows, int format, TexLevel* texLevels, uint16_t levelCount) {
	Sprite* source;
	uint16_t* out;
	uint32_t* rgba;
	uint32_t* rgbaBlock;
	uint32_t pixelCount;
	uint16_t palette[256];
	int level;

	source = sprite;
	if (source != NULL) {
		if (colorTable24 != NULL) {
			if (rows != NULL) {
				if (texLevels != NULL) {
					if (levelCount != 0) {
						level = 0;
						if (levelCount > 0) {
							do {
								Sprite* texture;

								if (texLevels->width == 0) {
									break;
								}

								{
									unsigned int width = source->width;
									unsigned int height = source->height;

									pixelCount = (width * height) >> (2 * texLevels->shift);
								}
								texture =
									(Sprite*)Memory_AllocTagged(SPRITE_TEXTURE_RESOURCE_TAG,
																SPRITE_TEXTURE_HEADER_SIZE + 2u * pixelCount);

								if (texture != NULL) {
									const uint8_t* src;

									src = rows;
									*(SpriteTextureSpriteHeader*)texture =
										*(SpriteTextureSpriteHeader*)source;
									texture->type = SPRITE_TEXTURE_FORMAT_ARGB4444;
									out = (uint16_t*)((uint8_t*)texture + SPRITE_TEXTURE_HEADER_SIZE);

									if (texLevels->shift == 0) {
										uint32_t count;

										SpriteTexture_BuildPalette16FromRGB24(colorTable24, (int)colorCount,
																			  palette,
																			  SPRITE_TEXTURE_FORMAT_ARGB4444);
										count = pixelCount;
										--count;
										if (pixelCount != 0) {
											++count;
											do {
												unsigned int index = *src++;
												unsigned int alpha = *src++;
												unsigned int alphaBits = (alpha & 0xf0u) << 8;

												*out++ = (uint16_t)(alphaBits | palette[index]);
												--count;
											} while (count != 0);
										}
									} else {
										rgba = (uint32_t*)Memory_AllocTagged(SPRITE_TEXTURE_SAMPLE_DOWN_TAG,
																			 sizeof(uint32_t) * pixelCount);
										rgbaBlock = rgba;

										if (rgba != NULL) {
											uint32_t count;
											uint32_t pixelDataSize = texture->pixelDataSize;

											texture->width = (uint16_t)(texture->width >> texLevels->shift);
											texture->height = (uint16_t)(texture->height >> texLevels->shift);
											texture->colorKey =
												(uint16_t)(texture->colorKey >> texLevels->shift);
											texture->field8 = (uint16_t)(texture->field8 >> texLevels->shift);
											texture->pixelDataSize = pixelDataSize >> (2 * texLevels->shift);
											SpriteTexture_DownsampleMip24(rows, source->width, source->height,
																		  colorTable24, texLevels->shift,
																		  rgba);

											count = pixelCount;
											--count;
											if (pixelCount != 0) {
												++count;
												do {
													uint32_t argb = *rgba++;
													unsigned int green = (argb >> 8) & 0x00f0u;
													unsigned int blue = (argb >> 4) & 0x000fu;
													unsigned int red = (argb >> 12) & 0x0f00u;
													unsigned int alpha = (argb >> 16) & 0xf000u;

													*out++ = (uint16_t)(blue | green | red | alpha);
													--count;
												} while (count != 0);
											}
											Memory_FreeTagged(SPRITE_TEXTURE_SAMPLE_DOWN_TAG, rgbaBlock);
										} else {
											Memory_FreeTagged(SPRITE_TEXTURE_RESOURCE_TAG, texture);
											texture = NULL;
										}
									}

									if ((format & SPRITE_TEXTURE_FORMAT_CK_BORDER) != 0) {
										uint16_t width = texture->width;
										uint16_t* top =
											(uint16_t*)((uint8_t*)texture + SPRITE_TEXTURE_HEADER_SIZE);
										uint16_t* bottom = top + width * (texture->height - 1);
										uint16_t x = 0;

										if (width > 0) {
											do {
												*top++ &= 0x0fffu;
												*bottom++ &= 0x0fffu;
												++x;
											} while (x < texture->width);
										}

										if ((int)texture->height - 1 > 1) {
											uint16_t* row =
												(uint16_t*)((uint8_t*)texture + SPRITE_TEXTURE_HEADER_SIZE) +
												texture->width;
											int y = 1;

											do {
												row[0] &= 0x0fffu;
												row[texture->width - 1] &= 0x0fffu;
												row += texture->width;
												++y;
											} while ((uint16_t)y < (uint16_t)(texture->height - 1));
										}
									}
									source = sprite;
								}

								texLevels->image = texture;
								++texLevels;
								++level;
							} while ((uint16_t)level < levelCount);
						}
					}
				}
			}
		}
	}
}

// FUNCTION: XWA 0x4CF590
void SpriteTexture_BuildPalette16FromRGB24(const uint8_t* colorTable24, unsigned int colorCount,
										   uint16_t* outPalette16, int format) {
	int redRightShift;
	int greenRightShift;
	int blueRightShift;
	int greenLeftShift;
	int redLeftShift;

	switch (format - SPRITE_TEXTURE_FORMAT_RGB555) {
		case SPRITE_TEXTURE_FORMAT_RGB555 - SPRITE_TEXTURE_FORMAT_RGB555:
		case SPRITE_TEXTURE_FORMAT_RGB555_OPAQUE - SPRITE_TEXTURE_FORMAT_RGB555:
		case SPRITE_TEXTURE_FORMAT_PAL555_CK - SPRITE_TEXTURE_FORMAT_RGB555:
		case SPRITE_TEXTURE_FORMAT_PAL555_OPAQUE - SPRITE_TEXTURE_FORMAT_RGB555:
			redRightShift = 3;
			redLeftShift = 10;
			greenRightShift = redRightShift;
			blueRightShift = redRightShift;
			greenLeftShift = 5;
			break;
		case SPRITE_TEXTURE_FORMAT_RGB565_CK - SPRITE_TEXTURE_FORMAT_RGB555:
		case SPRITE_TEXTURE_FORMAT_RGB565 - SPRITE_TEXTURE_FORMAT_RGB555:
		case SPRITE_TEXTURE_FORMAT_PAL565_CK - SPRITE_TEXTURE_FORMAT_RGB555:
		case SPRITE_TEXTURE_FORMAT_PAL565_OPAQUE - SPRITE_TEXTURE_FORMAT_RGB555:
			redRightShift = 3;
			greenRightShift = 2;
			blueRightShift = redRightShift;
			redLeftShift = 11;
			greenLeftShift = 5;
			break;
		case SPRITE_TEXTURE_FORMAT_ARGB4444 - SPRITE_TEXTURE_FORMAT_RGB555:
			blueRightShift = 4;
			redLeftShift = 8;
			redRightShift = blueRightShift;
			greenRightShift = blueRightShift;
			greenLeftShift = blueRightShift;
			break;
	}

	if (colorCount > 0) {
		const uint8_t* colorCursor = colorTable24;
		uint16_t* paletteCursor = outPalette16;
		unsigned int remaining;

		for (remaining = colorCount; remaining > 0; --remaining) {
			unsigned int red;
			unsigned int green;
			unsigned int blue;

			red = (*colorCursor++ >> redRightShift) << redLeftShift;
			green = (*colorCursor++ >> greenRightShift) << greenLeftShift;
			blue = *colorCursor++ >> blueRightShift;

			*paletteCursor++ = (uint16_t)(red | green | blue);
		}
	}

	if (colorCount < 256) {
		unsigned int padding = 256 - colorCount;
		uint16_t* palette = outPalette16 + colorCount;

		do {
			*palette++ = *outPalette16;
			--padding;
		} while (padding != 0);
	}
}

// FUNCTION: XWA 0x4CF6E0
void SpriteTexture_DownsampleMip24(const uint8_t* rows, uint16_t width, uint16_t height,
								   const uint8_t* colorTable24, uint16_t shift, uint32_t* outRGBA) {
	uint16_t destWidth;
	uint16_t destHeight;
	uint16_t sampleSize;
	uint32_t scratchSize;
	uint32_t* scratch;
	uint32_t* greenPlane;
	uint32_t* alphaPlane;
	uint32_t* redPlane;
	uint32_t* bluePlane;
	uint32_t* alphaCursor;
	uint32_t* redCursor;
	uint32_t* greenCursor;
	uint32_t* blueCursor;
	uint16_t y;

	sampleSize = (uint16_t)(1u << shift);
	destWidth = (uint16_t)(width >> shift);
	destHeight = (uint16_t)(height >> shift);
	scratchSize = (uint32_t)(uint16_t)(destWidth << 2) << 2;
	scratch = (uint32_t*)Memory_AllocTagged(SPRITE_TEXTURE_MIP_SCRATCH_TAG, scratchSize);

	if (scratch == NULL) {
		memset(outRGBA, 0, sizeof(uint32_t) * destWidth * destHeight);
		return;
	}

	greenPlane = scratch;
	alphaPlane = greenPlane + destWidth;
	redPlane = alphaPlane + destWidth;
	bluePlane = redPlane + destWidth;
	alphaCursor = alphaPlane;
	redCursor = redPlane;
	greenCursor = greenPlane;
	blueCursor = bluePlane;

	for (y = 0; y < destHeight; ++y) {
		uint16_t sourceY;

		memset(scratch, 0, scratchSize);

		for (sourceY = 0; sourceY < sampleSize; ++sourceY) {
			uint16_t x;

			for (x = 0; x < destWidth; ++x) {
				uint16_t sourceX;

				for (sourceX = 0; sourceX < sampleSize; ++sourceX) {
					uint32_t colorOffset = *rows++;

					colorOffset += colorOffset * 2;

					*redCursor += colorTable24[colorOffset];
					*greenCursor += colorTable24[colorOffset + 1];
					*blueCursor += colorTable24[colorOffset + 2];
					*alphaCursor += *rows++;
				}

				++alphaCursor;
				++redCursor;
				++greenCursor;
				++blueCursor;
			}

			alphaCursor -= destWidth;
			redCursor -= destWidth;
			greenCursor -= destWidth;
			blueCursor -= destWidth;
		}

		for (sourceY = 0; sourceY < destWidth; ++sourceY) {
			uint16_t shiftBits = (uint16_t)(2 * shift);

			*alphaCursor >>= shiftBits;
			*redCursor >>= shiftBits;
			*greenCursor >>= shiftBits;
			*blueCursor >>= shiftBits;
			*outRGBA++ = (((*alphaCursor << 8) | *redCursor) << 16) | (*greenCursor << 8) | *blueCursor;
			++alphaCursor;
			++redCursor;
			++greenCursor;
			++blueCursor;
		}

		alphaCursor -= destWidth;
		redCursor -= destWidth;
		greenCursor -= destWidth;
		blueCursor -= destWidth;
	}

	Memory_FreeTagged(SPRITE_TEXTURE_MIP_SCRATCH_TAG, scratch);
}

void SpriteTexture_FreeTexLevelImages(TexLevel* texLevels, uint16_t levelCount) {
	uint16_t i;

	if (texLevels == NULL) {
		return;
	}

	for (i = 0; i < levelCount; ++i) {
		free(texLevels[i].image);
		texLevels[i].image = NULL;
	}
}
