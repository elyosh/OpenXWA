#ifndef XWA_REMASTER_DEBUG_TOOLS_H
#define XWA_REMASTER_DEBUG_TOOLS_H

/*
 * XWA debug tools — registration entry point for the Aeron debug
 * overlay (aeron/debug.h): PBR global tuning, SSAO knobs, and
 * HDR & display. The implementation TU is compiled only when the
 * build enables AERON_DEBUG_UI (XWA_ENABLE_DEBUG_UI); call sites
 * guard with the same define.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Register every XWA inspector with Aeron_DebugRegisterTool. Call
 * once at remaster init (the tools poke remaster process-wide
 * state). No-op when the overlay is unavailable at runtime. */
void XwaRemasterDebugTools_Register(void);

#ifdef __cplusplus
}
#endif

#endif /* XWA_REMASTER_DEBUG_TOOLS_H */
