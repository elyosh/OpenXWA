#ifndef XWA_BAKE_XWAU_FLIGHT_H
#define XWA_BAKE_XWAU_FLIGHT_H

/* The mod importer's flight art: converts XWAU's flight DAT groups into
 * atlases with game geometry. */

#include "bake_sprites.h"

typedef struct BakeXwauFlightOptions {
	const BakeSpriteOptions* sprites; /* out_root, formats, BC7 quality and group_filter */
	const char* xwau_root;            /* the XWAU install */
	int planet_max;                   /* scale planet images wider than this down to it; 0 keeps them */
	int threads; /* the threads that decode a group's images; 0 or 1 decodes them on the calling thread */
	/* Called with how far the bake is, from 0 to 1000, on the calling
	   thread. Can be NULL. */
	void (*progress)(void* user, int permille);
	void* progress_user;
} BakeXwauFlightOptions;

typedef struct BakeXwauFlightStats {
	int atlases;     /* group atlases written, frame sets included */
	int images;      /* images written into them */
	int frame_sets;  /* alternative frame sets written */
	int mission_art; /* DATs whose mission art was written (resdata/mission_art) */
	int font_tiers;  /* flight font tiers written */
	int failures;
} BakeXwauFlightStats;

/* Returns nonzero when no failure occurred. */
int BakeXwau_RunFlight(const BakeXwauFlightOptions* opt, BakeXwauFlightStats* stats);

#endif
