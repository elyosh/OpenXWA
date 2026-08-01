#ifndef XWA_REMASTER_TEXT_H
#define XWA_REMASTER_TEXT_H

#include "aeron/scene/draw_list2d.h"
#include "xwa_runtime/runtime/presentation.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_remaster/assets.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Adds one captured frontend glyph in a target that represents the classic
 * 640x480 coordinate space. The target dimensions select the output scale. */
int XwaRemasterText_AddFrontendGlyph(AeronDrawList2D* list, XwaRemasterAssets* assets,
									 const XwaGlyph2D* glyph, int target_width, int target_height);

/* Flight-font byte strings use the recovered 0xfe,color escape convention.
 * size_px is the rendered font cell height in target pixels. */
float XwaRemasterText_MeasureFlightString(const XwaFlightFontRef* font, const char* text,
										 float size_px);
typedef enum XwaRemasterTextAlign {
	XWA_REMASTER_TEXT_ALIGN_LEFT = 0,
	XWA_REMASTER_TEXT_ALIGN_CENTER = 1,
	XWA_REMASTER_TEXT_ALIGN_RIGHT = 2,
} XwaRemasterTextAlign;
int XwaRemasterText_AddFlightString(AeronDrawList2D* list, const XwaFlightFontRef* font,
									const char* text, float x_px, float y_px, float size_px,
									XwaRemasterTextAlign align, uint32_t initial_argb,
									const AeronRectI* scissor);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_TEXT_H */
