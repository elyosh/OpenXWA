#ifndef XWA_FRONTEND_TECH_LIBRARY_H
#define XWA_FRONTEND_TECH_LIBRARY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CraftTechStats {
	int craftType;
	int genusId;
	int speedRating;
	int accelerationRating;
	int maneuverRating;
	int laserCount;
	int ionCount;
	int warheadRating;
	int shieldRating;
	int hullRating;
	int sizeRating;
	int combatRating;
} CraftTechStats;

typedef struct TechLibrarySpecText {
	char designation[64];
	char manufacturer[64];
	char inUseBy[64];
	char description[256];
	char crew[64];
} TechLibrarySpecText;

extern TechLibrarySpecText* g_techLibrarySpecTextTable;

int BuildCraftTechStats(CraftTechStats* stats);
char* GetCraftTypeModelLongName(int craftType);
int TechLibrary_LoadSpecTextTable(void);
int TechLibrary_UpdateModelControls(void);
int TechLibrary_DrawCraftSpecPanel(void);
int TechLibrary_Update(int frameCounter);
int TechLibrary_Exit(int frameCounter);

#ifdef __cplusplus
}
#endif

#endif
