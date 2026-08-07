/*
 * XwaSnapshot storage + emit helpers — see src/xwa_runtime/snapshot/snapshot.h.
 *
 * Triple-slot rotation: one write slot, two committed slots (Current +
 * Previous) so the remaster driver can interpolate across the last two
 * ticks while the next one fills. All state is process-global and
 * single-threaded with XwaPort_Tick.
 */

#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_runtime/snapshot/snapshot_flight_map.h"
#include "xwa_runtime/snapshot/snapshot_hud.h"

#include "aeron/aeron.h"
#include "xwa/assets/model_def.h"
#include "xwa/assets/model_type.h"
#include "xwa/assets/object_type.h"
#include "xwa/assets/opt_model.h"
#include "xwa/flight/fediskio.h"
#include "xwa/flight/flight.h"
#include "xwa/flight/flight_light.h"
#include "xwa/flight/hangar.h"
#include "xwa/flight/mission/mission.h"
#include "xwa/flight/object/object.h"
#include "xwa/flight/player/player.h"
#include "xwa/flight/starfield.h"
#include "xwa/frontend/frontend_cursor.h"
#include "xwa/frontend/frontend_display.h"
#include "xwa/frontend/frontend_draw.h"
#include "xwa/frontend/frontend_image.h"
#include "xwa/frontend/frontend_mission.h"
#include "xwa/frontend/frontend_text.h"
#include "xwa/render/effects.h"
#include "xwa/render/renderer.h"
#include "xwa/util/time.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static XwaSnapshot g_slots[3];
static int g_write_slot; /* index into g_slots */
static int g_current_slot = -1;
static int g_previous_slot = -1;
static uint64_t g_tick_index;
static uint32_t g_z_counter;
static XwaEmitTarget g_emit_target = XWA_EMIT_TARGET_MAIN;
static XwaSceneKind g_scene_kind = XWA_SCENE_NONE;
static uint32_t g_hyperspace_visible_streak_count;
static uint8_t g_hyperspace_visible_streak_valid;

static void snapshot_capture_frontend_assets(XwaSnapshot* snapshot);

static double snapshot_precise_coord(const XwaPreciseWorldPoint* point, int axis) {
	return (double)point->base[axis] + (double)point->offset[axis];
}

typedef struct GlowMarkCaptureMeta {
	uint32_t generation;
	uint16_t texture_frame;
	uint8_t world_segment_mode;
	float inv_scale_u;
	float inv_scale_v;
	uint64_t mesh_mask;
} GlowMarkCaptureMeta;
static GlowMarkCaptureMeta g_glow_mark_meta[24];
static GlowMarkCaptureMeta g_blast_mark_meta[32];
static uint32_t g_glow_mark_generation;

static XwaSnapshot* wr(void) { return &g_slots[g_write_slot]; }

/* Active frontend clip rect, stamped into every 2D record so the
 * remaster driver reproduces scrolling text fields / clipped panels.
 * Reading the live state at emit time can't miss a clip writer. */
static void stamp_clip(int16_t* l, int16_t* t, int16_t* r, int16_t* b) {
	FrontendRect clip;
	FrontendDisplay_GetScreenClipRect(&clip);
	*l = (int16_t)clip.left;
	*t = (int16_t)clip.top;
	*r = (int16_t)clip.right;
	*b = (int16_t)clip.bottom;
}

void XwaSnapshot_BeginTick(void) {
	XwaSnapshot* s = wr();
	s->tick_index = g_tick_index;
	s->scene_kind = g_scene_kind;
	s->draw_2d_count = 0;
	s->paint_cmd_count = 0;
	s->glyph_count = 0;
	s->model_preview_count = 0;
	s->surface_event_count = 0;
	s->backdrop_count = 0;
	s->glow_mark_count = 0;
	s->trail_emitter_count = 0;
	s->trail_point_count = 0;
	s->particle_effect_count = 0;
	s->particle_count = 0;
	s->hyperspace_streak_count = 0;
	s->cockpit_valid = 0;
	s->flight_object_count = 0;
	memset(&s->flight_map, 0, sizeof s->flight_map);
	s->flight_camera_valid = 0;
	memset(&s->death_star_beam, 0, sizeof s->death_star_beam);
	memset(&s->hyperspace, 0, sizeof s->hyperspace);
	memset(&s->hud, 0, sizeof s->hud);
	s->dropped_records = 0;
	g_z_counter = 0;
	g_emit_target = XWA_EMIT_TARGET_MAIN;
}

void XwaSnapshot_NoteHyperspaceVisibleStreakCount(uint32_t count) {
	if (count > XWA_SNAP_MAX_HYPERSPACE_STREAKS) {
		count = XWA_SNAP_MAX_HYPERSPACE_STREAKS;
	}
	g_hyperspace_visible_streak_count = count;
	g_hyperspace_visible_streak_valid = 1;
}

/* ---- debug dump ----------------------------------------------------
 * XWA_SNAPSHOT_DUMP_TICK=<n>[,<m>...] dumps those committed ticks to
 * xwa_snapshot_<n>.txt in the working directory for cross-run
 * comparisons. Parsed once; at most 8 trigger ticks. */
