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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 4.5x the classic 640x480 frame = 2880x2160: the scale at which the
 * 4:3 frontend exactly fills a 4K-UHD display's height, and the scale
 * the sprite bake is authored at (atlas_svga_to_4k) — assets land 1:1.
 * Fractional, so integer rects must go through rm_edge(). */
#define RM_SCALE 4.5f
#define RM_RT_W 2880
#define RM_RT_H 2160
#define RM_SAVE_STACK_CAP 8

typedef struct RmSaveSlot {
	AeronRenderTarget* rt;
	int16_t left, top, right, bottom;
} RmSaveSlot;

typedef struct RmState {
	int initialized;
	AeronRenderTarget* screen_rt;
	AeronRenderTarget* output_rt;
	/* External (scratch-surface) mirror: EXTERNAL-tagged records draw
	 * here; COMPOSITE surface events copy from it (briefing wireframe
	 * hologram). Persistent across ticks like the classic scratch. */
	AeronRenderTarget* external_rt;
	AeronDrawList2D* list;
	XwaSceneKind last_scene;
	int screen_needs_clear;
	RmSaveSlot save_stack[RM_SAVE_STACK_CAP];
	int save_depth;
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
 * declared LINEAR_SRGB so present encodes exactly once. Solid engine
 * colors are sRGB-encoded and must be converted before entering the
 * shader domain. */
/* Engine 16bpp color -> linear RGB floats (alpha separate). */
static void rm_color_linear(uint32_t engine_color, float out_rgb[3]) {
	uint8_t rgba[4];
	XwaSnapshotExport_ColorToRgba(engine_color, rgba);
	out_rgb[0] = XwaRemaster_SrgbToLinear((float)rgba[0] / 255.0f);
	out_rgb[1] = XwaRemaster_SrgbToLinear((float)rgba[1] / 255.0f);
	out_rgb[2] = XwaRemaster_SrgbToLinear((float)rgba[2] / 255.0f);
}

/* Classic-pixel EDGE -> RT-pixel edge. All integer rects (scissors,
 * reveal bands, save regions) round edges through this one function so
 * adjacent regions computed from the same classic edge share the same
 * RT pixel column — no seams or overlaps at the fractional scale. */
static int rm_edge(int c) { return (int)lroundf((float)c * RM_SCALE); }

/* Inclusive 640x480 clip rect -> RT-pixel scissor; full-screen -> none. */
static AeronRectI rm_scissor(int16_t l, int16_t t, int16_t r, int16_t b) {
	AeronRectI rect = { 0, 0, 0, 0 };
	if (l <= 0 && t <= 0 && r >= 639 && b >= 479) {
		return rect; /* zero size = no scissor */
	}
	rect.x = rm_edge(l);
	rect.y = rm_edge(t);
	rect.width = rm_edge(r + 1) - rm_edge(l);
	rect.height = rm_edge(b + 1) - rm_edge(t);
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
	s.blend = (d->kind == XWA_DRAW2D_SPRITE_OPAQUE) ? AERON_BLIT2D_BLEND_NONE : AERON_BLIT2D_BLEND_PMA;
	if (d->kind == XWA_DRAW2D_SPRITE_OPAQUE) {
		/* The engine's opaque blit copies EVERY pixel (no color key);
		 * keyed-out texels in the baked asset are PMA black with
		 * alpha 0 — force alpha 1 (shader bias.a) so the classic layer
		 * cannot bleed through opaque-blitted content. */
		s.bias[3] = 1.0f;
	}
	s.scissor = rm_scissor(d->clip_left, d->clip_top, d->clip_right, d->clip_bottom);

	/* Geometry authority is the record's CLASSIC dims: dst extents in
	 * classic px x RM_SCALE, src rect mapped proportionally inside the
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
	/* The classic *BlendMode blits' `blendMode` is an ORIENTATION
	 * switch, not a blend (FrontImage_BlitRectBlendMode and its tinted
	 * twin share the walks): 1 / 3 are the two 90-degree rotated blits
	 * (briefing-map ship icons face along their heading), 2 is a
	 * vertical flip, anything else the upright blit. Rotations need
	 * per-corner UVs — a Quad4 with the dst extents transposed,
	 * corners mapped per the classic source walks (mode 1:
	 * dst(c,r) <- src(row top+c, col right-r); mode 3:
	 * dst(c,r) <- src(row bottom-c, col left+r)). */
	const int oriented =
		d->kind == XWA_DRAW2D_SPRITE_RECT_BLEND || d->kind == XWA_DRAW2D_SPRITE_RECT_TINTED_BLEND;
	if (oriented && (d->blend_mode == 1 || d->blend_mode == 3)) {
		AeronDrawList2DQuad4 q = { 0 };
		q.texture = ref.texture;
		q.filter = AERON_BLIT2D_FILTER_LINEAR;
		q.blend = AERON_BLIT2D_BLEND_PMA;
		q.scissor = s.scissor;
		if (d->kind == XWA_DRAW2D_SPRITE_RECT_TINTED_BLEND) {
			rm_color_linear(d->tint_color, q.tint);
		} else {
			q.tint[0] = q.tint[1] = q.tint[2] = 1.0f;
		}
		q.tint[3] = 1.0f;
		const float x0 = (float)d->dst_x * RM_SCALE;
		const float y0 = (float)d->dst_y * RM_SCALE;
		const float x1 = x0 + (float)dst_ch * RM_SCALE; /* transposed dst */
		const float y1 = y0 + (float)dst_cw * RM_SCALE;
		/* corners[] = TL, TR, BL, BR {x, y, u, v} */
		q.corners[0][0] = x0;
		q.corners[0][1] = y0;
		q.corners[1][0] = x1;
		q.corners[1][1] = y0;
		q.corners[2][0] = x0;
		q.corners[2][1] = y1;
		q.corners[3][0] = x1;
		q.corners[3][1] = y1;
		if (d->blend_mode == 1) {
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
	if (oriented && d->blend_mode == 2) {
		const float t = sv0; /* vertical flip */
		sv0 = sv1;
		sv1 = t;
	}

	s.src_u0 = su0;
	s.src_v0 = sv0;
	s.src_u1 = su1;
	s.src_v1 = sv1;

	s.dst_x = (float)(d->dst_x * RM_SCALE);
	s.dst_y = (float)(d->dst_y * RM_SCALE);
	s.dst_w = (float)(dst_cw * RM_SCALE);
	s.dst_h = (float)(dst_ch * RM_SCALE);

	if (d->kind == XWA_DRAW2D_SPRITE_RECT_TINTED || d->kind == XWA_DRAW2D_SPRITE_RECT_TINTED_BLEND) {
		rm_color_linear(d->tint_color, s.tint);
		s.tint[3] = 1.0f;
	} else if (d->kind == XWA_DRAW2D_SPRITE_TRANSLUCENT) {
		/* 50/50 blend factor, not a color — stays a raw multiplier. */
		s.tint[0] = s.tint[1] = s.tint[2] = s.tint[3] = 0.5f;
	}

	AeronDrawList_AddSprite(list, &s);
}

static void rm_add_paint(AeronDrawList2D* list, const XwaPaintCmd* p) {
	float rgba[4];
	rm_color_linear(p->color, rgba);
	rgba[3] = 1.0f;
	AeronRectI sc = rm_scissor(p->clip_left, p->clip_top, p->clip_right, p->clip_bottom);
	const AeronRectI* scp = sc.width > 0 ? &sc : NULL;
	const float s = (float)RM_SCALE;

	switch ((XwaPaintKind)p->kind) {
		case XWA_PAINT_HLINE:
			AeronDrawList_AddFill(list, p->x0 * s, p->y0 * s, (p->x1 - p->x0 + 1) * s, s, rgba,
								  AERON_BLIT2D_BLEND_NONE, scp);
			break;
		case XWA_PAINT_VLINE:
			AeronDrawList_AddFill(list, p->x0 * s, p->y0 * s, s, (p->y1 - p->y0 + 1) * s, rgba,
								  AERON_BLIT2D_BLEND_NONE, scp);
			break;
		case XWA_PAINT_LINE:
		case XWA_PAINT_LINE_AA:
			/* One record per segment (rotated quad; the briefing hologram
			 * draws 20k+ per tick). Endpoints at classic texel centers,
			 * classic-px stroke. */
			AeronDrawList_AddLine(list, p->x0 * s + s * 0.5f, p->y0 * s + s * 0.5f, p->x1 * s + s * 0.5f,
								  p->y1 * s + s * 0.5f, s, rgba, AERON_BLIT2D_BLEND_NONE, scp);
			break;
		case XWA_PAINT_FILL_TRANSLUCENT: {
			float half[4] = { rgba[0] * 0.5f, rgba[1] * 0.5f, rgba[2] * 0.5f, 0.5f };
			AeronDrawList_AddFill(list, (p->x0 + p->dx) * s, (p->y0 + p->dy) * s, (p->x1 - p->x0 + 1) * s,
								  (p->y1 - p->y0 + 1) * s, half, AERON_BLIT2D_BLEND_PMA, scp);
			break;
		}
		case XWA_PAINT_PIXEL:
			AeronDrawList_AddFill(list, p->x0 * s, p->y0 * s, s, s, rgba, AERON_BLIT2D_BLEND_NONE, scp);
			break;
		case XWA_PAINT_FILL_RECT: {
			AeronDrawList_AddFill(list, (p->x0 + p->dx) * s, (p->y0 + p->dy) * s, (p->x1 - p->x0 + 1) * s,
								  (p->y1 - p->y0 + 1) * s, rgba, AERON_BLIT2D_BLEND_NONE, scp);
			break;
		}
		case XWA_PAINT_RECT_OUTLINE: {
			const float x = (p->x0 + p->dx) * s;
			const float y = (p->y0 + p->dy) * s;
			const float w = (p->x1 - p->x0 + 1) * s;
			const float h = (p->y1 - p->y0 + 1) * s;
			AeronDrawList_AddFill(list, x, y, w, s, rgba, AERON_BLIT2D_BLEND_NONE, scp);         /* top */
			AeronDrawList_AddFill(list, x, y + h - s, w, s, rgba, AERON_BLIT2D_BLEND_NONE, scp); /* bottom */
			AeronDrawList_AddFill(list, x, y, s, h, rgba, AERON_BLIT2D_BLEND_NONE, scp);         /* left */
			AeronDrawList_AddFill(list, x + w - s, y, s, h, rgba, AERON_BLIT2D_BLEND_NONE, scp); /* right */
			break;
		}
	}
}

static void rm_add_glyph(AeronDrawList2D* list, const XwaGlyph2D* gl) {
	(void)XwaRemasterText_AddFrontendGlyph(list, g.assets, gl, RM_RT_W, RM_RT_H);
}

/* ---- reconstruction core --------------------------------------------- */

/* Copy src RT into dst RT — one opaque full-surface sprite draw,
 * executed immediately on `cmd`. */
static void rm_blit_rt(AeronCommandBuffer* cmd, AeronRenderTarget* dst, AeronRenderTarget* src) {
	AeronDrawList2DSprite s = { 0 };
	s.texture = Aeron_RenderTargetGetTexture(src);
	s.filter = AERON_BLIT2D_FILTER_NEAREST;
	s.blend = AERON_BLIT2D_BLEND_NONE;
	s.src_u1 = 1.0f;
	s.src_v1 = 1.0f;
	s.dst_w = (float)RM_RT_W;
	s.dst_h = (float)RM_RT_H;
	AeronDrawList_Begin(g.list, dst, RM_RT_W, RM_RT_H, AERON_DRAWLIST2D_LOAD, NULL);
	AeronDrawList_AddSprite(g.list, &s);
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
			(AeronRectI) { rm_edge(l + reveal_w), rm_edge(t), rm_edge(r + 1) - rm_edge(l + reveal_w),
						   rm_edge(b + 1) - rm_edge(t) };
	}
	if (t + reveal_h <= b && reveal_w > 0) { /* bottom band under the revealed cols */
		bands[band_count++] =
			(AeronRectI) { rm_edge(l), rm_edge(t + reveal_h), rm_edge(l + reveal_w) - rm_edge(l),
						   rm_edge(b + 1) - rm_edge(t + reveal_h) };
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
	s.dst_w = (float)RM_RT_W;
	s.dst_h = (float)RM_RT_H;
	s.tint[0] = s.tint[1] = s.tint[2] = s.tint[3] = 1.0f;

	AeronRenderTarget* targets[2] = { g.output_rt, restore_enabled ? NULL : g.screen_rt };
	for (int ti = 0; ti < 2; ti++) {
		if (!targets[ti]) {
			continue;
		}
		AeronDrawList_Begin(g.list, targets[ti], RM_RT_W, RM_RT_H, AERON_DRAWLIST2D_LOAD, NULL);
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
	const int w = rm_edge(e->right + 1) - rm_edge(e->left);
	const int h = rm_edge(e->bottom + 1) - rm_edge(e->top);
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
	s.src_u0 = (float)rm_edge(e->left) / (float)RM_RT_W;
	s.src_v0 = (float)rm_edge(e->top) / (float)RM_RT_H;
	s.src_u1 = (float)rm_edge(e->right + 1) / (float)RM_RT_W;
	s.src_v1 = (float)rm_edge(e->bottom + 1) / (float)RM_RT_H;
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
	s.src_u1 = 1.0f;
	s.src_v1 = 1.0f;
	s.dst_x = (float)rm_edge(e->left);
	s.dst_y = (float)rm_edge(e->top);
	s.dst_w = (float)(rm_edge(e->right + 1) - rm_edge(e->left));
	s.dst_h = (float)(rm_edge(e->bottom + 1) - rm_edge(e->top));
	/* Visible now AND persistent (the engine restores into the
	 * offscreen surface when restore mode is on). */
	AeronDrawList_Begin(g.list, g.screen_rt, RM_RT_W, RM_RT_H, AERON_DRAWLIST2D_LOAD, NULL);
	AeronDrawList_AddSprite(g.list, &s);
	AeronDrawList_Render(g.list, cmd);
	AeronDrawList_Begin(g.list, g.output_rt, RM_RT_W, RM_RT_H, AERON_DRAWLIST2D_LOAD, NULL);
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
		AeronDrawList_Begin(g.list, g.screen_rt, RM_RT_W, RM_RT_H, AERON_DRAWLIST2D_CLEAR, rm_black);
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
				AeronDrawList_Begin(g.list, g.output_rt, RM_RT_W, RM_RT_H, AERON_DRAWLIST2D_LOAD, NULL);
				seg_open = 1;
			}
			AeronDrawList2DSprite ps = { 0 };
			ps.texture = tex;
			ps.filter = AERON_BLIT2D_FILTER_LINEAR;
			ps.blend = AERON_BLIT2D_BLEND_PMA;
			ps.src_u1 = 1.0f;
			ps.src_v1 = 1.0f;
			ps.dst_x = (float)(p->dst_x * RM_SCALE);
			ps.dst_y = (float)(p->dst_y * RM_SCALE);
			ps.dst_w = (float)(p->dst_w * RM_SCALE);
			ps.dst_h = (float)(p->dst_h * RM_SCALE);
			ps.tint[0] = ps.tint[1] = ps.tint[2] = ps.tint[3] = 1.0f;
			AeronDrawList_AddSprite(g.list, &ps);
			const int pv_to_screen = (p->target == XWA_EMIT_TARGET_OFFSCREEN) || !restore_on;
			if (pv_to_screen) {
				AeronDrawList_Render(g.list, cmd);
				AeronDrawList_Begin(g.list, g.screen_rt, RM_RT_W, RM_RT_H, AERON_DRAWLIST2D_LOAD, NULL);
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
					AeronDrawList_Begin(g.list, g.external_rt, RM_RT_W, RM_RT_H, AERON_DRAWLIST2D_CLEAR,
										NULL);
					AeronDrawList_Render(g.list, cmd);
					break;
				case XWA_SURFACE_EVENT_EXTERNAL_COMPOSITE_REVEAL:
					/* External (wireframe) texels over the main surface in
					 * the L-shaped remainder OUTSIDE the aux0 x aux1 reveal
					 * rect: a right band (cols >= reveal w) and a bottom
					 * band (rows >= reveal h, within the revealed cols). */
					rm_external_composite(cmd, e, restore_on);
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
				AeronDrawList_Begin(g.list, g.external_rt, RM_RT_W, RM_RT_H, AERON_DRAWLIST2D_LOAD, NULL);
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
			AeronDrawList_Begin(g.list, g.output_rt, RM_RT_W, RM_RT_H, AERON_DRAWLIST2D_LOAD, NULL);
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
			AeronDrawList_Begin(g.list, g.screen_rt, RM_RT_W, RM_RT_H, AERON_DRAWLIST2D_LOAD, NULL);
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

static int rm_ensure(void) {
	if (g.initialized) {
		return g.screen_rt != NULL && g.output_rt != NULL && g.list != NULL;
	}
	g.initialized = 1;
	/* sRGB RTs keep the compose domain linear in-shader (hardware
	 * encode on write / decode on sample) — see XwaRemaster_SrgbToLinear. */
	g.screen_rt =
		Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) { .width = RM_RT_W,
															.height = RM_RT_H,
															.format = AERON_TEXTURE_FORMAT_RGBA8_SRGB,
															.debug_name = "xwa.frontend.screen" });
	g.output_rt =
		Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) { .width = RM_RT_W,
															.height = RM_RT_H,
															.format = AERON_TEXTURE_FORMAT_RGBA8_SRGB,
															.debug_name = "xwa.frontend.output" });
	g.external_rt =
		Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) { .width = RM_RT_W,
															.height = RM_RT_H,
															.format = AERON_TEXTURE_FORMAT_RGBA8_SRGB,
															.debug_name = "xwa.frontend.external" });
	/* Sized for the briefing ship-inspect hologram: one quad record per
	 * classic wireframe line, 20k+ per tick for large exteriors. */
	g.list = AeronDrawList_Create(32768);
	g.screen_needs_clear = 1;
	if (!g.screen_rt || !g.output_rt || !g.list) {
		Aeron_LogError("xwa.remaster", "frontend overlay init failed");
	}
	return g.screen_rt != NULL && g.output_rt != NULL && g.list != NULL;
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
	XwaRemasterPreview_Shutdown();
	memset(&g, 0, sizeof g);
}

