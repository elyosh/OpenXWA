#ifndef XWA_FRONTEND_SKIRMISH_H
#define XWA_FRONTEND_SKIRMISH_H

#include "xwa/assets/file_io.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const double g_skirmishTeamSpawnAnglesRad[8];
extern int g_skirmishTeamHasCaptureRole[10];
extern int g_skirmishTeamHasStrikeRole[10];
extern int g_skirmishRegionHasCraft[4];
extern int g_skirmishTeamHasSuperiorityRole[10];
extern int g_skirmishTeamHasDisableRole[10];
extern int g_skirmishTeamHasPrimaryFg[10];
extern int g_skirmishTeamHasReconRole[10];
extern int g_skirmishTeamHasEscortRole[10];

int Skirmish_SetupProvingGroundsSession(void);
int Skirmish_InitMissionDefaults(void);
void Skirmish_ResetFlightGroupDefaults(int flightGroupIndex);
int Skirmish_WriteGeneratedMissionFile(XwaFile* stream);
int Skirmish_BuildFlightGroupDutyOrders(int combatSimSlotIdx, int fgIndex);
int Skirmish_PlaceFlightGroupNearPrevious(int fgIndex);
int Skirmish_ApplyGeneratedMissionGoals(int flightGroupCount);
int Skirmish_GenerateMission(const char* outMissionFile);

#ifdef __cplusplus
}
#endif

#endif
