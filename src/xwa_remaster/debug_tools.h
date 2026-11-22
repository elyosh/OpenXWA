#ifndef XWA_REMASTER_DEBUG_TOOLS_H
#define XWA_REMASTER_DEBUG_TOOLS_H

/*
 * The XWA windows and menus of the Aeron debug overlay (aeron/debug.h). The
 * code is in src/xwa_remaster/debug and is built only with the debug UI
 * (XWA_ENABLE_DEBUG_UI). Callers check the same define.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Adds the XWA windows and menus. Call it once when the remaster starts.
 * Does nothing when the overlay isn't available. */
void XwaRemasterDebugTools_Register(void);

/* The XWA rows of Aeron's Environment window: the authored environments
 * switch, the region's selection, and the sky capture. */
void XwaRemasterDebugTools_EnvironmentRows(void* user);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_DEBUG_TOOLS_H */
