#ifndef XWA_APP_KEYBOARD_CONFIG_H
#define XWA_APP_KEYBOARD_CONFIG_H
#include "aeron/config_file.h"
#include "xwa_runtime/input/keyboard_mapping.h"

bool XwaKeyboardConfig_Read(const AeronConfigFile* document, XwaKeyboardBindings* profile, char* error,
							size_t capacity);
bool XwaKeyboardConfig_Write(AeronConfigFile* document, const XwaKeyboardBindings* profile,
							 AeronConfigError* error);
/* Gives the bindings in effect: the shipped `defaults` with the user's
 * input.keyboard over them. An action that the user lists replaces its
 * defaults, including with an empty list. A user source also removes the same
 * source from inherited actions. Without input.keyboard in `user`, the
 * defaults are used. */
bool XwaKeyboardConfig_Resolve(const XwaKeyboardBindings* defaults, const AeronConfigFile* user,
							   XwaKeyboardBindings* effective, char* error, size_t capacity);
#endif
