#ifndef XWA_RUNTIME_AUDIO_SOUND_BANKS_H
#define XWA_RUNTIME_AUDIO_SOUND_BANKS_H

/* Sound banks are lists of sounds that a mod gives each object type, such as
 * a craft's engine or weapon sounds. They are listed in flight/sounds.yaml:
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
 * `count` is the number of entries in the bank. If an entry below the count
 * has no sound, it is silent. XwaSoundBank below lists the banks and how
 * their entries are numbered. `volume` (0 to 127, default 127) and `distance`
 * (the 3D sound distance, default 8192) work like the columns of the game's
 * .lst files.
 *
 * Each bank entry gets its own sound effect slot. These slots start at 2872,
 * after the game's own slots, so the game's sound functions can play them
 * like any other slot.
 *
 * At flight start, the banks load the sounds of the object types that the
 * mission's flight groups use. Any other entry loads the first time it is
 * needed. The banks load when sound effects are on, whether voices are on or
 * off.
 *
 * craft_sounds.h decides which bank entry each craft plays. */

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
