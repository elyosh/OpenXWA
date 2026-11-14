#ifndef XWA_RUNTIME_INPUT_CAPTURE_H
#define XWA_RUNTIME_INPUT_CAPTURE_H

#include "aeron/input.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* While input is captured (options window open, or no focus), nothing
 * reaches the game. Keys held across a capture change, a route change or a
 * host shortcut stay blocked until released. */
void XwaInput_BeginCaptureFrame(const AeronInputSnapshot* input, bool capture);
void XwaInput_SetCaptured(bool capture);
bool XwaInput_IsCaptured(void);
uint32_t XwaInput_FilterMouseButtons(uint32_t buttons);
void XwaInput_ResetCapture(void);
/* Drops queued keyboard commands and the raw DirectInput buffer, and blocks
 * held keys until release. */
void XwaInput_FlushKeyboard(void);
void XwaInput_FlushRawKeyboard(void);
void XwaInput_BlockHeldKeys(void);
void XwaInput_SuppressKey(int key);

#ifdef __cplusplus
}
#endif

#endif
