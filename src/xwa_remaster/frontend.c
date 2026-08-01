/*
 * XWA remaster driver — frontend 2D reconstruction.
 *
 * MODEL: mirror the engine's surface structure instead of its pixels
 * (the TIE incremental model, extended). Two RTs:
 *
 *   screen_rt : mirrors the persistent offscreen surface. Records
 *               emitted while the offscreen surface was locked land
 *               here and persist across ticks. Cleared on scene
 *               transitions; rooms repaint fully through records.
 *   output_rt : the per-tick visible frame. Rebuilt every tick as
 *               screen_rt + this tick's back-buffer records in z
 *               order — so transients (cursor, hover label, animated
 *               cels) never accumulate: their erasure is structural,
 *               mirroring the engine's per-present offscreen restore.
 *
 * Surface events captured from the engine's own copy operations are
 * replayed in z order between record segments:
 *   OFFSCREEN_RESTORE  -> output <- screen (transient wipe)
 *   BACKBUFFER_SAVE    -> screen <- output (promote composed frame)
 *   SCREEN_PUSH_SAVE   -> push a copy of the saved region (dialogs)
 *   SCREEN_POP_RESTORE -> draw the saved region back
 *
 * Textures are reconciled before reconstruction from the snapshot's
 * authoritative classic named-resource and DAT-group sets. Draw translation
 * is lookup-only; source-file identity stamped at emission disambiguates names
 * reused by different rooms (for example "background").
 *
 * HD is the reconstruction full-screen; SPLIT is classic left and
 * reconstruction right for live parity comparison; CLASSIC is classic only.
 * F2 toggles SPLIT against the current full-screen mode without a fade. F5
 * fades between the CLASSIC and HD full-screen modes.
 */

#include "xwa_remaster/frontend.h"

#include "aeron/aeron.h"
#include "aeron/scene/draw_list2d.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/preview.h"
#include "xwa_remaster/text.h"
#include "xwa_runtime/runtime/presentation.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RM_SAVE_STACK_CAP 8
#define RM_RETIRED_TARGET_CAP (3 + RM_SAVE_STACK_CAP)

typedef struct RmSaveSlot {
	AeronRenderTarget* rt;
	int16_t left, top, right, bottom;
} RmSaveSlot;

typedef struct RmState {
	AeronRenderTarget* screen_rt;
	AeronRenderTarget* output_rt;
	/* External (scratch-surface) mirror: EXTERNAL-tagged records draw
	 * here; COMPOSITE surface events copy from it (briefing wireframe
	 * hologram). Persistent across ticks like the classic scratch. */
	AeronRenderTarget* external_rt;
	AeronDrawList2D* list;
	int rt_width;
	int rt_height;
	XwaSceneKind last_scene;
	int screen_needs_clear;
	RmSaveSlot save_stack[RM_SAVE_STACK_CAP];
	int save_depth;
	/* Targets sampled during the last resize migration. They remain alive
	 * until the next render call, after their command buffer was submitted. */
	AeronRenderTarget* retired_targets[RM_RETIRED_TARGET_CAP];
	int retired_target_count;
	XwaRemasterAssets* assets; /* borrowed per Render call */
	/* Per-tick PiP results (rendered in rm_prepare_model_previews,
	 * composited by the z-merge; NULL = no cooked model, classic covers it). */
	AeronTexture* preview_tex[XWA_SNAP_MAX_MODEL_PREVIEWS];
	int warned_save_stack;
	/* Resolver coverage telemetry (throttled log). */
	uint32_t stat_hits;
	uint32_t stat_misses;
	uint32_t stat_ticks;
	char stat_miss_keys[4][48];
	int stat_miss_key_count;
} RmState;

static RmState g;

/* ---- record translation --------------------------------------------- */

/* The 2D compose domain is LINEAR (TIE invariant): sprite textures are
 * BC7_SRGB (hardware-decoded to linear on sample), the RTs are
 * RGBA8_SRGB (hardware-encoded on write), and the output layer is
 * declared LINEAR_DISPLAY so present encodes exactly once and may remap
 * the display gamma for HDR output. Solid engine colors are
 * sRGB-encoded and must be converted before entering the shader
 * domain. */
/* Engine 16bpp color -> linear RGB floats (alpha separate). */
static void rm_color_linear(uint32_t engine_color, float out_rgb[3]) {
	uint8_t rgba[4];
	XwaSnapshotExport_ColorToRgba(engine_color, rgba);
	out_rgb[0] = XwaRemaster_SrgbToLinear((float)rgba[0] / 255.0f);
	out_rgb[1] = XwaRemaster_SrgbToLinear((float)rgba[1] / 255.0f);
	out_rgb[2] = XwaRemaster_SrgbToLinear((float)rgba[2] / 255.0f);
}

static float rm_scale_x(void) { return (float)g.rt_width / (float)XWA_CLASSIC_WIDTH; }

static float rm_scale_y(void) { return (float)g.rt_height / (float)XWA_CLASSIC_HEIGHT; }

/* Classic-pixel EDGE -> RT-pixel edge. All integer rects (scissors,
 * reveal bands, save regions) round edges through these functions so
 * adjacent regions computed from the same classic edge share the same
 * RT pixel row or column — no seams or overlaps at fractional scales. */
static int rm_edge_for(int c, int target_extent, int classic_extent) {
	return (int)lroundf((float)c * (float)target_extent / (float)classic_extent);
}

static int rm_x_edge(int c) { return rm_edge_for(c, g.rt_width, XWA_CLASSIC_WIDTH); }

static int rm_y_edge(int c) { return rm_edge_for(c, g.rt_height, XWA_CLASSIC_HEIGHT); }

