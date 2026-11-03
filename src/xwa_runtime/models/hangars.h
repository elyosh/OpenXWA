#ifndef XWA_RUNTIME_MODELS_HANGARS_H
#define XWA_RUNTIME_MODELS_HANGARS_H

/* Modern port: hangar settings for each command ship.
 *
 * flight/hangars.yaml in the mod folder holds the settings in layers.
 * Lengths are in metres and angles in degrees, and objects are object type
 * names:
 *
 *   default:                        # the layer that every hangar uses
 *     hangar: {floor: normal, inverted_floor_height_m: 0, markings: command_ship}
 *     shuttle:
 *       enabled: true
 *       type: default               # the stock shuttle, or an object type
 *       markings: 0
 *       profile: Default
 *       position_m: [27.515, 23.413]
 *       height_m: 0                 # above its floor
 *       heading_degrees: 236.953
 *       floor: normal
 *       path: {exit_side: right, straight_m: 0, rise_m: 0}
 *     droids:                       # the two droids, in order
 *       - {enabled: true, type: hangar_droid, markings: 0, profile: Default, height_m: 0,
 *          moves: true, floor: normal}
 *       - {type: hangar_droid2}
 *     roof_crane: {position_m: [-34.18, 19.189, -6.885], axis: x, travel_m: [0, 68.359]}
 *     player:
 *       offset_m: [0, 0, 0]
 *       inverted_offset_m: [0, 0, 0]
 *       rise_m: 0
 *       inverted_rise_m: 0
 *       straight_m: 0
 *       floor: normal
 *       inverted_floor_crafts: [xwing]
 *       craft_offsets_m: {xwing: [0, 0, 0]}
 *     swaps: {hangardroid: hangardroidr2q5}
 *     crane_arm: {type: hangar_crane, position_m: [17.7, -10.156], heading_degrees: 129.089}
 *     parked:
 *       - {type: hangar_monitor, position_m: [-25.854, 32.056], heading_degrees: 212.036}
 *       - {type: escort_shuttle, markings: 4, position_m: [-36.62, -104.98, -23.93],
 *          heading_degrees: 0, pitch_degrees: 0, profile: NoPilot, floor: inverted}
 *     cameras: {key_1: [27.588, -56.641, -7.324]}
 *     family_base: {crane_arm: ..., parked: [...], cameras: {...}}
 *   ships:                          # by the model of the command ship
 *     frigate2:
 *       hangar_model: frigate2hangar
 *       ...                         # the same keys as `default`
 *       by_iff:
 *         1: {...}                  # for a command ship of IFF 1
 *   crafts:                         # by the model of the player craft
 *     xwing:
 *       rest_height_m: {normal: 1.2, inverted: 1.2}
 *       player_camera_m: {hangar: [...], hangar_inverted: [...], family_base: [...],
 *                         family_base_inverted: [...]}
 *
 * A mission can add its own layer, `hangar` in its mission file. There,
 * `use_ship_settings: {frigate2: frigate2carrier}` makes a command ship use
 * the settings and hangar model of another model.
 *
 * For the command ship of the hangar, the layers are searched in this order:
 * mission, ship IFF, ship, default. Each setting comes from the first layer
 * that sets it, and each droid's settings are looked up separately.
 * `crane_arm` and `parked` come together from the first layer that has
 * either of them. The same applies to the family base's `crane_arm` and
 * `parked`. Each camera key comes from the first layer that sets it, else
 * the stock shot is used.
 *
 * `parked` objects with a position of two values stand on the floor. With a
 * third value, they are at that height from the hangar's origin. The crane
 * arm swings in time with the first droid. The first parked hangar_monitor
 * and hangar_generator are the droids' targets.
 *
 * While the settings are active (the file exists and model replacements are
 * on), the hangar code uses them instead of its stock layout. Otherwise it
 * runs its stock code unchanged. */

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
	int load;
	int positionZ; /* above its floor */
	int isFloorInverted;
	int update; /* load and `moves` */
	uint16_t objectType;
	int markings;
	char objectProfile[XWA_HANGAR_PROFILE_MAX];
} XwaHangarDroid;

typedef struct XwaHangarPlayerCraft {
	uint16_t objectType;
	int offset[3];
} XwaHangarPlayerCraft;

/* The settings of the shuttle, the droids, the roof crane, the hangar and
   the player, in game units, with their defaults. */
