#ifndef XWA_REMASTER_HYPERSPACE_H
#define XWA_REMASTER_HYPERSPACE_H

#include "aeron/render.h"
#include "xwa_remaster/flight.h"
#include "xwa_remaster/ship.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaRemasterHyperspace XwaRemasterHyperspace;

typedef struct XwaRemasterHyperspaceTunnelView {
	float right[3];
	float up[3];
	float forward[3];
	float tan_half_fov_x;
	float tan_half_fov_y;
	float proj_offset_x;
	float proj_offset_y;
} XwaRemasterHyperspaceTunnelView;

typedef struct XwaRemasterHyperspaceLighting {
	XwaShipAmbientCube ambient_add;
	XwaShipEnvironmentMap environment;
	XwaRemasterDirLight key;
	uint32_t key_count;
	uint8_t active;
} XwaRemasterHyperspaceLighting;

/* The length of the recovered tunnel phase: it ends after this many ticks. */
#define XWA_REMASTER_HYPERSPACE_TUNNEL_TICKS 1298u

/* The moment of the hyperspace sequence to draw. Real hyperspace takes it
 * from the snapshot; the debug preview makes it up. */
typedef struct XwaRemasterHyperspaceMoment {
	uint8_t phase;      /* XwaHyperspacePhase */
	float phase_ticks;  /* ticks since the phase started; may have a fraction */
	float tunnel_ticks; /* the tunnel phase's length */
	uint32_t seed;      /* picks the jump's stars */
	uint8_t tunnel_only; /* the tunnel without end, and without the jump's brightening */
} XwaRemasterHyperspaceMoment;

XwaRemasterHyperspace* XwaRemasterHyperspace_Create(const XwaFlightHyperspaceTunnelParams* tunnel,
													const XwaFlightHyperspaceJumpParams* jump);
void XwaRemasterHyperspace_Destroy(XwaRemasterHyperspace* hyperspace);
void XwaRemasterHyperspace_SetParams(XwaRemasterHyperspace* hyperspace,
									 const XwaFlightHyperspaceTunnelParams* params);
void XwaRemasterHyperspace_SetJumpParams(XwaRemasterHyperspace* hyperspace,
										 const XwaFlightHyperspaceJumpParams* params);

/* Builds the tunnel and streak state for `moment`. Must run before
 * AeronScene_Render opens its render passes. `rt_w` and `rt_h` are the
 * scene's render size. Returns 0 for a phase that draws nothing.
 *
 * With `world_scene`, the effect is drawn over the normal flight scene (the
 * entry and the exit) by XwaRemasterHyperspace_RenderWarp, after the scene.
 * Without it, the effect is the background of the dedicated hyperspace scene
 * (the tunnel alone), and the tunnel's lighting cube is generated. */
int XwaRemasterHyperspace_Prepare(XwaRemasterHyperspace* hyperspace, AeronCommandBuffer* command_buffer,
								  const float camera_rows[9], int rt_w, int rt_h,
								  const XwaRemasterHyperspaceMoment* moment,
								  const XwaRemasterHyperspaceTunnelView* tunnel_view, int world_scene);

/* What the scene warp reads from the finished flight scene. */
typedef struct XwaRemasterHyperspaceWarpInput {
	AeronTexture* scene; /* the finished scene, at output size */
	AeronTexture* depth; /* the scene's reversed-Z depth, at render size */
	int output_w;
	int output_h;
	int render_w;
	int render_h;
	float near_z;          /* the camera's near plane, in view units */
	float keep_depth;      /* the depth at the keep distance; nearer is larger */
	float units_per_meter; /* view units per metre */
	int view;              /* XwaFlightHyperspaceWarpView */
	/* The cockpit's screens at render size (AeronScene_ScreenLayer), drawn
	 * over everything, or NULL. */
	AeronTexture* screens;
	/* The region's sky as a cube, without its suns, or NULL. The forward
	 * exit reads the shrunk sky from it, since most of it lies outside the
	 * screen. Without it, the forward exit only shows trails. */
	AeronTexture* sky_cube;
	AeronSampler* sky_sampler;
	float sky_view_to_cube[9]; /* row-major, from view space (x right, y down, z forward) */
	float sky_scale;           /* multiplies the cube's colour */
	int sky_max_level;         /* the highest mip level to read */
	/* A cube with premultiplied alpha drawn over the sky, or NULL. */
	AeronTexture* overlay_cube;
	AeronSampler* overlay_sampler;
	float overlay_view_to_cube[9];
	float overlay_scale;
	int overlay_max_level;
} XwaRemasterHyperspaceWarpInput;

/* Draws the entry or the exit prepared with `world_scene` over the finished
 * scene: the streaks and the tunnel's fades into their layer, the sky
 * passes, then the composite (docs/hd_hyperspace_jump.md, Scene warp).
 * Returns the image to present instead of the scene, or NULL after a
 * failure, which is recorded on the command buffer. */
AeronTexture* XwaRemasterHyperspace_RenderWarp(XwaRemasterHyperspace* hyperspace,
											   AeronCommandBuffer* command_buffer,
											   const XwaRemasterHyperspaceWarpInput* input);

/* Whether the exposure meter should keep its value this frame, so it doesn't
 * darken the white-out or the bright start and end of the tunnel. Set by the
 * latest successful Prepare. */
int XwaRemasterHyperspace_HoldsExposure(const XwaRemasterHyperspace* hyperspace);

/* Returns the lighting contribution built by the latest successful Prepare. */
int XwaRemasterHyperspace_GetPreparedLighting(const XwaRemasterHyperspace* hyperspace,
											  XwaRemasterHyperspaceLighting* out);

/* AeronScene BEFORE_OPAQUE hook, in the dedicated scene only: the tunnel. */
void XwaRemasterHyperspace_Draw(AeronCommandBuffer* command_buffer, AeronRenderPass* render_pass, int rt_w,
								int rt_h, void* user);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_HYPERSPACE_H */
