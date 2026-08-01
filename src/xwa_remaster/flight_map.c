#include "xwa_remaster/flight_map.h"

#include "aeron/scene/draw_list2d.h"
#include "xwa_remaster/color.h"
#include "xwa_remaster/text.h"
#include "xwa_remaster/world.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP_PLANE_Z (-65536)
#define MAP_GRID_LIMIT 0x100000
#define MAP_GRID_STEP 0x10000
#define MAP_ICON_GROUP 14800
#define MAP_ICON_CACHE_SIZE 256

typedef struct MapPreparedObject {
	const XwaFlightMapObject* map;
	const XwaFlightObject* object;
	uint32_t snapshot_index;
	float eye[3];
	float screen_x;
	float screen_y;
	uint16_t scan_ordinal;
	uint8_t annotation_pass;
} MapPreparedObject;

static struct {
	AeronDrawList2D* annotations[2];
	AeronDrawList2D* grid;
	AeronDrawList2D* text;
	MapPreparedObject objects[XWA_SNAP_MAX_FLIGHT_MAP_OBJECTS];
	uint16_t object_count;
	XwaAssetRef icons[MAP_ICON_CACHE_SIZE];
	uint8_t icon_valid[MAP_ICON_CACHE_SIZE];
	uint32_t icon_generation;
	float world_to_eye[9];
} map_state;

static uint8_t map_palette_for_iff(uint8_t iff) {
	switch (iff) {
		case 0:
			return 63;
		case 1:
		case 4:
			return 55;
		case 2:
			return 51;
		case 3:
			return 59;
		default:
			return 86;
	}
}

static uint32_t map_icon_argb(uint8_t iff) {
	switch (iff) {
		case 0:
			return 0xff00b400u;
		case 1:
		case 4:
			return 0xffb82400u;
		case 2:
			return 0xff008cf8u;
		case 3:
			return 0xffc0a400u;
		default:
			return 0xffb844a8u;
	}
}

static uint32_t map_color_argb(uint8_t color_code) {
	return XwaSnapshotExport_FlightPaletteColor(XwaSnapshotExport_FlightColorCodePaletteIndex(color_code));
}

static void map_argb_linear(uint32_t argb, float out[4]) {
	const float a = (float)((argb >> 24) & 255u) / 255.0f;
	out[0] = XwaRemaster_SrgbToLinear((float)((argb >> 16) & 255u) / 255.0f) * a;
	out[1] = XwaRemaster_SrgbToLinear((float)((argb >> 8) & 255u) / 255.0f) * a;
	out[2] = XwaRemaster_SrgbToLinear((float)(argb & 255u) / 255.0f) * a;
	out[3] = a;
}

static void map_world_to_eye(const XwaFlightCamera* camera, const float world_to_eye[9],
							 const int32_t world[3], float out[3]) {
	float delta[3];
	XwaRemasterWorld_DeltaI32(world, camera->world_pos, delta);
	for (int row = 0; row < 3; row++) {
		out[row] = world_to_eye[row * 3 + 0] * delta[0] + world_to_eye[row * 3 + 1] * delta[1] +
				   world_to_eye[row * 3 + 2] * delta[2];
	}
}

static int map_clip_project_line(const XwaRemasterFlightView* view, float a[3], float b[3], float* ax,
								 float* ay, float* bx, float* by) {
	const float near_z = view->camera.near_z;
	if (a[2] < near_z && b[2] < near_z)
		return 0;
	if (a[2] < near_z || b[2] < near_z) {
		float* behind = a[2] < near_z ? a : b;
		const float* front = a[2] < near_z ? b : a;
		const float t = (near_z - behind[2]) / (front[2] - behind[2]);
		behind[0] += (front[0] - behind[0]) * t;
		behind[1] += (front[1] - behind[1]) * t;
		behind[2] = near_z;
	}
	return XwaRemasterFlight_ProjectView(view, a, ax, ay) && XwaRemasterFlight_ProjectView(view, b, bx, by);
}

