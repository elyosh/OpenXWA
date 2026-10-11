#ifndef XWA_RUNTIME_MODELS_HANGAR_SCENE_H
#define XWA_RUNTIME_MODELS_HANGAR_SCENE_H

/* Modern port: the hangar scene from the hangar settings (hangars.h), as
 * XWAU's hook_hangars builds it. The hangar code calls these instead of its
 * stock spawns while settings are active. */

#ifdef __cplusplus
extern "C" {
#endif

/* The scene objects: the map layout, then the hangar's droids, shuttle and
   roof crane, or the family base's work droid (`family`). */
void XwaHangarScene_Spawn(int family);
/* The hangar shows the HangarIff markings, by default the command ship's. */
void XwaHangarScene_SetHangarMarkings(void);
/* The hangar of another command ship: the hangar model reloads in place and
   the scene spawns again. */
void XwaHangarScene_Rebuild(void);

#ifdef __cplusplus
}
#endif

#endif