static void snapshot_debug_dump(const XwaSnapshot* s) {
	char path[64];
	snprintf(path, sizeof path, "xwa_snapshot_%llu.txt", (unsigned long long)s->tick_index);
	FILE* fp = fopen(path, "w");
	if (!fp) {
		return;
	}
	fprintf(fp,
			"tick %llu scene %d draws %u paints %u glyphs %u previews %u "
			"frontend %u+%u/%llu opts %u/%llu textures %u/%llu "
			"flights %u trails %u/%u particles %u/%u dropped %u\n",
			(unsigned long long)s->tick_index, (int)s->scene_kind, s->draw_2d_count, s->paint_cmd_count,
			s->glyph_count, s->model_preview_count, s->frontend_file_count, s->frontend_group_count,
			(unsigned long long)s->frontend_asset_generation, s->opt_asset_count,
			(unsigned long long)s->opt_asset_generation, s->texture_asset_count,
			(unsigned long long)s->texture_asset_generation, s->flight_object_count, s->trail_emitter_count,
			s->trail_point_count, s->particle_effect_count, s->particle_count, s->dropped_records);
	fprintf(fp, "cursor vis %u pos %d,%d sprite '%s'\n", s->cursor_visible, s->cursor_x, s->cursor_y,
			s->cursor_sprite);
	fprintf(fp, "restore_enabled %u events %u\n", s->offscreen_restore_enabled, s->surface_event_count);
	for (uint32_t i = 0; i < s->surface_event_count; i++) {
		const XwaSurfaceEvent* e = &s->surface_events[i];
		fprintf(fp, "event z%u k%u rect %d,%d,%d,%d\n", e->z_order, e->kind, e->left, e->top, e->right,
				e->bottom);
	}
	for (uint32_t i = 0; i < s->draw_2d_count; i++) {
		const XwaDraw2D* d = &s->draws_2d[i];
		fprintf(fp,
				"draw z%u k%u t%u '%s' f%d g%d i%d src %d,%d,%d,%d dst %d,%d tint %08x "
				"opaque_fill %08x orientation %d clip %d,%d,%d,%d\n",
				d->z_order, d->kind, d->target, d->name, d->frame, d->atlas_group, d->atlas_index,
				d->src_left, d->src_top, d->src_right, d->src_bottom, d->dst_x, d->dst_y, d->tint_color,
				d->opaque_fill_color, d->orientation_mode, d->clip_left, d->clip_top, d->clip_right,
				d->clip_bottom);
	}
	for (uint32_t i = 0; i < s->paint_cmd_count; i++) {
		const XwaPaintCmd* c = &s->paint_cmds[i];
		fprintf(fp, "paint z%u k%u t%u %d,%d-%d,%d d%d,%d col %08x\n", c->z_order, c->kind, c->target, c->x0,
				c->y0, c->x1, c->y1, c->dx, c->dy, c->color);
	}
	for (uint32_t i = 0; i < s->glyph_count; i++) {
		const XwaGlyph2D* g = &s->glyphs[i];
		fprintf(fp, "glyph z%u t%u f%d '%c' %d,%d col %08x\n", g->z_order, g->target, g->font_size,
				(g->ch >= 32 && g->ch < 127) ? g->ch : '?', g->x, g->y, g->color);
	}
	for (uint32_t i = 0; i < s->model_preview_count; i++) {
		const XwaModelPreview* m = &s->model_previews[i];
		fprintf(fp, "preview z%u '%s' wf%u ypr %u,%u,%u pos %d,%d,%d dst %d,%d,%d,%d\n", m->z_order,
				m->opt_name, m->wireframe, m->yaw, m->pitch, m->roll, m->world_x, m->world_y, m->world_z,
				m->dst_x, m->dst_y, m->dst_w, m->dst_h);
	}
	for (uint32_t i = 0; i < s->flight_object_count; i++) {
		const XwaFlightObject* f = &s->flight_objects[i];
		fprintf(fp,
				"fobj sig %u slot %u type %u genus %u fg %u iff %d pos %d,%d,%d "
				"ypr %u,%u,%u spd %u\n",
				f->signature, f->slot, f->object_type, f->genus, f->fg_idx, f->iff, f->world_pos[0],
				f->world_pos[1], f->world_pos[2], f->yaw, f->pitch, f->roll, f->speed);
	}
	for (uint32_t i = 0; i < s->trail_emitter_count; i++) {
		const XwaTrailEmitter* e = &s->trail_emitters[i];
		fprintf(fp,
				"trail owner %u:%u kind %u tex %u:%u points %u+%u width %.2f bias %.3f "
				"fade %.3f,%.3f color %08x\n",
				e->owner_slot, e->owner_signature, e->trail_kind, e->texture_model_type, e->texture_frame,
				e->first_point, e->point_count, (double)e->ribbon_width, (double)e->start_alpha_bias,
				(double)e->alpha_fade_start, (double)e->alpha_fade_rate, e->argb_color);
		for (uint32_t j = 0; j < e->point_count; j++) {
			const XwaTrailPoint* p = &s->trail_points[e->first_point + j];
			fprintf(fp, "trailpt %u:%u p%u %d,%d,%d spawn %d age %.5f tv %.5f\n", e->owner_slot,
					e->owner_signature, j, p->world_pos[0], p->world_pos[1], p->world_pos[2],
					p->spawn_time_ms, (double)p->age_fade, (double)p->tex_v);
		}
	}
	for (uint32_t i = 0; i < s->particle_effect_count; i++) {
		const XwaParticleEffect* e = &s->particle_effects[i];
		fprintf(fp,
				"peffect id %u owner %u:%u src %u region %u type %u mode %u tex %u "
				"particles %u+%u light %u emitter %.3f,%.3f,%.3f flags %08x\n",
				e->stable_id, e->owner_slot, e->owner_signature, e->source_kind, e->region, e->effect_type,
				e->billboard_mode, e->texture_model_type, e->first_particle, e->particle_count,
				e->point_light, snapshot_precise_coord(&e->emitter_world_pos, 0),
				snapshot_precise_coord(&e->emitter_world_pos, 1),
				snapshot_precise_coord(&e->emitter_world_pos, 2), e->render_flags);
		for (uint32_t j = 0; j < e->particle_count; j++) {
			const XwaParticle* p = &s->particles[e->first_particle + j];
			fprintf(fp,
					"particle eid %u id %u p%u pos %.3f,%.3f,%.3f tail %.3f,%.3f,%.3f "
					"frame %u size %.6f argb %08x\n",
					e->stable_id, p->stable_id, j, snapshot_precise_coord(&p->world_pos, 0),
					snapshot_precise_coord(&p->world_pos, 1), snapshot_precise_coord(&p->world_pos, 2),
					snapshot_precise_coord(&p->tail_world_pos, 0),
					snapshot_precise_coord(&p->tail_world_pos, 1),
					snapshot_precise_coord(&p->tail_world_pos, 2), p->texture_frame, (double)p->size_scale,
					p->argb_color);
		}
	}
	if (s->flight_camera_valid) {
		const XwaFlightCamera* c = &s->flight_camera;
		fprintf(fp,
				"camera pos %d,%d,%d focus %d ypr %u,%u,%u ext %u ckpt %u hyp %u "
				"region %u map %u film %u hangar %u floor %d launchref %d gt %d\n",
				c->world_pos[0], c->world_pos[1], c->world_pos[2], c->focus_obj_idx, c->view_pitch,
				c->view_yaw, c->view_roll, c->external, c->cockpit_visible, c->hyperspace_phase, c->region,
				c->map_mode, c->film_overlay, c->in_hangar, c->hangar_floor_z, c->hangar_launch_ref_obj_idx,
				s->game_time_ms);
	}
	fprintf(fp, "hyperspace phase %u ticks %u streaks %u\n", s->hyperspace.phase,
			s->hyperspace.phase_elapsed_ticks, s->hyperspace_streak_count);
	for (uint32_t i = 0; i < s->hyperspace_streak_count; i++) {
		const XwaHyperspaceStreak* h = &s->hyperspace_streaks[i];
		fprintf(fp, "hyperstreak %u off %d,%d,%d half %d roll %u\n", i, h->offset[0], h->offset[1],
				h->offset[2], h->half_width, h->roll);
	}
	if (s->cockpit_valid) {
		const XwaCockpit* k = &s->cockpit;
		fprintf(fp,
				"cockpit '%s' ext '%s' seat %u look %u tog %u hp %.1f,%.1f,%.1f "
				"pan %.0f,%.0f,%.0f\n",
				k->model_name, k->exterior_name, k->seat, k->look_available, k->toggle_available,
				(double)k->hardpoint_world[0], (double)k->hardpoint_world[1], (double)k->hardpoint_world[2],
				(double)k->camera_pan[0], (double)k->camera_pan[1], (double)k->camera_pan[2]);
	}
	fprintf(fp,
			"hud valid %u enabled %u frame %u epoch %u scale %.3f mode %08x player %u:%u "
			"panes %u glyphs %u dropped %u scope_errors %u radar_radius %u blips %u marker %u:%u@%d,%d boxes "
			"%u\n",
			s->hud.valid, s->hud.hud_enabled, s->hud.classic_frame_valid, s->hud.classic_frame_epoch,
			(double)s->hud.classic_hud_scale, s->hud.mode_flags, s->hud.player_slot, s->hud.player_signature,
			s->hud.pane_count, s->hud.glyph_count, s->hud.glyph_dropped, s->hud.pane_scope_errors,
			s->hud.radar_classic_radius, s->hud.radar_blip_count, s->hud.radar_target_marker_visible,
			s->hud.radar_target_marker_radar, s->hud.radar_target_marker_local_x,
			s->hud.radar_target_marker_local_y, s->hud.target_box_count);
	if (s->hud.valid) {
		const XwaHudInstruments* i = &s->hud.instruments;
		fprintf(fp,
				"hud craft type %u model %u speed %u throttle %u output %u hull %d/%d "
				"shield %d,%d/%d flash %u/%u side %u features %04x/%04x systems %04x/%04x\n",
				i->player_object_type, i->player_model_index, i->speed, i->throttle_speed,
				i->engine_output_scale, i->hull_damage, i->hull_max, i->shield_front, i->shield_rear,
				i->shield_max, i->shield_damage_flash, i->hull_damage_flash, i->last_shield_damage_side,
				i->installed_features, i->active_features, i->system_flags, i->working_subsystems);
		fprintf(fp,
				"hud reticle visible %u mode %u seat %u look %d,%d hardpoints %u/%u "
				"range %u lock %u threats %u/%u/%u/%u flash %u\n",
				s->hud.reticle.visible, s->hud.reticle.weapon_mode, s->hud.reticle.seat,
				s->hud.reticle.look_yaw, s->hud.reticle.look_pitch, s->hud.reticle.laser_hardpoint_count,
				s->hud.reticle.warhead_hardpoint_count, s->hud.reticle.in_range,
				s->hud.reticle.missile_lock_state, s->hud.threats.laser, s->hud.threats.turret,
				s->hud.threats.beam, s->hud.threats.missile, s->hud.threats.flash_frame);
		fprintf(fp,
				"hud target valid %u id %u:%u component %u dist %u.%02u pct %u/%u/%u "
				"name '%s' status '%s' mfd %u pages %u,%u,%u enabled %u,%u,%u\n",
				s->hud.target.valid, s->hud.target.slot, s->hud.target.signature,
				s->hud.target.selected_component, s->hud.target.distance_whole, s->hud.target.distance_frac,
				s->hud.target.shield_pct, s->hud.target.system_pct, s->hud.target.hull_pct,
				s->hud.target.name, s->hud.target.status, s->hud.mfd_active, s->hud.mfd_page[0],
				s->hud.mfd_page[1], s->hud.mfd_page[2], s->hud.mfd_enabled[0], s->hud.mfd_enabled[1],
				s->hud.mfd_enabled[2]);
		const XwaHudCrt* crt = &s->hud.crt;
		fprintf(fp,
				"hud crt visible %u self %u map %u target %u:%u component %u marker %u "
				"exclude %u,%u viewport %ux%u projection %d,%u distance %d backstep %d,%d,%d "
				"rows %d,%d,%d/%d,%d,%d/%d,%d,%d focus %d,%d,%d\n",
				crt->visible, crt->self_view, crt->map_view, crt->target_slot, crt->target_signature,
				crt->selected_component, crt->component_marker_visible, crt->projectile_exclude_slots[0],
				crt->projectile_exclude_slots[1], crt->classic_viewport_w, crt->classic_viewport_h,
				crt->proj_scale, crt->proj_aspect_y_q16, crt->camera_distance, crt->camera_back_step[0],
				crt->camera_back_step[1], crt->camera_back_step[2], crt->camera_rows_q15[0],
				crt->camera_rows_q15[1], crt->camera_rows_q15[2], crt->camera_rows_q15[3],
				crt->camera_rows_q15[4], crt->camera_rows_q15[5], crt->camera_rows_q15[6],
				crt->camera_rows_q15[7], crt->camera_rows_q15[8], crt->component_focus[0],
				crt->component_focus[1], crt->component_focus[2]);
		for (uint16_t box_idx = 0; box_idx < s->hud.target_box_count; box_idx++) {
			const XwaHudTargetBox* box = &s->hud.target_boxes[box_idx];
			fprintf(fp, "hud box %u id %u:%u component %u color %u selected %u layer %u extent %d\n", box_idx,
					box->slot, box->signature, box->component, box->color_index, box->selected, box->layer,
					box->extent);
		}
	}
	for (uint32_t i = 0; i < s->backdrop_count; i++) {
		const XwaBackdrop* b = &s->backdrops[i];
		fprintf(fp,
				"backdrop type %u f%u side %u flags %u hid %u dir %.0f,%.0f,%.0f scale %u "
				"col %.2f,%.2f,%.2f i%.2f segs %u spf %u hh %d\n",
				b->model_type, b->frame, b->side, b->flags, b->hidden, (double)b->world_dir[0],
				(double)b->world_dir[1], (double)b->world_dir[2], b->angular_scale, (double)b->color[0],
				(double)b->color[1], (double)b->color[2], (double)b->intensity, b->strip_segment_count,
				b->strip_segments_per_frame, b->strip_half_height);
	}
	for (uint32_t i = 0; i < s->glow_mark_count; i++) {
		const XwaGlowMark* m = &s->glow_marks[i];
		fprintf(fp,
				"glow owner %u:%u pool %u:%u gen %u type %u frame %u age %u "
				"center %.2f,%.2f,%.2f inv %.6f,%.6f layer %.3f persistent %u\n",
				m->owner_slot, m->owner_signature, m->pool_kind, m->pool_index, m->generation,
				m->texture_model_type, m->texture_frame, m->age_ticks, (double)m->center[0],
				(double)m->center[1], (double)m->center[2], (double)m->inv_scale_u, (double)m->inv_scale_v,
				(double)m->layer_uv_scale, m->persistent_until_cleared);
	}
	fclose(fp);
	Aeron_LogInfo("xwa.snapshot", "dumped tick %llu to %s", (unsigned long long)s->tick_index, path);
}