static void map_add_world_line(AeronDrawList2D* list, const XwaFlightCamera* camera,
							   const XwaRemasterFlightView* view, const int32_t world_a[3],
							   const int32_t world_b[3], uint8_t palette, float thickness, int depth_test) {
	float a[3], b[3], ax, ay, bx, by, rgba[4];
	map_world_to_eye(camera, map_state.world_to_eye, world_a, a);
	map_world_to_eye(camera, map_state.world_to_eye, world_b, b);
	if (!map_clip_project_line(view, a, b, &ax, &ay, &bx, &by))
		return;
	map_argb_linear(map_color_argb(palette), rgba);
	if (depth_test) {
		AeronDrawList_AddProjectedLine(list, ax, ay, a[2], bx, by, b[2], view->camera.near_z, thickness, rgba,
									   AERON_BLIT2D_BLEND_PMA, &view->viewport);
	} else {
		AeronDrawList_AddLine(list, ax, ay, bx, by, thickness, rgba, AERON_BLIT2D_BLEND_PMA, &view->viewport);
	}
}

static int map_candidate_visible(const XwaFlightMapObject* map, const float eye[3],
								 const XwaRemasterFlightView* view, const XwaFlightCamera* camera) {
	float radius = (float)(map->max_bounds_extent > 0 ? map->max_bounds_extent : 1);
	const float far_z = eye[2] + radius;
	if (far_z < view->camera.near_z)
		return 0;
	if (map->cull_kind == XWA_FLIGHT_MAP_CULL_SPHERE && far_z / 256.0f > radius)
		return 0;
	if (map->cull_kind == XWA_FLIGHT_MAP_CULL_BOUNDS && radius < far_z / 16.0f)
		radius = far_z / 16.0f;
	const float source_w = camera->vp_w ? camera->vp_w : (camera->screen_w ? camera->screen_w : 640.0f);
	const float source_h = camera->vp_h ? camera->vp_h : (camera->screen_h ? camera->screen_h : 480.0f);
	const float source_aspect = source_w / source_h;
	const float output_aspect = (float)view->viewport.width / view->viewport.height;
	const float horizontal_scale = output_aspect / source_aspect;
	return fabsf(eye[0]) - radius <= far_z * horizontal_scale && fabsf(eye[1]) - radius <= far_z;
}

static int map_depth_compare(const void* left, const void* right) {
	const MapPreparedObject* a = (const MapPreparedObject*)left;
	const MapPreparedObject* b = (const MapPreparedObject*)right;
	if (a->eye[2] < b->eye[2])
		return 1;
	if (a->eye[2] > b->eye[2])
		return -1;
	if (a->scan_ordinal < b->scan_ordinal)
		return 1;
	if (a->scan_ordinal > b->scan_ordinal)
		return -1;
	return 0;
}

static const XwaAssetRef* map_resolve_icon(XwaRemasterAssets* assets, uint16_t icon_id) {
	const unsigned int frame = icon_id / 100u;
	if (frame == 0 || frame >= MAP_ICON_CACHE_SIZE)
		return NULL;
	const uint32_t generation = XwaRemasterAssets_Generation(assets);
	if (map_state.icon_generation != generation) {
		memset(map_state.icon_valid, 0, sizeof map_state.icon_valid);
		map_state.icon_generation = generation;
	}
	if (map_state.icon_valid[frame] == 0) {
		map_state.icon_valid[frame] =
			(uint8_t)(XwaRemasterAssets_FlightAtlasFrame(assets, MAP_ICON_GROUP, (int)frame - 1,
														 &map_state.icons[frame])
						  ? 1
						  : 2);
	}
	return map_state.icon_valid[frame] == 1 ? &map_state.icons[frame] : NULL;
}

