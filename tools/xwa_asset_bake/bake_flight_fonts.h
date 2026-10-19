#ifndef XWA_BAKE_FLIGHT_FONTS_H
#define XWA_BAKE_FLIGHT_FONTS_H

#include "bake_sprites.h"
#include "xwa_2d.h"

#include "aeron/vfs.h"

typedef struct BakeFlightFontsOptions {
	const char*    out_root;
	int            scale;
	int            zstd;
	Ktx2Bc7Quality bc7_quality;
} BakeFlightFontsOptions;

/* Bake the three hardware flight-font tiers. Returns 3 on success. */
int BakeFlightFonts_Run(const BakeFlightFontsOptions* opt, AeronVfs* vfs);

/* Writes the three tiers from `group`, the decoded images of the flight font
 * group (16000), sorted by id. Returns the number of tiers written, 3 on
 * success. */
int BakeFlightFonts_WriteTiers(const BakeFlightFontsOptions* opt, const Xwa2dFrameSet* group);

#endif
