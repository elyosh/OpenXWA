#ifndef XWA_APP_OPTIONS_VR_TRACKING_PAGE_H
#define XWA_APP_OPTIONS_VR_TRACKING_PAGE_H
#include "aeron/scene/ui.h"

#include <stdbool.h>
/* The tab with the VR headset, head tracking and VR controls. Builds without
 * VR show only head tracking, under that name. */
const char* XwaVrTrackingPage_Title(void);
void XwaVrTrackingPage_Draw(AeronUiContext* ui, const AeronInputSnapshot* input);
/* True once after the player asks for a restart, to apply the VR setting. */
bool XwaVrTrackingPage_TakeRestartRequest(void);
#endif