/* Inclusive 640x480 clip rect -> RT-pixel scissor; full-screen -> none. */
static AeronRectI rm_scissor(int16_t l, int16_t t, int16_t r, int16_t b) {
	AeronRectI rect = { 0, 0, 0, 0 };
	if (l <= 0 && t <= 0 && r >= 639 && b >= 479) {
		return rect; /* zero size = no scissor */
	}
	rect.x = rm_x_edge(l);
	rect.y = rm_y_edge(t);
	rect.width = rm_x_edge(r + 1) - rm_x_edge(l);
	rect.height = rm_y_edge(b + 1) - rm_y_edge(t);
	return rect;
}

static void rm_add_draw(AeronDrawList2D* list, const XwaDraw2D* d) {
	XwaAssetRef ref;
	int found;
	if (d->kind == XWA_DRAW2D_ATLAS_SPRITE) {
		found = XwaRemasterAssets_FrontendAtlasSprite(g.assets, d->atlas_group, d->atlas_index, &ref);
	} else {
		found = XwaRemasterAssets_FrontendSprite(g.assets, d->file, d->frame, &ref);
	}
	if (!found || !ref.texture || d->img_w <= 0 || d->img_h <= 0) {
		g.stat_misses++;
		/* Remember a few distinct miss identities for the heartbeat. */
		char key[48];
		if (d->kind == XWA_DRAW2D_ATLAS_SPRITE) {
			snprintf(key, sizeof key, "group %d/%d", d->atlas_group, d->atlas_index);
		} else {
			snprintf(key, sizeof key, "%s('%s') f%d", d->file[0] ? d->file : "<unbound>", d->name, d->frame);
		}
		int seen = 0;
		for (int i = 0; i < g.stat_miss_key_count; i++) {
			if (strncmp(g.stat_miss_keys[i], key, sizeof key) == 0) {
				seen = 1;
				break;
			}
		}
		if (!seen && g.stat_miss_key_count < 4) {
			snprintf(g.stat_miss_keys[g.stat_miss_key_count++], sizeof g.stat_miss_keys[0], "%s", key);
		}
		return; /* no modern asset -> not drawn in HD (classic covers it) */
	}
	g.stat_hits++;

	AeronDrawList2DSprite s = { 0 };
	s.texture = ref.texture;
	s.filter = AERON_BLIT2D_FILTER_LINEAR;
	s.blend = AERON_BLIT2D_BLEND_PMA;
	s.tint[0] = s.tint[1] = s.tint[2] = s.tint[3] = 1.0f;
	s.scissor = rm_scissor(d->clip_left, d->clip_top, d->clip_right, d->clip_bottom);

	/* Geometry authority is the record's CLASSIC dims: dst extents in
	 * classic px scaled to the live target, src rect mapped proportionally inside the
	 * resolved frame's UV rect. Asset resolution never enters — a
	 * hand-authored replacement of any size maps identically. */
	float su0 = ref.u0, sv0 = ref.v0, su1 = ref.u1, sv1 = ref.v1;
	int dst_cw = d->img_w, dst_ch = d->img_h;
	if (d->has_src_rect) {
		const float iw = (float)d->img_w;
		const float ih = (float)d->img_h;
		const float du = ref.u1 - ref.u0;
		const float dv = ref.v1 - ref.v0;
		su0 = ref.u0 + ((float)d->src_left / iw) * du;
		sv0 = ref.v0 + ((float)d->src_top / ih) * dv;
		su1 = ref.u0 + ((float)(d->src_right + 1) / iw) * du;
		sv1 = ref.v0 + ((float)(d->src_bottom + 1) / ih) * dv;
		dst_cw = d->src_right - d->src_left + 1;
		dst_ch = d->src_bottom - d->src_top + 1;
	}
	/* The classic oriented blits use modes 1 / 3 for the two 90-degree
	 * rotations, 2 for a vertical flip, and 4 for a horizontal flip.
	 * Briefing-map ship icons use these modes to face along their heading.
	 * Rotations need per-corner UVs — a Quad4 with the dst extents transposed,
	 * corners mapped per the classic source walks (mode 1:
	 * dst(c,r) <- src(row top+c, col right-r); mode 3:
	 * dst(c,r) <- src(row bottom-c, col left+r)). */
	const int oriented =
		d->kind == XWA_DRAW2D_SPRITE_RECT_ORIENTED || d->kind == XWA_DRAW2D_SPRITE_RECT_TINTED_ORIENTED;
	if (oriented && (d->orientation_mode == 1 || d->orientation_mode == 3)) {
		AeronDrawList2DQuad4 q = { 0 };
		q.texture = ref.texture;
		q.filter = AERON_BLIT2D_FILTER_LINEAR;
		q.blend = AERON_BLIT2D_BLEND_PMA;
		q.scissor = s.scissor;
		if (d->kind == XWA_DRAW2D_SPRITE_RECT_TINTED_ORIENTED) {
			rm_color_linear(d->tint_color, q.tint);
		} else {
			q.tint[0] = q.tint[1] = q.tint[2] = 1.0f;
		}
		q.tint[3] = 1.0f;
		const float scale_x = rm_scale_x();
		const float scale_y = rm_scale_y();
		const float x0 = (float)d->dst_x * scale_x;
		const float y0 = (float)d->dst_y * scale_y;
		const float x1 = x0 + (float)dst_ch * scale_x; /* transposed dst */
		const float y1 = y0 + (float)dst_cw * scale_y;
		/* corners[] = TL, TR, BL, BR {x, y, u, v} */
		q.corners[0][0] = x0;
		q.corners[0][1] = y0;
		q.corners[1][0] = x1;
		q.corners[1][1] = y0;
		q.corners[2][0] = x0;
		q.corners[2][1] = y1;
		q.corners[3][0] = x1;
		q.corners[3][1] = y1;
		if (d->orientation_mode == 1) {
			q.corners[0][2] = su1;
			q.corners[0][3] = sv0;
			q.corners[1][2] = su1;
			q.corners[1][3] = sv1;
			q.corners[2][2] = su0;
			q.corners[2][3] = sv0;
			q.corners[3][2] = su0;
			q.corners[3][3] = sv1;
		} else {
			q.corners[0][2] = su0;
			q.corners[0][3] = sv1;
			q.corners[1][2] = su0;
			q.corners[1][3] = sv0;
			q.corners[2][2] = su1;
			q.corners[2][3] = sv1;
			q.corners[3][2] = su1;
			q.corners[3][3] = sv0;
		}
		AeronDrawList_AddQuad4(list, &q);
		return;
	}
	if (oriented && d->orientation_mode == 2) {
		const float t = sv0; /* vertical flip */
		sv0 = sv1;
		sv1 = t;
	} else if (oriented && d->orientation_mode == 4) {
		const float t = su0; /* horizontal flip */
		su0 = su1;
		su1 = t;
	}

	s.src_u0 = su0;
	s.src_v0 = sv0;
	s.src_u1 = su1;
	s.src_v1 = sv1;

	s.dst_x = (float)d->dst_x * rm_scale_x();
	s.dst_y = (float)d->dst_y * rm_scale_y();
	s.dst_w = (float)dst_cw * rm_scale_x();
	s.dst_h = (float)dst_ch * rm_scale_y();

	if (d->kind == XWA_DRAW2D_SPRITE_RECT_TINTED || d->kind == XWA_DRAW2D_SPRITE_RECT_TINTED_ORIENTED) {
		rm_color_linear(d->tint_color, s.tint);
		s.tint[3] = 1.0f;
	} else if (d->kind == XWA_DRAW2D_SPRITE_TRANSLUCENT) {
		/* 50/50 blend factor, not a color — stays a raw multiplier. */
		s.tint[0] = s.tint[1] = s.tint[2] = s.tint[3] = 0.5f;
	}

	if (d->kind == XWA_DRAW2D_SPRITE_OPAQUE) {
		/* The classic opaque blit writes palette entry 0 where the keyed
		 * texture is transparent. Rebuild that lost color beneath the PMA
		 * sprite; this also gives filtered HD edges the correct coverage. */
		float fill[4];
		rm_color_linear(d->opaque_fill_color, fill);
		fill[3] = 1.0f;
		const AeronRectI* scissor = s.scissor.width > 0 && s.scissor.height > 0 ? &s.scissor : NULL;
		AeronDrawList_AddFill(list, s.dst_x, s.dst_y, s.dst_w, s.dst_h, fill, AERON_BLIT2D_BLEND_NONE,
							  scissor);
	}
	AeronDrawList_AddSprite(list, &s);
}

