#ifndef XWA_ASSETS_SPRITE_RESOURCE_H
#define XWA_ASSETS_SPRITE_RESOURCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	SPRITE_RESOURCE_MAX_GROUPS = 200,
	SPRITE_RESOURCE_MAX_CATALOG = 1000,
	SPRITE_RESOURCE_GROUP_NOT_FOUND = -2,
	SPRITE_RESOURCE_NO_FREE_GROUP = -3,
	SPRITE_RESOURCE_REALLOC_FAILED = -14,
	SPRITE_RESOURCE_GROUP_STILL_LOCKED = -16,
};

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define XWA_SPRITE_PACKED_STRUCT
#else
#define XWA_SPRITE_PACKED_STRUCT __attribute__((packed))
#endif

typedef struct XWA_SPRITE_PACKED_STRUCT Sprite {
	uint16_t type;
	uint16_t width;
	uint16_t height;
	uint16_t colorKey;
	uint16_t field8;
	uint16_t groupId;
	uint16_t spriteId;
	uint32_t pixelDataSize;
#ifndef XWA_MODERN
	uint8_t pixels[1];
#endif
} Sprite;

typedef struct XWA_SPRITE_PACKED_STRUCT SpritePayload {
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
} SpritePayload;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif
#undef XWA_SPRITE_PACKED_STRUCT

typedef struct SpriteCatalogEntry {
	uint16_t groupId;
	uint16_t spriteCount;
	uint32_t dataBytes;
} SpriteCatalogEntry;

typedef struct SpriteGroup {
	int16_t groupId;
	int16_t spriteCount;
	int32_t indexSize;
	int32_t dataSize;
	unsigned char* hGlobal;
	int16_t lockState;
	unsigned char* indexBase;
	unsigned char* dataBase;
} SpriteGroup;

extern char g_resdataPath[256];
extern int g_resourceFileCount;
extern int g_unusedFlightSwRotSpriteInitWord;
extern SpriteCatalogEntry g_spriteCatalog[SPRITE_RESOURCE_MAX_CATALOG];
extern SpriteGroup g_spriteGroups[SPRITE_RESOURCE_MAX_GROUPS];

int SpriteResource_LoadCatalog(char* listFile);
void SpriteResource_FreeGroups(void);
int16_t SpriteResource_LoadGroup(int16_t groupId);
int16_t SpriteResource_UnloadGroup(int16_t groupId);
Sprite* SpriteResource_ResolveSprite(int16_t groupId, uint16_t spriteId);
void* SpriteResource_GetRowData(Sprite* sprite);
uint16_t SpriteResource_GetGroupSpriteCount(int16_t groupId);
/* Port-side helper: the group's actual sprite ids (NOT dense; frontend
 * atlas bases like 4002 appear). Loads the group when needed. */
int SpriteResource_GetGroupSpriteIds(int16_t groupId, uint16_t* out_ids, int max_ids);

uint16_t SpriteResource_GetSpriteType(const Sprite* sprite);
uint16_t SpriteResource_GetSpriteWidth(const Sprite* sprite);
uint16_t SpriteResource_GetSpriteHeight(const Sprite* sprite);
uint16_t SpriteResource_GetSpriteColorKey(const Sprite* sprite);
uint16_t SpriteResource_GetSpriteGroupId(const Sprite* sprite);
uint16_t SpriteResource_GetSpriteId(const Sprite* sprite);
uint32_t SpriteResource_GetSpritePixelDataSize(const Sprite* sprite);
const unsigned char* SpriteResource_GetSpritePayload(const Sprite* sprite);
unsigned char* SpriteResource_GetMutableSpritePayload(Sprite* sprite);
const unsigned char* SpriteResource_GetSpritePalette16(const Sprite* sprite);
int16_t SpriteResource_GetSpriteAnchorX(const Sprite* sprite);
int16_t SpriteResource_GetSpriteAnchorY(const Sprite* sprite);

void SpriteResource_SetPixelFormat555(int enabled);

#ifdef __cplusplus
}
#endif

#endif
