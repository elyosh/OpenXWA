#ifndef XWA_RUNTIME_MODELS_GEOMETRY_TEXTURES_H
#define XWA_RUNTIME_MODELS_GEOMETRY_TEXTURES_H

/* Makes the texture levels for the images of a DAT group that an atlas with
 * game geometry defines (XwaFrontendGeometry_BuildSpriteGroup).
 *
 * These images have the atlas's sizes but no pixels, so the original
 * converters (SpriteTexture_ConvertById, SpriteTexture_ConvertByIndex) can't
 * read them. These functions give each texture level the size and shift that
 * the original converters would give. The game logic and the HD renderer then
 * see the atlas's sizes.
 *
 * Each level's image is an 8x8 transparent placeholder in the requested
 * format, made by SpriteTexture_ConvertSprite. The HD renderer draws the
 * atlas, and the placeholder only keeps the classic renderer's draw code
 * working. The placeholder's header says 8x8, not the level's size. So upload
 * it with XwaGeometryTextures_UploadLevel, not with
 * FeDiskIo_UploadTexLevelToStd3D, which uploads at the level's size. */

#include "xwa/assets/sprite_texture.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Takes the same arguments as SpriteTexture_ConvertById. */
void XwaGeometryTextures_ConvertById(int16_t groupId, uint16_t spriteId, int format, TexLevel* texLevels,
									 char baseShift, uint16_t levelCount);
/* Takes the same arguments as SpriteTexture_ConvertByIndex. */
void XwaGeometryTextures_ConvertByIndex(int16_t groupId, uint16_t spriteIndex, int format,
										TexLevel* texLevels, char baseShift, uint16_t levelCount);
/* Uploads a level's placeholder image at its own size, 8x8. It works like
 * FeDiskIo_UploadTexLevelToStd3D, which uploads at the level's size. The
 * level keeps the size from the atlas. */
void XwaGeometryTextures_UploadLevel(TexLevel* level);

#ifdef __cplusplus
}
#endif

#endif
