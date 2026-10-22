#ifndef XWA_RUNTIME_MODELS_HANGARS_H
#define XWA_RUNTIME_MODELS_HANGARS_H

/* Modern port: per-mothership hangar settings.
 *
 * flight/hangars.yaml in the mod folder holds the settings layers (written by the model
 * conversion from the mod's FlightModels files): a default layer, one per ship
 * model with variants per IFF, and the crafts' hangar cameras and
 * elevations. A mission adds its own layer (`hangar` in its mission file).
 * For the hangar's command ship, the layers mission > ship IFF > ship >
 * default give each object setting from the first layer that sets it, and
 * each map or camera section whole from the first layer that has it.
 *
 * While settings are active (the file exists and model replacements are on),
 * the hangar code takes them instead of its stock layout; otherwise it runs
 * its stock code unchanged. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	XWA_HANGAR_PROFILE_MAX = 64,
	XWA_HANGAR_CAMERA_KEYS = 10,
};

typedef enum XwaHangarShuttleAnimation {
	XWA_HANGAR_SHUTTLE_RIGHT = 0,
	XWA_HANGAR_SHUTTLE_TOP = 1,
	XWA_HANGAR_SHUTTLE_BOTTOM = 2,
} XwaHangarShuttleAnimation;

typedef struct XwaHangarDroid {
	int load; /* LoadDroids && LoadDroidN */
	int positionZ;
	int isFloorInverted; /* IsDroidsFloorInverted || IsDroidNFloorInverted */
	int update;          /* load && DroidNUpdate */
	uint16_t objectType;
	int markings;
	char objectProfile[XWA_HANGAR_PROFILE_MAX];
} XwaHangarDroid;

typedef struct XwaHangarPlayerCraft {
	uint16_t objectType;
	int offset[3];
} XwaHangarPlayerCraft;

/* The HangarObjects settings, with their defaults. */
typedef struct XwaHangarObjects {
	int loadShuttle;
	uint16_t shuttleObjectType; /* 0: the stock shuttle */
	int shuttleMarkings;
	char shuttleObjectProfile[XWA_HANGAR_PROFILE_MAX];
	int shuttlePosition[3];
	uint16_t shuttleOrientation;
	int isShuttleFloorInverted;
	XwaHangarShuttleAnimation shuttleAnimation;
	int shuttleAnimationStraightLine;
	int shuttleAnimationElevation;
	XwaHangarDroid droids[2];
	int roofCranePosition[3];
	int roofCraneAxis;
	int roofCraneLowOffset;
	int roofCraneHighOffset;
	int isHangarFloorInverted;
	int hangarFloorInvertedHeight;
	uint8_t hangarIff; /* 0xFF: the command ship's markings */
	int drawShadows;
	int playerAnimationElevation;
	int playerAnimationInvertedElevation;
	int playerAnimationStraightLine;
	int playerOffset[3];
	int playerInvertedOffset[3];
	const XwaHangarPlayerCraft* playerCrafts;
	int playerCraftCount;
	int isPlayerFloorInverted;
	const uint16_t* playerFloorInvertedTypes;
	int playerFloorInvertedTypeCount;
} XwaHangarObjects;

/* One HangarMap line; objectType 0 is a line that places nothing (it still
   takes its place among the first four, which name the droid and crane
   targets). */
typedef struct XwaHangarMapEntry {
	uint16_t objectType;
	int markings;
	int position[3];
	uint16_t headingXY;
	uint16_t headingZ;
	char objectProfile[XWA_HANGAR_PROFILE_MAX];
	int8_t isFloorInverted; /* -1: the hangar floor's */
} XwaHangarMapEntry;

typedef struct XwaHangarCamera {
	int present;
	int offsets[XWA_HANGAR_CAMERA_KEYS][3];
} XwaHangarCamera;

/* Loads the mod folder's flight/hangars.yaml once model replacements are set up, and makes
   the models it names loadable. */
void XwaHangars_Init(void);
/* Reads the mission's `hangar` layer (its mission file); NULL clears it. */
void XwaHangars_LoadMission(const char* missionPath);
/* Picks the layers for the hangar's command ship, found from the hangar
   player and its flight group. Returns 1 when the result differs from the
   previous one. */
int XwaHangars_Resolve(void);
/* True while settings apply (see above). */
int XwaHangars_IsActive(void);

const XwaHangarObjects* XwaHangars_Objects(void);
/* The map section (`family`: the family base's). Without one in any layer,
   the default layout: the stock one at the highest detail level. */
const XwaHangarMapEntry* XwaHangars_Map(int family, int* count);
const XwaHangarCamera* XwaHangars_Camera(int family);
/* The command ship's markings (FG markings, 0 without a command ship). */
uint8_t XwaHangars_CommandShipMarkings(void);
/* Returns the object type of the hangar's command ship, or -1 if there is
   none. It works without flight/hangars.yaml. */
int XwaHangars_CommandShipType(void);
/* How high `objectType` stands off the hangar floor (GetCraftElevation): the
   [Size] ClosedSFoilsElevation, or ClosedSFoilsElevationInverted below an
   inverted floor, of the model it loads (`crafts` in hangars.yaml); without
   one, 50 for the B-wing, 138 for the shuttle, else half its height. */
int XwaHangars_CraftElevation(uint16_t objectType, int inverted);
/* Whether the player craft `objectType` hangs from the inverted floor, and
   its offset from the stock place (GetPlayerCraft). */
int XwaHangars_PlayerFloorInverted(uint16_t objectType);
/* The player craft camera offset of `objectType` from the player ([Camera]
   of its model, `crafts` in hangars.yaml) for the hangar or the family base,
   on the normal or inverted floor; 0 without one. */
int XwaHangars_CraftCamera(uint16_t objectType, int family, int inverted, int offset[3]);
void XwaHangars_PlayerOffset(uint16_t objectType, int offset[3]);

/* The model file Hangar_LoadCraftModelByType loads for `objectType` instead
   of its stock one ("<key>.opt", resolved by the model source), or 0: the
   command ship's hangar model for the hangar, a HangarObjects swap. */
int XwaHangars_ModelFileName(uint16_t objectType, char* fileName, unsigned int fileNameSize);
/* The model file of any type's list row, for the types
   Hangar_LoadCraftModelByType has no name for (its type filter is skipped);
   0 without settings. */
int XwaHangars_TypeFileName(uint16_t objectType, char* fileName, unsigned int fileNameSize);
/* Records the handle Hangar_LoadCraftModelByType loaded for `objectType`. */
void XwaHangars_ModelLoaded(uint16_t objectType, uint16_t handle);
/* Frees `objectType`'s loaded model when the settings want another one, so
   the next Hangar_LoadCraftModelByType loads it. */
void XwaHangars_ReleaseStaleModel(uint16_t objectType);
/* HangarObjects swaps apply from a resolution until the player leaves the
   hangar. Back in the hangar without a new resolution, the hangar objects get
   their swapped models again; on leaving, the other objects get theirs
   back. */
void XwaHangars_ReapplyHangarModels(void);
void XwaHangars_LeaveHangar(void);

#ifdef __cplusplus
}
#endif

#endif
