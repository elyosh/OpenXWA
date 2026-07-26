#ifndef XWA_FLIGHT_AI_PAIFIGHT_H
#define XWA_FLIGHT_AI_PAIFIGHT_H

#include "xwa/flight/ai/pai.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int g_paifightSearchOriginX;
extern int g_paifightSearchOriginY;
extern int g_paifightSearchOriginZ;
extern uint8_t g_paifightGunnerTargetCandidateSet[3328];

char paifight_scanfortargetorder(void);
char paifight_checkescortorder(void);
char paifight_coverleaderorder(void);
char paifight_escorttargetorder(void);
char paifight_followleadatkorder(void);
char paifight_fightershootorder(void);
char paifight_gunnerselfdefenseorder(void);
char paifight_gunneroffenseorder(void);
char paifight_missiledefenseorder(void);
char paifight_scanforplayertargettypeorder(void);
char paifight_scanforplayerinspecttypeorder(void);
uint16_t paifight_SelectTargetComponentMesh(uint16_t targetObjIdx);
bool paifight_OrderSlotHasFutureTargets(uint8_t orderSlot, uint8_t regionIdx);
bool paifight_OrderSlotHasRemainingTargets(uint8_t orderSlot, uint8_t regionIdx);
bool paifight_OrderSlotCanTarget(uint8_t orderSlot, uint8_t regionIdx);
bool paifight_HasFutureFgTargets(uint16_t target1Type, uint16_t target1, int16_t targetRelationOp,
								 uint16_t target2Type, uint16_t target2);
int16_t paifight_CountHiddenInspectTargets(uint16_t target1Type, uint16_t target1, int16_t targetRelationOp,
										   uint16_t target2Type, uint16_t target2);
int16_t paifight_FindNearestUninspectedOrderTarget(uint16_t target1Type, uint16_t target1,
												   int16_t targetOrMode, uint16_t target2Type,
												   uint16_t target2);
int16_t paifight_FindInspectOrderTargetFromOrder(uint8_t orderSlot, uint8_t regionIdx);
int16_t paifight_CountRemainingOrderTargets(uint16_t target1Type, uint16_t target1, int16_t targetRelationOp,
											uint16_t target2Type, uint16_t target2);
int16_t paifight_FindNearestMatchingTargetFromOrigin(MissionTriggerVariableType target1Type, uint16_t target1,
													 int16_t target1OrTarget2,
													 MissionTriggerVariableType target2Type, uint16_t target2,
													 int16_t requireClearSweep);
int16_t paifight_FindNearestAttackerOfMatchingTarget(uint16_t target1Type, uint16_t target1,
													 int16_t targetOrMode, uint16_t target2Type,
													 uint16_t target2);
int16_t paifight_FindAttackerOfOrderTargetFromOrder(uint8_t orderSlot, uint8_t regionIdx);
int16_t paifight_FindNearestAttackOrderTarget(uint16_t target1Type, uint16_t target1, int16_t targetOrMode,
											  uint16_t target2Type, uint16_t target2);
int16_t paifight_FindAttackOrderTargetFromOrder(uint8_t orderSlot, uint8_t regionIdx);
int16_t paifight_searchforclosestingroup(uint16_t target1Type, uint16_t target1, int16_t targetRelationOp,
										 uint16_t target2Type, uint16_t target2);
uint16_t paifight_FindNearestEscortLeaderTarget(uint16_t fgTarget1Type, uint16_t fgTarget1,
												int16_t targetOrMode, uint16_t fgTarget2Type,
												uint16_t fgTarget2);
uint16_t paifight_FindEscortLeaderTargetFromOrder(uint8_t orderSlot, uint8_t regionIdx);
int paifight_TargetHasAttackCapacity(uint16_t targetObjIdx, uint16_t candidateCount);
void paifight_BuildGunnerTargetCandidateSet(uint16_t target1Type, uint16_t target1, int16_t target1OrTarget2,
											uint16_t target2Type, uint16_t target2, uint16_t candidateSetIdx);
int16_t paifight_FindNearestGunnerTargetInCandidateSet(uint16_t target1Type, uint16_t target1,
													   int16_t target1OrTarget2, uint16_t target2Type,
													   uint16_t target2, int candidateSetIdx);
char paifight_CanCountMagPulseAsRocket(uint16_t sourceObjIdx, ObjectTypeId warheadType,
									   int desiredWarheadClass, int ignoreFireInhibit);

#ifdef __cplusplus
}
#endif

#endif