static void map_add_icon(AeronDrawList2D* list, XwaRemasterAssets* assets, const XwaFlightMapObject* map,
						 const XwaRemasterFlightView* view, float center_x, float center_y) {
	const XwaAssetRef* ref = map_resolve_icon(assets, map->icon_id);
	if (!ref)
		return;
	const float size = 16.0f * view->classic_pixel_scale;
	const float x = center_x - size * 0.5f;
	const float y = center_y - size * 0.5f;
	if (x < view->viewport.x || y < view->viewport.y || x + size >= view->viewport.x + view->viewport.width ||
		y + size >= view->viewport.y + view->viewport.height)
		return;
	AeronDrawList2DSprite sprite = { 0 };
	sprite.texture = ref->texture;
	sprite.src_u0 = ref->u0;
	sprite.src_v0 = ref->v0;
	sprite.src_u1 = ref->u1;
	sprite.src_v1 = ref->v1;
	sprite.dst_x = x;
	sprite.dst_y = y;
	sprite.dst_w = size;
	sprite.dst_h = size;
	map_argb_linear(map_icon_argb(map->effective_iff), sprite.tint);
	sprite.blend = AERON_BLIT2D_BLEND_PMA;
	sprite.filter = AERON_BLIT2D_FILTER_LINEAR;
	sprite.scissor = view->viewport;
	AeronDrawList_AddSprite(list, &sprite);
}

static int32_t map_q15_mul(int32_t a, int16_t b) { return (int32_t)(((int64_t)a * b) >> 15); }

static void map_add_box(const XwaSnapshot* snapshot, const XwaRemasterFlightView* view,
						const MapPreparedObject* prepared) {
	const XwaFlightMapObject* map = prepared->map;
	if (!map->box_visible || prepared->eye[2] <= 0.0f)
		return;
	const float source_w = snapshot->flight_camera.screen_w ? snapshot->flight_camera.screen_w : 640.0f;
	const float proj_scale =
		snapshot->flight_camera.proj_scale > 0.0f ? snapshot->flight_camera.proj_scale : 512.0f;
	const int max_box_size = (int)source_w * 3 / 4;
	int box_h = (int)(proj_scale * (float)(map->box_extent > 0 ? map->box_extent : 0) / prepared->eye[2]);
	if (box_h < 8)
		box_h = 8;
	if (box_h > max_box_size)
		box_h = max_box_size;
	const int box_w = box_h + 8;
	box_h += 8;
	int arm_x = box_w >> 3;
	int arm_y = box_h >> 3;
	if (arm_x < 3)
		arm_x = 3;
	if (arm_y < 3)
		arm_y = 3;
	const float scale = view->classic_pixel_scale;
	const float x = prepared->screen_x - (float)(box_w / 2) * scale;
	const float y = prepared->screen_y - (float)(box_h / 2) * scale;
	const float w = (float)box_w * scale;
	const float h = (float)box_h * scale;
	const float edge = fmaxf(1.0f, scale);
	const float ax = (float)arm_x * scale;
	const float ay = (float)arm_y * scale;
	float rgba[4];
	map_argb_linear(map_color_argb(map->box_color_index), rgba);
	AeronDrawList2D* list = map_state.annotations[prepared->annotation_pass];
	const AeronRectI* scissor = &view->viewport;
	AeronDrawList_AddFill(list, x, y, ax, edge, rgba, AERON_BLIT2D_BLEND_PMA, scissor);
	AeronDrawList_AddFill(list, x + w - ax, y, ax, edge, rgba, AERON_BLIT2D_BLEND_PMA, scissor);
	AeronDrawList_AddFill(list, x, y + h, ax, edge, rgba, AERON_BLIT2D_BLEND_PMA, scissor);
	AeronDrawList_AddFill(list, x + w - ax, y + h, ax, edge, rgba, AERON_BLIT2D_BLEND_PMA, scissor);
	AeronDrawList_AddFill(list, x, y, edge, ay, rgba, AERON_BLIT2D_BLEND_PMA, scissor);
	AeronDrawList_AddFill(list, x + w, y, edge, ay, rgba, AERON_BLIT2D_BLEND_PMA, scissor);
	AeronDrawList_AddFill(list, x, y + h - ay, edge, ay, rgba, AERON_BLIT2D_BLEND_PMA, scissor);
	AeronDrawList_AddFill(list, x + w, y + h - ay, edge, ay, rgba, AERON_BLIT2D_BLEND_PMA, scissor);
}

