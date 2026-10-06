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
 * While a cockpit with screens is drawn, the HUD's foreground (the fixed
 * widgets without their panel images, and the CMD view) and its text are
 * captured into two textures for the screens, and the 2D HUD draws its
 * panels directly and the captures minus the erased regions
 * (dynamic_cockpit.md 2.6).
 *
 * Per frame: BeginFrame before XwaRemasterHud_PrepareFrame, Prepare after
 * it, ApplyToScene after AeronScene_Begin, and in the 2D HUD
 * XwaRemasterHudFixed_RenderPanels, RenderComposite(0) and
 * RenderComposite(1) in place of the fixed, CMD and text draws.
 */

#include "aeron/render.h"
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
/* Releases the capture targets (with the flight's sized resources); the
 * next active frame recreates them. */
void XwaRemasterDc_ReleaseTargets(void);
void XwaRemasterDc_GetSettings(XwaRemasterDcSettings* out);
void XwaRemasterDc_SetSettings(const XwaRemasterDcSettings* settings);
void XwaRemasterDc_ToggleHolograms(void);

/* Decides whether this frame shows screens: the settings allow it and
 * `cockpit_mesh` (the cockpit drawn this frame, or NULL) has screens.
 * Configures the fixed HUD's panel split. */
void XwaRemasterDc_BeginFrame(const XwaSnapshot* snapshot, const AeronSceneMesh* cockpit_mesh, int target_w,
							  int target_h);
/* Places the sources and regions on the HUD prepared for a
 * target_w x target_h target and, when active, captures it and prepares
 * the screen inputs and the 2D composite. */
void XwaRemasterDc_Prepare(AeronCommandBuffer* cmd, const XwaSnapshot* snapshot, int target_w, int target_h);
int XwaRemasterDc_Active(void);
void XwaRemasterDc_ApplyToScene(AeronScene3D* scene);
/* The captured foreground (layer 0) or text (layer 1) minus the erased
 * regions. */
void XwaRemasterDc_RenderComposite(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
								   int layer);

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