typedef struct XwaHangarObjects {
	int loadShuttle;
	uint16_t shuttleObjectType; /* 0 for the stock shuttle */
	int shuttleMarkings;
	char shuttleObjectProfile[XWA_HANGAR_PROFILE_MAX];
	int shuttlePosition[3]; /* x, y and the height above its floor */
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
	uint8_t hangarIff; /* 0xFF to use the command ship's markings */
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

/* An object of the layout: the crane arm or a parked object. */
typedef struct XwaHangarMapEntry {
	uint16_t objectType;
	int markings;
	int position[3]; /* a z of 0x7FFFFFFF puts the object on the floor */
	uint16_t headingXY;
	uint16_t headingZ;
	char objectProfile[XWA_HANGAR_PROFILE_MAX];
	int8_t isFloorInverted; /* -1 to use the hangar floor's side */
} XwaHangarMapEntry;

/* The fixed camera shots, by number key. A key that isn't set uses the
   stock shot. */
typedef struct XwaHangarCamera {
	uint8_t set[XWA_HANGAR_CAMERA_KEYS];
	int offsets[XWA_HANGAR_CAMERA_KEYS][3];
} XwaHangarCamera;

/* Loads the mod folder's flight/hangars.yaml once model replacements are set up, and makes
   the models it names loadable. */
void XwaHangars_Init(void);
/* Reads the mission's `hangar` layer (its mission file); NULL clears it. */
void XwaHangars_LoadMission(const char* missionPath);
/* Finds the hangar's command ship from the hangar player and its flight
   group, and picks its layers. Returns 1 when the result differs from the
   previous one. */
int XwaHangars_Resolve(void);
/* Returns true while the settings are active (see above). */
int XwaHangars_IsActive(void);

/* The command ship that the last resolution used, for films. */
typedef struct XwaHangarsFlightState {
	uint8_t resolved;
	uint8_t inHangar;
	uint8_t provingGround;
	uint8_t shipMarkings;
	int32_t shipIff; /* -1 when there is no command ship */
	char shipKey[64];
} XwaHangarsFlightState;
void XwaHangars_SaveFlightState(XwaHangarsFlightState* state);
/* Sets the command ship of the last resolution and builds its tables again,
   without rebuilding the scene. In the hangar, the hangar objects get their
   swapped models back. */
void XwaHangars_RestoreFlightState(const XwaHangarsFlightState* state);

const XwaHangarObjects* XwaHangars_Objects(void);
/* Returns the parked objects of the hangar, or of the family base when
   `family` is set. When no layer has a layout, returns the stock layout. */
const XwaHangarMapEntry* XwaHangars_Map(int family, int* count);
/* Returns the crane arm, or NULL when the layout has none. */
const XwaHangarMapEntry* XwaHangars_CraneArm(int family);
const XwaHangarCamera* XwaHangars_Camera(int family);
/* Returns the markings of the command ship's flight group, or 0 when there
   is no command ship. */
uint8_t XwaHangars_CommandShipMarkings(void);
/* Returns the object type of the hangar's command ship, or -1 if there is
   none. It works without flight/hangars.yaml. */
int XwaHangars_CommandShipType(void);
/* Returns how high `objectType` stands above the hangar floor, or how far it
   hangs below the inverted floor when `inverted` is set. This is the
   `rest_height_m` of the model it loads (`crafts` in hangars.yaml). Without
   one, it is 50 for the B-wing, 138 for the shuttle, and half the craft's
   height for the others. */
int XwaHangars_CraftElevation(uint16_t objectType, int inverted);
/* Returns whether the player craft `objectType` hangs from the inverted
   floor. */
int XwaHangars_PlayerFloorInverted(uint16_t objectType);
/* Gets the camera offset from the player craft `objectType`, from
   `player_camera_m` of its model. `family` picks the family base and
   `inverted` the inverted floor. Returns 0 when the model has no offset. */
int XwaHangars_CraftCamera(uint16_t objectType, int family, int inverted, int offset[3]);
/* Gets the player craft's offset from its stock place. */
void XwaHangars_PlayerOffset(uint16_t objectType, int offset[3]);

/* Gets the model file that Hangar_LoadCraftModelByType loads for
   `objectType` instead of its stock one. The file is "<key>.opt", and the
   model source resolves it. For the hangar, it is the command ship's hangar
   model. For the other hangar objects, it is a swap. Returns 0 when there is
   no such file. */
int XwaHangars_ModelFileName(uint16_t objectType, char* fileName, unsigned int fileNameSize);
/* Gets the model file of the list row of `objectType`. Returns 0 when there
   are no hangar settings. With settings, Hangar_LoadCraftModelByType loads
   this file for every type instead of its fixed names. */

int XwaHangars_TypeFileName(uint16_t objectType, char* fileName, unsigned int fileNameSize);
/* Records the handle Hangar_LoadCraftModelByType loaded for `objectType`. */
void XwaHangars_ModelLoaded(uint16_t objectType, uint16_t handle);
/* Frees `objectType`'s loaded model when the settings want another one, so
   the next Hangar_LoadCraftModelByType loads it. */
void XwaHangars_ReleaseStaleModel(uint16_t objectType);
/* Swaps apply from a resolution until the player leaves the hangar. Back in
   the hangar without a new resolution, the hangar objects get their swapped
   models again; on leaving, the other objects get theirs back. */
void XwaHangars_ReapplyHangarModels(void);
void XwaHangars_LeaveHangar(void);

#ifdef __cplusplus
}
#endif

#endif
