#ifndef XWA_RUNTIME_FRONTEND_FRONTEND_GEOMETRY_H
#define XWA_RUNTIME_FRONTEND_FRONTEND_GEOMETRY_H

/* Image geometry from atlases in the mod folder, and the frontend's paint
 * settings.
 *
 * An atlas with `game_geometry: true` replaces an image's geometry: the game
 * takes the frame count, sizes, offsets and bounds from the atlas and doesn't
 * read the original file:
 *   frontend/frontres/<dir>/atlas/<base>.yaml  a Frontres file
 *   resdata/atlas/group_<g>.yaml               a DAT group, for the frontend
 *                                              and for flight
 *
 * The whole game uses a DAT group's atlas. In flight, the simulation uses its
 * frame counts (an animated object ends after its type's last frame), and the
 * drawing code uses its sizes. The flight font group can't use an atlas,
 * because the game copies pixels out of its glyphs (flight_text.c).
 *
 * A video can also replace a Frontres file. It always sets the image's
 * geometry, with the same values for every frame, and takes priority over an
 * atlas:
 *   frontend/frontres/<dir>/video/<base>.yaml
 *
 *   video: techdoor.webm   # next to this file
 *   size: [388, 333]       # the drawn size
 *   offset: [0, 0]         # optional: added to the game's draw position
 *   classic_frames: 60     # optional: the game's frame count, 1 by default
 *   fps: 24                # optional: frames per second; the file's rate by default
 *
 * The video loops for as long as the game draws the image, whichever frame
 * the game asks for.
 *
 * The `units` key sets the units of an atlas's values:
 *   design  The default. These are pixels of the screen size that
 *           `design: [w, h]` sets in frontend/frontend.yaml (640x480 by
 *           default). The game gets them converted to its 640x480 units.
 *           Frontend atlases use these units, with `size`, `offset` and
 *           `bounds` for each frame.
 *   game    The game's own pixels, with no conversion. Flight atlases use
 *           these units. xwa_asset_bake writes them, with `classic_w` and
 *           `classic_h` for each frame's size, and `origin_x` and `origin_y`
 *           for its anchor.
 *
 * Each atlas is read once and kept. A missing atlas is remembered too, so it
 * is only looked for once. */

#include "xwa/assets/sprite_resource.h"
#include "xwa/frontend/frontend_image.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Which tints apply when the game draws an image as a tinted icon. */
typedef enum XwaGeometryTint {
	XWA_GEOMETRY_TINT_ALL,
	XWA_GEOMETRY_TINT_NONE,
	XWA_GEOMETRY_TINT_HIGHLIGHTS, /* only light blue, yellow and green */
} XwaGeometryTint;

typedef enum XwaGeometryKeepAspect {
	XWA_GEOMETRY_KEEP_ASPECT_NONE,
	XWA_GEOMETRY_KEEP_ASPECT_HEIGHT,
	XWA_GEOMETRY_KEEP_ASPECT_WIDTH,
} XwaGeometryKeepAspect;

/* Where an image is drawn, if not at the game's position. */
typedef enum XwaGeometryFit {
	XWA_GEOMETRY_FIT_NONE,          /* at the game's position and size */
	XWA_GEOMETRY_FIT_SCREEN_HEIGHT, /* as tall as the screen, keeping its shape, centred left to right */
} XwaGeometryFit;

/* The units of an atlas's values. */
typedef enum XwaGeometryUnits {
	XWA_GEOMETRY_UNITS_DESIGN, /* design pixels */
	XWA_GEOMETRY_UNITS_GAME,   /* the game's own pixels */
} XwaGeometryUnits;

/* One frame, in the atlas's units. */
typedef struct XwaGeometryFrame {
	int id;          /* DAT image id; -1 in a Frontres atlas */
	float size[2];   /* the drawn size */
	float offset[2]; /* added to the game's draw position */
	int has_bounds;
	float bounds[4];     /* left, top, right, bottom, as the game stores them */
	uint8_t keep_aspect; /* XwaGeometryKeepAspect */
	uint8_t fit;         /* XwaGeometryFit; changes the drawing only, not the geometry */
} XwaGeometryFrame;

/* A named rectangle of a sheet, such as a map icon. */
typedef struct XwaGeometryRegion {
	int type;
	float rect[4]; /* left, top, right, bottom */
} XwaGeometryRegion;