static int g_dump_ticks[8];
static int g_dump_tick_count = -1; /* -1 = env not parsed yet */

static void snapshot_maybe_dump(const XwaSnapshot* s) {
	if (g_dump_tick_count < 0) {
		g_dump_tick_count = 0;
		const char* env = getenv("XWA_SNAPSHOT_DUMP_TICK");
		while (env && *env && g_dump_tick_count < 8) {
			g_dump_ticks[g_dump_tick_count++] = atoi(env);
			env = strchr(env, ',');
			if (env) {
				env++;
			}
		}
	}
	for (int i = 0; i < g_dump_tick_count; i++) {
		if ((uint64_t)g_dump_ticks[i] == s->tick_index) {
			snapshot_debug_dump(s);
		}
	}
}

void XwaSnapshot_Commit(void) {
	XwaSnapshot* s = wr();
	s->scene_kind = g_scene_kind;
	s->game_time_ms = g_gameTime; /* sim clock; see header */
	s->offscreen_restore_enabled = g_offscreenRestoreEnabled;
	snapshot_capture_frontend_assets(s);
	XwaSnapshotExport_CaptureOptAssets(s);
	XwaSnapshotExport_CaptureTextureAssets(s);
	/* Shared 3D lighting state: the FlightLight directional table
	 * (world space), read-only sample once per tick. Lights every 3D
	 * view — flight scenes and the frontend model preview alike. */
	{
		uint32_t n = (uint32_t)(g_dirLightCount < 0 ? 0 : g_dirLightCount);
		if (n > XWA_SNAP_MAX_DIR_LIGHTS) {
			n = XWA_SNAP_MAX_DIR_LIGHTS;
		}
		const int region = g_players[g_localPlayer].regionIndex;
		for (uint32_t i = 0; i < n; i++) {
			const DirectionalLight* dl = &g_directionalLights[i];
			XwaDirLight* o = &s->dir_lights[i];
			o->world_dir[0] = (float)dl->worldDirX_Q15 * (1.0f / 32768.0f);
			o->world_dir[1] = (float)dl->worldDirY_Q15 * (1.0f / 32768.0f);
			o->world_dir[2] = (float)dl->worldDirZ_Q15 * (1.0f / 32768.0f);
			o->intensity = dl->intensity;
			o->color[0] = dl->colorR;
			o->color[1] = dl->colorG;
			o->color[2] = dl->colorB;
			o->source_backdrop_model_type = 0;
			const int backdrop_idx = g_directionalLightBackdropIndices[i];
			if (region >= 0 && region < XWA_BACKDROP_REGION_COUNT && backdrop_idx >= 0 &&
				backdrop_idx < g_backdropCountByRegion[region]) {
				o->source_backdrop_model_type = g_backdropRecordsByRegion[region][backdrop_idx].modelType;
			}
		}
		s->dir_light_count = n;
	}
	/* Cursor state, sampled once per tick (read-only observers). */
	s->cursor_visible = (uint8_t)(FrontendCursor_IsVisible() != 0);
	{
		int cx = 0, cy = 0;
		FrontendCursor_GetPos(&cx, &cy);
		s->cursor_x = cx;
		s->cursor_y = cy;
	}
	{
		size_t n = 0;
		while (n < sizeof s->cursor_sprite - 1 && g_cursorSpriteName[n] != '\0') {
			s->cursor_sprite[n] = g_cursorSpriteName[n];
			n++;
		}
		s->cursor_sprite[n] = '\0';
	}
	if (s->dropped_records) {
		/* Histogram of what filled the channels — identifies the
		 * emitter flooding the caps. */
		uint32_t draw_kinds[8] = { 0 };
		uint32_t paint_kinds[8] = { 0 };
		for (uint32_t i = 0; i < s->draw_2d_count; i++) {
			draw_kinds[s->draws_2d[i].kind & 7]++;
		}
		for (uint32_t i = 0; i < s->paint_cmd_count; i++) {
			paint_kinds[s->paint_cmds[i].kind & 7]++;
		}
		Aeron_LogWarn("xwa.snapshot",
					  "tick %llu: %u 2D records dropped (caps %d/%d); kept draws "
					  "spr=%u opq=%u trl=%u rect=%u rtint=%u rblend=%u rtb=%u atlas=%u | paints "
					  "hl=%u vl=%u ln=%u aa=%u ftr=%u fill=%u out=%u px=%u | glyphs=%u",
					  (unsigned long long)s->tick_index, s->dropped_records, XWA_SNAP_MAX_DRAWS_2D,
					  XWA_SNAP_MAX_PAINT_CMDS, draw_kinds[0], draw_kinds[1], draw_kinds[2], draw_kinds[3],
					  draw_kinds[4], draw_kinds[5], draw_kinds[6], draw_kinds[7], paint_kinds[0],
					  paint_kinds[1], paint_kinds[2], paint_kinds[3], paint_kinds[4], paint_kinds[5],
					  paint_kinds[6], paint_kinds[7], s->glyph_count);
	}
	snapshot_maybe_dump(s);
	g_previous_slot = g_current_slot;
	g_current_slot = g_write_slot;
	/* Next free slot: the one that is neither current nor previous. */
	for (int i = 0; i < 3; i++) {
		if (i != g_current_slot && i != g_previous_slot) {
			g_write_slot = i;
			break;
		}
	}
	g_tick_index++;
}

const XwaSnapshot* XwaSnapshot_Current(void) { return g_current_slot >= 0 ? &g_slots[g_current_slot] : 0; }

const XwaSnapshot* XwaSnapshot_Previous(void) { return g_previous_slot >= 0 ? &g_slots[g_previous_slot] : 0; }

/* ---- emitters ------------------------------------------------------ */

void XwaSnapshot_SetSceneKind(XwaSceneKind kind) { g_scene_kind = kind; }

void XwaSnapshot_SetEmitTarget(XwaEmitTarget target) { g_emit_target = target; }

/* ---- name -> source-file bindings -----------------------------------
 * Session-persistent side table (not per-slot): registration is rare
 * (room transitions), lookups happen per sprite emit. Names are reused
 * across rooms with different files. Inactive slots are reused, keeping
 * storage bounded by the engine's simultaneous resource cap. */

#define XWA_SNAP_MAX_BINDINGS 1024 /* == FRONT_IMAGE_MAX_RESOURCES */

typedef struct XwaResourceBinding {
	char name[64];
	char file[XWA_SNAP_FRONTEND_FILE_MAX];
	char source_file[XWA_SNAP_FRONTEND_SOURCE_MAX];
	uint16_t frame_count;
	uint8_t active;
} XwaResourceBinding;

static XwaResourceBinding g_bindings[XWA_SNAP_MAX_BINDINGS];
static int g_binding_count;
static int16_t g_frontend_active_groups[XWA_SNAP_MAX_SPRITE_GROUPS];
static uint32_t g_frontend_active_group_count;

/* "frontres\\FAMILY\\FamilyRoom.BMP" -> "family/familyroom": lowercase,
 * forward slashes, extension stripped, leading "frontres/" dropped
 * (every frontend source image lives there; the resolver re-prefixes). */
static void binding_normalize_file(const char* fileName, char* out, size_t outsz) {
	char tmp[256];
	size_t n = 0;
	for (const char* p = fileName; *p && n + 1 < sizeof tmp; p++) {
		char c = *p;
		if (c == '\\') {
			c = '/';
		} else if (c >= 'A' && c <= 'Z') {
			c = (char)(c + ('a' - 'A'));
		}
		tmp[n++] = c;
	}
	tmp[n] = '\0';
	char* dot = strrchr(tmp, '.');
	if (dot && !strchr(dot, '/')) {
		*dot = '\0';
	}
	const char* src = tmp;
	if (strncmp(src, "frontres/", 9) == 0) {
		src += 9;
	}
	snprintf(out, outsz, "%s", src);
}

static void frontend_file_add(XwaFrontendFileAsset* files, uint32_t* count, const char* file,
							  const char* source_file, uint16_t frame_count) {
	for (uint32_t i = 0; i < *count; i++) {
		if (strncmp(files[i].file, file, sizeof files[i].file) == 0) {
			if (!files[i].source_file[0] && source_file) {
				snprintf(files[i].source_file, sizeof files[i].source_file, "%s", source_file);
			}
			if (files[i].frame_count < frame_count) {
				files[i].frame_count = frame_count;
			}
			return;
		}
	}
	if (*count >= XWA_SNAP_MAX_FRONTEND_FILES) {
		return;
	}
	XwaFrontendFileAsset* asset = &files[(*count)++];
	memset(asset, 0, sizeof *asset);
	snprintf(asset->file, sizeof asset->file, "%s", file);
	snprintf(asset->source_file, sizeof asset->source_file, "%s", source_file ? source_file : "");
	asset->frame_count = frame_count;
}

static int frontend_group_find(const int16_t* groups, uint32_t count, int16_t group) {
	for (uint32_t i = 0; i < count; i++) {
		if (groups[i] == group) {
			return (int)i;
		}
	}
	return -1;
}

static void frontend_group_add(int16_t* groups, uint32_t* count, int16_t group) {
	if (group < 0 || frontend_group_find(groups, *count, group) >= 0 ||
		*count >= XWA_SNAP_MAX_SPRITE_GROUPS) {
		return;
	}
	groups[(*count)++] = group;
}

static void frontend_snapshot_group_add(XwaFrontendGroupAsset* groups, uint32_t* count, int16_t group) {
	for (uint32_t i = 0; i < *count; i++) {
		if (groups[i].group == group) {
			return;
		}
	}
	if (group < 0 || *count >= XWA_SNAP_MAX_SPRITE_GROUPS) {
		return;
	}
	groups[(*count)++].group = group;
}

