#ifndef XWA_RUNTIME_MODELS_OBJECT_PROFILES_H
#define XWA_RUNTIME_MODELS_OBJECT_PROFILES_H

/* Object profiles: model variants that hide some of a model's components.
 *
 * A GLB replacement lists the components profile <name> hides in its
 * `hidden_by_profile.<name>` component set, which may name exterior or
 * cockpit components past its own. A mission selects a profile per flight
 * group in its mission file (flight/missions/<mission path>.yaml in the mod
 * folder); Apply hides the selected
 * profile's components by marking them destroyed at spawn. The selections
 * are read at mission load, never when an object spawns. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CraftData;
struct XwaFlightGroup;

/* Reads the selections of mission `missionPath` (for example
   "missions\1b1m5fg.tie") from flight/missions/missions/1b1m5fg.yaml in the mod folder,
   replacing the previous mission's. `flightGroups` is the loaded mission's
   first flight group, `stride` the distance between two of them and `count`
   their number: an entry whose flight group does not exist, or whose `name`
   or `craft` check does not match it, is ignored with a warning. A mission
   without a file selects nothing. */
void XwaObjectProfiles_LoadMission(const char* missionPath, const struct XwaFlightGroup* flightGroups,
								   size_t stride, int count);

/* Hides the components of the profile selected for this object: those
   carrying the profile's tag get componentHp 0 and componentState 4. The
   profile is flight group `flightGroupIdx`'s selection, else Skirmish in a
   quick start or skirmish when the model has it, else Default_<markings>; a
   profile that hides nothing falls back to Default. Flight group 0xFFFF
   selects nothing. No-op for stock models and a NULL craft. Reads no file. */
void XwaObjectProfiles_Apply(uint16_t objectType, int flightGroupIdx, uint8_t markings,
							 struct CraftData* craft);

/* Shows every component, then hides those of profile `profile`. Used for
   hangar objects. A profile the model does not define hides nothing. No-op
   for stock models. */
void XwaObjectProfiles_ApplyNamed(uint16_t objectType, const char* profile, struct CraftData* craft);

#ifdef __cplusplus
}
#endif

#endif
