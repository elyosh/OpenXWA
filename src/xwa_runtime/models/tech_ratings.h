#ifndef XWA_RUNTIME_MODELS_TECH_RATINGS_H
#define XWA_RUNTIME_MODELS_TECH_RATINGS_H

/* The craft ratings of the tech room, the briefing, mission setup and
 * skirmish, and the points a craft is worth.
 *
 * The stock game computes them from the craft record and, for the size, from
 * the bounds of the loaded model. Mission setup and skirmish read them from
 * spec.rci, a file built once from the stock models. OpenXWA computes them
 * live everywhere instead:
 *   - the size comes from the model's file, without loading the model: the
 *     JSON part of a GLB, or the mesh descriptors of an OPT;
 *   - speed, acceleration, maneuverability, shields and hull come from the
 *     `default` stats profile of the model when it sets them, else from the
 *     record. The shield and hull ratings are then the profile's values,
 *     without the rounding of the record's units;
 *   - the score, which also sets the points that a craft is worth in flight,
 *     uses the same profile values.
 * Without a stats profile, the ratings come from the record, as in the stock
 * game. The warhead rating keeps the record's capacity: a weapon profile's
 * capacity depends on the warheads that a flight group loads. */

#include "xwa/assets/model_def.h"
#include "xwa/frontend/tech_library.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sets `boundSize` to the largest size of the model of `craftType`, read from
   its file, rounded as the model loader rounds it. Returns 0, and leaves
   `boundSize` alone, when the file can't be read. */
int XwaTechRatings_BoundSize(int craftType, int* boundSize);

/* Replaces the ratings of `stats`, a craft of `objectType`, that the model's
   default stats profile sets. Call it after the genus scaling. */
void XwaTechRatings_ApplyProfile(unsigned int objectType, CraftTechStats* stats);

/* Writes the values of the model's default stats profile into `record`, a
   copy of the craft record of `objectType`: speed, the three rates, shields,
   hull and whether it has shields. */
void XwaTechRatings_ProfileRecord(unsigned int objectType, ModelDef* record);

/* Builds the ratings of every craft of the ship list, in a block from
   Mem_Alloc, in place of reading spec.rci. Frees the previous `*table`.
   Sets `*count` to 0 when out of memory. */
void XwaTechRatings_BuildTable(CraftTechStats** table, int* count);

#ifdef __cplusplus
}
#endif

#endif
