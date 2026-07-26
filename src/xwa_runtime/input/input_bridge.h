#ifndef XWA_RUNTIME_INPUT_BRIDGE_H
#define XWA_RUNTIME_INPUT_BRIDGE_H

#include "aeron/input.h"

#ifdef __cplusplus
extern "C" {
#endif

void XwaInputBridge_UpdateFrontendMouse(const AeronInputSnapshot* input);
void XwaInputBridge_UpdateFrontendKeyboard(const AeronInputSnapshot* input);
void XwaInputBridge_UpdateFrontend(const AeronInputSnapshot* input);

#ifdef __cplusplus
}
#endif

#endif
