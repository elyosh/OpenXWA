#ifndef XWA_RUNTIME_PRESENTATION_H
#define XWA_RUNTIME_PRESENTATION_H

/*
 * OpenXWA application presentation contract.
 *
 * These dimensions are application-logical coordinates used by Aeron's
 * compositor and input mapping. They are deliberately independent of both
 * the classic 640x480 drawing space and physical GPU render-target sizes.
 */
#define XWA_PRESENTATION_WIDTH 1920
#define XWA_PRESENTATION_HEIGHT 1080
#define XWA_CLASSIC_WIDTH 640
#define XWA_CLASSIC_HEIGHT 480

typedef struct XwaPresentationRect {
	int x;
	int y;
	int width;
	int height;
} XwaPresentationRect;

static inline XwaPresentationRect XwaPresentation_AspectFit(int aspect_w, int aspect_h,
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

static inline XwaPresentationRect XwaPresentation_Frame(void) {
	return (XwaPresentationRect) { 0, 0, XWA_PRESENTATION_WIDTH, XWA_PRESENTATION_HEIGHT };
}

static inline XwaPresentationRect XwaPresentation_ClassicSafeFrame(void) {
	return XwaPresentation_AspectFit(4, 3, XwaPresentation_Frame());
}

/* Maps an original pixel position to the center of its corresponding pixel
 * footprint in the presentation safe frame. Pixel-center mapping makes the
 * integer forward/inverse transforms exact for all 640x480 input positions. */
static inline void XwaPresentation_FromClassic(int classic_x, int classic_y, int* presentation_x,
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

/* Maps application-logical mouse coordinates into the original 640x480
 * domain. The returned coordinates are clipped; the return value reports
 * whether the unclipped point was inside the classic safe frame. */
static inline int XwaPresentation_ToClassic(int x, int y, int* classic_x, int* classic_y) {
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

#endif