typedef struct XwaGeometryAtlas {
	XwaGeometryFrame* frames;
	int frame_count;
	int classic_frames; /* the frame count the game uses: `classic_frames`, else frame_count */
	/* Above 0, the frames change at this rate over time instead of following
	   the game's frame. Videos always play over time, at the file's rate when
	   `fps` is not set. */
	float fps;
	uint8_t tint; /* XwaGeometryTint */
	int has_draw_adjust;
	float draw_adjust[4]; /* x, y, width and height added to the drawn rectangle only */
	int has_bounds;
	float bounds[4]; /* the whole image's bounds */
	XwaGeometryRegion* regions;
	int region_count;
	char video[160]; /* a video's path in the mod folder, or empty for an atlas */
	uint8_t units;   /* XwaGeometryUnits */
} XwaGeometryAtlas;

/* The atlas with game geometry for a Frontres file, or NULL. `fileName` is
 * the name the game uses, such as "frontres\\concourse\\tourdoor.cbm". */
const XwaGeometryAtlas* XwaFrontendGeometry_FindFile(const char* fileName);
/* Returns the atlas with game geometry for a DAT group, or NULL. Returns NULL
 * for the flight font group, and logs an error if that group has an atlas. */
const XwaGeometryAtlas* XwaFrontendGeometry_FindGroup(int group);
/* The frame with DAT image id `id`, or NULL. */
const XwaGeometryFrame* XwaFrontendGeometry_FindFrame(const XwaGeometryAtlas* atlas, int id);
/* The region of type `type`, or NULL. */
const XwaGeometryRegion* XwaFrontendGeometry_FindRegion(const XwaGeometryAtlas* atlas, int type);

/* Design pixels to whole 640x480 units, rounded to the nearest. */
int XwaFrontendGeometry_UnitX(float x);
int XwaFrontendGeometry_UnitY(float y);
/* Design pixels to 640x480 units, not rounded. */
float XwaFrontendGeometry_UnitsX(float x);
float XwaFrontendGeometry_UnitsY(float y);

/* The `paint` section of frontend/frontend.yaml. Missing keys keep the
 * original look. */
typedef struct XwaFrontendPaint {
	float fill_alpha;     /* alpha of translucent fills; the original is 0.5 */
	int pixel_lines;      /* draw lines, outlines and the briefing grid one screen pixel thick */
	int has_grid_colours; /* use grid_major and grid_minor for the briefing grid */
	uint32_t grid_major;  /* 0xRRGGBB */
	uint32_t grid_minor;
} XwaFrontendPaint;

const XwaFrontendPaint* XwaFrontendGeometry_Paint(void);

/* The design screen's size in pixels: frontend.yaml `design`, else 640x480. */
void XwaFrontendGeometry_DesignSize(float* width, float* height);

/* Builds the descriptor that the game's loaders would build from the
 * original file: the frame count, each frame's size and bounds, and the
 * bounds of the whole image. Every pixel is index 0. Returns NULL when out of
 * memory. */
ResourceDescriptor* XwaFrontendGeometry_BuildDescriptor(const XwaGeometryAtlas* atlas);
/* Whether the loaded resource `name` gets its geometry from an atlas in the
 * mod folder. This is true for a named image built from an atlas, and for a
 * DAT image whose group has an atlas. */
int XwaFrontendGeometry_ResourceHasGeometry(const char* name);
/* Builds the DAT group `group_id` from its atlas into `group`, a free slot.
 * Each image gets the id, size and offset of its atlas frame, in game units.
 * Its rows have no pixel runs, so it draws nothing in the classic layer.
 * Because the images have no real pixels, flight makes their textures with
 * XwaGeometryTextures_ConvertByIndex and XwaGeometryTextures_ConvertById
 * (geometry_textures.h) instead of the original converters. Returns 0 when
 * out of memory. */
int XwaFrontendGeometry_BuildSpriteGroup(int16_t group_id, SpriteGroup* group, const XwaGeometryAtlas* atlas);
/* Whether `desc` was built by XwaFrontendGeometry_BuildDescriptor and not
 * yet passed to XwaFrontendGeometry_ForgetDescriptor. */
int XwaFrontendGeometry_IsBuiltDescriptor(const ResourceDescriptor* desc);
/* Called when a built descriptor is freed. */
void XwaFrontendGeometry_ForgetDescriptor(const ResourceDescriptor* desc);

/* Whether a concourse star can appear at (x, y), in 640x480 units. The mask
 * frontend/concourse/starfield_mask.png is white where the window is.
 * Returns -1 when there is no usable mask. */
int XwaFrontendGeometry_StarfieldOpen(int x, int y);

void XwaFrontendGeometry_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
