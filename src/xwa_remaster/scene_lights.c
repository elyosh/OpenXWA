#include "xwa_remaster/scene_lights.h"

#include "xwa_remaster/color.h"

#include <math.h>
#include <string.h>

static float s_classic_light_illuminance;
static float s_classic_light_illuminance_default;
static float s_classic_radiance;
static float s_classic_radiance_default;

float XwaRemaster_ClassicLightIlluminance(void) { return s_classic_light_illuminance; }

void XwaRemaster_SetClassicLightIlluminance(float illuminance) {
	if (isfinite(illuminance) && illuminance >= 0.0f) {
		s_classic_light_illuminance = illuminance;
	}
}

void XwaRemaster_ConfigureClassicLightIlluminance(float illuminance) {
	XwaRemaster_SetClassicLightIlluminance(illuminance);
	s_classic_light_illuminance_default = s_classic_light_illuminance;
}

float XwaRemaster_ClassicLightIlluminanceDefault(void) { return s_classic_light_illuminance_default; }

float XwaRemaster_ClassicRadiance(void) { return s_classic_radiance; }

void XwaRemaster_SetClassicRadiance(float radiance) {
	if (isfinite(radiance) && radiance >= 0.0f) {
		s_classic_radiance = radiance;
	}
}

void XwaRemaster_ConfigureClassicRadiance(float radiance) {
	XwaRemaster_SetClassicRadiance(radiance);
	s_classic_radiance_default = s_classic_radiance;
}

float XwaRemaster_ClassicRadianceDefault(void) { return s_classic_radiance_default; }

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
			linear->color[channel] = isfinite(value) && value > 0.0f
										 ? XwaRemaster_SrgbToLinear(value) * s_classic_light_illuminance
										 : 0.0f;
		}
		linear->source_backdrop_object_type = light->source_backdrop_object_type;
		linear->source_backdrop_index = light->source_backdrop_index;
	}
	return count;
}
