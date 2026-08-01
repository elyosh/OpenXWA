#ifndef XWA_FLIGHT_FLIGHT_MAP_H
#define XWA_FLIGHT_FLIGHT_MAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void FlightMap_UpdateCamera(int playerIdx);
void FlightMap_RenderView(void);
void FlightMap_DrawGrid(void);
void FlightMap_DrawObjectBoxCorners(int x, int y, int width, int height, uint8_t colorIndex);
int FlightMap_PickObjectNearestScreenCenter(int playerIdx);
void FlightMap_DrawObjectPass(int pass);
void FlightMap_DrawObjectIconAtViewPos(int objectIdx, int viewX, int viewY, int viewZ);

/* Original object-type-to-map-icon data used by FlightMap_DrawObjectIconAtViewPos. */
extern const uint16_t g_flightMapIconByObjectType[];

void RenderList_Reset(void);
void RenderList_QueueObject(int objectIdx, int sortDepth, int viewX, int viewY, int viewZ, int cullFlags,
							int projectedRadius);
void RenderList_SortDepthDescending(void);
void RenderList_SortDepthAscending(void);
int RenderList_ProjectObjectBoundsForCulling(int objectIdx, unsigned int boundsRadius, int playerIdx);

#ifdef __cplusplus
}
#endif

#endif
