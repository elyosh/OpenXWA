#ifndef XWA_RUNTIME_FRONTEND_FAMILY_MISSIONS_H
#define XWA_RUNTIME_FRONTEND_FAMILY_MISSIONS_H

/* The family missions. The original game plays missions 0 to 6 of the tour
 * from the family transport. While one of them is the current mission, the
 * frontend opens the family transport room instead of the concourse, and the
 * combat simulator and the mission setup lead back to it. The family room
 * hides the helmet, the jacket and the rating item, and the simulator's
 * briefing plays the family music.
 *
 * A mod can turn this off with `family_missions: false` in
 * frontend/frontend.yaml, for a campaign that starts on a big ship. The 30
 * checks that XWAU's SkipFirst7Missions setting patches then act as if no
 * mission were loaded. Other checks of missions 0 to 6 don't change. */

#ifdef __cplusplus
extern "C" {
#endif

/* g_frontendMissionLoaded, or 0 when the mod turns the family missions off.
   The checks above read it in place of g_frontendMissionLoaded. */
int XwaFamilyMissions_MissionLoaded(void);

void XwaFamilyMissions_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
