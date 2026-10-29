#include "xwa_runtime/models/cockpit_view.h"

#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/assets/object_type.h"
#include "xwa/render/effects.h"
#include "xwa/render/renderer.h"
#include "xwa_runtime/models/model_settings.h"
#include "xwa_runtime/models/opt_synth.h"

#include <math.h>
#include <string.h>

/* Modern port module; no original counterpart. */

#define COCKPIT_VIEW_PI 3.14159265358979323846

/* The wide view of each object type, in radians, or 0. */
static float s_wideFov[OBJ_Count];
static XwaCockpitFovMode s_mode = XWA_COCKPIT_FOV_COCKPIT;

/* The projection that the flight launch set. */
static int s_stockSaved;
static int s_stockScaleInt;
static int s_stockScaleHalfInt;

static int s_hasCraft;
static unsigned int s_objectType;
static int s_active;

void XwaCockpitView_ClearModelSettings(void) { memset(s_wideFov, 0, sizeof s_wideFov); }

void XwaCockpitView_ReadModelSettings(unsigned int objectType, const char* path,
									  const AeronConfigNode* group) {
	size_t index;

	if (objectType >= OBJ_Count || group == NULL) {
		return;
	}
	if (AeronConfigNode_Type(group) != AERON_CONFIG_MAP) {
		Aeron_LogWarn("xwa.models", "%s: cockpit_view isn't a map. It is ignored", path);
		return;
	}
	for (index = 0; index < AeronConfigNode_MapCount(group); ++index) {
		const char* key = AeronConfigNode_MapKeyAt(group, index);
		const AeronConfigNode* node = AeronConfigNode_MapValueAt(group, index);
		const AeronConfigNodeType type = AeronConfigNode_Type(node);
		const double degrees = AeronConfigNode_Float(node, 0.0);

		if (strcmp(key, "wide_vertical_fov") != 0) {
			Aeron_LogWarn("xwa.models", "%s: cockpit_view.%s isn't a setting. It is ignored", path, key);
		} else if ((type != AERON_CONFIG_FLOAT && type != AERON_CONFIG_INT) || !(degrees > 0.0) ||
				   !(degrees < 180.0)) {
			Aeron_LogWarn("xwa.models",
						  "%s: cockpit_view.wide_vertical_fov must be a number of degrees between 0 and 180. "
						  "It is ignored",
						  path);
		} else {
			s_wideFov[objectType] = (float)(degrees * (COCKPIT_VIEW_PI / 180.0));
		}
	}
}

/* The vertical field of view of the current craft, in radians, or 0 for the
   stock view. */
static float CockpitView_CraftFov(void) {
	float wide;

	if (!s_hasCraft || s_mode == XWA_COCKPIT_FOV_OFF || s_objectType >= OBJ_Count) {
		return 0.0f;
	}
	XwaModelSettings_Ensure(s_objectType);
	wide = s_wideFov[s_objectType];
	if (s_mode == XWA_COCKPIT_FOV_WIDE && wide > 0.0f) {
		return wide;
	}
	return OptSynth_TypeCockpitVerticalFov(s_objectType);
}

static void CockpitView_Update(void) {
	const float fov = CockpitView_CraftFov();

	if (!s_stockSaved) {
		return;
	}
	if (fov > 0.0f) {
		const double scale = 0.5 * (double)g_screenHeight / tan(0.5 * (double)fov);

		g_projScaleInt = (int)(scale + 0.5);
		g_projScaleHalfInt = g_projScaleInt / 2;
	} else {
		g_projScaleInt = s_stockScaleInt;
		g_projScaleHalfInt = s_stockScaleHalfInt;
	}
	g_projScale = (float)(unsigned int)g_projScaleInt;
	g_projScaleDiv512 = g_projScale * (1.0f / 512.0f);
	Particle_UpdateTemplateBillboardScales();
	if (fov > 0.0f || s_active) {
		Aeron_LogInfo("xwa.models", "cockpit view: object type %u, %.1f degrees%s", s_objectType,
					  (double)XwaCockpitView_VerticalFovDegrees(), fov > 0.0f ? "" : " (stock)");
	}
	s_active = fov > 0.0f;
}

void XwaCockpitView_SetMode(XwaCockpitFovMode mode) {
	s_mode = mode;
	CockpitView_Update();
}

XwaCockpitFovMode XwaCockpitView_Mode(void) { return s_mode; }

void XwaCockpitView_SaveStockProjection(void) {
	s_stockSaved = 1;
	s_stockScaleInt = g_projScaleInt;
	s_stockScaleHalfInt = g_projScaleHalfInt;
	s_hasCraft = 0;
	s_active = 0;
}

void XwaCockpitView_Apply(unsigned int objectType) {
	s_hasCraft = 1;
	s_objectType = objectType;
	CockpitView_Update();
}

int XwaCockpitView_Active(void) { return s_active; }

float XwaCockpitView_VerticalFovDegrees(void) {
	if (g_projScaleInt <= 0) {
		return 0.0f;
	}
	return (float)(2.0 * atan(0.5 * (double)g_screenHeight / (double)g_projScaleInt) *
				   (180.0 / COCKPIT_VIEW_PI));
}