void XwaSnapshot_NoteResourceBinding(const char* fileName, const char* name, int frame_count) {
	if (!fileName || !name || !fileName[0] || !name[0]) {
		return;
	}
	char file[XWA_SNAP_FRONTEND_FILE_MAX];
	binding_normalize_file(fileName, file, sizeof file);
	uint16_t frames = frame_count > 0 && frame_count <= UINT16_MAX ? (uint16_t)frame_count : 1;
	int inactive_index = -1;
	for (int i = 0; i < g_binding_count; i++) {
		if (strncmp(g_bindings[i].name, name, sizeof g_bindings[i].name) == 0) {
			if (!g_bindings[i].active || strcmp(g_bindings[i].file, file) != 0 ||
				strcmp(g_bindings[i].source_file, fileName) != 0 || g_bindings[i].frame_count != frames) {
				snprintf(g_bindings[i].file, sizeof g_bindings[i].file, "%s", file);
				snprintf(g_bindings[i].source_file, sizeof g_bindings[i].source_file, "%s", fileName);
				g_bindings[i].frame_count = frames;
				g_bindings[i].active = 1;
			}
			return;
		}
		if (!g_bindings[i].active && inactive_index < 0) {
			inactive_index = i;
		}
	}
	if (inactive_index < 0 && g_binding_count >= XWA_SNAP_MAX_BINDINGS) {
		Aeron_LogWarn("xwa.snapshot", "resource-binding table full; '%s' unmapped", name);
		return;
	}
	XwaResourceBinding* b =
		inactive_index >= 0 ? &g_bindings[inactive_index] : &g_bindings[g_binding_count++];
	memset(b, 0, sizeof *b);
	snprintf(b->name, sizeof b->name, "%s", name);
	snprintf(b->file, sizeof b->file, "%s", file);
	snprintf(b->source_file, sizeof b->source_file, "%s", fileName);
	b->frame_count = frames;
	b->active = 1;
}

void XwaSnapshot_NoteResourceFree(const char* name) {
	if (!name || !name[0]) {
		return;
	}
	for (int i = 0; i < g_binding_count; i++) {
		XwaResourceBinding* b = &g_bindings[i];
		if (b->active && strncmp(b->name, name, sizeof b->name) == 0) {
			b->active = 0;
			return;
		}
	}
}

void XwaSnapshot_NoteSpriteGroupLoad(int16_t group) {
	if (group < 0 ||
		frontend_group_find(g_frontend_active_groups, g_frontend_active_group_count, group) >= 0) {
		return;
	}
	frontend_group_add(g_frontend_active_groups, &g_frontend_active_group_count, group);
}

void XwaSnapshot_NoteSpriteGroupFree(int16_t group) {
	const int index = frontend_group_find(g_frontend_active_groups, g_frontend_active_group_count, group);
	if (index < 0) {
		return;
	}
	g_frontend_active_group_count--;
	if ((uint32_t)index != g_frontend_active_group_count) {
		g_frontend_active_groups[index] = g_frontend_active_groups[g_frontend_active_group_count];
	}
}

void XwaSnapshot_NoteSpriteGroupsReset(void) {
	if (!g_frontend_active_group_count) {
		return;
	}
	g_frontend_active_group_count = 0;
}

static int frontend_asset_sets_match(const XwaSnapshot* lhs, const XwaSnapshot* rhs) {
	if (!rhs || lhs->frontend_file_count != rhs->frontend_file_count ||
		lhs->frontend_group_count != rhs->frontend_group_count) {
		return 0;
	}
	for (uint32_t i = 0; i < lhs->frontend_file_count; i++) {
		if (lhs->frontend_files[i].frame_count != rhs->frontend_files[i].frame_count ||
			strcmp(lhs->frontend_files[i].source_file, rhs->frontend_files[i].source_file) != 0 ||
			strcmp(lhs->frontend_files[i].file, rhs->frontend_files[i].file) != 0) {
			return 0;
		}
	}
	for (uint32_t i = 0; i < lhs->frontend_group_count; i++) {
		if (lhs->frontend_groups[i].group != rhs->frontend_groups[i].group) {
			return 0;
		}
	}
	return 1;
}

static void snapshot_capture_frontend_assets(XwaSnapshot* snapshot) {
	snapshot->frontend_file_count = 0;
	for (int i = 0; i < g_binding_count; i++) {
		if (g_bindings[i].active) {
			frontend_file_add(snapshot->frontend_files, &snapshot->frontend_file_count, g_bindings[i].file,
							  g_bindings[i].source_file, g_bindings[i].frame_count);
		}
	}
	for (uint32_t i = 0; i < snapshot->draw_2d_count; i++) {
		const XwaDraw2D* draw = &snapshot->draws_2d[i];
		if (draw->kind != XWA_DRAW2D_ATLAS_SPRITE && draw->file[0]) {
			frontend_file_add(snapshot->frontend_files, &snapshot->frontend_file_count, draw->file,
							  draw->source_file, draw->resource_frame_count ? draw->resource_frame_count : 1);
		}
	}
	for (uint32_t i = 1; i < snapshot->frontend_file_count; i++) {
		XwaFrontendFileAsset value = snapshot->frontend_files[i];
		uint32_t j = i;
		while (j > 0 && strcmp(snapshot->frontend_files[j - 1].file, value.file) > 0) {
			snapshot->frontend_files[j] = snapshot->frontend_files[j - 1];
			j--;
		}
		snapshot->frontend_files[j] = value;
	}

	snapshot->frontend_group_count = 0;
	for (uint32_t i = 0; i < g_frontend_active_group_count; i++) {
		frontend_snapshot_group_add(snapshot->frontend_groups, &snapshot->frontend_group_count,
									g_frontend_active_groups[i]);
	}
	for (uint32_t i = 0; i < snapshot->draw_2d_count; i++) {
		const XwaDraw2D* draw = &snapshot->draws_2d[i];
		if (draw->kind == XWA_DRAW2D_ATLAS_SPRITE) {
			frontend_snapshot_group_add(snapshot->frontend_groups, &snapshot->frontend_group_count,
										draw->atlas_group);
		}
	}
	for (uint32_t i = 1; i < snapshot->frontend_group_count; i++) {
		XwaFrontendGroupAsset value = snapshot->frontend_groups[i];
		uint32_t j = i;
		while (j > 0 && snapshot->frontend_groups[j - 1].group > value.group) {
			snapshot->frontend_groups[j] = snapshot->frontend_groups[j - 1];
			j--;
		}
		snapshot->frontend_groups[j] = value;
	}

	/* Compare with the still-current committed slot; Commit rotates slots only
	 * after capture. Transient load/free pairs with no draw leave the set and
	 * generation unchanged. */
	const XwaSnapshot* previous = XwaSnapshot_Current();
	if (frontend_asset_sets_match(snapshot, previous)) {
		snapshot->frontend_asset_generation = previous->frontend_asset_generation;
	} else if (previous) {
		snapshot->frontend_asset_generation = previous->frontend_asset_generation + 1;
	} else {
		snapshot->frontend_asset_generation =
			(snapshot->frontend_file_count || snapshot->frontend_group_count) ? 1 : 0;
	}
}

static const XwaResourceBinding* binding_lookup(const char* name) {
	for (int i = 0; i < g_binding_count; i++) {
		if (g_bindings[i].active && strncmp(g_bindings[i].name, name, sizeof g_bindings[i].name) == 0) {
			return &g_bindings[i];
		}
	}
	return NULL;
}

void XwaSnapshot_EmitSurfaceEventAux(XwaSurfaceEventKind kind, int left, int top, int right, int bottom,
									 int aux0, int aux1) {
	XwaSnapshot* s = wr();
	if (s->surface_event_count >= XWA_SNAP_MAX_SURFACE_EVENTS) {
		s->dropped_records++;
		return;
	}
	XwaSurfaceEvent* e = &s->surface_events[s->surface_event_count++];
	e->z_order = g_z_counter++;
	e->kind = (uint8_t)kind;
	e->aux0 = (int16_t)aux0;
	e->aux1 = (int16_t)aux1;
	e->left = (int16_t)left;
	e->top = (int16_t)top;
	e->right = (int16_t)right;
	e->bottom = (int16_t)bottom;
}

void XwaSnapshot_EmitSurfaceEvent(XwaSurfaceEventKind kind, int left, int top, int right, int bottom) {
	XwaSnapshot_EmitSurfaceEventAux(kind, left, top, right, bottom, 0, 0);
}

void XwaSnapshot_EmitSprite(XwaDraw2DKind kind, const char* name, int frame, const int16_t src_ltrb[4],
							int dst_x, int dst_y, int img_w, int img_h, uint32_t tint_color,
							uint32_t opaque_fill_color, int32_t orientation_mode) {
	XwaSnapshot* s = wr();
	if (s->draw_2d_count >= XWA_SNAP_MAX_DRAWS_2D) {
		s->dropped_records++;
		return;
	}
	XwaDraw2D* d = &s->draws_2d[s->draw_2d_count++];
	memset(d, 0, sizeof *d);
	d->z_order = g_z_counter++;
	d->kind = (uint8_t)kind;
	d->target = (uint8_t)g_emit_target;
	if (name) {
		size_t n = strlen(name);
		if (n >= sizeof d->name) {
			n = sizeof d->name - 1;
		}
		memcpy(d->name, name, n);
		/* Stamp the name's CURRENT file binding — resolving at emit
		 * time (not consume time) keeps same-tick rebinds correct. */
		const XwaResourceBinding* binding = binding_lookup(name);
		if (binding) {
			memcpy(d->file, binding->file, sizeof d->file);
			snprintf(d->source_file, sizeof d->source_file, "%s", binding->source_file);
			d->resource_frame_count = binding->frame_count;
		}
	}
	if (src_ltrb) {
		d->has_src_rect = 1;
		d->src_left = src_ltrb[0];
		d->src_top = src_ltrb[1];
		d->src_right = src_ltrb[2];
		d->src_bottom = src_ltrb[3];
	}
	d->img_w = (int16_t)img_w;
	d->img_h = (int16_t)img_h;
	d->frame = (int16_t)frame;
	d->dst_x = (int16_t)dst_x;
	d->dst_y = (int16_t)dst_y;
	d->tint_color = tint_color;
	d->opaque_fill_color = opaque_fill_color;
	d->orientation_mode = orientation_mode;
	stamp_clip(&d->clip_left, &d->clip_top, &d->clip_right, &d->clip_bottom);
}

