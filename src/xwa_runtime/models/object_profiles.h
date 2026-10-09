#ifndef XWA_RUNTIME_MODELS_OBJECT_PROFILES_H
#define XWA_RUNTIME_MODELS_OBJECT_PROFILES_H

/* Object profiles: model variants that hide some of a model's components.
 *
 * A GLB replacement carries a `hidden_by_profile.<name>` tag on each
 * component profile <name> hides. A mission selects a profile per object
 * through ObjectProfile_* lines in its [Objects] and [HangarObjects]
 * sections; Apply hides the selected profile's components by marking them
 * destroyed at spawn. The selection files are read at startup and at
 * mission load, never when an object spawns. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CraftData;

/* Reads the selections that do not depend on a mission, for both sections:
   FlightModels\<section>.txt, else [<section>] of FlightModels\default.ini.
   They are the tech library's selections and every mission's fallback. Call
   once the VFS is mounted. */
void XwaObjectProfiles_Init(void);

/* Reads mission `missionPath`'s selections, for both sections:
   <mission>_<section>.txt, else [<section>] of <mission>.ini (<mission> is
   the path without its extension). A section with no lines in either file
   uses the startup selections: the first source that has any line wins.
   Replaces the previous mission's selections. */
void XwaObjectProfiles_LoadMission(const char* missionPath);

/* Hides the components of the profile selected for this object: those
   carrying the profile's tag get componentHp 0 and componentState 4. The
   profile is ObjectProfile_fg_<flightGroupIdx>, else ObjectProfile_<model>,
   else Skirmish in a skirmish when the model has it, else
   Default_<markings>; a profile that hides nothing falls back to Default.
   `hangar` selects the HangarObjects lines; `noMission` (the tech library)
   uses the startup selections. No-op for stock models and a NULL craft.
   Reads no file. */
void XwaObjectProfiles_Apply(uint16_t objectType, int flightGroupIdx, uint8_t markings, int hangar,
							 int noMission, struct CraftData* craft);

#ifdef __cplusplus
}
#endif

#endif