static void map_add_object_annotations(const XwaSnapshot* snapshot, XwaRemasterAssets* assets,
									   const XwaRemasterFlightView* view, const MapPreparedObject* prepared,
									   int used_icon) {
	const XwaFlightMapObject* map = prepared->map;
	const XwaFlightObject* object = prepared->object;
	AeronDrawList2D* list = map_state.annotations[prepared->annotation_pass];
	const float stroke = fmaxf(1.0f, view->classic_pixel_scale);
	if (used_icon)
		map_add_icon(list, assets, map, view, prepared->screen_x, prepared->screen_y);
	map_add_box(snapshot, view, prepared);
	if (object->genus == XWA_SNAP_GENUS_DEBRIS || object->genus == XWA_SNAP_GENUS_EXPLOSION ||
		prepared->eye[2] <= 0.0f)
		return;
	const uint8_t palette = map_palette_for_iff(map->effective_iff);
	int32_t grid[3] = { object->world_pos[0], object->world_pos[1], MAP_PLANE_Z };
	map_add_world_line(list, &snapshot->flight_camera, view, object->world_pos, grid, palette, stroke, 0);
	if (map->movement_visible) {
		int32_t movement[3] = { grid[0], grid[1], MAP_PLANE_Z };
		movement[0] += map_q15_mul(0x100, map->move_x);
		movement[1] += map_q15_mul(0x100, map->move_y);
		if (object->speed >= 0x400u) {
			movement[0] += map->move_x;
			movement[1] += map->move_y;
		} else {
			movement[0] += map_q15_mul(32 * object->speed, map->move_x);
			movement[1] += map_q15_mul(32 * object->speed, map->move_y);
		}
		map_add_world_line(list, &snapshot->flight_camera, view, grid, movement, palette, stroke, 0);
	}
	if (snapshot->flight_map.has_order_endpoint && object->slot == snapshot->flight_map.current_target_slot &&
		object->signature == snapshot->flight_map.current_target_signature) {
		map_add_world_line(list, &snapshot->flight_camera, view, object->world_pos,
						   snapshot->flight_map.order_endpoint_world, 0x36, stroke, 0);
	}
}

static void map_add_object_text(const XwaFlightMapObject* map, const XwaFlightObject* object,
								const XwaRemasterFlightView* view, const XwaFlightFontRef* font,
								const char* label, float screen_x, float screen_y, float depth,
								float classic_width_px, float font_size, float line_height, int show_range) {
	if (!map->label_visible || !font || depth <= 0.0f)
		return;
	const float focal = (float)view->viewport.height * 0.5f / tanf(view->camera.v_half_rad);
	float extent = focal * (float)(map->box_extent > 0 ? map->box_extent : 1) / depth;
	if (extent < classic_width_px / 80.0f)
		extent = classic_width_px / 80.0f;
	if (extent > classic_width_px * 0.5f)
		extent = classic_width_px * 0.5f;
	const float box_size = extent + 4.0f * view->classic_pixel_scale;
	const uint32_t argb = map_color_argb(map_palette_for_iff(map->effective_iff));
	if (label && label[0]) {
		XwaRemasterText_AddFlightString(map_state.text, font, label, screen_x,
										screen_y - box_size * 0.5f - line_height - view->classic_pixel_scale,
										font_size, XWA_REMASTER_TEXT_ALIGN_CENTER, argb, &view->viewport);
	}
	if (show_range && object->genus != XWA_SNAP_GENUS_PLAYER_PROJECTILE &&
		object->genus != XWA_SNAP_GENUS_NPC_PROJECTILE) {
		char range[16];
		const unsigned int whole = map->range_value / 100u;
		(void)snprintf(range, sizeof range, "%2u.%02u", whole, map->range_value - whole * 100u);
		const float x = screen_x - XwaRemasterText_MeasureFlightString(font, "00", font_size);
		XwaRemasterText_AddFlightString(map_state.text, font, range, x,
										screen_y + box_size * 0.5f + view->classic_pixel_scale, font_size,
										XWA_REMASTER_TEXT_ALIGN_LEFT, argb, &view->viewport);
	}
}

