#include "suns.h"

#include "aeron/log.h"
#include "aeron/scene/lens_flare.h"
#include "xwa_runtime/models/flight_config.h"

#include <math.h>
#include <string.h>

/* The flare's anchor is this far from the camera, as far as the backdrops'
 * directions, so its depth is behind every model. */
#define SUNS_FLARE_DISTANCE 1048576.0f
/* A procedural star's disc is full up to a tenth of its quad's width from
 * the centre: a fifth of its half size. */
#define SUNS_STAR_DISC 0.2f

static struct {
	AeronSceneLensFlareElement elements[AERON_SCENE_LENS_FLARE_MAX_ELEMENTS];
	uint32_t element_count;
	uint64_t prepared_serial;
} s_suns;

/* Reads the mod's list, which replaces the shipped one. */
static void suns_load_mod_elements(void) {
	AeronConfigError error = { 0 };
	const AeronConfigNode* root = XwaFlightConfig_Root();
	const AeronConfigNode* list =
		AeronConfigNode_MapGet(AeronConfigNode_MapGet(root, "suns"), "flare_elements");
	AeronSceneLensFlareElement elements[AERON_SCENE_LENS_FLARE_MAX_ELEMENTS];
	uint32_t count = 0;
	if (list) {
		if (AeronSceneLensFlare_ParseElements(list, elements, AERON_SCENE_LENS_FLARE_MAX_ELEMENTS, &count,
											  &error)) {
			memcpy(s_suns.elements, elements, count * sizeof elements[0]);
			s_suns.element_count = count;
		} else {
			Aeron_LogWarn("xwa.flight", "%s:%d: suns.flare_elements: %s; the default flare is used",
						  XWA_FLIGHT_CONFIG_PATH, error.line, error.message);
		}
	}
}

int XwaRemasterSuns_LoadFlareElements(const AeronConfigFile* shipped, const char* shipped_path) {
	AeronConfigError error = { 0 };
	if (!AeronSceneLensFlare_ParseElements(AeronConfigFile_GetNode(shipped, "suns.flare_elements"),
										   s_suns.elements, AERON_SCENE_LENS_FLARE_MAX_ELEMENTS,
										   &s_suns.element_count, &error)) {
		Aeron_LogError("xwa.remaster", "%s:%d: suns.flare_elements: %s", shipped_path, error.line,
					   error.message);
		return 0;
	}
	suns_load_mod_elements();
	return 1;
}

void XwaRemasterSuns_PrepareScene(AeronScene3D* scene, uint64_t scene_serial, int procedural_flare) {
	if (!scene || s_suns.prepared_serial == scene_serial)
		return;
	s_suns.prepared_serial = scene_serial;
	AeronScene_SetLensFlareElements(scene, s_suns.elements, s_suns.element_count);
	if (procedural_flare && !AeronScene_PrepareLensFlares(scene))
		Aeron_LogWarn("xwa.remaster", "flight: the procedural lens flare isn't available");
}

/* The corner of the quads whose uv is nearest `uv`. */
static const float* suns_corner_at(const float (*corners)[4][3], const float (*uvs)[4][2], uint32_t count,
								   float u, float v) {
	const float* best = NULL;
	float best_distance = INFINITY;
	for (uint32_t q = 0; q < count; q++) {
		for (int c = 0; c < 4; c++) {
			const float du = uvs[q][c][0] - u;
			const float dv = uvs[q][c][1] - v;
			if (du * du + dv * dv < best_distance) {
				best_distance = du * du + dv * dv;
				best = corners[q][c];
			}
		}
	}
	return best;
}

static float suns_distance(const float a[3], const float b[3]) {
	return sqrtf((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) +
				 (a[2] - b[2]) * (a[2] - b[2]));
}

int XwaRemasterSuns_StarDiscRadius(const float (*corners)[4][3], const float (*uvs)[4][2], uint32_t count,
								   float* out_tangent) {
	if (count == 0 || !out_tangent)
		return 0;
	const float* origin = suns_corner_at(corners, uvs, count, 0.0f, 0.0f);
	const float* right = suns_corner_at(corners, uvs, count, 1.0f, 0.0f);
	const float* down = suns_corner_at(corners, uvs, count, 0.0f, 1.0f);
	const float centre[3] = { 0.5f * (right[0] + down[0]), 0.5f * (right[1] + down[1]),
							  0.5f * (right[2] + down[2]) };
	const float zero[3] = { 0.0f, 0.0f, 0.0f };
	const float distance = suns_distance(centre, zero);
	if (!(distance > 0.0f))
		return 0;
	/* The half size, from the width and the height. */
	const float half = 0.25f * (suns_distance(origin, right) + suns_distance(origin, down));
	*out_tangent = SUNS_STAR_DISC * half / distance;
	return 1;
}

void XwaRemasterSuns_SetFlare(AeronScene3D* scene, const float camera_local[3], const float direction[3],
							  float tangent, const float tint[3], float intensity) {
	const float length =
		sqrtf(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
	if (!scene || !(length > 0.0f))
		return;
	AeronSceneLensFlareDesc desc;
	memset(&desc, 0, sizeof desc);
	for (int a = 0; a < 3; a++)
		desc.anchor_world[a] = camera_local[a] + direction[a] / length * SUNS_FLARE_DISTANCE;
	desc.radius_world = tangent * SUNS_FLARE_DISTANCE;
	memcpy(desc.tint, tint, sizeof desc.tint);
	desc.intensity = intensity;
	AeronScene_SetLensFlare(scene, &desc);
}
