#ifndef XWA_REMASTER_FLIGHT_MAP_H
#define XWA_REMASTER_FLIGHT_MAP_H

#include "aeron/scene/scene3d.h"
#include "xwa_remaster/assets.h"
#include "xwa_remaster/flight.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*XwaRemasterFlightMapSubmitObjectFn)(const XwaFlightMapObject* map_object,
												  const XwaFlightObject* object, uint32_t snapshot_index,
												  void* user);

int XwaRemasterFlightMap_Init(void);
void XwaRemasterFlightMap_Shutdown(void);

/* Builds and uploads all map-owned 2D records, and submits selected nearby
 * objects through the common flight object path. No asset residency changes
 * are permitted here; all asset lookups are against the synchronized mission set. */
int XwaRemasterFlightMap_Prepare(AeronCommandBuffer* cmd, AeronScene3D* scene, const XwaSnapshot* snapshot,
								 XwaRemasterAssets* assets, const XwaRemasterFlightView* view,
								 const float world_to_eye[9],
								 XwaRemasterFlightMapSubmitObjectFn submit_object, void* submit_user);

void XwaRemasterFlightMap_RenderDeferredText(AeronCommandBuffer* cmd, AeronRenderPass* pass,
											 AeronRenderTarget* target);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_FLIGHT_MAP_H */
