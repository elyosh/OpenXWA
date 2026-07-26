#ifndef XWA_REMASTER_H
#define XWA_REMASTER_H

/*
 * XWA remaster driver — translates XwaSnapshot into aeron_scene
 * submissions composited over the classic layers.
 *
 * Reads snapshot data as its semantic render input. Narrow modern-adapter
 * controls coordinate ownership of classic presentation layers.
 *
 * Frontend records replay onto a persistent HD target. Asset identities
 * resolve to authored remaster files or independently decoded original files.
 */

#include <stdint.h>

#include "aeron/render.h"
#include "xwa_runtime/config/modern_video_options.h"

struct AeronInputSnapshot;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaRemasterInitOptions {
	float opt_smooth_angle_degrees;
	float opt_emissive_strength;
	float opt_projectile_emissive_strength;
	int force_opt_models;
	int prefer_original_2d;
	XwaModernVideoOptions video_options;
	unsigned int video_options_override_mask;
} XwaRemasterInitOptions;

/* Loads mandatory renderer resources and creates process-wide remaster state. */
int XwaRemaster_Init(const XwaRemasterInitOptions* options);

/* Applies host controls and selects the classic-flight rendering policy before
 * the recovered game tick. F2 toggles SPLIT without a fade; F5 fades between
 * CLASSIC and HD. Transitions needing classic output wait for a fresh frame. */
void XwaRemaster_BeginFrame(const struct AeronInputSnapshot* input);

/* Renders the current snapshot and submits the remaster overlay. Call after
 * XwaPort_Tick/XwaPort_PausedFrame and before Aeron_Present. */
void XwaRemaster_Frame(int32_t delta_us);

void XwaRemaster_Shutdown(void);

/* Debug-tool surface: desired HDR output (effective = desired AND the
 * display supports it). The swapchain flip is deferred to the next
 * XwaRemaster_BeginFrame boundary — reconfiguring mid-frame would rebuild
 * pipelines (including the debug overlay's own) while a frame is
 * still being recorded. */
void XwaRemaster_SetHdrDesired(int want);
int XwaRemaster_GetHdrDesired(void);
AeronSampleCount XwaRemaster_MsaaSampleCount(void);

/* Narrow bridge surface used by the original-style modern options screen.
 * Only user-facing selections are changed; advanced renderer tuning remains
 * owned by the flight renderer and its debug tools. */
void XwaRemaster_GetVideoOptions(XwaModernVideoOptions* out);
void XwaRemaster_SetVideoOptions(const XwaModernVideoOptions* options);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_H */