static void map_build_grid(const XwaSnapshot* snapshot, const XwaRemasterFlightView* view) {
	const float stroke = fmaxf(1.0f, view->classic_pixel_scale);
	for (int coordinate = -MAP_GRID_LIMIT; coordinate <= MAP_GRID_LIMIT; coordinate += MAP_GRID_STEP) {
		const int32_t x_line_a[3] = { -MAP_GRID_LIMIT, coordinate, MAP_PLANE_Z };
		const int32_t x_line_b[3] = { MAP_GRID_LIMIT, coordinate, MAP_PLANE_Z };
		const int32_t y_line_a[3] = { coordinate, -MAP_GRID_LIMIT, MAP_PLANE_Z };
		const int32_t y_line_b[3] = { coordinate, MAP_GRID_LIMIT, MAP_PLANE_Z };
		map_add_world_line(map_state.grid, &snapshot->flight_camera, view, x_line_a, x_line_b, 0x31, stroke,
						   1);
		map_add_world_line(map_state.grid, &snapshot->flight_camera, view, y_line_a, y_line_b, 0x31, stroke,
						   1);
	}
}

static void map_after_meshes(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
							 void* user) {
	(void)user;
	if (!target)
		return;
	AeronDrawList_RenderIntoPass(map_state.annotations[0], cmd, pass, target);
	AeronDrawList_RenderIntoPass(map_state.grid, cmd, pass, target);
	AeronDrawList_RenderIntoPass(map_state.annotations[1], cmd, pass, target);
}

int XwaRemasterFlightMap_Init(void) {
	if (map_state.annotations[0])
		return 1;
	map_state.annotations[0] = AeronDrawList_Create(XWA_SNAP_MAX_FLIGHT_MAP_OBJECTS * 16);
	map_state.annotations[1] = AeronDrawList_Create(XWA_SNAP_MAX_FLIGHT_MAP_OBJECTS * 16);
	map_state.grid = AeronDrawList_Create(96);
	map_state.text = AeronDrawList_Create(XWA_SNAP_MAX_FLIGHT_MAP_OBJECTS * 48);
	if (map_state.annotations[0] && map_state.annotations[1] && map_state.grid && map_state.text)
		return 1;
	XwaRemasterFlightMap_Shutdown();
	return 0;
}

void XwaRemasterFlightMap_Shutdown(void) {
	for (int i = 0; i < 2; i++) {
		AeronDrawList_Destroy(map_state.annotations[i]);
		map_state.annotations[i] = NULL;
	}
	AeronDrawList_Destroy(map_state.grid);
	AeronDrawList_Destroy(map_state.text);
	memset(&map_state, 0, sizeof map_state);
}

