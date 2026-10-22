#include "xwa_remaster/scene_lights.h"

#include "xwa_remaster/color.h"

#include <math.h>
#include <string.h>

uint32_t XwaRemaster_ReadGameDirLights(const XwaDirLight* lights, uint32_t count, XwaRemasterDirLight* out) {
	if (!lights || !out) {
		return 0;
	}
	if (count > XWA_SNAP_MAX_DIR_LIGHTS) {
		count = XWA_SNAP_MAX_DIR_LIGHTS;
	}
	for (uint32_t i = 0; i < count; i++) {
		const XwaDirLight* light = &lights[i];
		XwaRemasterDirLight* linear = &out[i];
		memcpy(linear->world_dir, light->world_dir, sizeof linear->world_dir);
		for (int channel = 0; channel < 3; channel++) {
			const float value = light->color[channel] * light->intensity;
			linear->color[channel] = isfinite(value) && value > 0.0f ? XwaRemaster_SrgbToLinear(value) : 0.0f;
		}
		linear->source_backdrop_object_type = light->source_backdrop_object_type;
		linear->source_backdrop_index = light->source_backdrop_index;
	}
	return count;
}
