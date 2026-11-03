#include "xwa_runtime/models/metres.h"

#include "aeron/config_file.h"

#include <math.h>

/* Modern port module; no original counterpart. */

static const double s_unitsPerMetre = 40.96;

double XwaMetres_ToUnits(double metres) { return metres * s_unitsPerMetre; }

double XwaMetres_FromUnits(double units) { return units / s_unitsPerMetre; }

int XwaMetres_Read(const AeronConfigNode* node, int64_t minUnits, int64_t maxUnits, int32_t* out) {
	double units;

	if (AeronConfigNode_Type(node) != AERON_CONFIG_INT && AeronConfigNode_Type(node) != AERON_CONFIG_FLOAT) {
		return 0;
	}
	units = round(XwaMetres_ToUnits(AeronConfigNode_Float(node, 0.0)));
	if (!(units >= (double)minUnits && units <= (double)maxUnits)) {
		return 0;
	}
	*out = (int32_t)units;
	return 1;
}
