#ifndef XWA_REMASTER_HYPERSPACE_H
#define XWA_REMASTER_HYPERSPACE_H

#include "aeron/render.h"
#include "xwa_remaster/flight.h"
#include "xwa_runtime/snapshot/snapshot.h"

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

XwaRemasterHyperspace* XwaRemasterHyperspace_Create(const XwaFlightHyperspaceTunnelParams* params);
void XwaRemasterHyperspace_Destroy(XwaRemasterHyperspace* hyperspace);
void XwaRemasterHyperspace_SetParams(XwaRemasterHyperspace* hyperspace,
									 const XwaFlightHyperspaceTunnelParams* params);

/* Builds the analytic background state and uploads state-derived streak geometry.
 * Must run before AeronScene_Render opens its render passes. */
int XwaRemasterHyperspace_Prepare(XwaRemasterHyperspace* hyperspace, AeronCommandBuffer* command_buffer,
								  const XwaSnapshot* snapshot, const float view_proj[16],
								  const float camera_rows[9], int rt_w, int rt_h,
								  int force_tunnel, float tunnel_time_seconds,
								  const XwaRemasterHyperspaceTunnelView* tunnel_view);

/* AeronScene BEFORE_OPAQUE hook. The analytic background is drawn first and
 * additive streak quads follow, leaving the normal attachment untouched. */
void XwaRemasterHyperspace_Draw(AeronCommandBuffer* command_buffer, AeronRenderPass* render_pass, int rt_w,
								int rt_h, void* user);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_HYPERSPACE_H */
