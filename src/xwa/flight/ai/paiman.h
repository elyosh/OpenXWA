#ifndef XWA_FLIGHT_AI_PAIMAN_H
#define XWA_FLIGHT_AI_PAIMAN_H

#include "xwa/flight/ai/pai.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const int16_t g_formationDivisor[34];
extern const uint16_t g_aiTurnAwayStateDelayBySkill[4];
extern const uint16_t g_orderThrottleToCraftThrottleSpeed[11];
extern const uint16_t g_aiAvoidAttackerDelaySecondsByGroupAI[8];
extern const uint16_t g_aiAvoidAttackerDelayFracQ16ByGroupAI[6];
extern const int16_t g_formPosX[34][6];
extern const int16_t g_formPosY[34][6];
extern const int16_t g_formPosZ[34][6];

void paiman_initmaneuver(void);
int paiman_initturninsidemaneuver(void);
void paiman_initboardmaneuver(void);
int paiman_initsplitsmaneuver(void);
AiController* paiman_initimmelmannmaneuver(void);
void paiman_initscissorsmaneuver(void);
uint16_t paiman_initrendezvousmaneuver(void);
void paiman_initcruisemaneuver(void);
CraftData* paiman_initheadonattackmaneuver(void);
void paiman_initdivemaneuver(void);
void paiman_initzoommaneuver(void);
void paiman_initsplitsdivemaneuver(void);
void paiman_initspeedawaymaneuver(void);
CraftData* paiman_initheadtowardmaneuver(void);
void paiman_initheadtowardfullmaneuver(void);
void paiman_initkamikazemaneuver(void);
int paiman_initoutofhyperspacemaneuver(void);
void paiman_initintohyperspacemaneuver(void);
void paiman_initrunawaymaneuver(void);
AiController* paiman_initorbitmaneuver(void);
int paiman_initawaitboardmaneuver(void);
void paiman_initavoidstarshipmaneuver(void);
void paiman_initavoidattackermaneuver(void);
void paiman_initfollowtargetmaneuver(void);
void paiman_initparkmaneuver(void);
void paiman_initworkonmaneuver(void);
void paiman_initsetupattackmaneuver(void);
void paiman_initattackmaneuver(void);
void paiman_initturnawaymaneuver(void);
void paiman_initoutofhangarmaneuver(void);
void paiman_initbackupmaneuver(void);
void paiman_initreleasemaneuver(void);
void paiman_UpdateTurnInsideHeading(unsigned int fallbackObjIdx);
void paiman_attacktarget(uint16_t yawOffset, uint16_t pitchOffset);
int paiman_IsObjectCurrentPlayerObjectiveTarget(int goalCondType, int playerIdx, uint16_t objectIdx);
char paiman_AdvanceHyperspaceWaypoint(void);
void paiman_calcformation(void);
void paiman_calcplanelead(int targetObjIdx);
CraftData* paiman_setflighttotarget(Q16Angle pitchBias, int driveHeading);
void paiman_TransferObjectToAiTeam(unsigned int objectIdx, CraftData* craft, uint8_t ownerFlag);
char paiman_SlewCraftOrientation(uint16_t objectIdx, Q16Angle targetYaw, Q16Angle targetPitch,
								 Q16Angle targetRoll);
bool paiman_UpdateBoardOrPickupAutopilot(unsigned int playerIdx);
bool paiman_UpdatePlayerDeliveryAutopilot(unsigned int playerIdx);
bool paiman_UpdatePlayerTargetTrackingAutopilot(unsigned int playerIdx);
uint16_t paiman_setpower(int objIdx, uint16_t throttle);
void paiman_setspeed(int objIdx, unsigned int desiredSpeed);
void paiman_setturn(uint16_t turnStep);
void paiman_BeginPlayerFollowOverride(int objectIdx, int playerIdx);
void paiman_RefreshDeathStarPlayerFollow(int objectIdx, int playerIdx);
void paiman_ApplyBoardingPlanCompletionEffects(unsigned int boarderObjIdx, int planId, int orderSlot,
											   unsigned int targetObjIdx);

#ifdef __cplusplus
}
#endif

#endif
