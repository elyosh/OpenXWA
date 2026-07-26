#include "xwa/util/color.h"

// FUNCTION: XWA 0x4D3FB0
unsigned int Color_FindNearestRgbTripletIndex(const RgbTriplet* targetRgb, const RgbTriplet* palette,
											  unsigned int startIndex, unsigned int endIndex) {
	unsigned int index;
	unsigned int nearestIndex;
	int bestDistance;
	int targetR;
	int targetG;
	int targetB;

	nearestIndex = startIndex;
	index = startIndex;
	bestDistance = 0x7fffffff;
	if (startIndex < endIndex) {
		targetR = targetRgb->r;
		targetG = targetRgb->g;
		targetB = targetRgb->b;
		do {
			int dr;
			int dg;
			int db;
			int distance;

			dr = targetR - palette[index].r;
			dg = targetG - palette[index].g;
			db = targetB - palette[index].b;
			distance = dr * dr + dg * dg + db * db;
			if (distance < bestDistance) {
				bestDistance = distance;
				nearestIndex = index;
			}
			++index;
		} while (index < endIndex);
	}

	return nearestIndex;
}
