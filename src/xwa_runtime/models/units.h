#ifndef XWA_RUNTIME_MODELS_UNITS_H
#define XWA_RUNTIME_MODELS_UNITS_H

/* Mod files use common units: metres, milliseconds, MGLT, degrees and
 * percents. The game keeps its own units: game units of length, 40.96 to the
 * metre; ticks of 4 ms; speed units, 2.25 to the MGLT; angle units, 65536 to
 * the turn; and Q16 values, where 65536 is the whole. A reader turns a value
 * into the game's unit once, when it reads the file, and rounds it to the
 * nearest whole unit. So a value written with three decimals gives back the
 * game value it was written from, and the simulation sees the same numbers
 * as before. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

typedef enum XwaUnit {
	/* The file holds the game's own value. */
	XWA_UNIT_NONE,
	/* Metres, read as game units. */
	XWA_UNIT_METRES,
	/* Milliseconds, read as ticks. */
	XWA_UNIT_MILLISECONDS,
	/* MGLT, read as speed units. */
	XWA_UNIT_MGLT,
	/* Degrees, read as angle units. */
	XWA_UNIT_DEGREES,
	/* A percent, read as a Q16 value where 65536 is 100%. */
	XWA_UNIT_PERCENT_Q16,
} XwaUnit;

/* Converts `value` in `unit` to the game's unit, without rounding. */
double XwaUnits_ToGame(XwaUnit unit, double value);

/* Converts `game`, in the game's unit, to `unit`. */
double XwaUnits_FromGame(XwaUnit unit, double game);

/* The name of `unit` for log messages, such as "metres". */
const char* XwaUnits_Name(XwaUnit unit);

/* Reads `node`, a number in `unit`, as a game value rounded to the nearest
   whole unit. Returns 0, and leaves `out` alone, when the node isn't a
   number or the result is outside `minGame` to `maxGame`. With
   XWA_UNIT_NONE, the node must be a whole number. */
int XwaUnits_Read(const struct AeronConfigNode* node, XwaUnit unit, int64_t minGame, int64_t maxGame,
				  int32_t* out);

#ifdef __cplusplus
}
#endif

#endif