static void rm_add_paint(AeronDrawList2D* list, const XwaPaintCmd* p) {
	float rgba[4];
	rm_color_linear(p->color, rgba);
	rgba[3] = 1.0f;
	AeronRectI sc = rm_scissor(p->clip_left, p->clip_top, p->clip_right, p->clip_bottom);
	const AeronRectI* scp = sc.width > 0 ? &sc : NULL;
	const float sx = rm_scale_x();
	const float sy = rm_scale_y();
	const float line_thickness = fminf(sx, sy);

	switch ((XwaPaintKind)p->kind) {
		case XWA_PAINT_HLINE:
			AeronDrawList_AddFill(list, p->x0 * sx, p->y0 * sy, (p->x1 - p->x0 + 1) * sx, sy, rgba,
								  AERON_BLIT2D_BLEND_NONE, scp);
			break;
		case XWA_PAINT_VLINE:
			AeronDrawList_AddFill(list, p->x0 * sx, p->y0 * sy, sx, (p->y1 - p->y0 + 1) * sy, rgba,
								  AERON_BLIT2D_BLEND_NONE, scp);
			break;
		case XWA_PAINT_LINE:
		case XWA_PAINT_LINE_AA:
			/* One record per segment (rotated quad; the briefing hologram
			 * draws 20k+ per tick). Endpoints at classic texel centers,
			 * classic-px stroke. */
			AeronDrawList_AddLine(list, p->x0 * sx + sx * 0.5f, p->y0 * sy + sy * 0.5f,
								  p->x1 * sx + sx * 0.5f, p->y1 * sy + sy * 0.5f, line_thickness, rgba,
								  AERON_BLIT2D_BLEND_NONE, scp);
			break;
		case XWA_PAINT_FILL_TRANSLUCENT: {
			float half[4] = { rgba[0] * 0.5f, rgba[1] * 0.5f, rgba[2] * 0.5f, 0.5f };
			AeronDrawList_AddFill(list, (p->x0 + p->dx) * sx, (p->y0 + p->dy) * sy, (p->x1 - p->x0 + 1) * sx,
								  (p->y1 - p->y0 + 1) * sy, half, AERON_BLIT2D_BLEND_PMA, scp);
			break;
		}
		case XWA_PAINT_PIXEL:
			AeronDrawList_AddFill(list, p->x0 * sx, p->y0 * sy, sx, sy, rgba, AERON_BLIT2D_BLEND_NONE, scp);
			break;
		case XWA_PAINT_FILL_RECT: {
			AeronDrawList_AddFill(list, (p->x0 + p->dx) * sx, (p->y0 + p->dy) * sy, (p->x1 - p->x0 + 1) * sx,
								  (p->y1 - p->y0 + 1) * sy, rgba, AERON_BLIT2D_BLEND_NONE, scp);
			break;
		}
		case XWA_PAINT_RECT_OUTLINE: {
			const float x = (p->x0 + p->dx) * sx;
			const float y = (p->y0 + p->dy) * sy;
			const float w = (p->x1 - p->x0 + 1) * sx;
			const float h = (p->y1 - p->y0 + 1) * sy;
			AeronDrawList_AddFill(list, x, y, w, sy, rgba, AERON_BLIT2D_BLEND_NONE, scp); /* top */
			AeronDrawList_AddFill(list, x, y + h - sy, w, sy, rgba, AERON_BLIT2D_BLEND_NONE,
								  scp);                                                   /* bottom */
			AeronDrawList_AddFill(list, x, y, sx, h, rgba, AERON_BLIT2D_BLEND_NONE, scp); /* left */
			AeronDrawList_AddFill(list, x + w - sx, y, sx, h, rgba, AERON_BLIT2D_BLEND_NONE, scp); /* right */
			break;
		}
	}
}

