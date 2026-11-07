#ifndef XWA_RUNTIME_FRONTEND_FRONTEND_CONFIG_H
#define XWA_RUNTIME_FRONTEND_FRONTEND_CONFIG_H

/* The mod's frontend/frontend.yaml. The file is loaded once and kept, and
 * the modules that use it read their keys from its root. */

#include "aeron/config_file.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XWA_FRONTEND_CONFIG_PATH "frontend/frontend.yaml"

/* The root of the file, or NULL when the mod has no file or it can't be
   read. The first call loads the file and logs why it can't be read. */
const AeronConfigNode* XwaFrontendConfig_Root(void);

/* Frees the file. The next call to XwaFrontendConfig_Root loads it again. */
void XwaFrontendConfig_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
