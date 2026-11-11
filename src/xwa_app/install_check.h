#ifndef XWA_APP_INSTALL_CHECK_H
#define XWA_APP_INSTALL_CHECK_H

#include "aeron/vfs.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sets the asset root to `candidate` and checks that it has the game data:
 * an installed game folder with the contents of the CD's ALLIANCE folder
 * copied into it, plus the CD data folders (FLIGHTMODELS, MISSIONS, MOVIES,
 * RESDATA, WAVE). A plain copy of the CDs is refused with an error that
 * explains the fix. */
int XwaInstall_ValidateGameData(AeronVfs* vfs, const char* candidate, char* normalized,
								size_t normalized_capacity, char* error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
