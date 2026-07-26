#include "xwa/math/scalar.h"

// FUNCTION: XWA 0x441EB0
void Math_SetFpuSinglePrecisionMode(void) {
	/* Original MSVC/x87 code selects single precision; the portable port leaves
	   the host floating-point environment unchanged. */
}
