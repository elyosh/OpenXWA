#ifndef XWA_REMASTER_EFFECTS_H
#define XWA_REMASTER_EFFECTS_H

/* Shared modern transient-effect geometry. Object trails use it now;
 * particles will reuse the same view conversion, texture resolution and
 * depth-tested PMA quad submission without sharing trail-specific state. */

#include "aeron/render.h"
#include "aeron/scene/scene3d.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_remaster/assets.h"
#include "xwa_remaster/flight.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaRemasterEffectView {
	int32_t origin_world[3];
	float origin_eye[3];
	float world_to_eye[9];
	/* scene = scene_origin + eye_to_scene * eye. Main flight maps back
	 * into render-local space; CMD uses identity because its scene is
	 * eye-local. */
	float scene_origin[3];
	float eye_to_scene[9];
	float focal_x_px;
	float focal_y_px;
	float classic_pixel_scale;
	float near_z;
} XwaRemasterEffectView;

void XwaRemasterEffectView_Main(XwaRemasterEffectView* out, const XwaRemasterFlightView* view,
								const float world_to_eye[9]);
void XwaRemasterEffectView_EyeLocal(XwaRemasterEffectView* out, const int32_t origin_world[3],
									const float origin_eye[3], const float world_to_eye[9],
									const AeronSceneCamera* camera, float classic_pixel_scale);

typedef struct XwaRemasterTrails XwaRemasterTrails;
typedef struct XwaRemasterParticles XwaRemasterParticles;

XwaRemasterParticles* XwaRemasterParticles_Create(void);
void XwaRemasterParticles_Destroy(XwaRemasterParticles* particles);
void XwaRemasterParticles_Prepare(XwaRemasterParticles* particles, AeronCommandBuffer* cmd,
								  const XwaSnapshot* snapshot, const XwaSnapshot* previous_snapshot,
								  XwaRemasterAssets* assets, const XwaRemasterEffectView* view,
								  const XwaRemasterEffectView* history_view);
void XwaRemasterParticles_SubmitRegion(const XwaRemasterParticles* particles, AeronScene3D* scene,
									   const XwaSnapshot* snapshot, uint8_t region);
void XwaRemasterParticles_SubmitOwner(const XwaRemasterParticles* particles, AeronScene3D* scene,
									  const XwaSnapshot* snapshot, uint16_t owner_slot,
									  uint16_t owner_signature);

XwaRemasterTrails* XwaRemasterTrails_Create(void);
void XwaRemasterTrails_Destroy(XwaRemasterTrails* trails);

/* Resolve assets and build all ribbon quads before any render pass opens. */
void XwaRemasterTrails_Prepare(XwaRemasterTrails* trails, AeronCommandBuffer* cmd,
							   const XwaSnapshot* snapshot, XwaRemasterAssets* assets,
							   const XwaRemasterEffectView* view);

/* Main flight submits every validated owner in the active region. CMD uses
 * the owner-specific form after its existing visibility/source gates. */
void XwaRemasterTrails_SubmitRegion(const XwaRemasterTrails* trails, AeronScene3D* scene, uint8_t region);
void XwaRemasterTrails_SubmitOwner(const XwaRemasterTrails* trails, AeronScene3D* scene, uint16_t owner_slot,
								   uint16_t owner_signature);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_EFFECTS_H */
