#include "xwa/util/random.h"

#include "xwa/flight/hud/hud.h"
#include "xwa/math/trig2.h"
#include "xwa/render/renderer.h"
#include "xwa/util/time.h"

typedef union RandomSeedSlot {
	int value;
	uint16_t lowWord;
} RandomSeedSlot;

typedef union UInt64Parts {
	uint64_t value;
	struct {
		uint32_t low;
		uint32_t high;
	} words;
} UInt64Parts;

// GLOBAL: XWA 0x6937AC
RandomSeedSlot g_asteroidFieldRandSeed;
// GLOBAL: XWA 0x6937B0
RandomSeedSlot g_randSeed;
// GLOBAL: XWA 0x6937B4
RandomSeedSlot g_savedRandSeed;
// GLOBAL: XWA 0x6937B8
RandomSeedSlot g_randSeed2;

// FUNCTION: XWA 0x494C10
int32_t MATH2_ABoverC32(int32_t a, int32_t b, int32_t c) {
	int negative;

	negative = 0;
	if (a < 0) {
		a = (int32_t)(0u - (uint32_t)a);
		negative = 1;
	}
	if (b < 0) {
		b = (int32_t)(0u - (uint32_t)b);
		negative = !negative;
	}
	if (c < 0) {
		c = (int32_t)(0u - (uint32_t)c);
		negative = !negative;
	}

	if (negative) {
		UInt64Parts product;
		int32_t result;

		product.value = (uint64_t)(uint32_t)a * (uint64_t)(uint32_t)b;
		if (product.words.high < (uint32_t)c) {
			result = (int32_t)(product.value / (uint32_t)c);
		} else {
			result = 0x7fffffff;
		}
		return -result;
	}

	{
		UInt64Parts product;
		int32_t result;

		product.value = (uint64_t)(uint32_t)a * (uint64_t)(uint32_t)b;
		if (product.words.high < (uint32_t)c) {
			result = (int32_t)(product.value / (uint32_t)c);
		} else {
			result = 0x7fffffff;
		}
		return result;
	}
}

// FUNCTION: XWA 0x494C90
uint16_t MATH2_fraction(uint16_t value, uint16_t fracQ16) {
	uint32_t result = value;

	if (fracQ16 != 0xffffu) {
		result = ((uint32_t)value * (uint32_t)fracQ16 + 0x8000u) >> 16;
	}

	return (uint16_t)result;
}

// FUNCTION: XWA 0x494CC0
uint32_t MATH2_longfraction(uint32_t value, uint16_t fracQ16) {
	uint32_t result;

	if (fracQ16 == 0xffffu) {
		return value;
	}

	result = (value >> 16) * (uint32_t)fracQ16;
	result += ((uint32_t)(uint16_t)value * (uint32_t)fracQ16) >> 16;
	return result;
}

// FUNCTION: XWA 0x494D00
uint16_t MATH2_divide(uint16_t numerator, uint16_t denominator) {
	if (numerator != denominator) {
		if (denominator == 0) {
			return 0;
		}
		if (numerator < denominator) {
			return ((uint32_t)numerator << 16) / (uint32_t)denominator;
		}
	}

	return 0xffffu;
}

// FUNCTION: XWA 0x494D40
uint32_t MATH2_percentage(uint32_t numerator, uint32_t denominator) {
	if (numerator == denominator || denominator == 0 || numerator >= denominator) {
		return 0xffffu;
	}

	while (numerator > 0xffffu || denominator > 0xffffu) {
		numerator >>= 1;
		denominator >>= 1;
	}

	return (numerator << 16) / denominator;
}

// FUNCTION: XWA 0x494D80
void Math_SeedRandom(uint16_t seed) {
	g_randSeed.lowWord = seed;
	g_randSeed2.lowWord = (uint16_t)(seed + timeGetTime());
}

// FUNCTION: XWA 0x494DA0
uint16_t GameRand_SetSavedSeed(uint16_t seed) {
	g_asteroidFieldRandSeed.lowWord = seed;
	return seed;
}

// FUNCTION: XWA 0x494DB0
int16_t GameRand_SetSecondarySeed(int16_t seed) {
	g_randSeed2.lowWord = (uint16_t)seed;
	return seed;
}

// FUNCTION: XWA 0x494DC0
void GameRand_SavePrimarySeed(void) { g_savedRandSeed.lowWord = g_randSeed.lowWord; }

