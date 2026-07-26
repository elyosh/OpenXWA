#include "xwa/util/random.h"

// FUNCTION: XWA 0x41E9D0
uint16_t GameRandRange(uint16_t modulus) {
	if (modulus == 0) {
		return 0;
	}

	return (uint16_t)((uint16_t)GameRand() % (int)modulus);
}