static void rm_add_glyph(AeronDrawList2D* list, const XwaGlyph2D* gl) {
	(void)XwaRemasterText_AddFrontendGlyph(list, g.assets, gl, g.rt_width, g.rt_height);
}

/* ---- reconstruction core --------------------------------------------- */

/* Copy src RT into dst RT — one opaque full-surface sprite draw,
 * executed immediately on `cmd`. */
static void rm_blit_scaled(AeronCommandBuffer* cmd, AeronRenderTarget* dst, int dst_width, int dst_height,
						   AeronRenderTarget* src, AeronBlit2DFilter filter,
						   AeronDrawList2DClearMode clear_mode) {
	AeronDrawList2DSprite s = { 0 };
	s.texture = Aeron_RenderTargetGetTexture(src);
	s.filter = filter;
	s.blend = AERON_BLIT2D_BLEND_NONE;
	s.tint[0] = s.tint[1] = s.tint[2] = s.tint[3] = 1.0f;
	s.src_u1 = 1.0f;
	s.src_v1 = 1.0f;
	s.dst_w = (float)dst_width;
	s.dst_h = (float)dst_height;
	AeronDrawList_Begin(g.list, dst, dst_width, dst_height, clear_mode, NULL);
	AeronDrawList_AddSprite(g.list, &s);
	AeronDrawList_Render(g.list, cmd);
}

static void rm_blit_rt(AeronCommandBuffer* cmd, AeronRenderTarget* dst, AeronRenderTarget* src) {
	rm_blit_scaled(cmd, dst, g.rt_width, g.rt_height, src, AERON_BLIT2D_FILTER_NEAREST,
				   AERON_DRAWLIST2D_LOAD);
}

/* Full-surface engine colorfill. AddFill, not a pass clear value: the
 * fill shader applies the sRGB->linear conversion a clear would skip. */
static void rm_surface_clear(AeronCommandBuffer* cmd, AeronRenderTarget* rt, uint32_t engine_color) {
	float rgba[4];
	rm_color_linear(engine_color, rgba);
	rgba[3] = 1.0f;
	AeronDrawList_Begin(g.list, rt, g.rt_width, g.rt_height, AERON_DRAWLIST2D_LOAD, NULL);
	AeronDrawList_AddFill(g.list, 0.0f, 0.0f, (float)g.rt_width, (float)g.rt_height, rgba,
						  AERON_BLIT2D_BLEND_NONE, NULL);
	AeronDrawList_Render(g.list, cmd);
}

/* EXTERNAL_COMPOSITE_REVEAL: copy the external RT's texels (PMA — only
 * drawn content shows, mirroring the classic color-keyed scratch copy)
 * over the main surface in the L-shaped remainder OUTSIDE the reveal
 * rect: a right band (cols >= aux0) and a bottom band (rows >= aux1
 * within the revealed cols). Persistence follows MAIN record rules. */
static void rm_external_composite(AeronCommandBuffer* cmd, const XwaSurfaceEvent* e, int restore_enabled) {
	const int l = e->left, t = e->top, r = e->right, b = e->bottom;
	const int reveal_w = e->aux0 > 0 ? e->aux0 : 0;
	const int reveal_h = e->aux1 > 0 ? e->aux1 : 0;

	AeronRectI bands[2];
	int band_count = 0;
	if (l + reveal_w <= r) { /* right band, full height */
		bands[band_count++] =
			(AeronRectI) { rm_x_edge(l + reveal_w), rm_y_edge(t), rm_x_edge(r + 1) - rm_x_edge(l + reveal_w),
						   rm_y_edge(b + 1) - rm_y_edge(t) };
	}
	if (t + reveal_h <= b && reveal_w > 0) { /* bottom band under the revealed cols */
		bands[band_count++] =
			(AeronRectI) { rm_x_edge(l), rm_y_edge(t + reveal_h), rm_x_edge(l + reveal_w) - rm_x_edge(l),
						   rm_y_edge(b + 1) - rm_y_edge(t + reveal_h) };
	}
	if (!band_count) {
		return;
	}

	AeronDrawList2DSprite s = { 0 };
	s.texture = Aeron_RenderTargetGetTexture(g.external_rt);
	s.filter = AERON_BLIT2D_FILTER_NEAREST;
	s.blend = AERON_BLIT2D_BLEND_PMA;
	s.src_u1 = 1.0f;
	s.src_v1 = 1.0f;
	s.dst_w = (float)g.rt_width;
	s.dst_h = (float)g.rt_height;
	s.tint[0] = s.tint[1] = s.tint[2] = s.tint[3] = 1.0f;

	AeronRenderTarget* targets[2] = { g.output_rt, restore_enabled ? NULL : g.screen_rt };
	for (int ti = 0; ti < 2; ti++) {
		if (!targets[ti]) {
			continue;
		}
		AeronDrawList_Begin(g.list, targets[ti], g.rt_width, g.rt_height, AERON_DRAWLIST2D_LOAD, NULL);
		for (int i = 0; i < band_count; i++) {
			s.scissor = bands[i];
			AeronDrawList_AddSprite(g.list, &s);
		}
		AeronDrawList_Render(g.list, cmd);
	}
}

