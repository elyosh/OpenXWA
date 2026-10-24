#ifndef XWA_RUNTIME_MODELS_CRAFT_SOUNDS_H
#define XWA_RUNTIME_MODELS_CRAFT_SOUNDS_H

/* Picks the sounds that a mod gives each craft: engine, fly-by, wash,
 * take-off, weapon, explosion and hyperspace sounds, and four interface
 * sounds.
 *
 * A model settings file can choose some of the game's own
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
 *     ambient:               # the starship ambient sound's ellipsoid
 *       offset_m: [0, 300, 0]      # its centre: right, up, forward
 *       distance_m: 1000           # its radius, or [right, up, forward]
 *
 * For each sound, the game first uses the value from the craft's model file.
 * If there is none, it uses the entry for the craft's object type in the
 * sound bank. If the mod has no such bank, it plays the
 * stock sound. When the mod has the bank but the object type is past the
 * bank's count, the craft is silent, like an entry without a sound. The
 * take-off and wash sounds are exceptions: they fall back to the landing gear
 * and the normal wash.
 *
 * Setting `weapons` makes the craft use the game's own weapon sounds instead
 * of the bank and the ranges. With `plain`, each weapon type plays its own
 * sound, whatever the craft. The other values change the laser sounds:
 * corellian_transport, tie_fighter, tie_fighter_with_ion, rebel,
 * rebel_turbo, empire, empire_turbo, falcon, falcon_turbo, ion and
 * ion_turbo.
 *
 * A weapon's sound depends on the object type that fired it. An explosion's
 * sound depends on the object type that fired the exploding object. The
 * weapon_explosion bank has 8 sounds for each object type. They are picked at
 * random, and a sound doesn't repeat until 4 sounds have played. The
 * interface sounds come from the `sounds` bank.
 *
 * Starship ambient sounds need the starship_ambient_sounds rule. A craft in
 * the player's region plays the starship_ambient entry of its object type
 * while the player's craft is inside an ellipsoid around it.
 * `ambient.offset_m` moves the centre of the ellipsoid along the craft's
 * axes. `ambient.distance_m` sets its radius;
 * the default is the craft's size, and a null in the list keeps the default
 * for that axis. The sound gets louder towards the centre. If several craft
 * use the same sound, it plays once, at the volume of the nearest craft. The
 * ambient sounds stop in the hangar.
 *
 * The hangar ambience comes from the hangar_ambient bank, which has 10
 * sounds for each object type. The hangar uses the sounds of its command
 * ship's object type.
 *
 * Random picks use this module's own random numbers, not the game's. This
 * way the simulation stays the same with sound on or off. */

#include "xwa_runtime/audio/sound_banks.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

/* Drops the previous mission's model sounds. Called by the model settings
   reader. */
void XwaCraftSounds_ClearModelSettings(void);
/* Reads the `sound` group of `objectType`'s model settings file. `path` is
   the file, for the log. Called by the model settings reader. */
void XwaCraftSounds_ReadModelSettings(unsigned int objectType, const char* path,
									  const struct AeronConfigNode* group);

/* Picks the cockpit engine sound of object type `objectType`. If the mod
   gives the type a sound, sets `*engineSlot` (-1 for none),
   `*baseFrequency` and `*useHighQualityStep`. Otherwise leaves them as they
   are. */
void XwaCraftSounds_Interior(unsigned int objectType, int* engineSlot, int* baseFrequency,
							 uint8_t* useHighQualityStep);
/* Each function below returns the slot to play for object type
   `objectType`, or -1 for no sound. If the mod has nothing for the type, it
   returns `stockSlot`, the slot the game chose. */
int XwaCraftSounds_FlyBy(unsigned int objectType, int stockSlot);
int XwaCraftSounds_Wash(unsigned int objectType, int stockSlot);
int XwaCraftSounds_TakeOff(unsigned int objectType, int stockSlot);
/* Returns the slot of a hangar sound for object `objIdx`: take-off, engine
   slowing, canopy or a shuttle sound. `bank` is the bank to look in. */
int XwaCraftSounds_Hangar(XwaSoundBank bank, unsigned int objIdx, int stockSlot);
/* Returns the slot of an explosion sound for object `objIdx`. `stockSlot`
   comes from fsfx_PickRandomSmallExplosionSfx. The caller still calls it, so
   the game's random numbers are drawn as before. */
int XwaCraftSounds_Explosion(unsigned int objIdx, int stockSlot);
/* Returns the slot of an interface sound from the `sounds` bank. */
int XwaCraftSounds_Interface(XwaSoundBankSound sound, int stockSlot);

/* Picks the sound for firing projectile `projectileObjIdx`. If the mod
   chooses the sound, writes its slot (-1 for none) and returns 1. Otherwise
   returns 0, and the game picks the sound. */
int XwaCraftSounds_Weapon(unsigned int projectileObjIdx, int* sfxSlot);

/* Return the slots of the hyperspace sounds for player `playerIdx`'s craft. */
int XwaCraftSounds_HyperStart(unsigned int playerIdx, int stockSlot);
int XwaCraftSounds_HyperZoom(unsigned int playerIdx, int stockSlot);
/* Returns the slot of the arrival sound. Set `interdicted` to 1 for the
   abort sound. */
int XwaCraftSounds_HyperEnd(unsigned int playerIdx, int interdicted, int stockSlot);

/* Returns the slot of hangar ambience sound `index` (0 to 9). This is the
   command ship's bank entry if the mod has one, else slot 151 + index. */
int XwaCraftSounds_HangarAmbient(unsigned int index);

/* Starts, changes and stops the starship ambient sounds for the local
   player. Called once a frame, after the objects move. */
void XwaCraftSounds_UpdateAmbient(void);

/* The wash sound loops while the wash lasts. A silent slot counts as playing
   once it has started, so the wash message shows once per wash.
   XwaCraftSounds_WashStarted records the slot that started, and
   XwaCraftSounds_StopWash stops it if it's a bank slot. */
int XwaCraftSounds_WashPlaying(int sfxSlot);
void XwaCraftSounds_WashStarted(int sfxSlot);
void XwaCraftSounds_StopWash(void);

/* The map can play its own sound when it opens and selects a target. Call
   XwaCraftSounds_SetMapEnter(1) before the map calls Player_SetTarget, and
   XwaCraftSounds_SetMapEnter(0) after. */
void XwaCraftSounds_SetMapEnter(int mapEnter);
/* Returns the slot of the target select sound for player `playerIdx`'s
   Player_SetTarget call. During the map's call, if the mod has a map enter
   sound, plays that sound instead and returns -1. */
int XwaCraftSounds_TargetSelect(unsigned int playerIdx, int stockSlot);

#ifdef __cplusplus
}
#endif

#endif
