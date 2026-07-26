#ifndef XWA_FLIGHT_AI_PAI_H
#define XWA_FLIGHT_AI_PAI_H

#include "xwa/assets/file_io.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef char (*PaiManeuverFunc)(void);
typedef void (*PaiManeuverInitFunc)(void);

#pragma pack(push, 1)
typedef struct PlanRecord {
	char name[80];
	uint8_t isDefined;
	uint32_t dataOffset;
} PlanRecord;
#pragma pack(pop)

typedef union PaiOrderCoord {
	uint32_t raw;
	struct {
		uint16_t flightGroupIdx;
		uint8_t orderSlot;
		uint8_t regionIdx;
	} fields;
} PaiOrderCoord;

#pragma pack(push, 1)
typedef struct PaiContext {
	uint32_t aiObjIdx;
	ObjectRecord* aiSelfObjRecord;
	MobileObject* aiSelfMobj;
	CraftData* aiSelfCraft;
	AiController* aiController;
	int aiLeaderObjIdx;
	CraftData* aiTargetCraft;
	PaiOrderCoord curOrderCoord;
	int aiCurrentPointX;
	int aiCurrentPointY;
	int aiCurrentPointZ;
	uint16_t aiSkillTier;
	uint16_t aiPlanInitialManeuverId;
	uint8_t* planCursor;
	uint8_t aiRequireLiveOrderTarget;
	uint8_t nullPlanId;
	uint8_t aiTargetSearchFlags;
	uint32_t aiTargetSearchRangeLimit;
	int aiTargetSearchOriginX;
	int aiTargetSearchOriginY;
	int aiTargetSearchOriginZ;
	uint8_t aiSelfModelUsesExpandedTargetProbe;
} PaiContext;
#pragma pack(pop)

typedef char plan_record_size[(sizeof(PlanRecord) == 0x55) ? 1 : -1];
typedef char plan_record_data_offset[(offsetof(PlanRecord, dataOffset) == 0x51) ? 1 : -1];
#if UINTPTR_MAX == 0xffffffffu
typedef char pai_context_size[(sizeof(PaiContext) == 0x48) ? 1 : -1];
typedef char pai_context_plan_cursor_offset[(offsetof(PaiContext, planCursor) == 0x30) ? 1 : -1];
typedef char
	pai_context_target_range_offset[(offsetof(PaiContext, aiTargetSearchRangeLimit) == 0x37) ? 1 : -1];
typedef char pai_context_expanded_probe_offset
	[(offsetof(PaiContext, aiSelfModelUsesExpandedTargetProbe) == 0x47) ? 1 : -1];
#endif

enum {
	PAI_BUILTIN_PLAN_COUNT = 119,
	PAI_BUILTIN_PLAN_ID_CACHE_COUNT = 256,
	PAI_ORDER_PLAN_NAME_INDEX_COUNT = 72,
};

extern int g_targetRangeScore;
extern const uint16_t g_aiSkillValueQ16ByLevel[8];
extern const uint16_t g_aiThinkIntervalByGroupAI[8];
extern const uint8_t g_aiThreatBearingClassByOctant[8];
extern PaiManeuverFunc g_aiCourseOrderManeuverMode;
extern PaiManeuverInitFunc g_aiCurrentManeuverInitProc;
extern PaiContext g_paiContext;
extern uint8_t g_aiEscortCandidateFgIdx;

void pai_UpdateAllCraftAI(void);
void pai_ProcessPlan(void);
int pai_ApplyPendingPlanTargetAndManeuver(unsigned int objectIdx);
void pai_UpdateAimPointFromOrderTarget(void);
void pai_ObjectRefDirectionToObjectRef(unsigned int fromRef, unsigned int toRef);
void pai_ObjectRefUpdateApproxRangeScore(unsigned int fromRef, unsigned int toRef);
void pai_CalcAnglesToAimPoint(void);
uint16_t pai_GetEffectiveSkillValue(CraftData* craft);
AiController* pai_GetEffectiveAIController(CraftData* craft);
void pai_setupcraftcontext(int objIdx);
int16_t pai_FindMothershipObject(int16_t mothershipFlightGroupIdx);
int16_t pai_CurrentOrderTargetsMatchObject(uint16_t objIdx);
int pai_OrderSlotMatchingObjectHasOrderClass(int sourceObjIdx, int orderClass, uint16_t targetObjIdx);
uint16_t pai_OrderSlotCanBoardTarget(uint8_t orderSlot, uint8_t regionIdx);
int16_t pai_FindBoardingTargetFromOrder(uint8_t orderSlot, uint8_t regionIdx);
int16_t pai_FindNearestBoardingTarget(uint16_t target1Type, uint16_t target1, int16_t targetOrMode,
									  uint16_t target2Type, uint16_t target2);
int pai_IsObjectTargetable(unsigned int objIdx);
int pai_IsObjectTargetableNearCurrentPoint(int unused, unsigned int objIdx, int expandRange);
bool pai_IsObjectWithinCurrentPointRange(unsigned int objIdx, unsigned int maxRangeScore);
uint8_t pai_IsObjectWithinCurrentOrderRange(uint16_t objIdx);
bool pai_IsBoardingPlanId(uint16_t planId);
bool pai_IsBoardingPlanCompleteForOrderSlot(uint16_t planId, uint8_t orderSlot, uint8_t regionIdx);
bool pai_IsPlanCompleteForOrderSlot(uint16_t planId, uint8_t orderSlot, uint8_t regionIdx);

#ifdef __cplusplus
}
#endif

#endif
