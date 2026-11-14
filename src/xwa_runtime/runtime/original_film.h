#ifndef XWA_RUNTIME_ORIGINAL_FILM_H
#define XWA_RUNTIME_ORIGINAL_FILM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Films recorded by the original game (version 5 and older), with or
   without a mod's hooks.

   Films save objects by slot index, so the object table must use the
   counts of the game that recorded the film, which can differ from the
   mod's. These films have no layout block, but the counts are in the world
   state's tail, at a fixed distance from the end of the world state.

   A hook such as XWAU's hook_opt_limit makes each craft record larger and
   moves its per-mesh, engine and weapon rack arrays to the new end of the
   record. The mod describes that layout in the `original_films` section of
   flight/flight.yaml:

     original_films:
       craft_record_size: 3829
       meshes: 255             # the entries of each moved array
       engines: 255
       weapon_racks: 128
       component_state: 1017   # the offsets of the moved arrays
       mesh_rotation: 1272
       component_hp: 1527
       engine_health: 1782
       weapon_rack_data: 2037

   Without the section, the craft records have the stock 1017 bytes. */

/* Reads the `original_films` section of flight/flight.yaml. Called once at
   startup. */
void XwaOriginalFilm_Init(void);

/* The size of the craft records of original films. */
uint32_t XwaOriginalFilm_CraftRecordSize(void);

/* Reads the object counts from the end of the film's world state and makes
   the next mission use them. Call it while reading the film's header, before
   the mission loads. Logs an error and returns 0 if the counts can't be
   found. */
int XwaOriginalFilm_UseTailCounts(void);

/* Converts the world state of an original film to stock craft records, in
   place. Array entries past the stock counts go into the world state's
   extension block, and `size` is updated. Call it before
   Flight_RestoreWorldState. Logs an error and returns 0 if the world state
   doesn't match the mod's craft record layout. */
int XwaOriginalFilm_ConvertWorldState(uint8_t* worldState, int* size);

#ifdef __cplusplus
}
#endif

#endif
