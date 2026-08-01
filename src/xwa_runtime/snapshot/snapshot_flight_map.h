#ifndef XWA_RUNTIME_SNAPSHOT_FLIGHT_MAP_H
#define XWA_RUNTIME_SNAPSHOT_FLIGHT_MAP_H

#include "xwa_runtime/snapshot/snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Builds side-effect-free map metadata from the general flight snapshot. The
 * recovered map's global scratch state is not touched. */
void XwaSnapshotFlightMap_Begin(XwaSnapshot* snapshot);
void XwaSnapshotFlightMap_CaptureObject(XwaSnapshot* snapshot, uint32_t object_slot,
										uint16_t flight_object_index);
void XwaSnapshotFlightMap_End(XwaSnapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif
