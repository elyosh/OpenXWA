#ifndef XWA_FLIGHT_AI_PAI_PLAN_H
#define XWA_FLIGHT_AI_PAI_PLAN_H

#include "xwa/flight/ai/pai.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t g_planOrderData[0xffff];
extern uint8_t* g_planDataPtrs[256];
extern PlanRecord g_planTable[256];
extern int g_planCount;
extern uint8_t g_builtinPlanIdByNameIndex[PAI_BUILTIN_PLAN_ID_CACHE_COUNT];
extern const uint16_t g_planReportMessageIdByPlanId[PAI_BUILTIN_PLAN_COUNT];
extern const uint8_t g_orderLeaderBuiltinPlanNameIndex[PAI_ORDER_PLAN_NAME_INDEX_COUNT];
extern const uint8_t g_orderFollowerBuiltinPlanNameIndex[PAI_ORDER_PLAN_NAME_INDEX_COUNT];

int pai_ReadPlanTextToken(char* token, XwaFile* stream);
int pai_findplanbyname(const char* planName);
int pai_CompilePlansFromText(const char* baseName);
int pai_loadplans(const char* baseName);
void pai_cacheBuiltinPlanIds(void);

#ifdef __cplusplus
}
#endif

#endif
