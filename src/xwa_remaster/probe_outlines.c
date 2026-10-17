#include "xwa_remaster/probe_outlines.h"

#include "aeron/asset/opt_model.h"
#include "aeron/scene/draw_list2d.h"

#include <math.h>
#include <string.h>

#define OUTLINE_CIRCLE_SEGMENTS 32
/* Grid points are drawn within this distance of the camera only. */
#define OUTLINE_POINT_RANGE_M 50.0f
#define OUTLINE_CROSS_HALF_M 0.25f
#define OUTLINE_MAX_POINTS 4096
/* Per probe: influence, inner shape and parallax volume, each three circles
 * at most; per set, the grid box. */
#define OUTLINE_RECORDS                                                                                      \
	(AERON_SCENE_MAX_PROBE_SETS * (AERON_FLIGHT_MAX_PROBES * 3 * 3 * OUTLINE_CIRCLE_SEGMENTS + 12) +         \
	 OUTLINE_MAX_POINTS * 3)

static const float outline_colors[8][3] = {
	{ 1.0f, 0.25f, 0.25f }, { 0.25f, 1.0f, 0.25f }, { 0.3f, 0.5f, 1.0f }, { 1.0f, 0.9f, 0.2f },
	{ 1.0f, 0.3f, 1.0f },   { 0.2f, 1.0f, 1.0f },   { 1.0f, 0.6f, 0.2f }, { 0.7f, 0.5f, 1.0f },
};

static struct {
	AeronDrawList2D* list;
	/* The frame's view, for the lines. */
	const XwaRemasterFlightView* view;
	const float* world_to_eye;
	const float* model;
	float thickness;
	uint32_t points;
} outline;

static void outline_after_meshes(AeronCommandBuffer* cmd, AeronRenderPass* pass, AeronRenderTarget* target,
								 void* user) {
	(void)user;
	if (target) {
		AeronDrawList_RenderIntoPass(outline.list, cmd, pass, target);
	}
}

/* Model metres to the eye space of the view. */
static void outline_eye(const float p[3], float eye[3]) {
	const float* m = outline.model;
	const float* camera = outline.view->camera.pos;
	float d[3];
	for (int r = 0; r < 3; r++) {
		d[r] = m[r * 4 + 0] * p[0] + m[r * 4 + 1] * p[1] + m[r * 4 + 2] * p[2] + m[r * 4 + 3] - camera[r];
	}
	for (int r = 0; r < 3; r++) {
		eye[r] = outline.world_to_eye[r * 3 + 0] * d[0] + outline.world_to_eye[r * 3 + 1] * d[1] +
				 outline.world_to_eye[r * 3 + 2] * d[2];
	}
}

static void outline_line(const float pa[3], const float pb[3], const float rgba[4]) {
	float a[3], b[3], ax, ay, bx, by;
	outline_eye(pa, a);
	outline_eye(pb, b);
	if (!XwaRemasterFlight_ClipProjectLine(outline.view, a, b, &ax, &ay, &bx, &by)) {
		return;
	}
	AeronDrawList_AddProjectedLine(outline.list, ax, ay, a[2], bx, by, b[2], outline.view->camera.near_z,
								   outline.thickness, rgba, AERON_BLIT2D_BLEND_PMA, &outline.view->viewport);
}

static void outline_box(const float center[3], const float size[3], const float rgba[4]) {
	/* Corner k has bit a set for +half on axis a; edges join corners one bit apart. */
	for (int k = 0; k < 8; k++) {
		for (int axis = 0; axis < 3; axis++) {
			if (k & (1 << axis)) {
				continue;
			}
			float a[3], b[3];
			for (int c = 0; c < 3; c++) {
				const float half = size[c] * 0.5f;
				a[c] = center[c] + ((k >> c) & 1 ? half : -half);
				b[c] = c == axis ? center[c] + half : a[c];
			}
			outline_line(a, b, rgba);
		}
	}
}

/* Three circles, one in each axis plane. */
static void outline_ellipsoid(const float center[3], const float size[3], const float rgba[4]) {
	for (int plane = 0; plane < 3; plane++) {
		const int u = (plane + 1) % 3, v = (plane + 2) % 3;
		float previous[3];
		for (int i = 0; i <= OUTLINE_CIRCLE_SEGMENTS; i++) {
			const float t = 6.28318530718f * (float)i / (float)OUTLINE_CIRCLE_SEGMENTS;
			float p[3];
			memcpy(p, center, sizeof p);
			p[u] += cosf(t) * size[u] * 0.5f;
			p[v] += sinf(t) * size[v] * 0.5f;
			if (i > 0) {
				outline_line(previous, p, rgba);
			}
			memcpy(previous, p, sizeof previous);
		}
	}
}

