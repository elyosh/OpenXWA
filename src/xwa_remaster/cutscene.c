#include "xwa_remaster/cutscene.h"

#include "aeron/aeron.h"
#include "aeron/scene/draw_list2d.h"
#include "xwa_remaster/text.h"
#include "xwa_runtime/runtime/presentation.h"

#include <stdint.h>
#include <string.h>

static struct {
	AeronRenderTarget* target;
	AeronDrawList2D* draw_list;
	int width;
	int height;
} g_cutscene;

static int cutscene_ensure(int width, int height) {
	if (g_cutscene.target && g_cutscene.width == width && g_cutscene.height == height) {
		return g_cutscene.draw_list != NULL;
	}
	if (g_cutscene.target) {
		Aeron_DestroyRenderTarget(g_cutscene.target);
		g_cutscene.target = NULL;
	}
	g_cutscene.width = width;
	g_cutscene.height = height;
	g_cutscene.target =
		Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) { .width = width,
															.height = height,
															.format = AERON_TEXTURE_FORMAT_RGBA8_SRGB,
															.debug_name = "xwa.cutscene.subtitles" });
	if (!g_cutscene.draw_list) {
		g_cutscene.draw_list = AeronDrawList_Create(XWA_SNAP_MAX_GLYPHS);
	}
	if (!g_cutscene.target || !g_cutscene.draw_list) {
		Aeron_LogError("xwa.remaster", "cutscene subtitle overlay initialization failed");
	}
	return g_cutscene.target != NULL && g_cutscene.draw_list != NULL;
}

AeronTexture* XwaRemasterCutscene_Render(AeronCommandBuffer* cmd, const XwaSnapshot* snapshot,
										 XwaRemasterAssets* assets, int presentation_width,
										 int presentation_height) {
	int width;
	int height;
	int visible_glyphs = 0;

	if (!cmd || !snapshot || !assets || presentation_width <= 0 || presentation_height <= 0) {
		return NULL;
	}
	for (uint32_t i = 0; i < snapshot->glyph_count; ++i) {
		const XwaGlyph2D* glyph = &snapshot->glyphs[i];
		if (glyph->target == XWA_EMIT_TARGET_MAIN && glyph->color != 0) {
			visible_glyphs = 1;
			break;
		}
	}
	if (!visible_glyphs) {
		return NULL;
	}

	const XwaPresentationRect safe = XwaPresentation_AspectFit(
		XWA_CLASSIC_WIDTH, XWA_CLASSIC_HEIGHT,
		(XwaPresentationRect) { 0, 0, presentation_width, presentation_height });
	width = safe.width;
	height = safe.height;
	if (width <= 0 || height <= 0 || !cutscene_ensure(width, height)) {
		return NULL;
	}

	AeronDrawList_Begin(g_cutscene.draw_list, g_cutscene.target, width, height, AERON_DRAWLIST2D_CLEAR, NULL);
	for (uint32_t i = 0; i < snapshot->glyph_count; ++i) {
		const XwaGlyph2D* glyph = &snapshot->glyphs[i];
		if (glyph->target == XWA_EMIT_TARGET_MAIN && glyph->color != 0) {
			(void)XwaRemasterText_AddFrontendGlyph(g_cutscene.draw_list, assets, glyph, width, height);
		}
	}
	AeronDrawList_Render(g_cutscene.draw_list, cmd);
	return Aeron_RenderTargetGetTexture(g_cutscene.target);
}

void XwaRemasterCutscene_Shutdown(void) {
	if (g_cutscene.draw_list) {
		AeronDrawList_Destroy(g_cutscene.draw_list);
	}
	if (g_cutscene.target) {
		Aeron_DestroyRenderTarget(g_cutscene.target);
	}
	memset(&g_cutscene, 0, sizeof g_cutscene);
}
