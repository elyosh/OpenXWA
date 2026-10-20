#ifndef XWA_ASSETS_OBJECT_TYPE_INFO_H
#define XWA_ASSETS_OBJECT_TYPE_INFO_H

#include "xwa/assets/object_type.h"
#include "xwa/assets/sprite_texture.h"
#include "xwa/flight/object/object.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ObjectTypeInfo {
	uint8_t recordFlags;
	uint8_t assetFlags;
	uint8_t familyId;
	uint8_t genusId;
	int maxBoundsExtent;
	TexLevel* curTexLevel;
	TexLevel* texLevels;
	uint16_t flags;
	int16_t modelDefIndex; /* the craft record, or -1 */
	uint16_t textureGroup;
	uint16_t frameCount;
} ObjectTypeInfo;

typedef enum ObjectTypeAssetFlags {
	OBJECT_TYPE_ASSET_MODEL_LOADED = 0x01,
	OBJECT_TYPE_ASSET_TEXTURE_DRAW = 0x02,
	OBJECT_TYPE_ASSET_TEXTURE_READY = 0x04,
	OBJECT_TYPE_ASSET_TEXTURE_UNLOAD_CLASS_MASK = 0x18,
	OBJECT_TYPE_ASSET_DEATH_STAR_ONLY = 0x20,
	OBJECT_TYPE_ASSET_SPECIAL_MODE_ONLY = 0x40,
	OBJECT_TYPE_ASSET_NOT_DEATH_STAR = 0x80,
} ObjectTypeAssetFlags;

/* The textureGroup of an object type that has a model row
   (OBJECT_TYPE_RECORD_TEXTURE_BACKED): the list of the row. frameCount is then
   the row, counted from 0. */
typedef enum ObjectTypeList {
	OBJECT_TYPE_LIST_SPACECRAFT = 0, /* FlightModels\Spacecraft0.LST */
	OBJECT_TYPE_LIST_EQUIPMENT = 1,  /* FlightModels\Equipment0.LST */
} ObjectTypeList;

typedef enum ObjectTypeRecordFlags {
	OBJECT_TYPE_RECORD_TEXTURE_BACKED = 0x02,
} ObjectTypeRecordFlags;

typedef enum ObjectTypeFlags {
	OBJECT_TYPE_FLAG_FILM_OVERLAY_SELECTABLE = 0x0001,
	OBJECT_TYPE_FLAG_HARDWARE_ONLY = 0x0008,
	OBJECT_TYPE_FLAG_ANIMATION_LOOPS = 0x0200,
	OBJECT_TYPE_FLAG_SINGLE_MIP_LEVEL = 0x20,
	/* The craft fires from float hardpoints spread over its hull, not from its
	   weapon hardpoints: the large starships and the Rebel platform. */
	OBJECT_TYPE_FLAG_FLOAT_HARDPOINT_WEAPONS = 0x0800,
	/* The object spins: angleD turns with time and follows its yaw (gun
	   platforms, mines, satellites and buoys). */
	OBJECT_TYPE_FLAG_SPINS = 0x1000,
	OBJECT_TYPE_FLAG_COLLISION_FACEGROUPS = 0x2000,
} ObjectTypeFlags;

extern ObjectTypeInfo g_objectTypes[OBJ_Count];

ModelDefIndex GetModelDefIndexFromType(uint16_t objectType);

#ifdef __cplusplus
}
#endif

#endif