static void rm_push_save(AeronCommandBuffer* cmd, const XwaSurfaceEvent* e, int restore_enabled) {
	if (g.save_depth >= RM_SAVE_STACK_CAP) {
		if (!g.warned_save_stack) {
			g.warned_save_stack = 1;
			Aeron_LogError("xwa.remaster", "screen save stack overflow");
		}
		return;
	}
	const int w = rm_x_edge(e->right + 1) - rm_x_edge(e->left);
	const int h = rm_y_edge(e->bottom + 1) - rm_y_edge(e->top);
	if (w <= 0 || h <= 0) {
		return;
	}
	RmSaveSlot* slot = &g.save_stack[g.save_depth];
	slot->rt =
		Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) { .width = w,
															.height = h,
															.format = AERON_TEXTURE_FORMAT_RGBA8_SRGB,
															.debug_name = "xwa.frontend.saved_region" });
	slot->left = e->left;
	slot->top = e->top;
	slot->right = e->right;
	slot->bottom = e->bottom;
	if (!slot->rt) {
		return;
	}
	/* The engine saves from the persistent surface when restore mode is
	 * on, else from the live back buffer. */
	AeronRenderTarget* src = restore_enabled ? g.screen_rt : g.output_rt;
	AeronDrawList2DSprite s = { 0 };
	s.texture = Aeron_RenderTargetGetTexture(src);
	s.filter = AERON_BLIT2D_FILTER_NEAREST;
	s.blend = AERON_BLIT2D_BLEND_NONE;
	s.tint[0] = s.tint[1] = s.tint[2] = s.tint[3] = 1.0f;
	s.src_u0 = (float)rm_x_edge(e->left) / (float)g.rt_width;
	s.src_v0 = (float)rm_y_edge(e->top) / (float)g.rt_height;
	s.src_u1 = (float)rm_x_edge(e->right + 1) / (float)g.rt_width;
	s.src_v1 = (float)rm_y_edge(e->bottom + 1) / (float)g.rt_height;
	s.dst_w = (float)w;
	s.dst_h = (float)h;
	AeronDrawList_Begin(g.list, slot->rt, w, h, AERON_DRAWLIST2D_CLEAR, NULL);
	AeronDrawList_AddSprite(g.list, &s);
	AeronDrawList_Render(g.list, cmd);
	g.save_depth++;
}

static void rm_pop_restore(AeronCommandBuffer* cmd, const XwaSurfaceEvent* e) {
	if (g.save_depth <= 0) {
		return;
	}
	RmSaveSlot* slot = &g.save_stack[--g.save_depth];
	if (!slot->rt) {
		return;
	}
	AeronDrawList2DSprite s = { 0 };
	s.texture = Aeron_RenderTargetGetTexture(slot->rt);
	s.filter = AERON_BLIT2D_FILTER_NEAREST;
	s.blend = AERON_BLIT2D_BLEND_NONE;
	s.tint[0] = s.tint[1] = s.tint[2] = s.tint[3] = 1.0f;
	s.src_u1 = 1.0f;
	s.src_v1 = 1.0f;
	s.dst_x = (float)rm_x_edge(e->left);
	s.dst_y = (float)rm_y_edge(e->top);
	s.dst_w = (float)(rm_x_edge(e->right + 1) - rm_x_edge(e->left));
	s.dst_h = (float)(rm_y_edge(e->bottom + 1) - rm_y_edge(e->top));
	/* Visible now AND persistent (the engine restores into the
	 * offscreen surface when restore mode is on). */
	AeronDrawList_Begin(g.list, g.screen_rt, g.rt_width, g.rt_height, AERON_DRAWLIST2D_LOAD, NULL);
	AeronDrawList_AddSprite(g.list, &s);
	AeronDrawList_Render(g.list, cmd);
	AeronDrawList_Begin(g.list, g.output_rt, g.rt_width, g.rt_height, AERON_DRAWLIST2D_LOAD, NULL);
	AeronDrawList_AddSprite(g.list, &s);
	AeronDrawList_Render(g.list, cmd);
	Aeron_DestroyRenderTarget(slot->rt);
	slot->rt = NULL;
}

/* Replay one tick: records + surface events in cross-channel z order.
 * Record runs batch into the drawlist per destination; events cut the
 * batch and execute RT copies in between. */
/* Model-preview PiP passes own render passes and must precede the
 * reconstruction's draw-list passes on this command buffer. Their meshes and
 * LightingEffects page already follow the classic OPT/model-texture sets. */
static void rm_prepare_model_previews(AeronCommandBuffer* cmd, const XwaSnapshot* snap) {
	for (uint32_t i = 0; i < snap->model_preview_count && i < XWA_SNAP_MAX_MODEL_PREVIEWS; i++) {
		const XwaModelPreview* p = &snap->model_previews[i];
		g.preview_tex[i] = NULL;
		if (p->target != XWA_EMIT_TARGET_EXTERNAL && !p->wireframe) {
			XwaAssetRef glow_ref;
			const XwaAssetRef* glow_tex = NULL;
			if (XwaRemasterAssets_FlightAtlasFrame(g.assets, 1000, 0, &glow_ref)) {
				glow_tex = &glow_ref;
			}
			g.preview_tex[i] =
				XwaRemasterPreview_Render(cmd, p, (int)i, snap->dir_lights, snap->dir_light_count, glow_tex);
		}
	}
}

