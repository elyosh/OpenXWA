#ifndef XWA_FRONTEND_CUTSCENE_H
#define XWA_FRONTEND_CUTSCENE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CutsceneEntry {
	char movieName[128];
	char thumbnailSprite[32];
	char description[128];
	int playAfterDebriefing;
	int missionNumber;
} CutsceneEntry;

typedef int cutscene_entry_size_check[(sizeof(CutsceneEntry) == 296) ? 1 : -1];

extern int g_cutsceneCount;
extern CutsceneEntry* g_cutsceneTable;

int Cutscene_LoadTable(char* fileName);
int Cutscene_PlayForCurrentMissionPhase(int phase);

#ifdef __cplusplus
}
#endif

#endif