void XwaSnapshot_EmitAtlasSprite(int group_id, int index, int x, int y, int img_w, int img_h) {
	XwaSnapshot* s = wr();
	if (s->draw_2d_count >= XWA_SNAP_MAX_DRAWS_2D) {
		s->dropped_records++;
		return;
	}
	XwaDraw2D* d = &s->draws_2d[s->draw_2d_count++];
	memset(d, 0, sizeof *d);
	d->z_order = g_z_counter++;
	d->kind = XWA_DRAW2D_ATLAS_SPRITE;
	d->target = (uint8_t)g_emit_target;
	d->atlas_group = (int16_t)group_id;
	d->atlas_index = (int16_t)index;
	d->img_w = (int16_t)img_w;
	d->img_h = (int16_t)img_h;
	d->dst_x = (int16_t)x;
	d->dst_y = (int16_t)y;
	stamp_clip(&d->clip_left, &d->clip_top, &d->clip_right, &d->clip_bottom);
}

void XwaSnapshot_EmitPaint(XwaPaintKind kind, int x0, int y0, int x1, int y1, int dx, int dy,
						   uint32_t color) {
	XwaSnapshot* s = wr();
	if (s->paint_cmd_count >= XWA_SNAP_MAX_PAINT_CMDS) {
		s->dropped_records++;
		return;
	}
	XwaPaintCmd* p = &s->paint_cmds[s->paint_cmd_count++];
	memset(p, 0, sizeof *p);
	p->z_order = g_z_counter++;
	p->kind = (uint8_t)kind;
	p->target = (uint8_t)g_emit_target;
	p->x0 = (int16_t)x0;
	p->y0 = (int16_t)y0;
	p->x1 = (int16_t)x1;
	p->y1 = (int16_t)y1;
	p->dx = (int16_t)dx;
	p->dy = (int16_t)dy;
	p->color = color;
	stamp_clip(&p->clip_left, &p->clip_top, &p->clip_right, &p->clip_bottom);
}

typedef struct SnapshotParticleTransform {
	Matrix3x3 particle_to_world;
	int32_t emitter_base[3];
	Vec3f emitter_offset;
} SnapshotParticleTransform;

static void snapshot_particle_point_set(XwaPreciseWorldPoint* out, const int32_t base[3],
										const float offset[3]) {
	memcpy(out->base, base, sizeof out->base);
	memcpy(out->offset, offset, sizeof out->offset);
}

static void snapshot_particle_point_set_float(XwaPreciseWorldPoint* out, float x, float y, float z) {
	memset(out->base, 0, sizeof out->base);
	out->offset[0] = x;
	out->offset[1] = y;
	out->offset[2] = z;
}

static void snapshot_particle_point_add(XwaPreciseWorldPoint* out, const XwaPreciseWorldPoint* point,
										const float offset[3]) {
	*out = *point;
	for (int i = 0; i < 3; i++)
		out->offset[i] += offset[i];
}

static int snapshot_particle_transform(const ObjectRecord* owner, const ParticleEffect* effect,
									   SnapshotParticleTransform* out) {
	if (!owner || !owner->mobj) {
		return 0;
	}
	const MobileObject* m = owner->mobj;
	const float q15 = 0.000030518509f;
	Matrix3x3 orient = {
		{ (float)m->cachedSideX * q15, (float)m->cachedSideY * q15, (float)m->cachedSideZ * q15,
		  -(float)m->cachedFwdX * q15, -(float)m->cachedFwdY * q15, -(float)m->cachedFwdZ * q15,
		  (float)m->cachedUpX * q15, (float)m->cachedUpY * q15, (float)m->cachedUpZ * q15 }
	};
	Vec3f emitter = effect->localOffset;
	if (m->spinAngleQ16 != 0) {
		Matrix3x3 spin;
		float axis_angle[4] = { m->spinAxisX, m->spinAxisY, m->spinAxisZ,
								(float)-((double)m->spinAngleQ16 * 0.00009587379924285257) };
		const Vec3f pivot = { (float)m->renderOffsetX, (float)m->renderOffsetY, (float)m->renderOffsetZ };
		emitter.x -= pivot.x;
		emitter.y -= pivot.y;
		emitter.z -= pivot.z;
		Math3D_BuildAxisAngleMatrix(&spin, axis_angle);
		Math3D_RotateVec3(&emitter, &spin);
		emitter.x += pivot.x;
		emitter.y += pivot.y;
		emitter.z += pivot.z;
		Math3D_RotateVec3(&emitter, &orient);
		Math3D_MulMatrix3x3(&orient, &spin);
	} else {
		Math3D_RotateVec3(&emitter, &orient);
	}
	out->particle_to_world = orient;
	out->emitter_base[0] = owner->world_x;
	out->emitter_base[1] = owner->world_y;
	out->emitter_base[2] = owner->world_z;
	out->emitter_offset = emitter;
	return 1;
}

static void snapshot_particle_resolve_local(const SnapshotParticleTransform* transform, const Vec3f* local,
											XwaPreciseWorldPoint* out) {
	Vec3f offset = *local;
	Math3D_RotateVec3(&offset, (Matrix3x3*)&transform->particle_to_world);
	offset.x += transform->emitter_offset.x;
	offset.y += transform->emitter_offset.y;
	offset.z += transform->emitter_offset.z;
	const float values[3] = { offset.x, offset.y, offset.z };
	snapshot_particle_point_set(out, transform->emitter_base, values);
}

static int snapshot_particle_capture_effect(XwaSnapshot* s, const ParticleEffect* effect,
											const ObjectRecord* owner, uint16_t owner_slot,
											int child_effect) {
	const int object_source = owner != NULL;
	const int contributes_light = object_source && !child_effect && effect->effectType != 1;
	if (!effect->particles && !contributes_light) {
		return 1;
	}
	if (s->particle_effect_count >= XWA_SNAP_MAX_PARTICLE_EFFECTS) {
		s->dropped_records++;
		return 0;
	}
	SnapshotParticleTransform local_transform;
	memset(&local_transform, 0, sizeof local_transform);
	if (!effect->useAttachedTransform && !snapshot_particle_transform(owner, effect, &local_transform)) {
		return 1;
	}

	XwaParticleEffect* dst = &s->particle_effects[s->particle_effect_count++];
	memset(dst, 0, sizeof *dst);
	dst->stable_id = Particle_SnapshotEffectId(effect);
	dst->owner_slot = object_source ? owner_slot : 0xffffu;
	dst->owner_signature = object_source ? owner->objectSignature : 0;
	dst->source_kind = object_source ? XWA_PARTICLE_SOURCE_OBJECT : XWA_PARTICLE_SOURCE_WORLD;
	dst->region = object_source ? owner->regionIdx : (uint8_t)g_players[g_localPlayer].regionIndex;
	dst->effect_type = (uint8_t)effect->effectType;
	dst->billboard_mode = effect->stretchedBillboard
							  ? (effect->useAttachedTransform ? XWA_PARTICLE_BILLBOARD_STRETCHED
															  : XWA_PARTICLE_BILLBOARD_STRETCHED_LOCAL)
							  : XWA_PARTICLE_BILLBOARD_FACING;
	dst->hide_owner_external = (uint8_t)(!child_effect && !effect->useAttachedTransform);
	dst->hide_owner_film = dst->hide_owner_external;
	dst->point_light = (uint8_t)contributes_light;
	dst->texture_model_type =
		effect->textureFrameCount ? effect->textureModelType : effect->def->textureModelType;
	dst->render_flags = effect->def->renderFlags;
	dst->first_particle = s->particle_count;
	if (effect->useAttachedTransform) {
		if (!Particle_SnapshotEffectPoint(effect, dst->emitter_world_pos.base,
										  dst->emitter_world_pos.offset)) {
			snapshot_particle_point_set_float(&dst->emitter_world_pos, effect->world.x, effect->world.y,
											  effect->world.z);
		}
	} else if (object_source) {
		SnapshotParticleTransform emitter_transform;
		if (snapshot_particle_transform(owner, effect, &emitter_transform)) {
			const float offset[3] = { emitter_transform.emitter_offset.x, emitter_transform.emitter_offset.y,
									  emitter_transform.emitter_offset.z };
			snapshot_particle_point_set(&dst->emitter_world_pos, emitter_transform.emitter_base, offset);
		}
	} else {
		snapshot_particle_point_set_float(&dst->emitter_world_pos, effect->world.x, effect->world.y,
										  effect->world.z);
	}

	const ParticleRecord* src = effect->particles;
	uint32_t walked = 0;
	while (src && walked++ < (uint32_t)effect->maxParticles) {
		if (s->particle_count >= XWA_SNAP_MAX_PARTICLES) {
			s->dropped_records++;
			break;
		}
		XwaParticle* p = &s->particles[s->particle_count++];
		memset(p, 0, sizeof *p);
		p->stable_id = Particle_SnapshotRecordId(src);
		p->argb_color = src->argbColor;
		p->size_scale = src->size * (float)effect->def->billboardScale;
		if (effect->textureFrameCount) {
			int frame = (int)((float)src->ageTicks * effect->def->colorDeltaScale * effect->textureAnimRate);
			frame %= effect->textureFrameCount;
			if (frame < 0) {
				frame += effect->textureFrameCount;
			}
			p->texture_frame = (uint16_t)(frame + 1);
		} else {
			p->texture_frame = 1;
		}
		if (effect->useAttachedTransform) {
			if (!Particle_SnapshotRecordPoint(src, p->world_pos.base, p->world_pos.offset))
				snapshot_particle_point_set_float(&p->world_pos, src->world.x, src->world.y, src->world.z);
			const float tail_offset[3] = { -src->vel.x * 0.5f, -src->vel.y * 0.5f, -src->vel.z * 0.5f };
			snapshot_particle_point_add(&p->tail_world_pos, &p->world_pos, tail_offset);
		} else {
			const Vec3f head = src->world;
			snapshot_particle_resolve_local(&local_transform, &head, &p->world_pos);
			Vec3f tail = { src->world.x - src->vel.x * 0.1f, src->world.y - src->vel.y * 0.1f,
						   src->world.z - src->vel.z * 0.1f };
			snapshot_particle_resolve_local(&local_transform, &tail, &p->tail_world_pos);
		}
		dst->particle_count++;
		src = src->next;
	}
	if (src) {
		s->dropped_records++;
	}
	return 1;
}

