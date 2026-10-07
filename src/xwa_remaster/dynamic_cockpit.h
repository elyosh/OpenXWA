#ifndef XWA_REMASTER_DYNAMIC_COCKPIT_H
#define XWA_REMASTER_DYNAMIC_COCKPIT_H

/*
 * Dynamic Cockpit: the HD HUD shown on cockpit displays (screen materials,
 * aeron gltf_mesh.h) as the X-Wing Alliance Upgrade's ddraw wrapper does
 * (docs: dynamic_cockpit.md). Modern port module; no original counterpart.
 *
 * Every frame, after XwaRemasterHud_PrepareFrame, the module places XWAU's
 * source rectangles and erasable HUD regions on the frame's HUD: a
 * rectangle measured in a HUD.dat image (Dynamic_Cockpit_Internal_Areas.cfg)
 * maps onto the quad that image's draw record covers, and the message-pane
 * sources onto the HUD layout's pane anchors. Sources follow XWAU's table
 * order, the `source` index of a screen rectangle.
 *
 * While a cockpit with screens is drawn, the HUD modules emit their records
 * through AddSprite / AddQuad4 / AddFill, which route each one: onto the
 * 2D HUD except within the erased regions, and into a capture atlas (one
 * per layer: graphics, the fixed widgets and the CMD view; text) where it
 * overlaps a source. The atlas packs the sources, padded, and the screens
 * sample them there (dynamic_cockpit.md 2.6). The panel images are not
 * captured; erased ones are not drawn (XwaRemasterHudFixed_SetErasedPanels).
 *
 * Per frame: BeginFrame before XwaRemasterHud_PrepareFrame, which calls
 * PlaceFrame between the modules' Build and PrepareDrawList; Prepare after
 * it; ApplyToScene after AeronScene_Begin.
 */

#include "aeron/render.h"
#include "aeron/scene/draw_list2d.h"
#include "aeron/scene/mesh.h"
#include "aeron/scene/scene3d.h"
#include "xwa_runtime/snapshot/snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sources and HUD regions of XWAU's tables (dynamic_cockpit.md, 1.4). */
#define XWA_DC_SOURCE_COUNT 57
#define XWA_DC_REGION_COUNT 11

/* dynamic_cockpit.md D19, D21, D28, D31, D34. */
typedef struct XwaRemasterDcSettings {
	int enabled;     /* capture and screens; off draws the HUD as before */
	int classic_hud; /* nothing erased; transparent screens without always_visible hidden */
	int holograms;   /* hologram screens visible (fades) */
	float display_emissive_strength;
	float hologram_emissive_strength;
} XwaRemasterDcSettings;

int XwaRemasterDc_Init(void);
void XwaRemasterDc_Shutdown(void);
/* Releases the capture atlas (with the flight's sized resources); the next
 * frame recreates it. */
void XwaRemasterDc_ReleaseTargets(void);
void XwaRemasterDc_GetSettings(XwaRemasterDcSettings* out);
void XwaRemasterDc_SetSettings(const XwaRemasterDcSettings* settings);
void XwaRemasterDc_ToggleHolograms(void);

/* Capture layers, the screens' graphics and text inputs. */
typedef enum XwaRemasterDcLayer {
	XWA_DC_LAYER_GRAPHICS = 0,
	XWA_DC_LAYER_TEXT = 1,
} XwaRemasterDcLayer;

/* Decides whether this frame shows screens: the settings allow it and
 * `cockpit_mesh` (the cockpit drawn this frame, or NULL) has screens.
 * Tells the fixed HUD which panels are erased. */
void XwaRemasterDc_BeginFrame(const XwaSnapshot* snapshot, const AeronSceneMesh* cockpit_mesh, int target_w,
							  int target_h);
/* Places the sources and regions on the HUD built for a target_w x
 * target_h target and lays out this frame's capture (hud.c, before the
 * HUD's draw lists). */
void XwaRemasterDc_PlaceFrame(int target_w, int target_h);
/* A HUD record of `layer`, in place of AeronDrawList_AddSprite /
 * AddQuad4 / AddFill on `screen`: while active, onto `screen` outside the
 * erased regions and into the capture where it overlaps a source;
 * otherwise added to `screen` unchanged. */
void XwaRemasterDc_AddSprite(AeronDrawList2D* screen, XwaRemasterDcLayer layer,
							 const AeronDrawList2DSprite* sprite);
void XwaRemasterDc_AddQuad4(AeronDrawList2D* screen, XwaRemasterDcLayer layer, const AeronDrawList2DQuad4* quad);
void XwaRemasterDc_AddFill(AeronDrawList2D* screen, XwaRemasterDcLayer layer, float x, float y, float w, float h,
						   const float rgba[4], AeronBlit2DBlend blend, const AeronRectI* scissor);
/* After XwaRemasterHud_PrepareFrame: renders the capture and prepares the
 * screen inputs. */
void XwaRemasterDc_Prepare(AeronCommandBuffer* cmd, const XwaSnapshot* snapshot, int target_w, int target_h);
int XwaRemasterDc_Active(void);
void XwaRemasterDc_ApplyToScene(AeronScene3D* scene);

/* Source or region box in target pixels (x0, y0, x1, y1); 0 when the
 * frame's HUD does not place it. */
int XwaRemasterDc_SourceBox(unsigned source, float box[4]);
int XwaRemasterDc_RegionBox(unsigned region, float box[4]);
const char* XwaRemasterDc_SourceName(unsigned source);
const char* XwaRemasterDc_RegionName(unsigned region);

/* Debug overlay: outlines of the placed sources (green) and regions (red)
 * over the 2D HUD; `highlight` (a source, or -1) draws thicker in yellow. */
int XwaRemasterDc_DebugBoxes(void);
void XwaRemasterDc_SetDebugBoxes(int enabled, int highlight);
void XwaRemasterDc_PrepareDebugOverlay(AeronCommandBuffer* cmd, int target_w, int target_h);
void XwaRemasterDc_RenderDebugOverlay(AeronCommandBuffer* cmd, AeronRenderPass* pass,
									  AeronRenderTarget* target);

#ifdef __cplusplus
}
#endif

#endif
