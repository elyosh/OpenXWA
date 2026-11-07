#ifndef XWA_RUNTIME_MODELS_CRAFT_WEAPONS_H
#define XWA_RUNTIME_MODELS_CRAFT_WEAPONS_H

/* Weapon colours for each firing craft: the colour of its bolts, of the
 * impact marks they leave, and of the player's charge bars. Also which
 * shots of the craft use the weapon of its side. They come from the
 * `weapons` group of a model settings file, and the impact colours also from
 * the `weapons.impact` section of the mod's flight settings. The mod
 * documentation describes the keys and the order of the lookups.
 *
 * The colours only change drawing. Bolt markings are part of the world
 * state, but no simulation code reads them.
 *
 * Each weapon type except custom_weapon belongs to the Rebel side, the
 * Imperial side or neither, and each IFF counts as one of the two sides.
 * `switch_by_iff` decides which shots of the craft use the weapon of its
 * side. `turrets` is what the stock game does. Without the key, the
 * `rules.weapon_iff_swap` rule applies. This changes the simulation. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;
struct GlowMarkRequest;
struct MobileObject;

enum {
	XWA_CRAFT_WEAPONS_LASER_HIGH,
	XWA_CRAFT_WEAPONS_LASER_LOW,
	XWA_CRAFT_WEAPONS_ION_HIGH,
	XWA_CRAFT_WEAPONS_ION_LOW,
	XWA_CRAFT_WEAPONS_BARS
};

/* Reads the `weapons` section of flight/flight.yaml. Called once at
   startup. */
void XwaCraftWeapons_Init(void);
/* Drops the previous mission's settings. Called by the model settings
   reader. */
void XwaCraftWeapons_ClearModelSettings(void);
/* Reads the `weapons` group of `objectType`'s model settings file, or does
   nothing when `group` is NULL. `path` is the file, for the log. Called by
   the model settings reader. */
void XwaCraftWeapons_ReadModelSettings(unsigned int objectType, const char* path,
									   const struct AeronConfigNode* group);

/* Returns the markings of a bolt of `weaponType` fired by a craft of
   `objectType` with `markings`: 0 without settings, as in the stock game. */
uint8_t XwaCraftWeapons_BoltMarkings(unsigned int objectType, uint8_t markings, unsigned int weaponType);
/* Which shots of a craft with object type `objectType` switch to the weapon
   of its side, as an XwaWeaponIffSwap: 1 for every shot, 0 for none, and -1
   for the stock rule, where only turret racks switch. */
int XwaCraftWeapons_SwitchByIff(unsigned int objectType);
/* Returns the projectile type that a craft of `iff` fires for
   `weaponType`: the other side's type when the weapon belongs to the other
   side, else `weaponType`. This is the stock test of starship fire. */
unsigned int XwaCraftWeapons_IffWeaponType(uint8_t iff, unsigned int weaponType);
/* Writes the RGB impact colour of a bolt of `weaponType` from a craft of
   `objectType` with `markings`. `markings` is -1 when the firing craft is
   gone. Returns 0 when the mark keeps its stock white. */
int XwaCraftWeapons_ImpactColor(unsigned int objectType, int markings, unsigned int weaponType,
								uint32_t* rgb);
/* Gives mark request `request` (NULL does nothing) the impact colour of
   `bolt`, a bolt of `weaponType`, from its firing craft's object type and
   current markings. */
void XwaCraftWeapons_TintImpactMark(const struct GlowMarkRequest* request, const struct MobileObject* bolt,
									unsigned int weaponType);
/* Removes the colour of a request of g_glowMarkRequestPool when the game
   takes it from the pool. GlowMarkRequest keeps the original layout, so the
   colours are kept here, by pool position. */
void XwaCraftWeapons_ClearImpactTint(const struct GlowMarkRequest* request);
/* Returns the RGB impact colour of a request, 0 for the stock white. */
uint32_t XwaCraftWeapons_ImpactTint(const struct GlowMarkRequest* request);
/* Replaces the stock charge bar colours in `colors` (ARGB, in the order of
   the enum) with the ones of a craft of `objectType` with `markings`. */
void XwaCraftWeapons_EnergyBarColors(unsigned int objectType, uint8_t markings,
									 uint32_t colors[XWA_CRAFT_WEAPONS_BARS]);

#ifdef __cplusplus
}
#endif

#endif