static int snapshot_particle_capture_dispatch(XwaSnapshot* s, const ParticleEffect* effect,
											  const ObjectRecord* owner, uint16_t owner_slot) {
	if (effect->particleSpawnCallback) {
		const ParticleRecord* parent = effect->particles;
		uint32_t walked = 0;
		while (parent && walked++ < (uint32_t)effect->maxParticles) {
			const ParticleEffect* child = parent->childEffects;
			if (child && child->particles &&
				!snapshot_particle_capture_effect(s, child, owner, owner_slot, 1)) {
				return 0;
			}
			parent = parent->next;
		}
	}
	return snapshot_particle_capture_effect(s, effect, owner, owner_slot, 0);
}

static void snapshot_capture_particles(XwaSnapshot* s, uint32_t render_state_slot_count) {
	uint32_t walked_effects = 0;
	for (const ParticleEffect* effect = g_worldParticleEffects;
		 effect && walked_effects++ < XWA_SNAP_MAX_PARTICLE_EFFECTS; effect = effect->next) {
		if (!snapshot_particle_capture_dispatch(s, effect, NULL, 0xffffu)) {
			return;
		}
	}
	for (uint32_t slot = 0; slot < render_state_slot_count; slot++) {
		const ObjectRecord* owner = &g_objectTable[slot];
		if (owner->objectType == OBJ_None) {
			continue;
		}
		uint32_t owner_effects = 0;
		for (const ParticleEffect* effect = g_objRenderState[slot].particleEffects;
			 effect && owner_effects++ < XWA_SNAP_MAX_PARTICLE_EFFECTS; effect = effect->next) {
			if (!snapshot_particle_capture_dispatch(s, effect, owner, (uint16_t)slot)) {
				return;
			}
		}
	}
}