int XwaRemasterFlightMap_Prepare(AeronCommandBuffer* cmd, AeronScene3D* scene, const XwaSnapshot* snapshot,
								 XwaRemasterAssets* assets, const XwaRemasterFlightView* view,
								 const float world_to_eye[9],
								 XwaRemasterFlightMapSubmitObjectFn submit_object, void* submit_user) {
	if (!cmd || !scene || !snapshot || !assets || !view || !world_to_eye || !submit_object ||
		!snapshot->flight_map.active || !XwaRemasterFlightMap_Init())
		return 0;
	memcpy(map_state.world_to_eye, world_to_eye, sizeof map_state.world_to_eye);
	map_state.object_count = 0;
	for (int i = 0; i < 2; i++)
		AeronDrawList_Begin(map_state.annotations[i], NULL, view->viewport.width, view->viewport.height,
							AERON_DRAWLIST2D_LOAD, NULL);
	AeronDrawList_Begin(map_state.grid, NULL, view->viewport.width, view->viewport.height,
						AERON_DRAWLIST2D_LOAD, NULL);
	AeronDrawList_Begin(map_state.text, NULL, view->viewport.width, view->viewport.height,
						AERON_DRAWLIST2D_LOAD, NULL);

	for (uint16_t i = 0; i < snapshot->flight_map.object_count; i++) {
		const XwaFlightMapObject* map = &snapshot->flight_map.objects[i];
		if (map->flight_object_index >= snapshot->flight_object_count)
			continue;
		const XwaFlightObject* object = &snapshot->flight_objects[map->flight_object_index];
		float eye[3];
		map_world_to_eye(&snapshot->flight_camera, map_state.world_to_eye, object->world_pos, eye);
		if (!map_candidate_visible(map, eye, view, &snapshot->flight_camera))
			continue;
		MapPreparedObject* prepared = &map_state.objects[map_state.object_count++];
		prepared->map = map;
		prepared->object = object;
		prepared->snapshot_index = map->flight_object_index;
		prepared->scan_ordinal = i;
		memcpy(prepared->eye, eye, sizeof eye);
	}
	qsort(map_state.objects, map_state.object_count, sizeof map_state.objects[0], map_depth_compare);
	const int camera_above = snapshot->flight_camera.world_pos[2] >= MAP_PLANE_Z;
	int font_scale = (int)(snapshot->hud.classic_hud_scale * 10.0f);
	if (font_scale < 1)
		font_scale = 10;
	const int font_tier = font_scale < 12 ? 2 : (font_scale < 15 ? 1 : 0);
	const XwaFlightFontRef* font = XwaRemasterAssets_FlightFont(assets, font_tier);
	const float font_size = (float)font_scale * view->classic_pixel_scale;
	const float line_height = (float)(font_scale - (font_scale >> 2)) * view->classic_pixel_scale;
	const float source_w = snapshot->flight_camera.screen_w ? snapshot->flight_camera.screen_w : 640.0f;
	const float classic_width_px = source_w * view->classic_pixel_scale;
	for (uint16_t i = 0; i < map_state.object_count; i++) {
		MapPreparedObject* prepared = &map_state.objects[i];
		const int object_above = prepared->object->world_pos[2] >= MAP_PLANE_Z;
		prepared->annotation_pass = (uint8_t)(object_above != camera_above ? 0 : 1);
		if (!XwaRemasterFlight_ProjectView(view, prepared->eye, &prepared->screen_x, &prepared->screen_y))
			continue;
		const int icon_eligible = prepared->map->render_kind == XWA_FLIGHT_MAP_RENDER_CRAFT ||
								  prepared->object->genus == XWA_SNAP_GENUS_MINE;
		const int use_icon = icon_eligible && prepared->eye[2] > 0.0f &&
							 (prepared->eye[2] / 16.0f) > prepared->map->max_bounds_extent;
		if (!use_icon &&
			!submit_object(prepared->map, prepared->object, prepared->snapshot_index, submit_user))
			return 0;
		map_add_object_annotations(snapshot, assets, view, prepared, use_icon);
		const char* label = prepared->map->label_offset < snapshot->flight_map.label_bytes
								? &snapshot->flight_map.labels[prepared->map->label_offset]
								: NULL;
		map_add_object_text(prepared->map, prepared->object, view, font, label, prepared->screen_x,
							prepared->screen_y, prepared->eye[2], classic_width_px, font_size, line_height,
							snapshot->flight_camera.focus_obj_idx != 0xffff);
	}
	map_build_grid(snapshot, view);
	if (!AeronDrawList_Prepare(map_state.annotations[0], cmd) ||
		!AeronDrawList_Prepare(map_state.annotations[1], cmd) ||
		!AeronDrawList_Prepare(map_state.grid, cmd) || !AeronDrawList_Prepare(map_state.text, cmd))
		return 0;
	AeronScene_SetAfterMeshes(scene, map_after_meshes, NULL);
	return 1;
}

void XwaRemasterFlightMap_RenderDeferredText(AeronCommandBuffer* cmd, AeronRenderPass* pass,
											 AeronRenderTarget* target) {
	if (cmd && pass && target && map_state.text)
		AeronDrawList_RenderIntoPass(map_state.text, cmd, pass, target);
}