static void rm_reconstruct(AeronCommandBuffer* cmd, const XwaSnapshot* snap) {
	rm_prepare_model_previews(cmd, snap);
	if (g.screen_needs_clear) {
		/* Opaque black, not transparent: an unreconstructed region must
		 * read as a visible gap, not as the classic layer bleeding
		 * through from beneath the overlay (which disguised missing
		 * coverage as a ghostly classic cursor/background mix). */
		static const float rm_black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		g.screen_needs_clear = 0;
		AeronDrawList_Begin(g.list, g.screen_rt, g.rt_width, g.rt_height, AERON_DRAWLIST2D_CLEAR, rm_black);
		AeronDrawList_Render(g.list, cmd);
		while (g.save_depth > 0) {
			RmSaveSlot* slot = &g.save_stack[--g.save_depth];
			if (slot->rt) {
				Aeron_DestroyRenderTarget(slot->rt);
				slot->rt = NULL;
			}
		}
	}

	/* Frame starts from the persistent screen. */
	rm_blit_rt(cmd, g.output_rt, g.screen_rt);

	const int restore_on = snap->offscreen_restore_enabled != 0;

	uint32_t di = 0, pi = 0, gi = 0, ei = 0, vi = 0;
	int seg_open = 0;     /* g.list currently Begin'd on output_rt */
	int present_seen = 0; /* PRESENT event passed — restores after
						   * it clean the back buffer for the NEXT
						   * frame; the tick-start output<-screen
						   * copy already models that. */

	const uint32_t preview_count = snap->model_preview_count < XWA_SNAP_MAX_MODEL_PREVIEWS
									   ? snap->model_preview_count
									   : XWA_SNAP_MAX_MODEL_PREVIEWS;

	for (;;) {
		const uint32_t dz = di < snap->draw_2d_count ? snap->draws_2d[di].z_order : UINT32_MAX;
		const uint32_t pz = pi < snap->paint_cmd_count ? snap->paint_cmds[pi].z_order : UINT32_MAX;
		const uint32_t gz = gi < snap->glyph_count ? snap->glyphs[gi].z_order : UINT32_MAX;
		const uint32_t ez = ei < snap->surface_event_count ? snap->surface_events[ei].z_order : UINT32_MAX;
		const uint32_t vz = vi < preview_count ? snap->model_previews[vi].z_order : UINT32_MAX;
		if (dz == UINT32_MAX && pz == UINT32_MAX && gz == UINT32_MAX && ez == UINT32_MAX &&
			vz == UINT32_MAX) {
			break;
		}

		/* Model-preview PiP next? Composite the pre-rendered slot. */
		if (vz <= dz && vz <= pz && vz <= gz && vz <= ez) {
			const XwaModelPreview* p = &snap->model_previews[vi];
			AeronTexture* tex = g.preview_tex[vi];
			vi++;
			if (!tex || p->target == XWA_EMIT_TARGET_EXTERNAL) {
				continue; /* no cooked model / brief-map scratch: classic covers it */
			}
			if (seg_open != 1) {
				if (seg_open) {
					AeronDrawList_Render(g.list, cmd);
				}
				AeronDrawList_Begin(g.list, g.output_rt, g.rt_width, g.rt_height, AERON_DRAWLIST2D_LOAD,
									NULL);
				seg_open = 1;
			}
			AeronDrawList2DSprite ps = { 0 };
			ps.texture = tex;
			ps.filter = AERON_BLIT2D_FILTER_LINEAR;
			ps.blend = AERON_BLIT2D_BLEND_PMA;
			ps.src_u1 = 1.0f;
			ps.src_v1 = 1.0f;
			ps.dst_x = (float)p->dst_x * rm_scale_x();
			ps.dst_y = (float)p->dst_y * rm_scale_y();
			ps.dst_w = (float)p->dst_w * rm_scale_x();
			ps.dst_h = (float)p->dst_h * rm_scale_y();
			ps.tint[0] = ps.tint[1] = ps.tint[2] = ps.tint[3] = 1.0f;
			AeronDrawList_AddSprite(g.list, &ps);
			const int pv_to_screen = (p->target == XWA_EMIT_TARGET_OFFSCREEN) || !restore_on;
			if (pv_to_screen) {
				AeronDrawList_Render(g.list, cmd);
				AeronDrawList_Begin(g.list, g.screen_rt, g.rt_width, g.rt_height, AERON_DRAWLIST2D_LOAD,
									NULL);
				AeronDrawList_AddSprite(g.list, &ps);
				AeronDrawList_Render(g.list, cmd);
				seg_open = 0;
			}
			continue;
		}

		/* Surface event next? Flush the open batch, run the copy. */
		if (ez <= dz && ez <= pz && ez <= gz) {
			const XwaSurfaceEvent* e = &snap->surface_events[ei++];
			if (seg_open) {
				AeronDrawList_Render(g.list, cmd);
				seg_open = 0;
			}
			switch ((XwaSurfaceEventKind)e->kind) {
				case XWA_SURFACE_EVENT_PRESENT:
					present_seen = 1;
					break;
				case XWA_SURFACE_EVENT_OFFSCREEN_RESTORE:
					if (!present_seen) { /* mid-tick restore-then-redraw */
						rm_blit_rt(cmd, g.output_rt, g.screen_rt);
					}
					break;
				case XWA_SURFACE_EVENT_BACKBUFFER_SAVE:
					rm_blit_rt(cmd, g.screen_rt, g.output_rt);
					break;
				case XWA_SURFACE_EVENT_SCREEN_PUSH_SAVE:
					rm_push_save(cmd, e, restore_on);
					break;
				case XWA_SURFACE_EVENT_SCREEN_POP_RESTORE:
					rm_pop_restore(cmd, e);
					break;
				case XWA_SURFACE_EVENT_EXTERNAL_CLEAR:
					/* Classic scratch wipe before the wireframe render. */
					AeronDrawList_Begin(g.list, g.external_rt, g.rt_width, g.rt_height,
										AERON_DRAWLIST2D_CLEAR, NULL);
					AeronDrawList_Render(g.list, cmd);
					break;
				case XWA_SURFACE_EVENT_EXTERNAL_COMPOSITE_REVEAL:
					/* External (wireframe) texels over the main surface in
					 * the L-shaped remainder OUTSIDE the aux0 x aux1 reveal
					 * rect: a right band (cols >= reveal w) and a bottom
					 * band (rows >= reveal h, within the revealed cols). */
					rm_external_composite(cmd, e, restore_on);
					break;
				case XWA_SURFACE_EVENT_BACKBUFFER_CLEAR:
					/* Post-present clears clean the NEXT frame's buffer —
					 * the tick-start rebuild models that. Restore off:
					 * the back buffer is the persistent surface, so the
					 * clear persists (MAIN record rule). */
					if (!present_seen) {
						rm_surface_clear(cmd, g.output_rt, (uint32_t)(uint16_t)e->aux0);
					}
					if (!restore_on) {
						rm_surface_clear(cmd, g.screen_rt, (uint32_t)(uint16_t)e->aux0);
					}
					break;
				case XWA_SURFACE_EVENT_OFFSCREEN_CLEAR:
					/* Persistent surface only — the visible frame changes
					 * at the next restore, like the engine. */
					rm_surface_clear(cmd, g.screen_rt, (uint32_t)(uint16_t)e->aux0);
					break;
			}
			continue;
		}

		/* Record next. */
		uint8_t target;
		if (dz <= pz && dz <= gz) {
			target = snap->draws_2d[di].target;
		} else if (pz <= gz) {
			target = snap->paint_cmds[pi].target;
		} else {
			target = snap->glyphs[gi].target;
		}
		if (target == XWA_EMIT_TARGET_EXTERNAL) {
			/* External (scratch-surface) record: draws into the
			 * persistent external RT (consumed by the COMPOSITE
			 * events; e.g. the briefing wireframe hologram). */
			if (seg_open != 2) {
				if (seg_open) {
					AeronDrawList_Render(g.list, cmd);
				}
				AeronDrawList_Begin(g.list, g.external_rt, g.rt_width, g.rt_height, AERON_DRAWLIST2D_LOAD,
									NULL);
				seg_open = 2;
			}
			if (dz <= pz && dz <= gz) {
				rm_add_draw(g.list, &snap->draws_2d[di++]);
			} else if (pz <= gz) {
				rm_add_paint(g.list, &snap->paint_cmds[pi++]);
			} else {
				rm_add_glyph(g.list, &snap->glyphs[gi++]);
			}
			continue;
		}

		/* OFFSCREEN records persist. MAIN records persist only when the
		 * per-present restore is off (the back buffer then IS the
		 * persistent surface). Every record draws into output (visible
		 * this tick); persistent ones also draw into screen_rt. */
		const int to_screen = (target == XWA_EMIT_TARGET_OFFSCREEN) || !restore_on;

		if (seg_open != 1) {
			if (seg_open) {
				AeronDrawList_Render(g.list, cmd);
			}
			AeronDrawList_Begin(g.list, g.output_rt, g.rt_width, g.rt_height, AERON_DRAWLIST2D_LOAD, NULL);
			seg_open = 1;
		}
		const uint32_t rec_d = di, rec_p = pi, rec_g = gi;
		if (dz <= pz && dz <= gz) {
			rm_add_draw(g.list, &snap->draws_2d[di]);
		} else if (pz <= gz) {
			rm_add_paint(g.list, &snap->paint_cmds[pi]);
		} else {
			rm_add_glyph(g.list, &snap->glyphs[gi]);
		}

		if (to_screen) {
			/* Same record into the persistent RT (separate pass; order
			 * within screen_rt matches record order). */
			AeronDrawList_Render(g.list, cmd);
			AeronDrawList_Begin(g.list, g.screen_rt, g.rt_width, g.rt_height, AERON_DRAWLIST2D_LOAD, NULL);
			if (dz <= pz && dz <= gz) {
				rm_add_draw(g.list, &snap->draws_2d[rec_d]);
			} else if (pz <= gz) {
				rm_add_paint(g.list, &snap->paint_cmds[rec_p]);
			} else {
				rm_add_glyph(g.list, &snap->glyphs[rec_g]);
			}
			AeronDrawList_Render(g.list, cmd);
			seg_open = 0;
		}

		if (dz <= pz && dz <= gz) {
			di++;
		} else if (pz <= gz) {
			pi++;
		} else {
			gi++;
		}
	}
	if (seg_open) {
		AeronDrawList_Render(g.list, cmd);
	}
}

