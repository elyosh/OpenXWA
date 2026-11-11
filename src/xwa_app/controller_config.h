#ifndef XWA_APP_CONTROLLER_CONFIG_H
#define XWA_APP_CONTROLLER_CONFIG_H
#include "aeron/config_file.h"
#include "xwa_runtime/input/controller_options.h"
bool XwaControllerConfig_Parse(const AeronConfigFile* document, XwaControllerOptions* out, char* error,
							   size_t capacity);
bool XwaControllerConfig_Write(AeronConfigFile* document, const XwaControllerOptions* options,
							   AeronConfigError* error);
bool XwaControllerConfig_ReadProfile(const AeronConfigFile* document, const char* path,
									 AeronControllerKind kind, XwaControllerProfile* profile, char* error,
									 size_t capacity);
#endif
