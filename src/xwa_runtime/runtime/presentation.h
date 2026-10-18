#ifndef XWA_RUNTIME_PRESENTATION_H
#define XWA_RUNTIME_PRESENTATION_H

/*
 * OpenXWA application presentation contract.
 *
 * The presentation frame is the application-logical coordinate space Aeron
 * composites layers and maps input in. Its height is fixed; its width tracks
 * the host window aspect (XwaPresentation_SyncToWindow) so the frame covers
 * the whole display at any aspect ratio. Classic 640x480 content is
 * aspect-fitted into the frame — pillarboxed or letterboxed, never stretched.
 * These coordinates are deliberately independent of both the classic drawing
 * space and physical GPU render-target sizes.
 */
#define XWA_PRESENTATION_WIDTH 1920 /* initial width; the live width tracks the window */
#define XWA_PRESENTATION_HEIGHT 1080
#define XWA_CLASSIC_WIDTH 640
#define XWA_CLASSIC_HEIGHT 480

typedef struct XwaPresentationRect {
	int x;
	int y;
	int width;
	int height;
} XwaPresentationRect;

/* Largest centered sub-rectangle of bounds with the requested aspect. */
XwaPresentationRect XwaPresentation_AspectFit(int aspect_w, int aspect_h, XwaPresentationRect bounds);

/* Re-derives the presentation frame width from the host window aspect and,
 * when it changed, pushes the new size to Aeron's logical mapping. Called
 * once per frame by the application shell. */
void XwaPresentation_SyncToWindow(int window_width, int window_height);

/* The full application-logical frame. */
XwaPresentationRect XwaPresentation_Frame(void);

/* The centered 4:3 region classic content presents into. */
XwaPresentationRect XwaPresentation_ClassicSafeFrame(void);

/* Sets how the HD frontend is shown. The frontend picture has the shape of
 * the design screen (frontend.yaml `design`) and is fitted in the frame. A
 * wide screen spreads the 640x480 units over the whole picture; other
 * screens keep them in a centered 4:3 area. A design size of 0 means the
 * classic 640x480 surface is shown in the classic safe frame. */
void XwaPresentation_SetFrontendShape(int design_width, int design_height, int wide);

/* Where the frontend picture is shown. */
XwaPresentationRect XwaPresentation_FrontendFrame(void);

/* Where the frontend's 640x480 units are shown: the whole frontend frame on
 * a wide screen, else its centered 4:3 area. */
XwaPresentationRect XwaPresentation_ClassicContentRect(void);

/* Maps an original pixel position to the center of its corresponding pixel
 * footprint in the classic content rectangle. Pixel-center mapping makes
 * the integer forward/inverse transforms exact for all 640x480 input
 * positions. */
void XwaPresentation_FromClassic(int classic_x, int classic_y, int* presentation_x, int* presentation_y);

/* Maps application-logical mouse coordinates into the original 640x480
 * domain. The returned coordinates are clipped; the return value reports
 * whether the unclipped point was inside the classic content rectangle. */
int XwaPresentation_ToClassic(int x, int y, int* classic_x, int* classic_y);

#endif
