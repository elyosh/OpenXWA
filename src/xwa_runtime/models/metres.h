#ifndef XWA_RUNTIME_MODELS_METRES_H
#define XWA_RUNTIME_MODELS_METRES_H

/* Lengths in mod files are in metres. The game measures lengths in game
 * units, 40.96 to the metre, so a reader turns metres into game units once,
 * when it reads the file. Every reader rounds the same way, to the nearest
 * game unit, so that a value in metres with two decimals gives back the
 * game units it was written from. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

/* Converts metres to game units, without rounding. */
double XwaMetres_ToUnits(double metres);

/* Converts game units to metres. */
double XwaMetres_FromUnits(double units);

/* Reads `node`, a number of metres, as game units rounded to the nearest
   unit. Returns 0, and leaves `out` alone, when the node isn't a number or
   the result is outside `minUnits` to `maxUnits`. */
int XwaMetres_Read(const struct AeronConfigNode* node, int64_t minUnits, int64_t maxUnits, int32_t* out);

#ifdef __cplusplus
}
#endif

#endif