AeronTexture* XwaRemasterFrontend_Render(AeronCommandBuffer* cmd, const XwaSnapshot* snap,
										 XwaRemasterAssets* assets) {
	if (!cmd || !snap || !rm_ensure()) {
		return NULL;
	}
	g.assets = assets; /* borrowed for the call (rm_* helpers read it) */
	if (snap->scene_kind != g.last_scene) {
		g.last_scene = snap->scene_kind;
		g.screen_needs_clear = 1;
	}
	/* Resolver coverage heartbeat (~ every 10 s at 30 Hz). */
	if (++g.stat_ticks >= 300) {
		Aeron_LogDebug("xwa.remaster", "sprite resolve: %u hit, %u miss (last %u ticks; e.g. %s | %s | %s | %s)",
				  g.stat_hits, g.stat_misses, g.stat_ticks,
				  g.stat_miss_key_count > 0 ? g.stat_miss_keys[0] : "-",
				  g.stat_miss_key_count > 1 ? g.stat_miss_keys[1] : "-",
				  g.stat_miss_key_count > 2 ? g.stat_miss_keys[2] : "-",
				  g.stat_miss_key_count > 3 ? g.stat_miss_keys[3] : "-");
		g.stat_ticks = g.stat_hits = g.stat_misses = 0;
		g.stat_miss_key_count = 0;
	}
	rm_reconstruct(cmd, snap);
	return Aeron_RenderTargetGetTexture(g.output_rt);
}
