#ifndef XWA_ASSETS_FLIGHT_MODEL_H
#define XWA_ASSETS_FLIGHT_MODEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int Craft_FindCraftTypeForObjectType(unsigned short objectType);
int Craft_GetObjectMaxShield(unsigned short objIdx);

#ifdef __cplusplus
}
#endif

#endif
