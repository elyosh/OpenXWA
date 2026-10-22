#ifndef XWA_RUNTIME_MODELS_CRAFT_SOUNDS_H
#define XWA_RUNTIME_MODELS_CRAFT_SOUNDS_H

/* A mod's craft sounds: the engine, fly-by, wash, take-off, weapon,
 * explosion and hyperspace sounds of each craft, and four interface sounds.
 *
 * A model settings file (model_settings.h) can pick some of the game's own
 * sounds for its craft. All keys are optional:
 *
 *   sound:
 *     interior: awing        # the engine sound in the cockpit: xwing, ywing,
 *                            # awing, tie, gunboat, corellian or falcon
 *     fly_by: awing          # the same names, and slave (slots 129 and 130)
 *     fly_by_mode: 2d        # 2d or 3d: always that variant of the fly-by
 *     engine_wash: starship  # normal or starship
 *     take_off: falcon       # gear or falcon
 *     weapons: empire        # see below
 *     weapon_ranges:         # by weapon type: a random sound of the
 *       283: {first: 2, count: 3}  # weapon_range bank
 *     hyper: rebel           # rebel, imperial or imperial_zoom
 *
 * For each sound, a craft takes its model's value, else the entry of its
 * object type in the sound bank (sound_banks.h), else the game's sound. A
 * bank that the mod has silences an object type past the bank's count, as
 * the entries without a sound do. The take-off and wash sounds then fall
 * back to the gear and normal sounds.
 *
 * `weapons` replaces the bank and the ranges with the game's sounds, chosen
 * by weapon type: `plain` gives each weapon type its own sound, whatever
 * the craft. The other values change the laser sounds: corellian_transport,
 * tie_fighter, tie_fighter_with_ion, rebel, rebel_turbo, empire,
 * empire_turbo, falcon, falcon_turbo, ion and ion_turbo.
 *
 * A weapon's sound uses the shooter's object type. An explosion's sound uses
 * the object type that fired the exploding object; the weapon_explosion bank
 * has 8 sounds by type, picked at random with no repeat until 4 are used.
 * The interface sounds come from the `sounds` bank.
 *
 * The random picks use the module's own numbers, not the game's, so the
 * simulation is the same with sound on or off. */

#include "xwa_runtime/audio/sound_banks.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

/* Drops the previous mission's model sounds. Called by model_settings.c. */
void XwaCraftSounds_ClearModelSettings(void);
/* Reads the `sound` group of `objectType`'s model settings file. `path` is
   the file, for the log. Called by model_settings.c. */
void XwaCraftSounds_ReadModelSettings(unsigned int objectType, const char* path,
									  const struct AeronConfigNode* group);

/* The engine sound in the cockpit of object type `objectType`. Changes
   `*engineSlot` (-1 for none), `*baseFrequency` and `*useHighQualityStep`
   when the mod gives the type a sound; keeps them otherwise. */
void XwaCraftSounds_Interior(unsigned int objectType, int* engineSlot, int* baseFrequency,
							 uint8_t* useHighQualityStep);
/* Each function below returns the slot to play for object type
   `objectType`, or -1 for none. It returns `stockSlot`, the game's choice,
   when the mod has nothing for the type. */
int XwaCraftSounds_FlyBy(unsigned int objectType, int stockSlot);
int XwaCraftSounds_Wash(unsigned int objectType, int stockSlot);
int XwaCraftSounds_TakeOff(unsigned int objectType, int stockSlot);
/* A hangar sound of object `objIdx` from `bank`, which is indexed by object
   type: take-off, engine slowing, canopy and the shuttle sounds. */
int XwaCraftSounds_Hangar(XwaSoundBank bank, unsigned int objIdx, int stockSlot);
/* An explosion sound of object `objIdx`. `stockSlot` comes from
   fsfx_PickRandomSmallExplosionSfx, which keeps drawing its random
   numbers. */
int XwaCraftSounds_Explosion(unsigned int objIdx, int stockSlot);
/* An interface sound from the `sounds` bank. */
int XwaCraftSounds_Interface(XwaSoundBankSound sound, int stockSlot);

/* The sound of projectile `projectileObjIdx` when it's fired. Returns 1 and
   writes the slot (-1 for none) when the mod chooses it, 0 for the game's
   choice. */
int XwaCraftSounds_Weapon(unsigned int projectileObjIdx, int* sfxSlot);

/* The hyperspace sounds of player `playerIdx`'s craft. */
int XwaCraftSounds_HyperStart(unsigned int playerIdx, int stockSlot);
int XwaCraftSounds_HyperZoom(unsigned int playerIdx, int stockSlot);
/* The arrival sound; `interdicted` is 1 for the abort sound. */
int XwaCraftSounds_HyperEnd(unsigned int playerIdx, int interdicted, int stockSlot);

/* The wash sound loops while the wash lasts. A silent slot counts as playing
   once it has started, so the wash message comes once per wash.
   XwaCraftSounds_WashStarted records the slot that started, and
   XwaCraftSounds_StopWash stops it when it's a bank slot. */
int XwaCraftSounds_WashPlaying(int sfxSlot);
void XwaCraftSounds_WashStarted(int sfxSlot);
void XwaCraftSounds_StopWash(void);

/* The map plays its own sound when it targets: XwaCraftSounds_SetMapEnter(1)
   before the map's call of Player_SetTarget, 0 after it. */
void XwaCraftSounds_SetMapEnter(int mapEnter);
/* The target select sound of player `playerIdx`'s Player_SetTarget call.
   During the map's call, plays the map enter sound when the mod has one, and
   returns -1. */
int XwaCraftSounds_TargetSelect(unsigned int playerIdx, int stockSlot);

#ifdef __cplusplus
}
#endif

#endif
