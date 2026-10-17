#ifndef XWA_REMASTER_ASSETS_H
#define XWA_REMASTER_ASSETS_H

/*
 * XWA remaster 2D/flight texture resolver.
 *
 * Resolves a 2D record identity to either an authored KTX2 asset or an
 * uncompressed runtime atlas independently decoded from the original file.
 * Neither path depends on classic renderer buffers or processed resources.
 *
 * Identities are the ones the snapshot records carry:
 *   - named sprites: the SOURCE FILE key ("<dir>/<base>", stamped into
 *     records from the name->file bindings) + animation frame;
 *   - DAT atlas sprites: group id + sprite id.
 *
 * Layout consumed (exactly what tools/xwa_asset_bake emits in the mod
 * folder; hand-authored art substitutes by replacing the KTX2 at
 * the same path, any resolution — draw geometry comes from the
 * record's classic dims, never from asset dims):
 *   frontend/frontres/<dir>/sprites/<base>.ktx2         single frame
 *   frontend/frontres/<dir>/atlas/<base>.{ktx2,yaml}    packed animation
 *   frontend/frontres/<dir>/atlas/<base>/frame_NN.ktx2  oversized frames
 *   frontend/fonts/font<size>.{png,fnt}                 HD font atlas
 *   flight/fonts/font_tier_<n>.{ktx2,fnt}               HD flight font atlas
 *   resdata/atlas/group_<g>.{ktx2,yaml}                 DAT sprite group
 *
 * Substrate reused from aeron_scene: AeronImageCache,
 * AeronSpriteAtlas (layout YAML), AeronFontAtlas. Frontend and flight images
 * have separate owners, each reconciled to its corresponding classic snapshot
 * lifetime. Render paths use resident lookup and never initiate I/O or upload.
 * Classification is probed once and cached, including negative
 * results — assets dropped on disk mid-session need a restart to appear.
 */

#include <stdint.h>

#include "aeron/render.h"
#include "aeron/scene/font_atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaRemasterAssets XwaRemasterAssets;
struct XwaSnapshot;

/* Resolved modern texture + frame UV sub-rect (0..1). w/h are the
 * frame's texel dims inside the texture — informational; draw
 * geometry must come from the record's classic dims, or for
 * type-bound frames from classic_w/classic_h: the frame's original
 * classic pixel dims (per-frame YAML key; falls back to the
 * atlas-level classic ratio for older YAMLs; 0 when neither is
 * available). */
typedef struct XwaAssetRef {
	AeronTexture* texture;
	float u0, v0, u1, v1;
	int w, h;
	int classic_w, classic_h;
} XwaAssetRef;

/* `root` is the mod folder. `prefer_original_2d` changes
 * source order; the alternate source is selected only when the first is absent. */
XwaRemasterAssets* XwaRemasterAssets_Create(const char* root, int prefer_original_2d);
void XwaRemasterAssets_Destroy(XwaRemasterAssets* a);

/* Frontend residency mirrors the classic named-resource and DAT-group sets.
 * Sync performs all I/O/upload before reconstruction; sprite and font entry
 * points are lookup-only. */
int XwaRemasterAssets_FrontendAssetsNeedSync(const XwaRemasterAssets* a, const struct XwaSnapshot* snapshot);
int XwaRemasterAssets_SyncFrontendAssets(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
										 const struct XwaSnapshot* snapshot);
int XwaRemasterAssets_FrontendSprite(XwaRemasterAssets* a, const char* file_key, int frame, XwaAssetRef* out);
int XwaRemasterAssets_FrontendAtlasSprite(XwaRemasterAssets* a, int group, int index, XwaAssetRef* out);

/* Mission-flight texture residency mirrors the classic texture-backed model
 * set. Sync performs all I/O/upload before rendering; the two lookup entry
 * points below never load and return only currently resident pages. */
int XwaRemasterAssets_FlightTexturesNeedSync(const XwaRemasterAssets* a, const struct XwaSnapshot* snapshot);
int XwaRemasterAssets_SyncFlightTextures(XwaRemasterAssets* a, AeronCommandBuffer* cmd,
										 const struct XwaSnapshot* snapshot);
/* Publish synchronization generations only after SDL accepts the upload
 * command buffer. Renderer failures terminate the process and are never
 * committed. */
void XwaRemasterAssets_CommitFrontendAssets(XwaRemasterAssets* a, uint64_t generation);
void XwaRemasterAssets_CommitFlightTextures(XwaRemasterAssets* a, uint64_t generation);
int XwaRemasterAssets_FlightAtlasFrame(XwaRemasterAssets* a, int group, int frame_index0, XwaAssetRef* out);
int XwaRemasterAssets_FlightModelFrame(XwaRemasterAssets* a, int object_type, int classic_frame_1based,
									   XwaAssetRef* out);
/* The DAT image that a flight model type draws for a frame: its group, and
 * the image id. That is the type's own image, or for a type drawn by frame,
 * the id of the frame-th image of the group (its index when the atlas has
 * no ids). Returns 0 when the type has no texture group. */
int XwaRemasterAssets_FlightModelImageId(XwaRemasterAssets* a, int object_type, int classic_frame_1based,
										 int* out_group, int* out_image);

/* Flight HUD font tiers are process-resident. Prepare performs all three
 * loads once; FlightFont is lookup-only afterward. Atlas metrics use the
 * returned pixel scale relative to classic coordinates. */
int XwaRemasterAssets_PrepareFlightFonts(XwaRemasterAssets* a, AeronCommandBuffer* cmd);
const AeronFontAtlas* XwaRemasterAssets_FlightFont(XwaRemasterAssets* a,
		int tier, float* out_atlas_scale);

uint32_t XwaRemasterAssets_Generation(const XwaRemasterAssets* a);

/* Font atlas for a classic point size, or NULL when neither source loaded.
 * Both authored and runtime-original atlases use a 4x pixel scale.
 * Borrowed; owned by the resolver. */
const AeronFontAtlas* XwaRemasterAssets_FrontendFont(XwaRemasterAssets* a, int font_size,
													 float* out_atlas_scale);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_ASSETS_H */
