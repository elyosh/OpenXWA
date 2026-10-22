#ifndef XWA_RUNTIME_AUDIO_SOUND_BANKS_H
#define XWA_RUNTIME_AUDIO_SOUND_BANKS_H

/* A mod's sound banks: sets of sounds picked by object type, such as each
 * craft's engine or weapon sounds. flight/sounds.yaml lists them:
 *
 *   version: 1
 *   banks:
 *     interior:                     # by object type
 *       count: 225
 *       sounds:
 *         1: {file: 'Wave\Sfx_Interior\EngineIntXW.wav', volume: 76, distance: 8192}
 *     weapon:                       # 10 by object type
 *       count: 2230
 *       sounds:
 *         1:
 *           0: {file: ...}
 *     sounds:                       # by name
 *       count: 4
 *       sounds:
 *         map_exit: {file: ...}
 *
 * `count` is the bank's number of entries. An entry within the count that
 * has no sound is silent. The banks and their entries are listed with
 * XwaSoundBank below. `volume` (0 to 127, 127 by default) and `distance` (the
 * 3D sound distance, 8192 by default) work like the columns of the game's
 * .lst files.
 *
 * Each bank entry has a sound effect slot from 2872 on, past the game's
 * slots, so the game's sound functions play it like any other slot. A
 * bank's sounds load at flight start with the game's sound effects, but only
 * those of the object types that the mission's flight groups use. Another
 * entry loads the first time it is asked for. The banks load with the sound
 * effect settings, whatever the voice setting.
 *
 * Nothing plays the banks yet: the functions that pick a craft's sounds come
 * next. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The first sound effect slot of the banks. */
#define XWA_SOUND_BANKS_FIRST_SLOT 2872

struct XwaFlightGroup;

typedef enum XwaSoundBank {
	XWA_SOUND_BANK_CANOPY_OPENING,    /* by object type */
	XWA_SOUND_BANK_ENGINE_SLOWING,    /* by object type */
	XWA_SOUND_BANK_ENGINE_WASH,       /* by object type */
	XWA_SOUND_BANK_ENTER_HYPERSPACE,  /* by object type */
	XWA_SOUND_BANK_EXIT_HYPERSPACE,   /* by object type */
	XWA_SOUND_BANK_FLY_BY,            /* by object type */
	XWA_SOUND_BANK_HANGAR_AMBIENT,    /* 10 by mothership object type */
	XWA_SOUND_BANK_HYPER_ABORT,       /* by object type */
	XWA_SOUND_BANK_HYPER_END,         /* by object type */
	XWA_SOUND_BANK_HYPER_START,       /* by object type */
	XWA_SOUND_BANK_HYPER_ZOOM,        /* by object type */
	XWA_SOUND_BANK_INTERIOR,          /* by object type */
	XWA_SOUND_BANK_S_FOIL,            /* by object type */
	XWA_SOUND_BANK_SHUTTLE_BLAST_OFF, /* by object type */
	XWA_SOUND_BANK_SHUTTLE_SHUT_DOWN, /* by object type */
	XWA_SOUND_BANK_SHUTTLE_TAKE_OFF,  /* by object type */
	XWA_SOUND_BANK_SOUNDS,            /* by name, XwaSoundBankSound */
	XWA_SOUND_BANK_STARSHIP_AMBIENT,  /* by object type */
	XWA_SOUND_BANK_TAKE_OFF,          /* by object type */
	XWA_SOUND_BANK_WEAPON,            /* 10 by shooter object type */
	XWA_SOUND_BANK_WEAPON_EXPLOSION,  /* 8 by object type */
	XWA_SOUND_BANK_WEAPON_RANGE,      /* by position */
	XWA_SOUND_BANK_COUNT
} XwaSoundBank;

/* The entries of XWA_SOUND_BANK_SOUNDS. */
typedef enum XwaSoundBankSound {
	XWA_SOUND_BANK_PLAYER_TARGETED,
	XWA_SOUND_BANK_MAP_ENTER,
	XWA_SOUND_BANK_MAP_EXIT,
	XWA_SOUND_BANK_TURRET_SWITCH,
	XWA_SOUND_BANK_TARGET_SELECT
} XwaSoundBankSound;

/* Reads flight/sounds.yaml. Called once at startup. */
void XwaSoundBanks_Init(void);

/* Loads the bank sounds of the object types that the mission's flight groups
   use. `flightGroups` points to the first flight group, `stride` is the
   distance between two flight groups, and `count` is their number. Called at
   flight start, after the game's sound effects. */
void XwaSoundBanks_LoadFlight(const struct XwaFlightGroup* flightGroups, size_t stride, int count);

/* Returns the number of entries of `bank`, or 0 when the mod has no such
   bank. */
int XwaSoundBanks_Count(XwaSoundBank bank);

/* Returns the sound effect slot of entry `index` of `bank`, or -1 when
   `index` is past the bank's count. Loads the entry's sound the first time.
   A silent entry's slot has no sound: its g_sfxIds value is -1. */
int XwaSoundBanks_Slot(XwaSoundBank bank, unsigned int index);

/* The volume and the distance of bank slot `sfxSlot`. */
uint8_t XwaSoundBanks_Volume(unsigned int sfxSlot);
uint16_t XwaSoundBanks_Distance(unsigned int sfxSlot);

#ifdef __cplusplus
}
#endif

#endif