static void outline_shape(AeronFlightProbeShape shape, const float center[3], const float size[3],
						  const float rgba[4]) {
	if (shape == AERON_FLIGHT_PROBE_ELLIPSOID) {
		outline_ellipsoid(center, size, rgba);
	} else {
		outline_box(center, size, rgba);
	}
}

static void outline_set(const AeronFlightProbeLayout* d) {
	for (uint32_t k = 0; k < d->probe_count; k++) {
		const AeronFlightProbe* probe = &d->probes[k];
		const float* c = outline_colors[k % 8];
		const float color[4] = { c[0], c[1], c[2], 1.0f };
		const float dim[4] = { c[0] * 0.4f, c[1] * 0.4f, c[2] * 0.4f, 0.4f };
		outline_shape(probe->shape, probe->center, probe->size, color);
		/* Where its weight reaches 1. */
		const float inner[3] = { probe->size[0] * (1.0f - probe->falloff),
								 probe->size[1] * (1.0f - probe->falloff),
								 probe->size[2] * (1.0f - probe->falloff) };
		if (probe->falloff > 0.0f && probe->falloff < 1.0f) {
			outline_shape(probe->shape, probe->center, inner, dim);
		}
		if (probe->parallax_shape != probe->shape ||
			memcmp(probe->parallax_center, probe->center, sizeof probe->center) != 0 ||
			memcmp(probe->parallax_size, probe->size, sizeof probe->size) != 0) {
			static const float grey[4] = { 0.5f, 0.5f, 0.5f, 0.6f };
			outline_shape(probe->parallax_shape, probe->parallax_center, probe->parallax_size, grey);
		}
	}
	static const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	outline_box(d->grid_center, d->grid_size, white);

	/* Grid points near the camera: Blender's volume probe places point i of
	 * r at (i + 1) / (r + 1) of the box. */
	static const float point_color[4] = { 0.8f, 0.8f, 0.8f, 0.8f };
	const float range = OUTLINE_POINT_RANGE_M * AERON_OPT_UNITS_PER_METER;
	const uint32_t* r = d->grid_resolution;
	for (uint32_t z = 0; z < r[2]; z++) {
		for (uint32_t y = 0; y < r[1]; y++) {
			for (uint32_t x = 0; x < r[0]; x++) {
				const uint32_t i[3] = { x, y, z };
				float p[3], eye[3];
				for (int a = 0; a < 3; a++) {
					p[a] =
						d->grid_center[a] + d->grid_size[a] * ((float)(i[a] + 1) / (float)(r[a] + 1) - 0.5f);
				}
				outline_eye(p, eye);
				if (outline.points >= OUTLINE_MAX_POINTS ||
					eye[0] * eye[0] + eye[1] * eye[1] + eye[2] * eye[2] > range * range) {
					continue;
				}
				outline.points++;
				for (int a = 0; a < 3; a++) {
					float p0[3], p1[3];
					memcpy(p0, p, sizeof p0);
					memcpy(p1, p, sizeof p1);
					p0[a] -= OUTLINE_CROSS_HALF_M;
					p1[a] += OUTLINE_CROSS_HALF_M;
					outline_line(p0, p1, point_color);
				}
			}
		}
	}
}

int XwaRemasterProbeOutlines_Prepare(AeronCommandBuffer* cmd, AeronScene3D* scene,
									 const XwaRemasterFlightView* view, const float world_to_eye[9],
									 const XwaProbeOutlineSet* sets, uint32_t count) {
	if (!cmd || !scene || !view || !world_to_eye || !sets) {
		return 0;
	}
	if (!outline.list) {
		outline.list = AeronDrawList_Create(OUTLINE_RECORDS);
		if (!outline.list) {
			return 0;
		}
	}
	AeronDrawList_Begin(outline.list, NULL, view->viewport.width, view->viewport.height,
						AERON_DRAWLIST2D_LOAD, NULL);
	outline.view = view;
	outline.world_to_eye = world_to_eye;
	outline.thickness = fmaxf(1.0f, view->classic_pixel_scale);
	outline.points = 0;
	for (uint32_t i = 0; i < count; i++) {
		if (sets[i].probes) {
			outline.model = sets[i].model;
			outline_set(&sets[i].probes->settings);
		}
	}
	outline.view = NULL;
	if (!AeronDrawList_Prepare(outline.list, cmd)) {
		return 0;
	}
	AeronScene_SetAfterMeshes(scene, outline_after_meshes, NULL);
	return 1;
}

void XwaRemasterProbeOutlines_Shutdown(void) {
	AeronDrawList_Destroy(outline.list);
	memset(&outline, 0, sizeof outline);
}
