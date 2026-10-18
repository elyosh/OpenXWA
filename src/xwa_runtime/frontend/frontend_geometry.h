#ifndef XWA_RUNTIME_FRONTEND_FRONTEND_GEOMETRY_H
#define XWA_RUNTIME_FRONTEND_FRONTEND_GEOMETRY_H

/* Game geometry from the mod folder's frontend atlases, and the frontend's
 * paint look.
 *
 * An atlas with `game_geometry: true` gives the game the frame count, sizes,
 * offsets and bounds of the image it replaces, so the standard file isn't
 * read:
 *   frontend/frontres/<dir>/atlas/<base>.yaml  a Frontres file
 *   resdata/atlas/group_<g>.yaml               a DAT group
 *
 * Values are in design pixels: pixels of the screen that
 * frontend/frontend.yaml declares with `design: [w, h]` (640x480 without the
 * file). They are converted to the game's 640x480 units here, when the game
 * asks for them. Atlases are read once and kept, and so is a missing one. */

#include "xwa/assets/sprite_resource.h"
#include "xwa/frontend/frontend_image.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Which tints of the game's tinted icon draws apply to an image. */
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

/* Where an image is drawn, when not where the game draws it. */
typedef enum XwaGeometryFit {
	XWA_GEOMETRY_FIT_NONE,          /* where the game draws it, at the game's size */
	XWA_GEOMETRY_FIT_SCREEN_HEIGHT, /* as tall as the screen, keeping its shape, centred left to right */
} XwaGeometryFit;

/* One frame, in design pixels. */
typedef struct XwaGeometryFrame {
	int id;           /* DAT image id; -1 in a Frontres atlas */
	float size[2];    /* the drawn size */
	float offset[2];  /* added to the game's draw position */
	int has_bounds;
	float bounds[4];  /* left, top, right, bottom, as the game stores them */
	uint8_t keep_aspect; /* XwaGeometryKeepAspect */
	uint8_t fit;         /* XwaGeometryFit; it changes only the drawing */
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
	float fps;          /* when above 0, the frames are shown by time, not by the game's frame */
	uint8_t tint;       /* XwaGeometryTint */
	int has_draw_adjust;
	float draw_adjust[4]; /* x, y, width and height added to the drawn rectangle only */
	int has_bounds;
	float bounds[4]; /* the whole image's bounds */
	XwaGeometryRegion* regions;
	int region_count;
} XwaGeometryAtlas;

/* The atlas with game geometry for a Frontres file, named as the game names
 * it ("frontres\\concourse\\tourdoor.cbm"), or NULL. */
const XwaGeometryAtlas* XwaFrontendGeometry_FindFile(const char* fileName);
/* The atlas with game geometry for a DAT group, or NULL. */
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

/* The `paint` section of frontend/frontend.yaml. Without it, the original
 * look. */
typedef struct XwaFrontendPaint {
	float fill_alpha;     /* translucent fills; the original is 0.5 */
	int pixel_lines;      /* lines, outlines and the briefing grid one screen pixel thick */
	int has_grid_colours; /* the briefing grid's colours replace the game's */
	uint32_t grid_major;  /* 0xRRGGBB */
	uint32_t grid_minor;
} XwaFrontendPaint;

const XwaFrontendPaint* XwaFrontendGeometry_Paint(void);

/* The design screen's size in pixels: frontend.yaml `design`, else 640x480. */
void XwaFrontendGeometry_DesignSize(float* width, float* height);

/* Builds the descriptor that the recovered loaders would build from the
 * standard file: the frame count, each frame's size and bounds, and the
 * whole image's bounds. Each frame's pixels are all index 0. Returns NULL
 * when memory runs out. */
ResourceDescriptor* XwaFrontendGeometry_BuildDescriptor(const XwaGeometryAtlas* atlas);
/* Whether the loaded resource `name` takes its geometry from an atlas in the
 * mod folder: a named image built from its atlas, or a DAT image whose group
 * has one. */
int XwaFrontendGeometry_ResourceHasGeometry(const char* name);
/* Builds the DAT group `group_id` from its atlas in `group`, a free slot:
 * each image gets the id, size and offset of its atlas frame, and rows with
 * no runs, so it draws nothing in the classic layer. Returns 0 when memory
 * runs out. */
int XwaFrontendGeometry_BuildSpriteGroup(int16_t group_id, SpriteGroup* group, const XwaGeometryAtlas* atlas);
/* Whether `desc` was built by XwaFrontendGeometry_BuildDescriptor and not
 * forgotten since. */
int XwaFrontendGeometry_IsBuiltDescriptor(const ResourceDescriptor* desc);
/* Called when a built descriptor is freed. */
void XwaFrontendGeometry_ForgetDescriptor(const ResourceDescriptor* desc);

/* Whether a concourse star can show at (x, y), in 640x480 units, according
 * to frontend/concourse/starfield_mask.png: white where the window is.
 * Returns -1 when there is no usable mask. */
int XwaFrontendGeometry_StarfieldOpen(int x, int y);

void XwaFrontendGeometry_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
