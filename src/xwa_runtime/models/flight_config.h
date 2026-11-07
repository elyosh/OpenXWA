#ifndef XWA_RUNTIME_MODELS_FLIGHT_CONFIG_H
#define XWA_RUNTIME_MODELS_FLIGHT_CONFIG_H

/* The mod's flight/flight.yaml. The file is loaded once and kept, and the
 * modules that use it read their sections from its root. */

#include "aeron/config_file.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XWA_FLIGHT_CONFIG_PATH "flight/flight.yaml"

/* The root of the file, or NULL when the mod has no file or it can't be
   read. The first call loads the file and logs why it can't be read. */
const AeronConfigNode* XwaFlightConfig_Root(void);

/* Frees the file. The next call to XwaFlightConfig_Root loads it again. */
void XwaFlightConfig_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