/* ---- per-frame -------------------------------------------------------- */

static AeronRenderTarget* rm_create_target(int width, int height, const char* debug_name) {
	return Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) { .width = width,
															   .height = height,
															   .format = AERON_TEXTURE_FORMAT_RGBA8_SRGB,
															   .debug_name = debug_name });
}

static void rm_release_retired_targets(void) {
	for (int i = 0; i < g.retired_target_count; i++) {
		Aeron_DestroyRenderTarget(g.retired_targets[i]);
		g.retired_targets[i] = NULL;
	}
	g.retired_target_count = 0;
}

static void rm_retire_target(AeronRenderTarget* target) {
	if (!target) {
		return;
	}
	if (g.retired_target_count < RM_RETIRED_TARGET_CAP) {
		g.retired_targets[g.retired_target_count++] = target;
		return;
	}
	/* The fixed-capacity set holds every target a single resize can replace. */
	Aeron_DestroyRenderTarget(target);
}

static int rm_resize_targets(AeronCommandBuffer* cmd, int width, int height) {
	AeronRenderTarget* new_screen;
	AeronRenderTarget* new_output;
	AeronRenderTarget* new_external;
	AeronRenderTarget* new_saves[RM_SAVE_STACK_CAP] = { 0 };
	const int had_targets = g.screen_rt != NULL;

	new_screen = rm_create_target(width, height, "xwa.frontend.screen");
	new_output = rm_create_target(width, height, "xwa.frontend.output");
	new_external = rm_create_target(width, height, "xwa.frontend.external");
	for (int i = 0; i < g.save_depth; i++) {
		const RmSaveSlot* slot = &g.save_stack[i];
		const int save_width = rm_edge_for(slot->right + 1, width, XWA_CLASSIC_WIDTH) -
							   rm_edge_for(slot->left, width, XWA_CLASSIC_WIDTH);
		const int save_height = rm_edge_for(slot->bottom + 1, height, XWA_CLASSIC_HEIGHT) -
								rm_edge_for(slot->top, height, XWA_CLASSIC_HEIGHT);
		if (slot->rt && save_width > 0 && save_height > 0) {
			new_saves[i] = rm_create_target(save_width, save_height, "xwa.frontend.saved_region");
		}
	}

	int complete = new_screen != NULL && new_output != NULL && new_external != NULL;
	for (int i = 0; complete && i < g.save_depth; i++) {
		if (g.save_stack[i].rt && !new_saves[i]) {
			complete = 0;
		}
	}
	if (!complete) {
		Aeron_DestroyRenderTarget(new_screen);
		Aeron_DestroyRenderTarget(new_output);
		Aeron_DestroyRenderTarget(new_external);
		for (int i = 0; i < g.save_depth; i++) {
			Aeron_DestroyRenderTarget(new_saves[i]);
		}
		Aeron_LogError("xwa.remaster", "frontend target resize failed at %dx%d", width, height);
		return 0;
	}

	if (had_targets) {
		/* The snapshot contains only this tick's incremental operations. Preserve
		 * the authoritative persistent pixels before replaying those operations. */
		rm_blit_scaled(cmd, new_screen, width, height, g.screen_rt, AERON_BLIT2D_FILTER_LINEAR,
					   AERON_DRAWLIST2D_CLEAR);
		rm_blit_scaled(cmd, new_external, width, height, g.external_rt, AERON_BLIT2D_FILTER_LINEAR,
					   AERON_DRAWLIST2D_CLEAR);
		for (int i = 0; i < g.save_depth; i++) {
			if (g.save_stack[i].rt) {
				const int save_width = Aeron_TextureGetWidth(Aeron_RenderTargetGetTexture(new_saves[i]));
				const int save_height = Aeron_TextureGetHeight(Aeron_RenderTargetGetTexture(new_saves[i]));
				rm_blit_scaled(cmd, new_saves[i], save_width, save_height, g.save_stack[i].rt,
							   AERON_BLIT2D_FILTER_LINEAR, AERON_DRAWLIST2D_CLEAR);
			}
		}
	}

	rm_retire_target(g.screen_rt);
	rm_retire_target(g.output_rt);
	rm_retire_target(g.external_rt);
	g.screen_rt = new_screen;
	g.output_rt = new_output;
	g.external_rt = new_external;
	for (int i = 0; i < g.save_depth; i++) {
		rm_retire_target(g.save_stack[i].rt);
		g.save_stack[i].rt = new_saves[i];
	}
	g.rt_width = width;
	g.rt_height = height;
	if (!had_targets) {
		g.screen_needs_clear = 1;
	}
	Aeron_LogInfo("xwa.remaster", "frontend render size: %dx%d", width, height);
	return 1;
}

