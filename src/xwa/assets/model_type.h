#ifndef XWA_ASSETS_MODEL_TYPE_H
#define XWA_ASSETS_MODEL_TYPE_H

#include "xwa/assets/object_type.h"
#include "xwa/assets/sprite_texture.h"
#include "xwa/flight/object/object.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ModelTypeInfo {
	uint8_t recordFlags;
	uint8_t assetFlags;
	uint8_t familyId;
	uint8_t genusId;
	int maxBoundsExtent;
	TexLevel* curTexLevel;
	TexLevel* texLevels;
	uint16_t flags;
	int16_t modelIndex;
	uint16_t textureGroup;
	uint16_t frameCount;
} ModelTypeInfo;

typedef enum ModelTypeAssetFlags {
	MODEL_TYPE_ASSET_MODEL_LOADED = 0x01,
	MODEL_TYPE_ASSET_TEXTURE_DRAW = 0x02,
	MODEL_TYPE_ASSET_TEXTURE_READY = 0x04,
	MODEL_TYPE_ASSET_TEXTURE_UNLOAD_CLASS_MASK = 0x18,
	MODEL_TYPE_ASSET_DEATH_STAR_ONLY = 0x20,
	MODEL_TYPE_ASSET_SPECIAL_MODE_ONLY = 0x40,
	MODEL_TYPE_ASSET_NOT_DEATH_STAR = 0x80,
} ModelTypeAssetFlags;

typedef enum ModelTypeRecordFlags {
	MODEL_TYPE_RECORD_TEXTURE_BACKED = 0x02,
} ModelTypeRecordFlags;

typedef enum ModelTypeFlags {
	MODEL_TYPE_FLAG_FILM_OVERLAY_SELECTABLE = 0x0001,
	MODEL_TYPE_FLAG_HARDWARE_ONLY = 0x0008,
	MODEL_TYPE_FLAG_ANIMATION_LOOPS = 0x0200,
	MODEL_TYPE_FLAG_SINGLE_MIP_LEVEL = 0x20,
	MODEL_TYPE_FLAG_EXPANDED_TARGET_PROBE = 0x0800,
	MODEL_TYPE_FLAG_YAW_UPDATES_ANGLE_D = 0x1000,
	MODEL_TYPE_FLAG_COLLISION_FACEGROUPS = 0x2000,
} ModelTypeFlags;

extern ModelTypeInfo g_modelTypeTable[OBJ_Count];

ModelIndex GetModelIndexFromType(uint16_t objectType);

#ifdef __cplusplus
}
#endif

#endif
