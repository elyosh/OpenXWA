#ifndef XWA_RUNTIME_MODELS_CRAFT_SOUNDS_H
#define XWA_RUNTIME_MODELS_CRAFT_SOUNDS_H

/* Picks the sounds that a mod gives each craft: engine, fly-by, wash,
 * take-off, weapon, explosion and hyperspace sounds, and four interface
 * sounds. They come from the `sound` group of a model settings file and from
 * the sound banks. The mod documentation describes the keys.
 *
 * For each sound, the game first uses the value of the craft's model. Then
 * it uses the bank entry for the craft's object type. When the bank doesn't
 * list the object type, the bank's `unlisted` setting picks the stock sound
 * or no sound.
 *
 * A weapon's sound depends on the object type that fired it. An explosion's
 * sound depends on the object type that fired the exploding object.
 *
 * A craft in the player's region plays its starship ambient sound while the
 * player's craft is inside an ellipsoid around it. If several craft use the
 * same sound, it plays once, at the volume of the nearest craft. The ambient
 * sounds stop in the hangar. The hangar ambience plays the sounds listed for
 * its command ship.
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
/* When object `objIdx`, an AI craft, jumps away (`arriving` 0) or ends its
   arrival (`arriving` 1) in the local player's region, plays its
   enter_hyperspace or exit_hyperspace sound, without a position. The stock
   game plays nothing then, so without the bank nothing plays. */
void XwaCraftSounds_OtherCraftHyperspace(unsigned int objIdx, int arriving);

/* Returns the slot of the S-foil sound for player `playerIdx`'s craft: the
   s_foil bank entry if there is one, else `stockSlot`. */
int XwaCraftSounds_SFoil(unsigned int playerIdx, int stockSlot);

/* Returns the slot of hangar ambience sound `index` (0 to 9). This is a bank
   sound of the command ship if the mod has some, else slot 151 + index. */
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
