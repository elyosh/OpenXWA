#ifndef XWA_BAKE_XWAU_FLIGHT_H
#define XWA_BAKE_XWAU_FLIGHT_H

/* xwa_asset_bake xwau-flight: converts XWAU's flight DAT groups into atlases
 * with game geometry. See bake_xwau_flight.c. */

#include "bake_sprites.h"

typedef struct BakeXwauFlightOptions {
	const BakeSpriteOptions* sprites; /* out_root, formats, BC7 quality and group_filter */
	const char* xwau_root;            /* the XWAU install */
	int planet_max;                   /* halve planet images wider than this; 0 keeps them */
} BakeXwauFlightOptions;

typedef struct BakeXwauFlightStats {
	int atlases;    /* group atlases written, frame sets included */
	int images;     /* images written into them */
	int frame_sets; /* alternative frame sets written */
	int sets;       /* sets of the missions' own art written (resdata/sets) */
	int font_tiers; /* flight font tiers written */
	int failures;
} BakeXwauFlightStats;

/* Returns nonzero when no failure occurred. */
int BakeXwau_RunFlight(const BakeXwauFlightOptions* opt, BakeXwauFlightStats* stats);

#endif
