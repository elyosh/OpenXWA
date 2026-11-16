#ifndef XWA_APP_OPTIONS_VIDEO_PAGE_H
#define XWA_APP_OPTIONS_VIDEO_PAGE_H
#include "aeron/scene/ui.h"

#include <stdbool.h>
void XwaVideoPage_Draw(AeronUiContext* ui, const AeronInputSnapshot* input);
/* True once after the player asks for a restart, to apply the VR setting. */
bool XwaVideoPage_TakeRestartRequest(void);
#endif
