#ifndef XWA_RUNTIME_MODELS_COCKPIT_VIEW_H
#define XWA_RUNTIME_MODELS_COCKPIT_VIEW_H

/* Modern port: the field of view of the player's craft.
 *
 * A GLB craft model can give its cockpit view a vertical field of view with
 * a perspective camera on its Cockpit hardpoint (type 31), the pilot eye.
 * The `cockpit_view` group of the craft's model settings file can give a
 * wide view too. presentation.cockpit_fov in the player's configuration
 * picks the camera's view, the wide view or the stock view. A craft without
 * the view it needs keeps the stock view.
 *
 * The view sets the game's projection (g_projScale and the values that go
 * with it) at the height of the game's screen, each time the player's
 * cockpit model loads. It applies to every view of the flight. Only drawing
 * reads the projection: the simulation, films and netplay don't depend on
 * it. */

#ifdef __cplusplus
extern "C" {
#endif

struct AeronConfigNode;

typedef enum XwaCockpitFovMode {
	XWA_COCKPIT_FOV_COCKPIT = 0,
	XWA_COCKPIT_FOV_WIDE,
	XWA_COCKPIT_FOV_OFF,
} XwaCockpitFovMode;

/* Drops the previous mission's wide views. Called by the model settings
   reader. */
void XwaCockpitView_ClearModelSettings(void);
/* Reads the `cockpit_view` group of `objectType`'s model settings file, or
   does nothing when `group` is NULL. `path` is the file, for the log. Called
   by the model settings reader. */
void XwaCockpitView_ReadModelSettings(unsigned int objectType, const char* path,
									  const struct AeronConfigNode* group);

/* Picks the view and applies it to the current craft. */
void XwaCockpitView_SetMode(XwaCockpitFovMode mode);
XwaCockpitFovMode XwaCockpitView_Mode(void);

/* Keeps the stock projection that the flight launch has just set, so that a
   craft without a view can go back to it. */
void XwaCockpitView_SaveStockProjection(void);
/* Sets the projection from the view of `objectType`, the player's craft. */
void XwaCockpitView_Apply(unsigned int objectType);

/* Whether the projection comes from a craft's view. */
int XwaCockpitView_Active(void);
/* The vertical field of view of the projection, in degrees. */
float XwaCockpitView_VerticalFovDegrees(void);

#ifdef __cplusplus
}
#endif

#endif