void XwaSnapshot_CaptureFlight(void) {
	XwaSnapshot* s = wr();
	s->flight_object_count = 0;
	s->hyperspace_streak_count = 0;
	s->particle_effect_count = 0;
	s->particle_count = 0;
	/* Mission_FreeObjectStorageHandles zeroes the storage handles but leaves
	 * the locked base pointers (g_objectTable, mobile/craft pools) stale, and
	 * the next launch ticks MAIN_LOOP_INIT before Mission_Init reallocates
	 * them. Gate on the handle: it tracks the storage lifetime exactly. */
	if (!g_objectTable || g_objectTableHandle == 0) {
		return;
	}
	{
		const PlayerData* player = &g_players[g_localPlayer];
		XwaHyperspaceState* h = &s->hyperspace;
		const uint32_t live_count =
			(uint32_t)(g_hyperspaceStarStreakCount < 0 ? 0 : g_hyperspaceStarStreakCount);
		h->phase = player->hyperspacePhase;
		h->phase_elapsed_ticks = player->hyperspaceRuntime.phaseElapsedTicks;
		if (h->phase == PLAYER_HYPERSPACE_OUTBOUND || h->phase == PLAYER_HYPERSPACE_INBOUND) {
			uint32_t count =
				g_hyperspace_visible_streak_valid ? g_hyperspace_visible_streak_count : live_count;
			if (count > live_count) {
				count = live_count;
			}
			if (count > XWA_SNAP_MAX_HYPERSPACE_STREAKS) {
				count = XWA_SNAP_MAX_HYPERSPACE_STREAKS;
			}
			for (uint32_t i = 0; i < count; i++) {
				const HyperspaceStarStreak* src = &g_hyperspaceStarStreaks[i];
				XwaHyperspaceStreak* dst = &s->hyperspace_streaks[i];
				dst->offset[0] = src->offsetX;
				dst->offset[1] = src->offsetY;
				dst->offset[2] = src->offsetZ;
				dst->half_width = src->length;
				dst->roll = (uint16_t)src->rollAngle;
			}
			s->hyperspace_streak_count = count;
		} else if (h->phase == PLAYER_HYPERSPACE_PHASE_NONE) {
			g_hyperspace_visible_streak_valid = 0;
		}
	}
	/* OBJECT3D is allocated for [0, g_regionObjectSlotEnd), whereas the
	 * object table also contains the 32 local transient slots. Never index
	 * renderer state using the larger object-table capacity. Clamp as a
	 * defensive guard while mission slot ranges are being initialized. */
	uint32_t render_state_slot_count = g_objRenderState != NULL ? g_regionObjectSlotEnd : 0;
	if (render_state_slot_count > g_objectTableSlotCount) {
		render_state_slot_count = g_objectTableSlotCount;
	}
	for (uint32_t i = 0; i < g_objectTableSlotCount; i++) {
		const ObjectRecord* o = &g_objectTable[i];
		if (o->objectType == OBJ_None) {
			continue;
		}
		if (s->flight_object_count >= XWA_SNAP_MAX_FLIGHT_OBJECTS) {
			s->dropped_records++;
			continue;
		}
		XwaFlightObject* f = &s->flight_objects[s->flight_object_count++];
		memset(f, 0, sizeof *f);
		f->signature = o->objectSignature;
		f->slot = (uint16_t)i;
		f->object_type = o->objectType;
		f->genus = o->genusId;
		f->fg_idx = o->flightGroupIdx;
		f->region = o->regionIdx;
		f->world_pos[0] = o->world_x;
		f->world_pos[1] = o->world_y;
		f->world_pos[2] = o->world_z;
		f->prev_world_pos[0] = o->world_x;
		f->prev_world_pos[1] = o->world_y;
		f->prev_world_pos[2] = o->world_z;
		f->yaw = o->yaw;
		f->pitch = o->pitch;
		f->roll = o->roll;
		f->angle_d = o->angleD;
		f->player_owner = o->playerOwnerIdx;
		/* Billboard-law state (see header): the sprite frame / debris
		 * component selector, the type-2006 depth pull-in, and the
		 * render walk's slot-range dispatch class. */
		f->type_specific_0 = o->typeSpecificByte[0];
		f->type_specific_w = o->typeSpecificWord;
		if (i >= g_regionMainObjectSlotStart && i < g_regionMainObjectSlotEnd) {
			f->slot_class = XWA_SNAP_SLOT_MAIN;
		} else if (i >= g_localTransientSlotStart && i < g_localTransientSlotEnd) {
			f->slot_class = XWA_SNAP_SLOT_TRANSIENT;
		} else {
			f->slot_class = XWA_SNAP_SLOT_OTHER;
		}
		const MobileObject* m = o->mobj;
		if (m) {
			f->has_mobj = 1;
			f->iff = m->iff;
			f->team = m->team;
			f->state = m->state;
			f->motion_flags = m->motionFlags;
			f->node_switch = m->nodeSwitchIndex;
			f->orient_dirty = m->orientMatrixDirty;
			f->source_obj = m->sourceObjIdx;
			f->speed = m->speed;
			f->instance_extent = m->instanceExtent;
			f->source_object_type = m->sourceObjectType;
			f->render_offset[0] = (float)m->renderOffsetX;
			f->render_offset[1] = (float)m->renderOffsetY;
			f->render_offset[2] = (float)m->renderOffsetZ;
			f->prev_world_pos[0] = m->prevWorldX;
			f->prev_world_pos[1] = m->prevWorldY;
			f->prev_world_pos[2] = m->prevWorldZ;
			f->rows[0] = m->cachedSideX;
			f->rows[1] = m->cachedSideY;
			f->rows[2] = m->cachedSideZ;
			f->rows[3] = m->cachedFwdX;
			f->rows[4] = m->cachedFwdY;
			f->rows[5] = m->cachedFwdZ;
			f->rows[6] = m->cachedUpX;
			f->rows[7] = m->cachedUpY;
			f->rows[8] = m->cachedUpZ;
			f->spin_angle = m->spinAngleQ16;
			f->spin_axis[0] = m->spinAxisX;
			f->spin_axis[1] = m->spinAxisY;
			f->spin_axis[2] = m->spinAxisZ;
			const CraftData* cd = m->pCraft;
			if (cd) {
				f->has_craft = 1;
				f->sfoil_state = cd->sFoilState;
				f->carried_object_slot = cd->carriedObjectIndex;
				if (cd->carriedObjectIndex != 0xffffu && cd->carriedObjectIndex < g_objectTableSlotCount &&
					g_objectTable[cd->carriedObjectIndex].objectType != OBJ_None) {
					f->carried_object_signature = g_objectTable[cd->carriedObjectIndex].objectSignature;
				}
				f->system_flags = cd->systemFlags;
				f->subsystem_damage = cd->subsystemDamage;
				memcpy(f->component_state, cd->componentState, sizeof f->component_state);
				memcpy(f->mesh_rotation, cd->meshRotation, sizeof f->mesh_rotation);
				memcpy(f->component_hp, cd->componentHp, sizeof f->component_hp);
				/* Engine-glow scale inputs (EngineGlow_RenderObjectGlows). */
				f->object_kind = cd->objectKind;
				f->eg_working = (uint8_t)(cd->workingSubsystems != 0);
				f->eg_shield_redirect = cd->shieldRedirect;
				f->eg_laser_redirect = cd->laserRedirect;
				f->eg_beam_level = cd->beamLevel;
				f->eg_throttle = cd->throttleSpeed;
				f->eg_output_scale = cd->engineOutputScale;
				f->eg_max_speed = (uint16_t)cd->aiFlight.maxSpeedCache;
				/* Per-emitter damage knockouts -> bitmask (read-only walk
				 * of the render-state list; emitters >= 32 keep drawing).
				 * g_objRenderState is allocated with the flight scene —
				 * NULL during mission loading. */
				f->eg_knockout_mask = 0;
				if (i < render_state_slot_count) {
					for (const EngineGlowKnockoutMark* k = g_objRenderState[i].engineGlowKnockouts; k != NULL;
						 k = k->next) {
						if (k->emitterIndex < 32) {
							f->eg_knockout_mask |= 1u << k->emitterIndex;
						}
					}
				}
			}
		}
	}

	XwaSnapshotFlightMap_Begin(s);
	uint32_t flight_index = 0;
	for (uint32_t slot = g_regionMainObjectSlotStart;
		 s->flight_map.active && slot < g_regionStaticObjectSlotEnd && slot < g_objectTableSlotCount;
		 slot++) {
		while (flight_index < s->flight_object_count && s->flight_objects[flight_index].slot < slot) {
			flight_index++;
		}
		const uint16_t index =
			flight_index < s->flight_object_count && s->flight_objects[flight_index].slot == slot
				? (uint16_t)flight_index
				: UINT16_MAX;
		XwaSnapshotFlightMap_CaptureObject(s, slot, index);
	}
	XwaSnapshotFlightMap_End(s);
	/* Object trails are persistent renderer STATE. ObjectTrail_Update has
	 * already advanced these lists during the classic effects pass; capture
	 * the linked state here without invoking it again and without trusting
	 * ObjectTrailEmitter.pointCount (the list is the geometry authority). */
	if (g_objRenderState != NULL) {
		for (uint32_t slot = 0; slot < render_state_slot_count; slot++) {
			const ObjectRecord* owner = &g_objectTable[slot];
			if (owner->objectType == OBJ_None) {
				continue;
			}
			for (const ObjectTrailEmitter* src = g_objRenderState[slot].trailHead; src != NULL;
				 src = src->next) {
				if (s->trail_emitter_count >= XWA_SNAP_MAX_TRAIL_EMITTERS) {
					s->dropped_records++;
					break;
				}
				XwaTrailEmitter* dst = &s->trail_emitters[s->trail_emitter_count++];
				memset(dst, 0, sizeof *dst);
				dst->owner_slot = (uint16_t)slot;
				dst->owner_signature = owner->objectSignature;
				dst->trail_kind = (uint8_t)src->trailKind;
				dst->texture_model_type = (uint16_t)src->textureModelType;
				/* Every recovered trail initializer uses the non-animated path.
				 * Keep the resolved field in the POD contract so animated emitters
				 * have an explicit frame value. */
				dst->texture_frame = 1;
				dst->argb_color = src->argbColor;
				dst->ribbon_width = src->ribbonWidth;
				dst->start_alpha_bias = src->startAlphaBias;
				dst->alpha_fade_start = src->alphaFadeStart;
				dst->alpha_fade_rate = src->alphaFadeRate;
				dst->forward_offset = src->forwardOffset;
				dst->first_point = s->trail_point_count;

				const ObjectTrailPoint* point = src->pointHead;
				while (point != NULL && dst->point_count < XWA_SNAP_MAX_TRAIL_POINTS_PER_EMITTER &&
					   s->trail_point_count < XWA_SNAP_MAX_TRAIL_POINTS) {
					XwaTrailPoint* out = &s->trail_points[s->trail_point_count++];
					memcpy(out->world_pos, point->preciseWorld, sizeof out->world_pos);
					out->spawn_time_ms = point->spawnTime;
					out->age_fade = point->ageFade;
					out->tex_v = point->texV;
					dst->point_count++;
					point = point->next;
				}
				if (point != NULL) {
					/* Retain the newest, most visible contiguous prefix. One
					 * truncation record is sufficient even if a corrupt/cyclic list
					 * follows the cap. */
					s->dropped_records++;
				}
			}
		}
	}

	/* Particle lists are persistent renderer state. Capture the already-updated
	 * records without advancing them or consuming the game RNG. */
	if (g_objRenderState != NULL) {
		snapshot_capture_particles(s, render_state_slot_count);
	}

	/* Active legacy surface-projector state. Pool records persist across
	 * frames; capture only semantic projector parameters, never the classic
	 * renderer's generated per-face UV arrays. */
	for (int pool_kind = 0; pool_kind < 2; pool_kind++) {
		ObjectMeshTextureLayerBlock* pool = pool_kind ? g_blastMarkPatchPool : g_glowMarkPatchPool;
		GlowMarkCaptureMeta* meta = pool_kind ? g_blast_mark_meta : g_glow_mark_meta;
		const int count = pool_kind ? 32 : 24;
		for (int i = 0; i < count; i++) {
			const ObjectMeshTextureLayerBlock* p = &pool[i];
			if (!p->active || p->objectIndex >= g_objectTableSlotCount ||
				g_objectTable[p->objectIndex].objectType == OBJ_None) {
				continue;
			}
			if (s->glow_mark_count >= XWA_SNAP_MAX_GLOW_MARKS) {
				s->dropped_records++;
				continue;
			}
			XwaGlowMark* m = &s->glow_marks[s->glow_mark_count++];
			memset(m, 0, sizeof *m);
			m->owner_slot = p->objectIndex;
			m->owner_signature = g_objectTable[p->objectIndex].objectSignature;
			m->pool_kind = (uint8_t)pool_kind;
			m->pool_index = (uint8_t)i;
			m->generation = meta[i].generation;
			m->texture_model_type = p->modelType;
			m->texture_frame = pool_kind ? meta[i].texture_frame : (uint16_t)p->currentFrame;
			m->age_ticks = (uint32_t)p->currentFrame;
			m->world_segment_mode = meta[i].world_segment_mode;
			m->persistent_until_cleared = (uint8_t)(p->persistentUntilCleared != 0);
			memcpy(m->center, &p->center, sizeof m->center);
			memcpy(m->normal, &p->normal, sizeof m->normal);
			memcpy(m->u_axis, &p->uAxis, sizeof m->u_axis);
			memcpy(m->v_axis, &p->vAxis, sizeof m->v_axis);
			m->inv_scale_u = meta[i].inv_scale_u;
			m->inv_scale_v = meta[i].inv_scale_v;
			m->layer_uv_scale = p->facePatches[0].passes[0].uvScale;
			m->mesh_mask = meta[i].mesh_mask;
		}
	}

	const PlayerViewState* vs = &g_players[g_localPlayer].viewState;
	XwaFlightCamera* c = &s->flight_camera;
	c->world_pos[0] = vs->savedTargetX;
	c->world_pos[1] = vs->savedTargetY;
	c->world_pos[2] = vs->savedTargetZ;
	FVIEW_CopyRenderCameraRows(c->rows);
	c->view_pitch = vs->viewPitch;
	c->view_yaw = vs->viewYaw;
	c->view_roll = vs->viewRoll;
	c->view_angle_d = vs->viewAngleD;
	c->focus_obj_idx = vs->cameraFocusObjIdx;
	c->player_obj_idx = g_players[g_localPlayer].objectIndex;
	c->hangar_floor_z = g_launchBaseZ;
	c->hangar_launch_ref_obj_idx = g_launchRefObjIdx;
	c->external = (uint8_t)(vs->externalCameraActive != 0);
	c->cockpit_visible = g_players[g_localPlayer].cockpitVisible;
	c->hyperspace_phase = (uint8_t)g_players[g_localPlayer].hyperspacePhase;
	c->region = g_players[g_localPlayer].regionIndex;
	c->map_mode = g_players[g_localPlayer].mapCameraState;
	c->film_overlay = (uint8_t)(g_filmPlaybackMode && g_filmOverlayActive == 1);
	c->in_hangar = (uint8_t)(g_inHangarReady != 0);
	c->ds_hangar = (uint8_t)(g_missionHeader.body.hangar == XWA_HANGAR_DEATHSTAR);
	/* DS tunnel turbolaser beam light (FlightView_Render's direct
	 * scene-light write; regionIdx is the tunnel-region global). */
	c->ds_beam_active = (uint8_t)(g_deathStarTunnelLaserRegions[regionIdx].enabled &&
								  g_deathStarTunnelLaserRegions[regionIdx].beamLightActive);
	if (c->ds_beam_active) {
		c->ds_beam_world_pos[0] = g_deathStarTunnelLaserRegions[regionIdx].pointLightX;
		c->ds_beam_world_pos[1] = g_deathStarTunnelLaserRegions[regionIdx].pointLightY;
		c->ds_beam_world_pos[2] = g_deathStarTunnelLaserRegions[regionIdx].pointLightZ;

		const int object_idx = g_deathStarTunnelLaserRegions[regionIdx].laserObjIdx;
		const int model_extent = g_modelTypeTable[OBJ_LaserImperialDS].maxBoundsExtent;
		if (object_idx >= 0 && (uint32_t)object_idx < g_objectTableSlotCount && model_extent > 0) {
			const ObjectRecord* object = &g_objectTable[object_idx];
			if (object->objectType == OBJ_LaserImperialDS) {
				XwaDeathStarBeam* beam = &s->death_star_beam;
				beam->active = 1;
				beam->object_slot = (uint16_t)object_idx;
				beam->object_signature = object->objectSignature;
				beam->length_scale =
					-g_deathStarTunnelLaserRegions[regionIdx].remainingDistance / (float)model_extent;
			}
		}
	}
	c->proj_scale = g_projScale;
	c->vp_w = g_flightVpWidth;
	c->vp_h = g_flightVpHeight;
	c->vp_center_x = (int16_t)g_flightVpCenterX;
	c->vp_center_y = (int16_t)g_flightVpCenterY;
	c->proj_offset_y = (int16_t)g_projOffsetY;
	c->screen_w = (uint16_t)g_screenWidth;
	c->screen_h = (uint16_t)g_screenHeight;
	c->brightness_q8 = (uint16_t)g_flightBrightnessScaleQ8;
	s->flight_camera_valid = 1;

	/* Local-player weapon-fire light pulses (point-light channel). */
	s->light_pulse_active = (uint8_t)(g_localPlayerLightPulseActive != 0);
	for (int p = 0; p < XWA_SNAP_MAX_LIGHT_PULSES; p++) {
		const LocalPlayerLightPulse* src = &g_localPlayerLightPulses[p];
		XwaLightPulse* dst = &s->light_pulses[p];
		dst->enabled = (uint8_t)(src->enabled != 0);
		dst->start_time = src->startTime;
		dst->cycle_ticks = src->cycleTicks;
		dst->inv_cycle_ticks = src->invCycleTicks;
		dst->fade_ticks = src->fadeTicks;
		dst->intensity = src->intensity;
		dst->cull_radius = src->cullRadius;
		dst->color[0] = src->colorR;
		dst->color[1] = src->colorG;
		dst->color[2] = src->colorB;
	}

	/* Player cockpit STATE (see XwaCockpit): model identity from the
	 * loaded-handle registry (seat 0 = g_cockpitModel incl. the classic
	 * Tie/CombatSim fallbacks; turret seats = the seat's turret model),
	 * seat hardpoint + camera pan, aim, and the classic queue gates.
	 * The driver derives the draw transform from the anchor object's
	 * state basis + these fields. */
	{
		XwaCockpit* k = &s->cockpit;
		memset(k, 0, sizeof *k);
		const int playerObjIdx = g_players[g_localPlayer].objectIndex;
		if (g_objectTable != NULL && playerObjIdx >= 0) {
			const ObjectRecord* pobj = &g_objectTable[playerObjIdx];
			uint16_t handle = 0;
			k->seat = (uint8_t)g_players[g_localPlayer].currentSeatIdx;
			if (k->seat == 0) {
				handle = g_cockpitModel;
			} else {
				const uint16_t turretType =
					g_modelDefs[(uint16_t)GetModelIndexFromType((uint16_t)pobj->objectType)]
						.turretModelIndex[(k->seat - 1) & 1];
				if (turretType != 0) {
					handle = g_loadedModels.byObjectType[turretType];
				}
				if (pobj->mobj != NULL && pobj->mobj->pCraft != NULL) {
					k->aim_angle_a = (int16_t)pobj->mobj->pCraft->turretAim.aimAngleA[(k->seat - 1) & 1];
					k->aim_angle_b = (int16_t)pobj->mobj->pCraft->turretAim.aimAngleB[(k->seat - 1) & 1];
				}
			}
			k->look_available = g_players[g_localPlayer].cockpitLookAvailable;
			k->toggle_available = (uint8_t)g_players[g_localPlayer].cockpitToggleAvailable;
			k->hardpoint_world[0] = g_players[g_localPlayer].hardpointWorldX;
			k->hardpoint_world[1] = g_players[g_localPlayer].hardpointWorldY;
			k->hardpoint_world[2] = g_players[g_localPlayer].hardpointWorldZ;
			k->hardpoint_local[0] = g_players[g_localPlayer].hardpointLocalX;
			k->hardpoint_local[1] = g_players[g_localPlayer].hardpointLocalY;
			k->hardpoint_local[2] = g_players[g_localPlayer].hardpointLocalZ;
			k->camera_pan[0] = (float)vs->cameraPanDeltaX;
			k->camera_pan[1] = (float)vs->cameraPanDeltaY;
			k->camera_pan[2] = (float)vs->cameraPanDeltaZ;
			const char* name = XwaSnapshotExport_OptHandleName(handle);
			if (name != NULL) {
				snprintf(k->model_name, sizeof k->model_name, "%s", name);
				s->cockpit_valid = 1;
			}
			if (g_exteriorModelLoaded) {
				const char* ext = XwaSnapshotExport_OptHandleName(g_exteriorModel);
				if (ext != NULL) {
					snprintf(k->exterior_name, sizeof k->exterior_name, "%s", ext);
				}
			}
		}
	}

	/* Capture the backdrop records drawn by Backdrop_RenderCurrentRegion.
	 * Hangar backdrops come from the saved mission region while hangar
	 * objects and the camera remain in the synthetic hangar region. */
	{
		const int region =
			g_inHangarReady ? g_hangarSavedMissionRegionIdx : g_players[g_localPlayer].regionIndex;
		uint32_t n = 0;
		if (region >= 0 && region < XWA_BACKDROP_REGION_COUNT &&
			g_missionHeader.body.hangar != XWA_HANGAR_DEATHSTAR) {
			uint32_t count = (uint32_t)g_backdropCountByRegion[region];
			if (count > XWA_SNAP_MAX_BACKDROPS) {
				count = XWA_SNAP_MAX_BACKDROPS;
			}
			const WorldRectRecord* beam = g_deathStarTunnelLaserRegions[region].beamSpriteRect;
			for (; n < count; n++) {
				const WorldRectRecord* w = &g_backdropRecordsByRegion[region][n];
				XwaBackdrop* b = &s->backdrops[n];
				b->model_type = w->modelType;
				b->frame = w->frame;
				b->flags = w->flags;
				b->draw_flags = w->drawFlags;
				b->side = (uint8_t)w->side;
				b->hidden = (uint8_t)((g_backdropsEnabled == 0 && w->drawFlags != 0) ||
									  (beam == w && g_deathStarTunnelLaserRegions[region].shotActive == 0));
				b->world_dir[0] = (float)w->worldDirQ20.x;
				b->world_dir[1] = (float)w->worldDirQ20.y;
				b->world_dir[2] = (float)w->worldDirQ20.z;
				b->angular_scale = w->angularScale;
				b->color[0] = w->colorR;
				b->color[1] = w->colorG;
				b->color[2] = w->colorB;
				b->intensity = w->intensity;
				b->strip_half_height = w->stripHalfHeight;
				b->strip_segment_count = 0;
				b->strip_segments_per_frame = (uint8_t)w->stripSegmentsPerFrame;
				if (w->stripCoords != NULL && w->stripSegmentCount > 0) {
					int segs = w->stripSegmentCount;
					if (segs + 1 > XWA_SNAP_MAX_STRIP_COORDS) {
						segs = XWA_SNAP_MAX_STRIP_COORDS - 1;
						s->dropped_records++;
					}
					b->strip_segment_count = (uint8_t)segs;
					for (int c = 0; c <= segs; c++) {
						b->strip_coords[c][0] = (float)w->stripCoords[c].x;
						b->strip_coords[c][1] = (float)w->stripCoords[c].y;
						b->strip_coords[c][2] = (float)w->stripCoords[c].z;
					}
				}
			}
		}
		s->backdrop_count = n;
	}

	XwaSnapshotHud_Capture(&s->hud);
}

