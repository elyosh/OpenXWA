#ifndef XWA_FRONTEND_CONCOURSE_H
#define XWA_FRONTEND_CONCOURSE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int g_concoursePlanetGroupId;
extern int g_concourseMarkoHovered;
extern int g_concourseIdleVoiceFrame;
extern int g_concourseLabelColor;
extern int g_concourseStarX[128];
extern int g_concourseStarY[128];
extern int g_concourseStarColorIdx[128];
extern int g_concourseStarPeriod[128];
extern int16_t g_concourseStarPalette[31];

int Concourse_LoadBackground(void);
int Concourse_FreeBackground(void);
int Concourse_LoadPlanetSprite(int planetId);
int Concourse_FreePlanetSprite(void);
int Concourse_DrawPlanet(void);
int Concourse_InitStarfield(void);
int Concourse_DrawStarfield(int frameCounter);
int Concourse_LoadNextTourMission(void);
int Concourse_PlayIdleVoice(int frameCounter);
int Concourse_LoadMarkoVoiceClip(void);
int Concourse_Update(int frameCounter);
int Concourse_Exit(int frameCounter);

#ifdef __cplusplus
}
#endif

#endif
