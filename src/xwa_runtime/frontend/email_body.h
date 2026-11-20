#ifndef XWA_RUNTIME_FRONTEND_EMAIL_BODY_H
#define XWA_RUNTIME_FRONTEND_EMAIL_BODY_H

/* The body of the email selected in the family room's inbox. The original
 * draws it from its first line, so the end of a long email can't be read.
 * Here a body with more lines than its box shows gets a scroll bar at its
 * right, and the text is wrapped 20 units narrower to leave room for it. */

#include "xwa/frontend/frontend_rect.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shows the next email from its first line. Called when the inbox opens. */
void XwaEmailBody_Reset(void);

/* Draws `body`, the text of email `email`, in `rect`. The scroll position
   goes back to the first line when `email` changes. */
void XwaEmailBody_Draw(const char* body, int email, FrontendRect* rect);

#ifdef __cplusplus
}
#endif

#endif