static GlowMarkCaptureMeta* glow_mark_meta_for_patch(const void* ptr) {
	const uintptr_t p = (uintptr_t)ptr;
	const uintptr_t glow_begin = (uintptr_t)g_glowMarkPatchPool;
	const uintptr_t glow_end = glow_begin + sizeof g_glowMarkPatchPool;
	const uintptr_t blast_begin = (uintptr_t)g_blastMarkPatchPool;
	const uintptr_t blast_end = blast_begin + sizeof g_blastMarkPatchPool;
	if (p >= glow_begin && p < glow_end) {
		return &g_glow_mark_meta[(p - glow_begin) / sizeof g_glowMarkPatchPool[0]];
	}
	if (p >= blast_begin && p < blast_end) {
		return &g_blast_mark_meta[(p - blast_begin) / sizeof g_blastMarkPatchPool[0]];
	}
	return NULL;
}

void XwaSnapshot_NoteGlowMarkAllocation(const void* patch, uint16_t texture_frame) {
	GlowMarkCaptureMeta* m = glow_mark_meta_for_patch(patch);
	if (!m) {
		return;
	}
	memset(m, 0, sizeof *m);
	m->generation = ++g_glow_mark_generation;
	m->texture_frame = texture_frame;
}

void XwaSnapshot_NoteGlowMarkProjector(const void* patch, float inv_scale_u, float inv_scale_v,
									   int world_segment_mode) {
	GlowMarkCaptureMeta* m = glow_mark_meta_for_patch(patch);
	if (!m) {
		return;
	}
	m->inv_scale_u = inv_scale_u;
	m->inv_scale_v = inv_scale_v;
	m->world_segment_mode = (uint8_t)(world_segment_mode != 0);
	m->mesh_mask = 0;
}

void XwaSnapshot_NoteGlowMarkMesh(const void* patch, unsigned int mesh_index) {
	GlowMarkCaptureMeta* m = glow_mark_meta_for_patch(patch);
	if (m && mesh_index < 64) {
		m->mesh_mask |= (uint64_t)1 << mesh_index;
	}
}

void XwaSnapshot_EmitModelPreview(const XwaModelPreview* preview) {
	XwaSnapshot* s = wr();
	if (!preview) {
		return;
	}
	if (s->model_preview_count >= XWA_SNAP_MAX_MODEL_PREVIEWS) {
		s->dropped_records++;
		return;
	}
	XwaModelPreview* m = &s->model_previews[s->model_preview_count++];
	*m = *preview;
	m->z_order = g_z_counter++;
	m->target = (uint8_t)g_emit_target;
}

void XwaSnapshot_EmitGlyph(int font_size, unsigned char ch, int x, int y, uint32_t color) {
	XwaSnapshot* s = wr();
	/* Record the color the glyph is actually drawn with: the engine's
	 * blitter fades it while the text fade-in timer runs (every
	 * FrontendText_Draw* loop passes allowColorRemap=1). */
	if (color != 0 && g_glyphScratchTtl != 0) {
		color = FrontImage_GetFadedGlyphColor16((uint16_t)color);
	}
	if (s->glyph_count >= XWA_SNAP_MAX_GLYPHS) {
		s->dropped_records++;
		return;
	}
	XwaGlyph2D* g = &s->glyphs[s->glyph_count++];
	g->z_order = g_z_counter++;
	g->target = (uint8_t)g_emit_target;
	g->ch = ch;
	g->font_size = (int16_t)font_size;
	g->x = (int16_t)x;
	g->y = (int16_t)y;
	g->color = color;
	stamp_clip(&g->clip_left, &g->clip_top, &g->clip_right, &g->clip_bottom);
}
