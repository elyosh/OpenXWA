#ifndef XWA_RUNTIME_SNAPSHOT_HUD_H
#define XWA_RUNTIME_SNAPSHOT_HUD_H

#include "xwa_runtime/snapshot/snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Original-renderer hooks. Note/emit calls are valid only between
 * BeginClassicFrame and EndClassicFrame. */
void XwaSnapshotHud_BeginClassicFrame(void);
void XwaSnapshotHud_EndClassicFrame(void);
void XwaSnapshotHud_Reset(void);
void XwaSnapshotHud_NoteReticleReady(int slot, int ready);
void XwaSnapshotHud_NoteReticleInRange(int in_range);
void XwaSnapshotHud_NoteThreat(int slot, int state);
void XwaSnapshotHud_BeginRadar(int classic_radius);
void XwaSnapshotHud_NoteRadarBlip(uint16_t slot, uint16_t signature, int radar, int targeted,
								  int local_x, int local_y, uint16_t color_index);
void XwaSnapshotHud_NoteRadarTargetMarker(int radar, int local_x, int local_y);
void XwaSnapshotHud_NoteTargetBox(uint16_t slot, uint16_t signature, uint16_t component,
									  uint16_t color_index, int selected, int extent);
XwaHudTargetBoxLayer XwaSnapshotHud_SetTargetBoxLayer(XwaHudTargetBoxLayer layer);
void XwaSnapshotHud_NoteCrt(const XwaHudCrt* crt);
void XwaSnapshotHud_PushPane(XwaHudPaneId pane, int origin_x, int origin_y, int width, int height);
void XwaSnapshotHud_PushRelativePane(XwaHudPaneId pane, int origin_x, int origin_y, int width, int height);
void XwaSnapshotHud_PopPane(void);
void XwaSnapshotHud_EmitFlightGlyph(uint8_t ch, uint8_t font_tier, int x, int y, uint8_t scale,
									uint8_t classic_w, uint32_t argb);

/* Copies the last fully completed classic HUD presentation. A frame currently
 * being built is never exposed to the triple-buffered public snapshot. */
void XwaSnapshotHud_Capture(XwaHudState* out);

#ifdef __cplusplus
}
#endif

#endif
