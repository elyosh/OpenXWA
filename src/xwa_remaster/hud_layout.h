#ifndef XWA_REMASTER_HUD_LAYOUT_H
#define XWA_REMASTER_HUD_LAYOUT_H

#include "xwa_remaster/hud.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaHudLayoutRect {
	int x, y, w, h;
} XwaHudLayoutRect;

typedef enum XwaHudLayoutOrigin {
	XWA_HUD_LAYOUT_LEFT_TOP,
	XWA_HUD_LAYOUT_CENTER_TOP,
	XWA_HUD_LAYOUT_RIGHT_TOP,
	XWA_HUD_LAYOUT_LEFT_BOTTOM,
	XWA_HUD_LAYOUT_CENTER_BOTTOM,
	XWA_HUD_LAYOUT_RIGHT_BOTTOM,
	XWA_HUD_LAYOUT_CENTER,
	XWA_HUD_LAYOUT_VIEWPORT,
	/* Centred horizontally, with Y at the same fraction of the height as in
	   the source. The game places its message panes this way. */
	XWA_HUD_LAYOUT_CENTER_X_SCREEN_Y,
} XwaHudLayoutOrigin;

typedef struct XwaHudLayoutAnchor {
	XwaHudLayoutRect rect;
	int radius;
	uint8_t has_radius;
	uint8_t reserved[3];
} XwaHudLayoutAnchor;

/* The sizes the remaster HUD draws at, in target pixels. See
   docs/hud_scale_plan.md. */
typedef struct XwaHudScale {
	/* The player's HUD scale (video.hud_scale). */
	float user;
	/* One unit of the 640x480 HUD. */
	float unit_px;
	/* One pixel of the game's HUD layout: unit_px / g_flightHudScaleFactor. */
	float classic_px;
	/* One unit of the message anchors: user x target_h / 480. */
	float message_px;
} XwaHudScale;

typedef struct XwaHudLayoutProfile {
	uint8_t valid;
	char name[32];
	int reference_w, reference_h;
	/* The game's g_flightHudScaleFactor that `scale` was computed with. */
	float classic_hud_scale;
	XwaHudScale scale;
	XwaHudLayoutAnchor anchors[XWA_HUD_ANCHOR_COUNT];
} XwaHudLayoutProfile;

#define XWA_HUD_CANONICAL_WIDTH 640
#define XWA_HUD_CANONICAL_HEIGHT 480
#define XWA_HUD_MAX_LAYOUT_PROFILES 1

struct XwaHudLayout {
	uint32_t generation;
	uint8_t profile_count;
	uint8_t default_profile;
	uint16_t reserved;
	XwaHudLayoutProfile profiles[XWA_HUD_MAX_LAYOUT_PROFILES];
};

const char* XwaRemasterHud_AnchorName(XwaHudAnchorId id);
const XwaHudLayoutRect* XwaRemasterHudLayout_CanonicalAnchor(XwaHudAnchorId id);
void XwaRemasterHudLayout_Init(XwaHudLayout* out);
/* Resolves the layout for a target size, the player's HUD scale
   (video.hud_scale) and the game's g_flightHudScaleFactor. */
int XwaRemasterHudLayout_Resolve(XwaHudLayout* layout, int target_w, int target_h, float user_scale,
								 float classic_hud_scale);
void XwaRemasterHudLayout_MapPoint(XwaHudLayoutOrigin origin, float source_w, float source_h, float target_w,
								   float target_h, float scale, float source_x, float source_y, float* out_x,
								   float* out_y);
const XwaHudLayoutProfile* XwaRemasterHudLayout_Profile(const XwaHudLayout* layout, XwaHudProfileIndex index);
void XwaRemasterHudLayout_OutputTransform(const XwaHudLayoutProfile* profile, int target_w, int target_h,
										  int* out_x, int* out_y, float* out_scale);
int XwaRemasterHudLayout_TargetToReference(const XwaHudLayoutProfile* profile, int target_w, int target_h,
										   float target_x, float target_y, float* out_x, float* out_y);

#ifdef __cplusplus
}
#endif

#endif
