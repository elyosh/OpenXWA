#include "xwa_runtime/runtime/presentation.h"

#include "aeron/aeron.h"

/* Frame width bounds: at 4:3 (1440) the classic safe frame fills the whole
 * frame and Aeron letterboxes any narrower window; 32:9 (3840) bounds the
 * HUD edge spread and horizontal FOV on extreme ultrawide windows, with
 * Aeron pillarboxing the remainder. */
enum {
	XWA_PRESENTATION_MIN_WIDTH = 4 * XWA_PRESENTATION_HEIGHT / 3,
	XWA_PRESENTATION_MAX_WIDTH = 32 * XWA_PRESENTATION_HEIGHT / 9,
};

static XwaPresentationRect g_frame = { 0, 0, XWA_PRESENTATION_WIDTH, XWA_PRESENTATION_HEIGHT };

XwaPresentationRect XwaPresentation_AspectFit(int aspect_w, int aspect_h,
											  XwaPresentationRect bounds) {
	XwaPresentationRect out = bounds;
	if (aspect_w <= 0 || aspect_h <= 0 || bounds.width <= 0 || bounds.height <= 0) {
		return (XwaPresentationRect) { 0, 0, 0, 0 };
	}
	if ((long long)bounds.width * aspect_h > (long long)bounds.height * aspect_w) {
		out.width = (int)((long long)bounds.height * aspect_w / aspect_h);
		out.x += (bounds.width - out.width) / 2;
	} else {
		out.height = (int)((long long)bounds.width * aspect_h / aspect_w);
		out.y += (bounds.height - out.height) / 2;
	}
	return out;
}

void XwaPresentation_SyncToWindow(int window_width, int window_height) {
	int width;

	if (window_width <= 0 || window_height <= 0) {
		return;
	}
	width = (int)(((long long)XWA_PRESENTATION_HEIGHT * window_width + window_height / 2) /
				  window_height);
	width &= ~1;
	if (width < XWA_PRESENTATION_MIN_WIDTH) {
		width = XWA_PRESENTATION_MIN_WIDTH;
	}
	if (width > XWA_PRESENTATION_MAX_WIDTH) {
		width = XWA_PRESENTATION_MAX_WIDTH;
	}
	if (width == g_frame.width) {
		return;
	}
	g_frame.width = width;
	Aeron_SetLogicalSize(g_frame.width, g_frame.height);
}

XwaPresentationRect XwaPresentation_Frame(void) { return g_frame; }

XwaPresentationRect XwaPresentation_ClassicSafeFrame(void) {
	return XwaPresentation_AspectFit(4, 3, g_frame);
}

void XwaPresentation_FromClassic(int classic_x, int classic_y, int* presentation_x,
								 int* presentation_y) {
	const XwaPresentationRect safe = XwaPresentation_ClassicSafeFrame();
	if (classic_x < 0)
		classic_x = 0;
	if (classic_y < 0)
		classic_y = 0;
	if (classic_x >= XWA_CLASSIC_WIDTH)
		classic_x = XWA_CLASSIC_WIDTH - 1;
	if (classic_y >= XWA_CLASSIC_HEIGHT)
		classic_y = XWA_CLASSIC_HEIGHT - 1;
	if (presentation_x) {
		*presentation_x =
			safe.x + (int)(((long long)classic_x * 2 + 1) * safe.width / (2 * XWA_CLASSIC_WIDTH));
	}
	if (presentation_y) {
		*presentation_y =
			safe.y + (int)(((long long)classic_y * 2 + 1) * safe.height / (2 * XWA_CLASSIC_HEIGHT));
	}
}

int XwaPresentation_ToClassic(int x, int y, int* classic_x, int* classic_y) {
	const XwaPresentationRect safe = XwaPresentation_ClassicSafeFrame();
	const int inside = x >= safe.x && y >= safe.y && x < safe.x + safe.width && y < safe.y + safe.height;
	if (x < safe.x)
		x = safe.x;
	if (y < safe.y)
		y = safe.y;
	if (x >= safe.x + safe.width)
		x = safe.x + safe.width - 1;
	if (y >= safe.y + safe.height)
		y = safe.y + safe.height - 1;
	if (classic_x) {
		*classic_x = (int)((long long)(x - safe.x) * XWA_CLASSIC_WIDTH / safe.width);
	}
	if (classic_y) {
		*classic_y = (int)((long long)(y - safe.y) * XWA_CLASSIC_HEIGHT / safe.height);
	}
	return inside;
}
