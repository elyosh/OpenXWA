#include "xwa_runtime/models/units.h"

#include "aeron/config_file.h"

#include <math.h>

/* Modern port module; no original counterpart. */

/* The game units in one unit of the file, by XwaUnit. */
static const double s_gamePerUnit[] = {
	1.0,             /* XWA_UNIT_NONE */
	40.96,           /* XWA_UNIT_METRES */
	0.25,            /* XWA_UNIT_MILLISECONDS */
	2.25,            /* XWA_UNIT_MGLT */
	65536.0 / 360.0, /* XWA_UNIT_DEGREES */
	65536.0 / 100.0, /* XWA_UNIT_PERCENT_Q16 */
};

static const char* const s_unitNames[] = { "", "metres", "milliseconds", "MGLT", "degrees", "percent" };

double XwaUnits_ToGame(XwaUnit unit, double value) { return value * s_gamePerUnit[unit]; }

double XwaUnits_FromGame(XwaUnit unit, double game) { return game / s_gamePerUnit[unit]; }

const char* XwaUnits_Name(XwaUnit unit) { return s_unitNames[unit]; }

int XwaUnits_Read(const AeronConfigNode* node, XwaUnit unit, int64_t minGame, int64_t maxGame, int32_t* out) {
	double game;

	if (unit == XWA_UNIT_NONE) {
		if (AeronConfigNode_Type(node) != AERON_CONFIG_INT) {
			return 0;
		}
		game = (double)AeronConfigNode_Int(node, 0);
	} else if (AeronConfigNode_Type(node) == AERON_CONFIG_INT ||
			   AeronConfigNode_Type(node) == AERON_CONFIG_FLOAT) {
		game = round(XwaUnits_ToGame(unit, AeronConfigNode_Float(node, 0.0)));
	} else {
		return 0;
	}
	if (!(game >= (double)minGame && game <= (double)maxGame)) {
		return 0;
	}
	*out = (int32_t)game;
	return 1;
}