static int rm_ensure(AeronCommandBuffer* cmd, int width, int height) {
	if (!cmd || width <= 0 || height <= 0) {
		return 0;
	}
	/* Sized for the briefing ship-inspect hologram: one quad record per
	 * classic wireframe line, 20k+ per tick for large exteriors. */
	if (!g.list) {
		g.list = AeronDrawList_Create(32768);
		if (!g.list) {
			Aeron_LogError("xwa.remaster", "frontend draw-list initialization failed");
			return 0;
		}
	}
	rm_release_retired_targets();
	if ((!g.screen_rt || !g.output_rt || !g.external_rt || g.rt_width != width || g.rt_height != height) &&
		!rm_resize_targets(cmd, width, height)) {
		return 0;
	}
	return 1;
}

void XwaRemasterFrontend_Shutdown(void) {
	while (g.save_depth > 0) {
		RmSaveSlot* slot = &g.save_stack[--g.save_depth];
		if (slot->rt) {
			Aeron_DestroyRenderTarget(slot->rt);
		}
	}
	if (g.list) {
		AeronDrawList_Destroy(g.list);
	}
	if (g.screen_rt) {
		Aeron_DestroyRenderTarget(g.screen_rt);
	}
	if (g.output_rt) {
		Aeron_DestroyRenderTarget(g.output_rt);
	}
	if (g.external_rt) {
		Aeron_DestroyRenderTarget(g.external_rt);
	}
	rm_release_retired_targets();
	XwaRemasterPreview_Shutdown();
	memset(&g, 0, sizeof g);
}

AeronTexture* XwaRemasterFrontend_Render(AeronCommandBuffer* cmd, const XwaSnapshot* snap,
										 XwaRemasterAssets* assets, int target_width, int target_height) {
	if (!cmd || !snap || !rm_ensure(cmd, target_width, target_height)) {
		return NULL;
	}
	g.assets = assets; /* borrowed for the call (rm_* helpers read it) */
	if (snap->scene_kind != g.last_scene) {
		g.last_scene = snap->scene_kind;
		g.screen_needs_clear = 1;
	}
	/* Resolver coverage heartbeat (~ every 10 s at 30 Hz). */
	if (++g.stat_ticks >= 300) {
		Aeron_LogDebug("xwa.remaster",
					   "sprite resolve: %u hit, %u miss (last %u ticks; e.g. %s | %s | %s | %s)", g.stat_hits,
					   g.stat_misses, g.stat_ticks, g.stat_miss_key_count > 0 ? g.stat_miss_keys[0] : "-",
					   g.stat_miss_key_count > 1 ? g.stat_miss_keys[1] : "-",
					   g.stat_miss_key_count > 2 ? g.stat_miss_keys[2] : "-",
					   g.stat_miss_key_count > 3 ? g.stat_miss_keys[3] : "-");
		g.stat_ticks = g.stat_hits = g.stat_misses = 0;
		g.stat_miss_key_count = 0;
	}
	rm_reconstruct(cmd, snap);
	return Aeron_RenderTargetGetTexture(g.output_rt);
}
