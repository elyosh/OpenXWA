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
 * AeronScene_Render opens its render passes. Returns 0 for a phase that
 * draws nothing.
 *
 * With `world_scene`, the effect is drawn over the normal flight scene (the
 * entry and the exit): the streaks and the tunnel's fades are drawn in front
 * of everything farther than `front_depth` in the scene's depth buffer, so
 * only the cockpit stays in front of them. Without it, the effect is the
 * background of the dedicated hyperspace scene (the tunnel alone), and the
 * tunnel's lighting cube is generated. */
int XwaRemasterHyperspace_Prepare(XwaRemasterHyperspace* hyperspace, AeronCommandBuffer* command_buffer,
								  const float camera_rows[9], int rt_w, int rt_h,
								  const XwaRemasterHyperspaceMoment* moment,
								  const XwaRemasterHyperspaceTunnelView* tunnel_view, int world_scene,
								  float front_depth);

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

/* AeronScene AFTER_OPAQUE hook, in the world scene only: the tunnel's fades
 * and the streaks. */
void XwaRemasterHyperspace_DrawFront(AeronCommandBuffer* command_buffer, AeronRenderPass* render_pass, int rt_w,
									 int rt_h, void* user);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_HYPERSPACE_H */
