#include "xwa_runtime/frontend/email_body.h"

#include "xwa/frontend/frontend_color.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_scrollbar.h"
#include "xwa/frontend/frontend_text.h"

/* Modern port module; no original counterpart. */

enum {
	EMAIL_BODY_FONT = 12,
	EMAIL_BODY_LINE_SPACING = 3,
	/* A first line past the end of any text: FrontendText_DrawWrapped then
	   only measures the text. */
	EMAIL_BODY_MEASURE = 0x1000,
	/* The scroll bar's control. No other scroll bar uses 6. */
	EMAIL_BODY_SCROLLBAR = 6,
};

/* The first line shown, and the email it belongs to, or -1. */
static int s_line;
static int s_email = -1;

void XwaEmailBody_Reset(void) { s_email = -1; }

/* The number of lines of `body` wrapped in `rect`. */
static int email_body_lines(const char* body, FrontendRect* rect) {
	return FrontendText_DrawWrapped(EMAIL_BODY_FONT, body, rect, g_colorLightBlue, EMAIL_BODY_LINE_SPACING,
									EMAIL_BODY_MEASURE, 0, 0) +
		   1;
}

void XwaEmailBody_Draw(const char* body, int email, FrontendRect* rect) {
	const int visibleLines = (rect->bottom - rect->top) / (EMAIL_BODY_FONT + EMAIL_BODY_LINE_SPACING);
	FrontendRect textRect;
	FrontendRect barRect;
	int lines;

	if (s_email != email) {
		s_email = email;
		s_line = 0;
	}

	FrontendDraw_RectCopy(&textRect, rect);
	lines = email_body_lines(body, &textRect);
	if (lines > visibleLines) {
		textRect.right -= 20;
		lines = email_body_lines(body, &textRect);
		/* The last position shows the last line at the bottom of the box. */
		FrontendDraw_RectAssign(&barRect, 550, rect->top, 569, rect->bottom);
		s_line = FrontendScrollbar_Draw(&barRect, s_line, lines - visibleLines + 1, 0, 5, g_colorNavy,
										EMAIL_BODY_SCROLLBAR);
	} else {
		s_line = 0;
	}
	FrontendText_DrawWrappedClipped(EMAIL_BODY_FONT, body, &textRect, g_colorLightBlue, EMAIL_BODY_LINE_SPACING,
									s_line);
}