// FUNCTION: XWA 0x494DD0
void GameRand_RestorePrimarySeed(void) { g_randSeed.lowWord = g_savedRandSeed.lowWord; }

// FUNCTION: XWA 0x494DE0
uint16_t GameRand_GetPrimarySeed(void) { return g_randSeed.lowWord; }

// FUNCTION: XWA 0x494DF0
uint16_t GameRand_GetSecondarySeed(void) { return g_randSeed2.lowWord; }

// FUNCTION: XWA 0x494E00
uint16_t GameRand_GetSavedSeed(void) { return g_asteroidFieldRandSeed.lowWord; }

// FUNCTION: XWA 0x494E10
uint16_t GameRand(void) {
	int result;

	result = 9421 * g_randSeed.value + 1;
	g_randSeed.lowWord = (uint16_t)result;
	return result;
}

// FUNCTION: XWA 0x494E40
uint16_t GameRand2(void) {
	int result;

	result = 9421 * g_randSeed2.value + 1;
	g_randSeed2.lowWord = (uint16_t)result;
	return result;
}

// FUNCTION: XWA 0x494E70
uint32_t MATH2_mphconvert(int16_t speed, uint16_t divisor) {
	uint32_t scaled;
	uint32_t result;

	scaled = (uint32_t)((int32_t)speed * 4660 + 128) >> 8;
	result = scaled / (uint32_t)divisor;
	if ((scaled & (uint32_t)divisor) > (scaled >> 1)) {
		++result;
	}

	return result;
}

// FUNCTION: XWA 0x494EB0
void MATH2_getradarcoord(int relX, int relY, int relZ) {
	enum {
		RADAR_ELLIPSE_TABLE_STEP = 443,
		RADAR_ELLIPSE_QUADRANT = 0x4000,
		RADAR_PROJECTED_COORD_MAX = 0x7fff,
	};

	int angle;
	int tableIndex;
	int projectedX;
	int projectedY;
	uint16_t clampY;
	uint16_t clampX;
	uint16_t radarAngle;
	int16_t arctanRatio[2];
	int16_t arctanAngle[2];
	unsigned shift;

	tableIndex = 0;
	for (angle = 0; angle < RADAR_ELLIPSE_QUADRANT; angle += RADAR_ELLIPSE_TABLE_STEP) {
		g_radarEllipseClampTable[tableIndex] =
			(uint8_t)trig2_sinewordmult((uint16_t)g_radarEllipseClampRadius, angle);
		g_radarEllipseClampTable[tableIndex + 1] =
			(uint8_t)trig2_cosinewordmult((uint16_t)g_radarEllipseClampRadius, angle);
		tableIndex += 2;
	}

	projectedY = relY;
	projectedX = relX;
	if (relX < 0) {
		projectedX = -relX;
	}

#ifdef XWA_MODERN
	shift = (unsigned)(uint8_t)(perspShift - 5) & 0x1fu;
#else
	shift = perspShift - 5;
#endif
	projectedX = (int)((uint32_t)projectedX << shift);
	if (relZ != 0) {
		projectedX /= relZ;
	}
	if (projectedX > RADAR_PROJECTED_COORD_MAX) {
		projectedX = RADAR_PROJECTED_COORD_MAX;
	}

	if (relY < 0) {
		projectedY = -relY;
	}

	projectedY = (int)((uint32_t)projectedY << shift);
	if (relZ != 0) {
		projectedY /= relZ;
	}
	if (projectedY > RADAR_PROJECTED_COORD_MAX) {
		projectedY = RADAR_PROJECTED_COORD_MAX;
	}

	radarx = (int16_t)projectedX;
	radary = (int16_t)projectedY;
	trig2_calcarctan_core(projectedX, projectedY, arctanRatio, arctanAngle);

	radarAngle = (uint16_t)(RADAR_ELLIPSE_QUADRANT - (uint16_t)arctanRatio[0]);
	tableIndex = (uint16_t)(2 * (radarAngle / RADAR_ELLIPSE_TABLE_STEP));
	clampX = g_radarEllipseClampTable[tableIndex];
	if (clampX < radarx) {
		radarx = (int16_t)clampX;
	}
	if (relX < 0) {
		radarx = (int16_t)-radarx;
	}

	clampY = g_radarEllipseClampTable[tableIndex + 1];
	if (clampY < radary) {
		radary = (int16_t)clampY;
	}
	if (relY < 0) {
		radary = (int16_t)-radary;
	}
}
