#ifndef XWA_RUNTIME_MODELS_GEOMETRY_TEXTURES_H
#define XWA_RUNTIME_MODELS_GEOMETRY_TEXTURES_H

/* Texture levels for the images of a DAT group built from an atlas with game
 * geometry (XwaFrontendGeometry_BuildSpriteGroup).
 *
 * Those images have the atlas's sizes but no pixels, so the original
 * converters (SpriteTexture_ConvertById, SpriteTexture_ConvertByIndex) can't
 * read them. These functions give each texture level the size and shift that
 * the original converters would give, so the game logic and the HD renderer
 * see the atlas's sizes. Each level's image is an 8x8 transparent placeholder
 * in the requested format, made by SpriteTexture_ConvertSprite: the HD
 * renderer draws the atlas, and the placeholder only keeps the classic
 * renderer's draw code working. A placeholder's header holds 8x8, not the
 * level's size, so it must be uploaded with XwaGeometryTextures_UploadLevel,
 * not FeDiskIo_UploadTexLevelToStd3D, which uploads the level's size. */

#include "xwa/assets/sprite_texture.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Same arguments as SpriteTexture_ConvertById. */
void XwaGeometryTextures_ConvertById(int16_t groupId, uint16_t spriteId, int format, TexLevel* texLevels,
									 char baseShift, uint16_t levelCount);
/* Same arguments as SpriteTexture_ConvertByIndex. */
void XwaGeometryTextures_ConvertByIndex(int16_t groupId, uint16_t spriteIndex, int format, TexLevel* texLevels,
										char baseShift, uint16_t levelCount);
/* Uploads a level's placeholder image at its own 8x8 size, as
 * FeDiskIo_UploadTexLevelToStd3D uploads a level at the level's size. The
 * level keeps the size from the atlas. */
void XwaGeometryTextures_UploadLevel(TexLevel* level);

#ifdef __cplusplus
}
#endif

#endif
