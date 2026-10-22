#ifndef XWA_RUNTIME_MODELS_CRAFT_WEAPONS_H
#define XWA_RUNTIME_MODELS_CRAFT_WEAPONS_H

/* Weapon colours by firing craft: the colour of its bolts, of the impact
 * marks they leave, and of the player's charge bars. This is XWAU's
 * [WeaponColor].
 *
 * The `weapons` group of a model settings file (model_settings.h) holds a
 * craft's colours. All keys are optional:
 *
 *   weapons:
 *     colors:                          # any markings
 *       variant: {default: 3, 284: 1}  # the bolts' markings, by weapon type
 *       impact: {280: 0xff0000}        # RGB; 0 keeps the stock white mark
 *       energy_bars: {laser_high: 0xffff0000, laser_low: 0xff9b1e00,
 *                     ion_high: 0xff00b4ff, ion_low: 0xff006992}  # ARGB
 *     by_markings:                     # the changes for one markings value
 *       1: {variant: {default: 2}, impact: {280: 0x3333ff}}
 *
 * The weapon types are 280 to 307. For a craft with markings m, a bolt's
 * markings are `variant`'s value for the weapon type, else its default,
 * else 0; then `by_markings[m]`'s default replaces it for every weapon type,
 * and its value for the weapon type replaces that. A bolt's markings pick
 * the variant of the weapon model that it shows.
 *
 * An impact colour comes from `by_markings[m]`, else `colors`, else the
 * `weapons.impact` section of flight/flight.yaml, which gives every craft's
 * colours:
 *
 *   weapons:
 *     impact: {280: 0xff0000, ...}
 *
 * Without a colour, or with 0, the mark keeps its stock white. The charge
 * bars take `by_markings[m]`'s colour, else `colors`', else the stock one.
 *
 * The colours only change drawing. A bolt's markings are part of the world
 * state, but no simulation code reads them. */

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
/* Drops the previous mission's settings. Called by model_settings.c. */
void XwaCraftWeapons_ClearModelSettings(void);
/* Reads the `weapons` group of `objectType`'s model settings file, or does
   nothing when `group` is NULL. `path` is the file, for the log. Called by
   model_settings.c. */
void XwaCraftWeapons_ReadModelSettings(unsigned int objectType, const char* path,
									   const struct AeronConfigNode* group);

/* Returns the markings of a bolt of `weaponType` fired by a craft of
   `objectType` with `markings`: 0 without settings, as in the stock game. */
uint8_t XwaCraftWeapons_BoltMarkings(unsigned int objectType, uint8_t markings, unsigned int weaponType);
/* Writes the RGB impact colour of a bolt of `weaponType` from a craft of
   `objectType` with `markings`. `markings` is -1 when the firing craft is
   gone. Returns 0 when the mark keeps its stock white. */
int XwaCraftWeapons_ImpactColor(unsigned int objectType, int markings, unsigned int weaponType,
								uint32_t* rgb);
/* Gives mark request `request` (NULL does nothing) the impact colour of
   `bolt`, a bolt of `weaponType`, from its firing craft's object type and
   current markings, as XWAU does. */
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
