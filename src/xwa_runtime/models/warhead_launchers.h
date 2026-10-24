#ifndef XWA_RUNTIME_MODELS_WARHEAD_LAUNCHERS_H
#define XWA_RUNTIME_MODELS_WARHEAD_LAUNCHERS_H

/* Helpers for the warhead rules of flight/flight.yaml, two_launcher_fixes and
 * weapon_rack_count_from_record. In the stock game, only the Missile Boat and
 * the Skipray have two warhead launchers, and most of the code only handles
 * the Missile Boat. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CraftData;

/* Whether the craft record of `objectType` has a second warhead launcher. */
int XwaWarheadLaunchers_HasSecond(unsigned int objectType);

/* The number of launchers of `craft` that the hangar counts as loaded: 1 when
   the first launcher holds a warhead, 2 when the second one does too, else
   0. */
uint8_t XwaWarheadLaunchers_Loaded(const struct CraftData* craft);

/* The number of warheads left in launcher `launcher` of `craft`, or 0 when
   the craft doesn't have that launcher. */
int XwaWarheadLaunchers_Warheads(const struct CraftData* craft, int launcher);

/* The warhead index (into g_warheadTypeIds and g_warheadAmmoCounts) of
   warhead type `warheadType`, or 0 when it isn't a warhead. */
uint16_t XwaWarheadLaunchers_Index(uint16_t warheadType);

/* The rack count of a craft with craft record `modelDefIndex`: one more than
   the last rack of its laser groups and warhead launchers. */
uint8_t XwaWarheadLaunchers_RackCount(uint16_t modelDefIndex);

#ifdef __cplusplus
}
#endif

#endif
