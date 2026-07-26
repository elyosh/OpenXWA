#ifndef XWA_FRONTEND_MISSION_DEBRIEF_H
#define XWA_FRONTEND_MISSION_DEBRIEF_H

#ifdef __cplusplus
extern "C" {
#endif

extern int g_missionOutcome;

/* Mission hints / consequences text buffer (built by BuildDebriefText). */
extern char* g_hintsText;
/* First visible line of the hints body, owned by its scrollbar. */
extern int g_debriefMissionHintsScrollOffset;
/* Per-frame reveal counter shared by the debrief text pages. */
extern int g_debriefTextRevealFrame;

/* Active debrief tab: 0=Overview, 1=Player Stats, 2=Results, 3=Hints. */
extern int g_briefingTab;
/* Pending debrief action used by the per-frame exit router. */
extern int g_debriefAction;
/* Medal/award index pending the award ceremony (top bit = family award). */
extern int g_pendingAward;
/* Set when the local player was disconnected from the network game. */
extern int g_debriefDisconnectedFromNetGame;
/* Set to request the skip-mission confirmation prompt. */
extern int g_debriefSkipMissionConfirmPending;
/* Set when the statistics page needs to be rebuilt. */
extern int g_debriefStatsPageNeedsRebuild;
/* Mission overview page row count / scroll offset. */
extern int g_debriefMissionOverviewRowCount;
extern int g_debriefMissionOverviewScrollOffset;
/* Per-mission-type assist totals scratch (only index 3 is cleared here). */
extern int g_debriefAssistTotalByMissionType[4];

/* Debrief leaderboard state, populated by MissionDebrief_Prepare. */
extern int g_debriefTeamUseSummaryRows[10];
extern int g_debriefLocalTeamRankIndex;
extern int g_debriefActiveTeamCount;
extern int g_debriefSortedTeamIds[10];
extern int g_debriefTeamHasPlayer[10];
extern int g_debriefSortedPlayerIds[8];
extern int g_debriefKillsOnRankIds[8];
extern int g_debriefKillsFromRankIds[8];
extern int g_debriefRankByPilot;

int MissionDebrief_Update(int frameCounter);
int MissionDebrief_Exit(int frameCounter);
int MissionDebrief_Prepare(void);
int MissionDebrief_DrawMissionOverviewPage(int frameCounter);
int MissionDebrief_DrawPlayerStatisticsPage(void);
int MissionDebrief_DrawTabBar(void);
int MissionDebrief_MarkNetworkPlayersReady(void);
int MissionDebrief_ConfirmSkipMission(void);
int MissionDebrief_ShowAwardCeremony(int frameCounter);
void MissionDebrief_BuildText(char* outResults, char* outHints, int useWinText);
int MissionDebrief_DrawMissionResultsPage(void);
int MissionDebrief_DrawMissionHintsPage(void);

#ifdef __cplusplus
}
#endif

#endif
