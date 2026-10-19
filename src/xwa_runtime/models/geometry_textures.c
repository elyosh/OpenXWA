#include "xwa_runtime/models/geometry_textures.h"

#include "xwa/assets/sprite_resource.h"
#include "xwa/util/byte_order.h"

#include <string.h>

enum {
	GEOMETRY_TEXTURES_SPRITE_HEADER_SIZE = 18,
	/* sprite_resource.c's group index: 8-byte entries, a 16-bit id, then a
	   32-bit offset into the data. */
	GEOMETRY_TEXTURES_INDEX_ENTRY_SIZE = 8,
	GEOMETRY_TEXTURES_INDEX_OFFSET = 2,
	GEOMETRY_TEXTURES_PLACEHOLDER_SIZE = 8,
	GEOMETRY_TEXTURES_PLACEHOLDER_ROW_BYTES =
		2 * GEOMETRY_TEXTURES_PLACEHOLDER_SIZE * GEOMETRY_TEXTURES_PLACEHOLDER_SIZE,
	/* The payload header, one 24-bit colour, then 2 bytes (index, alpha) for
	   each pixel, as in a type-24 DAT image. */
	GEOMETRY_TEXTURES_PLACEHOLDER_PAYLOAD_SIZE =
		(int)sizeof(SpritePayload) + 3 + GEOMETRY_TEXTURES_PLACEHOLDER_ROW_BYTES,
};

/* An 8x8 type-24 image with one black colour, whose pixels all have alpha 0:
 * the source of every placeholder. */
static uint8_t g_geometryTexturesPlaceholderSource[GEOMETRY_TEXTURES_SPRITE_HEADER_SIZE +
												   GEOMETRY_TEXTURES_PLACEHOLDER_PAYLOAD_SIZE];
static int g_geometryTexturesPlaceholderReady;

static Sprite* geometry_textures_placeholder_source(void) {
	Sprite* sprite = (Sprite*)g_geometryTexturesPlaceholderSource;

	if (!g_geometryTexturesPlaceholderReady) {
		SpritePayload* payload =
			(SpritePayload*)(g_geometryTexturesPlaceholderSource + GEOMETRY_TEXTURES_SPRITE_HEADER_SIZE);

		sprite->type = 24;
		sprite->width = GEOMETRY_TEXTURES_PLACEHOLDER_SIZE;
		sprite->height = GEOMETRY_TEXTURES_PLACEHOLDER_SIZE;
		sprite->pixelDataSize = GEOMETRY_TEXTURES_PLACEHOLDER_PAYLOAD_SIZE;
		payload->payloadSize = GEOMETRY_TEXTURES_PLACEHOLDER_PAYLOAD_SIZE;
		payload->colorTable24Offset = (uint32_t)sizeof(SpritePayload);
		payload->rowDataOffset = (uint32_t)sizeof(SpritePayload) + 3;
		payload->colorCount = 1;
		g_geometryTexturesPlaceholderReady = 1;
	}
	return sprite;
}

static void geometry_textures_convert_sprite(const Sprite* sprite, int format, TexLevel* texLevels,
											 char baseShift, uint16_t levelCount) {
	uint16_t level;
	TexLevel* levelCursor = texLevels;

	memset(texLevels, 0, sizeof(*texLevels) * levelCount);
	/* The level sizes of SpriteTexture_ConvertSprite: each level halves the
	   size, and a level under 8 pixels ends the list, except the first, which
	   then keeps the full size. */
	for (level = 0; level < levelCount; ++level, ++levelCursor) {
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
		levelCursor->argbColor = -1;
	}
	for (level = 0; level < levelCount && texLevels[level].width != 0; ++level) {
		TexLevel placeholder;

		SpriteTexture_ConvertSprite(geometry_textures_placeholder_source(), format, &placeholder, 0, 1);
		texLevels[level].image = placeholder.image;
	}
}

void XwaGeometryTextures_ConvertById(int16_t groupId, uint16_t spriteId, int format, TexLevel* texLevels,
									 char baseShift, uint16_t levelCount) {
	const Sprite* sprite = SpriteResource_ResolveSprite(groupId, spriteId);

	if (sprite != NULL) {
		geometry_textures_convert_sprite(sprite, format, texLevels, baseShift, levelCount);
	}
}

void XwaGeometryTextures_ConvertByIndex(int16_t groupId, uint16_t spriteIndex, int format, TexLevel* texLevels,
										char baseShift, uint16_t levelCount) {
	const SpriteGroup* group = NULL;
	int16_t groupIndex;

	/* The group lookup of SpriteTexture_ConvertByIndex. */
	for (int i = 0; i < SPRITE_RESOURCE_MAX_GROUPS && group == NULL; ++i) {
		if (g_spriteGroups[i].groupId == groupId) {
			group = &g_spriteGroups[i];
		}
	}
	if (group == NULL) {
		groupIndex = SpriteResource_LoadGroup(groupId);
		if (groupIndex < 0 || groupIndex >= SPRITE_RESOURCE_MAX_GROUPS) {
			return;
		}
		group = &g_spriteGroups[groupIndex];
	}
	if (spriteIndex < (uint16_t)group->spriteCount) {
		const uint32_t offset = ByteOrder_ReadU32Le(group->indexBase + GEOMETRY_TEXTURES_INDEX_ENTRY_SIZE * spriteIndex +
													GEOMETRY_TEXTURES_INDEX_OFFSET);

		geometry_textures_convert_sprite((const Sprite*)(group->dataBase + offset), format, texLevels, baseShift,
										 levelCount);
	}
}
