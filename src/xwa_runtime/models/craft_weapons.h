#ifndef XWA_RUNTIME_MODELS_CRAFT_WEAPONS_H
#define XWA_RUNTIME_MODELS_CRAFT_WEAPONS_H

/* Weapon colours for each firing craft: the colour of its bolts, of the
 * impact marks they leave, and of the player's charge bars. Also which
 * shots of the craft use the weapon of its side.
 *
 * A model settings file sets these in its `weapons` group. All keys are
 * optional:
 *
 *   weapons:
 *     switch_by_iff: all               # turrets, none or all
 *     colors:                          # for all markings
 *       variant: {default: 3, ion_cannon: 1}  # the bolts' markings, by weapon type
 *       impact: {rebel_laser: 0xff0000}       # RGB; 0 keeps the stock white mark
 *       energy_bars: {laser_high: 0xffff0000, laser_low: 0xff9b1e00,
 *                     ion_high: 0xff00b4ff, ion_low: 0xff006992}  # ARGB
 *     by_markings:                     # changes for one markings value
 *       1: {variant: {default: 2}, impact: {rebel_laser: 0x3333ff}}
 *
 * Weapon types are written as names, such as rebel_laser. When a craft with
 * markings m fires, the bolt's markings come from the first of these that is
 * set:
 *   1. `by_markings[m].variant` for the weapon type;
 *   2. `by_markings[m].variant.default`;
 *   3. `colors.variant` for the weapon type;
 *   4. `colors.variant.default`;
 *   5. 0.
 * A bolt's markings pick the variant of the weapon model that it shows.
 *
 * The impact colour comes from `by_markings[m]`, then `colors`, then the
 * `weapons.impact` section of flight/flight.yaml, which sets colours for
 * every craft:
 *
 *   weapons:
 *     impact: {rebel_laser: 0xff0000, ...}
 *
 * Without a colour, or with 0, the mark stays white as in the stock game.
 * The charge bar colours come from `by_markings[m]`, then `colors`, then the
 * stock colours.
 *
 * The colours only change drawing. Bolt markings are part of the world
 * state, but no simulation code reads them.
 *
 * Each weapon type except custom_weapon belongs to the Rebel side, the
 * Imperial side or neither, and each IFF counts as one of the two sides.
 * `switch_by_iff` decides which shots of the craft use the weapon of its
 * side. For example, an Imperial craft fires Imperial lasers instead of
 * Rebel lasers. The values are:
 *   - turrets: only shots from turret racks. This is what the stock game
 *     does;
 *   - none: no shots;
 *   - all: all shots.
 * Without the key, the `rules.weapon_iff_swap` value of flight/flight.yaml
 * applies. This changes the simulation. */

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
